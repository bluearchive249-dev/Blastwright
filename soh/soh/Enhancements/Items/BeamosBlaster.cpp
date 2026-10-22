// Beamos Blaster
//
// Turns Link's three swords into three Beamos-inspired guns that fire Beamos-style laser bolts.
//
//   Kokiri Sword  -> Mk.I   "Sentry Beamer"      (weak)
//   Master Sword  -> Mk.II  "Sentinel Cannon"    (strong, pierces one target)
//   Biggoron      -> Mk.III "Overseer Cannon"    (strongest, wide bolt, pierces several targets)
//
// Controls (while a "sword" is equipped and drawn):
//   * Quick tap of B   : hip-fire. The bolt flies straight ahead (or at your Z-target).
//   * Hold B           : raise the gun and aim exactly like a bow (first-person, or Z-target aim). A red laser sight
//                        shows where the shot will go. Release B to fire.
//
// The gun re-uses the bow's aim state machine in z_player.c (see beamos_blaster.patch), so everything that works for
// the bow - first-person camera, stick / gyro / right-stick aiming, horseback, swimming restrictions - works here.

#include "BeamosBlaster.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/frame_interpolation.h" // gives the OPEN_DISPS macros C linkage for FrameInterpolation_*
#include "soh/ActorDB.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_vm/object_vm.h" // gBeamosLaserDL: the real Beamos laser model
extern PlayState* gPlayState;
}

// ---------------------------------------------------------------------------------------------------------------------
// CVars
// ---------------------------------------------------------------------------------------------------------------------

#define CVAR_BLASTER_ENABLED CVAR_ENHANCEMENT("BeamosBlaster.Enabled")
#define CVAR_BLASTER_TAP_FRAMES CVAR_ENHANCEMENT("BeamosBlaster.TapFrames")
#define CVAR_BLASTER_BEAM_WIDTH CVAR_ENHANCEMENT("BeamosBlaster.BeamWidth")
#define CVAR_BLASTER_GLOW_SIZE CVAR_ENHANCEMENT("BeamosBlaster.GlowSize")
#define CVAR_BLASTER_MODEL_SCALE CVAR_ENHANCEMENT("BeamosBlaster.ModelScale")
#define CVAR_BLASTER_MODEL_ORIENT CVAR_ENHANCEMENT("BeamosBlaster.ModelOrientation")
#define CVAR_BLASTER_MODEL_OFF_X CVAR_ENHANCEMENT("BeamosBlaster.ModelOffsetX")
#define CVAR_BLASTER_MODEL_OFF_Y CVAR_ENHANCEMENT("BeamosBlaster.ModelOffsetY")
#define CVAR_BLASTER_MODEL_OFF_Z CVAR_ENHANCEMENT("BeamosBlaster.ModelOffsetZ")
#define CVAR_BLASTER_MODEL_ROT_X CVAR_ENHANCEMENT("BeamosBlaster.ModelFineRotX")
#define CVAR_BLASTER_MODEL_ROT_Y CVAR_ENHANCEMENT("BeamosBlaster.ModelFineRotY")
#define CVAR_BLASTER_MODEL_ROT_Z CVAR_ENHANCEMENT("BeamosBlaster.ModelFineRotZ")

// ---------------------------------------------------------------------------------------------------------------------
// Gun tiers
// ---------------------------------------------------------------------------------------------------------------------

enum GunTier { GUN_MK1 = 0, GUN_MK2 = 1, GUN_MK3 = 2, GUN_TIER_COUNT = 3 };

struct TierStats {
    const char* name;
    u32 dmgFlags;     // damage type handed to enemies' damage tables (same tables swords/hammers use)
    f32 speed;        // world units per update
    s16 lifetime;     // updates before the bolt fizzles out
    f32 halfWidth;    // collision half width (world units); the visible beam is much slimmer
    f32 visualLength; // length of the visible bolt (world units)
    f32 beamThickness; // thickness of the visible beam, 1.0 = the width of a real Beamos laser
    f32 flareSize;     // size of the small glow at the head of the bolt
    s16 cooldown;     // minimum updates between shots
    u8 pierce;        // how many targets the bolt passes through before it stops
    Color_RGB8 glow;  // outer glow colour of the bolt
    Color_RGB8 core;  // inner core colour of the bolt
    f32 modelScale;   // visual size of the gun
};

