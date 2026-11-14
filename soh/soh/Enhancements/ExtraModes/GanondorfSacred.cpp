// SoH: "Play as Ganondorf", the way into the Sacred Realm Temple (stage 1).
//
// In the Temple of Time (adult), a sealed door stands in the Master Sword chamber's west wall: no keyhole, so no key fits,
// only a barred window through which the next room's glow shows. Only Ganondorf opens it: he kicks it in (the door
// kick, z_boss_ganon.c, asks GdfSacred_IsShut / GdfSacred_KickOpen). It stays open (a switch flag of the Temple of
// Time). Behind it is the Mirror Room (SCENE_SASATEST), six pillars for the six Sages and, on a dais, the Sages'
// mirror: the colours of all six swirl over its glass and sparks of them rise about it. Stepping into it takes him to
// the Sacred Realm Temple's first hall (SCENE_SYOTES), which has a mirror of its own to go back by.
//
// The rooms and the models are generated (tools/gdf_castle/sacred.py) and shipped as mods/gdf_sacred.o2r; they hold
// our own geometry and refer to the game's own textures by path.

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/frame_interpolation.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
extern PlayState* gPlayState;
}

#define MIRROR_ROOM_SCENE SCENE_SASATEST
#define MIRROR_ROOM_ENTRANCE ENTR_SASATEST_0
#define FIRST_HALL_SCENE SCENE_SYOTES
#define FIRST_HALL_ENTRANCE ENTR_SYOTES_0
#define SEALED_DOOR_FLAG 0x2F // (a Temple of Time switch flag nothing else uses: the door is broken in)
#define HUB_SCENE SCENE_SYOTES2
#define HUB_ENTRANCE ENTR_SYOTES2_0
#define GREAT_DOOR_FLAG 0x01 // (the first hall's switch flag: the great door is open)
#define GREAT_DOOR_RISE 345.0f

static const char sDoorFrameDL[] = "__OTR__objects/gdf_sacred/gGdfSealedDoorFrameDL";
static const char sDoorSlabDL[] = "__OTR__objects/gdf_sacred/gGdfSealedDoorSlabDL";
static const char sDoorwayDL[] = "__OTR__objects/gdf_sacred/gGdfSealedDoorwayDL";
static const char sMirrorFrameDL[] = "__OTR__objects/gdf_sacred/gGdfMirrorFrameDL";
static const char sMirrorGlassDL[] = "__OTR__objects/gdf_sacred/gGdfMirrorGlassDL";
static const char sGreatDoorDL[] = "__OTR__objects/gdf_sacred/gGdfGreatDoorDL";
static const char sGreatDoorCol[] = "__OTR__objects/gdf_sacred/gGdfGreatDoorCol";
static const char* sGreatDoorTriDL[3] = { "__OTR__objects/gdf_sacred/gGdfGreatDoorTri0DL",
                                          "__OTR__objects/gdf_sacred/gGdfGreatDoorTri1DL",
                                          "__OTR__objects/gdf_sacred/gGdfGreatDoorTri2DL" };
static const char sTriPlateDL[] = "__OTR__objects/gdf_sacred/gGdfTriPlateDL";
static const char sTriPlateRimDL[] = "__OTR__objects/gdf_sacred/gGdfTriPlateRimDL";

// The first hall's great door (its foot, in the north wall's opening) and, before it, the three Triforce plates laid
// out as the door's triangles are: top, left, right
static const Vec3f sGreatDoorPos = { 0.0f, 0.0f, -1090.0f };
static const Vec3f sPlatePos[3] = { { 0.0f, 0.0f, -960.0f }, { -130.0f, 0.0f, -840.0f }, { 130.0f, 0.0f, -840.0f } };
// The order the door's triangles show, and the plates must be stepped in: left, right, then the top
static const s8 sPlateOrder[3] = { 1, 2, 0 };

// The door: at the far end, in the Master Sword's chamber, in its left (west) wall (x -720, the flat wall facing the
// pedestal across the chamber), facing in (it stands sPorch out from the wall: a porch, with a passage behind it back
// to the wall, fading into the dark)
static const f32 sPorch = 140.0f; // (PORCH in tools/gdf_castle/sacred.py)
static const Vec3f sDoorPos = { -720.0f + 140.0f, 0.0f, 0.0f };
// The Mirror Room: its mirror at the north end, on the dais; the passage's end (the broken door) at the south
static const Vec3f sRoomMirrorPos = { 0.0f, 24.0f, -345.0f };
// The first hall: its mirror on the south wall, facing north into the hall
static const Vec3f sHallMirrorPos = { 0.0f, 0.0f, 290.0f };

