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
//  - optional title screen in his chamber, with him playing the organ (see BossGanon_TitleScreen),
//  - D-Pad Right: transforms into Ganon (ACTOR_BOSS_GANON2 with GANON2_PARAMS_PLAYER) and back. Ganon is only
//    allowed outdoors and in dungeons; anywhere else he turns back into Ganondorf.

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "GanondorfSongs.h"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h"
#include "overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
extern PlayState* gPlayState;
}

#include "PlayAsGanondorf.h"

#define CVAR_PLAY_AS_GDF(var) CVAR_ENHANCEMENT("PlayAsGanondorf." var)
#define CVAR_PLAY_AS_GDF_ENABLED CVAR_PLAY_AS_GDF("Enabled")

// Ganondorf's head is about this far above his root, which floats HoverHeight above Link's feet
#define GDF_HEAD_ABOVE_ROOT 80.0f
// ...and his eyes a bit below the top of his head
#define GDF_EYES_ABOVE_ROOT 70.0f
// Standing on the ground (not levitating): his head and eyes above his feet, and the height the camera frames
#define GDF_STANDING_EYE_HEIGHT 92.0f
#define GDF_STANDING_CAMERA_HEIGHT 120.0f

// Levitation climb/sink speed, world units per frame
#define GDF_LEVITATE_VERTICAL_SPEED 6.0f

static bool sLinkShadowHidden = false;
static bool sCapeShielding = false; // the cape is up and works as Link's shield
static bool sForcedIronBoots = false; // Link wears Iron Boots because Ganondorf is in water

static bool sWantGanon = false; // Ganon form, kept across areas where he's allowed
static bool sGanonMusic = false; // Ganon's battle theme is playing instead of the area music
static bool sGanonMusicNewArea = false;
static u16 sPrevBgm = 0;
static s16 sPrevBgmScene = -1;

static bool sLevitating = false;
static bool sSoftLanding = false; // let go of the levitation: no fall damage until Link lands
static f32 sLevitateY = 0.0f;
static f32 sLevitateSpeed = 0.0f;

static bool IsStandingOnGround() {
    return CVarGetInteger(CVAR_PLAY_AS_GDF("WalkOnGround"), 1) && !sLevitating;
}

static bool sSprinting = false; // holding A on the ground: Ganondorf sprints

static bool IsEnabled() {
    return CVarGetInteger(CVAR_PLAY_AS_GDF_ENABLED, 1) != 0;
}

extern "C" s32 PlayAsGanondorf_UseTitleScreen(void) {
    return CVarGetInteger(CVAR_PLAY_AS_GDF("TitleScreen"), 1) != 0;
}

extern "C" s32 PlayAsGanondorf_IsTitleScreen(PlayState* play) {
    return (play != NULL) && (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) &&
           (play->sceneNum == SCENE_GANONDORF_BOSS) && PlayAsGanondorf_UseTitleScreen();
}

static BossGanon* FindPlayerGanondorf(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if ((actor->id == ACTOR_BOSS_GANON) && (actor->params == GDF_PARAMS_PLAYER) && (actor->update != NULL)) {
            return (BossGanon*)actor;
        }
    }
    return NULL;
}

static BossGanon2* FindPlayerGanon(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if ((actor->id == ACTOR_BOSS_GANON2) && (actor->params == GANON2_PARAMS_PLAYER) && (actor->update != NULL)) {
            return (BossGanon2*)actor;
        }
    }
    return NULL;
}

static bool IsGanonFormEnabled() {
    return CVarGetInteger(CVAR_PLAY_AS_GDF("GanonForm"), 1) != 0;
}

