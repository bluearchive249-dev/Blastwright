// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// SoH: "Play as Ganondorf", the Sacred Realm Temple beyond the Sanctum of Light (stages 2b to 6).
//
// The Sanctum of Light (SCENE_SYOTES2) opens three ways:
//  - East, the Hall of Spears (SCENE_SUTARU): spear and shield guards, and four braziers his blows (or magic) set
//    alight. With the guards down and all four burning, the Seal of the East on the altar at the far end is his.
//  - West, the Hall of Bows (SCENE_HAIRAL_NIWA2): a chasm across the middle (he floats over it; a fall puts him back,
//    hurt), archers on balconies above it, sword guards, and beyond, the Guard Captain. With them all down, the Seal of
//    the West is his.
//  - North, through a gilded gate that rises once both seals are his: Rauru's sanctuary (SCENE_BESITU). Rauru, the
//    Sage of Light, fights him with bolts of light, pillars of light that fall where glyphs flare on the floor, blinking
//    from place to place, and (when hard pressed) a radiant burst he charges unless he's struck out of it.
// When Rauru falls, Ganondorf is taken to the Chamber of Sages, where a pillar of light leads up to the Goddesses' Spring
// (SCENE_DEPTH_TEST): a path of water climbing to the Triforce's sanctum, where Din, Nayru and Farore wait as figures of
// light he can talk to, and the Triforce comes down to him. Claiming it is for good: the Triforce of Power glows on his hand, his wounds close and his magic refills by itself.
//
// The guards are the castle soldiers' model (object_sd) with weapons of our own; Rauru is his own model (object_rl);
// the rooms and the props are generated (tools/gdf_castle/sacred.py, mods/gdf_sacred.o2r). Progress is kept in the
// Sanctum's switch flags (saved with the game).

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "GanondorfText.h"
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
extern PlayState* gPlayState;
void BossGanon_Player_HoldUp(s16 frames);
void GdfTemple_ArriveFromRauru(void);
s32 BossGanon_Player_GetHeldUpPos(Vec3f* pos);
}

#define HUB_SCENE SCENE_SYOTES2
#define HUB_ENTRANCE ENTR_SYOTES2_0
#define EAST_SCENE SCENE_SUTARU
#define EAST_ENTRANCE ENTR_SUTARU_0
#define WEST_SCENE SCENE_HAIRAL_NIWA2
#define WEST_ENTRANCE ENTR_HAIRAL_NIWA2_0
#define RAURU_SCENE SCENE_BESITU
#define RAURU_ENTRANCE ENTR_BESITU_0
#define SAGES_SCENE SCENE_CHAMBER_OF_THE_SAGES
#define SAGES_ENTRANCE ENTR_CHAMBER_OF_THE_SAGES_0
// The Goddesses' Spring, above the chamber: a path of water up to the Triforce's sanctum (sacred.py's build_spring)
#define SPRING_SCENE SCENE_DEPTH_TEST
#define SPRING_ENTRANCE ENTR_DEPTH_TEST_0
#define SPRING_BEAM 0.3f   // (SPRING_BEAM in sacred.py)
#define SPRING_LOW -480.0f // (the hall's foot)
#define SPRING_END 2200.0f // (its south wall)

// The Sanctum's switch flags
#define FLAG_SEAL_EAST 0x02
#define FLAG_SEAL_WEST 0x03
#define FLAG_RAURU_DOWN 0x04
#define FLAG_TRIFORCE 0x05

#define HUB_A 560.0f
#define WING_BEAM 0.3f  // (WING_BEAM in sacred.py)
#define RAURU_BEAM 0.6f // (RAURU_BEAM in sacred.py)
#define RAURU_A 1300.0f // (the arena: an octagon this far across the flats from its middle; RAURU_A in sacred.py)

static const char sHeishiSkel[] = "__OTR__objects/object_sd/gEnHeishiSkel";
static const char sHeishiIdle[] = "__OTR__objects/object_sd/gEnHeishiIdleAnim";
static const char sHeishiWalk[] = "__OTR__objects/object_sd/gEnHeishiWalkAnim";
static const char sHeishiSlam[] = "__OTR__objects/object_sd/gEnHeishiSlamSpearAnim";
static const char sHeishiDie[] = "__OTR__objects/object_sd/gEnHeishiDyingGuardDieAnim";
static const char sHeishiHand[] = "__OTR__objects/object_sd/gEnHeishiHandDL";
static const char sRauruSkel[] = "__OTR__objects/object_rl/object_rl_Skel_007B38";
static const char sRauruIdle[] = "__OTR__objects/object_rl/object_rl_Anim_000A3C";
static const char sRauruCast[] = "__OTR__objects/object_rl/object_rl_Anim_00040C";
static const char sRauruEyes[] = "__OTR__objects/object_rl/object_rl_Tex_003620";
static const char sTriforceDL[] = "__OTR__objects/object_triforce_spot/gTriforceDL";
static const char sArrowDL[] = "__OTR__objects/gameplay_keep/gArrowFarDL";
static const char sIronAxeDL[] = "__OTR__objects/object_ik/gIronKnuckleAxeDL";
// The Iron Knuckle guard: the Iron Knuckle itself, its own model and animations (object_ik)
static const char sIkSkel[] = "__OTR__objects/object_ik/gIronKnuckleSkel";
static const char sIkWalk[] = "__OTR__objects/object_ik/gIronKnuckleWalkAnim";
static const char sIkAttack[] = "__OTR__objects/object_ik/gIronKnuckleVerticalAttackAnim";
static const char sIkDeath[] = "__OTR__objects/object_ik/gIronKnuckleDeathAnim";
static const char sBrazierDL[] = "__OTR__objects/gdf_sacred/gGdfBrazierDL";
static const char sAltarDL[] = "__OTR__objects/gdf_sacred/gGdfAltarDL";
static const char sSwordDL[] = "__OTR__objects/gdf_sacred/gGdfGuardSwordDL";
static const char sShieldDL[] = "__OTR__objects/gdf_sacred/gGdfGuardShieldDL";
static const char sBowDL[] = "__OTR__objects/gdf_sacred/gGdfGuardBowDL";
static const char sOrbDL[] = "__OTR__objects/gdf_sacred/gGdfLightOrbDL";
// Rauru's light from the game's own effects: Phantom Ganon's ball of energy (his bullets, bolts and burst) and the
// columns of light from the Sacred Realm (his pillars)
static const char sEnergyBallDL[] = "__OTR__objects/object_fhg/gPhantomEnergyBallDL";
static const char sCrystalLightDL[] = "__OTR__objects/object_efc_crystal_light/gCrystalLightDL";
#define ENERGY_BALL_PER_ORB 2.7f // (the ball's scale for the size of one of our orbs)

// A ball of light at (x, y, z), facing the camera, tinted (the ball's own white heart, the colour round it)
static void DrawEnergyBall(PlayState* play, f32 x, f32 y, f32 z, f32 scale, u8 r, u8 g, u8 b, u8 alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, r, g, b, 0);
    gDPPipeSync(POLY_XLU_DISP++);
    Matrix_Translate(x, y, z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sEnergyBallDL);
    CLOSE_DISPS(play->state.gfxCtx);
}
static const char sGlyphDL[] = "__OTR__objects/gdf_sacred/gGdfGlyphDL";
static const char sGateDL[] = "__OTR__objects/gdf_sacred/gGdfRauruGateDL";
static const char* sGateGemDL[2] = { "__OTR__objects/gdf_sacred/gGdfRauruGateGem0DL",
                                     "__OTR__objects/gdf_sacred/gGdfRauruGateGem1DL" };

static const Color_RGBA8 sSageColors[6] = { { 120, 255, 140, 255 }, { 255, 150, 90, 255 }, { 120, 170, 255, 255 },
                                            { 210, 130, 255, 255 }, { 255, 190, 100, 255 }, { 255, 245, 160, 255 } };

// ---- Shared ----

static bool TempleAvailable() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.SacredRealm"), 1) &&
           ResourceMgr_FileExists("objects/gdf_sacred/gGdfBrazierDL");
}

static bool TFlag(PlayState* play, s32 f) {
    if ((play != NULL) && (play->sceneNum == HUB_SCENE)) {
        return Flags_GetSwitch(play, f);
    }
    return (gSaveContext.sceneFlags[HUB_SCENE].swch >> f) & 1;
}

static void TSet(PlayState* play, s32 f) {
    if ((play != NULL) && (play->sceneNum == HUB_SCENE)) {
        Flags_SetSwitch(play, f);
    }
    gSaveContext.sceneFlags[HUB_SCENE].swch |= (1u << f);
}

// The Chamber of Sages' only layer without a cutscene is its first; arriving as an adult, the game would load the
// Forest Medallion's (Saria's scene, which ends by sending him to Kokiri Forest). Set when he's sent there for the
// Triforce; z_play.c asks for it as the scene loads (GdfSacred_PlainChamber)
static bool sPlainChamber = false;

extern "C" s32 GdfSacred_PlainChamber(s16 entrance) {
    // (also the Goddesses' Spring: its debug scene has one entrance, not one for each age and time of day, and an
    // adult's or night's layer read the next entrance on, the Bazaar's)
    if (gEntranceTable[entrance].scene == SPRING_SCENE) {
        return true;
    }
    return sPlainChamber && (gEntranceTable[entrance].scene == SAGES_SCENE);
}

// The Chamber of Sages for the Triforce: which Sage's medallion scene plays there is kept in the save
// (chamberCutsceneNum, set by each blue warp; a new file's is 0, the Forest Medallion's). Each Sage's actor in the
// chamber starts its scene when it's theirs, and SoH's "skip blue warp" takes the chamber's entrance with it set as the
// Forest Temple's blue warp, to Kokiri Forest. None of them is the Triforce's: no Sage's number at all (the next blue
// warp sets its own again).
#define GDF_CHAMBER_CS_NONE (CHAMBER_CS_LIGHT + 1)

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

static void PlaceAt(Player* player, f32 x, f32 y, f32 z, s16 yaw) {
    player->actor.world.pos = { x, y, z };
    player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
}

static void Sparks(PlayState* play, Vec3f* pos, f32 spread, s32 count, f32 rise, s32 sage) {
    static Vec3f sNoAccel = { 0.0f, 0.0f, 0.0f };

    for (s32 i = 0; i < count; i++) {
        Color_RGBA8 prim = sSageColors[(sage >= 0) ? sage : ((s32)Rand_ZeroFloat(6.0f) % 6)];
        Color_RGBA8 env = prim;
        Vec3f p = { pos->x + Rand_CenteredFloat(spread), pos->y + Rand_CenteredFloat(spread),
                    pos->z + Rand_CenteredFloat(spread) };
        Vec3f v = { Rand_CenteredFloat(0.8f), rise + Rand_ZeroFloat(rise), Rand_CenteredFloat(0.8f) };

        prim.r = (u8)MIN(255, prim.r + 80);
        prim.g = (u8)MIN(255, prim.g + 80);
        prim.b = (u8)MIN(255, prim.b + 80);
        env.a = 0;
        EffectSsKiraKira_SpawnDispersed(play, &p, &v, &sNoAccel, &prim, &env, (s16)(Rand_ZeroFloat(900.0f) + 1100.0f),
                                        20);
    }
}

// A line of text (shown in the game's own box: the text is ours, the id one nothing else of his uses)
#define TEMPLE_TEXT_ID 0x0201
static std::string sPendingText;
static void Say(PlayState* play, const char* text) {
    sPendingText = text;
    Message_StartTextbox(play, TEMPLE_TEXT_ID, NULL);
}