// The Sages' colours: Forest, Fire, Water, Shadow, Spirit, Light
static const Color_RGBA8 sSageColors[6] = {
    { 70, 230, 80, 255 }, { 255, 80, 40, 255 }, { 60, 130, 255, 255 },
    { 175, 70, 235, 255 }, { 255, 160, 40, 255 }, { 255, 235, 110, 255 },
};

typedef struct {
    Actor actor;
    s16 timer;
} GdfSacredActor;

// The Temple of Time's sunbeams (Demo_Kankyo's light plane), as big as each room's windows (sacred.py)
static f32 BeamScale(s16 scene) {
    return (scene == MIRROR_ROOM_SCENE) ? 0.26f : (scene == FIRST_HALL_SCENE) ? 0.3f : 0.38f;
}

typedef struct {
    DynaPolyActor dyna;
    s16 timer;
    s8 step;       // how many plates have been stepped in the right order
    s8 onPlate;    // the plate he stands on (-1: none)
    f32 rise;      // how far the door has risen
    u8 opening;
} GdfGreatDoor;

static s32 sGreatDoorActorId = -1;
static s32 sDoorActorId = -1;
static s32 sMirrorActorId = -1;
static s16 sLastScene = -1;
static bool sSpawnPending = false;
static s16 sArrival = 0; // where to put him once he's in: 1 out of the broken door, 2 out of the room's mirror

static bool SacredAvailable() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.SacredRealm"), 1) &&
           ResourceMgr_FileExists("objects/gdf_sacred/gGdfSealedDoorFrameDL");
}

static void Warp(PlayState* play, s16 entrance) {
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    gSaveContext.respawnFlag = 0;
    gSaveContext.cutsceneIndex = 0;
}

static bool CanLeave(PlayState* play) {
    return (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE);
}

// Sparks of the Sages' colours, drifting up from about `pos`
static void SageSparks(PlayState* play, Vec3f* pos, f32 spread, s32 count, f32 rise) {
    static Vec3f sNoAccel = { 0.0f, 0.0f, 0.0f };

    for (s32 i = 0; i < count; i++) {
        Color_RGBA8 prim = sSageColors[(s32)Rand_ZeroFloat(6.0f) % 6];
        Color_RGBA8 env = prim;
        Vec3f p = { pos->x + Rand_CenteredFloat(spread), pos->y + Rand_CenteredFloat(spread),
                    pos->z + Rand_CenteredFloat(spread) };
        Vec3f v = { Rand_CenteredFloat(0.6f), rise + Rand_ZeroFloat(rise), Rand_CenteredFloat(0.6f) };

        prim.r = (u8)MIN(255, prim.r + 80);
        prim.g = (u8)MIN(255, prim.g + 80);
        prim.b = (u8)MIN(255, prim.b + 80);
        env.a = 0;
        EffectSsKiraKira_SpawnDispersed(play, &p, &v, &sNoAccel, &prim, &env, (s16)(Rand_ZeroFloat(900.0f) + 1100.0f),
                                        20);
    }
}

// ---- The sealed door ----

static bool DoorIsOpen(PlayState* play) {
    return Flags_GetSwitch(play, SEALED_DOOR_FLAG);
}

static void Door_Init(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, 1.0f);
}

