// Ganondorf's Cape for Link
//
// Spawns Ganondorf's cloth-physics cape (EnGanonMant, the one he wears in the Ganon's Tower fight)
// on Link. The cape's graphics live in gameplay_keep, so it can be spawned in any scene.
//
// How it works:
//  - Each frame Link is drawn, LinkGanonCape_RecordLimb stores his shoulder positions.
//  - The cape is spawned in ACTORCAT_MISC, which draws after the player, so those positions are
//    from the same frame when LinkGanonCape_PrepareDraw turns them into the cape's attach points.
//  - Ganondorf's own cape values are scaled to Link's size by (jointLength / 9.5).
//  - The cape uses static textures and vertex buffers, so only one can exist. Link's cape is
//    removed while Ganondorf (ACTOR_BOSS_GANON) is loaded.

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "LinkGanonCape.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"
extern PlayState* gPlayState;
}

#define CVAR_CAPE(var) CVAR_ENHANCEMENT("GanonCape." var)
#define CVAR_CAPE_ENABLED CVAR_CAPE("Enabled")

// Child Link is about 0.64x adult Link's size (see Player_OverrideLimbDrawGameplayCommon)
#define CHILD_SCALE 0.64f
// Ganondorf's strand segment length, all his cape values are tuned for this
#define GANONDORF_JOINT_LENGTH 9.5f

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sLeftShoulder;
static Vec3f sRightShoulder;
static u8 sRecordedLimbs = 0; // bit 0 = left shoulder, bit 1 = right shoulder

static bool ShouldWearCape() {
    if (!CVarGetInteger(CVAR_CAPE_ENABLED, 0)) {
        return false;
    }
    if (!LINK_IS_ADULT && !CVarGetInteger(CVAR_CAPE("Child"), 1)) {
        return false;
    }
    return true;
}

extern "C" void LinkGanonCape_RecordLimb(PlayState* play, Player* player, int limbIndex) {
    // Dark Link also draws with Player_PostLimbDrawGameplay
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }

    if (limbIndex == PLAYER_LIMB_L_SHOULDER) {
        Matrix_MultVec3f(&sZeroVec, &sLeftShoulder);
        sRecordedLimbs |= 1;
    } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
        Matrix_MultVec3f(&sZeroVec, &sRightShoulder);
        sRecordedLimbs |= 2;
    }
}