// The three Goddesses' words (each says the next of hers each time he talks to her)
#define GODDESS_TEXT_ID 0x0202 // (to 0x0204: Din, Nayru, Farore)
static const char* sGoddessLines[3][3] = {
    { "I am %rDin%w, who shaped the red earth with my burning arms.^Power is my gift, King of the Gerudo. You "
      "have shown me nothing but power, and so it is power the Triforce will answer you with.",
      "Do not mistake my gift for a blessing. Power alone burns whoever holds it.^Still... I would rather see it "
      "held than left to sleep. Take it, and let us see what you make of it.",
      "Rauru guarded this place for an age. You broke through him in an afternoon.^Ha! I have not been this "
      "entertained since the world was young." },
    { "I am %bNayru%w, who gave the world its law and order.^The Triforce does not choose good or evil. It "
      "weighs the heart of whoever touches it, and divides itself if that heart is out of balance.",
      "Wisdom would tell you to leave it where it rests.^But you did not walk through a sealed temple to be wise, "
      "did you?",
      "I have seen what follows. Two others will carry what you cannot.^Remember that, when a boy with a "
      "sword stands before you." },
    { "I am %gFarore%w, who breathed life into every creature that walks the land.^You came all this way alone, "
      "past spears, light and the Sage himself. That is not nothing.",
      "Courage is not only charging forward. It is also knowing when to turn back.^...I can see you will not "
      "turn back.",
      "The water on this path flows from the spring where we left this world.^Drink of it if you like. It will "
      "not make you any kinder." },
};
static s16 sGoddessLine[3] = { 0, 0, 0 };

static void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if ((gPlayState != NULL) && (gPlayState->sceneNum == SPRING_SCENE) && (*textId >= GODDESS_TEXT_ID) &&
        (*textId < GODDESS_TEXT_ID + 3)) {
        s32 g = *textId - GODDESS_TEXT_ID;
        CustomMessage msg = CustomMessage(GdfFitBoxes(sGoddessLines[g][sGoddessLine[g]]), TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM);
        msg.AutoFormat();
        msg.LoadIntoFont();
        *loadFromMessageTable = false;
        sGoddessLine[g] = (sGoddessLine[g] + 1) % 3;
        return;
    }
    if ((*textId != TEMPLE_TEXT_ID) || sPendingText.empty()) {
        return;
    }
    CustomMessage msg = CustomMessage(GdfFitBoxes(sPendingText), TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM);
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
    sPendingText.clear();
}

static void DrawObj(PlayState* play, const char* dl, Gfx** disp) {
    gSPMatrix((*disp)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*disp)++, (Gfx*)dl);
}

static s32 sGuardId = -1, sShotId = -1, sBrazierId = -1, sAltarId = -1, sGateId = -1, sRauruId = -1, sTriforceId = -1,
           sPortalId = -1, sGoddessId = -1;

static s32 CountLiving(PlayState* play, s32 id) {
    s32 n = 0;
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        if ((a->id == id) && (a->update != NULL) && (a->colChkInfo.health > 0)) {
            n++;
        }
    }
    return n;
}

// ---- Guards ----

// GUARD_IRON: an Iron Knuckle, a soldier built far bigger and broader, swinging the Iron Knuckle's own axe
enum { GUARD_SPEAR, GUARD_SWORD, GUARD_BOW, GUARD_SHIELD, GUARD_CAPTAIN, GUARD_IRON };
enum { GS_IDLE, GS_ENGAGE, GS_ATTACK, GS_HURT, GS_DIE, GS_CHARGE };

typedef struct {
    Actor actor;
    SkelAnime skel;
    Vec3s jt[30]; // (17 for the soldiers, 30 for the Iron Knuckle)
    Vec3s mt[30];
    ColliderCylinder body;
    ColliderCylinder weapon;
    s16 type;
    s16 state;
    s16 timer;
    s16 iframes;
    s16 cooldown;
    s16 attacks;
    u8 dormant; // the captain waits on the far side of the chasm
    u8 weaponActive;
} GdfGuard;

static ColliderCylinderInit sGuardBodyInit = {
    { COLTYPE_HIT0, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 26, 90, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sGuardWeaponInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x08 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE,
      OCELEM_NONE },
    { 24, 60, 0, { 0, 0, 0 } },
};

static void Guard_Anim(GdfGuard* g, const char* anim, f32 speed, u8 mode, f32 morph) {
    if (g->type == GUARD_IRON) { // the Iron Knuckle's own: standing (its walk, held), walking, the overhead chop, falling
        if (anim == sHeishiIdle) {
            anim = sIkWalk;
            speed = 0.0f;
        } else if (anim == sHeishiWalk) {
            anim = sIkWalk;
            speed = 1.0f;
        } else if (anim == sHeishiSlam) {
            anim = sIkAttack;
            speed = 1.5f;
        } else if (anim == sHeishiDie) {
            anim = sIkDeath;
            speed = 1.0f;
        }
    }
    Animation_Change(&g->skel, (AnimationHeader*)anim, speed, 0.0f, Animation_GetLastFrame((void*)anim), mode, morph);
}

static void Guard_Init(Actor* thisx, PlayState* play) {
    GdfGuard* g = (GdfGuard*)thisx;
    static const u8 sHp[] = { 4, 4, 3, 6, 18, 10 };

    g->type = thisx->params & 0xF;
    g->dormant = (thisx->params & 0x10) != 0;
    Actor_SetScale(thisx, (g->type == GUARD_IRON) ? 0.012f : (g->type == GUARD_CAPTAIN) ? 0.014f : 0.01f);
    if (g->type == GUARD_IRON) {
        SkelAnime_InitFlex(play, &g->skel, (FlexSkeletonHeader*)sIkSkel, (AnimationHeader*)sIkWalk, g->jt, g->mt, 30);
        g->skel.playSpeed = 0.0f;
    } else {
        SkelAnime_Init(play, &g->skel, (SkeletonHeader*)sHeishiSkel, (AnimationHeader*)sHeishiIdle, g->jt, g->mt, 17);
    }
    Collider_InitCylinder(play, &g->body);
    Collider_SetCylinder(play, &g->body, thisx, &sGuardBodyInit);
    Collider_InitCylinder(play, &g->weapon);
    Collider_SetCylinder(play, &g->weapon, thisx, &sGuardWeaponInit);
    if (g->type == GUARD_CAPTAIN) {
        g->body.dim.radius = 36;
        g->body.dim.height = 120;
        g->weapon.dim.radius = 34;
        g->weapon.info.toucher.damage = 0x10;
    }
    if (g->type == GUARD_IRON) {
        g->body.dim.radius = 48;
        g->body.dim.height = 150;
        g->weapon.dim.radius = 46;
        g->weapon.dim.height = 90;
        g->weapon.info.toucher.damage = 0x18;
    }
    thisx->colChkInfo.health = sHp[g->type];
    thisx->colChkInfo.mass = ((g->type == GUARD_CAPTAIN) || (g->type == GUARD_IRON)) ? MASS_HEAVY : 120;
    thisx->gravity = -1.5f;
    thisx->targetMode = 3;
    g->state = GS_IDLE;
    g->cooldown = 20 + (s16)Rand_ZeroFloat(30.0f);
}

static void Guard_Destroy(Actor* thisx, PlayState* play) {
    GdfGuard* g = (GdfGuard*)thisx;
    Collider_DestroyCylinder(play, &g->body);
    Collider_DestroyCylinder(play, &g->weapon);
}

static f32 Guard_Reach(GdfGuard* g) {
    switch (g->type) {
        case GUARD_SPEAR:
            return 115.0f;
        case GUARD_CAPTAIN:
            return 140.0f;
        case GUARD_IRON:
            return 170.0f;
        default:
            return 85.0f;
    }
}

static void Guard_Shoot(GdfGuard* g, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f from = g->actor.world.pos;
    Vec3f to = player->actor.world.pos;
    Actor* shot;

    from.y += 80.0f;
    to.y += 50.0f;
    shot = Actor_Spawn(&play->actorCtx, play, sShotId, from.x, from.y, from.z, 0, 0, 0, 0);
    if (shot != NULL) {
        f32 d = sqrtf(SQ(to.x - from.x) + SQ(to.y - from.y) + SQ(to.z - from.z));
        f32 s = 16.0f / MAX(d, 1.0f);
        shot->velocity.x = (to.x - from.x) * s;
        shot->velocity.y = (to.y - from.y) * s;
        shot->velocity.z = (to.z - from.z) * s;
    }
    Audio_PlayActorSound2(&g->actor, NA_SE_IT_ARROW_SHOT);
}

