// Play as Ganondorf
//
// Replaces Link with Ganondorf from the Ganon's Tower fight, using his boss moveset instead of Link's.
//
// Link is still there underneath. He stays invisible and keeps handling walking, collision, doors,
// ladders, loading zones and the camera, while Ganondorf (ACTOR_BOSS_GANON with GDF_PARAMS_PLAYER)
// follows him and does the attacking. See the "Play as Ganondorf" section in z_boss_ganon.c.
//
// This file:
//  - spawns Ganondorf next to Link (again after every scene change) and removes him when the option is
//    turned off,
//  - filters Link's input so he can't use his sword, shield, rolls, Z-targeting or (optionally) C-button items,
//  - hides Link and his shadow while Ganondorf is shown,
//  - levitation: L rises, Z sinks, letting go of both hovers in place, and the stick moves him around in
//    the air like running on the ground,
//  - while the cape is up, it works as a shield that every shield check accepts (Deku Scrub nuts, Octorok
//    rocks, Twinrova's magic, mirror light puzzles, ...),
//  - in water Ganondorf always walks the bottom like with Iron Boots (and doesn't need air); levitation
//    brings him back up,
//  - gives Link's voice clips Ganondorf's voice,
//  - tells the camera how tall Ganondorf is,
//  - optional title screen in his chamber, with him playing the organ (see BossGanon_TitleScreen).

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

// Ganondorf's head is about this far above his root, which floats HoverHeight above Link's feet
#define GDF_HEAD_ABOVE_ROOT 80.0f
// ...and his eyes a bit below the top of his head
#define GDF_EYES_ABOVE_ROOT 70.0f
// Levitation climb/sink speed, world units per frame
#define GDF_LEVITATE_VERTICAL_SPEED 6.0f

static bool sLinkShadowHidden = false;
static bool sCapeShielding = false; // the cape is up and works as Link's shield
static bool sForcedIronBoots = false; // Link wears Iron Boots because Ganondorf is in water

static bool sLevitating = false;
static bool sSoftLanding = false; // let go of the levitation: no fall damage until Link lands
static f32 sLevitateY = 0.0f;
static f32 sLevitateSpeed = 0.0f;

static bool IsEnabled() {
    return CVarGetInteger(CVAR_PLAY_AS_GDF_ENABLED, 0) != 0;
}

extern "C" s32 PlayAsGanondorf_UseTitleScreen(void) {
    return CVarGetInteger(CVAR_PLAY_AS_GDF("TitleScreen"), 0) != 0;
}

extern "C" s32 PlayAsGanondorf_IsTitleScreen(PlayState* play) {
    return (play != NULL) && (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) &&
           (play->sceneNum == SCENE_GANON_BOSS) && PlayAsGanondorf_UseTitleScreen();
}

static BossGanon* FindPlayerGanondorf(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if ((actor->id == ACTOR_BOSS_GANON) && (actor->params == GDF_PARAMS_PLAYER) && (actor->update != NULL)) {
            return (BossGanon*)actor;
        }
    }
    return NULL;
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

// First-person look (C-Up): the view comes from Ganondorf's eyes, and neither he nor Link is drawn.
// Aiming an item in first person (with Keep C-Button Items) still shows Link holding it.
static bool IsLookingFirstPerson(PlayState* play, Player* player) {
    if (!IsEnabled() || (play == NULL) || (player == NULL) || (player != GET_PLAYER(play)) || (player->unk_6AD != 1)) {
        return false;
    }
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("ShowLinkInCutscenes"), 1) && (play->csCtx.state != CS_STATE_IDLE)) {
        return false;
    }
    return FindPlayerGanondorf(play) != NULL;
}