// Ganon is too big for houses, shops, grottos and other interiors. Dungeons and temples (and their bosses) are fine,
// and so is anywhere outdoors. Not in the real Ganon fight, there can only be one Ganon.
static bool IsGanonAllowedHere(PlayState* play) {
    s16 scene = play->sceneNum;

    if (scene == SCENE_GANON_BOSS) {
        return false;
    }
    if (scene <= SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) {
        return scene != SCENE_TREASURE_BOX_SHOP;
    }
    if ((scene >= SCENE_MARKET_ENTRANCE_DAY) && (scene <= SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS)) {
        return true;
    }
    if ((scene == SCENE_CASTLE_COURTYARD_GUARDS_DAY) || (scene == SCENE_CASTLE_COURTYARD_GUARDS_NIGHT) ||
        (scene == SCENE_CASTLE_COURTYARD_ZELDA)) {
        return true;
    }
    return (scene >= SCENE_HYRULE_FIELD) && (scene <= SCENE_OUTSIDE_GANONS_CASTLE);
}

extern "C" s32 PlayAsGanondorf_UseOrganOcarina(void) {
    PlayState* play = gPlayState;

    return IsEnabled() && CVarGetInteger(CVAR_PLAY_AS_GDF("OrganOcarina"), 1) && (play != NULL) &&
           (gSaveContext.gameMode == GAMEMODE_NORMAL) && (FindPlayerGanondorf(play) != NULL) &&
           (FindPlayerGanon(play) == NULL);
}

extern "C" s32 PlayAsGanondorf_IsLevitating(void) {
    return sLevitating;
}

extern "C" s32 PlayAsGanondorf_IsGanonOut(PlayState* play) {
    return IsEnabled() && (play != NULL) && (FindPlayerGanon(play) != NULL);
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

    BossGanon2* ganon = FindPlayerGanon(play);
    if (ganon != NULL) {
        BossGanon2_Player_Remove(&ganon->actor, play);
    }
}

static bool ShouldHideLink(PlayState* play, Player* player) {
    if (!IsEnabled() || (play == NULL) || (player == NULL) || (player != GET_PLAYER(play))) {
        return false;
    }

    BossGanon* dorf = FindPlayerGanondorf(play);
    if (dorf == NULL) {
        return false;
    }

    BossGanon2* ganon = FindPlayerGanon(play);
    if (ganon != NULL) {
        return !BossGanon2_Player_IsHidden(&ganon->actor);
    }
    return !BossGanon_Player_IsHidden(&dorf->actor);
}

// First-person look (C-Up): the view comes from Ganondorf's eyes, and neither he nor Link is drawn.
// Aiming an item in first person (with Keep C-Button Items) still shows Link holding it.
static bool IsLookingFirstPerson(PlayState* play, Player* player) {
    if (!IsEnabled() || (play == NULL) || (player == NULL) || (player != GET_PLAYER(play)) || (player->unk_6AD != 1)) {
        return false;
    }
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("ShowLinkInCutscenes"), 0) && (play->csCtx.state != CS_STATE_IDLE)) {
        return false;
    }
    return FindPlayerGanondorf(play) != NULL;
}

// Crawlspaces are too small for him: instead of showing Link, he travels through as a purple flame
static bool IsCrawlFlame(PlayState* play, Player* player) {
    if (!IsEnabled() || (play == NULL) || (player == NULL) || (player != GET_PLAYER(play)) ||
        !(player->stateFlags2 & PLAYER_STATE2_CRAWLING) || !CVarGetInteger(CVAR_PLAY_AS_GDF("CrawlFlame"), 1)) {
        return false;
    }
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("ShowLinkInCutscenes"), 0) && (play->csCtx.state != CS_STATE_IDLE)) {
        return false;
    }
    return FindPlayerGanondorf(play) != NULL;
}

static bool ShouldHideLinkModel(PlayState* play, Player* player) {
    return ShouldHideLink(play, player) || IsLookingFirstPerson(play, player) || IsCrawlFlame(play, player);
}

// Epona is drawn as his black horse
extern "C" s32 PlayAsGanondorf_UseGanonHorse(void) {
    return IsEnabled() && CVarGetInteger(CVAR_PLAY_AS_GDF("GanondorfHorse"), 1);
}