static void Guard_Update(Actor* thisx, PlayState* play) {
    GdfGuard* g = (GdfGuard*)thisx;
    Player* player = GET_PLAYER(play);
    f32 dist = Actor_WorldDistXZToActor(thisx, &player->actor);
    s16 yawTo = Actor_WorldYawTowardActor(thisx, &player->actor);
    f32 reach = Guard_Reach(g);

    SkelAnime_Update(&g->skel);
    if (g->iframes > 0) {
        g->iframes--;
    }
    if (g->cooldown > 0) {
        g->cooldown--;
    }
    g->weaponActive = false;

    // Struck
    if ((g->body.base.acFlags & AC_HIT) && (g->state != GS_DIE)) {
        g->body.base.acFlags &= ~AC_HIT;
        if (g->iframes == 0) {
            s16 facing = (s16)(thisx->shape.rot.y - (yawTo));
            bool front = ABS(facing) < 0x3000;
            if (((g->type == GUARD_SHIELD) || (g->type == GUARD_CAPTAIN)) && front && (g->state != GS_ATTACK) &&
                (Rand_ZeroOne() < ((g->type == GUARD_CAPTAIN) ? 0.35f : 0.55f))) {
                // the shield takes it
                Vec3f p = thisx->world.pos;
                p.x += Math_SinS(thisx->shape.rot.y) * 30.0f;
                p.z += Math_CosS(thisx->shape.rot.y) * 30.0f;
                p.y += 60.0f;
                EffectSsHitMark_SpawnFixedScale(play, 3, &p);
                Audio_PlayActorSound2(thisx, NA_SE_IT_SHIELD_REFLECT_SW);
                g->iframes = 8;
                thisx->speedXZ = -6.0f;
            } else {
                thisx->colChkInfo.health--;
                g->iframes = 14;
                Actor_SetColorFilter(thisx, 0x4000, 255, 0, 10);
                Audio_PlayActorSound2(thisx, (g->type == GUARD_IRON)      ? NA_SE_EN_IRONNACK_ARMOR_HIT
                                             : (g->type == GUARD_CAPTAIN) ? NA_SE_EN_IRONNACK_DAMAGE
                                                                          : NA_SE_EN_STAL_DAMAGE);
                if (thisx->colChkInfo.health <= 0) {
                    g->state = GS_DIE;
                    g->timer = 0;
                    thisx->speedXZ = -4.0f;
                    thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
                    Guard_Anim(g, sHeishiDie, 1.0f, ANIMMODE_ONCE, -4.0f);
                    Audio_PlayActorSound2(thisx, (g->type == GUARD_IRON) ? NA_SE_EN_IRONNACK_DEAD : NA_SE_EN_STAL_DEAD);
                } else if ((g->type != GUARD_CAPTAIN) && (g->type != GUARD_IRON)) {
                    g->state = GS_HURT;
                    g->timer = 12;
                    thisx->speedXZ = -9.0f;
                    Guard_Anim(g, sHeishiIdle, 1.0f, ANIMMODE_LOOP, -3.0f);
                } else {
                    thisx->speedXZ = (g->type == GUARD_IRON) ? 0.0f : -3.0f; // (an Iron Knuckle doesn't give ground)
                }
            }
        }
    }

    switch (g->state) {
        case GS_IDLE:
            Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
            if (g->dormant) {
                // (the west hall's: he's crossed the chasm; the Temple of Light's (0x20): he comes into sight)
                if (((thisx->params & 0x20) == 0) ? (player->actor.world.pos.z < -700.0f) : (dist < 650.0f)) {
                    g->dormant = false;
                    if (g->type != GUARD_CAPTAIN) {
                        break; // (the captain speaks for them)
                    }
                    func_800F5ACC(NA_BGM_MINI_BOSS);
                    Say(play, "%rGuard Captain:%w Halt, thief of the desert! No one has crossed this hall since the "
                              "Sages sealed it, and no one will!");
                }
                break;
            }
            if (dist < 1100.0f) {
                g->state = GS_ENGAGE;
                Guard_Anim(g, (g->type == GUARD_BOW) ? sHeishiIdle : sHeishiWalk, 1.4f, ANIMMODE_LOOP, -6.0f);
            }
            break;

        case GS_ENGAGE:
            Math_SmoothStepToS(&thisx->world.rot.y, yawTo, 4, 0x800, 0x80);
            if (g->type == GUARD_BOW) {
                thisx->speedXZ = 0.0f;
                if (g->cooldown == 0) {
                    Guard_Shoot(g, play);
                    g->cooldown = 70 + (s16)Rand_ZeroFloat(40.0f);
                }
                break;
            }
            if ((g->type == GUARD_CAPTAIN) && (g->cooldown == 0) && (dist > 260.0f) && (++g->attacks % 3 == 0)) {
                g->state = GS_CHARGE; // a running thrust
                g->timer = 0;
                Guard_Anim(g, sHeishiWalk, 2.6f, ANIMMODE_LOOP, -3.0f);
                break;
            }
            if (dist > reach * 0.85f) {
                Math_StepToF(&thisx->speedXZ, (g->type == GUARD_IRON) ? 1.8f : (g->type == GUARD_CAPTAIN) ? 4.5f : 3.5f,
                             0.6f);
                if ((g->type == GUARD_IRON) && (Animation_OnFrame(&g->skel, 1.0f) || Animation_OnFrame(&g->skel, 20.0f))) {
                    Audio_PlayActorSound2(thisx, NA_SE_EN_IRONNACK_WALK);
                }
            } else {
                Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
                if (g->cooldown == 0) {
                    g->state = GS_ATTACK;
                    g->timer = 0;
                    Guard_Anim(g, sHeishiSlam, (g->type == GUARD_SWORD) ? 2.0f : (g->type == GUARD_IRON) ? 0.9f : 1.6f,
                               ANIMMODE_ONCE, -3.0f);
                    Audio_PlayActorSound2(thisx, (g->type == GUARD_IRON) ? NA_SE_EN_IRONNACK_SWING_AXE
                                                                         : NA_SE_IT_SWORD_SWING);
                }
            }
            break;

        case GS_ATTACK: {
            s16 windup = (g->type == GUARD_SWORD) ? 6 : (g->type == GUARD_IRON) ? 14 : 10; // (its chop lands at 21)
            g->timer++;
            Math_SmoothStepToS(&thisx->world.rot.y, yawTo, 8, 0x200, 0x40);
            Math_StepToF(&thisx->speedXZ, (g->timer > windup && g->timer < windup + 6) ? 4.0f : 0.0f, 1.0f);
            if ((g->timer >= windup) && (g->timer < windup + 7)) {
                g->weaponActive = true;
            }
            if ((g->type == GUARD_IRON) && (g->timer == windup)) { // the axe bites into the floor
                Vec3f p = thisx->world.pos;
                p.x += Math_SinS(thisx->shape.rot.y) * 150.0f;
                p.z += Math_CosS(thisx->shape.rot.y) * 150.0f;
                Audio_PlayActorSound2(thisx, NA_SE_EN_IRONNACK_HIT_GND);
                Actor_RequestQuakeAndRumble(thisx, play, 4, 10);
                Actor_SpawnFloorDustRing(play, thisx, &p, 40.0f, 10, 8.0f, 500, 20, true);
            }
            if (g->timer > windup + ((g->type == GUARD_IRON) ? 34 : 22)) {
                g->state = GS_ENGAGE;
                g->cooldown = 25 + (s16)Rand_ZeroFloat(35.0f);
                Guard_Anim(g, sHeishiWalk, 1.4f, ANIMMODE_LOOP, -6.0f);
            }
            break;
        }

        case GS_CHARGE:
            g->timer++;
            thisx->speedXZ = 12.0f;
            g->weaponActive = g->timer > 4;
            if ((g->timer > 26) || (thisx->bgCheckFlags & BGCHECKFLAG_WALL)) {
                g->state = GS_ENGAGE;
                g->cooldown = 40;
                thisx->speedXZ = 0.0f;
                Guard_Anim(g, sHeishiWalk, 1.4f, ANIMMODE_LOOP, -6.0f);
            }
            break;

        case GS_HURT:
            Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
            if (--g->timer <= 0) {
                g->state = GS_ENGAGE;
                Guard_Anim(g, (g->type == GUARD_BOW) ? sHeishiIdle : sHeishiWalk, 1.4f, ANIMMODE_LOOP, -6.0f);
            }
            break;

        case GS_DIE:
            Math_StepToF(&thisx->speedXZ, 0.0f, 0.5f);
            if (++g->timer == 40) {
                Vec3f p = thisx->world.pos;
                p.y += 50.0f;
                Sparks(play, &p, 60.0f, 16, 1.0f, 5);
                Item_DropCollectibleRandom(play, thisx, &thisx->world.pos, 0x30);
                if (g->type == GUARD_CAPTAIN) {
                    func_800F5B58();
                    Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
                }
                Actor_Kill(thisx);
                return;
            }
            break;
    }

    thisx->shape.rot.y = thisx->world.rot.y;
    if (g->state == GS_HURT || (g->state == GS_DIE && thisx->speedXZ < 0.0f)) {
        // knocked back: away from where he faces
        thisx->world.rot.y = thisx->shape.rot.y;
    }
    // Not off the edge: a guard stops short of a drop (the chasm) rather than walking or charging into it
    if ((thisx->speedXZ != 0.0f) && (g->state != GS_DIE) && (thisx->bgCheckFlags & BGCHECKFLAG_GROUND)) {
        f32 dir = (thisx->speedXZ > 0.0f) ? 1.0f : -1.0f;
        Vec3f ahead = { thisx->world.pos.x + Math_SinS(thisx->world.rot.y) * 45.0f * dir, thisx->world.pos.y + 40.0f,
                        thisx->world.pos.z + Math_CosS(thisx->world.rot.y) * 45.0f * dir };
        CollisionPoly* poly;
        f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &ahead);

        if (floorY < thisx->world.pos.y - 60.0f) {
            thisx->speedXZ = 0.0f;
            if (g->state == GS_CHARGE) {
                g->timer = 99; // (the charge ends at the edge)
            }
        }
    }
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 26.0f, 26.0f, 60.0f, 0x1D);
    // Over the edge all the same (knocked off): gone into the depths
    if (thisx->world.pos.y < thisx->home.pos.y - 150.0f) {
        Audio_PlayActorSound2(thisx, (g->type == GUARD_IRON) ? NA_SE_EN_IRONNACK_DEAD : NA_SE_EN_STAL_DEAD);
        Actor_Kill(thisx);
        return;
    }
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += (g->type == GUARD_IRON) ? 150.0f : (g->type == GUARD_CAPTAIN) ? 120.0f : 85.0f;

    if (g->state != GS_DIE) {
        Collider_UpdateCylinder(thisx, &g->body);
        CollisionCheck_SetAC(play, &play->colChkCtx, &g->body.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &g->body.base);
    }
    if (g->weaponActive) {
        f32 r = reach - g->weapon.dim.radius * 0.5f;
        g->weapon.dim.pos.x = (s16)(thisx->world.pos.x + Math_SinS(thisx->shape.rot.y) * r);
        g->weapon.dim.pos.y = (s16)(thisx->world.pos.y + 20.0f);
        g->weapon.dim.pos.z = (s16)(thisx->world.pos.z + Math_CosS(thisx->shape.rot.y) * r);
        CollisionCheck_SetAT(play, &play->colChkCtx, &g->weapon.base);
    }
}

// An Iron Knuckle's bulk: the torso, the chest, the arms and the thighs drawn broader (thicker across, not longer;
// the limbs' children load their own matrices, so only these grow)
static Gfx* Guard_Broaden(PlayState* play, Gfx* dl, f32 thick) {
    Gfx* wrap = (Gfx*)Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 4);
    Mtx* m;

    Matrix_Push();
    Matrix_Scale(1.0f, thick, thick, MTXMODE_NEW);
    m = MATRIX_NEWMTX(play->state.gfxCtx);
    Matrix_Pop();
    gSPMatrix(&wrap[0], m, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPDisplayList(&wrap[1], dl);
    gSPPopMatrix(&wrap[2], G_MTX_MODELVIEW);
    gSPEndDisplayList(&wrap[3]);
    return wrap;
}

static s32 Guard_OverrideLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    GdfGuard* g = (GdfGuard*)thisx;
    if ((limbIndex == 15) && (g->type != GUARD_SPEAR) && (g->type != GUARD_CAPTAIN)) {
        *dList = (Gfx*)sHeishiHand; // (the spear goes; a hand of his own)
    }
    if ((g->type == GUARD_IRON) && (*dList != NULL)) {
        switch (limbIndex) {
            case 2:  // torso
            case 9:  // chest armour
                *dList = Guard_Broaden(play, *dList, 1.45f);
                break;
            case 10: // upper arms and forearms
            case 11:
            case 13:
            case 14:
            case 3: // thighs
            case 6:
                *dList = Guard_Broaden(play, *dList, 1.35f);
                break;
        }
    }
    return false;
}

static void Guard_PostLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    GdfGuard* g = (GdfGuard*)thisx;
    const char* dl = NULL;
    f32 sc = 100.0f; // (props are in world units; the limbs' in model units)

    OPEN_DISPS(play->state.gfxCtx);
    if ((limbIndex == 15) && (g->type == GUARD_IRON)) {
        // the Iron Knuckle's axe, as its own hand holds it (its colours from segments 8 and 10, set in the draw)
        Matrix_Push();
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sIronAxeDL);
        Matrix_Pop();
    } else if ((limbIndex == 15) && (g->type == GUARD_SWORD)) {
        dl = sSwordDL;
    } else if ((limbIndex == 12) && (g->type == GUARD_BOW)) {
        dl = sBowDL;
    } else if ((limbIndex == 12) && ((g->type == GUARD_SHIELD) || (g->type == GUARD_CAPTAIN))) {
        dl = sShieldDL;
    }
    if (dl != NULL) {
        Matrix_Push();
        Matrix_Translate(400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        if (dl == sShieldDL) {
            Matrix_RotateY(-M_PI / 2, MTXMODE_APPLY);
        } else if (dl == sBowDL) {
            Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
        }
        Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
        DrawObj(play, dl, &POLY_OPA_DISP);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Guard_Draw(Actor* thisx, PlayState* play) {
    GdfGuard* g = (GdfGuard*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (g->type == GUARD_CAPTAIN) {
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 200, 90, 255);
    }
    if (g->type == GUARD_IRON) { // the axe's colours, as a silver Iron Knuckle's
        Gfx* seg8 = (Gfx*)Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 4);
        Gfx* segA = (Gfx*)Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 4);
        gDPPipeSync(&seg8[0]);
        gDPSetPrimColor(&seg8[1], 0, 0, 245, 255, 205, 255);
        gDPSetEnvColor(&seg8[2], 30, 35, 0, 255);
        gSPEndDisplayList(&seg8[3]);
        gDPPipeSync(&segA[0]);
        gDPSetPrimColor(&segA[1], 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(&segA[2], 30, 40, 20, 255);
        gSPEndDisplayList(&segA[3]);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)seg8);
        gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)segA);
    }
    if (g->type == GUARD_IRON) {
        // The Iron Knuckle drawn as the game draws a silver one: its colours in segments 8 to 10
        static const u8 sCol[3][6] = { { 245, 255, 205, 30, 35, 0 }, { 185, 135, 25, 20, 20, 0 },
                                       { 255, 255, 255, 30, 40, 20 } };

        for (s32 i = 0; i < 3; i++) {
            Gfx* seg = (Gfx*)Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 4);
            gDPPipeSync(&seg[0]);
            gDPSetPrimColor(&seg[1], 0, 0, sCol[i][0], sCol[i][1], sCol[i][2], 255);
            gDPSetEnvColor(&seg[2], sCol[i][3], sCol[i][4], sCol[i][5], 255);
            gSPEndDisplayList(&seg[3]);
            gSPSegment(POLY_OPA_DISP++, 0x08 + i, (uintptr_t)seg);
        }
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (g->type == GUARD_IRON) {
        SkelAnime_DrawSkeletonOpa(play, &g->skel, NULL, NULL, thisx);
        return;
    }
    SkelAnime_DrawOpa(play, g->skel.skeleton, g->skel.jointTable, Guard_OverrideLimb, Guard_PostLimb, thisx);
}

// ---- Shots: the archers' arrows (0) and Rauru's bolts of light (1) ----

typedef struct {
    Actor actor;
    ColliderCylinder col;
    s16 timer;
} GdfShot;

static ColliderCylinderInit sShotInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x08 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_NONE },
    { 14, 28, -14, { 0, 0, 0 } },
};

