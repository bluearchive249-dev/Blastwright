// SoH: "Play as Ganondorf", the way into the Sacred Realm Temple (stage 1).
//
// In the Temple of Time (adult), a sealed door stands in the west wall of the main hall: no keyhole, so no key fits,
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

static const char sDoorFrameDL[] = "__OTR__objects/gdf_sacred/gGdfSealedDoorFrameDL";
static const char sDoorSlabDL[] = "__OTR__objects/gdf_sacred/gGdfSealedDoorSlabDL";
static const char sDoorwayDL[] = "__OTR__objects/gdf_sacred/gGdfSealedDoorwayDL";
static const char sMirrorFrameDL[] = "__OTR__objects/gdf_sacred/gGdfMirrorFrameDL";
static const char sMirrorGlassDL[] = "__OTR__objects/gdf_sacred/gGdfMirrorGlassDL";

// The door: in the main hall's west wall (x -520), between the two west pillars, facing into the hall
static const Vec3f sDoorPos = { -520.0f, -40.0f, 2075.0f };
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
    if (!DoorIsOpen(play)) {
        // The glow beyond the bars: sparks of the Sages' colours in the window now and then
        if ((self->timer % 6) == 0) {
            Vec3f w = { sDoorPos.x - 14.0f, sDoorPos.y + 165.0f, sDoorPos.z };

            SageSparks(play, &w, 40.0f, 1, 0.4f);
        }
        return;
    }
    // Broken in: walking into the doorway takes him through
    if (CanLeave(play) && (p.x < sDoorPos.x + 30.0f) && (fabsf(p.z - sDoorPos.z) < 70.0f) &&
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

static void RegisterActors() {
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

extern "C" s32 GdfSacred_IsSealedDoor(Actor* a) {
    return (sDoorActorId >= 0) && (a->id == sDoorActorId);
}

extern "C" s32 GdfSacred_IsShut(Actor* a) {
    return GdfSacred_IsSealedDoor(a) && (gPlayState != NULL) && !DoorIsOpen(gPlayState);
}

extern "C" void GdfSacred_KickOpen(Actor* a, PlayState* play) {
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
            Actor_Spawn(&play->actorCtx, play, sDoorActorId, sDoorPos.x, sDoorPos.y, sDoorPos.z, 0, 0x4000, 0, 0);
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
            Actor_Spawn(&play->actorCtx, play, sMirrorActorId, sHallMirrorPos.x, sHallMirrorPos.y, sHallMirrorPos.z,
                        0, -0x8000, 0, 0);
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