static bool ShouldHideLinkModel(PlayState* play, Player* player) {
    return ShouldHideLink(play, player) || IsLookingFirstPerson(play, player);
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

// Runs right after Link's update: his update resets the shield collider and decides whether he's shielding
static void UpdateCapeShield(PlayState* play, Player* player, BossGanon* dorf) {
    if ((dorf != NULL) && BossGanon_Player_IsCapeUp(&dorf->actor)) {
        player->stateFlags1 |= PLAYER_STATE1_SHIELDING;
        BossGanon_Player_UpdateCapeShield(&dorf->actor, play);
        sCapeShielding = true;
    } else if (sCapeShielding) {
        player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
        sCapeShielding = false;
    }
}

// Ganondorf sinks in water and walks the bottom like with Iron Boots, using his whole moveset down there.
// Link's own Iron Boots handling does the work; his equipped boots come back once he's out of the water.
static void UpdateWaterBoots(PlayState* play, Player* player, bool ganondorfShown) {
    bool inWater = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;

    if (ganondorfShown && inWater && CVarGetInteger(CVAR_PLAY_AS_GDF("IronBootsInWater"), 1)) {
        if (player->currentBoots != PLAYER_BOOTS_IRON) {
            player->currentBoots = PLAYER_BOOTS_IRON;
            Player_SetBootData(play, player);
        }
        sForcedIronBoots = true;
    } else if (sForcedIronBoots) {
        player->currentBoots = BOOTS_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS));
        Player_SetBootData(play, player);
        sForcedIronBoots = false;
    }
}

static void StopLevitating() {
    if (sLevitating) {
        sLevitating = false;
        sSoftLanding = true;
    }
}

// L rises, Z sinks, neither hovers in place. The stick moves Ganondorf like running on the ground.
// Runs after Link's own update, so it overrides his falling every frame.
static void UpdateLevitation(PlayState* play, Player* player, BossGanon* dorf) {
    Input* input = &play->state.input[0];
    bool up = CHECK_BTN_ALL(input->cur.button, BTN_L);
    bool down = CHECK_BTN_ALL(input->cur.button, BTN_Z);
    bool notAllowed =
        (player->stateFlags1 &
         (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
          PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING | PLAYER_STATE1_ON_HORSE |
          PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_CARRYING_ACTOR)) ||
        (player->stateFlags2 & PLAYER_STATE2_CRAWLING) || BossGanon_Player_IsHidden(&dorf->actor);

    if (!sLevitating) {
        if (up && !notAllowed && BossGanon_Player_CanAct(&dorf->actor)) {
            sLevitating = true;
            sSoftLanding = false;
            sLevitateY = player->actor.world.pos.y;
            sLevitateSpeed = player->linearVelocity;
        } else {
            // Ganondorf floats back down after letting go, no fall damage
            if (sSoftLanding) {
                if (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
                    sSoftLanding = false;
                } else {
                    player->fallStartHeight = (s16)player->actor.world.pos.y;
                    player->fallDistance = 0;
                }
            }
            return;
        }
    }

    if (notAllowed) {
        StopLevitating();
        return;
    }

    // Up and down
    if (up && !down) {
        sLevitateY += GDF_LEVITATE_VERTICAL_SPEED;
    } else if (down && !up) {
        sLevitateY -= GDF_LEVITATE_VERTICAL_SPEED;

        // Landed: Link takes over on the ground again
        if ((player->actor.floorHeight > BGCHECK_Y_MIN + 1.0f) && (sLevitateY <= player->actor.floorHeight)) {
            player->actor.world.pos.y = player->actor.floorHeight;
            player->actor.velocity.y = 0.0f;
            StopLevitating();
            return;
        }
    }

    // Around, like running: same speed limit and camera-relative stick as Link on the ground
    f32 stickMagnitude;
    s16 stickAngle;
    func_80077D10(&stickMagnitude, &stickAngle, input);

    if (BossGanon_Player_IsBusy(&dorf->actor) || (stickMagnitude < 8.0f)) {
        stickMagnitude = 0.0f;
    }

    f32 targetSpeed = (stickMagnitude / 60.0f) * ((f32)R_RUN_SPEED_LIMIT / 100.0f);
    Math_StepToF(&sLevitateSpeed, targetSpeed, (targetSpeed > sLevitateSpeed) ? 1.5f : 2.0f);

    if (stickMagnitude > 0.0f) {
        s16 worldYaw = (s16)(Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + stickAngle);
        Math_ScaledStepToS(&player->actor.shape.rot.y, worldYaw, 0x1800);
    }
    player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y;

    player->actor.world.pos.x += Math_SinS(player->actor.shape.rot.y) * sLevitateSpeed;
    player->actor.world.pos.z += Math_CosS(player->actor.shape.rot.y) * sLevitateSpeed;
    player->actor.world.pos.y = sLevitateY;

    // Walls and ceilings still stop him
    Actor_UpdateBgCheckInfo(play, &player->actor, 26.0f, 18.0f, Player_GetHeight(player), 1 | 2);
    sLevitateY = player->actor.world.pos.y;

    // Link's own falling does nothing while levitating
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
    player->fallStartHeight = (s16)player->actor.world.pos.y;
    player->fallDistance = 0;
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

    // Title screen: Link stays hidden and frozen, and no second Ganondorf
    if (PlayAsGanondorf_IsTitleScreen(play)) {
        if (dorf != NULL) {
            KillPlayerGanondorf(play, dorf);
        }
        player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
        SetLinkShadowHidden(player, true);
        return;
    }

    if (!IsEnabled()) {
        if (dorf != NULL) {
            KillPlayerGanondorf(play, dorf);
        }
        SetLinkShadowHidden(player, false);
        UpdateCapeShield(play, player, NULL);
        UpdateWaterBoots(play, player, false);
        sLevitating = sSoftLanding = false;
        return;
    }

    if (dorf == NULL) {
        // New scene: start on the ground
        sLevitating = sSoftLanding = false;
        Actor_Spawn(&play->actorCtx, play, ACTOR_BOSS_GANON, player->actor.world.pos.x, player->actor.world.pos.y,
                    player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, GDF_PARAMS_PLAYER);
        dorf = FindPlayerGanondorf(play);
    }

    if (dorf != NULL) {
        UpdateLevitation(play, player, dorf);
    }
    UpdateCapeShield(play, player, dorf);
    UpdateWaterBoots(play, player, ShouldHideLink(play, player));

    SetLinkShadowHidden(player, ShouldHideLinkModel(play, player));
}