static void Shot_Init(Actor* thisx, PlayState* play) {
    GdfShot* s = (GdfShot*)thisx;
    Actor_SetScale(thisx, (thisx->params == 0) ? 0.01f : 1.0f);
    Collider_InitCylinder(play, &s->col);
    Collider_SetCylinder(play, &s->col, thisx, &sShotInit);
    if (thisx->params == 1) {
        s->col.info.toucher.damage = 0x10;
        s->col.dim.radius = 22;
    }
}

static void Shot_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((GdfShot*)thisx)->col);
}

static void Shot_Update(Actor* thisx, PlayState* play) {
    GdfShot* s = (GdfShot*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f prev = thisx->world.pos, hit;
    CollisionPoly* poly;
    s32 bgId;

    if (thisx->params == 1) { // homing, slowly
        Vec3f to = player->actor.world.pos;
        f32 d, sp;
        to.y += 50.0f;
        thisx->velocity.x += (to.x - thisx->world.pos.x) * 0.004f;
        thisx->velocity.y += (to.y - thisx->world.pos.y) * 0.004f;
        thisx->velocity.z += (to.z - thisx->world.pos.z) * 0.004f;
        d = sqrtf(SQ(thisx->velocity.x) + SQ(thisx->velocity.y) + SQ(thisx->velocity.z));
        sp = 7.5f / MAX(d, 0.01f);
        thisx->velocity.x *= sp;
        thisx->velocity.y *= sp;
        thisx->velocity.z *= sp;
        if ((s->timer % 3) == 0) {
            Sparks(play, &thisx->world.pos, 10.0f, 1, 0.2f, 5);
        }
    }
    thisx->world.pos.x += thisx->velocity.x;
    thisx->world.pos.y += thisx->velocity.y;
    thisx->world.pos.z += thisx->velocity.z;
    thisx->shape.rot.y = Math_Atan2S(thisx->velocity.z, thisx->velocity.x);
    thisx->shape.rot.x = -Math_Atan2S(sqrtf(SQ(thisx->velocity.x) + SQ(thisx->velocity.z)), thisx->velocity.y);
    if ((++s->timer > 150) || (s->col.base.atFlags & AT_HIT) || (s->col.base.acFlags & AC_HIT) ||
        BgCheck_EntityLineTest1(&play->colCtx, &prev, &thisx->world.pos, &hit, &poly, true, true, true, true, &bgId)) {
        if (thisx->params == 1) {
            Sparks(play, &thisx->world.pos, 20.0f, 8, 0.6f, 5);
        }
        Actor_Kill(thisx);
        return;
    }
    Collider_UpdateCylinder(thisx, &s->col);
    CollisionCheck_SetAT(play, &play->colChkCtx, &s->col.base);
    if (thisx->params == 1) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &s->col.base); // (he can strike them out of the air)
    }
}

static void Shot_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (thisx->params == 0) {
        DrawObj(play, sArrowDL, &POLY_OPA_DISP);
    } else {
        u8 pulse = (u8)(200 + 55 * Math_SinS(((GdfShot*)thisx)->timer * 0x1800));
        DrawEnergyBall(play, thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z,
                       thisx->scale.x * ENERGY_BALL_PER_ORB, 255, pulse, 90, 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- Braziers (params: the switch flag that keeps them lit) ----

typedef struct {
    Actor actor;
    ColliderCylinder col;
    u8 lit;
} GdfBrazier;

static ColliderCylinderInit sBrazierInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 30, 120, 0, { 0, 0, 0 } },
};

static void Brazier_Light(GdfBrazier* b, PlayState* play) {
    Vec3f top = b->actor.world.pos;
    top.y += 116.0f;
    b->lit = true;
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, top.x, top.y, top.z, 0, 0, 0, 0);
}

static void Brazier_Init(Actor* thisx, PlayState* play) {
    GdfBrazier* b = (GdfBrazier*)thisx;
    Actor_SetScale(thisx, 1.0f);
    Collider_InitCylinder(play, &b->col);
    Collider_SetCylinder(play, &b->col, thisx, &sBrazierInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    if (Flags_GetSwitch(play, thisx->params & 0x3F)) {
        Brazier_Light(b, play);
    }
}

static void Brazier_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((GdfBrazier*)thisx)->col);
}

static void Brazier_Update(Actor* thisx, PlayState* play) {
    GdfBrazier* b = (GdfBrazier*)thisx;
    if ((b->col.base.acFlags & AC_HIT) && !b->lit) {
        Vec3f top = thisx->world.pos;
        top.y += 116.0f;
        Flags_SetSwitch(play, thisx->params & 0x3F);
        Brazier_Light(b, play);
        Audio_PlayActorSound2(thisx, NA_SE_EV_FLAME_IGNITION);
        Sparks(play, &top, 30.0f, 10, 1.0f, 1);
    }
    b->col.base.acFlags &= ~AC_HIT;
    Collider_UpdateCylinder(thisx, &b->col);
    CollisionCheck_SetAC(play, &play->colChkCtx, &b->col.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &b->col.base);
}

static void Brazier_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    DrawObj(play, sBrazierDL, &POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- The altar and its seal (params: 0 east, 1 west) ----

typedef struct {
    Actor actor;
    s16 timer;
    u8 ready;
} GdfAltar;

static bool WingCleared(PlayState* play) {
    if (CountLiving(play, sGuardId) > 0) {
        return false;
    }
    if (play->sceneNum == EAST_SCENE) {
        for (s32 f = 0x10; f < 0x14; f++) {
            if (!Flags_GetSwitch(play, f)) {
                return false;
            }
        }
    }
    return true;
}

static void Altar_Init(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, 1.0f);
}

static void Altar_Update(Actor* thisx, PlayState* play) {
    GdfAltar* a = (GdfAltar*)thisx;
    Player* player = GET_PLAYER(play);
    s32 flag = (thisx->params == 0) ? FLAG_SEAL_EAST : FLAG_SEAL_WEST;
    Vec3f orb = thisx->world.pos;

    a->timer++;
    orb.y += 140.0f;
    if (TFlag(play, flag)) {
        return;
    }
    if (!a->ready && WingCleared(play)) {
        a->ready = true;
        Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE_FLASH);
        Sparks(play, &orb, 60.0f, 24, 1.0f, -1);
    }
    if (a->ready && (a->timer % 4 == 0)) {
        Sparks(play, &orb, 30.0f, 1, 0.6f, (thisx->params == 0) ? 1 : 2);
    }
    if (a->ready && CanLeave(play) && (Actor_WorldDistXZToActor(thisx, &player->actor) < 90.0f)) {
        TSet(play, flag);
        Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
        Sparks(play, &orb, 80.0f, 40, 1.4f, -1);
        Say(play, (thisx->params == 0)
                      ? "You took the %ySeal of the East%w!^One of the two seals on the way to Rauru, the Sage of "
                        "Light, is broken."
                      : "You took the %ySeal of the West%w!^One of the two seals on the way to Rauru, the Sage of "
                        "Light, is broken.");
    }
}

