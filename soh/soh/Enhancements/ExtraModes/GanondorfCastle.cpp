// SoH: "Play as Ganondorf", inside Hyrule Castle.
//
// The castle wing (throne room with its window onto Zelda's courtyard, grand hall, library, barracks, entrance hall,
// gallery and royal rooms) is a generated scene in the unused SCENE_TESTROOM slot, shipped as gdf_castle.o2r in the
// mods folder (made by the castle generator; it holds our own geometry and refers to the game's textures by path).
//
// On the castle grounds, the castle's raised drawbridge (part of the scene's mesh) is hidden and replaced by a working
// one (an actor of our own, with collision): it lowers when he walks up to the gate, the guards not daring to refuse,
// and the castle door beyond it leads inside. The interior's front doors lead back out onto the bridge.

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/frame_interpolation.h" // gives the OPEN_DISPS macros C linkage for FrameInterpolation_*
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "PlayAsGanondorf.h"
extern PlayState* gPlayState;
}

#define CASTLE_SCENE SCENE_TESTROOM
#define CASTLE_ENTRANCE ENTR_TESTROOM_0

static const char sBridgeDL[] = "__OTR__objects/gdf_castle/gGdfDrawbridgeDL";
static const char sDoorwayDL[] = "__OTR__objects/gdf_castle/gGdfDoorwayDL";
static const char sBridgeCol[] = "__OTR__objects/gdf_castle/gGdfDrawbridgeCol";
static const char sVanillaBridgeDL[] = "__OTR__scenes/shared/spot15_scene/spot15_room_0DL_004588";

// The drawbridge's hinge: the foot of the castle gate, above the moat
static const Vec3f sHinge = { -286.0f, 1617.0f, 420.0f };
static const Vec3f sGatePosts = { -286.0f, 1571.0f, 860.0f };
#define BRIDGE_RAISED (-0x4000)
#define BRIDGE_LOWERED 0x05B0 // its far end rests on the path by the gate posts

typedef struct {
    DynaPolyActor dyna;
    s16 angle;
    u8 moving;
} GdfDrawbridge;

static s32 sBridgeActorId = -1;
static bool sPatched = false;
static bool sReturning = false;
static bool sTeleportPending = false;
static bool sSpawnPending = false;
static s16 sLastScene = -1;
static GdfDrawbridge* sBridge = NULL;

static bool CastleAvailable() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.CastleInterior"), 1) &&
           ResourceMgr_FileExists("objects/gdf_castle/gGdfDrawbridgeDL");
}

// ---- The drawbridge actor ----

static void Bridge_Init(Actor* thisx, PlayState* play) {
    GdfDrawbridge* self = (GdfDrawbridge*)thisx;
    CollisionHeader* col = NULL;

    Actor_SetScale(thisx, 1.0f);
    DynaPolyActor_Init(&self->dyna, 1);
    CollisionHeader_GetVirtual((void*)sBridgeCol, &col);
    self->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, col);
    self->angle = (thisx->params == 1) ? BRIDGE_LOWERED : BRIDGE_RAISED;
    thisx->shape.rot.x = thisx->world.rot.x = self->angle;
    self->moving = false;
    sBridge = self;
}

static void Bridge_Destroy(Actor* thisx, PlayState* play) {
    GdfDrawbridge* self = (GdfDrawbridge*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, self->dyna.bgId);
    if (sBridge == self) {
        sBridge = NULL;
    }
}

static void Bridge_Update(Actor* thisx, PlayState* play) {
    GdfDrawbridge* self = (GdfDrawbridge*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f p = player->actor.world.pos;
    f32 dGate = sqrtf(SQ(p.x - sGatePosts.x) + SQ(p.z - sGatePosts.z));
    bool onBridge = (p.x > sHinge.x - 130.0f) && (p.x < sHinge.x + 130.0f) && (p.z > sHinge.z - 40.0f) &&
                    (p.z < sHinge.z + 340.0f) && (p.y > sHinge.y - 120.0f);
    // He walks up to the gate: down it comes. It stays down while he's on it or near it.
    bool lower = ((dGate < 330.0f) && (fabsf(p.y - sGatePosts.y) < 150.0f)) || onBridge;
    s16 target = lower ? BRIDGE_LOWERED : BRIDGE_RAISED;

    if (self->angle != target) {
        Math_ScaledStepToS(&self->angle, target, lower ? 0x110 : 0x0C0);
        Actor_PlaySfx_Flagged(thisx, lower ? NA_SE_EV_BRIDGE_OPEN - SFX_FLAG : NA_SE_EV_BRIDGE_CLOSE - SFX_FLAG);
        self->moving = true;
    } else if (self->moving) {
        self->moving = false;
        Audio_PlayActorSound2(thisx, lower ? NA_SE_EV_BRIDGE_OPEN_STOP : NA_SE_EV_BRIDGE_CLOSE_STOP);
        if (lower) {
            Actor_RequestQuakeAndRumble(thisx, play, 2, 8);
        }
    }
    thisx->shape.rot.x = thisx->world.rot.x = self->angle;
}

static void Bridge_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBridgeDL);
    // The dark gateway behind it doesn't turn with it
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDoorwayDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterBridgeActor() {
    if (sBridgeActorId >= 0) {
        return;
    }
    ActorDBInit init;
    init.name = "Gdf_Drawbridge";
    init.desc = "Hyrule Castle Drawbridge (Play as Ganondorf)";
    init.category = ACTORCAT_BG;
    init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    init.objectId = OBJECT_GAMEPLAY_KEEP;
    init.instanceSize = sizeof(GdfDrawbridge);
    init.init = Bridge_Init;
    init.destroy = Bridge_Destroy;
    init.update = Bridge_Update;
    init.draw = Bridge_Draw;
    sBridgeActorId = ActorDB::Instance->AddEntry(init).entry.id;
}