static void Door_Update(Actor* thisx, PlayState* play) {
    GdfSacredActor* self = (GdfSacredActor*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f p = player->actor.world.pos;

    self->timer++;
    // The porch is the actor's own model, without collision: keep him out of it while the door is shut, and between
    // the passage's walls once it's open
    if ((fabsf(p.y - sDoorPos.y) < 120.0f) && (p.x < sDoorPos.x + 22.0f) && (p.x > sDoorPos.x - sPorch - 10.0f)) {
        if (!DoorIsOpen(play)) {
            if (fabsf(p.z - sDoorPos.z) < 125.0f) {
                player->actor.world.pos.x = sDoorPos.x + 22.0f;
            }
        } else if (p.x < sDoorPos.x + 4.0f) {
            f32 dz = p.z - sDoorPos.z;
            if (fabsf(dz) < 125.0f) {
                player->actor.world.pos.z = sDoorPos.z + CLAMP(dz, -50.0f, 50.0f);
            }
        } else if ((fabsf(p.z - sDoorPos.z) > 80.0f) && (fabsf(p.z - sDoorPos.z) < 125.0f)) {
            player->actor.world.pos.x = sDoorPos.x + 22.0f; // (the porch's front, either side of the doorway)
        }
    }
    p = player->actor.world.pos;
    if (!DoorIsOpen(play)) {
        // The glow beyond the bars: sparks of the Sages' colours in the window now and then
        if ((self->timer % 6) == 0) {
            Vec3f w = { sDoorPos.x - 14.0f, sDoorPos.y + 165.0f, sDoorPos.z };

            SageSparks(play, &w, 40.0f, 1, 0.4f);
        }
        return;
    }
    // Broken in: walking into the doorway takes him through
    if (CanLeave(play) && (p.x < sDoorPos.x - 50.0f) && (fabsf(p.z - sDoorPos.z) < 70.0f) &&
        (fabsf(p.y - sDoorPos.y) < 60.0f)) {
        Warp(play, MIRROR_ROOM_ENTRANCE);
    }
}

static void Door_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDoorFrameDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(DoorIsOpen(play) ? sDoorwayDL : sDoorSlabDL));

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- The Sages' mirror ----

static void Mirror_Init(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, 1.0f);
}

static void Mirror_Update(Actor* thisx, PlayState* play) {
    GdfSacredActor* self = (GdfSacredActor*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f front = thisx->world.pos;
    f32 fx = Math_SinS(thisx->shape.rot.y);
    f32 fz = Math_CosS(thisx->shape.rot.y);
    Vec3f d = { player->actor.world.pos.x - front.x, 0.0f, player->actor.world.pos.z - front.z };
    f32 ahead = d.x * fx + d.z * fz;   // how far in front of the glass he stands
    f32 side = d.x * fz - d.z * fx;     // and how far to one side

    self->timer++;
    // Its aura: sparks of the six colours rising from the glass's edges, and a low hum
    if ((self->timer % 3) == 0) {
        Vec3f e = front;

        e.y += 30.0f + Rand_ZeroFloat(300.0f);
        e.x += fz * Rand_CenteredFloat(230.0f) + fx * 10.0f;
        e.z += -fx * Rand_CenteredFloat(230.0f) + fz * 10.0f;
        SageSparks(play, &e, 20.0f, 1, 0.8f);
    }
    Actor_PlaySfx_Flagged(thisx, NA_SE_EV_FANTOM_WARP_L - SFX_FLAG);
    // In the Mirror Room, a Sage's colour over each pillar
    if ((play->sceneNum == MIRROR_ROOM_SCENE) && ((self->timer % 8) == 0)) {
        static const Vec2f sPillars[6] = { { -240, -200 }, { -240, -20 }, { -240, 160 },
                                           { 240, 160 },   { 240, -20 },  { 240, -200 } };
        s32 i = (self->timer / 8) % 6;
        Vec3f top = { sPillars[i].x, 360.0f, sPillars[i].y };
        Color_RGBA8 prim = sSageColors[i];
        Color_RGBA8 env = prim;
        Vec3f v = { 0.0f, 0.5f, 0.0f };
        Vec3f a = { 0.0f, 0.0f, 0.0f };

        env.a = 0;
        EffectSsKiraKira_SpawnDispersed(play, &top, &v, &a, &prim, &env, 2400, 30);
    }
    // Stepping into it
    if (CanLeave(play) && (ahead < 50.0f) && (ahead > -30.0f) && (fabsf(side) < 100.0f) &&
        (fabsf(player->actor.world.pos.y - front.y) < 80.0f)) {
        Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE_FLASH);
        SageSparks(play, &player->actor.world.pos, 60.0f, 12, 1.5f);
        Warp(play, (play->sceneNum == MIRROR_ROOM_SCENE) ? FIRST_HALL_ENTRANCE : MIRROR_ROOM_ENTRANCE);
    }
}