static const TierStats kTiers[GUN_TIER_COUNT] = {
    // Mk.I - weak, fast, thin. Hits like a Master Sword slash.
    { "Mk.I Sentry Beamer", DMG_SLASH_MASTER, 55.0f, 14, 9.0f, 110.0f, 0.7f, 14.0f, 6, 0, { 70, 150, 255 },
      { 230, 245, 255 }, 0.85f },
    // Mk.II - strong. Hits like a Biggoron's Sword slash and punches through one target.
    { "Mk.II Sentinel Cannon", DMG_SLASH_GIANT, 70.0f, 18, 14.0f, 150.0f, 1.0f, 22.0f, 10, 1, { 50, 120, 255 },
      { 235, 245, 255 }, 1.0f },
    // Mk.III - strongest. Hits like a Megaton Hammer swing, thicker beam, pierces three targets.
    { "Mk.III Overseer Cannon", DMG_HAMMER_SWING, 90.0f, 22, 24.0f, 220.0f, 1.7f, 36.0f, 16, 3, { 90, 100, 255 },
      { 245, 250, 255 }, 1.2f },
};

static int sBeamActorId = -1;
static bool sEnabled = false;

// ---------------------------------------------------------------------------------------------------------------------
// Input tracking: tap (hip-fire) vs hold (aim)
// ---------------------------------------------------------------------------------------------------------------------

static int sHeldFrames = 0;
static bool sBWasDown = false;
static bool sAimLatched = false;
static int sHipFirePending = 0; // updates left before a pending hip-fire request expires
static int sCooldown = 0;

// Last known world position of Link's gun hand, recorded while the gun is drawn.
static Vec3f sHandPos = { 0.0f, 0.0f, 0.0f };
static u32 sHandPosFrame = 0;
static bool sHandPosValid = false;

static int GetTier(s32 itemAction) {
    switch (itemAction) {
        case PLAYER_IA_SWORD_KOKIRI:
            return GUN_MK1;
        case PLAYER_IA_SWORD_MASTER:
            return GUN_MK2;
        case PLAYER_IA_SWORD_BIGGORON:
            return GUN_MK3;
        default:
            return -1;
    }
}

extern "C" s32 BeamosBlaster_IsEnabled(void) {
    return sEnabled;
}

extern "C" s32 BeamosBlaster_IsGunAction(s32 itemAction) {
    return sEnabled && GetTier(itemAction) >= 0;
}

extern "C" s32 BeamosBlaster_WantsAim(void) {
    return sAimLatched;
}

extern "C" s32 BeamosBlaster_ConsumeHipFire(void) {
    if (sHipFirePending > 0) {
        sHipFirePending = 0;
        return 1;
    }
    return 0;
}

static void ResetInputState() {
    sHeldFrames = 0;
    sBWasDown = false;
    sAimLatched = false;
    sHipFirePending = 0;
}

static void TrackInput(Player* player, Input* input) {
    if (sCooldown > 0) {
        sCooldown--;
    }
    if (sHipFirePending > 0) {
        sHipFirePending--;
    }

    if (gPlayState == nullptr || !BeamosBlaster_IsGunAction(player->heldItemAction) ||
        gPlayState->msgCtx.msgMode != MSGMODE_NONE || (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE)) {
        ResetInputState();
        return;
    }

    const int tapFrames = CVarGetInteger(CVAR_BLASTER_TAP_FRAMES, 5);
    const bool bDown = CHECK_BTN_ALL(input->cur.button, BTN_B);

    if (bDown) {
        sHeldFrames++;
    } else {
        // Released: a short press that never turned into an aim counts as a hip-fire tap.
        if (sBWasDown && !sAimLatched && sHeldFrames > 0 && sHeldFrames <= tapFrames) {
            sHipFirePending = 4;
        }
        sHeldFrames = 0;
    }

    sAimLatched = bDown && sHeldFrames > tapFrames;
    sBWasDown = bDown;
}

// ---------------------------------------------------------------------------------------------------------------------
// Shot geometry
// ---------------------------------------------------------------------------------------------------------------------

static void AnglesToDir(s16 yaw, s16 pitch, Vec3f* dir) {
    // Same convention as actors: yaw 0 faces +Z, positive pitch looks down.
    f32 cp = Math_CosS(pitch);
    dir->x = Math_SinS(yaw) * cp;
    dir->y = -Math_SinS(pitch);
    dir->z = Math_CosS(yaw) * cp;
}

static void DirToAngles(const Vec3f& from, const Vec3f& to, s16* yaw, s16* pitch) {
    f32 dx = to.x - from.x;
    f32 dy = to.y - from.y;
    f32 dz = to.z - from.z;
    f32 dxz = sqrtf(dx * dx + dz * dz);
    *yaw = (s16)(atan2f(dx, dz) * (32768.0f / (f32)M_PI));
    *pitch = (s16)(-atan2f(dy, dxz) * (32768.0f / (f32)M_PI));
}