static void Altar_Draw(Actor* thisx, PlayState* play) {
    GdfAltar* a = (GdfAltar*)thisx;
    s32 flag = (thisx->params == 0) ? FLAG_SEAL_EAST : FLAG_SEAL_WEST;
    const Color_RGBA8* c = &sSageColors[(thisx->params == 0) ? 1 : 2];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    DrawObj(play, sAltarDL, &POLY_OPA_DISP);
    if (!TFlag(play, flag)) {
        f32 bob = Math_SinS(a->timer * 0x400) * 6.0f;
        u8 k = a->ready ? 255 : 90;
        Matrix_Translate(0.0f, 140.0f + bob, 0.0f, MTXMODE_APPLY);
        Matrix_RotateY(a->timer * 0.05f, MTXMODE_APPLY);
        Matrix_Scale(1.6f, 1.6f, 1.6f, MTXMODE_APPLY);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, c->r * k / 255, c->g * k / 255, c->b * k / 255, 255);
        DrawObj(play, sOrbDL, &POLY_OPA_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- The gilded gate to Rauru (rises once both seals are his) ----

typedef struct {
    Actor actor;
    f32 rise;
    s16 timer;
} GdfGate;

static bool GateOpen(PlayState* play) {
    return TFlag(play, FLAG_SEAL_EAST) && TFlag(play, FLAG_SEAL_WEST);
}

static void Gate_Init(Actor* thisx, PlayState* play) {
    GdfGate* g = (GdfGate*)thisx;
    Actor_SetScale(thisx, 1.0f);
    g->rise = (GateOpen(play) && TFlag(play, FLAG_RAURU_DOWN)) ? 400.0f : 0.0f;
}

static void Gate_Update(Actor* thisx, PlayState* play) {
    GdfGate* g = (GdfGate*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f p = player->actor.world.pos;

    g->timer++;
    if (GateOpen(play) && (g->rise < 400.0f)) {
        if ((g->rise == 0.0f) && (Actor_WorldDistXZToActor(thisx, &player->actor) < 700.0f)) {
            Audio_PlayActorSound2(thisx, NA_SE_EV_IRON_DOOR_OPEN);
        }
        if (Actor_WorldDistXZToActor(thisx, &player->actor) < 700.0f || g->rise > 0.0f) {
            g->rise = MIN(400.0f, g->rise + 4.0f);
        }
    }
    // shut: he can't pass it
    if ((g->rise < 260.0f) && (fabsf(p.x) < 175.0f) && (p.z < -HUB_A + 40.0f) && (p.z > -HUB_A - 60.0f)) {
        player->actor.world.pos.z = -HUB_A + 40.0f;
    }
}

static void Gate_Draw(Actor* thisx, PlayState* play) {
    GdfGate* g = (GdfGate*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, g->rise, 0.0f, MTXMODE_APPLY);
    DrawObj(play, sGateDL, &POLY_OPA_DISP);
    for (s32 i = 0; i < 2; i++) {
        bool lit = TFlag(play, (i == 0) ? FLAG_SEAL_WEST : FLAG_SEAL_EAST);
        const Color_RGBA8* c = &sSageColors[(i == 0) ? 2 : 1];
        u8 k = lit ? (u8)(220 + 35 * Math_SinS(g->timer * 0x600)) : 50;
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, c->r * k / 255, c->g * k / 255, c->b * k / 255, 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGateGemDL[i]);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- Rauru, the Sage of Light ----

enum { RS_WAIT, RS_HOVER, RS_BOLTS, RS_PILLARS, RS_BLINK, RS_CHARGE, RS_DEFEAT, RS_GONE, RS_TALK };
#define RAURU_HP 36
#define MAX_GLYPHS 6

// His spell cards: bullet patterns in the manner of a danmaku shooter. Up to 360 bullets of light at once, moved and
// drawn by Rauru himself (the game can only check about 50 attacking colliders at a time, so one actor each would
// leave most of them harmless). A bullet that reaches Ganondorf has a hit collider put on it for that frame, so it
// strikes the usual way (knockback, his flinch, and his cape guard turns it).
#define MAX_BULLETS 360
#define BULLET_HIT_RADIUS 20.0f // across, from his middle
#define BULLET_ROOM 1290.0f      // gone past this far from the room's middle
enum { PAT_HALO, PAT_SPIRAL, PAT_JUDGEMENT, PAT_CLOSING, PAT_BLOOM, PAT_COUNT };
enum { BC_GOLD, BC_WHITE, BC_ROSE, BC_SKY };

typedef struct {
    f32 x, z;
    f32 speed, accel, maxSpeed;
    s16 yaw, turn, life;
    u8 color, active;
    u16 epoch; // (a new bullet in this slot: the frame interpolation starts it afresh, not from the last one)
    f32 size;
} GdfBullet;

typedef struct {
    Actor actor;
    SkelAnime skel;
    ColliderCylinder body;
    ColliderCylinder pillar[MAX_GLYPHS];
    ColliderCylinder burst;
    s16 state;
    s16 timer;
    s16 iframes;
    s16 hitsSinceBlink;
    s16 cycle;
    s16 chargeHits;
    s16 frames;
    Vec3f glyph[MAX_GLYPHS];
    s16 glyphCount;
    s16 glyphTimer; // > 0: flaring (counting down to the fall); < 0: the pillars standing
    f32 visible;
    Vec3f blinkTo;
    GdfBullet bullet[MAX_BULLETS];
    s16 pattern;
    s16 lastPattern;
    s16 spin;
    s16 gapYaw;
    f32 bulletY; // they fly at his body's height, wherever he floats
    ColliderCylinder proxy;
} GdfRauru;

static void Rauru_Emit(GdfRauru* r, f32 x, f32 z, s16 yaw, f32 speed, f32 accel, f32 maxSpeed, s16 turn, u8 color,
                       f32 size) {
    for (s32 i = 0; i < MAX_BULLETS; i++) {
        GdfBullet* b = &r->bullet[i];
        if (!b->active) {
            b->x = x;
            b->z = z;
            b->yaw = yaw;
            b->speed = speed;
            b->accel = accel;
            b->maxSpeed = maxSpeed;
            b->turn = turn;
            b->life = 300;
            b->color = color;
            b->size = size;
            b->active = true;
            b->epoch++;
            return;
        }
    }
}

// A ring of n about (x, z), turned by `offset`
static void Rauru_Ring(GdfRauru* r, f32 x, f32 z, s32 n, s16 offset, f32 speed, f32 accel, f32 maxSpeed, s16 turn,
                       u8 color) {
    for (s32 i = 0; i < n; i++) {
        Rauru_Emit(r, x, z, offset + (s16)(i * 0x10000 / n), speed, accel, maxSpeed, turn, color, 0.42f);
    }
}

static void Rauru_ClearBullets(GdfRauru* r, PlayState* play) {
    s32 n = 0;
    for (s32 i = 0; i < MAX_BULLETS; i++) {
        if (r->bullet[i].active) {
            if ((n++ % 6) == 0) {
                Vec3f p = { r->bullet[i].x, r->bulletY, r->bullet[i].z };
                Sparks(play, &p, 10.0f, 1, 0.5f, -1);
            }
            r->bullet[i].active = false;
        }
    }
}

// One frame of the spell card under way (t: frames since it began); returns true when it's done
static bool Rauru_Spell(GdfRauru* r, PlayState* play, s16 t, s16 yawTo, bool pressed) {
    Player* player = GET_PLAYER(play);
    f32 ox = r->actor.world.pos.x;
    f32 oz = r->actor.world.pos.z;

    switch (r->pattern) {
        case PAT_HALO: // rings, each turned against the last, speeding up as they spread
            if ((t % (pressed ? 7 : 9) == 0) && (t <= 100)) {
                s16 off = ((t / 9) % 2) ? (s16)(t * 0x1C0) : (s16)(-t * 0x1C0);
                Rauru_Ring(r, ox, oz, pressed ? 30 : 22, off, 2.2f, 0.06f, 5.5f, 0, (t / 9) % 2 ? BC_GOLD : BC_WHITE);
                Audio_PlayActorSound2(&r->actor, NA_SE_EN_FANTOM_SPARK);
            }
            return t > 130;

        case PAT_SPIRAL: // arms of light wheeling out from him (two wheels the other way round when pressed)
            if ((t % 2 == 0) && (t <= 130)) {
                s32 arms = pressed ? 6 : 5;
                r->spin += 0x0780;
                for (s32 a = 0; a < arms; a++) {
                    Rauru_Emit(r, ox, oz, r->spin + (s16)(a * 0x10000 / arms), 3.2f, 0.0f, 3.2f, 0, BC_GOLD, 0.38f);
                    if (pressed && (t % 4 == 0)) {
                        Rauru_Emit(r, ox, oz, -r->spin + (s16)(a * 0x10000 / arms), 2.4f, 0.0f, 2.4f, 0, BC_ROSE,
                                   0.38f);
                    }
                }
                if (t % 8 == 0) {
                    Audio_PlayActorSound2(&r->actor, NA_SE_EN_FANTOM_SPARK);
                }
            }
            return t > 160;

        case PAT_JUDGEMENT: // fans aimed at him, fast, over a scatter of slow motes
            if ((t % 14 == 0) && (t <= 112)) {
                s32 n = pressed ? 13 : 9;
                for (s32 i = 0; i < n; i++) {
                    Rauru_Emit(r, ox, oz, yawTo + (s16)((i - n / 2) * 0x0900), 3.0f, 0.18f, 8.0f, 0, BC_WHITE, 0.45f);
                }
                Audio_PlayActorSound2(&r->actor, NA_SE_EN_FANTOM_THUNDER);
            }
            if (t <= 120) {
                for (s32 i = 0; i < (pressed ? 2 : 1); i++) {
                    Rauru_Emit(r, ox, oz, (s16)Rand_ZeroFloat(65535.0f), 1.6f + Rand_ZeroFloat(1.4f), 0.0f, 3.0f, 0,
                               BC_SKY, 0.3f);
                }
            }
            return t > 150;

        case PAT_CLOSING: // rings that close in on him from all round, each with one way out
            if ((t % 32 == 0) && (t <= 128)) {
                s32 n = 40;
                f32 radius = 820.0f;
                f32 px = player->actor.world.pos.x;
                f32 pz = player->actor.world.pos.z;
                r->gapYaw = (s16)Rand_ZeroFloat(65535.0f);
                for (s32 i = 0; i < n; i++) {
                    s16 a = (s16)(i * 0x10000 / n);
                    s16 fromGap = a - r->gapYaw;
                    if (ABS(fromGap) < 0x1400) {
                        continue; // the way out
                    }
                    Rauru_Emit(r, px + Math_SinS(a) * radius, pz + Math_CosS(a) * radius, a + 0x8000, 2.6f, 0.0f, 2.6f,
                               pressed ? 0x50 : 0, BC_SKY, 0.42f);
                }
                Audio_PlayActorSound2(&r->actor, NA_SE_EV_LIGHT_GATHER);
            }
            if (pressed && (t % 20 == 10) && (t <= 140)) {
                Rauru_Ring(r, ox, oz, 16, (s16)(t * 0x300), 2.0f, 0.0f, 2.0f, 0, BC_GOLD);
            }
            return t > 200;

        case PAT_BLOOM: // petals: rings that curve as they go, one way and then the other
        default:
            if ((t % 10 == 0) && (t <= 120)) {
                bool cw = (t / 10) % 2;
                Rauru_Ring(r, ox, oz, pressed ? 16 : 12, (s16)(t * 0x200), 3.4f, 0.0f, 3.4f,
                           cw ? (pressed ? 0x0160 : 0x0120) : (pressed ? -0x0160 : -0x0120), BC_ROSE);
                Audio_PlayActorSound2(&r->actor, NA_SE_EN_FANTOM_SPARK);
            }
            return t > 160;
    }
}

// The bullets fly on, and one that has reached him strikes
static void Rauru_UpdateBullets(GdfRauru* r, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 px = player->actor.world.pos.x;
    f32 pz = player->actor.world.pos.z;
    bool struck = false;

    Math_ApproachF(&r->bulletY, player->actor.world.pos.y + 45.0f, 0.3f, 12.0f);
    for (s32 i = 0; i < MAX_BULLETS; i++) {
        GdfBullet* b = &r->bullet[i];
        if (!b->active) {
            continue;
        }
        b->speed = MIN(b->speed + b->accel, b->maxSpeed);
        b->yaw += b->turn;
        b->x += Math_SinS(b->yaw) * b->speed;
        b->z += Math_CosS(b->yaw) * b->speed;
        if ((--b->life <= 0) || (fabsf(b->x - r->actor.home.pos.x) > BULLET_ROOM) ||
            (fabsf(b->z - r->actor.home.pos.z) > BULLET_ROOM)) {
            b->active = false;
            continue;
        }
        if (!struck && (SQ(b->x - px) + SQ(b->z - pz) < SQ(BULLET_HIT_RADIUS))) {
            // Its own collider for this frame: the strike the usual way
            r->proxy.dim.pos.x = (s16)b->x;
            r->proxy.dim.pos.y = (s16)(r->bulletY - 15.0f);
            r->proxy.dim.pos.z = (s16)b->z;
            CollisionCheck_SetAT(play, &play->colChkCtx, &r->proxy.base);
            if (player->invincibilityTimer <= 0) {
                b->active = false; // spent on him (passing through while he flashes)
            }
            struck = true;
        }
    }
}


static ColliderCylinderInit sRauruBodyInit = {
    { COLTYPE_HIT0, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 40, 110, -20, { 0, 0, 0 } },
};

static ColliderCylinderInit sRauruPillarInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE,
      OCELEM_NONE },
    { 64, 400, -20, { 0, 0, 0 } },
};

static void Rauru_Init(Actor* thisx, PlayState* play) {
    GdfRauru* r = (GdfRauru*)thisx;

    Actor_SetScale(thisx, 0.012f);
    SkelAnime_InitFlex(play, &r->skel, (FlexSkeletonHeader*)sRauruSkel, (AnimationHeader*)sRauruIdle, NULL, NULL, 0);
    Collider_InitCylinder(play, &r->body);
    Collider_SetCylinder(play, &r->body, thisx, &sRauruBodyInit);
    for (s32 i = 0; i < MAX_GLYPHS; i++) {
        Collider_InitCylinder(play, &r->pillar[i]);
        Collider_SetCylinder(play, &r->pillar[i], thisx, &sRauruPillarInit);
    }
    Collider_InitCylinder(play, &r->burst);
    Collider_SetCylinder(play, &r->burst, thisx, &sRauruPillarInit);
    Collider_InitCylinder(play, &r->proxy);
    Collider_SetCylinder(play, &r->proxy, thisx, &sRauruPillarInit);
    r->proxy.dim.radius = 14;
    r->proxy.dim.height = 30;
    r->proxy.dim.yShift = 0;
    r->proxy.info.toucher.damage = 0x08;
    r->bulletY = thisx->home.pos.y + 45.0f;
    r->lastPattern = -1;
    for (s32 i = 0; i < MAX_BULLETS; i++) {
        r->bullet[i].active = false;
    }
    r->burst.dim.radius = 420;
    r->burst.dim.height = 300;
    r->burst.info.toucher.damage = 0x20;
    thisx->colChkInfo.health = RAURU_HP;
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    thisx->targetMode = 5;
    r->visible = 1.0f;
    r->state = RS_WAIT;
}

static void Rauru_Destroy(Actor* thisx, PlayState* play) {
    GdfRauru* r = (GdfRauru*)thisx;
    SkelAnime_Free(&r->skel, play);
    Collider_DestroyCylinder(play, &r->body);
    for (s32 i = 0; i < MAX_GLYPHS; i++) {
        Collider_DestroyCylinder(play, &r->pillar[i]);
    }
    Collider_DestroyCylinder(play, &r->burst);
    Collider_DestroyCylinder(play, &r->proxy);
}

static void Rauru_Cast(GdfRauru* r) {
    Animation_Change(&r->skel, (AnimationHeader*)sRauruCast, 1.0f, 0.0f, Animation_GetLastFrame((void*)sRauruCast),
                     ANIMMODE_ONCE, -6.0f);
}

static void Rauru_Idle(GdfRauru* r) {
    Animation_Change(&r->skel, (AnimationHeader*)sRauruIdle, 1.0f, 0.0f, Animation_GetLastFrame((void*)sRauruIdle),
                     ANIMMODE_LOOP, -8.0f);
}

static bool Rauru_Pressed(GdfRauru* r) {
    return r->actor.colChkInfo.health <= RAURU_HP / 2;
}

static void Rauru_Update(Actor* thisx, PlayState* play) {
    GdfRauru* r = (GdfRauru*)thisx;
    Player* player = GET_PLAYER(play);
    f32 dist = Actor_WorldDistXZToActor(thisx, &player->actor);
    s16 yawTo = Actor_WorldYawTowardActor(thisx, &player->actor);
    bool canHit = (r->state != RS_WAIT) && (r->state != RS_TALK) && (r->state != RS_DEFEAT) && (r->state != RS_GONE) &&
                  (r->visible > 0.9f);

    SkelAnime_Update(&r->skel);
    r->frames++;
    if (r->iframes > 0) {
        r->iframes--;
    }
    // hovering over the floor, bobbing
    if (r->state != RS_DEFEAT && r->state != RS_GONE) {
        thisx->world.pos.y = thisx->home.pos.y + 90.0f + Math_SinS(r->frames * 0x300) * 12.0f;
        Math_SmoothStepToS(&thisx->shape.rot.y, yawTo, 5, 0x600, 0x80);
    }

    // Struck
    if (r->body.base.acFlags & AC_HIT) {
        r->body.base.acFlags &= ~AC_HIT;
        if (canHit && (r->iframes == 0)) {
            thisx->colChkInfo.health--;
            r->iframes = 14;
            r->hitsSinceBlink++;
            Actor_SetColorFilter(thisx, 0x4000, 255, 0, 10);
            Audio_PlayActorSound2(thisx, NA_SE_EN_FANTOM_DAMAGE);
            if (r->state == RS_CHARGE) {
                r->chargeHits++;
            }
            if (thisx->colChkInfo.health <= 0) {
                Rauru_ClearBullets(r, play);
                r->state = RS_DEFEAT;
                r->timer = 0;
                r->glyphCount = 0;
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS_CLEAR);
                Audio_PlayActorSound2(thisx, NA_SE_EN_FANTOM_DEAD);
            }
        }
    }

    switch (r->state) {
        case RS_WAIT:
            if ((dist < 1100.0f) && CanLeave(play)) {
                // Their words first: he is held where he stands (as in a cutscene) until the last box closes, and
                // only then does the fight begin
                r->state = RS_TALK;
                r->timer = 0;
                Player_SetCsActionWithHaltedActors(play, thisx, 8);
                Say(play, "%yRauru:%w So. The King of the Gerudo walks into the heart of the Sacred Realm, and its "
                          "doors did not stop him.^%rGanondorf:%w Doors, guards, prayers. Hyrule's whole "
                          "defence is hoping, old man. It isn't working.^%yRauru:%w I raised this temple so that no hand like "
                          "yours would ever close around the Triforce. I have waited an age for the one who would "
                          "try.^%rGanondorf:%w An age, waiting. Old man, you've "
                          "been talking to a triangle for a thousand years. Move.^%yRauru:%w The Triforce mirrors the heart that touches it. "
                          "Yours would break the world.^%rGanondorf:%w Your gods made a world where one "
                          "people gets rivers and the other gets sand, and you call ME the one who breaks things. Spare me.^%yRauru:%w "
                          "Then there is nothing left to say. Let the light judge you, Ganondorf!");
            }
            break;

        case RS_TALK:
            r->timer++;
            if (((r->timer > 10) && (play->msgCtx.msgMode == MSGMODE_NONE)) || (r->timer > 2400)) {
                Player_SetCsAction(play, NULL, 7); // (his own again)
                r->state = RS_HOVER;
                r->timer = 40;
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS);
            }
            break;

        case RS_HOVER:
            if (--r->timer <= 0) {
                r->cycle++;
                if (r->hitsSinceBlink >= 4) {
                    r->state = RS_BLINK;
                } else if (Rauru_Pressed(r) && (r->cycle % 4 == 0)) {
                    r->state = RS_CHARGE;
                    r->chargeHits = 0;
                } else {
                    r->state = (r->cycle % 2) ? RS_BOLTS : RS_PILLARS;
                }
                r->timer = 0;
                if (r->state == RS_BOLTS) {
                    // A spell card he hasn't just used
                    do {
                        r->pattern = (s16)Rand_ZeroFloat((f32)PAT_COUNT - 0.001f);
                    } while (r->pattern == r->lastPattern);
                    r->lastPattern = r->pattern;
                }
                if (r->state != RS_BLINK) {
                    Rauru_Cast(r);
                }
            }
            break;

        case RS_BOLTS:
            // A spell card
            if (Rauru_Spell(r, play, r->timer++, yawTo, Rauru_Pressed(r))) {
                r->state = RS_HOVER;
                r->timer = Rauru_Pressed(r) ? 24 : 36;
                Rauru_Idle(r);
            }
            break;

        case RS_PILLARS:
            if (r->timer++ == 12) {
                // glyphs flare: one under him, the rest about him
                r->glyphCount = Rauru_Pressed(r) ? 6 : 4;
                for (s32 i = 0; i < r->glyphCount; i++) {
                    Vec3f g = player->actor.world.pos;
                    if (i > 0) {
                        g.x += Rand_CenteredFloat(700.0f);
                        g.z += Rand_CenteredFloat(700.0f);
                    }
                    g.y = thisx->home.pos.y;
                    r->glyph[i] = g;
                }
                r->glyphTimer = 45;
                Audio_PlayActorSound2(thisx, NA_SE_EV_LIGHT_GATHER);
            }
            // Pressed: slow rings as the light gathers
            if (Rauru_Pressed(r) && (r->timer % 12 == 0)) {
                Rauru_Ring(r, thisx->world.pos.x, thisx->world.pos.z, 18, (s16)(r->timer * 0x400), 2.0f, 0.0f, 2.0f,
                           0, BC_WHITE);
            }
            if (r->timer > 30) {
                r->state = RS_HOVER;
                r->timer = 50;
                Rauru_Idle(r);
            }
            break;

        case RS_BLINK:
            r->timer++;
            if (r->timer == 1) {
                f32 a = Rand_ZeroFloat(2.0f * M_PI);
                r->blinkTo = thisx->home.pos;
                r->blinkTo.x += cosf(a) * 650.0f;
                r->blinkTo.z += sinf(a) * 650.0f;
                Audio_PlayActorSound2(thisx, NA_SE_EN_FANTOM_THUNDER);
                Sparks(play, &thisx->world.pos, 60.0f, 20, 1.0f, 5);
            }
            if (r->timer < 10) {
                r->visible = 1.0f - r->timer / 10.0f;
            } else if (r->timer == 10) {
                thisx->world.pos.x = r->blinkTo.x;
                thisx->world.pos.z = r->blinkTo.z;
                Sparks(play, &r->blinkTo, 60.0f, 20, 1.0f, 5);
            } else if (r->timer < 20) {
                r->visible = (r->timer - 10) / 10.0f;
            } else {
                r->visible = 1.0f;
                r->hitsSinceBlink = 0;
                r->state = RS_HOVER;
                r->timer = 20;
            }
            break;

        case RS_CHARGE:
            r->timer++;
            if (r->timer % 2 == 0) {
                Vec3f c = thisx->world.pos;
                c.y += 50.0f;
                Sparks(play, &c, 120.0f - r->timer, 2, 0.4f, -1);
            }
            if (r->chargeHits >= 3) {
                // struck out of it
                r->state = RS_BLINK;
                r->timer = 0;
                Audio_PlayActorSound2(thisx, NA_SE_EN_FANTOM_DAMAGE);
                break;
            }
            if (r->timer == 90) {
                Vec3f c = thisx->world.pos;
                Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE_FLASH);
                Audio_PlayActorSound2(thisx, NA_SE_EN_GANON_HIT_THUNDER);
                Actor_RequestQuakeAndRumble(thisx, play, 8, 20);
                Sparks(play, &c, 300.0f, 40, 2.0f, -1);
            }
            if ((r->timer >= 90) && (r->timer < 94)) {
                r->burst.dim.pos.x = (s16)thisx->world.pos.x;
                r->burst.dim.pos.y = (s16)thisx->home.pos.y;
                r->burst.dim.pos.z = (s16)thisx->world.pos.z;
                CollisionCheck_SetAT(play, &play->colChkCtx, &r->burst.base);
            }
            if (r->timer > 110) {
                r->state = RS_HOVER;
                r->timer = 40;
                Rauru_Idle(r);
            }
            break;

        case RS_DEFEAT:
            r->timer++;
            Math_StepToF(&thisx->world.pos.y, thisx->home.pos.y + 20.0f, 1.5f);
            if (r->timer % 3 == 0) {
                Vec3f c = thisx->world.pos;
                c.y += 60.0f;
                Sparks(play, &c, 80.0f, 2, 0.8f, -1);
            }
            if (r->timer == 40) {
                Say(play, "%yRauru:%w ...The light... could not hold you.^%rGanondorf:%w Light's just power in a "
                          "white robe, old man. Mine was bigger. That's all righteousness ever is.^%yRauru:%w The Triforce does not judge. "
                          "It answers the hand that reaches for it... but it will not come to you whole. Remember "
                          "that, when the rest is denied you.^%rGanondorf:%w A third. Of the gods' power. "
                          "More than any of your saints ever did with theirs. I'll take the rest.^%yRauru:%w Go, then. The "
                          "Chamber of Sages lies before you. One day seven will rise to answer what you do "
                          "here.^%rGanondorf:%w Seven Sages. Let them rise. I'll be the one who "
                          "decides how long they stay up.");
            }
            if ((r->timer > 50) && CanLeave(play)) {
                TSet(play, FLAG_RAURU_DOWN);
                r->state = RS_GONE;
                sPlainChamber = true;
                gSaveContext.chamberCutsceneNum = GDF_CHAMBER_CS_NONE;
                Warp(play, SAGES_ENTRANCE);
            }
            break;
    }

    // The pillars of light: glyphs flare, then the light falls
    if (r->glyphTimer > 0) {
        if (--r->glyphTimer == 0) {
            r->glyphTimer = -24;
            Audio_PlayActorSound2(thisx, NA_SE_EN_GANON_HIT_THUNDER);
            Actor_RequestQuakeAndRumble(thisx, play, 3, 8);
        }
    } else if (r->glyphTimer < 0) {
        for (s32 i = 0; i < r->glyphCount; i++) {
            r->pillar[i].dim.pos.x = (s16)r->glyph[i].x;
            r->pillar[i].dim.pos.y = (s16)r->glyph[i].y;
            r->pillar[i].dim.pos.z = (s16)r->glyph[i].z;
            CollisionCheck_SetAT(play, &play->colChkCtx, &r->pillar[i].base);
        }
        if (++r->glyphTimer == 0) {
            r->glyphCount = 0;
        }
    }

    if ((r->state != RS_DEFEAT) && (r->state != RS_GONE)) {
        Rauru_UpdateBullets(r, play);
    }

    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 60.0f;
    if (canHit) {
        Collider_UpdateCylinder(thisx, &r->body);
        CollisionCheck_SetAC(play, &play->colChkCtx, &r->body.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &r->body.base);
    }
}