extern "C" int LinkGanonCape_PrepareDraw(EnGanonMant* cape, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 recorded = sRecordedLimbs;

    // Consume this frame's shoulder positions
    sRecordedLimbs = 0;

    if (!ShouldWearCape()) {
        Actor_Kill(&cape->actor);
        return false;
    }

    // Hide when Link wasn't drawn this frame, or the camera is inside his head (first person)
    if (recorded != 3 || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) || player->unk_6AD == 1 ||
        player->unk_6AD == 2) {
        cape->needsReset = true;
        return false;
    }

    f32 sizeScale = LINK_IS_ADULT ? 1.0f : CHILD_SCALE;
    f32 jointLength = CVarGetFloat(CVAR_CAPE("Length"), 3.5f) * sizeScale;
    f32 width = CVarGetFloat(CVAR_CAPE("Width"), 1.15f);
    f32 backOffset = CVarGetFloat(CVAR_CAPE("BackOffset"), 1.5f) * sizeScale;
    f32 s = jointLength / GANONDORF_JOINT_LENGTH;

    Vec3f right = sRightShoulder;
    Vec3f left = sLeftShoulder;

    // The cape's "forward" is perpendicular to the right->left shoulder line. Make sure that matches
    // the way Link is facing, in case a custom model mirrors the shoulder limbs.
    f32 dx = left.x - right.x;
    f32 dz = left.z - right.z;
    f32 faceX = Math_SinS(player->actor.shape.rot.y);
    f32 faceZ = Math_CosS(player->actor.shape.rot.y);
    if ((-dz * faceX) + (dx * faceZ) < 0.0f) {
        Vec3f tmp = right;
        right = left;
        left = tmp;
        dx = -dx;
        dz = -dz;
    }

    Vec3f mid;
    mid.x = (right.x + left.x) * 0.5f;
    mid.y = (right.y + left.y) * 0.5f;
    mid.z = (right.z + left.z) * 0.5f;

    // Widen the attach points so the cape drapes over the shoulders instead of cutting into them
    right.x = mid.x + (right.x - mid.x) * width;
    right.y = mid.y + (right.y - mid.y) * width;
    right.z = mid.z + (right.z - mid.z) * width;
    left.x = mid.x + (left.x - mid.x) * width;
    left.y = mid.y + (left.y - mid.y) * width;
    left.z = mid.z + (left.z - mid.z) * width;

    // Push them behind Link's back so the cape doesn't start inside his torso
    f32 len = sqrtf(SQ(dx) + SQ(dz));
    if (len > 0.001f) {
        f32 backX = dz / len;
        f32 backZ = -dx / len;
        right.x += backX * backOffset;
        right.z += backZ * backOffset;
        left.x += backX * backOffset;
        left.z += backZ * backOffset;
    }

    // Snap the cape into place if Link teleported (warps, voiding out, respawning, ...)
    if (!cape->needsReset) {
        f32 jumpX = mid.x - cape->anchor.x;
        f32 jumpY = mid.y - cape->anchor.y;
        f32 jumpZ = mid.z - cape->anchor.z;
        if (SQ(jumpX) + SQ(jumpY) + SQ(jumpZ) > SQ(150.0f)) {
            cape->needsReset = true;
        }
    }

    cape->rightShoulderPos = right;
    cape->leftShoulderPos = left;

    // Ganondorf's values while floating/walking around in his fight, scaled down to Link
    cape->jointLength = jointLength;
    cape->gravity = -3.0f * s;
    cape->backPush = -3.0f * s;
    cape->backSwayMagnitude = 0.25f * s;
    cape->sideSwayMagnitude = -3.0f * s;
    cape->minDist = 20.0f * s;
    cape->velocityClamp = 5.0f * s;

    // Lie on the floor instead of sinking through it
    if (player->actor.floorHeight > BGCHECK_Y_MIN + 1.0f) {
        cape->minY = player->actor.floorHeight + 0.5f;
    } else {
        cape->minY = BGCHECK_Y_MIN;
    }

    // minDist pushes the cape away from this point
    cape->actor.world.pos = player->actor.world.pos;

    return true;
}

static EnGanonMant* FindLinkCape(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_EN_GANON_MANT && actor->params == LINK_GANON_CAPE_PARAMS && actor->update != NULL) {
            return (EnGanonMant*)actor;
        }
    }
    return NULL;
}

static bool IsGanondorfLoaded(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_GANON || actor->id == ACTOR_EN_GANON_MANT) {
            return true;
        }
    }
    return false;
}

static void LinkGanonCape_OnPlayerUpdate() {
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }

    Player* player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    EnGanonMant* cape = FindLinkCape(play);

    if (!ShouldWearCape() || IsGanondorfLoaded(play)) {
        if (cape != NULL) {
            Actor_Kill(&cape->actor);
        }
        return;
    }

    if (cape == NULL) {
        // Not spawned as Link's child: Player treats actor.child as the actor he's holding
        cape = (EnGanonMant*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_GANON_MANT, player->actor.world.pos.x,
                                         player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0,
                                         LINK_GANON_CAPE_PARAMS);
        if (cape == NULL) {
            return;
        }
        cape->actor.parent = &player->actor;
        // Keep it through room changes, and draw it after Link (ACTORCAT_MISC comes after ACTORCAT_PLAYER)
        cape->actor.room = -1;
        Actor_ChangeCategory(play, &play->actorCtx, &cape->actor, ACTORCAT_MISC);
    }

    cape->actor.world.pos = player->actor.world.pos;
}

static void RegisterLinkGanonCape() {
    // Always registered so turning the setting off also removes the cape
    COND_HOOK(OnPlayerUpdate, true, LinkGanonCape_OnPlayerUpdate);

    // Ganondorf brings his own cape, which shares the same buffers. Remove Link's right away.
    COND_ID_HOOK(OnActorInit, ACTOR_BOSS_GANON, true, [](void* actorPtr) {
        if (gPlayState == NULL) {
            return;
        }
        EnGanonMant* cape = FindLinkCape(gPlayState);
        if (cape != NULL) {
            Actor_Kill(&cape->actor);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterLinkGanonCape, {});