extern "C" void PlayAsGanondorf_FilterInput(PlayState* play, Player* player, Input* input) {
    if (!ShouldHideLink(play, player)) {
        return;
    }

    BossGanon* dorf = FindPlayerGanondorf(play);
    if (dorf == NULL) {
        return;
    }
    bool busy = BossGanon_Player_IsBusy(&dorf->actor);

    // B = sword, R = shield, Z = targeting, L = levitate. D-pad Up/Down are the laugh and the organ.
    u16 blocked = BTN_B | BTN_R | BTN_Z | BTN_L | BTN_DUP | BTN_DDOWN;

    if (!CVarGetInteger(CVAR_PLAY_AS_GDF("KeepItems"), 0)) {
        blocked |= BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
    }

    // A still opens doors, talks, climbs, grabs and so on, but no rolls, jump attacks or side hops
    u16 doAction = play->interfaceCtx.unk_1F0;
    if (busy || sLevitating || (doAction == DO_ACTION_NONE) || (doAction == DO_ACTION_ATTACK) ||
        (doAction == DO_ACTION_JUMP) || (doAction == DO_ACTION_PUTAWAY)) {
        blocked |= BTN_A;
    }

    input->cur.button &= ~blocked;
    input->press.button &= ~blocked;
    input->rel.button &= ~blocked;

    // Ganondorf stays put while he attacks, and levitation does its own movement
    if (busy || sLevitating) {
        input->cur.stick_x = input->cur.stick_y = 0;
        input->rel.stick_x = input->rel.stick_y = 0;
        input->press.stick_x = input->press.stick_y = 0;
    }
}

extern "C" s32 PlayAsGanondorf_BreathesUnderwater(PlayState* play, Player* player) {
    return ShouldHideLink(play, player) && CVarGetInteger(CVAR_PLAY_AS_GDF("IronBootsInWater"), 1);
}

extern "C" s32 PlayAsGanondorf_IsCapeShielding(void) {
    return sCapeShielding && IsEnabled();
}