extern "C" s32 PlayAsGanondorf_IsCrawlFlame(PlayState* play, Player* player) {
    return IsCrawlFlame(play, player);
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
    BossGanon2* ganon = (dorf != NULL) ? FindPlayerGanon(play) : NULL;

    if ((ganon != NULL) && BossGanon2_Player_IsGuarding(&ganon->actor)) {
        // Ganon's guard works the same way
        player->stateFlags1 |= PLAYER_STATE1_SHIELDING;
        BossGanon2_Player_UpdateGuardShield(&ganon->actor, play);
        sCapeShielding = true;
    } else if ((dorf != NULL) && (ganon == NULL) && BossGanon_Player_IsCapeUp(&dorf->actor)) {
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

    f32 targetSpeed = (stickMagnitude / 60.0f) * ((f32)R_RUN_SPEED_LIMIT / 100.0f) *
                      PlayAsGanondorf_GetRunSpeedFactor(play, player);
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

// Ganon's battle theme while he's out, then the area music again
static void PlayAreaMusic(PlayState* play) {
    if (play->sequenceCtx.seqId != NA_BGM_NO_MUSIC) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | play->sequenceCtx.seqId);
    } else if (play->sequenceCtx.natureAmbienceId != NATURE_ID_NONE) {
        Audio_PlayNatureAmbienceSequence(play->sequenceCtx.natureAmbienceId);
    } else {
        Audio_QueueSeqCmd(NA_BGM_STOP);
    }
}

static void UpdateGanonMusic(PlayState* play, bool on) {
    if (on && (!sGanonMusic || sGanonMusicNewArea)) {
        u16 current = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);

        // In a new area the old area's music (or Ganon's) may still be playing: bring back the new area's own later
        sPrevBgm = (sGanonMusic || ((current & 0xFF) == NA_BGM_GANON_BOSS)) ? (u16)NA_BGM_GANON_BOSS : current;
        sPrevBgmScene = play->sceneNum;
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_GANON_BOSS);
        sGanonMusic = true;
    } else if (!on && sGanonMusic) {
        sGanonMusic = false;

        if (((sPrevBgm & 0xFF) == NA_BGM_GANON_BOSS) || (sPrevBgmScene != play->sceneNum)) {
            PlayAreaMusic(play);
        } else if ((sPrevBgm & 0xFF) == NA_BGM_NATURE_AMBIENCE) {
            Audio_PlayNatureAmbienceSequence(play->sequenceCtx.natureAmbienceId);
        } else if ((sPrevBgm == NA_BGM_DISABLED) || ((sPrevBgm & 0xFF) == 0xFF)) {
            Audio_QueueSeqCmd(NA_BGM_STOP);
        } else {
            Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | sPrevBgm);
        }
    }
    sGanonMusicNewArea = false;
}

// Sword stance: the dash (stick + R) and the lunging thrust carry Link along
static void UpdateSwordDash(PlayState* play, Player* player, BossGanon* dorf) {
    s16 yaw;
    f32 speed;

    if (!BossGanon_Player_GetDash(&dorf->actor, &yaw, &speed) || (speed <= 0.0f)) {
        return;
    }
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
    player->actor.world.pos.x += Math_SinS(yaw) * speed;
    player->actor.world.pos.z += Math_CosS(yaw) * speed;
    player->linearVelocity = 0.0f;
    Actor_UpdateBgCheckInfo(play, &player->actor, 26.0f, 18.0f, Player_GetHeight(player), 1 | 4);
}