static void Rauru_Draw(Actor* thisx, PlayState* play) {
    GdfRauru* r = (GdfRauru*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if ((r->visible > 0.5f) && (r->state != RS_GONE)) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)SEGMENTED_TO_VIRTUAL(sRauruEyes));
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)SEGMENTED_TO_VIRTUAL(sRauruEyes));
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)&D_80116280[2]);
        SkelAnime_DrawFlexOpa(play, r->skel.skeleton, r->skel.jointTable, r->skel.dListCount, NULL, NULL, thisx);
    }
    // glyphs and pillars of light
    for (s32 i = 0; i < r->glyphCount; i++) {
        Matrix_Translate(r->glyph[i].x, r->glyph[i].y + 1.0f, r->glyph[i].z, MTXMODE_NEW);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        if (r->glyphTimer > 0) {
            u8 k = (u8)(150 + 105 * Math_SinS(r->frames * 0x1800));
            Matrix_RotateY(r->frames * 0.08f, MTXMODE_APPLY);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, k, k, k * 3 / 4, 255);
            DrawObj(play, sGlyphDL, &POLY_OPA_DISP);
        } else {
            // the Sacred Realm's column of light, standing where the glyph flared
            u32 f = play->gameplayFrames & 0xFFFF;

            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, 255, 255, 170, 255);
            gSPSegment(POLY_XLU_DISP++, 8,
                       (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (f * 2) % 512, 512 - (f % 512) - 1, 128, 128,
                                                     1, 512 - ((f * 2) % 512) - 1, 0, 64, 64, 2, -1, -2, 0));
            Matrix_RotateY(r->frames * 0.05f, MTXMODE_APPLY);
            Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 150.0f, 0.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sCrystalLightDL);
        }
    }
    // his bullets of light
    {
        static const Color_RGB8 sBulletColors[] = {
            { 255, 215, 110 }, { 255, 255, 235 }, { 255, 150, 200 }, { 150, 215, 255 } };
        for (s32 i = 0; i < MAX_BULLETS; i++) {
            GdfBullet* b = &r->bullet[i];
            if (!b->active) {
                continue;
            }
            const Color_RGB8* c = &sBulletColors[b->color];
            u8 k = (u8)(215 + 40 * Math_SinS((r->frames + i * 7) * 0x1400));
            // Each bullet its own in the frame interpolation (drawn in a loop, a bullet was blended with whichever
            // was drawn in its place the frame before: they warped about and vanished)
            FrameInterpolation_RecordOpenChild(b, b->epoch);
            DrawEnergyBall(play, b->x, r->bulletY, b->z, b->size * ENERGY_BALL_PER_ORB, c->r, c->g * k / 255,
                           c->b * k / 255, 255);
            FrameInterpolation_RecordCloseChild();
        }
    }
    // the radiant burst gathering
    if ((r->state == RS_CHARGE) && (r->timer < 94)) {
        f32 s = (r->timer < 90) ? 1.0f + r->timer * 0.03f : 12.0f;
        DrawEnergyBall(play, thisx->world.pos.x, thisx->world.pos.y + 60.0f, thisx->world.pos.z,
                       s * ENERGY_BALL_PER_ORB, 255, 230, 120, 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- The Triforce, in the Chamber of Sages ----

typedef struct {
    Actor actor;
    s16 state;
    s16 timer;
    f32 baseY;
} GdfTriforce;

static void Triforce_Init(Actor* thisx, PlayState* play) {
    GdfTriforce* t = (GdfTriforce*)thisx;
    Actor_SetScale(thisx, 0.06f);
    t->baseY = thisx->world.pos.y;
    thisx->world.pos.y += 500.0f;
    Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE_FLASH);
}

static void Triforce_Update(Actor* thisx, PlayState* play) {
    GdfTriforce* t = (GdfTriforce*)thisx;
    Player* player = GET_PLAYER(play);

    t->timer++;
    thisx->shape.rot.y += 0x300;
    Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE - SFX_FLAG);
    switch (t->state) {
        case 0: // coming down
            Math_StepToF(&thisx->world.pos.y, t->baseY, 4.0f);
            if (t->timer % 3 == 0) {
                Sparks(play, &thisx->world.pos, 60.0f, 2, 0.4f, 5);
            }
            if ((thisx->world.pos.y <= t->baseY) && (Actor_WorldDistXZToActor(thisx, &player->actor) < 120.0f) &&
                CanLeave(play)) {
                t->state = 1;
                t->timer = 0;
            }
            break;
        case 1: { // into his raised hands: he holds it up over his head, as Link holds up what he finds
            Vec3f to = player->actor.world.pos;

            to.y += 120.0f;
            BossGanon_Player_HoldUp(30);
            BossGanon_Player_GetHeldUpPos(&to);
            // (he stands still for it)
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            player->linearVelocity = 0.0f;
            Math_StepToF(&thisx->world.pos.x, to.x, 8.0f);
            Math_StepToF(&thisx->world.pos.y, to.y, 8.0f);
            Math_StepToF(&thisx->world.pos.z, to.z, 8.0f);
            Math_StepToF(&thisx->scale.x, 0.012f, 0.0015f);
            thisx->scale.y = thisx->scale.z = thisx->scale.x;
            if (t->timer == 40) {
                TSet(play, FLAG_TRIFORCE);
                Sparks(play, &thisx->world.pos, 120.0f, 50, 2.0f, 5);
                Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE_FLASH);
                func_800F5B58();
                Audio_PlayFanfare(NA_BGM_MEDALLION_GET | 0x900); // (the medallions' fanfare: a sacred thing claimed)
                player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
                Say(play, "You claimed the %yTriforce%w!^Its power answers you as the %rTriforce of Power%w, and it "
                          "will not leave your hand.^Your wounds close and your magic returns by themselves. Nothing "
                          "in Hyrule can stand against the King of Evil now.");
                t->state = 2;
                t->timer = 0;
            }
            break;
        }
        case 2: { // still held up over his head while the words are read
            Vec3f to;

            if (play->msgCtx.msgMode != MSGMODE_NONE) {
                BossGanon_Player_HoldUp(15);
            }
            if (BossGanon_Player_GetHeldUpPos(&to)) {
                thisx->world.pos = to;
            }
            if ((t->timer > 10) && CanLeave(play)) {
                t->state = 3;
                Warp(play, ENTR_TEMPLE_OF_TIME_ENTRANCE);
            }
            break;
        }
    }
}