static void Mirror_Draw(Actor* thisx, PlayState* play) {
    GdfSacredActor* self = (GdfSacredActor*)thisx;
    // The glass: the six colours, one melting into the next
    f32 t = self->timer / 40.0f;
    s32 a = (s32)t % 6;
    s32 b = (a + 1) % 6;
    f32 w = t - floorf(t);
    u8 r = (u8)(sSageColors[a].r + (sSageColors[b].r - sSageColors[a].r) * w);
    u8 g = (u8)(sSageColors[a].g + (sSageColors[b].g - sSageColors[a].g) * w);
    u8 bl = (u8)(sSageColors[a].b + (sSageColors[b].b - sSageColors[a].b) * w);
    f32 shimmer = 0.75f + 0.25f * Math_SinS(self->timer * 0x0900);

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMirrorFrameDL);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, (u8)(MIN(255, 120 + r * shimmer)), (u8)(MIN(255, 120 + g * shimmer)),
                    (u8)(MIN(255, 140 + bl * shimmer)), 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMirrorGlassDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- The great door and its Triforce plates ----

static void GreatDoor_Init(Actor* thisx, PlayState* play) {
    GdfGreatDoor* self = (GdfGreatDoor*)thisx;
    CollisionHeader* col = NULL;

    Actor_SetScale(thisx, 1.0f);
    DynaPolyActor_Init(&self->dyna, 0);
    CollisionHeader_GetVirtual((void*)sGreatDoorCol, &col);
    self->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, col);
    self->step = 0;
    self->onPlate = -1;
    self->opening = false;
    self->rise = Flags_GetSwitch(play, GREAT_DOOR_FLAG) ? GREAT_DOOR_RISE : 0.0f;
    thisx->world.pos.y = thisx->home.pos.y + self->rise;
}

static void GreatDoor_Destroy(Actor* thisx, PlayState* play) {
    GdfGreatDoor* self = (GdfGreatDoor*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, self->dyna.bgId);
}