// D-Pad Right: Ganondorf transforms into Ganon (standing on the ground), and Ganon turns back. Ganon comes back
// right away in the next area if it allows him.
static void UpdateGanon(PlayState* play, Player* player, BossGanon* dorf) {
    BossGanon2* ganon = FindPlayerGanon(play);
    Input* input = &play->state.input[0];
    bool toggle = CHECK_BTN_ALL(input->press.button, BTN_DRIGHT);

    if (!IsGanonFormEnabled() || !IsGanonAllowedHere(play)) {
        if (ganon != NULL) {
            BossGanon2_Player_Remove(&ganon->actor, play);
        }
        sWantGanon = false;
        UpdateGanonMusic(play, false);
        return;
    }

    if (ganon == NULL) {
        bool canTransform = BossGanon_Player_CanAct(&dorf->actor) && !BossGanon_Player_IsHidden(&dorf->actor) &&
                            !sLevitating && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                            !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && !GanondorfSongs_IsOcarinaOut();

        // Turned back (or the transformation never finished). Not when Ganon comes back in a new area.
        if (!sWantGanon) {
            UpdateGanonMusic(play, false);
        }

        // Not on horseback (he comes back once Link gets off)
        if ((sWantGanon && !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE)) || (toggle && canTransform)) {
            // home.rot.z = 1: already Ganon (a new area), no transformation
            Actor_Spawn(&play->actorCtx, play, ACTOR_BOSS_GANON2, player->actor.world.pos.x,
                        player->actor.world.pos.y, player->actor.world.pos.z, 0, player->actor.shape.rot.y,
                        sWantGanon ? 1 : 0, GANON2_PARAMS_PLAYER);
            sGanonMusicNewArea = sWantGanon;
            sWantGanon = true;
            StopLevitating();
            sSoftLanding = false;
        }
        return;
    }

    UpdateGanonMusic(play, BossGanon2_Player_IsGanonForm(&ganon->actor));

    if (BossGanon2_Player_IsReverting(&ganon->actor)) {
        sWantGanon = false;
    } else if (toggle && BossGanon2_Player_CanAct(&ganon->actor)) {
        BossGanon2_Player_StartRevert(&ganon->actor, play);
        sWantGanon = false;
    }

    // Charging: Ganon pulls Link along until a wall stops him
    s16 chargeYaw;
    f32 chargeSpeed;
    if (BossGanon2_Player_GetCharge(&ganon->actor, &chargeYaw, &chargeSpeed) && (chargeSpeed > 0.0f)) {
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = chargeYaw;
        player->actor.world.pos.x += Math_SinS(chargeYaw) * chargeSpeed;
        player->actor.world.pos.z += Math_CosS(chargeYaw) * chargeSpeed;
        player->linearVelocity = 0.0f;
        Actor_UpdateBgCheckInfo(play, &player->actor, 26.0f, 18.0f, Player_GetHeight(player), 1 | 4);

        if ((player->actor.bgCheckFlags & BGCHECKFLAG_WALL) &&
            (ABS((s16)(player->actor.wallYaw - chargeYaw + 0x8000)) < 0x3000)) {
            BossGanon2_Player_StopCharge(&ganon->actor, play);
        }
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
        UpdateGanonMusic(play, false);
        sWantGanon = false;
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
        UpdateGanon(play, player, dorf);
        if (FindPlayerGanon(play) == NULL) {
            UpdateLevitation(play, player, dorf);
            UpdateSwordDash(play, player, dorf);
        }
    }
    UpdateCapeShield(play, player, dorf);
    UpdateWaterBoots(play, player, ShouldHideLink(play, player));

    SetLinkShadowHidden(player, ShouldHideLinkModel(play, player));
}

extern "C" void PlayAsGanondorf_FilterInput(PlayState* play, Player* player, Input* input) {
    sSprinting = false;
    if (!ShouldHideLink(play, player)) {
        return;
    }

    BossGanon* dorf = FindPlayerGanondorf(play);
    if (dorf == NULL) {
        return;
    }
    BossGanon2* ganon = FindPlayerGanon(play);
    bool busy = (ganon != NULL) ? BossGanon2_Player_IsBusy(&ganon->actor) : BossGanon_Player_IsBusy(&dorf->actor);

    // B = sword, R = shield, Z = targeting, L = levitate. D-pad Up/Down are the laugh and the organ, D-pad Right
    // the transformation.
    u16 blocked = BTN_B | BTN_R | BTN_Z | BTN_L | BTN_DUP | BTN_DDOWN;

    if (IsGanonFormEnabled()) {
        blocked |= BTN_DRIGHT;
    }
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("SwordStance"), 1)) {
        blocked |= BTN_DLEFT;
    }
    // Ganon's claws can't hold items
    if ((ganon != NULL) || !CVarGetInteger(CVAR_PLAY_AS_GDF("KeepItems"), 0)) {
        blocked |= BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
    }

    // A still opens doors, talks, climbs, grabs and so on, but no rolls, jump attacks or side hops
    u16 doAction = play->interfaceCtx.unk_1F0;

    // Where A would roll or attack (blocked below), holding it makes him sprint
    sSprinting = CVarGetInteger(CVAR_PLAY_AS_GDF("Sprint"), 1) && (ganon == NULL) && !busy && !sLevitating &&
                 CHECK_BTN_ALL(input->cur.button, BTN_A) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                 ((doAction == DO_ACTION_NONE) || (doAction == DO_ACTION_ATTACK) || (doAction == DO_ACTION_JUMP) ||
                  (doAction == DO_ACTION_PUTAWAY));
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