static void DrawTriforce(PlayState* play, Gfx** disp, u8 alpha) {
    gSPSegment((*disp)++, 8, (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 16, 1, 0, 0, 16, 8, 0, 0, 0, 0));
    gDPSetPrimColor((*disp)++, 128, 128, 255, 255, 160, alpha);
    gDPSetEnvColor((*disp)++, 170, 140, 0, 255);
    gSPMatrix((*disp)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*disp)++, (Gfx*)sTriforceDL);
}

static void Triforce_Draw(Actor* thisx, PlayState* play) {
    GdfTriforce* t = (GdfTriforce*)thisx;
    if (t->state >= 2) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    DrawTriforce(play, &POLY_OPA_DISP, 255);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- His power-up (z_boss_ganon.c draws it on his hand) ----

extern "C" s32 GdfSacred_HasTriforce(void) {
    return TempleAvailable() && ((gSaveContext.sceneFlags[HUB_SCENE].swch >> FLAG_TRIFORCE) & 1);
}

// The Triforce of Power on the back of his left hand (drawn from his hand's limb)
extern "C" void GdfSacred_DrawHandTriforce(PlayState* play) {
    if (!GdfSacred_HasTriforce()) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(500.0f, 250.0f, 0.0f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
    Matrix_Scale(0.45f, 0.45f, 0.45f, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    DrawTriforce(play, &POLY_XLU_DISP, (u8)(170 + 85 * Math_SinS(play->gameplayFrames * 0x500)));
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- The portal in the Chamber of Sages, up to the Goddesses' Spring ----

typedef struct {
    Actor actor;
    s16 frames;
} GdfPortal;

// The game's own warp (a Sages' warp, as in their cutscenes), drawn gold and kept lit (z_door_warp1.c's gold mark)
static void Portal_Init(Actor* thisx, PlayState* play) {
    Audio_PlayActorSound2(thisx, NA_SE_EV_TRIFORCE_FLASH);
    Actor_Spawn(&play->actorCtx, play, ACTOR_DOOR_WARP1, thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, 0, 0,
                0x4744, 2 /* WARP_SAGES */);
}

static void Portal_Update(Actor* thisx, PlayState* play) {
    GdfPortal* p = (GdfPortal*)thisx;
    Player* player = GET_PLAYER(play);

    p->frames++;
    Audio_PlayActorSound2(thisx, NA_SE_EV_LIGHT_GATHER - SFX_FLAG);
    if (p->frames % 4 == 0) {
        Vec3f at = thisx->world.pos;
        at.y += 20.0f;
        Sparks(play, &at, 70.0f, 1, 1.2f, (p->frames / 4) % 3 == 0 ? 2 : 5);
    }
    if ((p->frames > 30) && (Actor_WorldDistXZToActor(thisx, &player->actor) < 50.0f) && CanLeave(play)) {
        Warp(play, SPRING_ENTRANCE);
    }
}

static void Portal_Draw(Actor* thisx, PlayState* play) {
    // (the warp draws itself)
}

// ---- The Golden Goddesses: figures of light round the Triforce's dais, to talk to ----

typedef struct {
    Actor actor;
    s16 frames;
    f32 baseY;
} GdfGoddess;

static const Color_RGB8 sGoddessColors[3] = { { 255, 110, 80 }, { 110, 160, 255 }, { 110, 255, 140 } };
static const s32 sGoddessSparks[3] = { 1, 2, 0 }; // (their colours among the Sages')

#define GODDESS_SCALE 0.5f // (five times the tale's: each stands a head taller than the King of Evil)
#define GODDESS_LIFT (GODDESS_SCALE * 220.0f) // (her light reaches as far below her middle as above it: lifted clear of
                                              // the floor)

static void Goddess_Init(Actor* thisx, PlayState* play) {
    GdfGoddess* g = (GdfGoddess*)thisx;
    g->baseY = thisx->world.pos.y + GODDESS_LIFT;
    g->frames = thisx->params * 40;
    thisx->textId = GODDESS_TEXT_ID + thisx->params;
    thisx->targetMode = 6;
    Actor_SetFocus(thisx, 0.0f);
}

static void Goddess_Update(Actor* thisx, PlayState* play) {
    GdfGoddess* g = (GdfGoddess*)thisx;
    Player* player = GET_PLAYER(play);

    g->frames++;
    thisx->world.pos.y = g->baseY + 8.0f * Math_SinS(g->frames * 0x300);
    Actor_SetFocus(thisx, 0.0f); // (her middle)
    // she turns to him
    Math_SmoothStepToS(&thisx->shape.rot.y, Actor_WorldYawTowardActor(thisx, &player->actor), 8, 0x400, 0x40);
    if (g->frames % 6 == 0) {
        Vec3f at = thisx->world.pos;
        Sparks(play, &at, GODDESS_LIFT, 2, 0.8f, sGoddessSparks[thisx->params]);
    }
    if (!Actor_ProcessTalkRequest(thisx, play)) {
        Actor_OfferTalk(thisx, play, 200.0f);
    }
}

// Each Goddess as the game shows her in the Great Deku Tree's tale of the creation: a golden figure in a flame of her
// own light (Din's crimson, Nayru's blue, Farore's green; the three figures alike), drawn from the game's own model
// (object_god_lgt, from the player's ROM). Standing upright here, turning slowly within her light.
static const char sGoddessBodyDL[] = "__OTR__objects/object_god_lgt/gGoldenGoddessBodyDL";
static const char sGoddessAuraDL[] = "__OTR__objects/object_god_lgt/gGoldenGoddessAuraDL";
static const u8 sGoddessPrim[3][3] = { { 255, 170, 255 }, { 170, 255, 255 }, { 170, 255, 170 } };
static const u8 sGoddessEnv[3][3] = { { 255, 0, 255 }, { 0, 40, 255 }, { 0, 200, 0 } };

static void Goddess_Draw(Actor* thisx, PlayState* play) {
    GdfGoddess* g = (GdfGoddess*)thisx;
    s32 k = thisx->params;
    u32 frames = play->gameplayFrames;
    Vec3f p = thisx->world.pos;

    OPEN_DISPS(play->state.gfxCtx);
    // Her light, standing on end (its flame streams up from her feet)
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (frames * 4) % 512, 0, 128, 64, 1,
                                             (frames * 2) % 256, 512 - ((frames * 70) % 512) - 1, 64, 32, 4, 0, 2,
                                             -70));
    gSPSegment(POLY_XLU_DISP++, 9,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 96, 1, (frames * 10) % 256,
                                             256 - ((frames * 30) % 512) - 1, 8, 32, 0, 0, 10, -30));
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, sGoddessPrim[k][0], sGoddessPrim[k][1], sGoddessPrim[k][2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sGoddessEnv[k][0], sGoddessEnv[k][1], sGoddessEnv[k][2], 255);
    Matrix_Translate(p.x, p.y, p.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(GODDESS_SCALE, GODDESS_SCALE, GODDESS_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGoddessAuraDL);
    // Her golden figure within it, turning slowly
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateZ(g->frames * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -140.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGoddessBodyDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- For the Temple of Light (GanondorfLightTemple.cpp) ----

static void RegisterActorsOnce();

extern "C" Actor* GdfSacred_SpawnGuard(PlayState* play, f32 x, f32 y, f32 z, s16 yaw, s16 params) {
    RegisterActorsOnce();
    return Actor_Spawn(&play->actorCtx, play, sGuardId, x, y, z, 0, yaw, 0, params);
}

extern "C" s32 GdfSacred_GuardId(void) {
    return sGuardId;
}

extern "C" void GdfSacred_Say(PlayState* play, const char* text) {
    Say(play, text);
}

// ---- Registering them ----

static s32 Register(const char* name, const char* desc, s32 category, u32 flags, s32 size, ActorFunc init,
                    ActorFunc destroy, ActorFunc update, ActorFunc draw) {
    ActorDBInit i;
    i.name = name;
    i.desc = desc;
    i.category = category;
    i.flags = flags;
    i.objectId = OBJECT_GAMEPLAY_KEEP;
    i.instanceSize = size;
    i.init = init;
    i.destroy = destroy;
    i.update = update;
    i.draw = draw;
    return ActorDB::Instance->AddEntry(i).entry.id;
}

static void RegisterActors() {
    const u32 always = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    if (sGuardId >= 0) {
        return;
    }
    sGuardId = Register("Gdf_SacredGuard", "Sacred Realm guard (Play as Ganondorf)", ACTORCAT_ENEMY,
                        always | ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE, sizeof(GdfGuard), Guard_Init,
                        Guard_Destroy, Guard_Update, Guard_Draw);
    sShotId = Register("Gdf_SacredShot", "Arrow or bolt of light (Play as Ganondorf)", ACTORCAT_ITEMACTION, always,
                       sizeof(GdfShot), Shot_Init, Shot_Destroy, Shot_Update, Shot_Draw);
    sBrazierId = Register("Gdf_Brazier", "Sacred Realm brazier (Play as Ganondorf)", ACTORCAT_PROP, always,
                          sizeof(GdfBrazier), Brazier_Init, Brazier_Destroy, Brazier_Update, Brazier_Draw);
    sAltarId = Register("Gdf_SealAltar", "Sacred Realm seal altar (Play as Ganondorf)", ACTORCAT_PROP, always,
                        sizeof(GdfAltar), Altar_Init, NULL, Altar_Update, Altar_Draw);
    sGateId = Register("Gdf_RauruGate", "Gate to Rauru (Play as Ganondorf)", ACTORCAT_BG, always, sizeof(GdfGate),
                       Gate_Init, NULL, Gate_Update, Gate_Draw);
    sRauruId = Register("Gdf_RauruBoss", "Rauru, the Sage of Light (Play as Ganondorf)", ACTORCAT_BOSS,
                        always | ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE, sizeof(GdfRauru), Rauru_Init,
                        Rauru_Destroy, Rauru_Update, Rauru_Draw);
    sTriforceId = Register("Gdf_SacredTriforce", "The Triforce (Play as Ganondorf)", ACTORCAT_ITEMACTION, always,
                           sizeof(GdfTriforce), Triforce_Init, NULL, Triforce_Update, Triforce_Draw);
    sPortalId = Register("Gdf_SpringPortal", "Portal to the Goddesses' Spring (Play as Ganondorf)", ACTORCAT_PROP,
                         always, sizeof(GdfPortal), Portal_Init, NULL, Portal_Update, Portal_Draw);
    sGoddessId = Register("Gdf_Goddess", "A Golden Goddess (Play as Ganondorf)", ACTORCAT_NPC,
                          always | ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY, sizeof(GdfGoddess),
                          Goddess_Init, NULL, Goddess_Update, Goddess_Draw);
}

static void RegisterActorsOnce() {
    RegisterActors();
}

// ---- The rooms ----

static s16 sLastScene = -1;
static s16 sFrom = -1;
static bool sPending = false;
static s16 sRegenTimer = 0;

static void ScaleBeams(PlayState* play, f32 scale) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != NULL; a = a->next) {
        if ((a->id == ACTOR_DEMO_KANKYO) && (a->params == 0x0E)) {
            Actor_SetScale(a, scale);
        }
    }
}

static void Spawn(PlayState* play, s32 id, f32 x, f32 y, f32 z, s16 yaw, s16 params) {
    Actor_Spawn(&play->actorCtx, play, id, x, y, z, 0, yaw, 0, params);
}

static void OnSceneInit(int16_t sceneNum) {
    sFrom = sLastScene;
    sPlainChamber = false; // (the scene has loaded: it was used, or isn't wanted any more)
    sLastScene = sceneNum;
    sPending = TempleAvailable() &&
               ((sceneNum == HUB_SCENE) || (sceneNum == EAST_SCENE) || (sceneNum == WEST_SCENE) ||
                (sceneNum == RAURU_SCENE) || (sceneNum == SAGES_SCENE) || (sceneNum == SPRING_SCENE));
    if (sPending) {
        RegisterActors();
    }
}

static void SetUpRoom(PlayState* play, Player* player) {
    switch (play->sceneNum) {
        case HUB_SCENE:
            Spawn(play, sGateId, 0.0f, 0.0f, -HUB_A + 2.0f, 0, 0);
            if (sFrom == EAST_SCENE) {
                PlaceAt(player, HUB_A - 90.0f, 0.0f, 0.0f, -0x4000);
            } else if (sFrom == WEST_SCENE) {
                PlaceAt(player, -HUB_A + 90.0f, 0.0f, 0.0f, 0x4000);
            } else if (sFrom == RAURU_SCENE) {
                PlaceAt(player, 0.0f, 0.0f, -HUB_A + 100.0f, 0);
            }
            break;

        case EAST_SCENE:
            ScaleBeams(play, WING_BEAM);
            for (s32 i = 0; i < 4; i++) {
                Spawn(play, sBrazierId, (i & 1) ? 170.0f : -170.0f, 0.0f, (i & 2) ? -750.0f : -350.0f, 0, 0x10 + i);
            }
            Spawn(play, sAltarId, 0.0f, 28.0f, -1185.0f, 0, 0);
            if (!TFlag(play, FLAG_SEAL_EAST)) {
                Spawn(play, sGuardId, -120.0f, 0.0f, -150.0f, 0, GUARD_SPEAR);
                Spawn(play, sGuardId, 120.0f, 0.0f, -150.0f, 0, GUARD_SPEAR);
                Spawn(play, sGuardId, -150.0f, 0.0f, -550.0f, 0, GUARD_SPEAR);
                Spawn(play, sGuardId, 150.0f, 0.0f, -550.0f, 0, GUARD_SHIELD);
                Spawn(play, sGuardId, 0.0f, 0.0f, -950.0f, 0, GUARD_IRON);
                Spawn(play, sGuardId, -180.0f, 0.0f, -1000.0f, 0, GUARD_SPEAR);
            }
            break;

        case WEST_SCENE:
            ScaleBeams(play, WING_BEAM);
            Spawn(play, sAltarId, 0.0f, 28.0f, -1385.0f, 0, 1);
            if (!TFlag(play, FLAG_SEAL_WEST)) {
                Spawn(play, sGuardId, -110.0f, 0.0f, 120.0f, 0, GUARD_SWORD);
                Spawn(play, sGuardId, 110.0f, 0.0f, 120.0f, 0, GUARD_SWORD);
                Spawn(play, sGuardId, -295.0f, 220.0f, -500.0f, 0x4000, GUARD_BOW);
                Spawn(play, sGuardId, 295.0f, 220.0f, -500.0f, -0x4000, GUARD_BOW);
                Spawn(play, sGuardId, -150.0f, 0.0f, -900.0f, 0, GUARD_SWORD);
                Spawn(play, sGuardId, 0.0f, 0.0f, -1150.0f, 0, GUARD_CAPTAIN | 0x10);
                Spawn(play, sGuardId, 170.0f, 0.0f, -1250.0f, 0, GUARD_IRON | 0x10);
            }
            break;

        case RAURU_SCENE:
            ScaleBeams(play, RAURU_BEAM);
            if (!TFlag(play, FLAG_RAURU_DOWN)) {
                Spawn(play, sRauruId, 0.0f, 16.0f, 0.0f, 0, 0);
            }
            break;

        case SAGES_SCENE:
            // Coming from the temple there's no Sage's cutscene here, and nothing to bring Link down from the blue
            // warp's arrival (he'd float in its crystal for good): down onto the floor, where he lands and stands
            if ((sFrom == RAURU_SCENE) || (sFrom == SPRING_SCENE)) {
                CollisionPoly* poly;
                Vec3f probe = player->actor.world.pos;
                f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);

                player->actor.world.pos.y = (floorY > BGCHECK_Y_MIN) ? floorY : (player->actor.world.pos.y - 800.0f);
                player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
                player->actor.velocity.y = 0.0f;
                // and out of the arrival (it waits for a Sage's cutscene to take him out of it): the game's own "end of
                // cutscene" for Link, back on his feet and in control (down on the floor alone, he hung there still)
                Player_SetCsAction(play, NULL, 7);
            }
            // the way up to the Goddesses' Spring, where the Triforce waits
            if (((sFrom == RAURU_SCENE) || (sFrom == SPRING_SCENE)) && TFlag(play, FLAG_RAURU_DOWN) &&
                !TFlag(play, FLAG_TRIFORCE)) {
                f32 x = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 220.0f;
                f32 z = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 220.0f;
                Spawn(play, sPortalId, x, player->actor.world.pos.y, z, 0, 0);
            }
            break;

        case SPRING_SCENE: {
            static const f32 sSpots[3][2] = { { 0.0f, -400.0f }, { -346.0f, 200.0f }, { 346.0f, 200.0f } };

            ScaleBeams(play, SPRING_BEAM);
            if (!TFlag(play, FLAG_TRIFORCE)) {
                Spawn(play, sTriforceId, 0.0f, 16.0f + 110.0f, 0.0f, 0, 0);
            }
            for (s32 i = 0; i < 3; i++) {
                Spawn(play, sGoddessId, sSpots[i][0], 20.0f, sSpots[i][1], 0, i);
            }
            break;
        }
    }
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;
    Player* player;
    Vec3f p;

    if ((play == NULL) || !TempleAvailable()) {
        return;
    }
    player = GET_PLAYER(play);

    // The Triforce of Power: his wounds close, his magic returns
    if (GdfSacred_HasTriforce() && (play->pauseCtx.state == 0)) {
        if (++sRegenTimer >= 40) {
            sRegenTimer = 0;
            if ((gSaveContext.health > 0) && (gSaveContext.health < gSaveContext.healthCapacity)) {
                Health_ChangeBy(play, 4);
            }
            if (gSaveContext.magicLevel > 0) {
                gSaveContext.magic = MIN(gSaveContext.magic + 4, gSaveContext.magicCapacity);
            }
        }
    }

    if (sPending) {
        sPending = false;
        SetUpRoom(play, player);
        return;
    }
    if (!CanLeave(play)) {
        return;
    }
    p = player->actor.world.pos;
    switch (play->sceneNum) {
        case HUB_SCENE:
            if ((p.x > HUB_A + 110.0f) && (fabsf(p.z) < 110.0f)) {
                Warp(play, EAST_ENTRANCE);
            } else if ((p.x < -HUB_A - 110.0f) && (fabsf(p.z) < 110.0f)) {
                Warp(play, WEST_ENTRANCE);
            } else if ((p.z < -HUB_A - 110.0f) && (fabsf(p.x) < 160.0f) && GateOpen(play)) {
                Warp(play, RAURU_ENTRANCE);
            }
            break;
        case EAST_SCENE:
            if (p.z > 700.0f) {
                Warp(play, HUB_ENTRANCE);
            }
            break;
        case WEST_SCENE:
            if (p.z > 700.0f) {
                Warp(play, HUB_ENTRANCE);
            } else if (p.y < -150.0f) { // down into the chasm: hurt, and put back at its near edge
                Health_ChangeBy(play, -8);
                Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
                PlaceAt(player, 0.0f, 0.0f, -260.0f, -0x8000);
                player->actor.velocity.y = 0.0f;
            }
            break;
        case SPRING_SCENE:
            if (p.z > SPRING_END + 100.0f) { // back down to the chamber
                sPlainChamber = true;
                gSaveContext.chamberCutsceneNum = GDF_CHAMBER_CS_NONE;
                Warp(play, SAGES_ENTRANCE);
            } else if (p.y < SPRING_LOW - 300.0f) {
                PlaceAt(player, 0.0f, SPRING_LOW, SPRING_END - 80.0f, -0x8000);
                player->actor.velocity.y = 0.0f;
            }
            break;
        case RAURU_SCENE:
            if (p.z > RAURU_A + 100.0f) {
                GdfTemple_ArriveFromRauru(); // (back into the Temple of Light's antechamber)
                Warp(play, ENTR_SYOTES_0);
            } else if (TFlag(play, FLAG_RAURU_DOWN) && !TFlag(play, FLAG_TRIFORCE) &&
                       (sqrtf(SQ(p.x) + SQ(p.z)) < 80.0f)) {
                sPlainChamber = true;
                gSaveContext.chamberCutsceneNum = GDF_CHAMBER_CS_NONE;
                Warp(play, SAGES_ENTRANCE); // (back for the Triforce, if he left before claiming it)
            }
            break;
    }
}

static void RegisterSacredTemple() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
}

static RegisterShipInitFunc initFunc(RegisterSacredTemple, {});