static void GreatDoor_Update(Actor* thisx, PlayState* play) {
    GdfGreatDoor* self = (GdfGreatDoor*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f p = player->actor.world.pos;
    s8 on = -1;

    self->timer++;
    if (self->rise >= GREAT_DOOR_RISE) {
        // Open: walking on through the passage behind it
        if (CanLeave(play) && (p.z < sGreatDoorPos.z - 40.0f) && (fabsf(p.x) < 150.0f)) {
            Warp(play, HUB_ENTRANCE);
        }
        return;
    }
    if (self->opening) {
        // Grinding up into the wall above
        self->rise = MIN(GREAT_DOOR_RISE, self->rise + 3.0f);
        thisx->world.pos.y = thisx->home.pos.y + self->rise;
        Actor_PlaySfx_Flagged(thisx, NA_SE_EV_STONE_STATUE_OPEN - SFX_FLAG);
        if ((self->timer % 4) == 0) {
            Actor_RequestQuakeAndRumble(thisx, play, 1, 4);
        }
        if (self->rise >= GREAT_DOOR_RISE) {
            Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
            Actor_RequestQuakeAndRumble(thisx, play, 4, 10);
        }
        return;
    }
    // The plates: stepping onto one (not standing on it) counts
    for (s32 i = 0; i < 3; i++) {
        if ((SQ(p.x - sPlatePos[i].x) + SQ(p.z - sPlatePos[i].z) < SQ(55.0f)) && (p.y < sPlatePos[i].y + 25.0f)) {
            on = i;
        }
    }
    if ((on >= 0) && (on != self->onPlate)) {
        if (on == sPlateOrder[self->step]) {
            self->step++;
            if (self->step >= 3) {
                Audio_PlaySfxGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                Flags_SetSwitch(play, GREAT_DOOR_FLAG);
                self->opening = true;
            } else {
                Audio_PlaySfxGeneral(NA_SE_SY_TRE_BOX_APPEAR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        } else if (self->step > 0) {
            // Out of order: the plates go dark, from the beginning again
            Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            self->step = (on == sPlateOrder[0]) ? 1 : 0;
        }
    }
    self->onPlate = on;
}

static void SetGlow(PlayState* play, u8 lit) {
    OPEN_DISPS(play->state.gfxCtx);
    if (lit) {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 245, 190, 255);
    } else {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 130, 100, 50, 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void GreatDoor_Draw(Actor* thisx, PlayState* play) {
    GdfGreatDoor* self = (GdfGreatDoor*)thisx;
    bool open = self->opening || (self->rise > 0.0f);
    // The door shows the order: its triangles light one after another (left, right, top), then all go dark a while
    s32 beat = (self->timer / 25) % 5;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGreatDoorDL);
    for (s32 t = 0; t < 3; t++) {
        bool lit = open;

        for (s32 k = 0; k < 3; k++) {
            if (sPlateOrder[k] == t) {
                lit = lit || (beat == k) || (k < self->step);
            }
        }
        SetGlow(play, lit);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGreatDoorTriDL[t]);
    }
    // The plates on the floor before it, each lit once stepped on in its turn
    for (s32 i = 0; i < 3; i++) {
        bool lit = open;

        for (s32 k = 0; k < self->step; k++) {
            lit = lit || (sPlateOrder[k] == i);
        }
        Matrix_Translate(sPlatePos[i].x, sPlatePos[i].y, sPlatePos[i].z, MTXMODE_NEW);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sTriPlateRimDL);
        SetGlow(play, lit);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sTriPlateDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterActors() {
    if (sGreatDoorActorId < 0) {
        ActorDBInit init;
        init.name = "Gdf_GreatDoor";
        init.desc = "The Sacred Realm Temple's great door (Play as Ganondorf)";
        init.category = ACTORCAT_BG;
        init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = sizeof(GdfGreatDoor);
        init.init = GreatDoor_Init;
        init.destroy = GreatDoor_Destroy;
        init.update = GreatDoor_Update;
        init.draw = GreatDoor_Draw;
        sGreatDoorActorId = ActorDB::Instance->AddEntry(init).entry.id;
    }
    if (sDoorActorId < 0) {
        ActorDBInit init;
        init.name = "Gdf_SealedDoor";
        init.desc = "Sealed door to the Sages' mirror (Play as Ganondorf)";
        init.category = ACTORCAT_BG;
        init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = sizeof(GdfSacredActor);
        init.init = Door_Init;
        init.update = Door_Update;
        init.draw = Door_Draw;
        sDoorActorId = ActorDB::Instance->AddEntry(init).entry.id;
    }
    if (sMirrorActorId < 0) {
        ActorDBInit init;
        init.name = "Gdf_SageMirror";
        init.desc = "The Sages' mirror (Play as Ganondorf)";
        init.category = ACTORCAT_BG;
        init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = sizeof(GdfSacredActor);
        init.init = Mirror_Init;
        init.update = Mirror_Update;
        init.draw = Mirror_Draw;
        sMirrorActorId = ActorDB::Instance->AddEntry(init).entry.id;
    }
}

extern "C" s32 GdfSacred_IsAvailable(void) {
    return SacredAvailable();
}

extern "C" void GdfSacred_VisitMirrorRoom(void) {
    if ((gPlayState != NULL) && SacredAvailable()) {
        RegisterActors();
        Warp(gPlayState, MIRROR_ROOM_ENTRANCE);
    }
}

// ---- His kick (z_boss_ganon.c) ----

// (the Temple of Light's key doors, boss door, chests and kick block answer his kick too: GanondorfLightTemple.cpp)
extern "C" s32 GdfTemple_IsKickable(Actor* a);
extern "C" s32 GdfTemple_IsShut(Actor* a);
extern "C" void GdfTemple_Kick(Actor* a, PlayState* play);

extern "C" s32 GdfSacred_IsSealedDoor(Actor* a) {
    return ((sDoorActorId >= 0) && (a->id == sDoorActorId)) || GdfTemple_IsKickable(a);
}

extern "C" s32 GdfSacred_IsShut(Actor* a) {
    if (GdfTemple_IsKickable(a)) {
        return GdfTemple_IsShut(a);
    }
    return GdfSacred_IsSealedDoor(a) && (gPlayState != NULL) && !DoorIsOpen(gPlayState);
}

extern "C" void GdfSacred_KickOpen(Actor* a, PlayState* play) {
    if (GdfTemple_IsKickable(a)) {
        GdfTemple_Kick(a, play);
        return;
    }
    Vec3f mid = { sDoorPos.x + 20.0f, sDoorPos.y + 120.0f, sDoorPos.z };

    Flags_SetSwitch(play, SEALED_DOOR_FLAG);
    Audio_PlayActorSound2(a, NA_SE_EV_WALL_BROKEN);
    Audio_PlayActorSound2(a, NA_SE_EV_TRIFORCE_FLASH);
    EffectSsHahen_SpawnBurst(play, &mid, 16.0f, 0, 20, 14, 30, HAHEN_OBJECT_DEFAULT, 10, NULL);
    SageSparks(play, &mid, 80.0f, 24, 1.2f); // the Sages' power spilling out
}

// ---- Scenes ----

static void OnSceneInit(int16_t sceneNum) {
    s16 from = sLastScene;

    sLastScene = sceneNum;
    sSpawnPending = false;
    sArrival = 0;
    if ((gPlayState == NULL) || !SacredAvailable()) {
        return;
    }
    RegisterActors();
    if ((sceneNum == SCENE_TEMPLE_OF_TIME) && LINK_IS_ADULT) {
        sSpawnPending = true;
        sArrival = (from == MIRROR_ROOM_SCENE) ? 1 : 0;
    } else if (sceneNum == MIRROR_ROOM_SCENE) {
        sSpawnPending = true;
        sArrival = (from == FIRST_HALL_SCENE) ? 2 : 0;
    } else if (sceneNum == FIRST_HALL_SCENE) {
        sSpawnPending = true;
        sArrival = (from == HUB_SCENE) ? 3 : 0;
    } else if (sceneNum == HUB_SCENE) {
        sSpawnPending = true;
    }
}

// The rooms' sunbeams, as big as their windows
static void ScaleBeams(PlayState* play) {
    f32 scale = BeamScale(play->sceneNum);

    for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != NULL; a = a->next) {
            if ((a->id == ACTOR_DEMO_KANKYO) && (a->params == 0x0E)) {
                Actor_SetScale(a, scale);
            }
        }
    }
}

static void PlaceAt(Player* player, f32 x, f32 y, f32 z, s16 yaw) {
    player->actor.world.pos = { x, y, z };
    player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;
    Player* player;

    if ((play == NULL) || !SacredAvailable()) {
        return;
    }
    player = GET_PLAYER(play);
    if (sSpawnPending) {
        sSpawnPending = false;
        if (play->sceneNum == SCENE_TEMPLE_OF_TIME) {
            Actor* door =
                Actor_Spawn(&play->actorCtx, play, sDoorActorId, sDoorPos.x, sDoorPos.y, sDoorPos.z, 0, 0x4000, 0, 0);
            // Belonging to no room: spawned while he stands in the Master Sword's chamber (as he does after the
            // Sages' cutscene that makes him an adult) it would otherwise vanish as soon as he walks into the hall
            if (door != NULL) {
                door->room = -1;
            }
            if (sArrival == 1) {
                PlaceAt(player, sDoorPos.x + 70.0f, sDoorPos.y, sDoorPos.z, 0x4000); // out of the doorway
            }
        } else if (play->sceneNum == MIRROR_ROOM_SCENE) {
            Actor_Spawn(&play->actorCtx, play, sMirrorActorId, sRoomMirrorPos.x, sRoomMirrorPos.y, sRoomMirrorPos.z,
                        0, 0, 0, 0);
            if (sArrival == 2) {
                PlaceAt(player, 0.0f, 24.0f, -230.0f, 0); // out of the mirror, facing the hall
            }
        } else if (play->sceneNum == FIRST_HALL_SCENE) {
            // (the Temple of Light's Entrance Hall: belonging to no room, so it is there again when he comes back
            // to the hall from the temple's other rooms)
            Actor* m = Actor_Spawn(&play->actorCtx, play, sMirrorActorId, sHallMirrorPos.x, sHallMirrorPos.y,
                                   sHallMirrorPos.z, 0, -0x8000, 0, 0);
            if (m != NULL) {
                m->room = -1;
            }
        }
        if ((play->sceneNum == MIRROR_ROOM_SCENE) || (play->sceneNum == HUB_SCENE)) {
            ScaleBeams(play);
        }
        return;
    }
    // Back out of the Mirror Room: walking up to the broken door at the passage's end
    if ((play->sceneNum == MIRROR_ROOM_SCENE) && CanLeave(play) && (player->actor.world.pos.z > 595.0f)) {
        Warp(play, ENTR_TEMPLE_OF_TIME_ENTRANCE);
    }
}

static void RegisterGanondorfSacred() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfSacred, {});