// Holding A on the ground (where A has nothing else to do): Ganondorf sprints (see PlayAsGanondorf_FilterInput)
extern "C" s32 PlayAsGanondorf_IsSprinting(void) {
    return sSprinting && IsEnabled();
}

extern "C" f32 PlayAsGanondorf_GetRunSpeedFactor(PlayState* play, Player* player) {
    if (!ShouldHideLink(play, player)) {
        return 1.0f;
    }
    return CVarGetFloat(CVAR_PLAY_AS_GDF("RunSpeed"), 1.5f) *
           (sSprinting ? CVarGetFloat(CVAR_PLAY_AS_GDF("SprintSpeed"), 1.5f) : 1.0f);
}

// Holding a found item up: it's drawn above his hands instead of Link's
extern "C" s32 PlayAsGanondorf_GetItemHoldPos(PlayState* play, Player* player, Vec3f* pos) {
    if (!ShouldHideLink(play, player)) {
        return false;
    }
    BossGanon* dorf = FindPlayerGanondorf(play);
    return (dorf != NULL) && (FindPlayerGanon(play) == NULL) && BossGanon_Player_GetItemHoldPos(&dorf->actor, pos);
}

extern "C" s32 PlayAsGanondorf_GetFirstPersonEye(Player* player, Vec3f* eye) {
    if (!IsLookingFirstPerson(gPlayState, player)) {
        return false;
    }
    // Link's feet + Ganondorf's hover height + his eyes above his root. Levitating lifts Link too.
    eye->x = player->actor.world.pos.x;
    eye->y = player->actor.world.pos.y + (IsStandingOnGround()
                                              ? GDF_STANDING_EYE_HEIGHT
                                              : CVarGetFloat(CVAR_PLAY_AS_GDF("HoverHeight"), 70.0f) + GDF_EYES_ABOVE_ROOT);
    eye->z = player->actor.world.pos.z;

    // Ganon's eyes are a bit below the top of his head
    BossGanon2* ganon = FindPlayerGanon(gPlayState);
    if (ganon != NULL) {
        eye->y = player->actor.world.pos.y + BossGanon2_Player_GetHeadHeight(&ganon->actor) - 20.0f;
    }
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
        f32 ganondorfHeight = (IsStandingOnGround()
                                   ? GDF_STANDING_CAMERA_HEIGHT
                                   : CVarGetFloat(CVAR_PLAY_AS_GDF("HoverHeight"), 70.0f) + GDF_HEAD_ABOVE_ROOT) *
                              distance;

        // Ganon has his own distance
        BossGanon2* ganon = FindPlayerGanon(gPlayState);
        if (ganon != NULL) {
            ganondorfHeight = (BossGanon2_Player_GetHeadHeight(&ganon->actor) + 20.0f) *
                              CVarGetFloat(CVAR_PLAY_AS_GDF("GanonCameraDistance"), 0.8f);
        }

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

    // Ganon only growls when he's hurt
    if (FindPlayerGanon(gPlayState) != NULL) {
        switch (NA_SE_VO_LI_SWORD_N + ((sfxId - NA_SE_VO_LI_SWORD_N) & 0x1F)) {
            case NA_SE_VO_LI_FALL_L:
            case NA_SE_VO_LI_TAKEN_AWAY:
            case NA_SE_VO_LI_GROAN:
                return NA_SE_EN_MGANON_DAMAGE;
            case NA_SE_VO_LI_DOWN:
                return NA_SE_EN_MGANON_DEAD1;
            default:
                return PLAY_AS_GANONDORF_MUTE;
        }
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