// ---- The castle ----

static void SetVanillaBridgeHidden(bool hidden) {
    if (hidden && !sPatched) {
        ResourceMgr_PatchGfxByName(sVanillaBridgeDL, "gdfCastleHideBridge", 0, gsSPEndDisplayList());
        sPatched = true;
    } else if (!hidden && sPatched) {
        ResourceMgr_UnpatchGfxByName(sVanillaBridgeDL, "gdfCastleHideBridge");
        sPatched = false;
    }
}

static void Warp(PlayState* play, s16 entrance) {
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.respawnFlag = 0;
    gSaveContext.cutsceneIndex = 0;
}

extern "C" void GanondorfCastle_VisitInterior(void) {
    if ((gPlayState != NULL) && CastleAvailable()) {
        Warp(gPlayState, CASTLE_ENTRANCE);
    }
}

extern "C" s32 GanondorfCastle_IsAvailable(void) {
    return CastleAvailable();
}

static void OnSceneInit(int16_t sceneNum) {
    PlayState* play = gPlayState;
    bool fromCastle = (sLastScene == CASTLE_SCENE);

    sLastScene = sceneNum;
    sBridge = NULL;
    if (play == NULL) {
        return;
    }
    if (!CastleAvailable()) {
        SetVanillaBridgeHidden(false);
        return;
    }
    RegisterBridgeActor();
    if ((sceneNum == SCENE_HYRULE_CASTLE) && LINK_IS_CHILD) {
        SetVanillaBridgeHidden(true);
        sReturning = fromCastle;
        sSpawnPending = true; // once the actors are set up (the first update)
        sTeleportPending = sReturning;
    }
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;
    Player* player;

    if ((play == NULL) || !CastleAvailable()) {
        return;
    }
    player = GET_PLAYER(play);
    // Out through the interior's front doors (the threshold's floor exit, made sure of here: walking into the shut
    // doors at the south end of the entrance hall), back onto the drawbridge
    if (play->sceneNum == CASTLE_SCENE) {
        if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
            (player->actor.world.pos.z > 2300.0f) && (fabsf(player->actor.world.pos.x + 960.0f) < 110.0f) &&
            (player->actor.world.pos.y < 120.0f)) {
            Warp(play, ENTR_CASTLE_GROUNDS_SOUTH_EXIT);
        }
        return;
    }
    if (play->sceneNum != SCENE_HYRULE_CASTLE) {
        return;
    }
    if (sSpawnPending) {
        sSpawnPending = false;
        Actor_Spawn(&play->actorCtx, play, sBridgeActorId, sHinge.x, sHinge.y, sHinge.z, BRIDGE_RAISED, 0, 0,
                    sReturning ? 1 : 0);
        return;
    }
    // Out through the castle's front doors: back on the drawbridge, facing out
    if (sTeleportPending) {
        sTeleportPending = false;
        player->actor.world.pos = { sHinge.x, sHinge.y + 30.0f, sHinge.z + 120.0f };
        player->actor.prevPos = player->actor.world.pos;
        player->actor.home.pos = player->actor.world.pos;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = 0;
        return;
    }
    // In through the castle gate, once the bridge is down. The scene keeps a wall where the raised bridge stood
    // (z 419), so he can't step into the gateway itself: walking into it at the foot of the bridge is enough.
    if ((sBridge != NULL) && (sBridge->angle == BRIDGE_LOWERED) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
        (play->msgCtx.msgMode == MSGMODE_NONE) && (player->actor.world.pos.z < sHinge.z + 70.0f) &&
        (player->actor.world.pos.z > sHinge.z - 80.0f) && (fabsf(player->actor.world.pos.x - sHinge.x) < 95.0f) &&
        (fabsf(player->actor.world.pos.y - sHinge.y) < 60.0f) &&
        (ABS((s16)(player->actor.shape.rot.y - 0x8000)) < 0x3000)) {
        Warp(play, CASTLE_ENTRANCE);
    }
}

static void RegisterGanondorfCastle() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfCastle, {});
