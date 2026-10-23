// Play as Ganondorf
//
// Replaces Link with Ganondorf from the Ganon's Tower fight, using his boss moveset instead of Link's.
//
// Link is still there underneath. He stays invisible and keeps handling walking, collision, doors,
// ladders, loading zones and the camera, while Ganondorf (ACTOR_BOSS_GANON with GDF_PARAMS_PLAYER)
// follows him and does the attacking. See the "Play as Ganondorf" section in z_boss_ganon.c.
//
// This file:
//  - spawns Ganondorf next to Link (again after every scene change) and removes him when the
//    option is turned off or the real Ganondorf / Ganon is in the scene,
//  - filters Link's input so he can't use his sword, shield, rolls or (optionally) C-button items,
//  - hides Link and his shadow while Ganondorf is shown.

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h"
extern PlayState* gPlayState;
}

#include "PlayAsGanondorf.h"

#define CVAR_PLAY_AS_GDF(var) CVAR_ENHANCEMENT("PlayAsGanondorf." var)
#define CVAR_PLAY_AS_GDF_ENABLED CVAR_PLAY_AS_GDF("Enabled")

static bool sLinkShadowHidden = false;

static bool IsEnabled() {
    return CVarGetInteger(CVAR_PLAY_AS_GDF_ENABLED, 0) != 0;
}

static BossGanon* FindPlayerGanondorf(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if ((actor->id == ACTOR_BOSS_GANON) && (actor->params == GDF_PARAMS_PLAYER) && (actor->update != NULL)) {
            return (BossGanon*)actor;
        }
    }
    return NULL;
}

// The real fight shares Ganondorf's static data (cape, effects), so he can't be in two places at once
static bool IsRealGanondorfPresent(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if ((actor->id == ACTOR_BOSS_GANON) && (actor->params < 0x64) && (actor->params != GDF_PARAMS_PLAYER)) {
            return true;
        }
        if (actor->id == ACTOR_BOSS_GANON2) {
            return true;
        }
    }
    return false;
}

static void KillPlayerGanondorf(PlayState* play, BossGanon* dorf) {
    // His cape and projectiles point back at him
    for (s32 i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = play->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            if ((actor->parent == &dorf->actor) &&
                ((actor->id == ACTOR_BOSS_GANON) || (actor->id == ACTOR_EN_GANON_MANT))) {
                Actor_Kill(actor);
            }
        }
    }
    Actor_Kill(&dorf->actor);
}

static bool ShouldHideLink(PlayState* play, Player* player) {
    if (!IsEnabled() || (play == NULL) || (player == NULL) || (player != GET_PLAYER(play))) {
        return false;
    }

    BossGanon* dorf = FindPlayerGanondorf(play);
    return (dorf != NULL) && !BossGanon_Player_IsHidden(&dorf->actor);
}

static void SetLinkShadowHidden(Player* player, bool hidden) {
    if (hidden) {
        player->actor.shape.shadowAlpha = 0;
        sLinkShadowHidden = true;
    } else if (sLinkShadowHidden) {
        player->actor.shape.shadowAlpha = 255;
        sLinkShadowHidden = false;
    }
}

static void PlayAsGanondorf_OnPlayerUpdate() {
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }

    Player* player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    BossGanon* dorf = FindPlayerGanondorf(play);

    if (!IsEnabled() || IsRealGanondorfPresent(play)) {
        if (dorf != NULL) {
            KillPlayerGanondorf(play, dorf);
        }
        SetLinkShadowHidden(player, false);
        return;
    }

    if (dorf == NULL) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_BOSS_GANON, player->actor.world.pos.x, player->actor.world.pos.y,
                    player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, GDF_PARAMS_PLAYER);
    }

    SetLinkShadowHidden(player, ShouldHideLink(play, player));
}

extern "C" void PlayAsGanondorf_FilterInput(PlayState* play, Player* player, Input* input) {
    if (!ShouldHideLink(play, player)) {
        return;
    }

    BossGanon* dorf = FindPlayerGanondorf(play);
    bool busy = BossGanon_Player_IsBusy(&dorf->actor);

    // B = sword, R = shield. They're Ganondorf's buttons now.
    u16 blocked = BTN_B | BTN_R;

    if (!CVarGetInteger(CVAR_PLAY_AS_GDF("KeepItems"), 0)) {
        blocked |= BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
    }

    // A still opens doors, talks, climbs, grabs and so on, but no rolls, jump attacks or side hops
    u16 doAction = play->interfaceCtx.unk_1F0;
    if (busy || (doAction == DO_ACTION_NONE) || (doAction == DO_ACTION_ATTACK) || (doAction == DO_ACTION_JUMP) ||
        (doAction == DO_ACTION_PUTAWAY)) {
        blocked |= BTN_A;
    }

    input->cur.button &= ~blocked;
    input->press.button &= ~blocked;
    input->rel.button &= ~blocked;

    // Ganondorf stays put while he attacks
    if (busy) {
        input->cur.stick_x = input->cur.stick_y = 0;
        input->rel.stick_x = input->rel.stick_y = 0;
        input->press.stick_x = input->press.stick_y = 0;
    }
}

extern "C" s32 PlayAsGanondorf_ShouldHideLink(PlayState* play, Player* player) {
    return ShouldHideLink(play, player);
}

static void RegisterPlayAsGanondorf() {
    // Always registered, so turning the option off also removes Ganondorf
    COND_HOOK(OnPlayerUpdate, true, PlayAsGanondorf_OnPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterPlayAsGanondorf, {});