// Works out where a shot starts and which way it flies.
static void ComputeShot(Player* player, Vec3f* muzzle, s16* yaw, s16* pitch) {
    // Origin: the gun hand if we drew it recently, else a point in front of Link's chest.
    Vec3f origin;
    const bool handFresh = sHandPosValid && gPlayState != nullptr && (gPlayState->gameplayFrames - sHandPosFrame) < 10;
    if (handFresh) {
        origin = sHandPos;
    } else {
        f32 chest = LINK_IS_ADULT ? 55.0f : 40.0f;
        origin = player->actor.world.pos;
        origin.y += chest;
    }

    const bool firstPerson = (player->unk_6AD == 2);
    if (firstPerson) {
        // Aimed like a bow: stick / gyro / right stick drive focus.rot.
        *yaw = player->actor.focus.rot.y;
        *pitch = player->actor.focus.rot.x;
    } else if (player->focusActor != nullptr && player->focusActor->update != nullptr) {
        // Z-targeting: fire at the target.
        DirToAngles(origin, player->focusActor->focus.pos, yaw, pitch);
    } else {
        // Hip-fire: straight ahead.
        *yaw = player->actor.shape.rot.y;
        *pitch = 0;
    }

    Vec3f dir;
    AnglesToDir(*yaw, *pitch, &dir);
    const f32 muzzleDist = LINK_IS_ADULT ? 22.0f : 16.0f;
    muzzle->x = origin.x + dir.x * muzzleDist;
    muzzle->y = origin.y + dir.y * muzzleDist;
    muzzle->z = origin.z + dir.z * muzzleDist;
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing helpers (world space ribbons + flare)
// ---------------------------------------------------------------------------------------------------------------------

// Draws the real Beamos laser model (slim, blue, with its scrolling texture) from `start` along yaw / pitch.
// This is exactly how EnVm draws its own beam: x/y scale 0.01 is a normal Beamos laser, z scale is length * 0.0015.
static void DrawLaser(PlayState* play, const Vec3f& start, s16 yaw, s16 pitch, f32 length, f32 thickness) {
    if (length <= 0.5f || thickness <= 0.001f) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)func_80094E78(play->state.gfxCtx, 0, (u32)(play->gameplayFrames * 12)));

    Matrix_Push();
    Matrix_Translate(start.x, start.y, start.z, MTXMODE_NEW);
    Matrix_RotateZYX(pitch, yaw, 0, MTXMODE_APPLY);
    const f32 w = 0.01f * thickness;
    Matrix_Scale(w, w, length * 0.0015f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBeamosLaserDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFlare(PlayState* play, const Vec3f& pos, f32 scale, const Color_RGB8& prim, const Color_RGB8& env,
                      u8 alpha) {
    if (scale < 0.5f) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu2(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, prim.r, prim.g, prim.b, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, env.r, env.g, env.b, alpha);

    Matrix_Push();
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_RotateZ(((play->gameplayFrames & 0xFF) * 4000) * ((f32)M_PI / 0x8000), MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffSparklesDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---------------------------------------------------------------------------------------------------------------------
// The laser bolt actor ("En_BeamosShot") - registered through ActorDB so no overlay table changes are needed.
// ---------------------------------------------------------------------------------------------------------------------

struct EnBeamosShot {
    Actor actor;
    ColliderQuad collider;
    Vec3f spawnPos;
    Vec3f dir;
    s16 yaw;
    s16 pitch;
    s16 timer;
    s16 fadeTimer;
    u8 tier;
    u8 pierceLeft;
    u8 dying;
};

static ColliderQuadInit sShotColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x01 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static void BeamShot_Impact(PlayState* play, const Vec3f& pos, int tier, bool wall) {
    Vec3f p = pos;
    EffectSsHitMark_SpawnCustomScale(play, 0, wall ? 120 : 170, &p);
    Sfx_PlaySfxAtPos(&p, wall ? NA_SE_IT_HOOKSHOT_REFLECT : NA_SE_IT_ARROW_STICK_CRE);
    (void)tier;
}

static void BeamShot_Init(Actor* thisx, PlayState* play) {
    EnBeamosShot* self = (EnBeamosShot*)thisx;
    const int tier = std::clamp((int)thisx->params, 0, (int)GUN_TIER_COUNT - 1);
    const TierStats& stats = kTiers[tier];

    self->tier = (u8)tier;
    self->pierceLeft = stats.pierce;
    self->timer = stats.lifetime;
    self->fadeTimer = 0;
    self->dying = 0;
    self->yaw = thisx->shape.rot.y;
    self->pitch = thisx->shape.rot.x;
    AnglesToDir(self->yaw, self->pitch, &self->dir);
    self->spawnPos = thisx->world.pos;
    thisx->prevPos = thisx->world.pos;

    Collider_InitQuad(play, &self->collider);
    Collider_SetQuad(play, &self->collider, thisx, &sShotColliderInit);
    self->collider.info.toucher.dmgFlags = stats.dmgFlags;
}

static void BeamShot_Destroy(Actor* thisx, PlayState* play) {
    EnBeamosShot* self = (EnBeamosShot*)thisx;
    Collider_DestroyQuad(play, &self->collider);
}

static void BeamShot_UpdateCollider(EnBeamosShot* self, PlayState* play, const Vec3f& from, const Vec3f& to) {
    const TierStats& stats = kTiers[self->tier];

    // Build a ribbon whose plane is tilted 45 degrees around the flight direction so it catches targets both above /
    // below and left / right of the bolt's centre line.
    Vec3f up = { 0.0f, 1.0f, 0.0f };
    if (fabsf(self->dir.y) > 0.95f) {
        up = { 1.0f, 0.0f, 0.0f };
    }
    Vec3f s1 = { self->dir.y * up.z - self->dir.z * up.y, self->dir.z * up.x - self->dir.x * up.z,
                 self->dir.x * up.y - self->dir.y * up.x };
    f32 m1 = sqrtf(s1.x * s1.x + s1.y * s1.y + s1.z * s1.z);
    if (m1 < 0.0001f) {
        return;
    }
    s1.x /= m1;
    s1.y /= m1;
    s1.z /= m1;
    Vec3f s2 = { self->dir.y * s1.z - self->dir.z * s1.y, self->dir.z * s1.x - self->dir.x * s1.z,
                 self->dir.x * s1.y - self->dir.y * s1.x };
    Vec3f side = { s1.x + s2.x, s1.y + s2.y, s1.z + s2.z };
    f32 ms = sqrtf(side.x * side.x + side.y * side.y + side.z * side.z);
    if (ms < 0.0001f) {
        return;
    }
    const f32 w = stats.halfWidth / ms;
    side.x *= w;
    side.y *= w;
    side.z *= w;

    // Include a little tail so the collider also covers the volume the visible bolt occupies.
    Vec3f a = { to.x + side.x, to.y + side.y, to.z + side.z };
    Vec3f b = { to.x - side.x, to.y - side.y, to.z - side.z };
    Vec3f c = { from.x + side.x, from.y + side.y, from.z + side.z };
    Vec3f d = { from.x - side.x, from.y - side.y, from.z - side.z };

    Collider_ResetQuadAT(play, &self->collider.base);
    Collider_SetQuadVertices(&self->collider, &a, &b, &c, &d);
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);
}

static void BeamShot_Die(EnBeamosShot* self) {
    self->dying = 1;
    self->fadeTimer = 3;
    self->actor.speedXZ = 0.0f;
}

static void BeamShot_Update(Actor* thisx, PlayState* play) {
    EnBeamosShot* self = (EnBeamosShot*)thisx;
    const TierStats& stats = kTiers[self->tier];

    if (self->dying) {
        if (--self->fadeTimer <= 0) {
            Actor_Kill(thisx);
        }
        return;
    }

    if (--self->timer <= 0) {
        BeamShot_Die(self);
        return;
    }

    // Resolve what the quad hit last update.
    if (self->collider.base.atFlags & AT_HIT) {
        self->collider.base.atFlags &= ~AT_HIT;

        Vec3f hitPos = thisx->world.pos;
        if (self->collider.info.atHitInfo != nullptr && (self->collider.info.atHitInfo->bumperFlags & 2)) {
            hitPos.x = self->collider.info.atHitInfo->bumper.hitPos.x;
            hitPos.y = self->collider.info.atHitInfo->bumper.hitPos.y;
            hitPos.z = self->collider.info.atHitInfo->bumper.hitPos.z;
        }
        BeamShot_Impact(play, hitPos, self->tier, false);

        if ((self->collider.base.atFlags & AT_BOUNCED) || self->pierceLeft == 0) {
            thisx->world.pos = hitPos;
            BeamShot_Die(self);
            return;
        }
        self->pierceLeft--;
    }

    // Advance.
    thisx->prevPos = thisx->world.pos;
    thisx->world.pos.x += self->dir.x * stats.speed;
    thisx->world.pos.y += self->dir.y * stats.speed;
    thisx->world.pos.z += self->dir.z * stats.speed;

    // Walls / floors / ceilings.
    CollisionPoly* poly = nullptr;
    s32 bgId = 0;
    Vec3f hitPoint;
    if (BgCheck_ProjectileLineTest(&play->colCtx, &thisx->prevPos, &thisx->world.pos, &hitPoint, &poly, true, true, true,
                                   true, &bgId)) {
        thisx->world.pos = hitPoint;
        BeamShot_Impact(play, hitPoint, self->tier, true);
        BeamShot_UpdateCollider(self, play, thisx->prevPos, thisx->world.pos);
        BeamShot_Die(self);
        return;
    }

    BeamShot_UpdateCollider(self, play, thisx->prevPos, thisx->world.pos);
}

static void BeamShot_Draw(Actor* thisx, PlayState* play) {
    EnBeamosShot* self = (EnBeamosShot*)thisx;
    const TierStats& stats = kTiers[self->tier];

    const f32 travelled = sqrtf(Math3D_Vec3fDistSq(&thisx->world.pos, &self->spawnPos));
    const f32 length = std::min(stats.visualLength, travelled + 10.0f);
    const u8 fade = self->dying ? (u8)(255 * std::max(0, (int)self->fadeTimer) / 3) : 255;

    Vec3f tail = { thisx->world.pos.x - self->dir.x * length, thisx->world.pos.y - self->dir.y * length,
                   thisx->world.pos.z - self->dir.z * length };

    const f32 widthMul = CVarGetInteger(CVAR_BLASTER_BEAM_WIDTH, 100) / 100.0f;
    const f32 glowMul = CVarGetInteger(CVAR_BLASTER_GLOW_SIZE, 100) / 100.0f;
    const f32 fadeF = self->dying ? (f32)std::max(0, (int)self->fadeTimer) / 3.0f : 1.0f;

    // The beam itself, then a small spark on the head of the bolt.
    DrawLaser(play, tail, self->yaw, self->pitch, length, stats.beamThickness * widthMul * fadeF);
    DrawFlare(play, thisx->world.pos, stats.flareSize * glowMul * fadeF, stats.core, stats.glow, fade);
}

static void RegisterBeamActor() {
    if (sBeamActorId >= 0) {
        return;
    }

    ActorDBInit init;
    init.name = "En_BeamosShot";
    init.desc = "Beamos Blaster Bolt";
    init.category = ACTORCAT_ITEMACTION;
    init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    init.objectId = OBJECT_GAMEPLAY_KEEP;
    init.instanceSize = sizeof(EnBeamosShot);
    init.init = BeamShot_Init;
    init.destroy = BeamShot_Destroy;
    init.update = BeamShot_Update;
    init.draw = BeamShot_Draw;

    sBeamActorId = ActorDB::Instance->AddEntry(init).entry.id;
}

// ---------------------------------------------------------------------------------------------------------------------
// Firing
// ---------------------------------------------------------------------------------------------------------------------

extern "C" void BeamosBlaster_Fire(PlayState* play, Player* player) {
    const int tier = GetTier(player->heldItemAction);
    if (!sEnabled || tier < 0 || sBeamActorId < 0) {
        return;
    }

    if (sCooldown > 0) {
        Sfx_PlaySfxCentered(NA_SE_IT_BOW_FLICK); // gun is still recharging
        return;
    }

    Vec3f muzzle;
    s16 yaw, pitch;
    ComputeShot(player, &muzzle, &yaw, &pitch);

    // Don't let Link shoot through a wall he is pressed up against.
    Vec3f chest = player->actor.world.pos;
    chest.y += LINK_IS_ADULT ? 55.0f : 40.0f;
    CollisionPoly* poly = nullptr;
    s32 bgId = 0;
    Vec3f hitPoint;
    sCooldown = kTiers[tier].cooldown;
    if (BgCheck_ProjectileLineTest(&play->colCtx, &chest, &muzzle, &hitPoint, &poly, true, true, true, true, &bgId)) {
        BeamShot_Impact(play, hitPoint, tier, true);
        return;
    }

    Actor* shot = Actor_Spawn(&play->actorCtx, play, (s16)sBeamActorId, muzzle.x, muzzle.y, muzzle.z, pitch, yaw, 0,
                              (s16)tier);
    if (shot == nullptr) {
        return;
    }

    Sfx_PlaySfxAtPos(&muzzle, NA_SE_IT_MAGIC_ARROW_SHOT); // one-shot (the Beamos laser sfx is a loop)
}

// ---------------------------------------------------------------------------------------------------------------------
// Gun model. Built procedurally (no asset files needed): a chunky body, a Beamos "head" ring and a glowing red eye.
// ---------------------------------------------------------------------------------------------------------------------

struct Vec3 {
    f32 x, y, z;
};

struct MeshChunk {
    std::vector<Vtx> verts;
    std::vector<u8> tris; // 3 indices per triangle
    bool emissive = false;
};

struct GunMesh {
    std::vector<MeshChunk> chunks;
};

struct GunStyle {
    Color_RGB8 body;
    Color_RGB8 trim;
    Color_RGB8 eye;
    f32 barrelLen; // extra barrel length in mesh units
    f32 bulk;      // body thickness multiplier
};

static const GunStyle kStyles[GUN_TIER_COUNT] = {
    { { 96, 104, 120 }, { 190, 60, 40 }, { 255, 60, 40 }, 0.0f, 0.85f },
    { { 70, 76, 92 }, { 235, 140, 30 }, { 255, 110, 20 }, 45.0f, 1.0f },
    { { 38, 40, 52 }, { 210, 30, 30 }, { 255, 30, 30 }, 95.0f, 1.2f },
};

static MeshChunk& ChunkFor(GunMesh& mesh, int needVerts, bool emissive) {
    if (mesh.chunks.empty() || mesh.chunks.back().emissive != emissive ||
        mesh.chunks.back().verts.size() + needVerts > 30) {
        mesh.chunks.emplace_back();
        mesh.chunks.back().emissive = emissive;
    }
    return mesh.chunks.back();
}

static u8 ShadeChannel(int base, f32 lit) {
    return (u8)std::clamp((int)(base * lit), 0, 255);
}

static void AddFace(MeshChunk& chunk, const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& n,
                    const Color_RGB8& col) {
    // Baked flat shading: brighter on faces turned toward the "light" (up / right / front).
    f32 lit = 0.45f + 0.55f * std::max(0.0f, n.x * 0.35f + n.y * 0.8f + n.z * 0.45f);
    lit = std::min(lit, 1.0f);
    const Vec3* pts[4] = { &p0, &p1, &p2, &p3 };
    const u8 base = (u8)chunk.verts.size();
    for (int i = 0; i < 4; i++) {
        Vtx v;
        v.v.ob[0] = (s16)pts[i]->x;
        v.v.ob[1] = (s16)pts[i]->y;
        v.v.ob[2] = (s16)pts[i]->z;
        v.v.flag = 0;
        v.v.tc[0] = 0;
        v.v.tc[1] = 0;
        v.v.cn[0] = ShadeChannel(col.r, lit);
        v.v.cn[1] = ShadeChannel(col.g, lit);
        v.v.cn[2] = ShadeChannel(col.b, lit);
        v.v.cn[3] = 255;
        chunk.verts.push_back(v);
    }
    chunk.tris.insert(chunk.tris.end(), { base, (u8)(base + 1), (u8)(base + 2), base, (u8)(base + 2), (u8)(base + 3) });
}

static void AddBox(GunMesh& mesh, f32 cx, f32 cy, f32 cz, f32 hx, f32 hy, f32 hz, const Color_RGB8& col,
                   bool emissive = false) {
    MeshChunk& c = ChunkFor(mesh, 24, emissive);
    const f32 x0 = cx - hx, x1 = cx + hx, y0 = cy - hy, y1 = cy + hy, z0 = cz - hz, z1 = cz + hz;
    AddFace(c, { x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { 0, 0, 1 }, col);   // front (+Z)
    AddFace(c, { x1, y0, z0 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { 0, 0, -1 }, col);  // back
    AddFace(c, { x1, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { 1, 0, 0 }, col);   // right
    AddFace(c, { x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, { -1, 0, 0 }, col);  // left
    AddFace(c, { x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 }, { x0, y1, z0 }, { 0, 1, 0 }, col);   // top
    AddFace(c, { x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 }, { 0, -1, 0 }, col);  // bottom
}

static void AddTri(MeshChunk& chunk, const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& n,
                   const Color_RGB8& col) {
    f32 lit = 0.45f + 0.55f * std::max(0.0f, n.x * 0.35f + n.y * 0.8f + n.z * 0.45f);
    lit = std::min(lit, 1.0f);
    const Vec3* pts[3] = { &p0, &p1, &p2 };
    const u8 base = (u8)chunk.verts.size();
    for (int i = 0; i < 3; i++) {
        Vtx v;
        v.v.ob[0] = (s16)pts[i]->x;
        v.v.ob[1] = (s16)pts[i]->y;
        v.v.ob[2] = (s16)pts[i]->z;
        v.v.flag = 0;
        v.v.tc[0] = 0;
        v.v.tc[1] = 0;
        v.v.cn[0] = ShadeChannel(col.r, lit);
        v.v.cn[1] = ShadeChannel(col.g, lit);
        v.v.cn[2] = ShadeChannel(col.b, lit);
        v.v.cn[3] = 255;
        chunk.verts.push_back(v);
    }
    chunk.tris.insert(chunk.tris.end(), { base, (u8)(base + 1), (u8)(base + 2) });
}

// A squashed octahedron - the Beamos eye. Faceted, so it reads as a jewel. 8 faces * 3 vertices = 24 vertices.
static void AddEye(GunMesh& mesh, f32 cx, f32 cy, f32 cz, f32 r, bool emissive) {
    MeshChunk& c = ChunkFor(mesh, 24, emissive);
    const Vec3 top = { cx, cy + r, cz }, bot = { cx, cy - r, cz };
    const Vec3 fr = { cx, cy, cz + r }, bk = { cx, cy, cz - r };
    const Vec3 rt = { cx + r, cy, cz }, lf = { cx - r, cy, cz };
    const Color_RGB8 white = { 255, 255, 255 };
    AddTri(c, top, fr, rt, { 0.58f, 0.58f, 0.58f }, white);
    AddTri(c, top, rt, bk, { 0.58f, 0.58f, -0.58f }, white);
    AddTri(c, top, bk, lf, { -0.58f, 0.58f, -0.58f }, white);
    AddTri(c, top, lf, fr, { -0.58f, 0.58f, 0.58f }, white);
    AddTri(c, bot, rt, fr, { 0.58f, -0.58f, 0.58f }, white);
    AddTri(c, bot, bk, rt, { 0.58f, -0.58f, -0.58f }, white);
    AddTri(c, bot, lf, bk, { -0.58f, -0.58f, -0.58f }, white);
    AddTri(c, bot, fr, lf, { -0.58f, -0.58f, 0.58f }, white);
}

// Mesh units: 1 unit == 1 mm of a ~40 cm gun. +Z is the barrel direction, +Y is up, the grip is at the origin.
static GunMesh BuildGun(int tier) {
    const GunStyle& s = kStyles[tier];
    GunMesh mesh;
    const f32 b = s.bulk;
    const f32 barrelEnd = 250.0f + s.barrelLen;

    // Grip + trigger guard
    AddBox(mesh, 0, -10, -10, 17 * b, 52, 20 * b, s.body);
    AddBox(mesh, 0, -50, 25, 5 * b, 5, 24, s.trim);
    // Receiver
    AddBox(mesh, 0, 70, 60, 30 * b, 34 * b, 105, s.body);
    // Energy cells on the sides (tier trim colour)
    AddBox(mesh, 36 * b, 66, 40, 7 * b, 20 * b, 55, s.trim);
    AddBox(mesh, -36 * b, 66, 40, 7 * b, 20 * b, 55, s.trim);
    // Top sight rail
    AddBox(mesh, 0, 112, 35, 7, 9, 62, s.body);
    // Barrel
    AddBox(mesh, 0, 74, 120 + (barrelEnd - 250.0f) * 0.4f + 70, 14 * b, 14 * b, 80 + s.barrelLen * 0.5f, s.body);
    // Beamos head housing ring at the muzzle
    AddBox(mesh, 0, 74, barrelEnd - 10, 44 * b, 44 * b, 14, s.trim);
    // Beamos eye (separate emissive chunk so it can pulse)
    AddEye(mesh, 0, 74, barrelEnd + 22, 34 * b, true);
    return mesh;
}

static GunMesh sMeshes[GUN_TIER_COUNT];
static bool sMeshesBuilt = false;

static void EnsureMeshes() {
    if (sMeshesBuilt) {
        return;
    }
    for (int i = 0; i < GUN_TIER_COUNT; i++) {
        sMeshes[i] = BuildGun(i);
    }
    sMeshesBuilt = true;
}

// Rotates the model so its barrel (+Z) points along one of the six limb axes, then rolls it about the barrel.
// Index = forwardAxis * 4 + roll, forwardAxis: 0 +Z, 1 -Z, 2 +X, 3 -X, 4 +Y, 5 -Y; roll in 90 degree steps.
// The hand limb's axes can't be verified without running the game, so this is exposed as a menu slider.
static void ApplyOrientationPreset(int preset) {
    preset = std::clamp(preset, 0, 23);
    const int forward = preset / 4;
    const int roll = preset % 4;
    const f32 halfPi = (f32)M_PI / 2.0f;

    switch (forward) {
        case 1:
            Matrix_RotateY((f32)M_PI, MTXMODE_APPLY);
            break;
        case 2:
            Matrix_RotateY(halfPi, MTXMODE_APPLY);
            break;
        case 3:
            Matrix_RotateY(-halfPi, MTXMODE_APPLY);
            break;
        case 4:
            Matrix_RotateX(-halfPi, MTXMODE_APPLY);
            break;
        case 5:
            Matrix_RotateX(halfPi, MTXMODE_APPLY);
            break;
        default:
            break;
    }
    if (roll != 0) {
        Matrix_RotateZ(halfPi * roll, MTXMODE_APPLY);
    }
}

static void DrawGunMesh(PlayState* play, int tier) {
    EnsureMeshes();
    const GunMesh& mesh = sMeshes[tier];

    // Pulsing eye: faster and brighter while the player is aiming.
    const bool aiming = (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) != 0;
    const f32 pulse = 0.5f + 0.5f * Math_SinS((s16)(play->gameplayFrames * (aiming ? 5000 : 1800)));
    const Color_RGB8& eye = kStyles[tier].eye;
    const f32 level = aiming ? (0.7f + 0.3f * pulse) : (0.35f + 0.4f * pulse);

    OPEN_DISPS(play->state.gfxCtx);

    // Vertex colours are used directly, so lighting (which reinterprets them as normals) must be off.
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BACK);

    for (const MeshChunk& chunk : mesh.chunks) {
        if (chunk.emissive) {
            gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, 1, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, 1);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, (u8)(eye.r * level), (u8)(eye.g * level), (u8)(eye.b * level), 255);
        } else {
            gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
        }
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)chunk.verts.data(), (int)chunk.verts.size(), 0);
        for (size_t i = 0; i + 2 < chunk.tris.size(); i += 3) {
            gSP1Triangle(POLY_OPA_DISP++, chunk.tris[i], chunk.tris[i + 1], chunk.tris[i + 2], 0);
        }
    }

    gSPSetGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BACK);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void BeamosBlaster_DrawInHand(PlayState* play, Player* player) {
    const int tier = GetTier(player->heldItemAction);
    if (!sEnabled || tier < 0) {
        return;
    }

    // Remember where the hand is in the world: shots start here.
    static Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Matrix_MultVec3f(&zero, &sHandPos);
    sHandPosFrame = play->gameplayFrames;
    sHandPosValid = true;

    // Model transform: hand limb space is ~100 units per cm of Link's model.
    const f32 userScale = CVarGetInteger(CVAR_BLASTER_MODEL_SCALE, 65) / 100.0f;
    const f32 scale = 10.0f * userScale * kTiers[tier].modelScale;
    const f32 offStep = 25.0f;

    Matrix_Push();
    Matrix_Translate(CVarGetInteger(CVAR_BLASTER_MODEL_OFF_X, 0) * offStep,
                     CVarGetInteger(CVAR_BLASTER_MODEL_OFF_Y, 0) * offStep,
                     CVarGetInteger(CVAR_BLASTER_MODEL_OFF_Z, 0) * offStep, MTXMODE_APPLY);
    ApplyOrientationPreset(CVarGetInteger(CVAR_BLASTER_MODEL_ORIENT, 8));
    const f32 deg = (f32)M_PI / 180.0f;
    Matrix_RotateX(CVarGetInteger(CVAR_BLASTER_MODEL_ROT_X, 0) * deg, MTXMODE_APPLY);
    Matrix_RotateY(CVarGetInteger(CVAR_BLASTER_MODEL_ROT_Y, 0) * deg, MTXMODE_APPLY);
    Matrix_RotateZ(CVarGetInteger(CVAR_BLASTER_MODEL_ROT_Z, 0) * deg, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    CLOSE_DISPS(play->state.gfxCtx);

    DrawGunMesh(play, tier);
    Matrix_Pop();
}

// ---------------------------------------------------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------------------------------------------------

// Beamos-style targeting laser while aiming. Drawn after the world, not inside Link's limb draw, so it cannot disturb
// the render state of his model.
static void DrawAimSight() {
    PlayState* play = gPlayState;
    if (!sEnabled || play == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(play);
    const int tier = GetTier(player->heldItemAction);
    if (tier < 0 || player->itemAction != player->heldItemAction ||
        !(player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE)) {
        return;
    }

    Vec3f muzzle;
    s16 yaw, pitch;
    ComputeShot(player, &muzzle, &yaw, &pitch);

    Vec3f dir;
    AnglesToDir(yaw, pitch, &dir);
    const f32 range = 1200.0f;
    Vec3f end = { muzzle.x + dir.x * range, muzzle.y + dir.y * range, muzzle.z + dir.z * range };

    CollisionPoly* poly = nullptr;
    s32 bgId = 0;
    Vec3f hitPoint;
    const bool hitSomething =
        BgCheck_ProjectileLineTest(&play->colCtx, &muzzle, &end, &hitPoint, &poly, true, true, true, true, &bgId);
    if (hitSomething) {
        end = hitPoint;
    }
    const f32 len = sqrtf(Math3D_Vec3fDistSq(&muzzle, &end));
    const TierStats& stats = kTiers[tier];
    const f32 widthMul = CVarGetInteger(CVAR_BLASTER_BEAM_WIDTH, 100) / 100.0f;
    const f32 glowMul = CVarGetInteger(CVAR_BLASTER_GLOW_SIZE, 100) / 100.0f;

    DrawLaser(play, muzzle, yaw, pitch, len, 0.16f * widthMul);
    if (hitSomething) {
        DrawFlare(play, end, 6.0f * glowMul, stats.core, stats.glow, 200);
    }
}

static void RegisterBeamosBlaster() {
    sEnabled = CVarGetInteger(CVAR_BLASTER_ENABLED, 0) != 0;

    if (sEnabled) {
        RegisterBeamActor();
    }
    if (!sEnabled) {
        ResetInputState();
        sCooldown = 0;
    }

    COND_HOOK(OnPlayDrawEnd, sEnabled, DrawAimSight);

    // Runs right before the player's action function every update, with the player's controller input.
    COND_VB_SHOULD(VB_EXECUTE_PLAYER_ACTION_FUNC, sEnabled, {
        Player* player = (Player*)va_arg(args, void*);
        Input* input = (Input*)va_arg(args, void*);
        TrackInput(player, input);
    });
}

static RegisterShipInitFunc initFunc(RegisterBeamosBlaster, { CVAR_BLASTER_ENABLED });