extern "C" s32 PlayAsGanondorf_ShouldHideLink(PlayState* play, Player* player) {
    return ShouldHideLinkModel(play, player) || PlayAsGanondorf_IsTitleScreen(play);
}

extern "C" s32 PlayAsGanondorf_GetFirstPersonEye(Player* player, Vec3f* eye) {
    if (!IsLookingFirstPerson(gPlayState, player)) {
        return false;
    }
    // Link's feet + Ganondorf's hover height + his eyes above his root. Levitating lifts Link too.
    eye->x = player->actor.world.pos.x;
    eye->y = player->actor.world.pos.y + CVarGetFloat(CVAR_PLAY_AS_GDF("HoverHeight"), 70.0f) + GDF_EYES_ABOVE_ROOT;
    eye->z = player->actor.world.pos.z;
    return true;
}

// Every camera mode scales its distance and height from the player's height, so report Ganondorf's
extern "C" f32 PlayAsGanondorf_GetCameraHeight(Player* player) {
    f32 height = Player_GetHeight(player);

    if (ShouldHideLink(gPlayState, player)) {
        // Open areas (anywhere with the open sky: Hyrule Field, Lake Hylia, ...) use wider camera settings, so they
        // have their own distance
        bool outdoors = (gPlayState->skyboxId == SKYBOX_NORMAL_SKY) && !gPlayState->envCtx.skyboxDisabled;
        f32 distance = outdoors ? CVarGetFloat(CVAR_PLAY_AS_GDF("CameraDistanceOutdoors"), 0.75f)
                                : CVarGetFloat(CVAR_PLAY_AS_GDF("CameraDistance"), 1.0f);
        f32 ganondorfHeight = (CVarGetFloat(CVAR_PLAY_AS_GDF("HoverHeight"), 70.0f) + GDF_HEAD_ABOVE_ROOT) * distance;

        if (ganondorfHeight > height) {
            height = ganondorfHeight;
        }
    }

    return height;
}

// Link's voice clips, child ones included, are swapped for Ganondorf's. Clips he has no equivalent for are muted.
extern "C" u16 PlayAsGanondorf_GetVoiceSfx(Player* player, u16 sfxId) {
    if (!ShouldHideLink(gPlayState, player) || !CVarGetInteger(CVAR_PLAY_AS_GDF("Voice"), 1)) {
        return sfxId;
    }

    // Child clips are the adult ones + 0x20
    switch (NA_SE_VO_LI_SWORD_N + ((sfxId - NA_SE_VO_LI_SWORD_N) & 0x1F)) {
        // Idling
        case NA_SE_VO_LI_RELAX:
            return NA_SE_EN_GANON_LAUGH;
        case NA_SE_VO_LI_SWEAT:
        case NA_SE_VO_LI_BREATH_REST:
            return NA_SE_EN_GANON_BREATH;
        case NA_SE_VO_LI_SNEEZE:
            return NA_SE_EN_GANON_VOICE_DEMO;
        // Getting hurt (the hit itself already plays his damage voice)
        case NA_SE_VO_LI_FALL_L:
        case NA_SE_VO_LI_TAKEN_AWAY:
            return NA_SE_EN_GANON_DAMAGE2;
        case NA_SE_VO_LI_DOWN:
            return NA_SE_EN_GANON_DOWN;
        case NA_SE_VO_LI_GROAN:
            return NA_SE_EN_GANON_DAMAGE1;
        // Effort
        case NA_SE_VO_LI_SURPRISE:
        case NA_SE_VO_LI_MAGIC_NALE:
        case NA_SE_VO_LI_MAGIC_FROL:
        case NA_SE_VO_LI_MAGIC_ATTACK:
            return NA_SE_EN_GANON_AT_RETURN;
        case NA_SE_VO_LI_BREATH_DRINK:
            return NA_SE_EN_GANON_RESTORE;
        default:
            return PLAY_AS_GANONDORF_MUTE;
    }
}

static void RegisterPlayAsGanondorf() {
    // Always registered, so turning the option off also removes Ganondorf
    COND_HOOK(OnPlayerUpdate, true, PlayAsGanondorf_OnPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterPlayAsGanondorf, {});
