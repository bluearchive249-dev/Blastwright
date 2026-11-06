#include "z_boss_ganon.h"
#include "overlays/ovl_Boss_Ganon/ovl_Boss_Ganon.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"
#include "overlays/actors/ovl_En_Zl3/z_en_zl3.h"
#include "overlays/actors/ovl_Bg_Ganon_Otyuka/z_bg_ganon_otyuka.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "textures/boss_title_cards/object_ganon.h"
#include "assets/objects/object_ganon/object_ganon.h"
#include "assets/objects/object_ganon_anime1/object_ganon_anime1.h"
#include "assets/objects/object_ganon_anime2/object_ganon_anime2.h"
#include "assets/objects/object_gndd/object_gndd.h"
#include "assets/objects/object_gnd/object_gnd.h" // SoH: Phantom Ganon skin for "Play as Ganondorf"
#include "soh/Enhancements/ExtraModes/GanondorfSword.h"
#include "soh/Enhancements/ExtraModes/GanondorfTrident.h"
#include "soh/Enhancements/ExtraModes/PlayAsGanondorf.h"
#include "soh/Enhancements/ExtraModes/GanondorfSongs.h"
#include "soh/Enhancements/ExtraModes/GanondorfKills.h"
#include "soh/Enhancements/ExtraModes/GanondorfIntro.h"
#include "soh/Enhancements/ExtraModes/GanondorfInvasion.h"
#include "soh/Enhancements/ExtraModes/GanondorfDance.h"
#include "textures/icon_item_field_static/icon_item_field_static.h" // SoH: the map on the intro's war table
#include "overlays/actors/ovl_En_Horse_Ganon/z_en_horse_ganon.h" // SoH: the Gerudo intro's horse
#include "overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "assets/scenes/dungeons/ganon_boss/ganon_boss_scene.h"
#include "overlays/ovl_En_Ganon_Organ/ovl_En_Ganon_Organ.h" // SoH: his organ, for "Play as Ganondorf"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h" // SoH: riding his horse
#include "overlays/actors/ovl_En_Heishi1/z_en_heishi1.h" // SoH: guards hunting him after a murder
#include "overlays/actors/ovl_En_Door/z_en_door.h" // SoH: he kicks doors open
#include "overlays/actors/ovl_Door_Shutter/z_door_shutter.h" // SoH: ...locked, barred and chained ones too
#include "overlays/actors/ovl_Bg_Spot02_Objects/z_bg_spot02_objects.h" // ...and the Shadow Temple's stone door
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "overlays/actors/ovl_En_Box/z_en_box.h"   // SoH: ...and chests
#include "overlays/actors/ovl_En_GeldB/z_en_geldb.h" // SoH: his Gerudo bodyguards
#include "overlays/effects/ovl_Effect_Ss_Fhg_Flash/z_eff_ss_fhg_flash.h" // SoH: shocks on enemies in his dark aura
#include "objects/gameplay_keep/gameplay_keep.h" // SoH: the torch flame for his crawlspace form
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "soh/ResourceManagerHelpers.h"

#include "soh/frame_interpolation.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/savestate_serialize.h"
#include <libultraship/bridge/resourcebridge.h>

// SoH: "Play as Ganondorf", Invasion mode: every sound the player's Ganondorf (or his light balls) makes is also noted
// for the other players, who hear it from his puppet (see GanondorfInvasion.cpp)
s32 PlayAsGanondorf_KeepItems(void);

static void BossGanon_PlayActorSfx(Actor* actor, u16 sfxId) {
    Audio_PlayActorSound2(actor, sfxId);
    if ((actor != NULL) && (actor->id == ACTOR_BOSS_GANON) && BossGanon_IsPlayerAttack(actor)) {
        GdfInvasion_NoteSfx(sfxId);
    }
}
#define Audio_PlayActorSound2 BossGanon_PlayActorSfx

#include <string.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define FLAGS                                                                                 \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)

void BossGanon_Init(Actor* thisx, PlayState* play);
void BossGanon_Destroy(Actor* thisx, PlayState* play);
void BossGanon_Update(Actor* thisx, PlayState* play);
void BossGanon_Draw(Actor* thisx, PlayState* play);
void func_808E1EB4(Actor* thisx, PlayState* play); // update
void func_808E2544(Actor* thisx, PlayState* play); // update
void BossGanon_LightBall_Update(Actor* thisx, PlayState* play);
void func_808E229C(Actor* thisx, PlayState* play); // draw
void func_808E324C(Actor* thisx, PlayState* play); // draw
void BossGanon_LightBall_Draw(Actor* thisx, PlayState* play);
void BossGanon_Reset(void);

void BossGanon_SetupIntroCutscene(BossGanon* this, PlayState* play);
void BossGanon_SetupTowerCutscene(BossGanon* this, PlayState* play);
void BossGanon_IntroCutscene(BossGanon* this, PlayState* play);
void BossGanon_DeathAndTowerCutscene(BossGanon* this, PlayState* play);
void BossGanon_Wait(BossGanon* this, PlayState* play);
void BossGanon_ChargeLightBall(BossGanon* this, PlayState* play);
void BossGanon_PlayTennis(BossGanon* this, PlayState* play);
void BossGanon_PoundFloor(BossGanon* this, PlayState* play);
void BossGanon_ChargeBigMagic(BossGanon* this, PlayState* play);
void BossGanon_Block(BossGanon* this, PlayState* play);
void BossGanon_HitByLightBall(BossGanon* this, PlayState* play);
void BossGanon_Vulnerable(BossGanon* this, PlayState* play);
void BossGanon_Damaged(BossGanon* this, PlayState* play);

void BossGanon_SetupWait(BossGanon* this, PlayState* play);
void BossGanon_SetupChargeLightBall(BossGanon* this, PlayState* play);
void BossGanon_SetupPlayTennis(BossGanon* this, PlayState* play);

void BossGanon_DrawEffects(PlayState* play);
void BossGanon_UpdateEffects(PlayState* play);

s32 BossGanon_CheckFallingPlatforms(BossGanon* this, PlayState* play, Vec3f* checkPos);

// SoH: "Play as Ganondorf"
// (also another player's Ganondorf in Invasion mode, drawn the same way: his puppet only takes its pose from the network)
#define GDF_IS_PLAYER(dorf) (((dorf)->actor.params == GDF_PARAMS_PLAYER) || ((dorf)->actor.params == GDF_PARAMS_PUPPET))
void BossGanon_Puppet_Init(BossGanon* this, PlayState* play);
void BossGanon_Puppet_Update(Actor* thisx, PlayState* play);
void BossGanon_PuppetBall_Init(BossGanon* this, PlayState* play);
static struct BossGanon* sPuppetBalls[GDF_NET_BALLS]; // Invasion mode: his projectiles on the others' screens
void BossGanon_LightBall_Draw(Actor* thisx, PlayState* play);
void func_808E324C(Actor* thisx, PlayState* play);
// The arena floor is at y = 0. Outside the arena, use the floor under Link.
#define GDF_FLOOR_Y(dorf) (GDF_IS_PLAYER(dorf) ? (dorf)->playerFloorY : 0.0f)
// How far below the floor his organ starts and goes back to
#define GDF_ORGAN_SINK_DEPTH 450.0f
void BossGanon_Player_Init(BossGanon* this, PlayState* play);
void BossGanon_PlayerProjectile_Init(BossGanon* this, PlayState* play);
void BossGanon_Player_StopOrganMusic(BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawOrgan(BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawDarkAura(BossGanon* this, PlayState* play);
static void BossGanon_Player_ReleaseOrganCam(BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawCrawlFlame(BossGanon* this, PlayState* play);
static void BossGanon_Init_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Update_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Draw_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Player_DrawSword(struct BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawPhantom(BossGanon* this, PlayState* play, MtxF* base);
static void BossGanon_Player_ApplyDoorKick(BossGanon* this, PlayState* play);
static void BossGanon_Player_ApplyBreakKick(BossGanon* this, PlayState* play);
static s16 sGdfBreakTimer = 0; // kicking in a locked door, bars or a grate
static Actor* sGdfBreakTarget = NULL;
static void BossGanon_Player_ApplyDoorDuck(BossGanon* this, PlayState* play);
static void BossGanon_Player_UpdateBodyguards(BossGanon* this, PlayState* play);
static void BossGanon_Player_ApplyIntroCs(BossGanon* this, PlayState* play);
void BossGanon_Player_DrawIntroProps(BossGanon* this, PlayState* play);
static void BossGanon_Player_ApplyRunArms(BossGanon* this, f32 weight);
static void BossGanon_Player_ApplyRunLegs(BossGanon* this, f32 weight);
static u8 sGdfPgCapture = false;
static MtxF sGdfLimbMtx[25];
static void BossGanon_LightBall_Update_Impl(Actor* thisx, PlayState* play);
static void func_808E1EB4_Impl(Actor* thisx, PlayState* play);
static void func_808E2544_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Player_Update_Impl(Actor* thisx, PlayState* play);
static void BossGanon_PlayerLightBall_Update_Impl(Actor* thisx, PlayState* play);
static void BossGanon_FireballFlames(PlayState* play, Vec3f* pos, s16 frame);
static void BossGanon_PlayerBigMagic_Update_Impl(Actor* thisx, PlayState* play);
static ActorFunc sPlayerCapeDraw = NULL;
// Phantom Ganon's energy ball uses the custom lens flare too, so only clear it if Ganondorf turned it on
static u8 sPlayerLensFlareOn = false;

const ActorInit Boss_Ganon_InitVars = {
    ACTOR_BOSS_GANON,
    ACTORCAT_BOSS,
    FLAGS,
    OBJECT_GANON,
    sizeof(BossGanon),
    (ActorFunc)BossGanon_Init,
    (ActorFunc)BossGanon_Destroy,
    (ActorFunc)BossGanon_Update,
    (ActorFunc)BossGanon_Draw,
    (ActorResetFunc)BossGanon_Reset,
};

static ColliderCylinderInit sDorfCylinderInit = {
    {
        COLTYPE_HIT3,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x10 },
        { 0xFFCFFFFE, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_ON,
    },
    { 20, 80, -50, { 0, 0, 0 } },
};

static ColliderCylinderInit sLightBallCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK6,
        { 0x00100700, 0x00, 0x08 },
        { 0x0D900740, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON,
    },
    { 20, 30, -15, { 0, 0, 0 } },
};

static u8 D_808E4C58[] = { 0, 12, 10, 12, 14, 16, 12, 14, 16, 12, 14, 16, 12, 14, 16, 10, 16, 14 };
static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static EnGanonMant* sCape;

static s32 sSeed1;
static s32 sSeed3;
static s32 sSeed2;

static BossGanon* sGanondorf;

static EnZl3* sZelda;

static GanondorfEffect sEffects[200];

// SoH: "Play as Ganondorf". The player-controlled Ganondorf can be in the same room as the real one (the final
// fight), so each has its own Ganondorf/cape pointers and effect buffer. sGanondorf, sCape and sCurEffects point at
// whichever one's code is running; the entry points below switch them (see BossGanon_UseContext).
static GanondorfEffect sPlayerEffects[200];
static GanondorfEffect* sCurEffects = sEffects;
static BossGanon* sVanillaGanondorf;
static EnGanonMant* sVanillaCape;
static BossGanon* sPlayerGanondorf;
static EnGanonMant* sPlayerCape;

typedef struct {
    BossGanon* dorf;
    EnGanonMant* cape;
    GanondorfEffect* effects;
} BossGanonContext;

static BossGanonContext BossGanon_SaveContext(void) {
    BossGanonContext ctx = { sGanondorf, sCape, sCurEffects };
    return ctx;
}

static void BossGanon_RestoreContext(BossGanonContext ctx) {
    sGanondorf = ctx.dorf;
    sCape = ctx.cape;
    sCurEffects = ctx.effects;
}

static void BossGanon_UseContext(s32 player) {
    if (player) {
        sGanondorf = sPlayerGanondorf;
        sCape = sPlayerCape;
        sCurEffects = sPlayerEffects;
    } else {
        sGanondorf = sVanillaGanondorf;
        sCape = sVanillaCape;
        sCurEffects = sEffects;
    }
}

#define BOSS_GANON_SHIP_SAVESTATE_FIELDS(F) \
    F(sSeed1)                               \
    F(sSeed2)                               \
    F(sSeed3)                               \
    F(sGanondorf)                           \
    F(sZelda)                               \
    F(sCape)                                \
    F(sEffects)                             \
    F(sPlayerEffects)                       \
    F(sCurEffects)                          \
    F(sVanillaGanondorf)                    \
    F(sVanillaCape)                         \
    F(sPlayerGanondorf)                     \
    F(sPlayerCape)
SHIP_SAVESTATE_DEFINE(BossGanon, BOSS_GANON_SHIP_SAVESTATE_FIELDS)

static u8 sWindowShatterTex[2048] = { { 0 } };

void BossGanonEff_SpawnWindowShard(PlayState* play, Vec3f* pos, Vec3f* velocity, f32 scale) {
    static Color_RGB8 shardColors[] = { { 255, 175, 85 }, { 155, 205, 155 }, { 155, 125, 55 } };
    s16 i;
    GanondorfEffect* eff = sCurEffects;
    Color_RGB8* color;

    for (i = 0; i < 200; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_WINDOW_SHARD;
            eff->pos = *pos;
            eff->velocity = *velocity;
            eff->accel = sZeroVec;
            eff->scale = scale;
            eff->accel.y = -1.5f;
            eff->unk_44 = Rand_ZeroFloat(6.28f);
            eff->unk_48 = Rand_ZeroFloat(6.28f);
            color = &shardColors[(s16)Rand_ZeroFloat(2.99f)];
            eff->color.r = color->r;
            eff->color.g = color->g;
            eff->color.b = color->b;
            eff->timer = (s16)Rand_ZeroFloat(20.0f);
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnSparkle(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, s16 arg6) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_SPARKLE;
            eff->pos = *pos;
            eff->velocity = *velocity;
            eff->accel = *accel;
            eff->scale = scale / 1000.0f;
            eff->unk_2E = (s16)Rand_ZeroFloat(100.0f) + 0xC8;
            eff->unk_30 = arg6;
            eff->timer = (s16)Rand_ZeroFloat(10.0f);
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnLightRay(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, f32 arg5,
                                s16 arg6) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_LIGHT_RAY;
            eff->pos = *pos;
            eff->velocity = *velocity;
            eff->accel = *accel;
            eff->scale = scale / 1000.0f;
            eff->unk_38 = 1.0f;
            eff->unk_40 = arg5;
            eff->unk_2E = (s16)Rand_ZeroFloat(100.0f) + 0xC8;
            eff->unk_30 = arg6;
            eff->timer = (s16)Rand_ZeroFloat(10.0f);
            eff->unk_48 = Math_Atan2F(eff->velocity.z, eff->velocity.x);
            eff->unk_44 = -Math_Atan2F(sqrtf(SQXZ(eff->velocity)), eff->velocity.y);
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnShock(PlayState* play, f32 scale, s16 shockType) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 75; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_SHOCK;
            eff->pos = sZeroVec;
            eff->pos.y = -2000.0f;
            eff->velocity = sZeroVec;
            eff->accel = sZeroVec;
            eff->scale = scale / 1000.0f;
            eff->unk_2E = shockType;
            eff->timer = 0;
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnLightning(PlayState* play, f32 scale, f32 arg2, f32 arg3) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_LIGHTNING;
            eff->velocity = sZeroVec;
            eff->accel = sZeroVec;
            eff->unk_2E = 0;
            eff->scale = scale;
            eff->unk_48 = arg2;
            eff->unk_3C = arg3;
            eff->timer = 0;
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnDustDark(PlayState* play, Vec3f* pos, f32 scale, f32 arg3) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_IMPACT_DUST_DARK;
            eff->pos = *pos;
            eff->velocity = sZeroVec;
            eff->accel = sZeroVec;
            eff->scale = scale;
            eff->unk_40 = 1.0f;
            eff->unk_38 = arg3;
            eff->unk_30 = (s16)Rand_ZeroFloat(100.0f);
            eff->unk_2E = eff->timer = eff->alpha = 0;
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnDustLight(PlayState* play, Vec3f* pos, f32 scale, f32 arg3, s16 bufIndex) {
    GanondorfEffect* effArr = sCurEffects;

    effArr[bufIndex].type = GDF_EFF_IMPACT_DUST_LIGHT;
    effArr[bufIndex].pos = *pos;
    effArr[bufIndex].velocity = sZeroVec;
    effArr[bufIndex].accel = sZeroVec;
    effArr[bufIndex].unk_40 = 1.0f;
    effArr[bufIndex].scale = scale;
    effArr[bufIndex].unk_38 = arg3;
    effArr[bufIndex].unk_30 = Rand_ZeroFloat(100.0f);
    effArr[bufIndex].unk_2E = effArr[bufIndex].timer = effArr[bufIndex].alpha = 0;
    effArr[bufIndex].epoch++;
}

void BossGanonEff_SpawnShockwave(PlayState* play, Vec3f* pos, f32 scale, f32 arg3) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_SHOCKWAVE;
            eff->pos = *pos;
            eff->velocity = sZeroVec;
            eff->accel = sZeroVec;
            eff->alpha = 255;
            eff->unk_40 = 0.6f;
            eff->scale = scale;
            eff->unk_38 = arg3;
            eff->unk_30 = (s16)Rand_ZeroFloat(100.0f);
            eff->unk_2E = eff->timer = 0;
            eff->epoch++;
            break;
        }
    }
}

void BossGanonEff_SpawnBlackDot(PlayState* play, Vec3f* pos, f32 scale) {
    s16 i;
    GanondorfEffect* eff = sCurEffects;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_NONE) {
            eff->type = GDF_EFF_BLACK_DOT;
            eff->pos = *pos;
            eff->velocity = sZeroVec;
            eff->accel = sZeroVec;
            eff->unk_38 = 0.0f;
            eff->scale = scale / 1000.0f;
            eff->timer = 0;
            eff->alpha = 0;
            eff->unk_2E = 0;
            eff->epoch++;
            break;
        }
    }
}

void BossGanon_SetColliderPos(Vec3f* pos, ColliderCylinder* collider) {
    collider->dim.pos.x = pos->x;
    collider->dim.pos.y = pos->y;
    collider->dim.pos.z = pos->z;
}

void BossGanon_SetAnimationObject(BossGanon* this, PlayState* play, s32 objectId) {
    this->animBankIndex = Object_GetIndex(&play->objectCtx, objectId);
    // SoH: the animation objects aren't loaded when Ganondorf is player-controlled outside his scene
    if (this->animBankIndex < 0) {
        this->animBankIndex = 0;
        return;
    }
    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animBankIndex].segment);
}

static InitChainEntry sInitChain[] = {
    ICHAIN_U8(targetMode, 5, ICHAIN_CONTINUE),
    ICHAIN_S8(naviEnemyId, 0x3D, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, 0, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 0, ICHAIN_STOP),
};

static void BossGanon_Init_Impl(Actor* thisx, PlayState* play2) {
    s16 i;
    PlayState* play = play2;
    BossGanon* this = (BossGanon*)thisx;
    s32 cond;
    f32 xDistFromPlayer;
    f32 yDistFromPlayer;
    f32 zDistFromPlayer;
    Player* player = GET_PLAYER(play);

    // SoH: "Play as Ganondorf"
    if (thisx->params == GDF_PARAMS_PLAYER) {
        BossGanon_Player_Init(this, play);
        return;
    }
    if (thisx->params == GDF_PARAMS_PUPPET) {
        BossGanon_Puppet_Init(this, play);
        return;
    }
    if (thisx->params == GDF_PARAMS_PUPPET_BALL) {
        BossGanon_PuppetBall_Init(this, play);
        return;
    }
    if ((thisx->params >= GDF_PARAMS_PLAYER_LIGHT_BALL) &&
        (thisx->params < GDF_PARAMS_PLAYER_BIG_MAGIC + GDF_PLAYER_BIG_MAGIC_COUNT)) {
        BossGanon_PlayerProjectile_Init(this, play);
        return;
    }

    if (thisx->params < 0x64) {
        Flags_SetSwitch(play, 0x14);
        play->specialEffects = sEffects;

        for (i = 0; i < ARRAY_COUNT(sEffects); i++) {
            sEffects[i].type = GDF_EFF_NONE;
        }

        sGanondorf = this;
        thisx->colChkInfo.health = 40;
        Actor_ProcessInitChain(thisx, sInitChain);
        ActorShape_Init(&thisx->shape, 0, NULL, 0);
        Actor_SetScale(thisx, 0.01f);
        SkelAnime_InitFlex(play, &this->skelAnime, &gGanondorfSkel, NULL, NULL, NULL, 0);
        Collider_InitCylinder(play, &this->collider);
        Collider_SetCylinder(play, &this->collider, thisx, &sDorfCylinderInit);

        if (thisx->params != 1) {
            BossGanon_SetupIntroCutscene(this, play);
            this->organAlpha = 255;
        } else {
            cond = Flags_GetSwitch(play, 0x37) &&
                   ((play->sceneNum == SCENE_GANON_BOSS) || (play->sceneNum == SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) ||
                    (play->sceneNum == SCENE_GANONS_TOWER_COLLAPSE_INTERIOR) ||
                    (play->sceneNum == SCENE_INSIDE_GANONS_CASTLE_COLLAPSE));

            if (!cond) {
                BossGanon_SetupTowerCutscene(this, play);
            } else {
                Actor_Kill(thisx);
                return;
            }

            BossGanon_SetupTowerCutscene(this, play);
        }

        sCape = (EnGanonMant*)Actor_SpawnAsChild(&play->actorCtx, thisx, play, ACTOR_EN_GANON_MANT, 0.0f, 0.0f, 0.0f, 0,
                                                 0, 0, 1);
        Actor_ChangeCategory(play, &play->actorCtx, thisx, ACTORCAT_BOSS);
    } else {
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        this->fwork[GDF_FWORK_1] = 255.0f;

        if (thisx->params >= 0xC8) {
            if (thisx->params == 0x12C) {
                thisx->update = BossGanon_LightBall_Update;
                thisx->draw = BossGanon_LightBall_Draw;
                this->unk_1A8 = 2;
            } else if (thisx->params == 0x190) {
                thisx->update = BossGanon_LightBall_Update;
                thisx->draw = BossGanon_LightBall_Draw;
                this->unk_1A8 = 1;
            } else if (thisx->params >= 0x104) {
                // big magic light ball thrown
                thisx->update = func_808E2544;
                thisx->draw = func_808E324C;
                this->unk_1C2 = 10;
                this->unk_1A2 = 520 + (-thisx->params * 2);

                for (i = 0; i < 15; i++) {
                    this->unk_2EC[i] = thisx->world.pos;
                }

                this->timers[1] = 3;
                Collider_InitCylinder(play, &this->collider);
                Collider_SetCylinder(play, &this->collider, thisx, &sLightBallCylinderInit);
            } else if (thisx->params >= 0xFA) {
                // big magic light ball charge
                thisx->update = func_808E2544;
                thisx->draw = func_808E324C;
                this->unk_1A2 = Rand_ZeroFloat(10000.0f);

                for (i = 0; i < 15; i++) {
                    this->unk_2EC[i] = thisx->world.pos;
                }

                this->fwork[GDF_FWORK_1] = 0;
            } else {
                thisx->update = func_808E1EB4;
                thisx->draw = func_808E229C;
                thisx->speedXZ = 11.0f;

                if (thisx->params == 0xC8) {
                    this->timers[0] = 7;
                } else {
                    this->timers[0] = (s16)Rand_ZeroFloat(3.0f) + 3;
                }

                for (i = 0; i < 15; i++) {
                    this->unk_2EC[i].y = 5000.0f;
                }
            }
        } else {
            // light ball (anything from 0x64 - 0xC7)
            thisx->update = BossGanon_LightBall_Update;
            thisx->draw = BossGanon_LightBall_Draw;
            thisx->speedXZ = 12.0f;

            xDistFromPlayer = player->actor.world.pos.x - thisx->world.pos.x;
            yDistFromPlayer = (player->actor.world.pos.y + 30.0f) - thisx->world.pos.y;
            zDistFromPlayer = player->actor.world.pos.z - thisx->world.pos.z;

            thisx->world.rot.y = Math_Atan2S(zDistFromPlayer, xDistFromPlayer);
            thisx->world.rot.x = Math_Atan2S(sqrtf(SQ(xDistFromPlayer) + SQ(zDistFromPlayer)), yDistFromPlayer);

            if (Rand_ZeroOne() < 0) {
                thisx->world.rot.y += (s16)Rand_CenteredFloat(5000.0f);
                thisx->world.rot.x += (s16)Rand_CenteredFloat(5000.0f);
            }

            this->timers[1] = 3;
            Collider_InitCylinder(play, &this->collider);
            Collider_SetCylinder(play, &this->collider, thisx, &sLightBallCylinderInit);
        }
    }

    for (int i = 0; i < ARRAY_COUNT(sWindowShatterTex); i++) {
        sWindowShatterTex[i] = 0;
    }

    Gfx_RegisterBlendedTexture(ganon_boss_sceneTex_006C18, sWindowShatterTex, NULL);
    Gfx_RegisterBlendedTexture(ganon_boss_sceneTex_007418, sWindowShatterTex, NULL);
}

void BossGanon_Destroy(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;

    if ((this->actor.params < 0xC8) || (this->actor.params >= 0x104)) {
        Collider_DestroyCylinder(play, &this->collider);
    }

    if (this->actor.params < 0x64) {
        SkelAnime_Free(&this->skelAnime, play);
    }

    // SoH: "Play as Ganondorf"
    if (this->actor.params == GDF_PARAMS_PUPPET) {
        s32 b;

        GdfInvasion_PuppetDestroyed(&this->actor);
        BossGanon_Player_StopOrganMusic(this, play);
        for (b = 0; b < GDF_NET_BALLS; b++) {
            if ((sPuppetBalls[b] != NULL) && (sPuppetBalls[b]->actor.update != NULL)) {
                Actor_Kill(&sPuppetBalls[b]->actor);
            }
            sPuppetBalls[b] = NULL;
        }
    }
    if (this->actor.params == GDF_PARAMS_PUPPET_BALL) {
        s32 b;

        for (b = 0; b < GDF_NET_BALLS; b++) {
            if (sPuppetBalls[b] == this) {
                sPuppetBalls[b] = NULL;
            }
        }
    }
    if (GDF_IS_PLAYER(this)) {
        // His cape goes with him. It's an actor of its own (spawned as his child), and nothing else removes it: when a
        // remote player's Ganondorf left (or came back into) an area, the cape stayed behind, hanging where he'd been
        // (the area's entrance), and the next one brought a cape of its own.
        for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
            Actor* a = play->actorCtx.actorLists[cat].head;

            for (; a != NULL; a = a->next) {
                if ((a->id == ACTOR_EN_GANON_MANT) && (a->parent == &this->actor)) {
                    a->draw = NULL;
                    Actor_Kill(a);
                    if (&sCape->actor == a) {
                        sCape = NULL;
                    }
                    if (&sPlayerCape->actor == a) {
                        sPlayerCape = NULL;
                    }
                }
            }
        }
        if (this->playerPgReady) {
            // Phantom Ganon's tables are part of the actor: only the skeleton registration goes
            ResourceMgr_UnregisterSkeleton(&this->playerPgSkel);
            this->playerPgReady = false;
        }
        Collider_DestroyQuad(play, &this->playerSwordQuad);
        if ((this->playerSwordBlure >= 0) && (this->playerSwordBlure < TOTAL_EFFECT_COUNT)) {
            Effect_Delete(play, this->playerSwordBlure);
        }
        BossGanon_Player_StopOrganMusic(this, play);
        BossGanon_Player_ReleaseOrganCam(this, play);
        if (this->playerOrganSink < GDF_ORGAN_SINK_DEPTH) {
            Audio_StopSfxById(NA_SE_EV_ELEVATOR_MOVE3 - SFX_FLAG);
        }

        if (sPlayerGanondorf == this) {
            if (sPlayerLensFlareOn) {
                gCustomLensFlareOn = false;
                sPlayerLensFlareOn = false;
            }
            if (sGanondorf == this) {
                sGanondorf = NULL;
                sCape = NULL;
                sCurEffects = sEffects;
            }
            sPlayerGanondorf = NULL;
            sPlayerCape = NULL;
        }
    }
}

void BossGanon_SetupIntroCutscene(BossGanon* this, PlayState* play) {
    s32 pad;
    s32 animBankIndex = Object_GetIndex(&play->objectCtx, OBJECT_GANON_ANIME2);

    if (animBankIndex < 0) {
        Actor_Kill(&this->actor);
        return;
    }

    if (Object_IsLoaded(&play->objectCtx, animBankIndex)) {
        this->actionFunc = BossGanon_IntroCutscene;
        this->unk_198 = 1;
        this->animBankIndex = animBankIndex;
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[animBankIndex].segment);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfPlayOrganAnim, 0.0f);
    } else {
        this->actionFunc = BossGanon_SetupIntroCutscene;
    }
}

typedef struct {
    /* 0x00 */ Vec3s eye;
    /* 0x06 */ Vec3s at;
} CutsceneCameraPosition; // size = 0x12

static CutsceneCameraPosition sIntroCsCameraPositions[] = {
    { { 0, 40, 0 }, { 0, 50, 430 } },
    { { -20, 30, 400 }, { 10, 55, 440 } },
    { { 0, 60, 300 }, { 0, 273, -150 } },
    { { 0, 180, -260 }, { 0, 155, -300 } },
    { { -30, 60, 440 }, { 20, 25, 390 } },
    { { -50, 140, -360 }, { 50, 92, -390 } },
    { { -10, 264, -121 }, { 5, 266, -160 } },
    { { -13, 200, -310 }, { 0, 125, -410 } },
    { { 0, 40, -50 }, { 0, 35, 230 } },
    { { 0, 140, -250 }, { 0, 115, -570 } },
    { { -410, 150, -130 }, { 50, 155, -170 } },
    { { 0, 130, -230 }, { 0, 125, -2000 } },
    { { -2, 147, -293 }, { -200, 345, -2000 } },
};

void BossGanon_SetIntroCsCamera(BossGanon* this, u8 camPosIndex) {
    CutsceneCameraPosition* camPos = &sIntroCsCameraPositions[camPosIndex];

    this->csCamEye.x = camPos->eye.x;
    this->csCamEye.y = camPos->eye.y;
    this->csCamEye.z = camPos->eye.z;

    this->csCamAt.x = camPos->at.x;
    this->csCamAt.y = camPos->at.y;
    this->csCamAt.z = camPos->at.z;
}

// SoH: "Play as Ganondorf" title screen. Instead of the Hyrule Field title cutscene, the game opens in his
// chamber at the top of the tower: he plays Ganondorf's Theme on the organ forever, seen from behind, with the
// logo and "Press Start" on top. Link is there too, hidden and frozen at the door.
// The camera sits behind and above him and drifts slowly from side to side.
#define GDF_TITLE_CAM_EYE_Y 225.0f
#define GDF_TITLE_CAM_EYE_Z -95.0f
#define GDF_TITLE_CAM_AT_Y 150.0f
#define GDF_TITLE_CAM_AT_Z -470.0f

static void BossGanon_TitleScreen(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 drift;

    if (this->csState == 0) {
        this->actor.world.pos.x = 0.0f;
        this->actor.world.pos.y = 112.0f;
        this->actor.world.pos.z = -333.0f;
        this->actor.shape.yOffset = -7000.0f;
        this->actor.shape.rot.y = 0;
        this->useOpenHand = true;

        this->csCamIndex = Play_CreateSubCamera(play);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->csCamIndex, CAM_STAT_ACTIVE);
        this->csCamFov = 60.0f;

        Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_GANON_ORGAN, 0.0f, 0.0f, 0.0f, 0, 0, 0, 1);
        sCape->minY = 57.0f;

        // The logo and "Press Start". It starts the file select by itself.
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_MAG, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);

        this->csState = 1;
        this->csTimer = 0;
    }

    // Ganondorf's Theme loops on its own; start it if the scene didn't
    if ((this->csTimer == 1) && ((func_800FA0B4(SEQ_PLAYER_BGM_MAIN) & 0xFF) != NA_BGM_GANON_TOWER)) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_GANON_TOWER);
    }

    if (player != NULL) {
        player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
    }

    // The dark, stormy lighting from his intro
    this->envLightMode = 3;

    drift = Math_SinS((s16)(this->csTimer * 0x30));
    this->csCamEye.x = drift * 30.0f;
    this->csCamEye.y = GDF_TITLE_CAM_EYE_Y;
    this->csCamEye.z = GDF_TITLE_CAM_EYE_Z;
    this->csCamAt.x = drift * 10.0f;
    this->csCamAt.y = GDF_TITLE_CAM_AT_Y;
    this->csCamAt.z = GDF_TITLE_CAM_AT_Z;

    if (this->csCamIndex != 0) {
        Play_CameraSetAtEye(play, this->csCamIndex, &this->csCamAt, &this->csCamEye);
        Play_CameraSetFov(play, this->csCamIndex, this->csCamFov);
    }
}

void BossGanon_IntroCutscene(BossGanon* this, PlayState* play) {
    u8 moveCam = false;
    Player* player = GET_PLAYER(play);
    s32 pad;
    f32 sin;
    f32 cos;
    Camera* mainCam;

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animBankIndex].segment);

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;

    this->csTimer++;

    SkelAnime_Update(&this->skelAnime);

    // SoH: "Play as Ganondorf" title screen, he keeps playing the organ
    if (PlayAsGanondorf_IsTitleScreen(play)) {
        BossGanon_TitleScreen(this, play);
        return;
    }

    switch (this->csState) {
        case 0:
            player->actor.world.pos.x = 0.0f;
            player->actor.world.pos.y = 0.0f;
            player->actor.world.pos.z = 430.0f;

            this->actor.world.pos.x = 0.0f;
            this->actor.world.pos.y = 112.0f;
            this->actor.world.pos.z = -333.0f;

            this->actor.shape.yOffset = -7000.0f;
            this->actor.shape.rot.y = 0;

            func_80064520(play, &play->csCtx);
            Player_SetCsActionWithHaltedActors(play, &this->actor, 8);
            this->csCamIndex = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, this->csCamIndex, CAM_STAT_ACTIVE);
            this->csCamFov = 60.0f;

            if (Flags_GetEventChkInf(EVENTCHKINF_BEGAN_GANONDORF_BATTLE)) {
                // watched cutscene already, skip most of it
                this->csState = 17;
                this->csTimer = 0;
                player->actor.world.pos.z = 20.0f;
                this->useOpenHand = false;
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandBackwardsAnim, -5.0f);
                this->fwork[GDF_FWORK_1] = 1000.0f;
                BossGanon_SetIntroCsCamera(this, 11);
                this->unk_198 = 2;
                this->timers[2] = 110;
                if (GameInteractor_Should(VB_GANON_HEAL_BEFORE_FIGHT, true)) {
                    gSaveContext.healthAccumulator = MAX_HEALTH;
                }
                Audio_QueueSeqCmd(NA_BGM_STOP);
            } else {
                this->useOpenHand = true;
                BossGanon_SetIntroCsCamera(this, 0);
                this->csState = 1;
                sZelda = (EnZl3*)Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_ZL3, 0.0f, 220.0f,
                                                    -150.0f, 0, 0, 0, 0x2000);
            }

            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_GANON_ORGAN, 0.0f, 0.0f, 0.0f, 0, 0, 0, 1);
            sCape->minY = 57.0f;
            // fallthrough
        case 1:
            this->envLightMode = 3;
            if (this->csTimer == 70) {
                this->csState = 2;
                this->csTimer = 0;
            }
            break;

        case 2:
            BossGanon_SetIntroCsCamera(this, 1);

            if (this->csTimer == 10) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 5);
            }

            if (this->csTimer == 13) {
                Player_PlaySfx(&player->actor, player->ageProperties->unk_92 + NA_SE_VO_LI_SURPRISE);
            }

            if (this->csTimer != 35) {
                break;
            }

            this->csState = 3;
            this->csTimer = 0;

            this->csCamEye.x = 0.0f;
            this->csCamEye.y = 60.0f;
            this->csCamEye.z = 300.0f;

            this->csCamAt.x = 0.0f;
            this->unk_704 = 1.2566371f;
            // fallthrough
        case 3:
            this->envLightMode = 0;
            play->envCtx.unk_D8 = 0.0f;
            this->csCamAt.y = (sinf(this->unk_704) * 300.0f) + this->csCamEye.y;
            this->csCamAt.z = (cosf(this->unk_704) * -300.0f) + this->csCamEye.z;
            Math_ApproachF(&this->unk_704, 0.25f, 0.05f, this->csCamAtMaxStep.y);
            Math_ApproachF(&this->csCamAtMaxStep.y, 0.01f, 1.0f, 0.0001f);

            if (this->csTimer != 200) {
                break;
            }

            Player_SetCsActionWithHaltedActors(play, &this->actor, 8);
            this->csState = 4;
            BossGanon_SetIntroCsCamera(this, 2);
            this->csTimer = 0;
            // fallthrough
        case 4:
            if ((this->csTimer == 0) || (this->csTimer == 10) || (this->csTimer == 20)) {
                this->csCamEye.y += 68.0f;
                this->csCamEye.z -= 142.0f;
            }

            if (this->csTimer >= 20) {
                this->envLightMode = 4;
            } else {
                this->envLightMode = 35;
            }

            if (this->csTimer == 60) {
                BossGanon_SetIntroCsCamera(this, 1);
                this->csState = 5;
                this->csTimer = 0;
            }
            break;

        case 5:
            this->envLightMode = 5;

            if (this->csTimer < 50) {
                play->envCtx.unk_D8 = 1.0f;
            }

            if (this->csTimer == 10) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x4B);
            }

            if (this->csTimer == 70) {
                BossGanon_SetIntroCsCamera(this, 3);
                this->csState = 6;
                this->csTimer = 0;
                this->envLightMode = 3;
            }
            break;

        case 6:
            this->envLightMode = 3;

            if (this->csTimer != 30) {
                break;
            }

            this->csState = 7;
            this->csTimer = 0;
            BossGanon_SetIntroCsCamera(this, 4);
            this->triforceType = GDF_TRIFORCE_PLAYER;
            this->fwork[GDF_TRIFORCE_SCALE] = 10.0f;
            this->fwork[GDF_TRIFORCE_PRIM_A] = 0.0f;
            this->fwork[GDF_TRIFORCE_PRIM_B] = 255.0f;
            this->fwork[GDF_TRIFORCE_ENV_G] = 100.0f;
            Sfx_PlaySfxCentered(NA_SE_EV_TRIFORCE_MARK);
            play->envCtx.unk_D8 = 0.0f;
            // fallthrough
        case 7:
            this->envLightMode = 6;
            // fade in links triforce
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_A], 255.0f, 1.0f, 10.0f);
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_SCALE], 0.4f, 1.0f, 0.3f);
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_B], 170.0f, 1.0f, 2.55f);
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_ENV_G], 200.0f, 1.0f, 3.0f);

            if (this->csTimer >= 30) {
                this->envLightMode = 65;
            }

            if (this->csTimer == 30) {
                play->envCtx.unk_D8 = 1.0f;
            }

            BossGanon_SetIntroCsCamera(this, 4);
            this->csCamEye.x += 5.0f;
            this->csCamEye.z += -10.0f;
            this->csCamAt.x += 18.0f;

            if (this->csTimer == 60) {
                this->csState = 8;
                this->csTimer = 0;
            }
            break;

        case 8:
            this->envLightMode = 3;
            BossGanon_SetIntroCsCamera(this, 5);

            if (this->csTimer != 30) {
                break;
            }

            this->csState = 9;
            this->csTimer = 0;
            Player_SetCsActionWithHaltedActors(play, &this->actor, 8);
            sZelda->unk_3C8 = 0;
            this->triforceType = GDF_TRIFORCE_ZELDA;
            this->fwork[GDF_TRIFORCE_SCALE] = 10.0f;
            this->fwork[GDF_TRIFORCE_PRIM_A] = 0.0f;
            this->fwork[GDF_TRIFORCE_PRIM_B] = 255.0f;
            this->fwork[GDF_TRIFORCE_ENV_G] = 100.0f;
            Sfx_PlaySfxCentered(NA_SE_EV_TRIFORCE_MARK);
            play->envCtx.unk_D8 = 0.0f;
            // fallthrough
        case 9:
            this->envLightMode = 7;
            BossGanon_SetIntroCsCamera(this, 6);
            // fade in zeldas triforce
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_A], 255.0f, 1.0f, 10.0f);
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_SCALE], 0.4f, 1.0f, 0.3f);
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_B], 170.0f, 1.0f, 2.55f);
            Math_ApproachF(&this->fwork[GDF_TRIFORCE_ENV_G], 200.0f, 1.0f, 3.0f);

            if (this->csTimer == 30) {
                sZelda->unk_3C8 = 1;
            }

            if (this->csTimer >= 32) {
                this->envLightMode = 75;
            }

            if (this->csTimer == 32) {
                play->envCtx.unk_D8 = 1.0f;
            }

            if (this->csTimer == 50) {
                this->csState = 10;
                this->csTimer = 0;
            }
            break;

        case 10: // top view of playing the organ
            this->envLightMode = 3;
            BossGanon_SetIntroCsCamera(this, 7);

            if (this->csTimer == 40) {
                this->csState = 11;
                this->csTimer = 0;
                this->fwork[GDF_TRIFORCE_PRIM_A] = 0.0f;
            }
            break;

        case 11: // link is healed
            this->envLightMode = 3;
            BossGanon_SetIntroCsCamera(this, 8);
            player->actor.world.pos.z = 20.0f;

            if (this->csTimer == 20) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x17);
                Interface_ChangeHudVisibilityMode(11); // show hearts only
            }

            if (this->csTimer == 25) {
                gSaveContext.healthAccumulator = MAX_HEALTH;
            }

            if (this->csTimer == 100) {
                Interface_ChangeHudVisibilityMode(1);
            }

            if (this->csTimer == 120) {
                this->csState = 12;
                this->csTimer = 0;
            }
            break;

        case 12: // first dialogue, ganondorf facing away from link
            this->envLightMode = 3;
            BossGanon_SetIntroCsCamera(this, 9);

            if (this->csTimer == 30) {
                Audio_QueueSeqCmd(0x100100FF);
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfStopPlayingOrganAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfStopPlayingOrganAnim, -5.0f);
            }

            if ((this->csTimer > 30) && Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfLeanOnOrganAnim, 0.0f);
                this->fwork[GDF_FWORK_1] = 1000.0f;
            }

            if (this->csTimer == 80) {
                Message_StartTextbox(play, 0x70C8, NULL);
            }

            if ((this->csTimer > 180) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                this->csState = 15;
                this->csTimer = 0;
                this->useOpenHand = false;
            }
            break;

        case 15: // side view of all 3 of them
            this->envLightMode = 0;
            play->envCtx.unk_D8 = 0.0f;
            BossGanon_SetIntroCsCamera(this, 10);

            if (this->csTimer == 30) {
                Message_StartTextbox(play, 0x70C9, NULL);
            }

            if ((this->csTimer > 100) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                this->csState = 16;
                this->csTimer = 0;
                BossGanon_SetIntroCsCamera(this, 11);
                this->unk_198 = 2;
                sZelda->unk_3C8 = 2;
                this->timers[2] = 110;
                this->envLightMode = 3;
            }
            break;

        case 16:
            this->envLightMode = 3;

            if (this->csTimer <= 20) {
                if (this->csTimer == 20) {
                    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfStandUpFromOrganAnim, -5.0f);
                    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfStandUpFromOrganAnim);
                }
            } else if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                Message_StartTextbox(play, 0x70CA, NULL);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandBackwardsAnim, -5.0f);
                this->fwork[GDF_FWORK_1] = 1000.0f;
            }

            if ((this->csTimer > 100) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                this->csState = 17;
                this->csTimer = 0;
            }
            break;

        case 17: // turns around
            this->envLightMode = 3;

            if (this->csTimer == 20) {
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfTurnAroundAnim, -5.0f);
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfTurnAroundAnim);
            }

            if (this->csTimer > 10) {
                if (this->csTimer == 62) {
                    sCape->attachRightArmTimer = 20.0f;
                }

                if (this->csTimer == 57) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                }

                Math_ApproachF(&this->csCamFov, 110.0f, 0.1f, this->csCamMaxStepScale * 2.0f);
                Math_ApproachF(&this->csCamEye.z, -290.0f, 0.1f, this->csCamMaxStepScale * 2.4f);
                Math_ApproachF(&this->csCamMaxStepScale, 0.75f, 1.0f, 0.05f);

                if (this->csTimer == 70) {
                    this->csState = 18;
                    this->csTimer = 0;
                    this->csCamFov = 60.0f;
                    BossGanon_SetIntroCsCamera(this, 12);
                    if (GameInteractor_Should(VB_PLAY_GANONDORF_INTRO_CS, true, this, 0x70CB)) {
                        Message_StartTextbox(play, 0x70CB, NULL);
                    }
                }
            }
            break;

        case 18: // last dialog before triforce
            this->envLightMode = 3;
            BossGanon_SetIntroCsCamera(this, 12);
            this->csCamEye.y += -6.0f;
            this->csCamEye.z += 6.0f;

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1] - 5.0f)) {
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -5.0f);
                this->fwork[GDF_FWORK_1] = 1000.0f;
            }

            if ((this->csTimer <= 50) || (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE)) {
                break;
            }

            this->csState = 19;
            this->csTimer = 0;
            if (GameInteractor_Should(VB_PLAY_GANONDORF_INTRO_CS, true, this, 0x70CC)) {
                Message_StartTextbox(play, 0x70CC, NULL);
            }
            Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfRaiseHandStartAnim, -5.0f);
            this->triforceType = GDF_TRIFORCE_DORF;
            this->fwork[GDF_TRIFORCE_SCALE] = 10.0f;
            this->fwork[GDF_TRIFORCE_PRIM_A] = 0.0f;
            this->fwork[GDF_TRIFORCE_PRIM_B] = 255.0f;
            this->fwork[GDF_TRIFORCE_ENV_G] = 100.0f;
            play->envCtx.unk_D8 = 0.0f;
            // fallthrough
        case 19: // show triforce
            this->envLightMode = 8;

            if (this->csTimer >= 60) {
                this->envLightMode = 9;

                if (this->csTimer == 60) {
                    play->envCtx.unk_D8 = 1.0f;
                }
            }

            BossGanon_SetIntroCsCamera(this, 12);
            this->csCamEye.y += -6.0f;
            this->csCamEye.z += 6.0f;

            if (this->csTimer >= 30) {
                if (this->csTimer == 30) {
                    Sfx_PlaySfxCentered(NA_SE_EV_TRIFORCE_MARK);
                }

                // fade in ganondorf's triforce
                Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_A], 255.0f, 1.0f, 10.0f);
                Math_ApproachF(&this->fwork[GDF_TRIFORCE_SCALE], 0.6f, 1.0f, 0.3f);
                Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_B], 170.0f, 1.0f, 2.55f);
                Math_ApproachF(&this->fwork[GDF_TRIFORCE_ENV_G], 200.0f, 1.0f, 3.0f);
            }

            if (this->csTimer == 17) {
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfRaiseHandLoopAnim, -5.0f);
            }

            if ((this->csTimer > 80) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                if (!GameInteractor_Should(VB_PLAY_GANONDORF_INTRO_CS, true, this, 0)) {
                    this->timers[2] = 30;
                    this->csCamAt.x = this->unk_1FC.x - 10.0f;
                    this->csCamAt.y = this->unk_1FC.y + 30.0f;
                    this->csCamAt.z = this->unk_1FC.z;
                    this->fwork[GDF_VORTEX_ALPHA] = 0.0f;
                    this->fwork[GDF_VORTEX_SCALE] = 0.0f;
                    this->csState = 22;
                    this->csTimer = 0;
                    break;
                }

                this->csState = 20;
                this->csTimer = 0;

                this->csCamTargetEye.x = this->csCamEye.x - 50.0f;
                this->csCamTargetEye.y = this->csCamEye.y - 100.0f;
                this->csCamTargetEye.z = this->csCamEye.z + 400.0f;

                this->csCamEyeMaxStep.x = 50.0f;
                this->csCamEyeMaxStep.y = 100.0f;
                this->csCamEyeMaxStep.z = 400.0f;

                this->csCamAtMaxStep.x = 400.0f;
                this->csCamMaxStepScale = 0.0f;

                this->csCamTargetAt.x = this->csCamAt.x + 400.0f;
                this->csCamTargetAt.y = this->csCamAt.y;
                this->csCamTargetAt.z = this->csCamAt.z;

                this->csCamMovementScale = 0.2f;

                this->fwork[GDF_VORTEX_ALPHA] = 0.0f;
                this->fwork[GDF_VORTEX_SCALE] = 0.1f;

                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DARKWAVE);
            }
            break;

        case 20: // zoom cam out
            this->envLightMode = 10;
            moveCam = true;
            Math_ApproachF(&this->csCamMaxStepScale, 0.15f, 1.0f, 0.015f);

            if (this->csTimer <= 40) {
                Math_ApproachF(&this->fwork[GDF_VORTEX_ALPHA], 255.0f, 1.0f, 6.5f);
                Math_ApproachF(&this->fwork[GDF_VORTEX_SCALE], 0.2f, 1.0f, 0.025f);
            }

            if (this->csTimer > 20) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DARKWAVE_M - SFX_FLAG);
            }

            if (this->csTimer > 20) {
                BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
                BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
            }

            if (this->csTimer == 30) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x4A);
            }

            if (this->csTimer <= 50) {
                break;
            }

            this->csState = 21;
            this->csTimer = 0;
            this->fwork[GDF_TRIFORCE_PRIM_A] = 0.0f;
            this->fwork[GDF_VORTEX_SCALE] = 0.16f;
            goto skip_sound_and_fx;

        case 21: // purple vortex
            this->envLightMode = 11;
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DARKWAVE_M - SFX_FLAG);
            BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
            BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);

        skip_sound_and_fx:
            this->csCamEye.x = -30.0f;
            this->csCamEye.y = 37.0f;
            this->csCamEye.z = -30.0f;

            this->csCamAt.x = -10.0f;
            this->csCamAt.y = 45.0f;
            this->csCamAt.z = 0.0f;

            if (this->csTimer == 13) {
                Message_StartTextbox(play, 0x70CD, NULL);
            }

            if ((this->csTimer <= 120) || (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE)) {
                break;
            }

            this->csState = 22;
            this->csTimer = 0;
            this->timers[2] = 30;
            this->organAlpha = 254;
            this->csCamAt.x = this->unk_1FC.x - 10.0f;
            this->csCamAt.y = this->unk_1FC.y + 30.0f;
            this->csCamAt.z = this->unk_1FC.z;
            this->fwork[GDF_VORTEX_ALPHA] = 255.0f;
            this->fwork[GDF_VORTEX_SCALE] = 0.2f;
            // fallthrough
        case 22: // start floating, show title card, start fight
            if (this->csTimer > 30) {
                this->envLightMode = 0;
            } else {
                this->envLightMode = 12;
            }

            Math_ApproachZeroF(&this->fwork[GDF_VORTEX_ALPHA], 1.0f, 10.0f);

            this->csCamEye.x = -30.0f;
            this->csCamEye.y = 137.0f;
            this->csCamEye.z = -110.0f;

            Math_ApproachF(&this->csCamAt.y, this->unk_1FC.y + 30.0f, 0.1f, 20.0f);
            Math_ApproachF(&this->csCamAt.x, this->unk_1FC.x - 10.0f, 0.1f, 5.0f);

            if (this->csTimer == 20) {
                BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, 0.0f);
                SkelAnime_Update(&this->skelAnime);
                this->actor.shape.yOffset = 0.0f;
                sCape->attachShouldersTimer = 18.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                this->unk_198 = 0;
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_GANONDORF_BOSS);
            }

            if (this->csTimer == 50) {
                gSegments[6] = VIRTUAL_TO_PHYSICAL(
                    play->objectCtx.status[Object_GetIndex(&play->objectCtx, OBJECT_GANON)].segment);

                if (!Flags_GetEventChkInf(EVENTCHKINF_BEGAN_GANONDORF_BATTLE)) {
                    TitleCard_InitBossName(play, &play->actorCtx.titleCtx,
                                           SEGMENTED_TO_VIRTUAL(gGanondorfTitleCardENGTex), 160, 180, 128, 40, true);
                }

                Flags_SetEventChkInf(EVENTCHKINF_BEGAN_GANONDORF_BATTLE);
            }

            if (this->csTimer >= 20) {
                this->legSwayEnabled = true;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_FLOAT - SFX_FLAG);

                Math_ApproachF(&this->actor.world.pos.y, 228.0f, 0.05f, 2.0f);
                Math_ApproachF(&this->actor.world.pos.z, -230.0f, 0.05f, 4.0f);

                sCape->backPush = -3.0f;
                sCape->backSwayMagnitude = 0.25f;
                sCape->sideSwayMagnitude = -3.0f;

                sin = Math_SinS(this->csTimer * 1500);
                this->actor.velocity.y = this->fwork[GDF_FWORK_0] * sin * 0.04f;
                this->actor.world.pos.y += this->actor.velocity.y;

                cos = Math_CosS(this->csTimer * 1800);
                this->actor.world.pos.x = this->fwork[GDF_FWORK_0] * cos * 0.5f;
                this->actor.velocity.x = this->actor.world.pos.x - this->actor.prevPos.x;

                Math_ApproachF(&this->fwork[GDF_FWORK_0], 50.0f, 1.0f, 1.0f);
            }

            if (this->csTimer > 30) {
                this->organAlpha -= 5;

                if (this->organAlpha < 0) {
                    this->organAlpha = 0;
                }
            }

            if (this->csTimer == 120) {
                mainCam = Play_GetCamera(play, CAM_ID_MAIN);
                mainCam->eye = this->csCamEye;
                mainCam->eyeNext = this->csCamEye;
                mainCam->at = this->csCamAt;
                func_800C08AC(play, this->csCamIndex, 0);
                this->csState = this->csCamIndex = 0;
                func_80064534(play, &play->csCtx);
                Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
                BossGanon_SetupWait(this, play);
            }

            if (sZelda != NULL) {
                sZelda->actor.world.pos.x = 0.0f;
                sZelda->actor.world.pos.y = 350.0f;
                sZelda->actor.world.pos.z = 0.0f;
            }
    }

    if (this->csCamIndex != 0) {
        if (moveCam) {
            Math_ApproachF(&this->csCamEye.x, this->csCamTargetEye.x, this->csCamMovementScale,
                           this->csCamEyeMaxStep.x * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamEye.y, this->csCamTargetEye.y, this->csCamMovementScale,
                           this->csCamEyeMaxStep.y * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamEye.z, this->csCamTargetEye.z, this->csCamMovementScale,
                           this->csCamEyeMaxStep.z * this->csCamMaxStepScale);

            Math_ApproachF(&this->csCamAt.x, this->csCamTargetAt.x, this->csCamMovementScale,
                           this->csCamAtMaxStep.x * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamAt.y, this->csCamTargetAt.y, this->csCamMovementScale,
                           this->csCamAtMaxStep.y * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamAt.z, this->csCamTargetAt.z, this->csCamMovementScale,
                           this->csCamAtMaxStep.z * this->csCamMaxStepScale);
        }

        Play_CameraSetAtEye(play, this->csCamIndex, &this->csCamAt, &this->csCamEye);
        Play_CameraSetFov(play, this->csCamIndex, this->csCamFov);
    }
}

void BossGanon_SetupDeathCutscene(BossGanon* this, PlayState* play) {
    s32 pad;
    s32 animBankIndex = Object_GetIndex(&play->objectCtx, OBJECT_GANON_ANIME2);

    if (Object_IsLoaded(&play->objectCtx, animBankIndex)) {
        this->actionFunc = BossGanon_DeathAndTowerCutscene;
        this->csTimer = this->csState = 0;
        this->unk_198 = 1;
        this->animBankIndex = animBankIndex;
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[animBankIndex].segment);
        Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDefeatedStartAnim, 0.0f);
        this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDefeatedStartAnim);
        this->unk_508 = 0.0f;
    }
}

void BossGanon_SetupTowerCutscene(BossGanon* this, PlayState* play) {
    s32 pad;
    s32 animBankIndex = Object_GetIndex(&play->objectCtx, OBJECT_GANON_ANIME2);

    if (Object_IsLoaded(&play->objectCtx, animBankIndex)) {
        this->animBankIndex = animBankIndex;
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[animBankIndex].segment);
        Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDefeatedStartAnim, 0.0f);
        this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDefeatedStartAnim);
        this->actionFunc = BossGanon_DeathAndTowerCutscene;
        this->csTimer = 0;
        this->csState = 100;
        this->unk_198 = 1;
        gSaveContext.magic = gSaveContext.magicCapacity;
        gSaveContext.health = gSaveContext.healthCapacity;
    } else {
        this->actionFunc = BossGanon_SetupTowerCutscene;
    }
}

void BossGanon_ShatterWindows(u8 windowShatterState) {
    s16 i;
    u8* templateTex = ResourceGetDataByName(SEGMENTED_TO_VIRTUAL(gGanondorfWindowShatterTemplateTex));

    for (i = 0; i < ARRAY_COUNT(sWindowShatterTex); i++) {
        if ((sWindowShatterTex[i] != 1) && (Rand_ZeroOne() < 0.03f)) {
            if ((templateTex[i] == 0) || (windowShatterState == GDF_WINDOW_SHATTER_FULL)) {
                sWindowShatterTex[i] = 1;
            }
        }
    }
}

void BossGanon_DeathAndTowerCutscene(BossGanon* this, PlayState* play) {
    static Color_RGBA8 bloodPrimColor = { 0, 120, 0, 255 };
    static Color_RGBA8 bloodEnvColor = { 0, 120, 0, 255 };

    if (CVarGetInteger(CVAR_ENHANCEMENT("RedGanonBlood"), 0)) {
        bloodPrimColor.r = 120;
        bloodPrimColor.g = 0;

        bloodEnvColor.r = 120;
        bloodEnvColor.g = 0;
    }

    s16 i;
    u8 moveCam = false;
    Player* player = GET_PLAYER(play);
    s16 pad;
    Vec3f sp98;
    Vec3f sp8C;
    Vec3f sp80;
    Vec3f sp74;
    Camera* mainCam;
    Vec3f sp64;

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animBankIndex].segment);

    this->csTimer++;
    SkelAnime_Update(&this->skelAnime);

    switch (this->csState) {
        case 0:
            func_80064520(play, &play->csCtx);
            Player_SetCsActionWithHaltedActors(play, &this->actor, 8);
            this->csCamIndex = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, this->csCamIndex, CAM_STAT_ACTIVE);

            this->actor.world.pos.x = 0.0f;
            this->actor.world.pos.y = 70.0f;
            this->actor.world.pos.z = -80.0f;

            this->actor.shape.yOffset = -7000.0f;

            this->actor.shape.rot.y = 0;
            if (GameInteractor_Should(VB_GANONDORF_DEATH_SCENE, true, this)) {
                this->csState = 1;
                this->csTimer = 0;
            } else {
                this->csState = 9;
                this->csTimer = 170;
            }
            this->useOpenHand = true;
            // fallthrough
        case 1:
            player->actor.shape.rot.y = -0x8000;

            player->actor.world.pos.x = -10.0f;
            player->actor.world.pos.y = 0.0f;
            player->actor.world.pos.z = 115.0f;

            this->envLightMode = 13;

            if (this->csTimer < 30) {
                play->envCtx.unk_D8 = 0.0f;
            }

            if (this->csTimer >= 2) {
                play->envCtx.fillScreen = false;
            }

            this->csCamEye.x = -50.0f;
            this->csCamEye.z = -50.0f;
            this->csCamEye.y = 50.0f;

            this->csCamAt.x = this->unk_1FC.x;
            this->csCamAt.y = this->unk_1FC.y + 30.0f;
            this->csCamAt.z = this->unk_1FC.z;

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfDefeatedLoopAnim, 0.0f);
                this->csState = 2;
                this->csTimer = 0;
            }
            break;

        case 2:
            this->csCamEye.x = -100.0f;
            this->csCamEye.y = 20.0f;
            this->csCamEye.z = -130.0f;

            this->envLightMode = 13;

            this->csCamAt.x = this->unk_1FC.x;
            this->csCamAt.y = this->unk_1FC.y;
            this->csCamAt.z = this->unk_1FC.z + 40.0f;

            if (this->csTimer >= 30) {
                this->csState = 3;
                this->csTimer = 0;
                Message_StartTextbox(play, 0x70CE, NULL);
                this->fwork[GDF_FWORK_1] = 1000.0f;
            }

            if ((this->unk_1A2 % 32) == 0) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_BREATH);
            }
            break;

        case 3:
            this->envLightMode = 14;

            if ((this->fwork[GDF_FWORK_1] > 100.0f) && ((this->unk_1A2 % 32) == 0)) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_BREATH);
            }

            this->csCamEye.x = 7.0f;
            this->csCamEye.y = 52.0f;
            this->csCamEye.z = -15.0f;

            this->csCamAt.x = this->unk_1FC.x - 5.0f;
            this->csCamAt.y = this->unk_1FC.y + 30.0f - 10.0f;
            this->csCamAt.z = this->unk_1FC.z;

            if ((this->fwork[GDF_FWORK_1] > 100.0f) && (this->csTimer > 100) &&
                (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfVomitStartAnim, 0.0f);
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfVomitStartAnim);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_TOKETU);
            } else {
                if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1] - 16.0f)) {
                    for (i = 0; i < 40; i++) {
                        sp98.x = Rand_CenteredFloat(5.0f);
                        sp98.y = Rand_CenteredFloat(1.5f) + 1.0f;
                        sp98.z = Rand_ZeroFloat(5.0f) + 2.0f;

                        sp8C.x = 0.0f;
                        sp8C.y = -1.0f;
                        sp8C.z = 0.0f;

                        sp80.x = this->unk_208.x;
                        sp80.y = this->unk_208.y - 10.0f;
                        sp80.z = this->unk_208.z;

                        func_8002836C(play, &sp80, &sp98, &sp8C, &bloodPrimColor, &bloodEnvColor,
                                      (s16)Rand_ZeroFloat(50.0f) + 50, 0, 17);
                    }
                }

                if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                    Animation_MorphToLoop(&this->skelAnime, &gGanondorfVomitLoopAnim, 0.0f);
                    this->csState = 4;
                    this->csTimer = 0;
                }
            }
            break;

        case 4:
            this->envLightMode = 14;

            if (this->csTimer == 30) {
                Message_StartTextbox(play, 0x70CF, NULL);
                this->csState = 5;
                this->csTimer = 0;
            }
            break;

        case 5:
            this->envLightMode = 14;

            if ((this->csTimer > 70) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                this->csState = 6;
                this->csTimer = 0;
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfYellStartAnim, 0.0f);
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfYellStartAnim);

                this->csCamMovementScale = 0.05f;
                this->csCamMaxStepScale = 0.0f;

                this->csCamTargetEye.x = 7.0f;
                this->csCamTargetEye.y = 12.0f;
                this->csCamTargetEye.z = 70.0f;

                this->csCamTargetAt.x = this->unk_1FC.x - 5.0f;
                this->csCamTargetAt.y = (this->unk_1FC.y + 30.0f) - 10.0f;
                this->csCamTargetAt.z = this->unk_1FC.z;

                this->csCamEyeMaxStep.x = fabsf(this->csCamEye.x - this->csCamTargetEye.x);
                this->csCamEyeMaxStep.y = fabsf(this->csCamEye.y - this->csCamTargetEye.y);
                this->csCamEyeMaxStep.z = fabsf(this->csCamEye.z - this->csCamTargetEye.z);

                this->csCamAtMaxStep.x = fabsf(this->csCamAt.x - this->csCamTargetAt.x);
                this->csCamAtMaxStep.y = fabsf(this->csCamAt.y - this->csCamTargetAt.y);
                this->csCamAtMaxStep.z = fabsf(this->csCamAt.z - this->csCamTargetAt.z);

                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_CASBREAK);
            }
            break;

        case 6:
            this->envLightMode = 14;
            moveCam = true;
            Math_ApproachF(&this->csCamMaxStepScale, 0.2f, 1.0f, 0.01f);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfYellLoopAnim, 0.0f);
                this->csState = 7;
                this->csTimer = 0;
                this->unk_2E8 = 0;
                this->envLightMode = 15;
                this->unk_508 = 0.0f;
                this->fwork[GDF_FWORK_1] = 1000.0f;
                play->envCtx.unk_D8 = 0.0f;
            }
            break;

        case 7:
            if (this->csTimer < 10) {
                play->envCtx.unk_D8 = 0.0f;
            }

            if (this->csTimer == 30) {
                this->csState = 8;
                this->csTimer = 0;
                this->unk_70C = 0.0f;
            }
            goto skip_cam_and_quake;

        case 8:
            this->csCamEye.x = -60.0f;
            this->csCamEye.y = 80.0f;
            this->csCamEye.z = -130.0f;

            this->csCamAt.x = 0.0f;
            this->csCamAt.y = 0.0f;
            this->csCamAt.z = 70.0f;

            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * 0.2f;

            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

        skip_cam_and_quake:
            this->envLightMode = 15;
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_BODY_SPARK - SFX_FLAG);

            for (i = 1; i < 15; i++) {
                this->unk_4E4[i] = 0xA;
            }

            this->unk_2E6 = 20000;
            Math_ApproachF(&this->unk_508, 5.0f, 0.05f, 0.1f);

            if (this->csTimer == 30) {
                this->csState = 9;
                this->csTimer = 0;

                this->csCamEye.x = -30.0f;
                this->csCamEye.y = 40.0f;
                this->csCamEye.z = 60.0f;

                this->csCamAt.x = 492.0f;
                this->csCamAt.y = 43.0f;
                this->csCamAt.z = 580.0f;

                this->csCamMaxStepScale = 0.0f;
                this->unk_710 = 10.0f;
            }
            break;

        case 9:
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_BODY_SPARK - SFX_FLAG);

            if (this->csTimer == 2) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x39);
            }

            if (this->csTimer > 50) {
                Math_ApproachZeroF(&this->unk_710, 1.0f, 0.2f);
                Math_ApproachF(&this->csCamEye.x, 270.0f, 0.05f, this->csCamMaxStepScale * 30.0f);
                Math_ApproachF(&this->csCamEye.z, 260.0f, 0.05f, this->csCamMaxStepScale * 20.0f);
                Math_ApproachF(&this->csCamAt.y, 103.0f, 0.05f, this->csCamMaxStepScale * 6.0f);
                Math_ApproachF(&this->csCamAt.z, 280.0f, 0.05f, this->csCamMaxStepScale * 20.0f);
                Math_ApproachF(&this->csCamMaxStepScale, 1.0f, 1.0f, 0.01f);
            }

            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * this->unk_710;
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            if (this->csTimer < 100) {
                this->windowShatterState = GDF_WINDOW_SHATTER_PARTIAL;
                this->envLightMode = 15;
            } else {
                this->envLightMode = 16;
                this->windowShatterState = GDF_WINDOW_SHATTER_FULL;
            }

            if (this->csTimer >= 130) {
                Math_ApproachF(&this->whiteFillAlpha, 255.0f, 1.0f, 5.0f);
            }

            if (this->csTimer == 180) {
                play->transitionTrigger = TRANS_TRIGGER_START;
                play->nextEntranceIndex = ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0;
                play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
            }
            break;

        case 100:
            func_80064520(play, &play->csCtx);
            Player_SetCsActionWithHaltedActors(play, &this->actor, 8);
            this->csCamIndex = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, this->csCamIndex, CAM_STAT_ACTIVE);
            Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfCollapseAnim, 0.0f);
            this->fwork[1] = Animation_GetLastFrame(&gGanondorfDefeatedStartAnim);
            this->skelAnime.playSpeed = 0.0f;
            sZelda = (EnZl3*)Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_ZL3, 0.0f, 6000.0f, 0.0f,
                                                0, 0, 0, 0x2000);

            if (GameInteractor_Should(VB_PLAY_ZELDA_CRYSTAL_CS, true, this)) {
                this->csState = 101;
            } else {
                this->skelAnime.playSpeed = 1.0f;
                sZelda->actor.world.pos.x = -472.0f;
                sZelda->actor.world.pos.y = 4102.0f;
                sZelda->actor.world.pos.z = -200.0f;
                sZelda->unk_3C8 = 3;
                this->csState = 104;
            }

            player->actor.world.pos.x = -472.0f;
            player->actor.world.pos.y = 4102.0f;
            player->actor.world.pos.z = -130.0f;

            player->actor.shape.rot.y = -0x8000;

            this->actor.world.pos.x = -472.0f;
            this->actor.world.pos.y = 4172.0f;
            this->actor.world.pos.z = -400.0f;

            this->actor.shape.yOffset = -7000.0f;
            this->actor.shape.rot.y = 0;

            this->csCamEye.x = this->csCamAt.x = -472.0f;
            this->csCamEye.y = this->csCamAt.y = 4152.0f;
            this->csCamEye.z = -160.0f;

            this->csCamAt.z = -100.0f;

            sCape->backPush = -2.0f;
            sCape->backSwayMagnitude = 0.25f;
            sCape->sideSwayMagnitude = -1.0f;
            sCape->minDist = 0.0f;
            sCape->minY = 4104.0f;
            sCape->tearTimer = 20;

            this->whiteFillAlpha = 255.0f;
            play->envCtx.unk_D8 = 1.0f;
            // fallthrough
        case 101:
            player->actor.world.pos.y = 4102.0f;
            Math_ApproachZeroF(&this->whiteFillAlpha, 1.0f, 5.0f);

            if (this->csTimer > 40) {
                Math_ApproachF(&this->csCamEye.z, -520.0f, 0.1f, this->csCamMaxStepScale);
                Math_ApproachF(&this->csCamMaxStepScale, 5.0f, 1.0f, 0.1f);

                if (this->csTimer == 150) {
                    this->skelAnime.playSpeed = 1.0f;
                }

                if (this->csTimer == 160) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_PL_BOUND_NOWEAPON);
                }

                if (this->csTimer == 187) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_PL_BODY_HIT);
                }

                if (this->csTimer == 180) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                }

                if (this->csTimer == 190) {
                    sp74 = this->actor.world.pos;
                    sp74.y = 4102.0f;
                    BossGanonEff_SpawnDustDark(play, &sp74, 0.2f, 0.7f);
                }

                if (this->csTimer == 230) {
                    this->csState = 102;
                    this->csTimer = 0;
                }
            }
            break;

        case 102:
            player->actor.world.pos.y = 4102.0f;

            this->csCamEye.x = -442.0f;
            this->csCamEye.y = 4152.0f;
            this->csCamEye.z = -135.0f;

            this->csCamAt.x = -472.0f;
            this->csCamAt.y = 4152.0f;
            this->csCamAt.z = -135.0f;

            if (this->csTimer == 5) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x4C);
            }

            if (this->csTimer == 70) {
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x4D);
            }

            if (this->csTimer == 90) {
                this->csState = 103;
                this->csTimer = 0;
                sZelda->actor.world.pos.x = -472.0f;
                sZelda->actor.world.pos.y = 4352.0f;
                sZelda->actor.world.pos.z = -200.0f;
                sZelda->unk_3C8 = 3;
            }
            break;

        case 103:
            Audio_PlayActorSound2(&sZelda->actor, NA_SE_EV_DOWN_TO_GROUND - SFX_FLAG);
            Math_ApproachF(&sZelda->actor.world.pos.y, 4102.0f, 0.05f, 1.5f);

            this->csCamEye.x = -242.0f;
            this->csCamEye.y = 4122.0f;
            this->csCamEye.z = -190.0f;

            this->csCamAt.x = sZelda->actor.world.pos.x;
            this->csCamAt.y = sZelda->actor.world.pos.y + 40.0f + 5.0f;
            this->csCamAt.z = sZelda->actor.world.pos.z;

            if (this->csTimer == 200) {
                sZelda->actor.world.pos.y = 4102.0f;
                this->csState = 104;
                this->csTimer = 0;
            } else {
                break;
            }
            // fallthrough
        case 104:
            if (!GameInteractor_Should(VB_PLAY_ZELDA_CRYSTAL_CS, true, this)) {
                Math_ApproachZeroF(&this->whiteFillAlpha, 1.0f, 10.0f);
            }

            this->csCamEye.x = -432.0f;
            this->csCamEye.y = 4147.0f;
            this->csCamEye.z = -200.0f;

            this->csCamAt.x = sZelda->actor.world.pos.x;
            this->csCamAt.y = sZelda->actor.world.pos.y + 40.0f + 5.0f;
            this->csCamAt.z = sZelda->actor.world.pos.z;

            if (this->csTimer >= 10) {
                Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.05f);
            }

            if (this->csTimer == 10) {
                sZelda->unk_3C8 = 8;
            }

            if (this->csTimer == 50) {
                if (GameInteractor_Should(VB_PLAY_ZELDA_CRYSTAL_CS, true, this)) {
                    sZelda->unk_3C8 = 4;
                } else {
                    this->csState = 108;
                    this->csTimer = 0;
                    sZelda->unk_3C8 = 7;
                    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_ESCAPE);
                    break;
                }
            }

            if (this->csTimer == 100) {
                this->csState = 105;
                this->csTimer = 0;
            }
            break;

        case 105:
            this->csCamEye.x = -450.0f;
            this->csCamEye.y = 4154.0f;
            this->csCamEye.z = -182.0f;

            this->csCamAt.x = sZelda->actor.world.pos.x - 5.0f;
            this->csCamAt.y = sZelda->actor.world.pos.y + 40.0f + 5.0f;
            this->csCamAt.z = sZelda->actor.world.pos.z - 25.0f;

            if (this->csTimer == 10) {
                Message_StartTextbox(play, 0x70D0, NULL);
            }

            if ((this->csTimer > 100) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                this->csState = 1055;
                this->csTimer = 0;
            }
            break;

        case 1055:
            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * 0.3f;
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            if (this->csTimer == 20) {
                sZelda->unk_3C8 = 5;
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x39);
            }

            if (this->csTimer == 40) {
                this->csState = 1056;
                this->csTimer = 0;
            }
            break;

        case 1056:
            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * 0.3f;
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            this->csCamEye.x = -503.0f;
            this->csCamEye.y = 4128.0f;
            this->csCamEye.z = -162.0f;

            this->csCamAt.x = -416.0f;
            this->csCamAt.y = 4181.0f;
            this->csCamAt.z = -75.0f;

            if (this->csTimer > 40) {
                this->csState = 1057;
                this->csTimer = 0;
            }
            break;

        case 1057:
            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * (50.0f * this->csCamMovementScale);
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            Math_ApproachF(&this->csCamEye.x, -1200.0f, 0.1f, this->csCamMovementScale * 697.0f);
            Math_ApproachF(&this->csCamEye.y, 4241.0f, 0.1f, this->csCamMovementScale * 113.0f);
            Math_ApproachF(&this->csCamEye.z, -1048.0f, 0.1f, this->csCamMovementScale * 886.0f);

            Math_ApproachF(&this->csCamMovementScale, 0.05f, 1.0f, 0.001f);

            if (this->csTimer > 80) {
                this->csState = 106;
                this->csTimer = 60;
            }
            break;

        case 106:
            this->csCamEye.x = -450.0f;
            this->csCamEye.y = 4154.0f;
            this->csCamEye.z = -182.0f;

            this->csCamAt.x = sZelda->actor.world.pos.x - 5.0f;
            this->csCamAt.y = sZelda->actor.world.pos.y + 40.0f + 5.0f;
            this->csCamAt.z = sZelda->actor.world.pos.z - 25.0f;

            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * 0.3f;
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            if (this->csTimer == 70) {
                sZelda->unk_3C8 = 6;
            }

            if (this->csTimer == 90) {
                Message_StartTextbox(play, 0x70D1, NULL);
            }

            if ((this->csTimer > 150) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                this->csState = 107;
                this->csTimer = 0;
                Message_StartTextbox(play, 0x70D2, NULL);
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x39);
            }
            break;

        case 107:
            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * 0.8f;
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            this->csCamEye.x = -380.0f;
            this->csCamEye.y = 4154.0f;
            this->csCamEye.z = -242.0f;

            this->csCamAt.x = (sZelda->actor.world.pos.x - 5.0f) - 30.0f;
            this->csCamAt.y = (sZelda->actor.world.pos.y + 40.0f + 5.0f) - 20.0f;
            this->csCamAt.z = (sZelda->actor.world.pos.z - 25.0f) + 80.0f;

            if ((this->csTimer > 50) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                sZelda->unk_3C8 = 7;
                this->csState = 108;
                this->csTimer = 0;
            }
            break;

        case 108:
            this->unk_70C = Math_SinS(this->csTimer * 0x6300) * 0.8f;
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);

            this->csCamAt.x = (sZelda->actor.world.pos.x - 5.0f) - 30.0f;
            this->csCamAt.y = (sZelda->actor.world.pos.y + 40.0f + 5.0f) - 20.0f;
            this->csCamAt.z = (sZelda->actor.world.pos.z - 25.0f) + 80.0f;

            if (this->csTimer > 50) {
                mainCam = Play_GetCamera(play, CAM_ID_MAIN);

                mainCam->eye = this->csCamEye;
                mainCam->eyeNext = this->csCamEye;
                mainCam->at = this->csCamAt;

                func_800C08AC(play, this->csCamIndex, 0);
                this->csState = 109;
                this->csCamIndex = 0;
                func_80064534(play, &play->csCtx);
                Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
                Flags_SetSwitch(play, 0x37);
            }
            break;

        case 109:
            Sfx_PlaySfxCentered(NA_SE_EV_EARTHQUAKE - SFX_FLAG);
            break;
    }

    if (this->csState >= 100) {
        this->envLightMode = 20;
    }

    if (this->csCamIndex != 0) {
        if (moveCam) {
            Math_ApproachF(&this->csCamEye.x, this->csCamTargetEye.x, this->csCamMovementScale,
                           this->csCamEyeMaxStep.x * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamEye.y, this->csCamTargetEye.y, this->csCamMovementScale,
                           this->csCamEyeMaxStep.y * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamEye.z, this->csCamTargetEye.z, this->csCamMovementScale,
                           this->csCamEyeMaxStep.z * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamAt.x, this->csCamTargetAt.x, this->csCamMovementScale,
                           this->csCamAtMaxStep.x * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamAt.y, this->csCamTargetAt.y, this->csCamMovementScale,
                           this->csCamAtMaxStep.y * this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamAt.z, this->csCamTargetAt.z, this->csCamMovementScale,
                           this->csCamAtMaxStep.z * this->csCamMaxStepScale);
        }

        sp64 = this->csCamAt;
        sp64.y += this->unk_70C;
        Play_CameraSetAtEye(play, this->csCamIndex, &sp64, &this->csCamEye);
    }
}

void BossGanon_SetupPoundFloor(BossGanon* this, PlayState* play) {
    this->unk_1C2 = 0;
    this->timers[0] = 40;
    this->actionFunc = BossGanon_PoundFloor;
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->fwork[GDF_CENTER_POS] = 100.0f;
}

void BossGanon_PoundFloor(BossGanon* this, PlayState* play) {
    s16 i;
    f32 heightTarget;
    f32 targetPosX;
    f32 targetPosZ;
    Vec3f sp6C;
    Vec3f sp60;
    Vec3f sp54;
    Vec3f sp48;

    SkelAnime_Update(&this->skelAnime);

    switch (this->unk_1C2) {
        case 0:
            targetPosX = Math_SinS(this->unk_1A2 * 1280);
            targetPosX = targetPosX * this->fwork[GDF_CENTER_POS];
            targetPosZ = Math_CosS(this->unk_1A2 * 1792);
            targetPosZ = targetPosZ * this->fwork[GDF_CENTER_POS];

            Math_ApproachF(&this->actor.world.pos.x, targetPosX, 0.05f, this->fwork[GDF_FWORK_0]);
            Math_ApproachF(&this->actor.world.pos.z, targetPosZ, 0.05f, this->fwork[GDF_FWORK_0]);
            Math_ApproachF(&this->fwork[GDF_CENTER_POS], 0.0f, 1, 1.5f);

            if (this->timers[0] == 5) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_HIT_GND);
            }

            if (this->timers[0] < 14) {
                heightTarget = 250.0f;
                this->unk_258 += (Rand_ZeroFloat(M_PI / 2) + (M_PI / 2));
                Math_ApproachF(&this->handLightBallScale, 7.0f, 0.5f, 1.0f);
                this->envLightMode = 1;
            } else {
                heightTarget = 200.0f;
            }

            Math_ApproachF(&this->actor.world.pos.y, heightTarget, 0.1f, this->actor.velocity.y);
            Math_ApproachF(&this->actor.velocity.y, 20.0f, 1.0f, 1.0f);

            if (this->timers[0] == 14) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfPoundAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfPoundAnim, 0.0f);
                this->actor.velocity.y = 0.0f;
            }

            if (this->timers[0] == 0) {
                this->unk_1C2 = 1;
                this->actor.velocity.y = 0.0f;
            }
            break;

        case 1:
            sCape->gravity = -1.0f;
            this->envLightMode = 1;
            Math_ApproachF(&this->actor.velocity.y, -50.0f, 1.0f, 10.0f);
            this->actor.world.pos.y += this->actor.velocity.y;

            if (this->actor.world.pos.y < 60.0f) {
                this->actor.world.pos.y = 60.0f;
                this->unk_1C2 = 2;
                this->timers[0] = 10;
                Actor_RequestQuakeAndRumble(&this->actor, play, 0xA, 0x14); // rumble
                this->unk_19C = 35;
                this->unk_19E = 0;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_HIT_GND_IMP);
                this->handLightBallScale = 0.0f;
                sp60 = this->unk_260;
                sp60.y = 0.0f;

                for (i = 0; i < 80; i++) {
                    sp6C.x = Rand_CenteredFloat(25.0f);
                    sp6C.y = Rand_ZeroFloat(17.0f);
                    sp6C.z = Rand_CenteredFloat(25.0f);
                    BossGanonEff_SpawnLightRay(play, &sp60, &sp6C, &sZeroVec, Rand_ZeroFloat(300.0f) + 500.0f, 13.0f,
                                               0x1E);
                }
            }
            break;

        case 2:
            this->envLightMode = 1;

            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfPoundEndAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfPoundEndAnim, 0.0f);
                this->unk_1C2 = 3;
                this->unk_19F = 1;
                this->actor.velocity.y = 0.0f;
            }
            break;

        case 3:
            Math_ApproachF(&this->actor.world.pos.y, 150.0f, 0.1f, this->actor.velocity.y);
            Math_ApproachF(&this->actor.velocity.y, 20.0f, 1.0f, 1.0f);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp3Anim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, 0.0f);
                SkelAnime_Update(&this->skelAnime);
                sCape->attachShouldersTimer = 18.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                this->unk_1C2 = 4;
            }
            break;

        case 4:
            Math_ApproachF(&this->actor.world.pos.y, 150.0f, 0.1f, this->actor.velocity.y);
            Math_ApproachF(&this->actor.velocity.y, 20.0f, 1.0f, 1.0f);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_SetupWait(this, play);
            }
            break;
    }

    if ((this->unk_19C == 35) || (this->unk_19C == 30) || (this->unk_19C == 25)) {
        sp54 = this->actor.world.pos;
        sp54.y = 0.0f;
        BossGanonEff_SpawnDustLight(play, &sp54, 0, 3.0f, this->unk_19C - 25);
    }

    if (this->unk_19C == 35) {
        sp48 = this->actor.world.pos;
        sp48.y = 0.0f;
        BossGanonEff_SpawnShockwave(play, &sp48, 0, 3.0f);
    }
}

void BossGanon_SetupChargeBigMagic(BossGanon* this, PlayState* play) {
    this->unk_1C2 = 0;
    this->timers[0] = 30;
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->fwork[GDF_CENTER_POS] = 100.0f;
    this->unk_1AA = Rand_ZeroFloat(20000.0f);
    this->unk_1AC = 0;
    this->actionFunc = BossGanon_ChargeBigMagic;
}

void BossGanon_ChargeBigMagic(BossGanon* this, PlayState* play) {
    s32 pad;
    f32 targetPosX;
    f32 targetPosZ;
    Vec3f sp80;
    Vec3f sp74;
    Vec3f sp68;
    s16 i;

    SkelAnime_Update(&this->skelAnime);

    targetPosX = Math_SinS(this->unk_1A2 * 1280);
    targetPosX = targetPosX * this->fwork[GDF_CENTER_POS];

    targetPosZ = Math_CosS(this->unk_1A2 * 1792);
    targetPosZ = targetPosZ * this->fwork[GDF_CENTER_POS];

    Math_ApproachF(&this->actor.world.pos.x, targetPosX, 0.05f, this->fwork[GDF_FWORK_0]);
    Math_ApproachF(&this->actor.world.pos.z, targetPosZ, 0.05, this->fwork[GDF_FWORK_0]);

    Math_ApproachF(&this->fwork[GDF_CENTER_POS], 0.0f, 1.0f, 1.5f);
    Math_ApproachF(&this->actor.world.pos.y, 200.0f, 0.05f, this->actor.velocity.y);
    Math_ApproachF(&this->actor.velocity.y, 20.0f, 1.0f, 1.0f);

    switch (this->unk_1C2) {
        case 0:
            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicChargeStartAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBigMagicChargeStartAnim, 0.0f);
                this->unk_1C2 = 1;
            }
            break;

        case 1:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicChargeHoldAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfBigMagicChargeHoldAnim, 0.0f);
                this->unk_1C2 = 2;
                this->timers[0] = 100;
            }
            break;

        case 2:
            this->envLightMode = 2;
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_CHARGE_MASIC - SFX_FLAG);
            this->unk_278.x = this->unk_2EC[0].x;
            this->unk_278.y = this->unk_2EC[0].y + 50.0f + 30.0f;
            this->unk_278.z = this->unk_2EC[0].z;

            Math_ApproachF(&this->unk_284, 0.25f, 0.1f, 0.006f);
            Math_ApproachF(&this->unk_288, 255.0f, 1.0f, 255.0f);
            Math_ApproachF(&this->unk_28C, 0.25f, 0.1f, 0.006f);

            if ((this->timers[0] > 20) && (this->timers[0] < 60)) {
                Math_ApproachF(&this->unk_290, 255.0f, 1.0f, 15.0f);
            }

            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicWindupAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBigMagicWindupAnim, 0.0f);
                this->unk_1C2 = 3;
                this->timers[0] = 6;
                this->timers[1] = 15;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DARKWAVE);
                break;
            }

            Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0x3E8);

            if (this->timers[0] < -4) {
                for (i = 0; i < ARRAY_COUNT(this->unk_294); i++) {
                    Math_ApproachF(&this->unk_294[i], 0.0f, 1.0f, 40.0f);
                }
            } else if ((this->timers[0] >= 7) && (this->timers[0] < 26)) {
                if (this->unk_1AC < ARRAY_COUNT(this->unk_294)) {
                    this->unk_1AC++;
                }

                for (i = 0; i < this->unk_1AC; i++) {
                    Math_ApproachF(&this->unk_294[i], 200.0f, 1.0f, 40.0f);
                }
            }

            if (this->timers[0] <= 30) {
                Math_ApproachF(&this->unk_284, 0.4f, 0.5f, 0.017f);
                this->unk_28C = this->unk_284;
            }

            if (this->timers[0] <= 30) {
                Math_ApproachF(&this->unk_2D0, 45.0f, 0.1f, 10.0f);
                this->lensFlareTimer = 1;
                this->lensFlareMode = 2;
                gCustomLensFlarePos = this->unk_278;
            }

            if (this->timers[0] == 47) {
                this->unk_274 = 1;
            }

            if (this->timers[0] == 46) {
                this->unk_274 = 2;
            }

            if (this->timers[0] == 45) {
                this->unk_274 = 3;
            }

            if (this->timers[0] == 44) {
                this->unk_274 = 4;
            }

            if (this->timers[0] == 43) {
                this->unk_274 = 5;
            }

            if (this->timers[0] == 42) {
                this->unk_274 = 6;
            }

            if (this->timers[0] > 30) {
                sp74.x = 0.0f;
                sp74.y = Rand_ZeroFloat(10.0f) + 150.0f;
                sp74.z = 0.0f;

                Matrix_RotateY(BINANG_TO_RAD(this->actor.yawTowardsPlayer), MTXMODE_NEW);
                Matrix_RotateZ(Rand_ZeroFloat(65536.0f), MTXMODE_APPLY);
                Matrix_MultVec3f(&sp74, &sp68);

                sp80.x = this->unk_278.x + sp68.x;
                sp80.y = this->unk_278.y + sp68.y;
                sp80.z = this->unk_278.z + sp68.z;

                BossGanonEff_SpawnBlackDot(play, &sp80, 20.0f);
            }
            break;

        case 3:
            this->envLightMode = 2;

            for (i = 0; i < ARRAY_COUNT(this->unk_294); i++) {
                Math_ApproachF(&this->unk_294[i], 0.0f, 1.0f, 40.0f);
            }

            if (this->timers[0] == 1) {
                sCape->attachLeftArmTimer = 15.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            }

            if (this->timers[0] == 0) {
                Math_ApproachZeroF(&this->unk_284, 1.0f, 0.08f);
                this->unk_28C = this->unk_284;
                Math_ApproachZeroF(&this->unk_2D0, 1.0f, 10.0f);
                Math_ApproachF(&this->unk_278.x, this->unk_1FC.x, 0.5f, 30.0f);
                Math_ApproachF(&this->unk_278.y, this->unk_1FC.y, 0.5f, 30.0f);
                Math_ApproachF(&this->unk_278.z, this->unk_1FC.z, 0.5f, 30.0f);
            }

            if (this->timers[1] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicThrowAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfBigMagicThrowAnim, 0.0f);
                this->unk_1C2 = 4;
                this->unk_288 = 0.0f;
                this->unk_290 = 0.0f;
                this->unk_284 = 0.0f;
                this->unk_28C = 0.0f;
            }
            break;

        case 4:
            this->envLightMode = 2;

            if (Animation_OnFrame(&this->skelAnime, 5.0f)) {
                for (i = 0; i < 5; i++) {
                    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_1FC.x,
                                       this->unk_1FC.y, this->unk_1FC.z, 0, this->actor.yawTowardsPlayer, 0, 0x104 + i);
                }

                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_BIGMASIC);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_THROW_BIG);
            }

            if (Animation_OnFrame(&this->skelAnime, 3.0f)) {
                sCape->attachShouldersTimer = 26.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            }

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicThrowEndAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfBigMagicThrowEndAnim, 0.0f);
                this->unk_1C2 = 5;
            }
            break;

        case 5:
            this->envLightMode = 2;

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_SetupWait(this, play);
            }
            break;
    }
}

void BossGanon_SetupWait(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, -10.0f);
    this->actionFunc = BossGanon_Wait;
    this->fwork[GDF_FWORK_0] = 0.0f;
    this->timers[0] = (s16)Rand_ZeroFloat(64.0f) + 30;
    this->unk_1C2 = 0;
    sCape->minY = 2.0f;
}

void BossGanon_Wait(BossGanon* this, PlayState* play) {
    f32 sin;
    s32 pad;
    f32 cos;
    Player* player = GET_PLAYER(play);

    this->legSwayEnabled = true;

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    SkelAnime_Update(&this->skelAnime);

    if ((this->unk_1C2 == 0) && !(player->actor.world.pos.y < 0.0f)) {
        if (!(player->stateFlags1 & PLAYER_STATE1_HANGING_OFF_LEDGE) && (fabsf(player->actor.world.pos.x) < 110.0f) &&
            (fabsf(player->actor.world.pos.z) < 110.0f)) {
            BossGanon_SetupPoundFloor(this, play);
        } else if ((this->timers[0] == 0) && !(player->stateFlags1 & PLAYER_STATE1_HANGING_OFF_LEDGE)) {
            this->timers[0] = (s16)Rand_ZeroFloat(30.0f) + 30;

            if ((s8)this->actor.colChkInfo.health >= 20) {
                BossGanon_SetupChargeLightBall(this, play);
            } else if (Rand_ZeroOne() >= 0.5f) {
                if ((Rand_ZeroOne() >= 0.5f) || (this->actor.xzDistToPlayer > 350.0f)) {
                    BossGanon_SetupChargeBigMagic(this, play);
                } else {
                    BossGanon_SetupPoundFloor(this, play);
                }
            } else {
                BossGanon_SetupChargeLightBall(this, play);
            }
        }
    }

    sin = Math_SinS(this->unk_1A2 * 1280) * 100.0f;
    cos = Math_CosS(this->unk_1A2 * 1792) * 100.0f;

    Math_ApproachF(&this->actor.world.pos.x, sin, 0.05f, this->fwork[GDF_FWORK_0]);
    Math_ApproachF(&this->actor.world.pos.y, 150.0f, 0.05f, this->fwork[GDF_FWORK_0] * 0.2f);
    Math_ApproachF(&this->actor.world.pos.z, cos, 0.05f, this->fwork[GDF_FWORK_0]);
    Math_ApproachF(&this->fwork[GDF_FWORK_0], 50.0f, 1.0f, 0.5f);

    this->actor.velocity.x = this->actor.world.pos.x - this->actor.prevPos.x;
    this->actor.velocity.z = this->actor.world.pos.z - this->actor.prevPos.z;

    sin = Math_SinS(this->unk_1A2 * 1500);
    this->actor.velocity.y = this->fwork[GDF_FWORK_0] * sin * 0.04f;
    this->actor.world.pos.y += this->actor.velocity.y;

    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0xBB8);
    Sfx_PlaySfxAtPos(&this->actor.projectedPos, NA_SE_EN_FANTOM_FLOAT - SFX_FLAG);
}

void BossGanon_SetupChargeLightBall(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfChargeLightBallAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfChargeLightBallAnim, -3.0f);
    this->actionFunc = BossGanon_ChargeLightBall;
    this->timers[0] = 25;
}

void BossGanon_ChargeLightBall(BossGanon* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 1.25f;
    sCape->sideSwayMagnitude = -2.0f;
    sCape->minDist = 10.0f;

    if (this->timers[0] < 17) {
        this->envLightMode = 1;
    }

    if (this->timers[0] == 17) {
        this->unk_26C = 10;
        this->unk_270 = Rand_ZeroFloat(M_PI);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_SPARK);
    }

    if (this->timers[0] < 10) {
        this->unk_258 += (Rand_ZeroFloat(M_PI / 2) + (M_PI / 2));
        Math_ApproachF(&this->handLightBallScale, 10.0f, 0.5f, 1.25f);

        if (this->timers[0] == 0) {
            BossGanon_SetupPlayTennis(this, play);
        }
    }

    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0x7D0);

    this->actor.world.pos.x += this->actor.velocity.x;
    this->actor.world.pos.z += this->actor.velocity.z;

    Math_ApproachZeroF(&this->actor.velocity.x, 1.0f, 0.5f);
    Math_ApproachZeroF(&this->actor.velocity.z, 1.0f, 0.5f);

    this->actor.velocity.y = Math_SinS(this->unk_1A2 * 1500) * 2.0f;
    this->actor.world.pos.y += this->actor.velocity.y;
}

void BossGanon_SetupPlayTennis(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfThrowAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfThrowAnim, 0.0f);
    this->actionFunc = BossGanon_PlayTennis;
}

void BossGanon_PlayTennis(BossGanon* this, PlayState* play) {
    static AnimationHeader* volleyAnims[] = { &gGanondorfVolleyLeftAnim, &gGanondorfVolleyRightAnim };
    static s16 capeRightArmDurations[] = { 26, 20 };
    s16 rand;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->handLightBallScale, 1.0f, 0.2f);

    switch (this->unk_1C2) {
        case 0:
            this->envLightMode = 1;

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 1;
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, 0.0f);
            }

            if (this->skelAnime.curFrame <= 12.0f) {
                this->lensFlareTimer = 2;
                this->lensFlareMode = 2;
                gCustomLensFlarePos = this->unk_260;
            }

            if (Animation_OnFrame(&this->skelAnime, 12.0f)) {
                this->handLightBallScale = 0.0f;
            }

            if (Animation_OnFrame(&this->skelAnime, 11.0f)) {
                this->unk_25C = 1;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_THROW);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_THROW_MASIC);
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_260.x,
                                   this->unk_260.y, this->unk_260.z, 0, 0, 0, 0x64);
            }
            break;

        case 1:
            if (this->startVolley) {
                rand = Rand_ZeroOne() * 1.99f;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(volleyAnims[rand]);
                Animation_MorphToPlayOnce(&this->skelAnime, volleyAnims[rand], 0.0f);
                sCape->attachRightArmTimer = capeRightArmDurations[rand];
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                this->startVolley = false;
            }
            break;
    }

    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0x7D0);

    this->actor.world.pos.x += this->actor.velocity.x;
    this->actor.world.pos.z += this->actor.velocity.z;

    Math_SmoothStepToF(&this->actor.velocity.x, 0.0f, 1.0f, 0.5f, 0.0f);
    Math_SmoothStepToF(&this->actor.velocity.z, 0.0f, 1.0f, 0.5f, 0.0f);

    this->actor.velocity.y = Math_SinS(this->unk_1A2 * 1500) * 2.0f;
    this->actor.world.pos.y += this->actor.velocity.y;
}

void BossGanon_SetupBlock(BossGanon* this, PlayState* play) {
    if ((this->actionFunc != BossGanon_Block) || (this->unk_1C2 != 0)) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
        this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBlockAnim);
        Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBlockAnim, 0.0f);
        this->actionFunc = BossGanon_Block;
    }

    this->unk_1C2 = 0;
    sCape->attachLeftArmTimer = this->timers[0] = 10;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
    this->handLightBallScale = 0.0f;
}

void BossGanon_Block(BossGanon* this, PlayState* play) {
    this->collider.base.colType = 9;
    SkelAnime_Update(&this->skelAnime);
    sCape->backPush = -9.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -2.0f;
    sCape->minDist = 13.0f;

    if (this->unk_1C2 == 0) {
        if (this->timers[0] == 0) {
            this->unk_1C2 = 1;
            Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBlockReleaseAnim, 0.0f);
            this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBlockReleaseAnim);
            SkelAnime_Update(&this->skelAnime);
            sCape->attachShouldersTimer = 15.0f;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
        }
    } else {
        sCape->sideSwayMagnitude = -13.0f;

        if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
            BossGanon_SetupWait(this, play);
        }
    }

    this->actor.world.pos.x += this->actor.velocity.x;
    this->actor.world.pos.y += this->actor.velocity.y;
    this->actor.world.pos.z += this->actor.velocity.z;

    Math_ApproachZeroF(&this->actor.velocity.x, 1.0f, 0.5f);
    Math_ApproachZeroF(&this->actor.velocity.y, 1.0f, 0.5f);
    Math_ApproachZeroF(&this->actor.velocity.z, 1.0f, 0.5f);
}

void BossGanon_SetupHitByLightBall(BossGanon* this, PlayState* play) {
    s16 i;

    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicHitAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBigMagicHitAnim, 0);
    this->timers[0] = 70;
    sCape->attachRightArmTimer = sCape->attachLeftArmTimer = 0;

    for (i = 1; i < 15; i++) {
        this->unk_4E4[i] = D_808E4C58[i];
    }

    this->unk_2E6 = 80;
    this->unk_2E8 = 0;
    this->actionFunc = BossGanon_HitByLightBall;
    this->actor.velocity.x = this->actor.velocity.z = 0.0f;
    this->unk_1C2 = 0;
    this->unk_1A6 = 15;
    this->unk_508 = 6.0f;
}

void BossGanon_HitByLightBall(BossGanon* this, PlayState* play) {
    s16 i;
    Vec3f sp50;

    SkelAnime_Update(&this->skelAnime);

    if (this->unk_1C2 == 0) {
        BossGanonEff_SpawnShock(play, 1500.0f, GDF_SHOCK_DORF_YELLOW);

        if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
            this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLightArrowWaitAnim);
            Animation_MorphToLoop(&this->skelAnime, &gGanondorfLightArrowWaitAnim, 0.0f);
            this->unk_1C2 = 1;
        }
    } else if (this->unk_1C2 == 1) {
        BossGanonEff_SpawnShock(play, 1000.0f, GDF_SHOCK_DORF_YELLOW);

        if (this->timers[0] == 0) {
            this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp3Anim);
            Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, 0.0f);
            this->unk_1C2 = 2;
            SkelAnime_Update(&this->skelAnime);
            sCape->attachShouldersTimer = 18.0f;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_RESTORE);
            this->timers[2] = 130;
        }
    } else {
        if (Animation_OnFrame(&this->skelAnime, 7.0f)) {
            for (i = 0; i < 100; i++) {
                sp50.x = Rand_CenteredFloat(25.0f);
                sp50.y = Rand_CenteredFloat(25.0f);
                sp50.z = Rand_CenteredFloat(25.0f);

                BossGanonEff_SpawnSparkle(play, &this->unk_1FC, &sp50, &sZeroVec, Rand_ZeroFloat(200.0f) + 500.0f,
                                          0x14);
            }
            Audio_PlayActorSound2(&this->actor, NA_SE_PL_WALK_WATER2);
        }

        if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
            BossGanon_SetupWait(this, play);
        }
    }

    this->actor.velocity.y = Math_SinS(this->unk_1A2 * 1500) * 2.0f;
    this->actor.world.pos.y += this->actor.velocity.y;
}

void BossGanon_SetupVulnerable(BossGanon* this, PlayState* play) {
    s16 i;

    if (this->actionFunc != BossGanon_Vulnerable) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
        this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLightArrowHitAnim);
        Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfLightArrowHitAnim, 0.0f);
        sCape->attachRightArmTimer = sCape->attachLeftArmTimer = 0;
        this->actionFunc = BossGanon_Vulnerable;

        this->actor.velocity.x = 0.0f;
        this->actor.velocity.y = 0.0f;
        this->actor.velocity.z = 0.0f;

        this->unk_1C2 = 0;
        sCape->backPush = -4.0f;
        sCape->backSwayMagnitude = 0.75f;
        sCape->sideSwayMagnitude = -3.0f;
        sCape->minDist = 20.0f;

        for (i = 0; i < 10; i++) {
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_1FC.x, this->unk_1FC.y,
                               this->unk_1FC.z, Rand_CenteredFloat(0x8000),
                               (s16)Rand_CenteredFloat(0x8000) + this->actor.yawTowardsPlayer, 0, 0xC8 + i);
        }

        this->unk_1A4 = 0;
        this->unk_288 = 0.0f;
        this->unk_290 = 0.0f;
        this->unk_284 = 0.0f;
        this->unk_28C = 0.0f;
    }
}

void BossGanon_Vulnerable(BossGanon* this, PlayState* play) {
    s16 i;
    Vec3f sp40;

    if (this->timers[3] == 0) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }

    SkelAnime_Update(&this->skelAnime);

    this->envLightMode = 1;
    this->actor.world.pos.y += this->actor.velocity.y;

    if (this->unk_1A4 < 0x28) {
        Math_ApproachF(&this->unk_508, 4.0f, 0.1f, 0.1f);
    }

    if ((this->unk_1A4 >= 0x28) && (this->unk_1A4 < 0x37)) {
        Math_ApproachF(&this->unk_508, 0.0f, 1.0f, 0.5f);
    }

    if (this->unk_1A4 >= 0x37) {
        Math_ApproachF(&this->unk_508, 5.0f, 0.1f, 0.15f);
        this->shockGlow = true;
    }

    switch (this->unk_1C2) {
        case 0:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 1;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLightEnergyHitAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfLightEnergyHitAnim, 0.0f);
            }
            break;

        case 1:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 2;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDownedAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDownedAnim, 0.0f);
            }
            break;

        case 2:
            sCape->minDist = 0.0f;
            this->actor.velocity.y = this->actor.velocity.y - 0.5f;

            if (this->actor.world.pos.y < 40.0f) {
                this->actor.world.pos.y = 40.0f;
                this->actor.velocity.y = 0.0f;
                this->unk_1C2 = 3;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLandAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfLandAnim, 0.0f);
                this->timers[0] = 70;
                this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
            }
            break;

        case 3:
            if (this->timers[0] == 68) {
                this->unk_19F = 1;
            }

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 4;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfVulnerableAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfVulnerableAnim, 0.0f);
            }
            break;

        case 4:
            if (Animation_OnFrame(&this->skelAnime, 5.0f)) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DOWN);
            }

            if (this->timers[0] == 0) {
                this->unk_1C2 = 5;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp1Anim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp1Anim, 0.0f);
                this->unk_2D4 = 80;

                for (i = 1; i < 15; i++) {
                    this->unk_4E4[i] = Rand_ZeroFloat(10.0f);
                }

                this->unk_2E6 = 80;
                this->unk_2E8 = 0;
                this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
            }
            break;

        case 5:
            BossGanonEff_SpawnShock(play, 1000.0f, GDF_SHOCK_DORF_YELLOW);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 6;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp2Anim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp2Anim, 0.0f);
                sCape->minDist = 20.0f;
                this->unk_19F = 1;
            }
            break;

        case 6:
            this->envLightMode = 0;
            Math_ApproachF(&this->actor.world.pos.y, 200.0f, 0.1f, 1000.0f);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 7;
            }
            break;

        case 7:
            this->envLightMode = 0;
            Math_ApproachF(&this->actor.world.pos.y, 150.0f, 0.05f, 30.0f);
            this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp3Anim);
            Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, 0.0f);
            this->unk_1C2 = 8;
            SkelAnime_Update(&this->skelAnime);
            sCape->attachShouldersTimer = 18.0f;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_RESTORE);
            break;

        case 8:
            this->envLightMode = 0;

            if (Animation_OnFrame(&this->skelAnime, 7.0f)) {
                for (i = 0; i < 100; i++) {
                    sp40.x = Rand_CenteredFloat(25.0f);
                    sp40.y = Rand_CenteredFloat(25.0f);
                    sp40.z = Rand_CenteredFloat(25.0f);
                    BossGanonEff_SpawnSparkle(play, &this->unk_1FC, &sp40, &sZeroVec, Rand_ZeroFloat(200.0f) + 500.0f,
                                              0x14);
                }

                Audio_PlayActorSound2(&this->actor, NA_SE_PL_WALK_WATER2);
                this->timers[3] = 50;
            }

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_SetupWait(this, play);
            }
            break;
    }
}

void BossGanon_SetupDamaged(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDamageAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDamageAnim, 0.0f);
    this->actionFunc = BossGanon_Damaged;
}

void BossGanon_Damaged(BossGanon* this, PlayState* play) {
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;

    SkelAnime_Update(&this->skelAnime);

    if (this->unk_1A4 >= 0x37) {
        Math_ApproachF(&this->unk_508, 5.0f, 0.1f, 0.15f);
        this->shockGlow = true;
    }

    if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
        this->actionFunc = BossGanon_Vulnerable;
        this->unk_1C2 = 4;
        this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfVulnerableAnim);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfVulnerableAnim, 0.0f);
    }
}

void BossGanon_UpdateDamage(BossGanon* this, PlayState* play) {
    s16 i;
    s16 j;
    ColliderInfo* acHitInfo;

    if (this->collider.base.acFlags & 2) {
        this->unk_2D4 = 2;
        this->collider.base.acFlags &= ~2;
        acHitInfo = this->collider.info.acHitInfo;
        // SoH: "Play as Ganondorf". His attacks carry every damage type, including the hookshot flag that the
        // vulnerable check below ignores, so treat them as a strong sword hit instead.
        u8 byPlayerDorf = BossGanon_IsPlayerAttack(this->collider.base.ac);

        if ((this->actionFunc == BossGanon_HitByLightBall) || (this->actionFunc == BossGanon_ChargeBigMagic)) {
            if (acHitInfo->toucher.dmgFlags & 0x2000) {
                BossGanon_SetupVulnerable(this, play);
                this->timers[2] = 0;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DAMAGE1);
                this->unk_1A6 = 15;
            }
        } else if ((this->actionFunc == BossGanon_Vulnerable) && (this->unk_1C2 >= 3)) {
            if (!(acHitInfo->toucher.dmgFlags & 0x80) || byPlayerDorf) {
                u8 hitWithSword = false;
                u8 damage;
                Vec3f sp50;
                u32 flags;

                for (i = 0; i < 30; i++) {
                    sp50.x = Rand_CenteredFloat(20.0f);
                    sp50.y = Rand_CenteredFloat(20.0f);
                    sp50.z = Rand_CenteredFloat(20.0f);
                    BossGanonEff_SpawnSparkle(play, &this->unk_1FC, &sp50, &sZeroVec, Rand_ZeroFloat(200.0f) + 500.0f,
                                              0x1E);
                }

                damage = flags = byPlayerDorf ? 4 : CollisionCheck_GetSwordDamage(acHitInfo->toucher.dmgFlags, play);

                if (flags == 0) {
                    damage = 2;
                } else {
                    hitWithSword = true;
                }

                if (((s8)this->actor.colChkInfo.health >= 3) || hitWithSword) {
                    this->actor.colChkInfo.health -= damage;
                }

                for (i = 0; i < ARRAY_COUNT(sCape->strands); i++) {
                    for (j = 1; j < 12; j++) {
                        sCape->strands[i].velocities[j].x = Rand_CenteredFloat(15.0f);
                        sCape->strands[i].velocities[j].z = Rand_CenteredFloat(15.0f);
                    }
                }

                if ((s8)this->actor.colChkInfo.health <= 0) {
                    BossGanon_SetupDeathCutscene(this, play);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DEAD);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DD_THUNDER);
                    Sfx_PlaySfxAtPos(&sZeroVec, NA_SE_EN_LAST_DAMAGE);
                    Audio_QueueSeqCmd(0x100100FF);
                    this->screenFlashTimer = 4;
                    GameInteractor_ExecuteOnBossDefeat(&this->actor);
                } else {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DAMAGE2);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_CUTBODY);
                    BossGanon_SetupDamaged(this, play);
                    this->unk_1A6 = 15;
                    sCape->tearTimer = 1;
                }
            }
        } else if (acHitInfo->toucher.dmgFlags & 0x1F8A4) {
            Audio_PlayActorSound2(&this->actor, 0);

            for (i = 0; i < ARRAY_COUNT(sCape->strands); i++) {
                for (j = 1; j < 12; j++) {
                    sCape->strands[i].velocities[j].x = Rand_CenteredFloat(15.0f);
                    sCape->strands[i].velocities[j].z = Rand_CenteredFloat(15.0f);
                }
            }
        }
    }
}

static f32 D_808E4D44[] = {
    1.0f, 3.0f, 0.0f, 7.0f, 13.0f, 4.0f, 6.0f, 11.0f, 5.0f, 2.0f, 8.0f, 14.0f, 10.0f, 12.0f, 9.0f,
};

static void BossGanon_Update_Impl(Actor* thisx, PlayState* play2) {
    BossGanon* this = (BossGanon*)thisx;
    PlayState* play = play2;
    f32 legRotX;
    f32 legRotY;
    f32 legRotZ;
    Player* player = GET_PLAYER(play);
    s16 i;
    f32 sin;
    f32 cos;
    Vec3f shardPos;
    Vec3f shardVel;
    Vec3f spE8;
    s16 i2;
    s16 j;
    Vec3f spD8;
    Vec3f platformCheckPos;
    Actor* explosive;
    Vec3f spBC;
    Vec3f spB0;
    Vec3f platCheckPosBomb;
    Actor* prop;
    BgGanonOtyuka* platform;
    f32 targetLensFlareScale;
    f32 xOffset;
    f32 zOffset;

    if ((this->actionFunc != BossGanon_IntroCutscene) && (this->actionFunc != BossGanon_DeathAndTowerCutscene)) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    } else {
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animBankIndex].segment);
    }

    if (this->windowShatterState != GDF_WINDOW_SHATTER_OFF) {
        BossGanon_ShatterWindows(this->windowShatterState);
        shardVel.y = 0.0f;

        for (i = 0; i < 10; i++) {
            shardPos.y = Rand_ZeroFloat(240.0f) + 20.0f;

            if (Rand_ZeroOne() < 0.5f) {
                shardPos.x = 463;
                shardPos.z = Rand_ZeroFloat(463.0f);

                shardVel.x = Rand_ZeroFloat(2.0f);
                shardVel.z = Rand_ZeroFloat(1.0f);
            } else {
                shardPos.z = 463;
                shardPos.x = Rand_ZeroFloat(463.0f);

                shardVel.z = Rand_ZeroFloat(2.0f);
                shardVel.x = Rand_ZeroFloat(1.0f);
            }

            BossGanonEff_SpawnWindowShard(play, &shardPos, &shardVel, Rand_ZeroFloat(0.075f) + 0.08f);
        }
    }

    this->collider.base.colType = 3;
    sCape->gravity = -3.0f;
    this->shockGlow = false;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->unk_1A2++;
    this->unk_1A4++;

    // block players attack if hes shooting something
    if ((this->actionFunc == BossGanon_Wait) || (this->actionFunc == BossGanon_Block)) {
        if (player->unk_A73 != 0) {
            BossGanon_SetupBlock(this, play);
        }
    }

    this->actionFunc(this, play);

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    if (this->unk_1A6 != 0) {
        this->unk_1A6--;
    }

    if (this->unk_2D4 != 0) {
        this->unk_2D4--;
    }

    if (this->unk_2E8 != 0) {
        this->unk_2E8--;
    }

    if (this->unk_2E6 != 0) {
        this->unk_2E6--;
    }

    if (this->unk_19C != 0) {
        this->unk_19C--;
    }

    if (this->csState == 0) {
        BossGanon_UpdateDamage(this, play);
        BossGanon_SetColliderPos(&this->unk_1FC, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);

        if (this->unk_2D4 == 0) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);

            if ((this->actionFunc != BossGanon_HitByLightBall) && (this->actionFunc != BossGanon_Vulnerable) &&
                (this->actionFunc != BossGanon_Damaged)) {
                CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
            }
        }
    }

    if (this->legSwayEnabled) {
        sin = Math_SinS(-this->actor.shape.rot.y);
        cos = Math_CosS(-this->actor.shape.rot.y);

        legRotX = ((this->actor.velocity.z * sin) + (cos * this->actor.velocity.x)) * 300.0f;
        legRotY = ((-sin * this->actor.velocity.x) + (cos * this->actor.velocity.z)) * 300.0f;
        legRotZ = (Math_SinS(this->unk_1A2 * 2268) * -500.0f) - 500.0f;
    } else {
        legRotY = legRotX = legRotZ = 0.0f;
    }

    this->legSwayEnabled = false;

    Math_SmoothStepToF(&this->legRot.x, legRotX, 1.0f, 600.0f, 0.0f);
    Math_SmoothStepToF(&this->legRot.y, legRotY, 1.0f, 600.0f, 0.0f);
    Math_SmoothStepToF(&this->legRot.z, legRotZ, 1.0f, 100.0f, 0.0f);

    if (this->timers[2] == 1) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_LAUGH);
    }

    if (this->timers[2] == 100) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_ST_LAUGH);
        this->timers[2] = 0;
    }

    if ((this->unk_2E6 != 0) || (this->unk_2E8 != 0)) {
        for (i = 1; i < ARRAY_COUNT(this->unk_49C); i++) {
            if (this->unk_4E4[i] != 0) {
                this->unk_4E4[i]--;
                Math_ApproachF(&this->unk_49C[i], this->unk_508, 1.0f, 2.0f);
            } else {
                Math_ApproachZeroF(&this->unk_49C[i], 1.0f, 0.2f);
            }
        }

        // player hit, spawn shock and play sound
        if (this->unk_2E8 != 0) {
            Sfx_PlaySfxAtPos(&player->actor.projectedPos, NA_SE_PL_SPARK - SFX_FLAG);
            BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_YELLOW);
        }
    }

    if (this->unk_19F != 0) {
        this->unk_19F = 0;
        spE8 = this->actor.world.pos;
        spE8.y = 0.0f;
        BossGanonEff_SpawnDustDark(play, &spE8, 0.2, 0.7f);
        BossGanonEff_SpawnDustDark(play, &spE8, 0.3f, 0.8f);
    }

    if (this->unk_26C != 0) {
        this->unk_26C--;

        if (this->unk_26C == 0) {
            BossGanonEff_SpawnLightning(play, 1.0f, 0.0f, 0.0f);
        }

        BossGanonEff_SpawnLightning(play, 1.0f, D_808E4D44[this->unk_26C] * (M_PI / 5) + this->unk_270,
                                    Rand_CenteredFloat(M_PI / 5) + (M_PI / 2));
    }

    // see if light ball hit and should knock platform down?
    if ((this->unk_19C != 0) && (this->unk_19E < 4)) {
        if ((this->unk_19A == 0) && (this->unk_19C == 20)) {
            this->unk_19A = 1;
            platformCheckPos.x = -180.0f;
            platformCheckPos.y = 0.0f;

            for (i2 = 0; i2 < 4; i2++) {
                for (j = 0, platformCheckPos.z = -180.0f; j < 4; j++) {
                    BossGanon_CheckFallingPlatforms(this, play, &platformCheckPos);
                    platformCheckPos.z += 120.0f;
                }

                platformCheckPos.x += 120.0f;
            }
        } else if (this->unk_19C < 30) {
            spD8.x = 0.0f;
            spD8.y = 0.0f;
            spD8.z = 15.0f * (30.0f - this->unk_19C);

            Matrix_RotateY(Rand_ZeroFloat(6.2831855f), MTXMODE_NEW);
            Matrix_MultVec3f(&spD8, &platformCheckPos);

            this->unk_19E += BossGanon_CheckFallingPlatforms(this, play, &platformCheckPos);
        }
    }

    // see if a bomb exploded near a group of platforms and if they should fall
    explosive = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;

    while (explosive != NULL) {
        if (explosive->params != BOMB_EXPLOSION) {
            explosive = explosive->next;
        } else {
            for (i = 0; i < 8; i++) {
                spBC.x = 0.0f;
                spBC.y = 0.0f;
                spBC.z = 60.0f;

                Matrix_RotateY(i * (M_PI / 4), MTXMODE_NEW);
                Matrix_MultVec3f(&spBC, &spB0);

                platCheckPosBomb.x = explosive->world.pos.x + spB0.x;
                platCheckPosBomb.y = explosive->world.pos.y;
                platCheckPosBomb.z = explosive->world.pos.z + spB0.z;

                BossGanon_CheckFallingPlatforms(this, play, &platCheckPosBomb);
            }

            explosive = explosive->next;
        }
    }

    BossGanon_UpdateEffects(play);

    prop = play->actorCtx.actorLists[ACTORCAT_PROP].head;

    // if a platform is lit up, change the room lighting
    while (prop != NULL) {
        if (prop->id != ACTOR_BG_GANON_OTYUKA) {
            prop = prop->next;
        } else {
            platform = (BgGanonOtyuka*)prop;

            if (platform->flashState != 0) {
                this->envLightMode = 1;
                break;
            }

            prop = prop->next;
        }
    }

    play->envCtx.unk_BF = 0;
    play->envCtx.unk_BE = 0;
    play->envCtx.unk_DC = 2;

    switch (this->envLightMode) {
        case -1:
            break;
        case 0:
            Math_ApproachF(&play->envCtx.unk_D8, 0.0f, 1.0f, 0.02f);
            break;
        case 1:
            play->envCtx.unk_BD = 1;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.1f);
            break;
        case 2:
            play->envCtx.unk_BD = 1;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.02f);
            break;
        case 3:
            play->envCtx.unk_BD = 3;
            play->envCtx.unk_D8 = 1.0f;
            break;
        case 35:
            play->envCtx.unk_BD = 0;
            play->envCtx.unk_D8 = 1.0f;
            break;
        case 4:
            play->envCtx.unk_BD = 4;
            play->envCtx.unk_D8 = 1.0f;
            break;
        case 5:
            play->envCtx.unk_BE = 5;
            play->envCtx.unk_BD = 3;
            Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.075f);
            break;
        case 6:
            play->envCtx.unk_BE = 5;
            play->envCtx.unk_D8 = 0.0f;
            break;
        case 65:
            play->envCtx.unk_BE = 3;
            play->envCtx.unk_BD = 6;
            Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.05f);
            break;
        case 7:
            play->envCtx.unk_BE = 7;
            play->envCtx.unk_D8 = 0.0f;
            break;
        case 75:
            play->envCtx.unk_BE = 4;
            play->envCtx.unk_BD = 8;
            Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.05f);
            break;
        case 8:
            play->envCtx.unk_BE = 3;
            play->envCtx.unk_BD = 9;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.05f);
            break;
        case 9:
            play->envCtx.unk_BE = 3;
            play->envCtx.unk_BD = 0xA;
            Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.05f);
            break;
        case 10:
            play->envCtx.unk_BE = 3;
            play->envCtx.unk_BD = 0xB;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.05f);
            this->unk_1A4 = 0;
            break;
        case 11:
            play->envCtx.unk_BE = 0xC;
            play->envCtx.unk_BD = 0xB;
            Math_ApproachF(&play->envCtx.unk_D8, (Math_CosS(this->unk_1A4 * 0x1800) * 0.5f) + 0.5f, 1.0f, 1.0f);
            break;
        case 12:
            play->envCtx.unk_BE = 0xC;
            play->envCtx.unk_BD = 3;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.05f);
            break;
        case 13:
            play->envCtx.unk_BD = 0xD;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.025f);
            break;
        case 14:
            play->envCtx.unk_BD = 0xE;
            play->envCtx.unk_D8 = 1.0f;
            break;
        case 15:
            play->envCtx.unk_BE = 0xE;
            play->envCtx.unk_BD = 0xF;
            Math_ApproachF(&play->envCtx.unk_D8, 1.0f, 1.0f, 0.01f);
            break;
        case 16:
            play->envCtx.unk_BE = 0x10;
            play->envCtx.unk_BD = 0xF;
            Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.05f);
            break;
        case 20:
            play->envCtx.unk_BE = 2;
            play->envCtx.unk_BD = 1;
            break;
        default:
            break;
    }

    this->envLightMode = 0;

    if (this->whiteFillAlpha != 0) {
        play->envCtx.screenFillColor[3] = (s8)(u8)this->whiteFillAlpha;
        play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] = 255;
        play->envCtx.fillScreen = true;
    } else if (this->screenFlashTimer != 0) {
        play->envCtx.fillScreen = true;
        play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] = 255;

        play->envCtx.screenFillColor[3] = ((this->screenFlashTimer % 2) != 0) ? 100 : 0;

        this->screenFlashTimer--;
    } else {
        play->envCtx.fillScreen = play->envCtx.screenFillColor[3] = 0;
    }

    if (this->lensFlareTimer != 0) {
        this->lensFlareTimer--;

        if (this->lensFlareMode == 1) {
            targetLensFlareScale = 40.0f;
        } else if (this->lensFlareMode == 4) {
            targetLensFlareScale = 25.0f;
        } else {
            targetLensFlareScale = 10.0f;
        }

        Math_ApproachF(&this->lensFlareScale, targetLensFlareScale, 0.3f, 10.0f);
    } else {
        Math_ApproachZeroF(&this->lensFlareScale, 1.0f, 5.0f);

        if (this->lensFlareScale == 0.0f) {
            this->lensFlareMode = 0;
        }
    }

    if (this->lensFlareMode != 0) {
        gCustomLensFlareOn = true;

        if (this->lensFlareMode == 1) {
            gCustomLensFlarePos = this->actor.world.pos;
        }

        gLensFlareScale = this->lensFlareScale;
        gLensFlareColorIntensity = 10.0f;
        gLensFlareScreenFillAlpha = 0;
    } else {
        gCustomLensFlareOn = false;
    }

    if (this->unk_274 != 0) {
        i = this->unk_274 - 1;

        this->unk_278.x = this->unk_2EC[0].x;
        this->unk_278.y = this->unk_2EC[0].y + 50.0f + 30.0f;
        this->unk_278.z = this->unk_2EC[0].z;

        xOffset = (sinf(i * 1.2566371f) * 600.0f);
        zOffset = (cosf(i * 1.2566371f) * 600.0f);

        // 5 or 6 light balls that go into the charge. not the same as the ones that he throws
        Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_1FC.x + xOffset,
                           this->unk_1FC.y, this->unk_1FC.z + zOffset, 0, (s16)(i * 13107.2f) + 0x6000, 0, 0xFA + i);
        this->unk_274 = 0;
    }
}

s32 BossGanon_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    BossGanon* this = (BossGanon*)thisx;

    switch (limbIndex) {
        case 10:
            if (this->useOpenHand) {
                *dList = gGanondorfRightHandOpenDL;
            }
            break;

        case 20:
            rot->y += this->legRot.x + this->legRot.z;
            rot->z += this->legRot.y;
            break;

        case 21:
            if (this->legRot.y > 0.0f) {
                rot->z += this->legRot.y;
            }
            break;

        case 22:
            rot->y += this->legRot.x + this->legRot.z;
            rot->z += this->legRot.y;
            break;

        case 23:
            rot->y += this->legRot.x - this->legRot.z;
            rot->z += this->legRot.y;
            break;

        case 24:
            if (this->legRot.y > 0.0f) {
                rot->z += this->legRot.y;
            }
            break;

        case 25:
            rot->y += this->legRot.x - this->legRot.z;
            rot->z += this->legRot.y;
            break;

        default:
            break;
    }

    // SoH: Phantom Ganon skin, Ganondorf's own model isn't drawn
    if (sGdfPgCapture) {
        *dList = NULL;
    }

    return 0;
}

// SoH: "Play as Ganondorf", the lowest point of his model from his last draw, from a few points on the outside of his
// hands, shins and feet (measured on his model, in each limb's own space): toes, heels, soles, knees and knuckles. The
// ground offset keeps that point on the floor, so no animation (kneeling, the heavy landing, getting up) sinks him in.
static const Vec3f sGdfContactPoints[6][8] = {
    // left hand (limb 6)
    { { 584, -529, 345 }, { 159, 303, -299 }, { 1174, 315, 207 }, { 977, -387, -322 }, { 410, 166, 563 },
      { 162, -349, -243 }, { 1142, -262, 373 }, { 958, 124, -426 } },
    // right hand (limb 10)
    { { 584, -529, -345 }, { 159, 303, 299 }, { 1174, 315, -207 }, { 977, -387, 322 }, { 410, 166, -563 },
      { 162, -349, 243 }, { 1142, -262, -373 }, { 958, 124, 426 } },
    // left shin (limb 21): the knee end
    { { 1800, -737, 17 }, { 575, 971, -45 }, { 71, -645, -34 }, { 2170, 968, -20 }, { 2224, 123, 737 },
      { 2224, 123, -711 }, { 392, 27, 671 }, { 392, 27, -595 } },
    // left foot (limb 22): heel, sole and toes
    { { -391, -592, -565 }, { 208, -592, 1277 }, { 2324, -351, -313 }, { -539, -586, 773 }, { 373, -592, -565 },
      { 596, -586, 773 }, { 2324, -351, 311 }, { -324, -156, 1160 } },
    // right shin (limb 24)
    { { 1800, -737, -18 }, { 575, 971, 44 }, { 71, -645, 34 }, { 2170, 968, 19 }, { 2224, 123, -737 },
      { 2224, 123, 710 }, { 392, 27, -671 }, { 392, 27, 594 } },
    // right foot (limb 25)
    { { -372, -592, -565 }, { 237, -592, 1277 }, { 2324, -351, -312 }, { -596, -586, 772 }, { 391, -592, -565 },
      { -273, -156, 1161 }, { 2324, -351, 312 }, { 538, -586, 773 } },
};
static f32 sGdfStandOffset = 0.0f;   // his ground offset standing still on his feet
static s32 sGdfStandOffsetValid = false;
static s16 sGdfFastGroundTimer = 0;   // frames left where the ground offset follows his pose at once
static f32 sGdfMeshLowY = 0.0f;       // world height of his lowest point, last draw
static f32 sGdfMeshLowYNext = 1.0e9f; // being gathered during this draw
static u8 sGdfMeshLowValid = false;
// Walking about on his own during a cutscene (BossGanon_Player_UpdateRoam)
static u8 sGdfRoaming = false;
static f32 sGdfRoamSpeed = 0.0f;
static s16 sGdfRoamYaw = 0;
static f32 sGdfRoamFloorY = 0.0f;
// The trident throw (BossGanon_Player_TridentSpin / TridentThrow)
#define GDF_TRIDENT_IN_HAND 0
#define GDF_TRIDENT_OVERHEAD 1
#define GDF_TRIDENT_OUT 2
#define GDF_TRIDENT_RETURNING 3
static u8 sGdfTridentState = GDF_TRIDENT_IN_HAND;
static Vec3f sGdfTridentPos;
static Vec3f sGdfTridentVel;
static s16 sGdfTridentSpin = 0;
// The hidden move: the trident spun for 5 s raises a ring of fireballs around him, scattered at the Links on the throw
#define GDF_FIRE_RING_COUNT 8
#define GDF_FIRE_RING_FRAMES 100
#define GDF_FIRE_RING_COST 20
static Actor* sGdfFireRing[GDF_FIRE_RING_COUNT];
static Actor* sGdfTridentTarget = NULL;
static s16 sGdfTridentTimer = 0;
static f32 sGdfTridentTravel = 0.0f;

// SoH: "Play as Ganondorf", his head (limb 18) from his last draw: Link's head, hat and focus while Link is hidden
static Vec3f sGdfPlayerHeadPos;
static u8 sGdfPlayerHeadValid = false;

// The throat grab: whoever he holds hangs from his right fist, placed as the hand is drawn (see the grab, below)
static void BossGanon_Player_GrabFollowHand(BossGanon* this);

void BossGanon_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static s8 bodyPartLimbMap[] = {
        -1, -1, 1, -1, 3, 4, 5, -1, 6, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, 2, 12, 13, 14, 9, 10, 11, -1, -1, -1, -1,
    };
    static Vec3f D_808E4DA0 = { -500.0f, 200.0f, -300.0f };
    static Vec3f D_808E4DAC = { -500.0f, 200.0f, 300.0f };
    static Vec3f D_808E4DB8 = { 0.0f, 0.0f, 0.0f };
    static Vec3f D_808E4DC4 = { 0.0f, 0.0f, 0.0f };
    static Vec3f D_808E4DD0 = { 0.0f, 0.0f, 0.0f };
    static Vec3f D_808E4DDC = { 1300.0f, 0.0f, 0.0f };
    static Vec3f D_808E4DE8 = { 600.0f, 420.0f, 100.0f };
    s8 bodyPart;
    BossGanon* this = (BossGanon*)thisx;

    bodyPart = bodyPartLimbMap[limbIndex];
    if (bodyPart >= 0) {
        Matrix_MultVec3f(&D_808E4DB8, &this->unk_2EC[bodyPart]);
    }

    if (GDF_IS_PLAYER(this)) {
        s32 set = -1;

        switch (limbIndex) {
            case 6: set = 0; break;
            case 10: set = 1; break;
            case 21: set = 2; break;
            case 22: set = 3; break;
            case 24: set = 4; break;
            case 25: set = 5; break;
        }
        if (set >= 0) {
            Vec3f p;
            s32 k;

            for (k = 0; k < 8; k++) {
                Matrix_MultVec3f(&sGdfContactPoints[set][k], &p);
                sGdfMeshLowYNext = MIN(sGdfMeshLowYNext, p.y);
            }
        }
    }

    if ((limbIndex == 18) && GDF_IS_PLAYER(this)) {
        static Vec3f sHeadCenter = { 800.0f, 0.0f, 0.0f }; // a little way up the head from the neck joint

        Matrix_MultVec3f(&sHeadCenter, &sGdfPlayerHeadPos);
        sGdfPlayerHeadValid = true;
    }

    // SoH: Phantom Ganon skin, every limb's matrix for posing his skeleton
    if (sGdfPgCapture && (limbIndex >= 1) && (limbIndex <= 25)) {
        Matrix_Get(&sGdfLimbMtx[limbIndex - 1]);
    }

    // SoH: "Play as Ganondorf", his hands hold the sword and the sheath
    if (GDF_IS_PLAYER(this)) {
        if (limbIndex == 6) {
            Matrix_Get(&this->playerLeftHandMtx);
        } else if (limbIndex == 10) {
            Matrix_Get(&this->playerRightHandMtx);
            this->playerHandMtxValid = true;
            BossGanon_Player_GrabFollowHand(this);
        }
    }

    if (limbIndex == 2) {
        Matrix_MultVec3f(&D_808E4DB8, &this->unk_1FC);
    } else if (limbIndex == 19) {
        Matrix_MultVec3f(&D_808E4DB8, &this->actor.focus.pos);
    } else if (limbIndex == 11) {
        OPEN_DISPS(play->state.gfxCtx);

        Matrix_MultVec3f(&D_808E4DB8, &this->unk_208);
        if (!sGdfPgCapture) {
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfEyesDL));
        }

        CLOSE_DISPS(play->state.gfxCtx);
    } else if (limbIndex == 6) {
        Matrix_MultVec3f(&D_808E4DC4, &this->unk_238);
    } else if (limbIndex == 10) {
        Matrix_MultVec3f(&D_808E4DD0, &this->unk_22C);

        if (this->unk_25C == 0) {
            Matrix_MultVec3f(&D_808E4DDC, &this->unk_260);
        }

        this->unk_25C = 0;

        if (this->triforceType == GDF_TRIFORCE_DORF) {
            Matrix_MultVec3f(&D_808E4DE8, &this->triforcePos);
        }
    } else if (limbIndex == 4) {
        Vec3f sp28 = D_808E4DA0;

        if (this->unk_198 == 1) {
            sp28.x += -300.0f;
            sp28.y += -300.0f;
            sp28.z += 700.0f;
        } else if (this->unk_198 == 2) {
            sp28.x += -300.0f;
            sp28.z += 700.0f;
        }

        Matrix_MultVec3f(&sp28, &this->unk_220);
    } else if (limbIndex == 8) {
        Vec3f sp1C = D_808E4DAC;

        if (this->unk_198 == 1) {
            sp1C.x += -300.0f;
            sp1C.y += -300.0f;
            sp1C.z += -700.0f;
        } else if (this->unk_198 == 2) {
            sp1C.x += -300.0f;
            sp1C.y += 100.0f;
            sp1C.z += -700.0f;
        }

        Matrix_MultVec3f(&sp1C, &this->unk_214);
    }
}

void BossGanon_InitRand(s32 seedInit0, s32 seedInit1, s32 seedInit2) {
    sSeed1 = seedInit0;
    sSeed2 = seedInit1;
    sSeed3 = seedInit2;
}

f32 BossGanon_RandZeroOne(void) {
    // Wichmann-Hill algorithm
    f32 randFloat;

    sSeed1 = (sSeed1 * 171) % 30269;
    sSeed2 = (sSeed2 * 172) % 30307;
    sSeed3 = (sSeed3 * 170) % 30323;

    randFloat = (sSeed1 / 30269.0f) + (sSeed2 / 30307.0f) + (sSeed3 / 30323.0f);

    while (randFloat >= 1.0f) {
        randFloat -= 1.0f;
    }

    return fabsf(randFloat);
}

void BossGanon_DrawShock(BossGanon* this, PlayState* play) {
    s32 pad;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s16 i;

    OPEN_DISPS(gfxCtx);

    if ((this->unk_2E8 != 0) || (this->unk_2E6 != 0)) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);

        if (this->unk_2E8 != 0) {
            Player* player = GET_PLAYER(play);

            for (i = 0; i < ARRAY_COUNT(player->bodyPartsPos); i++) {
                FrameInterpolation_RecordOpenChild("Ganondorf Shock 0", i);

                Matrix_Translate(player->bodyPartsPos[i].x, player->bodyPartsPos[i].y, player->bodyPartsPos[i].z,
                                 MTXMODE_NEW);
                Matrix_ReplaceRotation(&play->billboardMtxF);
                Matrix_Scale(this->unk_49C[i], this->unk_49C[i], this->unk_49C[i], MTXMODE_APPLY);
                Matrix_RotateZ(Rand_CenteredFloat(M_PI), MTXMODE_APPLY);
                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);

                FrameInterpolation_RecordCloseChild();
            }
        } else {
            for (i = 1; i < 15; i++) {
                FrameInterpolation_RecordOpenChild("Ganondorf Shock 1", i);

                Matrix_Translate(this->unk_2EC[i].x, this->unk_2EC[i].y, this->unk_2EC[i].z, MTXMODE_NEW);
                Matrix_ReplaceRotation(&play->billboardMtxF);
                Matrix_Scale(this->unk_49C[i], this->unk_49C[i], this->unk_49C[i], MTXMODE_APPLY);

                if (!this->shockGlow) {
                    Matrix_RotateZ(Rand_CenteredFloat(M_PI), MTXMODE_APPLY);
                }

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

                if (this->shockGlow) {
                    gSPSegment(POLY_XLU_DISP++, 0x08,
                               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0, (this->unk_1A2 + i) * -15,
                                                  32, 64, 0, 0, 0, -15));
                    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 200, 255, 170, 255);
                    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 128);
                    gSPDisplayList(POLY_XLU_DISP++, gGanondorfShockGlowDL);
                } else {
                    gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);
                }

                FrameInterpolation_RecordCloseChild();
            }
        }
    }

    CLOSE_DISPS(gfxCtx);
}

void BossGanon_DrawHandLightBall(BossGanon* this, PlayState* play) {
    s32 pad;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 alpha;

    OPEN_DISPS(gfxCtx);

    if (this->handLightBallScale > 0.0f) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);

        if ((this->unk_1A2 % 2) != 0) {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 100, 255, 0, 0);
        }

        gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);

        Matrix_Translate(this->unk_260.x, this->unk_260.y, this->unk_260.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(this->handLightBallScale, this->handLightBallScale, this->handLightBallScale, MTXMODE_APPLY);
        Matrix_RotateZ(this->unk_258, 1);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);

        alpha = ((this->unk_1A2 % 2) != 0) ? 100 : 80;
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 155, alpha);
        Matrix_Translate(this->unk_260.x, GDF_FLOOR_Y(this), this->unk_260.z, MTXMODE_NEW);
        Matrix_Scale(this->handLightBallScale * 0.75f, 1.0f, this->handLightBallScale * 0.75f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightCoreDL);
    }

    CLOSE_DISPS(gfxCtx);
}

void BossGanon_DrawBigMagicCharge(BossGanon* this, PlayState* play) {
    s32 pad;
    f32 yRot;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s16 i;

    OPEN_DISPS(gfxCtx);

    if (this->unk_284 > 0.0f) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);

        // light flecks
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, (s8)this->unk_290);
        gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 128);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, this->unk_1A2 * -2, 0, 0x40, 0x40, 1, 0,
                                      this->unk_1A2 * 0xA, 0x40, 0x40, -2, 0, 0, 0xA));
        Matrix_Translate(this->unk_278.x, this->unk_278.y, this->unk_278.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(this->unk_28C, this->unk_28C, this->unk_28C, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightFlecksDL);

        // background circle texture
        Matrix_Translate(this->unk_278.x, this->unk_278.y, this->unk_278.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(this->unk_284, this->unk_284, this->unk_284, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 0, 100, (s8)this->unk_288);
        gSPSegment(POLY_XLU_DISP++, 0x09,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x20, 1, 0, this->unk_1A2 * -4, 0x20, 0x20, 0,
                                      0, 0, -4));
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfBigMagicBGCircleDL);

        // yellow background dot
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 170, 0, (s8)this->unk_288);
        gSPSegment(POLY_XLU_DISP++, 0x0A,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x20, 1, this->unk_1A2 * 2,
                                      this->unk_1A2 * -0x14, 0x40, 0x40, 0, 0, 2, -0x14));
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfDotDL);

        // light ball material
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 100, 0);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);

        // light ball geometry
        Matrix_Translate(this->unk_278.x, this->unk_278.y, this->unk_278.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(this->unk_2D0, this->unk_2D0, this->unk_2D0, MTXMODE_APPLY);
        Matrix_RotateZ((this->unk_1A2 * 10.0f) / 1000.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);

        BossGanon_InitRand(this->unk_1AA + 1, 0x71AC, 0x263A);
        Matrix_Translate(this->unk_278.x, this->unk_278.y, this->unk_278.z, MTXMODE_NEW);
        Matrix_RotateY((this->unk_1A2 * 10.0f) / 1000.0f, MTXMODE_APPLY);
        gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 0);

        yRot = BINANG_TO_RAD(this->actor.yawTowardsPlayer);

        for (i = 0; i < this->unk_1AC; i++) {
            FrameInterpolation_RecordOpenChild("Ganondorf Big Magic", i);

            f32 xzRot = (BossGanon_RandZeroOne() - 0.5f) * M_PI * 1.5f;

            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s8)this->unk_294[i]);
            Matrix_Push();
            Matrix_RotateY(xzRot + yRot, MTXMODE_APPLY);
            Matrix_RotateX((BossGanon_RandZeroOne() - 0.5f) * M_PI, MTXMODE_APPLY);
            Matrix_RotateZ(xzRot, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 0.0f, 50.0f, MTXMODE_APPLY);
            Matrix_Scale(4.0f, 4.0f, 1.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightRayTriDL);

            Matrix_Pop();

            FrameInterpolation_RecordCloseChild();
        }
    }

    CLOSE_DISPS(gfxCtx);
}

void BossGanon_DrawTriforce(BossGanon* this, PlayState* play) {
    s32 pad;

    if (this->fwork[GDF_TRIFORCE_PRIM_A] > 0.0f) {
        OPEN_DISPS(play->state.gfxCtx);

        Matrix_Push();

        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, (u8)this->fwork[GDF_TRIFORCE_PRIM_B],
                        (s8)this->fwork[GDF_TRIFORCE_PRIM_A]);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, (u8)this->fwork[GDF_TRIFORCE_ENV_G], 0, 128);

        if (this->triforceType == GDF_TRIFORCE_PLAYER) {
            Player* player = GET_PLAYER(play);

            this->triforcePos = player->bodyPartsPos[12];

            this->triforcePos.x += -0.6f;
            this->triforcePos.y += 3.0f;
            this->triforcePos.z += -2.0f;
        } else if (this->triforceType == GDF_TRIFORCE_ZELDA) {
            this->triforcePos = sZelda->unk_31C;

            this->triforcePos.y += 1.8f;
            this->triforcePos.z += 4.0f;
        }

        Matrix_Translate(this->triforcePos.x, this->triforcePos.y, this->triforcePos.z, MTXMODE_NEW);

        if (this->triforceType == GDF_TRIFORCE_PLAYER) {
            Matrix_RotateX(-1.4f, MTXMODE_APPLY);
            Matrix_RotateZ(4.0f, MTXMODE_APPLY);
        } else if (this->triforceType == GDF_TRIFORCE_ZELDA) {
            Matrix_RotateY(1.5f, 1);
            Matrix_RotateX(1.1f, 1);
            Matrix_RotateZ(-0.99999994f, MTXMODE_APPLY);
        } else {
            Matrix_ReplaceRotation(&play->billboardMtxF);
        }

        Matrix_Scale(this->fwork[GDF_TRIFORCE_SCALE], this->fwork[GDF_TRIFORCE_SCALE], 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfTriforceDL));

        Matrix_Pop();

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void BossGanon_DrawDarkVortex(BossGanon* this, PlayState* play) {
    s32 pad;

    if (this->fwork[GDF_VORTEX_ALPHA] > 0.0f) {
        OPEN_DISPS(play->state.gfxCtx);

        Matrix_Push();
        gDPPipeSync(POLY_XLU_DISP++);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, this->unk_1A2 * -8, 0, 0x20, 0x40, 1, this->unk_1A2 * -4,
                                      this->unk_1A2 * -8, 0x20, 0x20, -8, 0, -4, -8));
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 100, 0, 200, (s8)this->fwork[GDF_VORTEX_ALPHA]);
        gDPSetEnvColor(POLY_XLU_DISP++, 130, 0, 0, 128);

        if (this->csState != 21) {
            Matrix_Translate(0.0f, 105.0f, -400.0f, MTXMODE_NEW);
            Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
        } else {
            Matrix_Translate(-50.0f, 50.0f, -150.0f, MTXMODE_NEW);
            Matrix_RotateY(M_PI / 10, MTXMODE_APPLY);
            Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
        }

        Matrix_Scale(this->fwork[GDF_VORTEX_SCALE], this->fwork[GDF_VORTEX_SCALE], this->fwork[GDF_VORTEX_SCALE],
                     MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfVortexDL));

        Matrix_Pop();

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void func_808E0254(BossGanon* this, u8* tex, f32 arg2) {
    static s16 D_808E4DF4[] = { 1, 2, 3, 3, 2, 1 };
    static s16 D_808E4E00[] = { 2, 3, 4, 4, 4, 3, 2 };
    static s16 D_808E4E10[] = { 2, 3, 4, 4, 4, 4, 3, 2 };
    static s16 D_808E4E20[] = { 2, 4, 5, 5, 6, 6, 6, 6, 5, 5, 4, 2 };
    static s16 D_808E4E38[] = { 1, -1, 1, 1, 3, 4, 1, 6, 7, 2, 9, 10, 2, 12, 13 };
    static u8 D_808E4E58[] = { 3, 2, 2, 1, 3, 3, 1, 3, 3, 1, 0, 3, 1, 0, 3 };
    s16 baseX;
    s16 index;
    s16 i;
    s16 baseY;
    s16 x;
    s16 addY;
    f32 lerpX;
    s16 y;
    f32 lerpY;
    f32 lerpZ;
    Vec3f sp68;
    Vec3f sp5C;

    for (i = 0; i < 15; i++) {
        if (arg2 == 0.0f || (y = D_808E4E38[i]) >= 0) {
            if (arg2 > 0.0f) {
                lerpX = this->unk_2EC[i].x + (this->unk_2EC[y].x - this->unk_2EC[i].x) * arg2;
                lerpY = this->unk_2EC[i].y + (this->unk_2EC[y].y - this->unk_2EC[i].y) * arg2;
                lerpZ = this->unk_2EC[i].z + (this->unk_2EC[y].z - this->unk_2EC[i].z) * arg2;

                sp68.x = lerpX - this->actor.world.pos.x;
                sp68.y = lerpY - this->actor.world.pos.y + 76 + 30 + 30;
                sp68.z = lerpZ - this->actor.world.pos.z;
            } else {

                sp68.x = this->unk_2EC[i].x - this->actor.world.pos.x;
                sp68.y = this->unk_2EC[i].y - this->actor.world.pos.y + 76 + 30 + 30;
                sp68.z = this->unk_2EC[i].z - this->actor.world.pos.z;
            }
            Matrix_MultVec3f(&sp68, &sp5C);

            sp5C.x *= 0.4f;
            sp5C.y *= 0.4f;

            baseX = (s16)(sp5C.x + 32.0f);
            baseY = (s16)sp5C.y * 64;

            if (D_808E4E58[i] == 2) {
                for (y = 0, addY = -0x180; y < 12; y++, addY += 0x40) {
                    for (x = -D_808E4E20[y]; x < D_808E4E20[y]; x++) {
                        index = baseX + x + baseY + addY;
                        if ((index >= 0) && (index < 0x1000)) {
                            tex[index] = 255;
                        }
                    }
                }
            } else if (D_808E4E58[i] == 1) {
                for (y = 0, addY = -0x100; y < 8; y++, addY += 0x40) {
                    for (x = -D_808E4E10[y]; x < D_808E4E10[y]; x++) {
                        index = baseX + x + baseY + addY;
                        if ((index >= 0) && (index < 0x1000)) {
                            tex[index] = 255;
                        }
                    }
                }
            } else if (D_808E4E58[i] == 0) {
                for (y = 0, addY = -0xC0; y < 7; y++, addY += 0x40) {
                    for (x = -D_808E4E00[y]; x < D_808E4E00[y] - 1; x++) {
                        index = baseX + x + baseY + addY;
                        if ((index >= 0) && (index < 0x1000)) {
                            tex[index] = 255;
                        }
                    }
                }
            } else {
                for (y = 0, addY = -0x80; y < 6; y++, addY += 0x40) {
                    for (x = -D_808E4DF4[y]; x < D_808E4DF4[y] - 1; x++) {
                        index = baseX + x + baseY + addY;
                        if ((index >= 0) && (index < 0x1000)) {
                            tex[index] = 255;
                        }
                    }
                }
            }
        }
    }
}

void BossGanon_GenShadowTexture(u8* tex, BossGanon* this, PlayState* play) {
    s16 addY;
    s16 baseX;
    s16 baseY;
    s16 i;
    s16 j;
    s16 y;
    s16 x;
    s16 index;
    Vec3f sp7C;
    Vec3f sp70;
    s32* ptr = (s32*)tex;

    for (i = 0; i < 64 * 64 / 4; i++, ptr++) {
        *ptr = 0;
    }

    Matrix_RotateX(1.0f, MTXMODE_NEW);

    for (i = 0; i <= 5; i++) {
        func_808E0254(this, tex, i / 5.0f);
    }

    for (i = 0; i < 12; i++) {
        for (j = 0; j < 12; j++) {
            sp7C.x = sCape->strands[i].joints[j].x - this->actor.world.pos.x;
            sp7C.y = sCape->strands[i].joints[j].y - this->actor.world.pos.y + 76.0f + 100.0f + 30.0f;
            sp7C.z = sCape->strands[i].joints[j].z - this->actor.world.pos.z;

            Matrix_MultVec3f(&sp7C, &sp70);

            sp70.x = sp70.x * 0.28f;
            sp70.y = sp70.y * 0.28f;

            baseX = (s32)(sp70.x + 32.0f);
            baseY = (s16)sp70.y * 0x40;

            if (!sCape->strands[i].torn[j]) {
                for (y = -1, addY = -0x40; y <= 1; y++, addY += 0x40) {
                    for (x = -3; x <= 3; x++) {
                        index = baseX + x + baseY + addY;
                        if (0 <= index && index < 0x1000) {
                            tex[index] = 255;
                        }
                    }
                }
            } else {
                for (y = -1, addY = -0x40; y <= 1; y++, addY += 0x40) {
                    for (x = -1; x <= 1; x++) {
                        index = baseX + x + baseY + addY;
                        if (0 <= index && index < 0x1000) {
                            tex[index] = 255;
                        }
                    }
                }
            }
        }
    }
}

void BossGanon_DrawShadowTexture(void* tex, BossGanon* this, PlayState* play) {
    s32 pad;
    f32 zOffset;
    GraphicsContext* gfxCtx = play->state.gfxCtx;

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, 50);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);

    if (this->csState < 100) {
        zOffset = (((((this->actor.world.pos.y - GDF_FLOOR_Y(this) - 10) + 70.0f) * -5.0f) / 10.0f) + 10.0f);
        Matrix_Translate(this->actor.world.pos.x, GDF_FLOOR_Y(this) + (GDF_IS_PLAYER(this) ? 1.0f : 0.0f),
                         this->actor.world.pos.z + zOffset, MTXMODE_NEW);
    } else {
        Matrix_Translate(this->actor.world.pos.x, 4102.0f, this->actor.world.pos.z - 20.0f, MTXMODE_NEW);
    }

    Matrix_Scale(0.95000005f, 1.0f, 0.95000005f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gGanondorfShadowSetupDL);
    gDPLoadTextureBlock(POLY_OPA_DISP++, tex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gSPDisplayList(POLY_OPA_DISP++, gGanondorfShadowModelDL);
    gSPInvalidateTexCache(POLY_OPA_DISP++, tex); // Shadow texture will change every frame, no use keeping it around

    CLOSE_DISPS(gfxCtx);
}

static void BossGanon_Draw_Impl(Actor* thisx, PlayState* play) {
    s32 i;
    BossGanon* this = (BossGanon*)thisx;
    void* shadowTex;

    // SoH: "Play as Ganondorf", hidden in first person and while Link is shown instead
    if (GDF_IS_PLAYER(this) && this->playerHidden) {
        BossGanon_Player_DrawCrawlFlame(this, play);
        BossGanon_DrawEffects(play);
        BossGanon_Player_DrawIntroProps(this, play);
        return;
    }

    if (GDF_IS_PLAYER(this)) {
        BossGanon_Player_DrawOrgan(this, play);
        BossGanon_Player_DrawDarkAura(this, play);
    }

    shadowTex = Graph_Alloc(play->state.gfxCtx, 64 * 64);

    OPEN_DISPS(play->state.gfxCtx);

    // Invalidate texture mask if it has changed
    if (this->windowShatterState != GDF_WINDOW_SHATTER_OFF) {
        gSPInvalidateTexCache(POLY_OPA_DISP++, sWindowShatterTex);
    }

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    if ((this->unk_1A6 & 2) != 0) {
        POLY_OPA_DISP = Gfx_SetFog(POLY_OPA_DISP, 255, 50, 0, 0, 900, 1099);
    }

    gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gGanondorfNormalEyeTex));

    // SoH: "Play as Ganondorf", Phantom Ganon skin: Ganondorf's skeleton still runs (without drawing) so everything
    // that follows his hands, head and shoulders keeps working, and Phantom Ganon is drawn in the same pose
    if (GDF_IS_PLAYER(this) && BossGanon_Player_IsPhantom()) {
        MtxF base;

        Matrix_Get(&base);
        sGdfPgCapture = true;
        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, BossGanon_OverrideLimbDraw, BossGanon_PostLimbDraw,
                                  &this->actor);
        sGdfPgCapture = false;
        BossGanon_Player_DrawPhantom(this, play, &base);
    } else {
        this->playerSpearValid = false;
        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, BossGanon_OverrideLimbDraw, BossGanon_PostLimbDraw,
                                  &this->actor);
    }

    // SoH: "Play as Ganondorf", sword stance
    if (GDF_IS_PLAYER(this)) {
        BossGanon_Player_DrawSword(this, play);
        // His lowest point, for the ground offset
        sGdfMeshLowValid = sGdfMeshLowYNext < 1.0e8f;
        if (sGdfMeshLowValid) {
            sGdfMeshLowY = sGdfMeshLowYNext;
        }
        sGdfMeshLowYNext = 1.0e9f;
    }

    this->unk_2EC[0].x = this->unk_2EC[1].x;
    this->unk_2EC[0].y = this->unk_2EC[1].y + 30.0f;
    this->unk_2EC[0].z = this->unk_2EC[1].z;

    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);

    BossGanon_DrawEffects(play);

    sCape->actor.world.pos = this->actor.world.pos;

    sCape->rightForearmPos = this->unk_214;
    sCape->leftForearmPos = this->unk_220;

    sCape->rightShoulderPos = this->unk_22C;
    sCape->leftShoulderPos = this->unk_238;

    BossGanon_DrawShock(this, play);
    BossGanon_DrawHandLightBall(this, play);
    BossGanon_DrawBigMagicCharge(this, play);
    BossGanon_DrawTriforce(this, play);
    BossGanon_DrawDarkVortex(this, play);
    if (GDF_IS_PLAYER(this)) {
        BossGanon_Player_DrawIntroProps(this, play);
    }

    BossGanon_GenShadowTexture(shadowTex, this, play);
    BossGanon_DrawShadowTexture(shadowTex, this, play);

    CLOSE_DISPS(play->state.gfxCtx);
}

s32 BossGanon_CheckFallingPlatforms(BossGanon* this, PlayState* play, Vec3f* checkPos) {
    Actor* prop = play->actorCtx.actorLists[ACTORCAT_PROP].head;

    while (prop != NULL) {
        if (((BossGanon*)prop == this) || (prop->id != ACTOR_BG_GANON_OTYUKA)) {
            prop = prop->next;
        } else {
            BgGanonOtyuka* platform = (BgGanonOtyuka*)prop;
            f32 xDiff = platform->dyna.actor.world.pos.x - checkPos->x;
            f32 yDiff = platform->dyna.actor.world.pos.y - checkPos->y;
            f32 zDiff = platform->dyna.actor.world.pos.z - checkPos->z;

            if ((fabsf(xDiff) < 60.0f) && (yDiff < 20.0f) && (yDiff > -20.0f) && (fabsf(zDiff) < 60.0f)) {
                platform->isFalling = true;
                platform->visibleSides = OTYUKA_SIDE_ALL;

                return 1;
            } else {
                prop = prop->next;
            }
        }
    }

    return 0;
}

static void BossGanon_LightBall_Update_Impl(Actor* thisx, PlayState* play2) {
    u8 hitWithBottle;
    s16 i;
    s16 spBA = 0;
    Vec3f spAC;
    Vec3f spA0;
    Vec3f sp94;
    BossGanon* this = (BossGanon*)thisx;
    PlayState* play = play2;
    f32 xDistFromLink;
    f32 yDistFromLink;
    f32 zDistFromLink;
    f32 minReflectDist;
    f32 xDistFromGanondorf;
    f32 yDistFromGanondorf;
    f32 zDistFromGanondorf;
    Player* player = GET_PLAYER(play);
    s32 pad;
    BossGanon* ganondorf = (BossGanon*)this->actor.parent;
    s32 pad1;

    this->unk_1A2++;
    ganondorf->envLightMode = 1;

    if (this->unk_1A8 != 0) {
        if (this->unk_1A8 == 2) {
            Math_ApproachZeroF(&this->fwork[GDF_FWORK_1], 1.0f, 10.0f);
            Math_ApproachF(&this->actor.scale.x, 30.0f, 0.5f, 100.0f);
        } else {
            this->actor.shape.rot.y += 0x1000;
            ganondorf->lensFlareTimer = 1;
            gCustomLensFlarePos = this->actor.world.pos;
            Math_ApproachZeroF(&this->fwork[GDF_FWORK_1], 1.0f, 30.0f);
            Math_ApproachF(&this->actor.scale.x, 20.0f, 0.5f, 100.0f);
            this->fwork[GDF_FWORK_0] += ((M_PI / 2) + Rand_ZeroFloat(M_PI / 4));
        }

        Actor_SetScale(&this->actor, this->actor.scale.x);

        if (this->fwork[GDF_FWORK_1] == 0.0f) {
            Actor_Kill(&this->actor);
        }
    } else {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_FIRE - SFX_FLAG);

        if ((this->unk_1A2 % 2) != 0) {
            Actor_SetScale(&this->actor, 6.0f);
        } else {
            Actor_SetScale(&this->actor, 5.25f);
        }

        this->actor.shape.rot.z += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;

        for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
            if (this->timers[i] != 0) {
                this->timers[i]--;
            }
        }

        xDistFromGanondorf = ganondorf->unk_1FC.x - this->actor.world.pos.x;
        yDistFromGanondorf = ganondorf->unk_1FC.y - this->actor.world.pos.y;
        zDistFromGanondorf = ganondorf->unk_1FC.z - this->actor.world.pos.z;

        xDistFromLink = player->actor.world.pos.x - this->actor.world.pos.x;
        yDistFromLink = (player->actor.world.pos.y + 40.0f) - this->actor.world.pos.y;
        zDistFromLink = player->actor.world.pos.z - this->actor.world.pos.z;

        Actor_UpdateVelocityXYZ(&this->actor);
        Actor_UpdatePos(&this->actor);

        switch (this->unk_1C2) {
            case 0:
                if ((player->stateFlags1 & PLAYER_STATE1_SWINGING_BOTTLE) &&
                    (ABS((s16)(player->actor.shape.rot.y - (s16)(ganondorf->actor.yawTowardsPlayer + 0x8000))) <
                     0x2000) &&
                    (sqrtf(SQ(xDistFromLink) + SQ(yDistFromLink) + SQ(zDistFromLink)) <= 25.0f)) {
                    hitWithBottle = true;
                } else {
                    hitWithBottle = false;
                }

                if ((this->collider.base.acFlags & 2) || hitWithBottle) {
                    ColliderInfo* acHitInfo = this->collider.info.acHitInfo;

                    this->collider.base.acFlags &= ~2;

                    if ((hitWithBottle == false) && (acHitInfo->toucher.dmgFlags & 0x100000)) {
                        spBA = 2;
                        Audio_PlaySfxGeneral(NA_SE_IT_SHIELD_REFLECT_MG, &player->actor.projectedPos, 4,
                                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                             &gSfxDefaultReverb);
                        Rumble_Request(this->actor.xyzDistToPlayerSq, 0xFF, 0x14, 0x96);
                    } else {
                        spBA = 1;
                        this->actor.world.rot.y = Math_Atan2S(zDistFromGanondorf, xDistFromGanondorf);
                        this->actor.world.rot.x =
                            Math_Atan2S(sqrtf(SQ(xDistFromGanondorf) + SQ(zDistFromGanondorf)), yDistFromGanondorf);
                        this->unk_1A4++;
                        this->timers[1] = 2;
                        Audio_PlaySfxGeneral(NA_SE_IT_SWORD_REFLECT_MG, &player->actor.projectedPos, 4,
                                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                             &gSfxDefaultReverb);
                        Rumble_Request(this->actor.xyzDistToPlayerSq, 0xB4, 0x14, 0x64);

                        if (hitWithBottle == false) {
                            // if ganondorf is 250 units away from link, at least 3 volleys are required
                            if ((ganondorf->actor.xyzDistToPlayerSq > 62500.0f) && (this->unk_1A4 < 3)) {
                                this->unk_1C2 = 1;
                            } else if (Rand_ZeroOne() < 0.7f) {
                                this->unk_1C2 = 1;
                            } else {
                                this->unk_1C2 = 3;
                            }

                            // if a spin attack is used
                            if (player->meleeWeaponAnimation >= 0x18) {
                                this->actor.speedXZ = 20.0f;
                            }
                            break;
                        } else {
                            if (Rand_ZeroOne() < 0.9f) {
                                this->unk_1C2 = 1;
                            } else {
                                this->unk_1C2 = 3;
                            }
                        }
                    }
                } else {
                    if (sqrtf(SQ(xDistFromLink) + SQ(yDistFromLink) + SQ(zDistFromLink)) <= 25.0f) {
                        spBA = 5;
                        Actor_SetPlayerKnockbackLarge(play, &this->actor, 3.0f, this->actor.world.rot.y, 0.0f, 0x30);
                        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40,
                                                           NA_SE_EN_GANON_HIT_THUNDER);
                        ganondorf->timers[2] = 20;

                        for (i = 0; i < ARRAY_COUNT(ganondorf->unk_4E4); i++) {
                            ganondorf->unk_4E4[i] = D_808E4C58[i];
                        }

                        ganondorf->unk_2E6 = 0;
                        ganondorf->unk_2E8 = 60;
                        ganondorf->unk_508 = 4.0f;
                    }
                }
                break;

            case 1:
                if ((ganondorf->actionFunc == BossGanon_PlayTennis) && (ganondorf->unk_1C2 == 1)) {
                    minReflectDist = (this->actor.speedXZ >= 19.0f) ? 250.0f : 170.0f;

                    if (sqrtf(SQ(xDistFromGanondorf) + SQ(yDistFromGanondorf) + SQ(zDistFromGanondorf)) <
                        minReflectDist) {
                        ganondorf->startVolley = true;
                        this->timers[0] = 8;
                        this->unk_1C2 = 2;
                    }
                }
                break;

            case 2:
                if (this->timers[0] == 1) {
                    spBA = 1;
                    this->actor.world.rot.y = Math_Atan2S(zDistFromLink, xDistFromLink);
                    this->actor.world.rot.x = Math_Atan2S(sqrtf(SQ(xDistFromLink) + SQ(zDistFromLink)), yDistFromLink);
                    this->timers[1] = 2;
                    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_REFLECT_MG);
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_AT_RETURN);
                    this->unk_1C2 = 0;
                    break;
                }
                // fallthrough
            case 4:
                if (sqrtf(SQ(xDistFromGanondorf) + SQ(yDistFromGanondorf) + SQ(zDistFromGanondorf)) < 30.0f) {
                    spBA = 3;
                    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EN_GANON_DAMAGE1);
                    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EN_GANON_HIT_THUNDER);
                }
                break;

            case 3:
                if (sqrtf(SQ(xDistFromGanondorf) + SQ(yDistFromGanondorf) + SQ(zDistFromGanondorf)) < 100.0f) {
                    ganondorf->startVolley = true;
                    this->unk_1C2 = 4;
                }
                break;
        }

        Collider_UpdateCylinder(&this->actor, &this->collider);

        if (this->timers[1] == 0) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }

        for (i = 0; i < 2; i++) {
            spA0.x = spA0.z = 0.0f;
            spA0.y = 0.2f;

            spAC.x = spAC.y = spAC.z = 0.0f;

            sp94.x = Rand_CenteredFloat(30.0f) + this->actor.world.pos.x;
            sp94.y = Rand_CenteredFloat(30.0f) + this->actor.world.pos.y;
            sp94.z = Rand_CenteredFloat(30.0f) + this->actor.world.pos.z;

            BossGanonEff_SpawnSparkle(play, &sp94, &spAC, &spA0, Rand_ZeroFloat(500.0f) + 700.0f, 0x1E);
        }

        if (this->actor.world.pos.y < 10.0f) {
            Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 20.0f, 20.0f, 4);
        }

        if ((fabsf(this->actor.world.pos.x) > 465.0f) || (this->actor.world.pos.y > 500.0f) ||
            (fabsf(this->actor.world.pos.z) > 465.0f)) {
            spBA = 4;
        }

        if ((spBA != 0) || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            f32 sp58;
            f32 sp54;
            f32 phi_f20;
            s16 sp4E;

            if (spBA == 1) {
                sp58 = Rand_ZeroFloat(100.0f) + 300.0f;
                sp54 = 10.0f;
                phi_f20 = 25.0f;
                sp4E = 40;
            } else {
                sp58 = Rand_ZeroFloat(200.0f) + 500.0f;
                sp54 = 15.0f;
                phi_f20 = 30.0f;
                sp4E = 70;
                SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 80, NA_SE_EN_GANON_HIT_THUNDER);
            }

            for (i = 0; i < sp4E; i++) {
                if (spBA != 0) {
                    spAC.x = Rand_CenteredFloat(phi_f20);
                    spAC.y = Rand_CenteredFloat(phi_f20);
                    spAC.z = Rand_CenteredFloat(phi_f20);
                } else {
                    spAC.x = Rand_CenteredFloat(phi_f20);
                    spAC.y = Rand_ZeroFloat(25.0f);
                    spAC.z = Rand_CenteredFloat(phi_f20);
                }

                BossGanonEff_SpawnLightRay(play, &this->actor.world.pos, &spAC, &sZeroVec, sp58, sp54, 0x1E);
            }

            if (spBA != 1) {
                this->unk_1A8 = 1;

                if (spBA == 0) {
                    BossGanon_CheckFallingPlatforms(this, play, &this->actor.world.pos);
                }

                if (spBA == 3) {
                    BossGanon_SetupHitByLightBall(ganondorf, play);
                } else if (ganondorf->actionFunc == BossGanon_PlayTennis) {
                    BossGanon_SetupWait(ganondorf, play);

                    if (spBA == 5) {
                        ganondorf->timers[0] = 125;
                    }
                }
            }
        }
    }
}

void BossGanon_LightBall_Draw(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    s16 i;
    f32 alpha;
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    alpha = ((this->unk_1A2 % 2) != 0) ? this->fwork[GDF_FWORK_1] * 0.4f : this->fwork[GDF_FWORK_1] * 0.35f;

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 155, (s8)alpha);
    Matrix_Push();
    Matrix_Translate(this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_Scale(this->actor.scale.x * 0.75f, 1.0f, this->actor.scale.z * 0.75f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightCoreDL);

    Matrix_Pop();
    gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);

    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s8)this->fwork[GDF_FWORK_1]);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);

    if (this->unk_1A8 == 1) {
        for (i = 0; i < 8; i++) {
            FrameInterpolation_RecordOpenChild("Ganondorf Light Ball 0", i);

            Matrix_Push();
            Matrix_RotateY(i * (M_PI / 8), MTXMODE_APPLY);
            Matrix_RotateZ(this->fwork[GDF_FWORK_0], MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);
            Matrix_Pop();

            FrameInterpolation_RecordCloseChild();
        }
    } else if (this->unk_1A8 == 0) {
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_RotateZ((this->actor.shape.rot.z / 32768.0f) * 3.1416f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void func_808E1EB4_Impl(Actor* thisx, PlayState* play2) {
    s16 i;
    BossGanon* this = (BossGanon*)thisx;
    PlayState* play = play2;
    BossGanon* dorf = (BossGanon*)this->actor.parent;
    f32 xDiff;
    f32 yDiff;
    f32 zDiff;
    f32 xzDist;
    s16 xRotTarget;
    s16 yRotTarget;
    Vec3f vel;
    Vec3f accel;

    this->unk_1A2++;
    dorf->envLightMode = 1;
    Actor_SetScale(&this->actor, 6.0f);
    this->actor.shape.rot.z += ((s16)(Rand_ZeroOne() * 20000.0f) + 0x4000);

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    Actor_UpdateVelocityXYZ(&this->actor);
    Actor_UpdatePos(&this->actor);

    this->unk_1A6++;

    if (this->unk_1A6 >= 15) {
        this->unk_1A6 = 0;
    }

    this->unk_2EC[this->unk_1A6] = this->actor.world.pos;

    if (this->unk_1C2 == 0) {
        if (this->timers[0] == 0) {
            this->unk_1C2 = 1;
        }
    } else if (this->unk_1C2 == 1) {
        xDiff = dorf->unk_1FC.x - this->actor.world.pos.x;
        yDiff = dorf->unk_1FC.y - this->actor.world.pos.y;
        zDiff = dorf->unk_1FC.z - this->actor.world.pos.z;

        yRotTarget = RADF_TO_BINANG(Math_FAtan2F(xDiff, zDiff));
        xzDist = sqrtf(SQ(xDiff) + SQ(zDiff));
        xRotTarget = RADF_TO_BINANG(Math_FAtan2F(yDiff, xzDist));

        Math_ApproachS(&this->actor.world.rot.x, xRotTarget, 1, 0x1000);
        Math_ApproachS(&this->actor.world.rot.y, yRotTarget, 1, 0x1000);

        if (sqrtf(SQ(xDiff) + SQ(zDiff) + SQ(yDiff)) < 40.0f) {
            this->unk_1C2 = 2;
            this->timers[0] = 30;
            this->actor.speedXZ = 0.0f;

            if (this->actor.params == 0xC8) {
                Sfx_PlaySfxCentered(NA_SE_EN_GANON_DAMAGE2);
                Sfx_PlaySfxCentered(NA_SE_EN_GANON_DD_THUNDER);

                for (i = 0; i < 150; i++) {

                    vel.x = Rand_CenteredFloat(25.0f);
                    vel.y = Rand_CenteredFloat(25.0f);
                    vel.z = Rand_CenteredFloat(25.0f);

                    accel.x = vel.x * -0.03f;
                    accel.y = vel.y * -0.03f;
                    accel.z = vel.z * -0.03f;

                    BossGanonEff_SpawnLightRay(play, &dorf->unk_1FC, &vel, &accel, Rand_ZeroFloat(500.0f) + 1000.0f,
                                               15.0f, 0x14);
                }

                for (i = 1; i < 15; i++) {
                    dorf->unk_4E4[i] = 1000;
                }

                dorf->unk_2E6 = 1000;
                dorf->unk_2E8 = 0;
                dorf->screenFlashTimer = 4;
                dorf->lensFlareTimer = 10;
                dorf->lensFlareMode = 1;
                dorf->unk_508 = 10.0f;

                Actor_SpawnAsChild(&play->actorCtx, &dorf->actor, play, ACTOR_BOSS_GANON, dorf->unk_1FC.x,
                                   dorf->unk_1FC.y, dorf->unk_1FC.z, 0, 0, 0, 0x12C);
            }

            this->actor.world.pos.y = 5000.0f;
        }
    } else if (this->timers[0] == 0) {
        Actor_Kill(&this->actor);
    }
}

void func_808E229C(Actor* thisx, PlayState* play2) {
    BossGanon* this = (BossGanon*)thisx;
    PlayState* play = play2;
    s16 i;
    s32 temp;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
    gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);

    for (i = 9; i >= 0; i--) {
        temp = (s16)(((this->unk_1A6 - i) + 0xF) % 15);
        Matrix_Translate(this->unk_2EC[temp].x, this->unk_2EC[temp].y, this->unk_2EC[temp].z, MTXMODE_NEW);
        Matrix_Scale(this->actor.scale.x * (1.0f - (i * 0.07000001f)), this->actor.scale.y * (1.0f - (i * 0.07000001f)),
                     this->actor.scale.z * (1.0f - (i * 0.07000001f)), MTXMODE_APPLY);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_RotateZ(((2.0f * (i * M_PI)) / 10.0f) + BINANG_TO_RAD(this->actor.shape.rot.z), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void func_808E2544_Impl(Actor* thisx, PlayState* play) {
    u8 numEffects = 0;
    s16 xRot;
    f32 xDiff;
    f32 yDiff;
    f32 zDiff;
    f32 xzDist;
    f32 new_var;
    f32 sp84;
    s16 i;
    s16 sp80;
    BossGanon* this = (BossGanon*)thisx;
    BossGanon* dorf = (BossGanon*)this->actor.parent;
    s32 pad;
    Player* player = GET_PLAYER(play);
    ColliderInfo* acHitInfo;
    Vec3f sp60;

    this->unk_1A2++;
    Actor_SetScale(&this->actor, 0.01f);

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    Actor_UpdateVelocityXYZ(&this->actor);
    Actor_UpdatePos(&this->actor);

    this->unk_1A6++;

    if (this->unk_1A6 >= 15) {
        this->unk_1A6 = 0;
    }

    this->unk_2EC[this->unk_1A6] = this->actor.world.pos;
    this->unk_3C4[this->unk_1A6].x = BINANG_TO_RAD(this->actor.world.rot.x);
    this->unk_3C4[this->unk_1A6].y = BINANG_TO_RAD(this->actor.world.rot.y);

    switch (this->unk_1C2) {
        case 0:
            this->actor.speedXZ = 40.0f;
            Math_ApproachF(&this->fwork[1], 255.0f, 1.0f, 40.0f);
            xDiff = dorf->unk_278.x - this->actor.world.pos.x;
            yDiff = dorf->unk_278.y - this->actor.world.pos.y;
            zDiff = dorf->unk_278.z - this->actor.world.pos.z;
            sp80 = RADF_TO_BINANG(Math_FAtan2F(xDiff, zDiff));
            xzDist = sqrtf(SQ(xDiff) + SQ(zDiff));

            xRot = RADF_TO_BINANG(Math_FAtan2F(yDiff, xzDist));
            sp84 = (xzDist * 700.0f) / 10.0f;
            if (sp84 > 6144.0f) {
                sp84 = 6144.0f;
            }

            xRot += (Math_CosS(this->unk_1A2 * 0x2200) * sp84);
            this->actor.world.rot.x = xRot;
            Math_ApproachS(&this->actor.shape.rot.y, sp80, 1, this->csCamMaxStepScale);
            Math_ApproachF(&this->csCamMaxStepScale, 4096.0f, 1.0f, 256.0f);
            this->actor.world.rot.y = (Math_SinS(this->unk_1A2 * 0x1A00) * sp84) + this->actor.shape.rot.y;

            if (sqrtf(SQ(xDiff) + SQ(zDiff) + SQ(yDiff)) < 45.0f) {
                this->unk_1C2 = 1;
                this->actor.speedXZ = 0.0f;
            }
            break;

        case 1:
            Math_ApproachZeroF(&this->fwork[1], 1.0f, 40.0f);

            if (this->fwork[1] == 0.0f) {
                Actor_Kill(&this->actor);
            }
            break;

        case 10:
            this->unk_1C2 = 0xB;
            this->timers[0] = 14;

            this->collider.dim.radius = 15;
            this->collider.dim.height = 20;
            this->collider.dim.yShift = -10;

            this->actor.speedXZ = 20.0f;
            this->fwork[1] = 255.0f;
            this->unk_1F0 = player->actor.world.pos;
            new_var = this->unk_1F0.x - this->actor.world.pos.x;
            this->actor.shape.rot.y = RADF_TO_BINANG(Math_FAtan2F(new_var, this->unk_1F0.z - this->actor.world.pos.z)) +
                                      (this->actor.params << 0xD) - 0x20C000;
            // fallthrough
        case 11:
            if (this->timers[0] != 0) {
                this->unk_1F0 = player->actor.world.pos;
                xDiff = this->unk_1F0.x - this->actor.world.pos.x;
                yDiff = (this->unk_1F0.y + 30.0f) - this->actor.world.pos.y;
                zDiff = this->unk_1F0.z - this->actor.world.pos.z;

                sp80 = RADF_TO_BINANG(Math_FAtan2F(xDiff, zDiff));
                this->actor.shape.rot.x = RADF_TO_BINANG(Math_FAtan2F(yDiff, sqrtf(SQ(xDiff) + SQ(zDiff))));
                Math_ApproachS(&this->actor.shape.rot.y, sp80, 1, this->csCamMaxStepScale);
                Math_ApproachF(&this->csCamMaxStepScale, 4096.0f, 1.0f, 256.0f);
            }

            sp84 = (sqrtf(this->actor.xyzDistToPlayerSq) * 200.0f) / 10.0f;
            if (sp84 > 13824.0f) {
                sp84 = 13824.0f;
            }

            this->actor.world.rot.x = (Math_CosS(this->unk_1A2 * 0x3400) * sp84 * 0.1f) + this->actor.shape.rot.x;
            this->actor.world.rot.y = (Math_SinS(this->unk_1A2 * 0x1A00) * sp84) + this->actor.shape.rot.y;

            if ((player->meleeWeaponState != 0) && (player->meleeWeaponAnimation >= 0x18) &&
                (this->actor.xzDistToPlayer < 80.0f)) {
                this->unk_1C2 = 0xC;
                this->actor.speedXZ = -30.0f;
                Actor_UpdateVelocityXYZ(&this->actor);
                Actor_UpdatePos(&this->actor);
                this->unk_1F0 = dorf->unk_1FC;
                numEffects = 10;
                break;
            }

            if (this->collider.base.acFlags & 2) {
                acHitInfo = this->collider.info.acHitInfo;

                this->collider.base.acFlags &= ~2;

                if (!(acHitInfo->toucher.dmgFlags & 0x100000) || Player_HasMirrorShieldEquipped(play)) {
                    Rumble_Request(this->actor.xyzDistToPlayerSq, 0xB4, 0x14, 0x64);
                    this->unk_1C2 = 0xC;
                    this->actor.speedXZ = -30.0f;

                    Actor_UpdateVelocityXYZ(&this->actor);
                    Actor_UpdatePos(&this->actor);

                    this->unk_1F0.x = Rand_CenteredFloat(700.0f) + dorf->unk_1FC.x;
                    this->unk_1F0.y = Rand_CenteredFloat(200.0f) + dorf->unk_1FC.y;
                    this->unk_1F0.z = Rand_CenteredFloat(700.0f) + dorf->unk_1FC.z;

                    this->unk_1F0.x = this->unk_1F0.x + ((this->unk_1F0.x - this->actor.world.pos.x) * 100.0f);
                    this->unk_1F0.y = this->unk_1F0.y + ((this->unk_1F0.y - this->actor.world.pos.y) * 100.0f);
                    this->unk_1F0.z = this->unk_1F0.z + ((this->unk_1F0.z - this->actor.world.pos.z) * 100.0f);

                    numEffects = 10;
                    break;
                }
            }

            Collider_UpdateCylinder(&this->actor, &this->collider);

            if (this->timers[1] == 0) {
                CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            }

            xDiff = player->actor.world.pos.x - this->actor.world.pos.x;
            yDiff = (player->actor.world.pos.y + 30.0f) - this->actor.world.pos.y;
            zDiff = player->actor.world.pos.z - this->actor.world.pos.z;

            if (sqrtf(SQ(xDiff) + SQ(zDiff) + SQ(yDiff)) < 30.0f) {
                this->unk_1C2 = 1;
                this->actor.speedXZ = 0.0f;

                if (dorf->timers[2] == 0) {
                    Actor_SetPlayerKnockbackLarge(play, &this->actor, 3.0f, this->actor.world.rot.y, 0.0f, 0x50);
                    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EN_GANON_HIT_THUNDER);
                    dorf->timers[2] = 20;

                    for (i = 0; i < ARRAY_COUNT(this->unk_4E4); i++) {
                        dorf->unk_4E4[i] = D_808E4C58[i];
                    }

                    dorf->unk_2E6 = 0;
                    dorf->unk_2E8 = 60;
                    dorf->unk_508 = 4.0f;
                    numEffects = 40;
                }
            }
            break;

        case 12:
            this->actor.speedXZ = 20.0f;

            xDiff = this->unk_1F0.x - this->actor.world.pos.x;
            yDiff = this->unk_1F0.y - this->actor.world.pos.y;
            zDiff = this->unk_1F0.z - this->actor.world.pos.z;

            sp80 = RADF_TO_BINANG(Math_FAtan2F(xDiff, zDiff));
            xzDist = sqrtf(SQ(xDiff) + SQ(zDiff));
            xRot = RADF_TO_BINANG(Math_FAtan2F(yDiff, xzDist));
            sp84 = (xzDist * 700.0f) / 10.0f;

            if (sp84 > 6144.0f) {
                sp84 = 6144.0f;
            }

            sp80 += Math_SinS(this->unk_1A2 * 0x2200) * sp84;

            xRot += Math_CosS(this->unk_1A2 * 0x1800) * sp84;

            this->actor.world.rot.x = xRot;
            this->actor.world.rot.y = sp80;

            xDiff = dorf->unk_1FC.x - this->actor.world.pos.x;
            yDiff = dorf->unk_1FC.y - this->actor.world.pos.y;
            zDiff = dorf->unk_1FC.z - this->actor.world.pos.z;

            if (sqrtf(SQ(xDiff) + SQ(zDiff) + SQ(yDiff)) < 45.0f) {
                BossGanon_SetupHitByLightBall(dorf, play);
                this->timers[0] = 150;
                numEffects = 40;
                this->unk_1C2 = 1;
                this->actor.speedXZ = 0.0f;
            }
            break;
    }

    if (this->unk_1C2 >= 0xB) {
        xzDist = (this->unk_1C2 == 0xC) ? -65.0f : 0.0f;

        if ((fabsf(this->actor.world.pos.x) > (465.0f + xzDist)) ||
            (fabsf(this->actor.world.pos.z) > (465.0f + xzDist)) || ((this->actor.world.pos.y < 0.0f)) ||
            (this->actor.world.pos.y > 450.0f)) {
            this->unk_1C2 = 1;
            this->actor.speedXZ = 0.0f;
            numEffects = 10;
            BossGanon_CheckFallingPlatforms(this, play, &this->actor.world.pos);
            Actor_SpawnAsChild(&play->actorCtx, &dorf->actor, play, ACTOR_BOSS_GANON, this->actor.world.pos.x,
                               this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, 0x190);
        }
    }

    if (numEffects) {
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 80, NA_SE_EN_FANTOM_THUNDER);

        for (i = 0; i < numEffects; i++) {
            sp60.x = Rand_CenteredFloat(30.0f);
            sp60.y = Rand_CenteredFloat(30.0f);
            sp60.z = Rand_CenteredFloat(30.0);

            BossGanonEff_SpawnLightRay(play, &this->actor.world.pos, &sp60, &sZeroVec, Rand_ZeroFloat(200.0f) + 500.0f,
                                       15.0f, 0x1E);
        }
    }
}

static Gfx* sBigMagicLightStreakDLists[] = {
    gGanondorfLightStreak12DL, gGanondorfLightStreak11DL, gGanondorfLightStreak10DL, gGanondorfLightStreak9DL,
    gGanondorfLightStreak8DL,  gGanondorfLightStreak7DL,  gGanondorfLightStreak6DL,  gGanondorfLightStreak5DL,
    gGanondorfLightStreak4DL,  gGanondorfLightStreak3DL,  gGanondorfLightStreak2DL,  gGanondorfLightStreak1DL,
};

void func_808E324C(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    Mtx* mtx;
    s16 i;
    s32 temp;

    mtx = Graph_Alloc(play->state.gfxCtx, 12 * sizeof(Mtx));

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, (s8)this->fwork[GDF_FWORK_1]);
    gDPSetEnvColor(POLY_XLU_DISP++, 150, 255, 0, 128);
    gSPSegment(POLY_XLU_DISP++, 0x0D, mtx);

    for (i = 0; i < 12; i++) {
        temp = (s16)(((this->unk_1A6 - i) + 0xF) % 15);
        Matrix_Translate(this->unk_2EC[temp].x, this->unk_2EC[temp].y, this->unk_2EC[temp].z, MTXMODE_NEW);
        Matrix_RotateY(this->unk_3C4[temp].y, MTXMODE_APPLY);
        Matrix_RotateX(-this->unk_3C4[temp].x, MTXMODE_APPLY);
        Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
        Matrix_RotateY(M_PI / 2, MTXMODE_APPLY);
        MATRIX_TOMTX(mtx);
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, sBigMagicLightStreakDLists[i]);
        mtx++;
    };

    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(10.0f, 10.0f, 10.0f, MTXMODE_APPLY);
    Matrix_RotateZ(Rand_CenteredFloat(M_PI), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);

    gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void BossGanon_UpdateEffects(PlayState* play) {
    Player* player = GET_PLAYER(play);
    GanondorfEffect* eff = sCurEffects;
    s16 i;
    s32 pad;
    f32 xDiff;
    f32 yDiff;
    f32 zDiff;
    f32 yRot;
    f32 xRot;
    Vec3f spA0;
    s16 bodyPart;
    f32 distToPlayer;
    s32 pad2;
    s32 pad3;

    spA0.x = 0.0f;
    spA0.y = 0.0f;

    for (i = 0; i < ARRAY_COUNT(sEffects); i++, eff++) {
        if (eff->type != GDF_EFF_NONE) {
            eff->pos.x += eff->velocity.x;
            eff->pos.y += eff->velocity.y;
            eff->pos.z += eff->velocity.z;

            eff->timer++;

            eff->velocity.x += eff->accel.x;
            eff->velocity.y += eff->accel.y;
            eff->velocity.z += eff->accel.z;

            if (eff->type == GDF_EFF_WINDOW_SHARD) {
                eff->unk_44 += 0.3f;
                eff->unk_48 += 0.5f;

                if (eff->pos.y < 0.0f) {
                    eff->type = GDF_EFF_NONE;
                }
            } else if (eff->type == GDF_EFF_SPARKLE) {
                eff->unk_3C += Rand_ZeroFloat(M_PI / 2) + M_PI / 2;
                eff->unk_2E -= eff->unk_30;

                if (eff->unk_2E <= 0) {
                    eff->unk_2E = 0;
                    eff->type = GDF_EFF_NONE;
                }

                eff->alpha = eff->unk_2E;

                if (eff->alpha > 255) {
                    eff->alpha = 255;
                }
            } else if (eff->type == GDF_EFF_BLACK_DOT) {
                xDiff = sGanondorf->unk_278.x - eff->pos.x;
                yDiff = sGanondorf->unk_278.y - eff->pos.y;
                zDiff = sGanondorf->unk_278.z - eff->pos.z;

                yRot = Math_FAtan2F(xDiff, zDiff);

                xRot = -Math_FAtan2F(yDiff, sqrtf(SQ(xDiff) + SQ(zDiff)));
                spA0.z = eff->unk_38;
                Matrix_RotateY(yRot, MTXMODE_NEW);
                Matrix_RotateX(xRot, MTXMODE_APPLY);
                Matrix_MultVec3f(&spA0, &eff->velocity);
                Math_ApproachF(&eff->unk_38, 10.0f, 1.0f, 0.5f);

                eff->alpha += 10;

                if (eff->alpha > 255) {
                    eff->alpha = 255;
                }

                if ((sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) < 20.0f) || (eff->timer > 70)) {
                    eff->type = GDF_EFF_NONE;
                }
            } else if (eff->type == GDF_EFF_LIGHT_RAY) {
                eff->unk_3C += Rand_ZeroFloat(M_PI / 2) + M_PI / 2;
                eff->unk_2E -= eff->unk_30;

                if (eff->unk_2E <= 0) {
                    eff->unk_2E = 0;
                    eff->type = GDF_EFF_NONE;
                }

                eff->alpha = eff->unk_2E;

                if (eff->alpha > 255) {
                    eff->alpha = 255;
                }

                Math_ApproachF(&eff->unk_38, eff->unk_40, 1.0f, (eff->unk_40 / 15.0f) * 4.0f);
            } else if (eff->type == GDF_EFF_SHOCK) {
                if (eff->unk_2E == GDF_SHOCK_DORF_YELLOW) {
                    bodyPart = (s16)Rand_ZeroFloat(13.9f) + 1;

                    eff->pos.x = sGanondorf->unk_2EC[bodyPart].x + Rand_CenteredFloat(20.0f);
                    eff->pos.y = sGanondorf->unk_2EC[bodyPart].y + Rand_CenteredFloat(20.0f);
                    eff->pos.z = sGanondorf->unk_2EC[bodyPart].z + Rand_CenteredFloat(20.0f);
                } else {
                    bodyPart = (s16)Rand_ZeroFloat(17.9f);

                    eff->pos.x = player->bodyPartsPos[bodyPart].x + Rand_CenteredFloat(10.0f);
                    eff->pos.y = player->bodyPartsPos[bodyPart].y + Rand_CenteredFloat(15.0f);
                    eff->pos.z = player->bodyPartsPos[bodyPart].z + Rand_CenteredFloat(10.0f);
                }

                eff->unk_3C += (Rand_ZeroFloat(M_PI / 2) + M_PI / 2);

                if (eff->timer > 20) {
                    eff->type = GDF_EFF_NONE;
                }
            } else if (eff->type == GDF_EFF_LIGHTNING) {
                if (eff->unk_3C == 0.0f) {
                    eff->unk_44 = BINANG_TO_RAD(Camera_GetInputDirYaw(Play_GetCamera(play, CAM_ID_MAIN)));
                } else {
                    eff->unk_44 = M_PI / 2;
                }

                if (eff->timer > 12) {
                    eff->type = GDF_EFF_NONE;
                }
            } else if (eff->type == GDF_EFF_IMPACT_DUST_DARK) {
                eff->unk_30++; // unused

                if (eff->unk_2E == 0) {
                    eff->alpha += 26;

                    if (eff->alpha > 255) {
                        eff->alpha = 255;
                        eff->unk_2E = 1;
                    }
                } else if (eff->unk_2E == 1) {
                    eff->unk_2E = 2;
                } else if (eff->unk_2E == 2) {
                    eff->alpha -= 26;

                    if (eff->alpha < 0) {
                        eff->alpha = 0;
                        eff->type = GDF_EFF_NONE;
                    }
                }

                Math_ApproachF(&eff->scale, eff->unk_38, 1.0f, 0.01f);
                Math_ApproachF(&eff->unk_40, 4.0f, 1.0f, 0.15f);
            } else if (eff->type == GDF_EFF_IMPACT_DUST_LIGHT) {
                if (i == 0) {
                    Sfx_PlaySfxCentered(NA_SE_EN_GANON_WAVE_GND - SFX_FLAG);
                }

                eff->unk_30++; // unused

                if (eff->unk_2E == 0) {
                    eff->alpha += 100;

                    if (eff->alpha > 255) {
                        eff->alpha = 255;
                        eff->unk_2E = 1;
                    }
                } else if (eff->unk_2E == 1) {
                    if (eff->timer >= 20) {
                        eff->unk_2E = 2;
                    }
                } else if (eff->unk_2E == 2) {
                    eff->alpha -= 30;

                    if (eff->alpha < 0) {
                        eff->alpha = 0;
                        eff->type = GDF_EFF_NONE;
                    }
                }

                Math_ApproachF(&eff->scale, eff->unk_38, 1.0f, 0.1f);
                Math_ApproachF(&eff->unk_40, 1.0f, 1.0f, 0.15f);
            } else if (eff->type == GDF_EFF_SHOCKWAVE) {
                eff->unk_30++; // unused
                eff->alpha -= 30;

                if (eff->alpha < 0) {
                    eff->alpha = 0;
                    eff->type = GDF_EFF_NONE;
                }

                Math_ApproachF(&eff->scale, eff->unk_38, 1.0f, 0.13f);

                if ((eff->timer < 150) && (fabsf(player->actor.world.pos.y) < 5.0f) && !GDF_IS_PLAYER(sGanondorf)) {
                    distToPlayer =
                        sqrtf(SQ(eff->pos.x - player->actor.world.pos.x) + SQ(eff->pos.z - player->actor.world.pos.z));

                    if (((eff->scale * 150.0f) < distToPlayer) && (distToPlayer < (eff->scale * 300.0f))) {
                        eff->timer = 150;
                        Actor_SetPlayerKnockbackLarge(play, &sGanondorf->actor, 7.0f,
                                                      sGanondorf->actor.yawTowardsPlayer, 0.0f, 0x20);
                    }
                }
            }
        }
    }
}

static void* sLightningTextures[] = {
    gGanondorfLightning1Tex,  gGanondorfLightning1Tex, gGanondorfLightning2Tex,  gGanondorfLightning3Tex,
    gGanondorfLightning4Tex,  gGanondorfLightning5Tex, gGanondorfLightning6Tex,  gGanondorfLightning7Tex,
    gGanondorfLightning8Tex,  gGanondorfLightning9Tex, gGanondorfLightning10Tex, gGanondorfLightning11Tex,
    gGanondorfLightning12Tex,
};

static u8 sLightningPrimColors[] = {
    0,   0,   0,   255, 255, 255, 231, 250, 231, 208, 245, 208, 185, 240, 185, 162, 235, 162, 139, 230,
    139, 115, 225, 115, 92,  220, 92,  69,  215, 69,  46,  210, 46,  23,  205, 23,  0,   200, 0,
};

static u8 sLightningEnvColors[] = {
    0,   0,   0,   255, 255, 0,   240, 231, 23,  226, 208, 46,  212, 185, 69,  198, 162, 92,
    184, 139, 115, 170, 115, 139, 156, 92,  162, 142, 69,  185, 128, 46,  208, 114, 23,  231,
    100, 0,   255, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

void BossGanon_DrawEffects(PlayState* play) {
    u8 flag = 0;
    s16 i;
    s32 pad;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    GanondorfEffect* eff = sCurEffects;
    GanondorfEffect* effFirst = eff;

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    for (i = 0; i < 200; i++, eff++) {
        if (eff->type == GDF_EFF_WINDOW_SHARD) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_OPA_DISP++);
            if (flag == 0) {
                gSPDisplayList(POLY_OPA_DISP++, gGanondorfWindowShardMaterialDL);
                flag++;
            }
            if ((eff->timer & 7) != 0) {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, eff->color.r, eff->color.g, eff->color.b, 255);
            } else {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
            }
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale(eff->scale, eff->scale, eff->scale, MTXMODE_APPLY);
            Matrix_RotateY(eff->unk_48, MTXMODE_APPLY);
            Matrix_RotateX(eff->unk_44, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gGanondorfWindowShardModelDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;
    flag = 0;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_SPARKLE) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            if (flag == 0) {
                gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
                gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);
                flag++;
            }
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(eff->scale, eff->scale, 1.0f, MTXMODE_APPLY);
            Matrix_RotateZ(eff->unk_3C, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;
    flag = 0;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_LIGHT_RAY) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            if (flag == 0) {
                gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
                gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);
                flag++;
            }
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_RotateY(eff->unk_48, MTXMODE_APPLY);
            Matrix_RotateX(eff->unk_44, MTXMODE_APPLY);
            Matrix_RotateZ(eff->unk_3C, MTXMODE_APPLY);
            Matrix_Scale(eff->scale, eff->scale, eff->unk_38 * eff->scale, MTXMODE_APPLY);
            Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;
    flag = 0;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_SHOCK) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            if (flag == 0) {
                gDPPipeSync(POLY_XLU_DISP++);
                if (eff->unk_2E == GDF_SHOCK_PLAYER_PURPLE) {
                    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 100, 0, 200, 255);
                    gDPSetEnvColor(POLY_XLU_DISP++, 130, 0, 0, 0);
                } else { // GDF_SHOCK_DORF_YELLOW or GDF_SHOCK_PLAYER_YELLOW
                    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
                    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
                }
                flag++;
            }
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale(eff->scale, eff->scale, 1.0f, MTXMODE_APPLY);
            Matrix_RotateX(eff->unk_3C * 1.3f, MTXMODE_APPLY);
            Matrix_RotateZ(eff->unk_3C, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfShockDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_LIGHTNING) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sLightningPrimColors[(eff->timer * 3) + 0],
                            sLightningPrimColors[(eff->timer * 3) + 1], sLightningPrimColors[(eff->timer * 3) + 2],
                            255);
            gDPSetEnvColor(POLY_XLU_DISP++, sLightningEnvColors[(eff->timer * 3) + 0],
                           sLightningEnvColors[(eff->timer * 3) + 1], sLightningEnvColors[(eff->timer * 3) + 2], 0);
            Matrix_Translate(sGanondorf->unk_260.x, sGanondorf->unk_260.y, sGanondorf->unk_260.z, MTXMODE_NEW);
            Matrix_RotateY(eff->unk_48, MTXMODE_APPLY);
            Matrix_RotateZ(eff->unk_3C, MTXMODE_APPLY);
            Matrix_Scale(eff->scale, eff->scale, eff->scale, MTXMODE_APPLY);
            Matrix_RotateY(eff->unk_44, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sLightningTextures[eff->timer]));
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightningDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_IMPACT_DUST_DARK) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 0, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 100, 70, 0, 128);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, eff->timer * 4, 0, 32, 64, 1, eff->timer * 2,
                                          eff->timer * -20, 32, 32, 4, 0, 2, -20));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale(eff->scale, eff->unk_40 * eff->scale, eff->scale, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfImpactDarkDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_IMPACT_DUST_LIGHT) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 200, 100, 0, 128);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, eff->timer * 4, 0, 32, 64, 1, eff->timer * 2,
                                          eff->timer * -20, 32, 32, 4, 0, 2, -20));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale(eff->scale, eff->unk_40 * eff->scale, eff->scale, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfImpactLightDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_SHOCKWAVE) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 150, 255, 0, 128);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (eff->timer * 100), 0, 64, 32, 1, (eff->timer * 100),
                                          0, 64, 32, 100, 0, 100, 0));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale((eff->scale * 200.0f) / 1500.0f, (eff->unk_40 * 200.0f) / 1500.0f,
                         (eff->scale * 200.0f) / 1500.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfShockwaveDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    eff = effFirst;

    for (i = 0; i < 150; i++, eff++) {
        if (eff->type == GDF_EFF_BLACK_DOT) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 170, 0, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 128);
            gSPSegment(POLY_XLU_DISP++, 0x0A,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 32, 1, eff->timer * 2, eff->timer * -20, 64,
                                          64, 0, 0, 2, -20));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(eff->scale, eff->scale, 1.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gGanondorfDotDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    CLOSE_DISPS(gfxCtx);
}

// ================================================================================================
// SoH: "Play as Ganondorf"
//
// Ganondorf from the Ganon's Tower fight, controlled by the player. Link is still there underneath,
// invisible, so walking, collision, doors, ladders, the camera and loading zones keep working. This
// actor follows him around and replaces his moveset with Ganondorf's boss attacks:
//
//   B            Charge a light ball and throw it. Keep holding B to charge Big Magic instead.
//   R            Block with the cape (Link can't be hurt while blocking).
//   R + B        Pound the floor (shockwave around Ganondorf).
//   C-Left       Light ball        (when C-button items are disabled)
//   C-Down       Pound the floor   (when C-button items are disabled)
//   C-Right      Big Magic         (when C-button items are disabled)
//
// Every attack carries all damage types (see DMG_GANONDORF_PLAYER), but only looks like Ganondorf's magic.
//
// The vanilla AI is tied to the arena (absolute coordinates, falling platforms, room lighting), so
// this mode has its own update function and action functions modelled on the vanilla ones. The
// model, animations, cape, effects and draw code are shared with the boss.
// ================================================================================================

#define CVAR_PLAY_AS_GDF(var) CVAR_ENHANCEMENT("PlayAsGanondorf." var)

// ---- Killing NPCs (option, permanent) ----
// Every attack volume he puts out in a frame is noted here (cylinders from his pound, aura, slam and projectiles,
// segments from the blade). At the start of his next update, NPCs touching any of them die, bloodily, and are
// remembered in the save file as dead (GanondorfKills.cpp).
#define GDF_MAX_AT_VOLUMES 24
typedef struct {
    u8 isSegment;
    Vec3f a;     // cylinder: bottom centre; segment: one end
    Vec3f b;     // segment: other end
    f32 radius;
    f32 height;  // cylinder
} GdfAtVolume;
static GdfAtVolume sGdfAtVolumes[GDF_MAX_AT_VOLUMES];
static s32 sGdfAtCount = 0;

static void BossGanon_Player_NoteCylinder(ColliderCylinder* cyl) {
    GdfAtVolume* v;

    if (!GanondorfKills_IsEnabled() || (sGdfAtCount >= GDF_MAX_AT_VOLUMES)) {
        return;
    }
    v = &sGdfAtVolumes[sGdfAtCount++];
    v->isSegment = false;
    v->a.x = cyl->dim.pos.x;
    v->a.y = cyl->dim.pos.y + cyl->dim.yShift;
    v->a.z = cyl->dim.pos.z;
    v->radius = cyl->dim.radius;
    v->height = cyl->dim.height;
}

static void BossGanon_Player_NoteSegment(Vec3f* a, Vec3f* b, f32 radius) {
    GdfAtVolume* v;

    if (!GanondorfKills_IsEnabled() || (sGdfAtCount >= GDF_MAX_AT_VOLUMES)) {
        return;
    }
    v = &sGdfAtVolumes[sGdfAtCount++];
    v->isSegment = true;
    v->a = *a;
    v->b = *b;
    v->radius = radius;
}

static void BossGanon_Player_SetAT(BossGanon* this, PlayState* play) {
    BossGanon_Player_NoteCylinder(&this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}

static f32 BossGanon_Player_DistToSegment(Vec3f* p, Vec3f* a, Vec3f* b) {
    Vec3f ab = { b->x - a->x, b->y - a->y, b->z - a->z };
    f32 len2 = SQ(ab.x) + SQ(ab.y) + SQ(ab.z);
    f32 t = 0.0f;
    Vec3f c;

    if (len2 > 0.001f) {
        t = ((p->x - a->x) * ab.x + (p->y - a->y) * ab.y + (p->z - a->z) * ab.z) / len2;
        t = CLAMP(t, 0.0f, 1.0f);
    }
    c.x = a->x + ab.x * t;
    c.y = a->y + ab.y * t;
    c.z = a->z + ab.z * t;
    return Math_Vec3f_DistXYZ(p, &c);
}

// The NPC (as a column from its feet to its head) touches the volume
static s32 BossGanon_Player_NpcInVolume(Actor* npc, GdfAtVolume* v) {
    f32 top = MAX(npc->focus.pos.y, npc->world.pos.y + 30.0f);
    f32 bottom = npc->world.pos.y;
    s32 i;

    if (!v->isSegment) {
        f32 dx = npc->world.pos.x - v->a.x;
        f32 dz = npc->world.pos.z - v->a.z;

        return (SQ(dx) + SQ(dz) < SQ(v->radius + 15.0f)) && (top > v->a.y) && (bottom < v->a.y + v->height);
    }
    for (i = 0; i <= 4; i++) {
        Vec3f p = npc->world.pos;

        p.y = bottom + (top - bottom) * (i / 4.0f);
        if (BossGanon_Player_DistToSegment(&p, &v->a, &v->b) < v->radius + 15.0f) {
            return true;
        }
    }
    return false;
}

// ---- The guards hunt him (after a murder in the market) ----
// A murder anywhere in Castle Town makes him wanted. While he's wanted, alarm bells ring through the market and
// guards come for him with their spears, a new one running in for every one he cuts down (three in the market, six
// on the castle grounds, where they swarm him). It lasts until he leaves the market by the drawbridge.
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
static u8 sGdfWanted = false;
static s16 sGdfLastScene = -1;
static s16 sGdfBellTimer = 0;

s32 BossGanon_Player_IsWanted(void) {
    return sGdfWanted;
}

static s32 BossGanon_Player_IsMarketScene(s16 scene) {
    switch (scene) {
        case SCENE_MARKET_DAY:
        case SCENE_MARKET_NIGHT:
        case SCENE_MARKET_ENTRANCE_DAY:
        case SCENE_MARKET_ENTRANCE_NIGHT:
        case SCENE_BACK_ALLEY_DAY:
        case SCENE_BACK_ALLEY_NIGHT:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT:
            return true;
        default:
            return false;
    }
}

static s32 BossGanon_Player_IsCastleScene(s16 scene) {
    return (scene == SCENE_HYRULE_CASTLE) || (scene == SCENE_CASTLE_COURTYARD_GUARDS_DAY) ||
           (scene == SCENE_CASTLE_COURTYARD_GUARDS_NIGHT);
}

static s32 BossGanon_Player_IsHunter(Actor* actor) {
    return (actor->id == ACTOR_EN_HEISHI1) && (actor->params == EN_HEISHI1_PARAMS_HUNTER);
}

// Scene changes: leaving the market entrance for Hyrule Field (over the drawbridge) ends it
static void BossGanon_Player_ManhuntSceneChange(PlayState* play) {
    if (((sGdfLastScene == SCENE_MARKET_ENTRANCE_DAY) || (sGdfLastScene == SCENE_MARKET_ENTRANCE_NIGHT)) &&
        (play->sceneNum == SCENE_HYRULE_FIELD)) {
        sGdfWanted = false;
    }
    sGdfLastScene = play->sceneNum;
    sGdfBellTimer = 20;
}

// A new guard, out of sight behind the camera, on the ground
static void BossGanon_Player_SpawnHunter(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Camera* cam = GET_ACTIVE_CAM(play);
    s16 behind = Math_Vec3f_Yaw(&cam->at, &cam->eye);
    s32 attempt;

    if (Object_GetIndex(&play->objectCtx, OBJECT_SD) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_SD);
    }
    for (attempt = 0; attempt < 6; attempt++) {
        s16 yaw = behind + (s16)Rand_CenteredFloat(0x6000) + ((attempt >= 3) ? 0x8000 : 0);
        f32 dist = 380.0f + Rand_ZeroFloat(220.0f);
        CollisionPoly* poly;
        Vec3f pos;
        f32 floorY;

        pos.x = player->actor.world.pos.x + Math_SinS(yaw) * dist;
        pos.y = player->actor.world.pos.y + 150.0f;
        pos.z = player->actor.world.pos.z + Math_CosS(yaw) * dist;
        floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &pos);
        if ((floorY < BGCHECK_Y_MIN + 10.0f) || (fabsf(floorY - player->actor.world.pos.y) > 120.0f)) {
            continue;
        }
        GdfInvasion_MirrorActor(Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HEISHI1, pos.x, floorY, pos.z, 0,
                                            Math_Vec3f_Yaw(&pos, &player->actor.world.pos), 0,
                                            EN_HEISHI1_PARAMS_HUNTER)); // (the Links see them too)
        return;
    }
}

static void BossGanon_Player_UpdateManhunt(BossGanon* this, PlayState* play) {
    s16 scene = play->sceneNum;
    s32 market = BossGanon_Player_IsMarketScene(scene);
    s32 castle = BossGanon_Player_IsCastleScene(scene);
    s32 want = castle ? 6 : 3;
    s32 count = 0;
    Actor* actor;

    if (!sGdfWanted || !(market || castle)) {
        return;
    }

    // An ominous bell, slow and low, until he's out over the drawbridge
    if (market && (--sGdfBellTimer <= 0)) {
        static f32 sBellFreq = 0.55f;
        static f32 sBellVol = 1.0f;

        sGdfBellTimer = 75;
        Audio_PlaySfxGeneral(NA_SE_EV_SHIP_BELL, &gSfxDefaultPos, 4, &sBellFreq, &sBellVol, &gSfxDefaultReverb);
    }

    for (actor = play->actorCtx.actorLists[ACTORCAT_NPC].head; actor != NULL; actor = actor->next) {
        if (BossGanon_Player_IsHunter(actor) && (actor->update != NULL)) {
            count++;
        }
    }
    // Endless: whenever one falls another comes running
    if ((count < want) && ((play->gameplayFrames % 15) == 0) && !this->playerHidden) {
        BossGanon_Player_SpawnHunter(play);
    }
}

static void BossGanon_Player_SlayNpc(PlayState* play, Actor* npc) {
    static Color_RGBA8 sBloodPrim = { 170, 0, 0, 255 };
    static Color_RGBA8 sBloodEnv = { 70, 0, 0, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos;
    Vec3f vel;
    Vec3f accel = { 0.0f, -0.8f, 0.0f };
    s32 i;

    pos.x = npc->world.pos.x;
    pos.y = (npc->world.pos.y + MAX(npc->focus.pos.y, npc->world.pos.y + 30.0f)) * 0.5f;
    pos.z = npc->world.pos.z;

    // A spray of blood, a red burst, and a pool of it on the ground
    for (i = 0; i < 40; i++) {
        vel.x = Rand_CenteredFloat(9.0f);
        vel.y = Rand_ZeroFloat(8.0f) + 2.0f;
        vel.z = Rand_CenteredFloat(9.0f);
        EffectSsDtBubble_SpawnCustomColor(play, &pos, &vel, &accel, &sBloodPrim, &sBloodEnv,
                                          (s16)(Rand_ZeroFloat(40.0f) + 40.0f), (s16)(Rand_ZeroFloat(15.0f) + 20.0f),
                                          false);
    }
    EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 160, 0, 170, 0, 0, 255, 90, 0, 0, 1, 9, false);
    for (i = 0; i < 12; i++) {
        Vec3f floorPos = npc->world.pos;
        Vec3f slow = { Rand_CenteredFloat(2.0f), 0.0f, Rand_CenteredFloat(2.0f) };

        floorPos.x += Rand_CenteredFloat(30.0f);
        floorPos.y += 1.0f;
        floorPos.z += Rand_CenteredFloat(30.0f);
        EffectSsDtBubble_SpawnCustomColor(play, &floorPos, &slow, &zero, &sBloodPrim, &sBloodEnv,
                                          (s16)(Rand_ZeroFloat(60.0f) + 90.0f), 60, false);
    }
    SoundSource_PlaySfxAtFixedWorldPos(play, &pos, 40, NA_SE_IT_SWORD_STRIKE_HARD);
    SoundSource_PlaySfxAtFixedWorldPos(play, &pos, 40, NA_SE_EN_EXTINCT);

    GdfInvasion_NoteSlain(npc); // (one from the shared world: slain in everyone's)
    if (!BossGanon_Player_IsHunter(npc)) {
        GanondorfKills_Record(npc);
        if (BossGanon_Player_IsMarketScene(play->sceneNum)) {
            sGdfWanted = true;
        }
    }
    Actor_Kill(npc);
}

static void BossGanon_Player_SlayNpcs(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* npc;
    s32 i;

    if (GanondorfKills_IsEnabled() && (sGdfAtCount > 0)) {
        npc = play->actorCtx.actorLists[ACTORCAT_NPC].head;
        while (npc != NULL) {
            Actor* next = npc->next;

            // Not the one he's carrying (Ruto), not his own bodyguards, and nothing already gone
            // Online, never the other players either: a Link is fought, not cut down like an NPC
            if ((npc->update != NULL) && (npc != player->heldActor) && (npc->init == NULL) &&
                !((npc->id == ACTOR_EN_GELDB) && EN_GELDB_IS_FRIEND(npc->params)) && (npc->id != ACTOR_PLAYER) &&
                (GdfInvasion_DummyClientId(npc) == 0)) {
                for (i = 0; i < sGdfAtCount; i++) {
                    if (BossGanon_Player_NpcInVolume(npc, &sGdfAtVolumes[i])) {
                        BossGanon_Player_SlayNpc(play, npc);
                        break;
                    }
                }
            }
            npc = next;
        }
    }
    sGdfAtCount = 0;
}

// Ganon (the beast form) notes his attacks here too, and runs the check while Ganondorf is hidden
void BossGanon_Player_NoteAttackCylinder(ColliderCylinder* cyl) {
    BossGanon_Player_NoteCylinder(cyl);
}

void BossGanon_Player_NoteAttackSphere(Vec3f* center, f32 radius) {
    BossGanon_Player_NoteSegment(center, center, radius);
}

void BossGanon_Player_KillNpcsNow(PlayState* play) {
    BossGanon_Player_SlayNpcs(NULL, play);
}

s32 BossGanon_Player_IsPhantom(void);

// Phantom Ganon skin: his own voice and magic sounds in place of Ganondorf's
static u16 BossGanon_Player_Voice(u16 sfx) {
    // Looping sounds are passed as "id - SFX_FLAG"
    u8 looping = !(sfx & SFX_FLAG);
    u16 id = looping ? (sfx + SFX_FLAG) : sfx;
    u16 out;

    if (!BossGanon_Player_IsPhantom()) {
        return sfx;
    }
    switch (id) {
        case NA_SE_EN_GANON_LAUGH:
            out = NA_SE_EN_FANTOM_LAUGH;
            break;
        case NA_SE_EN_GANON_AT_RETURN:
        case NA_SE_EN_GANON_THROW:
        case NA_SE_EN_GANON_THROW_BIG:
            out = NA_SE_EN_FANTOM_VOICE;
            break;
        case NA_SE_EN_GANON_DAMAGE1:
            out = NA_SE_EN_FANTOM_DAMAGE;
            break;
        case NA_SE_EN_GANON_DAMAGE2:
            out = NA_SE_EN_FANTOM_DAMAGE2;
            break;
        case NA_SE_EN_GANON_SPARK:
            out = NA_SE_EN_FANTOM_SPARK;
            break;
        case NA_SE_EN_GANON_CHARGE_MASIC:
        case NA_SE_EN_GANON_THROW_MASIC:
            out = NA_SE_EN_FANTOM_MASIC2;
            break;
        case NA_SE_EN_GANON_FIRE:
            out = NA_SE_EN_FANTOM_FIRE;
            break;
        case NA_SE_EN_GANON_HIT_THUNDER:
            out = NA_SE_EN_FANTOM_HIT_THUNDER;
            break;
        default:
            return sfx;
    }
    return looping ? (out - SFX_FLAG) : out;
}

void BossGanon_Player_Update(Actor* thisx, PlayState* play);
void BossGanon_PlayerLightBall_Update(Actor* thisx, PlayState* play);
void BossGanon_PlayerBigMagic_Update(Actor* thisx, PlayState* play);

void BossGanon_Player_SetupFloat(BossGanon* this, PlayState* play);
void BossGanon_Player_Float(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupChargeLightBall(BossGanon* this, PlayState* play);
void BossGanon_Player_ChargeLightBall(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupThrow(BossGanon* this, PlayState* play);
void BossGanon_Player_Throw(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupChargeBigMagic(BossGanon* this, PlayState* play);
void BossGanon_Player_ChargeBigMagic(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupPound(BossGanon* this, PlayState* play);
void BossGanon_Player_Pound(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupBlock(BossGanon* this, PlayState* play);
void BossGanon_Player_Block(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupHurt(BossGanon* this, PlayState* play);
void BossGanon_Player_Hurt(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupDowned(BossGanon* this, PlayState* play);
void BossGanon_Player_Downed(BossGanon* this, PlayState* play);
static void BossGanon_Player_Pacified(BossGanon* this, PlayState* play);
static void BossGanon_Player_SetupPacified(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupLaugh(BossGanon* this, PlayState* play);
static void BossGanon_Player_KeepFloatRoot(BossGanon* this);
static s32 BossGanon_Player_UsesSpear(void);
static s32 BossGanon_Player_HasTrident(void);
static s32 BossGanon_Player_ApplyTridentAttack(BossGanon* this);
static void BossGanon_Player_ApplyClip(BossGanon* this, const Vec3s (*clip)[13], s32 count, f32 frame, s32 loop,
                                       f32 weight, f32 body);
static const Vec3s sGdfClipRun[21][13];
static const Vec3s sGdfClipJump[21][13];
static const Vec3s sGdfClipLadder[24][13];
static const Vec3s sGdfClipLadderTop[34][13];
void BossGanon_Player_SetupDance(BossGanon* this, PlayState* play);
#define GDF_DANCE_TAUNT_CHANCE 0.125f // D-Pad Up: how often the taunt is the dance
static f32 sGdfJumpFrame = 0.0f; // where in the running jump he is
static void BossGanon_Player_ApplyTridentRun(BossGanon* this);
void BossGanon_Player_SetupTridentSpin(BossGanon* this, PlayState* play);
void BossGanon_Player_TridentSpin(BossGanon* this, PlayState* play);
void BossGanon_Player_TridentThrow(BossGanon* this, PlayState* play);
void BossGanon_Player_Grab(BossGanon* this, PlayState* play);
void BossGanon_Player_GrabThrow(BossGanon* this, PlayState* play);
s32 BossGanon_Player_TryGrab(BossGanon* this, PlayState* play);
static void BossGanon_Player_UpdateGrabbed(BossGanon* this, PlayState* play);
static void BossGanon_Player_UpdateRoam(BossGanon* this, PlayState* play);
void BossGanon_Player_Brood(BossGanon* this, PlayState* play);
void BossGanon_Player_Laugh(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupOrgan(BossGanon* this, PlayState* play);
void BossGanon_Player_Organ(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupDarkAura(BossGanon* this, PlayState* play);
void BossGanon_Player_Ride(BossGanon* this, PlayState* play);
void BossGanon_Player_Climb(BossGanon* this, PlayState* play);
void BossGanon_Player_LedgeClimb(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupLedgeClimb(BossGanon* this, PlayState* play);
static void BossGanon_Player_PlaceLedgeClimb(BossGanon* this, PlayState* play);
static f32 BossGanon_Player_RunBump(f32 phase, f32 center, f32 width);
void BossGanon_Player_ItemGet(BossGanon* this, PlayState* play);
static void BossGanon_Player_TwistLean(BossGanon* this, f32 twist, f32 lean);
static s32 BossGanon_Player_GetSeat(PlayState* play, Vec3f* seat);
void BossGanon_Player_DarkAura(BossGanon* this, PlayState* play);
static u8 sPlayerDarkAuraMode = false;
static void BossGanon_Player_DeflectMagic(BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawOrgan(BossGanon* this, PlayState* play);
static s32 BossGanon_Player_CanStartAttack(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupSwordIdle(BossGanon* this, PlayState* play);
void BossGanon_Player_SwordIdle(BossGanon* this, PlayState* play);
void BossGanon_Player_SummonTrident(BossGanon* this, PlayState* play);
static void BossGanon_Player_SetupSummonTrident(BossGanon* this, PlayState* play);
void BossGanon_Player_UnsummonTrident(BossGanon* this, PlayState* play);
static void BossGanon_Player_SetupUnsummonTrident(BossGanon* this, PlayState* play);
static void BossGanon_Player_TridentBurst(BossGanon* this, PlayState* play, Vec3f* hand, s32 own);
static const Vec3s sGdfTridentArmPoses[2][3];
#define GDF_VANISH_BURST 100 // sGdfSummonBurst above this: the trident coming apart, not bursting in
static void BossGanon_Player_TridentVanish(BossGanon* this, PlayState* play, Vec3f* hand, s32 own);
static u8 sGdfSummonBurst = 0; // counts down after the trident bursts into his hand (sent to the others)
void BossGanon_Player_SwordAttack(BossGanon* this, PlayState* play);
void BossGanon_Player_SwordDash(BossGanon* this, PlayState* play);
void BossGanon_Player_Land(BossGanon* this, PlayState* play);
s32 BossGanon_Player_IsSwordGuarding(BossGanon* this);
static s32 BossGanon_Player_TryToggleSword(BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawSword(BossGanon* this, PlayState* play);
static void BossGanon_Player_UpdateSwordHit(BossGanon* this, PlayState* play);
static void BossGanon_Player_UpdateAim(BossGanon* this, PlayState* play);
extern PlayState* gPlayState;

// Ganondorf's own attacks hurt enemies instead of Link
static ColliderCylinderInit sPlayerLightBallCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_GANONDORF_PLAYER, 0x00, 0x04 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 20, 30, -15, { 0, 0, 0 } },
};

static ColliderCylinderInit sPlayerBigMagicCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_GANONDORF_PLAYER, 0x00, 0x02 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 15, 20, -10, { 0, 0, 0 } },
};

static ColliderCylinderInit sPlayerPoundCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_GANONDORF_PLAYER, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 60, 80, 0, { 0, 0, 0 } },
};

// The throat grab (D-Pad Down, see BossGanon_Player_TryGrab)
#define GDF_GRAB_NONE 0
#define GDF_GRAB_HELD 1
#define GDF_GRAB_FLYING 2
#define GDF_GRAB_DOWN 3
static Actor* sGdfGrabActor = NULL;
static u32 sGdfGrabBusyFrame = 0;
static u32 sGdfLastBFrame = 0xFFFF0000; // when Ganondorf last pressed B (the volley) // the last frame he held someone (Invasion mode's grab cooldown)
static u8 sGdfGrabState = GDF_GRAB_NONE;
static u8 sGdfGrabIsEnemy = false;
static f32 sGdfGrabNeck = 0.0f;
static f32 sGdfGrabArmWeight = 0.0f;
static Vec3f sGdfGrabVel;
static s16 sGdfGrabTimer = 0;
static u8 sGdfGrabOrigCategory = ACTORCAT_NPC;
static u32 sGdfGrabNetClient = 0; // Invasion mode: the one held is another player's Link (his game moves him)

// Holding someone up by the throat (A mustn't talk to them)
s32 BossGanon_Player_IsHolding(void) {
    return sGdfGrabState == GDF_GRAB_HELD;
}

s32 BossGanon_Player_IsBusy(Actor* thisx) {
    BossGanon* this = (BossGanon*)thisx;

    if ((thisx == NULL) || !GDF_IS_PLAYER(this)) {
        return false;
    }

    return (this->actionFunc == BossGanon_Player_ChargeLightBall) || (this->actionFunc == BossGanon_Player_Throw) ||
           (this->actionFunc == BossGanon_Player_ChargeBigMagic) || (this->actionFunc == BossGanon_Player_Pound) ||
           (this->actionFunc == BossGanon_Player_Block) || (this->actionFunc == BossGanon_Player_Laugh) ||
           (this->actionFunc == BossGanon_Player_Organ) || (this->actionFunc == BossGanon_Player_SwordAttack) ||
           (this->actionFunc == BossGanon_Player_SwordDash) || (this->actionFunc == BossGanon_Player_Land) ||
           (this->actionFunc == BossGanon_Player_TridentSpin) || (this->actionFunc == BossGanon_Player_GrabThrow) ||
           (this->actionFunc == BossGanon_Player_SummonTrident) ||
           (this->actionFunc == BossGanon_Player_UnsummonTrident) || (sGdfBreakTimer > 0);
    // Not the dark aura: he moves freely while holding it. Nor the trident's flight: he walks about while it's out
}

// Whether Ganondorf is free to start something (used for levitation)
s32 BossGanon_Player_CanAct(Actor* thisx) {
    BossGanon* this = (BossGanon*)thisx;

    if ((thisx == NULL) || !GDF_IS_PLAYER(this) || (gPlayState == NULL)) {
        return false;
    }

    return BossGanon_Player_CanStartAttack(this, gPlayState) && (this->actionFunc != BossGanon_Player_Organ) &&
           (this->actionFunc != BossGanon_Player_Downed);
}

s32 BossGanon_IsPlayerAttack(Actor* actor) {
    if ((actor != NULL) && (actor->id == ACTOR_BOSS_GANON2)) {
        return actor->params == GANON2_PARAMS_PLAYER; // Ganon's swords, stomp and charge
    }
    if ((actor == NULL) || (actor->id != ACTOR_BOSS_GANON)) {
        return false;
    }

    return (actor->params == GDF_PARAMS_PLAYER) ||
           ((actor->params >= GDF_PARAMS_PLAYER_LIGHT_BALL) &&
            (actor->params < GDF_PARAMS_PLAYER_BIG_MAGIC + GDF_PLAYER_BIG_MAGIC_COUNT));
}

s32 BossGanon_Player_IsHidden(Actor* thisx) {
    BossGanon* this = (BossGanon*)thisx;

    if ((thisx == NULL) || !GDF_IS_PLAYER(this)) {
        return true;
    }

    return this->playerHidden;
}

static f32 BossGanon_Player_GetHoverHeight(void) {
    return CVarGetFloat(CVAR_PLAY_AS_GDF("HoverHeight"), 70.0f);
}

// Where Ganondorf's root sits above Link's feet: on the ground he stands and walks, levitating he floats
static f32 BossGanon_Player_GetHeightAboveLink(BossGanon* this) {
    if (this->playerGrounded) {
        return this->playerGroundOffset;
    }
    return BossGanon_Player_GetHoverHeight() + (Math_SinS(this->unk_1A2 * 1500) * 3.0f);
}

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", walking on the ground
//
// His boss model has no walk, so the walk of young Ganondorf (object_gndd, the cutscene model) is retargeted onto it
// at runtime, from the game's own files. The two skeletons match limb for limb except that the boss model has an
// extra shoulder joint on each arm; the upper arms are corrected for it, so the arms swing the same way. Faster
// than his natural pace the cycle speeds up and the stride widens, into a run.
// ------------------------------------------------------------------------------------------------

// z_skelanime.c
void SkelAnime_GetFrameData(AnimationHeader* animation, s32 frame, s32 limbCount, Vec3s* frameTable);

#define GDF_LIMB_COUNT_PLAYER 25 // gGanondorfSkel
#define GDF_YOUNG_WALK_JOINTS 24 // 23 limbs + the root translation
#define GDF_WALK_CYCLE_FRAMES 30.0f
#define GDF_WALK_NATURAL_SPEED 2.3f // world units per frame at the animation's own pace
#define GDF_WALK_MAX_RATE 2.5f
#define GDF_WALK_MAX_STRIDE 1.6f
#define GDF_ANKLE_HEIGHT 9.9f // his ankles above the floor when standing
#define GDF_SOLE_CLEARANCE 2.5f // his lowest point (soles, toes, knees, knuckles) above the floor
#define GDF_LEFT_FOOT_BODYPART 14
#define GDF_RIGHT_FOOT_BODYPART 11
#define GDF_LEFT_KNEE_BODYPART 13
#define GDF_RIGHT_KNEE_BODYPART 10

// Boss limb -> young Ganondorf limb (-1: keep the boss animation's own rotation). The legs come from the walk; the
// feet, face and torso keep his own (young Ganondorf's feet are rotated differently, and his torso is narrower).
static const s8 sGdfBossToYoungLimb[GDF_LIMB_COUNT_PLAYER] = {
    0,  1,  -1, -1, -1, -1, -1, -1, -1, -1, // root, torso (see below), arms (their own poses, see sGdfWalkArmPoses)
    -1, -1, -1, -1, -1, -1, -1, -1,         // neck, face and hair
    15, 16, 17, -1, 20, 21, -1,             // pelvis, legs (thigh, shin, foot)
};

static void BossGanon_Player_RotToMtx(Vec3s* rot, f32 m[3][3]) {
    f32 sx = sinf((f32)BINANG_TO_RAD(rot->x));
    f32 cx = cosf((f32)BINANG_TO_RAD(rot->x));
    f32 sy = sinf((f32)BINANG_TO_RAD(rot->y));
    f32 cy = cosf((f32)BINANG_TO_RAD(rot->y));
    f32 sz = sinf((f32)BINANG_TO_RAD(rot->z));
    f32 cz = cosf((f32)BINANG_TO_RAD(rot->z));

    // Rz * Ry * Rx, the order Matrix_TranslateRotateZYX applies them in
    m[0][0] = cz * cy;
    m[0][1] = (cz * sy * sx) - (sz * cx);
    m[0][2] = (cz * sy * cx) + (sz * sx);
    m[1][0] = sz * cy;
    m[1][1] = (sz * sy * sx) + (cz * cx);
    m[1][2] = (sz * sy * cx) - (cz * sx);
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
}

static void BossGanon_Player_MtxToRot(f32 m[3][3], Vec3s* rot) {
    f32 sy = CLAMP(-m[2][0], -1.0f, 1.0f);

    rot->x = RADF_TO_BINANG(atan2f(m[2][1], m[2][2]));
    rot->y = RADF_TO_BINANG(asinf(sy));
    rot->z = RADF_TO_BINANG(atan2f(m[1][0], m[0][0]));
}

static s16 BossGanon_Player_ScaleAngle(s16 base, s16 target, f32 scale) {
    s16 diff = target - base;

    return (s16)(base + (s16)(diff * scale));
}

// Blends the walk over the current (standing) pose in jointTable
static void BossGanon_Player_BlendArm(BossGanon* this, s32 firstLimb, const Vec3s* pose, f32 weight);
static void BossGanon_Player_SlerpRot(Vec3s* a, Vec3s* b, f32 t, Vec3s* out);

// His arms while walking: solved like the sword poses (fists close to his body, forward and back), because young
// Ganondorf's arms hang differently from his torso and swing far out on the boss model
static const Vec3s sGdfWalkArmPoses[4][3] = {
    { { 15506, 758, -14779 }, { -685, -4771, -6823 }, { 2315, 1027, -1733 } },     // right forward
    { { 19634, -5749, -14194 }, { -1134, -4536, -6861 }, { 2247, 1229, -2127 } },  // right back
    { { -15893, 251, -13672 }, { 25, 2864, -5541 }, { -790, 20, -1990 } },         // left forward
    { { -18870, 6804, -16061 }, { 524, 1983, -4121 }, { -845, 196, -2362 } },      // left back
};

// How far through the swing the right arm is: back at frame 0, forward at frame 15 (the left arm is opposite)
static f32 BossGanon_Player_WalkArmPhase(BossGanon* this) {
    return 0.5f - (0.5f * Math_CosS((s16)(this->playerWalkFrame * (0x10000 / GDF_WALK_CYCLE_FRAMES))));
}

// Young Ganondorf's pelvis sits turned about 20 degrees on the boss skeleton through the whole walk cycle, which
// swung his legs out to one side (his lower body looked skewed) and crossed his feet. Turned back by this (in actor
// space), his legs run straight on two tracks.
#define GDF_WALK_PELVIS_YAW -3641
#define GDF_WALK_FOOT_SPLAY 1820 // 10 degrees outward
#define GDF_CLIMB_WALL_GAP 10.0f // climbing: further from the wall than Link

// out = (transposeA ? a^T : a) * b
static void BossGanon_Player_Mul3(f32 a[3][3], f32 b[3][3], f32 out[3][3], s32 transposeA) {
    f32 r[3][3];
    s32 i;
    s32 j;
    s32 k;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r[i][j] = 0.0f;
            for (k = 0; k < 3; k++) {
                r[i][j] += (transposeA ? a[k][i] : a[i][k]) * b[k][j];
            }
        }
    }
    memcpy(out, r, sizeof(r));
}

static void BossGanon_Player_ApplyWalk(BossGanon* this) {
    static Vec3s sYoungA[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sYoungB[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sYoung[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sYoungRest;
    static Vec3s sYoungTorsoMean;
    static u8 sYoungRestRead = false;
    Vec3s walk[GDF_LIMB_COUNT_PLAYER + 1];
    Vec3s arm[3];
    Vec3s* joints = this->skelAnime.jointTable;
    f32 speed = this->playerWalkWeight;
    s32 frameA = (s32)this->playerWalkFrame;
    s32 frameB = (frameA + 1) % (s32)GDF_WALK_CYCLE_FRAMES;
    f32 frac = this->playerWalkFrame - frameA;
    f32 stride;
    f32 phase;
    s32 b;
    s32 i;

    if (this->playerWalkWeight <= 0.0f) {
        return;
    }

    if (!sYoungRestRead) {
        f32 sum[3] = { 0.0f, 0.0f, 0.0f };

        for (i = 0; i < (s32)GDF_WALK_CYCLE_FRAMES; i++) {
            SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, i, GDF_YOUNG_WALK_JOINTS, sYoungA);
            if (i == 0) {
                sYoungRest = sYoungA[0];
            }
            sum[0] += sYoungA[2].x;
            sum[1] += sYoungA[2].y;
            sum[2] += sYoungA[2].z;
        }
        sYoungTorsoMean.x = (s16)(sum[0] / GDF_WALK_CYCLE_FRAMES);
        sYoungTorsoMean.y = (s16)(sum[1] / GDF_WALK_CYCLE_FRAMES);
        sYoungTorsoMean.z = (s16)(sum[2] / GDF_WALK_CYCLE_FRAMES);
        sYoungRestRead = true;
    }

    SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, frameA, GDF_YOUNG_WALK_JOINTS, sYoungA);
    SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, frameB, GDF_YOUNG_WALK_JOINTS, sYoungB);
    SkelAnime_InterpFrameTable(GDF_YOUNG_WALK_JOINTS, sYoung, sYoungA, sYoungB, frac);

    stride = CLAMP(this->playerWalkStride, 1.0f, GDF_WALK_MAX_STRIDE + (0.7f * this->playerSprintWeight));

    for (b = 0; b < GDF_LIMB_COUNT_PLAYER; b++) {
        s8 y = sGdfBossToYoungLimb[b];

        walk[1 + b] = (y >= 0) ? sYoung[1 + y] : joints[1 + b];
    }

    // His own upright torso, with a little of the walk's sway
    walk[2].x = (s16)(joints[2].x + (s16)(sYoung[2].x - sYoungTorsoMean.x) * 0.35f);
    walk[2].y = (s16)(joints[2].y + (s16)(sYoung[2].y - sYoungTorsoMean.y) * 0.35f);
    walk[2].z = (s16)(joints[2].z + (s16)(sYoung[2].z - sYoungTorsoMean.z) * 0.35f);

    // Wider stride when running
    for (b = 1; b <= GDF_LIMB_COUNT_PLAYER; b++) {
        walk[b].x = BossGanon_Player_ScaleAngle(joints[b].x, walk[b].x, stride);
        walk[b].y = BossGanon_Player_ScaleAngle(joints[b].y, walk[b].y, stride);
        walk[b].z = BossGanon_Player_ScaleAngle(joints[b].z, walk[b].z, stride);
    }

    // Straighten his pelvis: pelvis = root^T * turn * root * pelvis
    {
        Vec3s turnRot = { 0, GDF_WALK_PELVIS_YAW, 0 };
        f32 root[3][3];
        f32 turn[3][3];
        f32 m[3][3];

        BossGanon_Player_RotToMtx(&walk[1], root);
        BossGanon_Player_RotToMtx(&turnRot, turn);
        BossGanon_Player_RotToMtx(&walk[1 + 18], m);
        BossGanon_Player_Mul3(root, m, m, false);
        BossGanon_Player_Mul3(turn, m, m, false);
        BossGanon_Player_Mul3(root, m, m, true);
        BossGanon_Player_MtxToRot(m, &walk[1 + 18]);
    }

    // Point his feet where he walks. His feet keep the boss animation's own rotation (young Ganondorf's are built
    // differently), which left them turned out (about 30 and 20 degrees) on the walk. Turn each foot about the
    // vertical so its toes (the foot's local +z, 11 degrees out in his standing pose) face forward, splayed a little
    // outward, keeping the foot's pitch from the step. (v37 took local +x for the toes and turned them sideways.)
    {
        static const s8 sLegChains[2][5] = { { 0, 18, 19, 20, 21 }, { 0, 18, 22, 23, 24 } };
        s32 leg;
        s32 k;

        for (leg = 0; leg < 2; leg++) {
            f32 parent[3][3];
            f32 m[3][3];
            f32 turn[3][3];
            Vec3s turnRot = { 0, 0, 0 };
            s16 target = (leg == 0) ? GDF_WALK_FOOT_SPLAY : -GDF_WALK_FOOT_SPLAY;
            s16 toeYaw;

            BossGanon_Player_RotToMtx(&walk[1 + sLegChains[leg][0]], parent);
            for (k = 1; k < 4; k++) {
                BossGanon_Player_RotToMtx(&walk[1 + sLegChains[leg][k]], m);
                BossGanon_Player_Mul3(parent, m, parent, false);
            }
            BossGanon_Player_RotToMtx(&walk[1 + sLegChains[leg][4]], m);
            BossGanon_Player_Mul3(parent, m, m, false); // the foot in actor space
            toeYaw = Math_Atan2S(m[2][2], m[0][2]);   // binang of atan2(x, z) of the toe axis
            turnRot.y = (s16)(target - toeYaw);
            BossGanon_Player_RotToMtx(&turnRot, turn);
            BossGanon_Player_Mul3(turn, m, m, false);
            BossGanon_Player_Mul3(parent, m, m, true); // back into the shin's space
            BossGanon_Player_MtxToRot(m, &walk[1 + sLegChains[leg][4]]);
        }
    }

    // His own standing height, with the walk's bob
    walk[0] = joints[0];
    walk[0].y += (s16)((sYoung[0].y - sYoungRest.y) * stride);

    SkelAnime_InterpFrameTable(GDF_LIMB_COUNT_PLAYER + 1, joints, joints, walk, speed);

    // Arms swing close to his body, opposite to the legs
    phase = BossGanon_Player_WalkArmPhase(this);
    for (i = 0; i < 3; i++) {
        Vec3s fwd = sGdfWalkArmPoses[0][i];
        Vec3s back = sGdfWalkArmPoses[1][i];

        BossGanon_Player_SlerpRot(&back, &fwd, phase, &arm[i]);
    }
    BossGanon_Player_BlendArm(this, 7, arm, speed);
    for (i = 0; i < 3; i++) {
        Vec3s fwd = sGdfWalkArmPoses[2][i];
        Vec3s back = sGdfWalkArmPoses[3][i];

        BossGanon_Player_SlerpRot(&fwd, &back, phase, &arm[i]);
    }
    BossGanon_Player_BlendArm(this, 3, arm, speed);
}

// Cutscenes and conversations: Link is held by the scene, but Ganondorf can walk about on his own with the stick
// (camera-relative, like Link). He stays wherever he walked to until the scene is over, then he's back with Link. Not
// through doors, chests, items, ladders, ledges or on horseback, where his own animations follow Link.
#define GDF_ROAM_MAX_SPEED 5.0f
#define GDF_ROAM_RADIUS 30.0f

static s32 BossGanon_Player_CanRoam(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!CVarGetInteger(CVAR_PLAY_AS_GDF("RoamInCutscenes"), 1) || !this->playerGrounded || this->playerHidden ||
        gGdfIntroCs.active) {
        return false;
    }
    if (!(player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING)) &&
        (play->csCtx.state == CS_STATE_IDLE)) {
        return false;
    }
    if ((player->doorActor != NULL) ||
        (player->stateFlags1 & (PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_ON_HORSE |
                                PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_DEAD))) {
        return false;
    }
    return (this->actionFunc == BossGanon_Player_Float) || (this->actionFunc == BossGanon_Player_Brood) ||
           (this->actionFunc == BossGanon_Player_Laugh);
}

static void BossGanon_Player_UpdateRoam(BossGanon* this, PlayState* play) {
    f32 mag;
    s16 angle;
    s16 yaw;
    f32 speed;
    Vec3f next;
    Vec3f from;
    Vec3f to;
    Vec3f hit;
    Vec3f probe;
    CollisionPoly* poly;
    s32 bgId;
    f32 floorY;

    if (!BossGanon_Player_CanRoam(this, play)) {
        sGdfRoaming = false;
        sGdfRoamSpeed = 0.0f;
        return;
    }
    func_80077D10(&mag, &angle, &play->state.input[0]);
    if (mag < 8.0f) {
        mag = 0.0f;
    }
    if (!sGdfRoaming && (mag <= 0.0f)) {
        return;
    }
    if (!sGdfRoaming) {
        sGdfRoaming = true;
        sGdfRoamYaw = this->actor.shape.rot.y;
        sGdfRoamFloorY = this->playerFloorY;
    }
    speed = (mag / 60.0f) * GDF_ROAM_MAX_SPEED;
    Math_ApproachF(&sGdfRoamSpeed, (this->actionFunc == BossGanon_Player_Laugh) ? 0.0f : speed, 1.0f, 0.6f);
    if (mag <= 0.0f) {
        return;
    }
    yaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + angle;
    sGdfRoamYaw = yaw;
    if (sGdfRoamSpeed <= 0.0f) {
        return;
    }

    next = this->actor.world.pos;
    next.x += Math_SinS(yaw) * sGdfRoamSpeed;
    next.z += Math_CosS(yaw) * sGdfRoamSpeed;

    // Walls: look ahead by his radius at waist height
    from = this->actor.world.pos;
    from.y += 40.0f;
    to = next;
    to.x += Math_SinS(yaw) * GDF_ROAM_RADIUS;
    to.z += Math_CosS(yaw) * GDF_ROAM_RADIUS;
    to.y = from.y;
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
        return;
    }
    // Floor: walk up and down slopes and steps, but not off a ledge
    probe = next;
    probe.y += 50.0f;
    floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
    if ((floorY <= BGCHECK_Y_MIN) || (floorY < sGdfRoamFloorY - 40.0f) || (floorY > sGdfRoamFloorY + 30.0f)) {
        return;
    }
    sGdfRoamFloorY = floorY;
    this->playerFloorY = floorY;
    this->actor.world.pos.x = next.x;
    this->actor.world.pos.z = next.z; // his height follows the floor through the ground offset (Update)
}

// Standing and walking, while not levitating: the walk cycle follows Link's speed
static void BossGanon_Player_UpdateWalk(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    // Walking about on his own in a cutscene: his own speed, not Link's
    f32 speed = sGdfRoaming ? sGdfRoamSpeed : fabsf(player->linearVelocity);
    f32 rate;
    u8 moving = (speed > 0.5f) && (sGdfRoaming || (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND));

    Math_ApproachF(&this->playerWalkWeight, moving ? 1.0f : 0.0f, 1.0f, 0.15f);
    // Sprinting (holding A): a faster cycle, longer strides and a forward lean
    Math_ApproachF(&this->playerSprintWeight, (moving && PlayAsGanondorf_IsSprinting()) ? 1.0f : 0.0f, 1.0f, 0.1f);

    rate = CLAMP(speed / GDF_WALK_NATURAL_SPEED, 0.5f, GDF_WALK_MAX_RATE + this->playerSprintWeight);
    this->playerWalkStride = speed / (GDF_WALK_NATURAL_SPEED * GDF_WALK_MAX_RATE);
    if (moving || (this->playerWalkWeight > 0.0f)) {
        f32 prev = this->playerWalkFrame;

        this->playerWalkFrame += rate;
        while (this->playerWalkFrame >= GDF_WALK_CYCLE_FRAMES) {
            this->playerWalkFrame -= GDF_WALK_CYCLE_FRAMES;
        }

        // His footsteps, on the frames his heels come down (right foot at 1, left at 16), with the floor's sound
        if (moving && !this->playerHidden) {
            static const f32 sContacts[] = { 1.0f, 16.0f };
            s32 i;

            for (i = 0; i < ARRAY_COUNT(sContacts); i++) {
                f32 c = sContacts[i];
                u8 crossed = (prev < this->playerWalkFrame) ? ((prev < c) && (this->playerWalkFrame >= c))
                                                            : ((prev < c) || (this->playerWalkFrame >= c));

                if (crossed) {
                    u16 sfx = NA_SE_PL_WALK_GROUND + player->floorSfxOffset + player->ageProperties->unk_94;

                    func_800F4010(&player->actor.projectedPos, sfx, speed * 0.6f);
                    GdfInvasion_NoteSfx(sfx);
                    if (this->playerSprintWeight > 0.5f) {
                        Audio_PlayActorSound2(&this->actor, NA_SE_PL_WALK_HEAVYBOOTS);
                    }
                    player->stateFlags2 |= PLAYER_STATE2_FOOTSTEP;
                }
            }
        }
    }

    BossGanon_Player_ApplyWalk(this);
    // Running (the stick pushed far): his torso leans into it, more the faster he goes (up to 11 degrees). Upright,
    // with the run's long stride reaching out ahead, he looked like he was leaning back. (The sprint adds its own.)
    {
        f32 run = CLAMP((this->playerWalkStride - 1.0f) / (GDF_WALK_MAX_STRIDE - 1.0f), 0.0f, 1.0f);

        run *= this->playerWalkWeight * (1.0f - this->playerSprintWeight);
        if (run > 0.0f) {
            BossGanon_Player_TwistLean(this, 0.0f, 0x0800 * run);
        }
    }
    if (this->playerSprintWeight > 0.0f) {
        f32 runWeight = this->playerSprintWeight * this->playerWalkWeight;
        // Shoulders turn against the hips with each stride (the right shoulder comes forward with the right arm)
        f32 twist = -0x0500 * Math_CosS((s16)(this->playerWalkFrame * (0x10000 / GDF_WALK_CYCLE_FRAMES)));

        f32 phase = (this->playerWalkFrame - 1.0f) / GDF_WALK_CYCLE_FRAMES; // (the right heel at walk frame 1)

        (void)twist;
        phase -= floorf(phase);
        // (the Mixamo run, as with the trident: its right heel comes down at its frame 17)
        BossGanon_Player_ApplyClip(this, sGdfClipRun, ARRAY_COUNT(sGdfClipRun),
                                   17.0f + phase * ARRAY_COUNT(sGdfClipRun), true, runWeight, 0.3f);
        // Both arms swinging: the run's sword arm holds still, so his arms take the runner's swing from the CMU jog
        // (left and right in turn, in step)
        BossGanon_Player_ApplyRunArms(this, runWeight);
    }
}

// Keeps his lowest ankle or knee at the floor (from the last draw), so any animation stands on the ground. Knees
// count too: kneeling (the heavy landing) puts a knee lower than either foot, and his knee pad is about as thick as
// his sole.
// His lowest knee or ankle above his origin, from this frame's pose (not the last draw, a frame behind: after a fall
// that put him a whole fall's step too low on landing, and while the landing's pose changed fast it was always off)
static f32 BossGanon_Player_PoseLowest(BossGanon* this) {
    static const s8 sChains[2][5] = { { 0, 18, 19, 20, 21 }, { 0, 18, 22, 23, 24 } };
    Vec3s* joints = this->skelAnime.jointTable;
    f32 lowest = FLT_MAX;
    s32 leg;
    s32 k;

    for (leg = 0; leg < 2; leg++) {
        f32 rot[3][3];
        f32 local[3][3];
        f32 pos[3] = { joints[0].x, joints[0].y, joints[0].z };

        BossGanon_Player_RotToMtx(&joints[1 + 0], rot);
        for (k = 1; k < 5; k++) {
            s32 limb = sChains[leg][k];
            StandardLimb* l = SEGMENTED_TO_VIRTUAL(this->skelAnime.skeleton[limb]);
            f32 jp[3] = { l->jointPos.x, l->jointPos.y, l->jointPos.z };
            s32 r;

            for (r = 0; r < 3; r++) {
                pos[r] += rot[r][0] * jp[0] + rot[r][1] * jp[1] + rot[r][2] * jp[2];
            }
            if (k >= 3) { // the knee (the shin's origin) and the ankle (the foot's)
                lowest = MIN(lowest, pos[1]);
            }
            {
                // As drawn: the leg bends the limb draw adds (OverrideLimbDraw's legRot, the vanilla boss's floating
                // legs; still bent after some landings, which left him floating above the floor)
                Vec3s r = joints[1 + limb];
                s32 side = (limb >= 22) ? -1 : 1;
                s32 seg = (limb - 19) % 3; // thigh, shin, foot

                if ((limb >= 19) && (seg != 1)) {
                    r.y += (s16)(this->legRot.x + side * this->legRot.z);
                    r.z += (s16)this->legRot.y;
                } else if ((limb >= 19) && (this->legRot.y > 0.0f)) {
                    r.z += (s16)this->legRot.y;
                }
                BossGanon_Player_RotToMtx(&r, local);
            }
            BossGanon_Player_Mul3(rot, local, rot, false);
        }
    }
    return (lowest + this->actor.shape.yOffset) * this->actor.scale.y;
}

static void BossGanon_Player_UpdateGroundOffset(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 ankleAboveRoot = BossGanon_Player_PoseLowest(this);
    f32 target = GDF_ANKLE_HEIGHT - ankleAboveRoot + (this->playerFloorY - player->actor.world.pos.y);
    s32 fast = (this->actionFunc == BossGanon_Player_Land) || (sGdfFastGroundTimer > 0);

    // Better: the lowest point of the model itself (toes, heels, knees, knuckles), kept just above the floor. The
    // joints alone let tucked toes and a planted hand go through the floor when kneeling.
    if (sGdfMeshLowValid && !fast) { // (the mesh is from the last draw: not while the pose changes fast)
        f32 meshAboveRoot = sGdfMeshLowY - this->actor.world.pos.y;

        if ((meshAboveRoot > -300.0f) && (meshAboveRoot < 100.0f)) {
            target = MAX(target, GDF_SOLE_CLEARANCE - meshAboveRoot + (this->playerFloorY - player->actor.world.pos.y));
        }
    }

    if (this->playerHidden || (ankleAboveRoot < -300.0f) || (ankleAboveRoot > 100.0f)) {
        return; // not drawn (stale), or not a sensible pose
    }

    // (on the ground, as high as a pose needs: the landing's root sits far lower than his idle's, and capped at 60 he
    // sank into the floor until he stood)
    target = CLAMP(target, -20.0f, BossGanon_Player_GetHoverHeight() + (this->playerGrounded ? 200.0f : 60.0f));
    // The landing drops him onto one knee in two frames: follow it at once, or he sinks into the floor first. Same
    // while he blends from getting up into his standing idle afterwards: the get-up ends with his root far lower than
    // the idle's, and a slow follow let the blend lift him off the floor for a moment.
    if (sGdfFastGroundTimer > 0) {
        sGdfFastGroundTimer--;
    }
    if ((this->actionFunc == BossGanon_Player_Land) && (this->unk_1C2 < 2)) {
        Math_ApproachF(&this->playerGroundOffset, target, 1.0f, 40.0f);
    } else if ((this->actionFunc == BossGanon_Player_Land) || (sGdfFastGroundTimer > 0)) {
        // Getting up (and blending into his idle after): follow the pose at once both ways, so his feet never sink
        // and he never rises slowly out of the floor afterwards (this frame's pose, so it no longer overshoots)
        Math_ApproachF(&this->playerGroundOffset, target, 1.0f, 60.0f);
    } else {
        Math_ApproachF(&this->playerGroundOffset, target, 0.5f, 6.0f);
        // Settled on his feet, standing still in his normal idle: remember the height, the ceiling for the next
        // get-up (not while running or in a stance: bent knees there put it too low, and he sank into the ground on
        // landing)
        if (this->playerGrounded && (this->actionFunc == BossGanon_Player_Float) &&
            (this->playerWalkWeight < 0.05f) && (fabsf(target - this->playerGroundOffset) < 1.5f)) {
            sGdfStandOffset = this->playerGroundOffset + 1.0f;
            sGdfStandOffsetValid = true;
        }
    }
}

// Rotation that points from `from` to `to`, using the same convention as the vanilla light ball
static void BossGanon_Player_AimRot(Vec3f* from, Vec3f* to, s16* rotX, s16* rotY) {
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    f32 dz = to->z - from->z;

    *rotY = Math_Atan2S(dz, dx);
    *rotX = Math_Atan2S(sqrtf(SQ(dx) + SQ(dz)), dy);
}

// Closest targetable enemy in front of `from`
// Puzzle targets that are meant to be shot: eye switches, crystal switches, sun switches and unlit torches
static s32 BossGanon_Player_IsPuzzleTarget(Actor* actor) {
    switch (actor->id) {
        case ACTOR_OBJ_SWITCH: {
            s32 type = actor->params & 7;

            return (type == OBJSWITCH_TYPE_EYE) || (type == OBJSWITCH_TYPE_CRYSTAL) ||
                   (type == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE);
        }
        case ACTOR_OBJ_LIGHTSWITCH:
            return true;
        case ACTOR_OBJ_SYOKUDAI:
            return ((ObjSyokudai*)actor)->litTimer == 0;
        default:
            return false;
    }
}

// What Ganondorf is facing: the target that best combines being in front of him and being close.
// With puzzleTargets, eye switches, crystal switches, sun switches and unlit torches count too.
static Actor* BossGanon_Player_FindTarget(PlayState* play, Vec3f* from, s16 facingYaw, f32 maxDist, s16 maxAngle,
                                          u8 puzzleTargets) {
    static u8 sTargetCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS, ACTORCAT_SWITCH, ACTORCAT_PROP };
    Actor* best = NULL;
    f32 bestScore = 0.0f;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sTargetCategories); i++) {
        u8 isEnemyCategory = (sTargetCategories[i] == ACTORCAT_ENEMY) || (sTargetCategories[i] == ACTORCAT_BOSS);
        Actor* actor;

        if (!isEnemyCategory && !puzzleTargets) {
            continue;
        }

        for (actor = play->actorCtx.actorLists[sTargetCategories[i]].head; actor != NULL; actor = actor->next) {
            f32 dist;
            f32 score;
            s16 yawDiff;

            if ((actor->update == NULL) || (actor->id == ACTOR_BOSS_GANON) || (actor->id == ACTOR_EN_GANON_MANT)) {
                continue;
            }
            if (isEnemyCategory) {
                if (!(actor->flags & ACTOR_FLAG_ATTENTION_ENABLED)) {
                    continue;
                }
            } else if (!BossGanon_Player_IsPuzzleTarget(actor)) {
                continue;
            }

            dist = Math_Vec3f_DistXYZ(from, &actor->focus.pos);
            if (dist >= maxDist) {
                continue;
            }

            yawDiff = Math_Vec3f_Yaw(from, &actor->focus.pos) - facingYaw;
            if (ABS(yawDiff) > maxAngle) {
                continue;
            }

            // Angle counts most, so the thing he's turned toward wins over something closer off to the side
            score = (f32)ABS(yawDiff) + (dist * 12.0f);
            if ((best == NULL) || (score < bestScore)) {
                best = actor;
                bestScore = score;
            }
        }
    }

    // Invasion mode: the Links (their dummies here) are his first targets, ahead of any monster
    if (GdfInvasion_CanHurtLinks()) {
        Actor* actor;

        for (actor = play->actorCtx.actorLists[ACTORCAT_NPC].head; actor != NULL; actor = actor->next) {
            u32 clientId = GdfInvasion_DummyClientId(actor);
            Vec3f at;
            f32 dist;
            f32 score;
            s16 yawDiff;

            if ((actor->update == NULL) || (clientId == 0) || !GdfInvasion_CanHurtLink(clientId)) {
                continue;
            }
            at = actor->world.pos;
            at.y += 40.0f;
            if (Math_Vec3f_DistXYZ(&actor->focus.pos, &at) > 60.0f) {
                actor->focus.pos = at; // (aimed at by the light balls and the trident)
            }
            dist = Math_Vec3f_DistXYZ(from, &at);
            yawDiff = Math_Vec3f_Yaw(from, &at) - facingYaw;
            if ((dist >= maxDist * 1.3f) || (ABS(yawDiff) > maxAngle + 0x800)) {
                continue;
            }
            score = ((f32)ABS(yawDiff) + (dist * 12.0f)) * 0.5f;
            if ((best == NULL) || (score < bestScore) || (GdfInvasion_DummyClientId(best) == 0)) {
                best = actor;
                bestScore = score;
            }
        }
    }

    return best;
}

// Aim at Link's lock-on target, else the enemy or puzzle target he's facing, else straight ahead.
// yawTowardsPlayer is reused as the aim direction because the vanilla effect code reads it.
static void BossGanon_Player_UpdateAim(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* target = player->focusActor;

    if ((target == NULL) || (target->update == NULL)) {
        target = BossGanon_Player_FindTarget(play, &this->unk_1FC, player->actor.shape.rot.y, 700.0f, 0x2000, true);
    }

    if (target != NULL) {
        this->playerAimPos = target->focus.pos;
    } else {
        this->playerAimPos.x = player->actor.world.pos.x + (Math_SinS(player->actor.shape.rot.y) * 300.0f);
        this->playerAimPos.y = player->actor.world.pos.y + 25.0f;
        this->playerAimPos.z = player->actor.world.pos.z + (Math_CosS(player->actor.shape.rot.y) * 300.0f);
    }

    this->actor.yawTowardsPlayer = Math_Vec3f_Yaw(&this->actor.world.pos, &this->playerAimPos);
}

static void BossGanon_Player_SnapToLink(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.world.pos = player->actor.world.pos;
    this->actor.world.pos.y += BossGanon_Player_GetHeightAboveLink(this);
    this->actor.prevPos = this->actor.world.pos;
    this->actor.shape.rot.y = this->actor.world.rot.y = player->actor.shape.rot.y;
}

static void BossGanon_Player_Follow(BossGanon* this, PlayState* play, u8 controlY) {
    Player* player = GET_PLAYER(play);
    Vec3f target;
    f32 dx;
    f32 dy;
    f32 dz;

    // Up onto a ledge: his own path, timed by Link's climb
    if (this->actionFunc == BossGanon_Player_LedgeClimb) {
        BossGanon_Player_PlaceLedgeClimb(this, play);
        return;
    }

    // Climbing: a little further from the wall than Link, he's much bigger
    if (this->actionFunc == BossGanon_Player_Climb) {
        target = player->actor.world.pos;
        target.x -= Math_SinS(player->actor.shape.rot.y) * GDF_CLIMB_WALL_GAP;
        target.z -= Math_CosS(player->actor.shape.rot.y) * GDF_CLIMB_WALL_GAP;
        target.y += this->playerGroundOffset;
        this->actor.world.pos.x += (target.x - this->actor.world.pos.x) * 0.5f;
        this->actor.world.pos.y += (target.y - this->actor.world.pos.y) * 0.5f;
        this->actor.world.pos.z += (target.z - this->actor.world.pos.z) * 0.5f;
        return;
    }

    // Riding: his pelvis on the saddle
    if ((this->actionFunc == BossGanon_Player_Ride) && BossGanon_Player_GetSeat(play, &target)) {
        target.y += CVarGetFloat(CVAR_PLAY_AS_GDF("HorseSeatHeight"), 0.0f) - 72.56f;
        this->actor.world.pos = target;
        return;
    }

    target.x = player->actor.world.pos.x;
    target.y = player->actor.world.pos.y + BossGanon_Player_GetHeightAboveLink(this);
    target.z = player->actor.world.pos.z;

    dx = target.x - this->actor.world.pos.x;
    dy = target.y - this->actor.world.pos.y;
    dz = target.z - this->actor.world.pos.z;

    // Warps, voiding out, ...
    if ((SQ(dx) + SQ(dy) + SQ(dz)) > SQ(250.0f)) {
        BossGanon_Player_SnapToLink(this, play);
        return;
    }

    this->actor.world.pos.x += dx * 0.5f;
    this->actor.world.pos.z += dz * 0.5f;

    if (controlY) {
        // Keeps up with Link's jumps
        this->actor.world.pos.y += dy * ((this->playerAirWeight > 0.0f) ? 0.7f : 0.25f);
    }
}

// Also used by Ganon (Boss_Ganon2): moments when Link has to be shown instead of whichever form is out
s32 BossGanon_Player_ShouldHideAnyForm(PlayState* play) {
    Player* player = GET_PLAYER(play);

    // The Gerudo intro: shots without him, and the boy in his vision
    if (gGdfIntroCs.active && (gGdfIntroCs.hideAll || gGdfIntroCs.showLink)) {
        return true;
    }

    // First person
    if ((player->unk_6AD == 1) || (player->unk_6AD == 2)) {
        return true;
    }
    // Link isn't drawn either
    if (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) {
        return true;
    }
    // Crawlspaces are too small for Ganondorf, show Link instead
    if (player->stateFlags2 & PLAYER_STATE2_CRAWLING) {
        return true;
    }
    // Story cutscenes animate Link, show him instead
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("ShowLinkInCutscenes"), 0) && (play->csCtx.state != CS_STATE_IDLE)) {
        return true;
    }

    return false;
}

static s32 BossGanon_Player_ShouldHide(BossGanon* this, PlayState* play) {
    // Ganon is out instead of him
    return BossGanon_Player_ShouldHideAnyForm(play) || PlayAsGanondorf_IsGanonOut(play);
}

static s32 BossGanon_Player_CanStartAttack(BossGanon* this, PlayState* play) {
    // Invasion mode: not while his Dark Arts menu is open or he's been pacified
    return !this->playerHidden && BossGanon_Player_LinkCanAttack(play) && GdfInvasion_CanAct();
}

// Also used by Ganon: Link is in a state where an attack can start
s32 BossGanon_Player_LinkCanAttack(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Player_InCsMode(play) || (play->csCtx.state != CS_STATE_IDLE)) {
        return false;
    }
    if ((Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) || (player->textboxBtnCooldownTimer != 0)) {
        return false;
    }
    if (player->stateFlags1 &
        (PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_GETTING_ITEM |
         PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FIRST_PERSON |
         PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
         PLAYER_STATE1_CARRYING_ACTOR)) {
        return false;
    }
    if (player->stateFlags2 & PLAYER_STATE2_CRAWLING) {
        return false;
    }

    return true;
}

static void BossGanon_Player_StartAttack(BossGanon* this, u16 button) {
    this->playerBtnHeld = button;
    this->playerHeldTimer = 0;
}

static void BossGanon_Player_ReadButtons(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 useCButtons = !PlayAsGanondorf_KeepItems();
    u8 bigMagicEnabled = CVarGetInteger(CVAR_PLAY_AS_GDF("BigMagic"), 1);

    if (!BossGanon_Player_CanStartAttack(this, play)) {
        return;
    }
    // Invasion mode: C-buttons with a Dark Art on them cast it
    if (GdfInvasion_HandleCButtons(play)) {
        return;
    }
    // The ocarina is out: the D-pad controls the song player
    if (GanondorfSongs_IsOcarinaOut()) {
        return;
    }

    if (BossGanon_Player_TryToggleSword(this, play)) {
        return;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        if (sPlayerDarkAuraMode) {
            BossGanon_Player_SetupDarkAura(this, play);
        } else if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
            BossGanon_Player_SetupPound(this, play);
        } else {
            BossGanon_Player_StartAttack(this, BTN_B);
            BossGanon_Player_SetupChargeLightBall(this, play);
        }
    } else if (useCButtons && CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
        BossGanon_Player_StartAttack(this, BTN_CLEFT);
        BossGanon_Player_SetupChargeLightBall(this, play);
    } else if (useCButtons && CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
        BossGanon_Player_SetupPound(this, play);
    } else if (useCButtons && bigMagicEnabled && CHECK_BTN_ALL(input->press.button, BTN_CRIGHT)) {
        BossGanon_Player_SetupChargeBigMagic(this, play);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        // His taunt. Now and then (one in eight), standing idle on the ground, he breaks into the dance instead
        if (this->playerGrounded && (this->actionFunc == BossGanon_Player_Float) &&
            (Rand_ZeroOne() < GDF_DANCE_TAUNT_CHANCE)) {
            BossGanon_Player_SetupDance(this, play);
        } else {
            BossGanon_Player_SetupLaugh(this, play);
        }
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN) && BossGanon_Player_TryGrab(this, play)) {
        // by the throat
    } else if (PlayAsGanondorf_OrganButtonPressed() &&
               (GET_PLAYER(play)->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        // The organ rises out of solid ground, so not while levitating
        BossGanon_Player_SetupOrgan(this, play);
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        BossGanon_Player_SetupBlock(this, play);
    }
}


// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", sword stance (D-Pad Left)
//
// D-Pad Left: the sword appears in its sheath, the sheath held at his left hip. The first B draws the blade with a
// cut on the way out; after that B, B, B is a three-hit combo, stick + R a dash (B out of it: a lunging thrust),
// R alone a guard, and the C-buttons sword skills (spin slash, ground slam, dark energy slash). D-Pad Left puts the
// blade back in the sheath, and D-Pad Left again puts the sword away.
//
// The models come from gdf_sword.bin (GanondorfSword.cpp). The arm poses were solved offline with inverse kinematics
// on his skeleton (fist position + blade direction) and are blended with quaternions on top of whatever his body is
// doing (standing, walking, floating). The left arm holds the sheath still while the right arm swings with the walk.
// ------------------------------------------------------------------------------------------------

#define GDF_SWORD_OFF 0
#define GDF_SWORD_SHEATHED 1
#define GDF_SWORD_DRAWN 2

typedef enum {
    GDF_POSE_REST,
    GDF_POSE_GRAB,
    GDF_POSE_DRAW_CUT,
    GDF_POSE_H1_WIND,
    GDF_POSE_H1_HIT,
    GDF_POSE_H1_FOLLOW,
    GDF_POSE_H2_HIT,
    GDF_POSE_H2_FOLLOW,
    GDF_POSE_H3_RAISE,
    GDF_POSE_H3_SLAM,
    GDF_POSE_THRUST,
    GDF_POSE_GUARD,
    GDF_POSE_SHEATH_HOLD, // left arm
    GDF_POSE_SWING_FWD,   // walking with the blade: fist forward
    GDF_POSE_SWING_BACK,  // ...and back
    GDF_POSE_MAX,
} GdfSwordPose;

// Upper arm, forearm and hand rotations (the shoulder keeps the animation's)
static const Vec3s sGdfSwordPoses[GDF_POSE_MAX][3] = {
    { { 32010, 29971, 23028 }, { 1209, 4825, -8607 }, { 4289, -13661, -1643 } }, // rest
    { { -24427, -31715, 8174 }, { 2889, 4607, -6547 }, { 16078, -601, 4283 } }, // grab
    { { -28730, 29878, 24638 }, { 1853, 5778, -9964 }, { 13918, -710, -7349 } }, // draw_cut
    { { -24122, -27454, 32439 }, { 2425, 6842, -11812 }, { 14006, -239, -3192 } }, // h1_wind
    { { 24675, 28341, 14931 }, { -1745, 5958, -2439 }, { -13920, -554, 8581 } }, // h1_hit
    { { -29516, -23484, 10321 }, { 3176, 5670, -12897 }, { 17519, 2549, -4039 } }, // h1_follow
    { { -30391, 30387, 16607 }, { 1421, 5733, -9965 }, { 8524, -2000, -1621 } }, // h2_hit
    { { -25361, -31449, 24174 }, { 2409, 6432, -11162 }, { 13317, 370, -1941 } }, // h2_follow
    { { -16998, -32175, -23642 }, { 2219, 8469, -11946 }, { 11727, 1137, 2273 } }, // h3_raise
    { { 30798, 24401, 18203 }, { 793, 4581, -7324 }, { 3763, -17056, -153 } }, // h3_slam
    { { -29810, 25473, 11710 }, { 2110, 5219, -9589 }, { 14782, 17209, -12905 } }, // thrust
    { { 31176, 25553, 18697 }, { -744, 6249, -13057 }, { -4786, 428, 6294 } }, // guard
    { { 351, 2785, -8876 }, { 1613, -5640, -11036 }, { 978, 19, -3269 } }, // sheath_hold
    { { 31232, 30617, 22845 }, { 971, 4901, -8784 }, { 4469, -12067, -2338 } }, // swing_fwd
    { { 26886, -27634, 26633 }, { -270, 4689, -6994 }, { 1227, -6930, -2906 } }, // swing_back
};

#define GDF_LEFT_ARM_LIMB 3  // upper arm; forearm and hand follow
#define GDF_RIGHT_ARM_LIMB 7

// Sword in his hand: fist centre, then the sword frame turned so the blade comes out along the hand's -z
#define GDF_GRIP_X 650.0f
#define GDF_GRIP_Y 0.0f
#define GDF_GRIP_Z -100.0f
#define GDF_TRIDENT_GRIP_Z -500.0f // the trident model's grip, in the sword frame
#define GDF_TRIDENT_TAIL_Z -2700.0f // about where its butt end is
#define GDF_TRIDENT_ATTACK_GRIP_Z -1900.0f // where he holds it to attack: near the butt end
#define GDF_TRIDENT_CARRY_GRIP_Z 1300.0f // the middle of the shaft, where he carries it
#define GDF_SWORD_SCALE 1.4f // a greatsword: about 62 units of blade on a 100 unit tall Ganondorf
#define GDF_SWORD_GRIP_Z -500.0f   // where he holds the sword (sword frame)
#define GDF_SHEATH_GRIP_Z 350.0f   // where he holds the sheath, just under its mouth
#define GDF_BLADE_BASE_Z -125.0f
#define GDF_BLADE_TIP_Z 4530.0f

typedef enum {
    GDF_SWORD_ATK_NONE,
    GDF_SWORD_ATK_DRAW,
    GDF_SWORD_ATK_SHEATHE,
    GDF_SWORD_ATK_COMBO1,
    GDF_SWORD_ATK_COMBO2,
    GDF_SWORD_ATK_COMBO3,
    GDF_SWORD_ATK_THRUST,
    GDF_SWORD_ATK_SPIN,
    GDF_SWORD_ATK_SLAM,
    GDF_SWORD_ATK_WAVE,
} GdfSwordAttackId;

#define GDF_KEY_FROM -1 // the arm as it was when the attack started
#define GDF_KEY_END -2

typedef struct {
    s8 frame;
    s8 pose;
} GdfSwordKey;

typedef struct {
    GdfSwordKey keys[6];
    s8 length;      // frames until back to the stance
    s8 activeStart; // blade hits on these frames
    s8 activeEnd;
    s8 comboFrom;   // B from this frame queues the next hit
    s8 next;        // the next hit of the combo
    s8 impactFrame; // ground slam / dark wave
    u16 swingSfx;
} GdfSwordAttack;

static const GdfSwordAttack sGdfSwordAttacks[] = {
    /* NONE */ { { { 0, GDF_KEY_END } }, 0, -1, -1, -1, 0, -1, 0 },
    // Draws the blade from the sheath at his hip and cuts outward in one motion
    /* DRAW */
    { { { 0, GDF_KEY_FROM }, { 5, GDF_POSE_GRAB }, { 7, GDF_POSE_GRAB }, { 10, GDF_POSE_DRAW_CUT },
        { 17, GDF_POSE_DRAW_CUT }, { 25, GDF_POSE_REST } },
      25, 7, 12, -1, 0, -1, NA_SE_IT_SWORD_SWING_HARD },
    // Back into the sheath, with a click
    /* SHEATHE */
    { { { 0, GDF_KEY_FROM }, { 6, GDF_POSE_DRAW_CUT }, { 12, GDF_POSE_GRAB }, { 15, GDF_POSE_GRAB },
        { 20, GDF_KEY_END } },
      20, -1, -1, -1, 0, -1, 0 },
    /* COMBO1: horizontal slash from his right */
    { { { 0, GDF_KEY_FROM }, { 3, GDF_POSE_H1_WIND }, { 6, GDF_POSE_H1_HIT }, { 9, GDF_POSE_H1_FOLLOW },
        { 18, GDF_POSE_REST } },
      18, 3, 10, 5, GDF_SWORD_ATK_COMBO2, -1, NA_SE_IT_SWORD_SWING_HARD },
    /* COMBO2: backhand */
    { { { 0, GDF_KEY_FROM }, { 3, GDF_POSE_H2_HIT }, { 6, GDF_POSE_H2_FOLLOW }, { 15, GDF_POSE_REST } },
      15, 1, 7, 3, GDF_SWORD_ATK_COMBO3, -1, NA_SE_IT_SWORD_SWING_HARD },
    /* COMBO3: overhead slam */
    { { { 0, GDF_KEY_FROM }, { 6, GDF_POSE_H3_RAISE }, { 10, GDF_POSE_H3_SLAM }, { 20, GDF_POSE_H3_SLAM },
        { 30, GDF_POSE_REST } },
      30, 7, 11, -1, 0, 10, NA_SE_IT_SWORD_SWING_HARD },
    /* THRUST: out of a dash */
    { { { 0, GDF_KEY_FROM }, { 3, GDF_POSE_THRUST }, { 11, GDF_POSE_THRUST }, { 18, GDF_POSE_REST } },
      18, 2, 10, -1, 0, -1, NA_SE_IT_SWORD_SWING_HARD },
    /* SPIN: a full turn with the blade out */
    { { { 0, GDF_KEY_FROM }, { 3, GDF_POSE_H1_HIT }, { 16, GDF_POSE_H1_HIT }, { 24, GDF_POSE_REST } },
      24, 3, 16, -1, 0, -1, NA_SE_IT_ROLLING_CUT },
    /* SLAM: overhead into the ground, with a shockwave */
    { { { 0, GDF_KEY_FROM }, { 8, GDF_POSE_H3_RAISE }, { 12, GDF_POSE_H3_SLAM }, { 24, GDF_POSE_H3_SLAM },
        { 32, GDF_POSE_REST } },
      32, 9, 13, -1, 0, 12, NA_SE_IT_SWORD_SWING_HARD },
    /* WAVE: a slash that sends dark energy flying */
    { { { 0, GDF_KEY_FROM }, { 5, GDF_POSE_H1_WIND }, { 9, GDF_POSE_H1_HIT }, { 12, GDF_POSE_H1_FOLLOW },
        { 20, GDF_POSE_REST } },
      20, 7, 11, -1, 0, 9, NA_SE_IT_SWORD_SWING_HARD },
};

// His whole body moves with the sword: the upper body twists into the swing (positive: turned to his left) and
// leans (positive: forward), the hips turn a little the other way, and his legs step into a lunge (a frame of the
// walk with the right foot forward) on the strikes.
typedef struct {
    s8 frame;
    s16 twist;
    s16 lean;
    u8 lunge; // percent
} GdfSwordBodyKey;

#define GDF_LUNGE_FRAME 0.0f // walk frame with the right foot forward and both feet down

static const GdfSwordBodyKey sGdfSwordBody[][6] = {
    /* NONE */ { { -1 } },
    /* DRAW: turn to the sheath at his left hip, then unwind into the cut */
    { { 0, 0, 0, 0 }, { 5, 0x1800, 0x0A00, 20 }, { 7, 0x1C00, 0x0A00, 30 }, { 10, -0x2000, 0x0600, 90 },
      { 17, -0x1400, 0x0300, 70 }, { 25, 0, 0, 0 } },
    /* SHEATHE: lift, then turn to the hip and look down at it */
    { { 0, 0, 0, 0 }, { 6, -0x0C00, -0x0300, 0 }, { 12, 0x1800, 0x0C00, 20 }, { 15, 0x1800, 0x0C00, 20 },
      { 20, 0, 0, 0 }, { -1 } },
    /* COMBO1: wind up turned right, unwind to the left */
    { { 0, 0, 0, 0 }, { 3, -0x2400, 0x0200, 30 }, { 6, 0x1400, 0x0A00, 100 }, { 9, 0x2800, 0x0800, 100 },
      { 18, 0, 0, 0 }, { -1 } },
    /* COMBO2: backhand, unwinding to the right */
    { { 0, 0x2800, 0x0800, 100 }, { 3, -0x0800, 0x0A00, 100 }, { 6, -0x2800, 0x0800, 100 }, { 15, 0, 0, 0 },
      { -1 } },
    /* COMBO3: rear back, then bend into the slam */
    { { 0, -0x2800, 0x0800, 100 }, { 6, -0x0800, -0x0C00, 40 }, { 10, 0, 0x1C00, 100 }, { 20, 0, 0x1800, 100 },
      { 30, 0, 0, 0 }, { -1 } },
    /* THRUST: right shoulder forward, leaning into it */
    { { 0, 0, 0, 0 }, { 3, -0x1400, 0x1000, 100 }, { 11, -0x1000, 0x0C00, 100 }, { 18, 0, 0, 0 }, { -1 } },
    /* SPIN: crouched forward while he turns */
    { { 0, 0, 0, 0 }, { 3, 0, 0x0800, 60 }, { 16, 0, 0x0800, 60 }, { 24, 0, 0, 0 }, { -1 } },
    /* SLAM */
    { { 0, 0, 0, 0 }, { 8, -0x0800, -0x1000, 40 }, { 12, 0, 0x2000, 100 }, { 24, 0, 0x1C00, 100 }, { 32, 0, 0, 0 },
      { -1 } },
    /* WAVE: like the first slash, flung wider */
    { { 0, 0, 0, 0 }, { 5, -0x2800, 0x0200, 40 }, { 9, 0x1800, 0x0C00, 100 }, { 12, 0x2C00, 0x0800, 100 },
      { 20, 0, 0, 0 }, { -1 } },
};

// Persists across areas (the player Ganondorf is spawned again in every scene)
static u8 sPlayerSwordState = GDF_SWORD_OFF;

// His sword or trident is out: the C-buttons are its moves, not Link's items
s32 BossGanon_Player_InStance(void) {
    return sPlayerSwordState != GDF_SWORD_OFF;
}


static ColliderQuadInit sPlayerSwordQuadInit = {
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
        { DMG_GANONDORF_PLAYER, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// A dark trail behind the blade
static EffectBlureInit2 sPlayerSwordBlureInit = {
    0, 8, 0, { 200, 120, 255, 255 }, { 90, 0, 140, 160 }, { 60, 0, 100, 0 }, { 30, 0, 60, 0 }, 6,
    0, 2, 0, { 200, 120, 255, 255 }, { 90, 0, 140, 160 }, TRAIL_TYPE_SWORDS,
};

// ---- Quaternion blending of joint rotations ----------------------------------------------------

typedef struct {
    f32 w, x, y, z;
} GdfQuat;

static GdfQuat BossGanon_Player_RotToQuat(Vec3s* rot) {
    f32 m[3][3];
    GdfQuat q;
    f32 trace;
    f32 s;

    BossGanon_Player_RotToMtx(rot, m);
    trace = m[0][0] + m[1][1] + m[2][2];
    if (trace > 0.0f) {
        s = sqrtf(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m[2][1] - m[1][2]) / s;
        q.y = (m[0][2] - m[2][0]) / s;
        q.z = (m[1][0] - m[0][1]) / s;
    } else if ((m[0][0] > m[1][1]) && (m[0][0] > m[2][2])) {
        s = sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
        q.w = (m[2][1] - m[1][2]) / s;
        q.x = 0.25f * s;
        q.y = (m[0][1] + m[1][0]) / s;
        q.z = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        s = sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
        q.w = (m[0][2] - m[2][0]) / s;
        q.x = (m[0][1] + m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (m[1][2] + m[2][1]) / s;
    } else {
        s = sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
        q.w = (m[1][0] - m[0][1]) / s;
        q.x = (m[0][2] + m[2][0]) / s;
        q.y = (m[1][2] + m[2][1]) / s;
        q.z = 0.25f * s;
    }
    return q;
}

static void BossGanon_Player_QuatToRot(GdfQuat q, Vec3s* rot) {
    f32 m[3][3];

    m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    m[0][1] = 2.0f * (q.x * q.y - q.z * q.w);
    m[0][2] = 2.0f * (q.x * q.z + q.y * q.w);
    m[1][0] = 2.0f * (q.x * q.y + q.z * q.w);
    m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    m[1][2] = 2.0f * (q.y * q.z - q.x * q.w);
    m[2][0] = 2.0f * (q.x * q.z - q.y * q.w);
    m[2][1] = 2.0f * (q.y * q.z + q.x * q.w);
    m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    BossGanon_Player_MtxToRot(m, rot);
}

// Rotation part way from `a` to `b`
static void BossGanon_Player_SlerpRot(Vec3s* a, Vec3s* b, f32 t, Vec3s* out) {
    GdfQuat qa;
    GdfQuat qb;
    GdfQuat q;
    f32 dot;
    f32 len;

    if (t <= 0.0f) {
        *out = *a;
        return;
    }
    if (t >= 1.0f) {
        *out = *b;
        return;
    }

    qa = BossGanon_Player_RotToQuat(a);
    qb = BossGanon_Player_RotToQuat(b);
    dot = qa.w * qb.w + qa.x * qb.x + qa.y * qb.y + qa.z * qb.z;
    if (dot < 0.0f) {
        qb.w = -qb.w;
        qb.x = -qb.x;
        qb.y = -qb.y;
        qb.z = -qb.z;
        dot = -dot;
    }

    if (dot > 0.9995f) {
        q.w = qa.w + (qb.w - qa.w) * t;
        q.x = qa.x + (qb.x - qa.x) * t;
        q.y = qa.y + (qb.y - qa.y) * t;
        q.z = qa.z + (qb.z - qa.z) * t;
    } else {
        f32 theta = acosf(dot);
        f32 sinTheta = sinf(theta);
        f32 wa = sinf((1.0f - t) * theta) / sinTheta;
        f32 wb = sinf(t * theta) / sinTheta;

        q.w = qa.w * wa + qb.w * wb;
        q.x = qa.x * wa + qb.x * wb;
        q.y = qa.y * wa + qb.y * wb;
        q.z = qa.z * wa + qb.z * wb;
    }
    len = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    q.w /= len;
    q.x /= len;
    q.y /= len;
    q.z /= len;
    BossGanon_Player_QuatToRot(q, out);
}

// Moves an arm's three joints (upper arm, forearm, hand) part way towards `pose`
static void BossGanon_Player_BlendArm(BossGanon* this, s32 firstLimb, const Vec3s* pose, f32 weight) {
    Vec3s* joints = this->skelAnime.jointTable;
    s32 i;

    if (weight <= 0.0f) {
        return;
    }
    for (i = 0; i < 3; i++) {
        Vec3s target = pose[i];
        BossGanon_Player_SlerpRot(&joints[1 + firstLimb + i], &target, weight, &joints[1 + firstLimb + i]);
    }
}

static f32 BossGanon_Player_SmoothStep(f32 t) {
    t = CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// The right arm at `frame` of the current attack: blends between its keyframes
static void BossGanon_Player_ApplyAttackArm(BossGanon* this, f32 frame) {
    const GdfSwordAttack* atk = &sGdfSwordAttacks[this->playerSwordAttack];
    Vec3s from[3];
    Vec3s to[3];
    Vec3s* joints = this->skelAnime.jointTable;
    const GdfSwordKey* k0 = &atk->keys[0];
    const GdfSwordKey* k1 = NULL;
    f32 t;
    s32 i;

    for (i = 1; i < ARRAY_COUNT(atk->keys); i++) {
        if ((atk->keys[i].frame == 0) && (atk->keys[i].pose == 0) && (i > 1)) {
            break; // unused
        }
        if (atk->keys[i].frame > frame) {
            k1 = &atk->keys[i];
            break;
        }
        k0 = &atk->keys[i];
    }

    // The pose of a key; "from" is the arm when the attack started, "end" lets the animation take over
    for (i = 0; i < 3; i++) {
        from[i] = (k0->pose >= 0) ? sGdfSwordPoses[k0->pose][i]
                                  : ((k0->pose == GDF_KEY_FROM) ? this->playerArmFrom[i] : joints[1 + GDF_RIGHT_ARM_LIMB + i]);
    }
    if (k1 == NULL) {
        for (i = 0; i < 3; i++) {
            joints[1 + GDF_RIGHT_ARM_LIMB + i] = from[i];
        }
        return;
    }
    for (i = 0; i < 3; i++) {
        to[i] = (k1->pose >= 0) ? sGdfSwordPoses[k1->pose][i] : joints[1 + GDF_RIGHT_ARM_LIMB + i];
    }
    t = BossGanon_Player_SmoothStep((frame - k0->frame) / (f32)(k1->frame - k0->frame));
    for (i = 0; i < 3; i++) {
        BossGanon_Player_SlerpRot(&from[i], &to[i], t, &joints[1 + GDF_RIGHT_ARM_LIMB + i]);
    }
}

// Left arm holds the sheath still. The right arm swings with the walk; with the blade out it rests the blade lowered
// when standing, and swings between two poses that keep the blade pointing forward and down when walking.
static void BossGanon_Player_ApplyStanceArms(BossGanon* this) {
    Vec3s swing[3];
    Vec3s pose[3];
    f32 phase;
    s32 i;

    // Phantom Ganon has no sheath: his left arm stays free. Nor does a trident: drawn (in his right hand), his left arm
    // swings free as he walks; put away, it's carried in his left fist (that arm held still)
    Math_ApproachF(&this->playerSheathArmWeight,
                   ((sPlayerSwordState != GDF_SWORD_OFF) && !BossGanon_Player_IsPhantom() &&
                    !(BossGanon_Player_HasTrident() && (sPlayerSwordState == GDF_SWORD_DRAWN)))
                       ? 1.0f
                       : 0.0f,
                   1.0f, 0.2f);
    BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfSwordPoses[GDF_POSE_SHEATH_HOLD],
                              this->playerSheathArmWeight);

    Math_ApproachF(&this->playerSwordArmWeight, (sPlayerSwordState == GDF_SWORD_DRAWN) ? 1.0f : 0.0f, 1.0f, 0.2f);
    // In time with the walk: the fist is furthest back at frame 0 and furthest forward at frame 15
    phase = 0.5f - (0.5f * Math_CosS((s16)(this->playerWalkFrame * (0x10000 / GDF_WALK_CYCLE_FRAMES))));
    for (i = 0; i < 3; i++) {
        Vec3s back = sGdfSwordPoses[GDF_POSE_SWING_BACK][i];
        Vec3s fwd = sGdfSwordPoses[GDF_POSE_SWING_FWD][i];
        Vec3s rest = sGdfSwordPoses[GDF_POSE_REST][i];

        BossGanon_Player_SlerpRot(&back, &fwd, phase, &swing[i]);
        // A trident is held still at his side while he walks (a swinging arm put the shaft through his legs)
        BossGanon_Player_SlerpRot(&rest, &swing[i], BossGanon_Player_HasTrident() ? 0.0f : this->playerWalkWeight,
                                  &pose[i]);
    }
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, pose, this->playerSwordArmWeight);

    // Guard: the blade up in front of him
    Math_ApproachF(&this->playerGuardWeight, this->playerSwordGuard ? 1.0f : 0.0f, 1.0f, 0.25f);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfSwordPoses[GDF_POSE_GUARD], this->playerGuardWeight);
}

// ---- Whole-body motion -------------------------------------------------------------------------

static void BossGanon_Player_MtxMul(f32 a[3][3], f32 b[3][3], f32 out[3][3]) {
    f32 r[3][3];
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r[i][j] = (a[i][0] * b[0][j]) + (a[i][1] * b[1][j]) + (a[i][2] * b[2][j]);
        }
    }
    memcpy(out, r, sizeof(r));
}

// Turns a joint (whose parent is the root) by `turn` in his own space (actor space): R' = Rroot^T * turn * Rroot * R
static void BossGanon_Player_TurnJoint(BossGanon* this, s32 limb, f32 turn[3][3]) {
    Vec3s* joints = this->skelAnime.jointTable;
    f32 root[3][3];
    f32 rootT[3][3];
    f32 joint[3][3];
    f32 m[3][3];
    s32 i;
    s32 j;

    BossGanon_Player_RotToMtx(&joints[1], root);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            rootT[i][j] = root[j][i];
        }
    }
    BossGanon_Player_RotToMtx(&joints[1 + limb], joint);
    BossGanon_Player_MtxMul(root, joint, m);
    BossGanon_Player_MtxMul(turn, m, m);
    BossGanon_Player_MtxMul(rootT, m, m);
    BossGanon_Player_MtxToRot(m, &joints[1 + limb]);
}

// On the ground, his legs stand (from his standing idle) while his upper body plays the action. The block, light
// ball and magic animations are made for floating: their legs fold up or hang down, which sank him into the floor.
// The pelvis is set so his legs keep their standing direction whatever the animation does with his root.
static void BossGanon_Player_StandLegs(BossGanon* this) {
    static Vec3s sStand[GDF_LIMB_COUNT_PLAYER + 1];
    static u8 sStandRead = false;
    Vec3s* joints = this->skelAnime.jointTable;
    f32 weight = this->playerStandLegsWeight;
    f32 root[3][3];
    f32 standRoot[3][3];
    f32 standPelvis[3][3];
    f32 m[3][3];
    Vec3s pelvis;
    s32 i;
    s32 j;

    if (weight <= 0.0f) {
        return;
    }
    if (!sStandRead) {
        SkelAnime_GetFrameData((AnimationHeader*)gGanondorfStandIdleAnim, 0, GDF_LIMB_COUNT_PLAYER + 1, sStand);
        sStandRead = true;
    }

    // pelvis = root^T * standRoot * standPelvis
    BossGanon_Player_RotToMtx(&joints[1], root);
    BossGanon_Player_RotToMtx(&sStand[1], standRoot);
    BossGanon_Player_RotToMtx(&sStand[1 + 18], standPelvis);
    BossGanon_Player_MtxMul(standRoot, standPelvis, m);
    for (i = 0; i < 3; i++) {
        for (j = i + 1; j < 3; j++) {
            f32 t = root[i][j];

            root[i][j] = root[j][i];
            root[j][i] = t;
        }
    }
    BossGanon_Player_MtxMul(root, m, m);
    BossGanon_Player_MtxToRot(m, &pelvis);

    BossGanon_Player_SlerpRot(&joints[1 + 18], &pelvis, weight, &joints[1 + 18]);
    for (i = 19; i <= 24; i++) {
        BossGanon_Player_SlerpRot(&joints[1 + i], &sStand[1 + i], weight, &joints[1 + i]);
    }
    // His standing height, so crouching poses bend at the hips instead of pushing his legs into the floor. Fully,
    // not blended: while the animations morph in and out of the floating ones (whose root sits 72 units lower), a
    // blended height dipped him into the ground at the start and end of every throw.
    joints[0].y = sStand[0].y;
}

static void BossGanon_Player_TwistLean(BossGanon* this, f32 twist, f32 lean) {
    Vec3s yaw = { 0, 0, 0 };
    Vec3s pitch = { 0, 0, 0 };
    f32 my[3][3];
    f32 mx[3][3];
    f32 turn[3][3];

    if ((twist == 0.0f) && (lean == 0.0f)) {
        return;
    }
    // Upper body
    yaw.y = (s16)twist;
    pitch.x = (s16)lean;
    BossGanon_Player_RotToMtx(&yaw, my);
    BossGanon_Player_RotToMtx(&pitch, mx);
    BossGanon_Player_MtxMul(my, mx, turn);
    BossGanon_Player_TurnJoint(this, 1, turn);
    // Hips turn a little the other way
    yaw.y = (s16)(twist * -0.35f);
    BossGanon_Player_RotToMtx(&yaw, my);
    BossGanon_Player_TurnJoint(this, 18, my);
}

// Body pose of the current attack at `frame`: twist, lean and the lunge
static void BossGanon_Player_ApplyAttackBody(BossGanon* this, f32 frame) {
    const GdfSwordBodyKey* keys = sGdfSwordBody[this->playerSwordAttack];
    const GdfSwordBodyKey* k0 = &keys[0];
    const GdfSwordBodyKey* k1 = NULL;
    f32 t;
    f32 twist;
    f32 lean;
    f32 lunge;
    f32 savedFrame;
    f32 savedWeight;
    f32 savedStride;
    s32 i;

    if (k0->frame < 0) {
        return;
    }
    for (i = 1; i < 6; i++) {
        if (keys[i].frame < 0) {
            break;
        }
        if (keys[i].frame > frame) {
            k1 = &keys[i];
            break;
        }
        k0 = &keys[i];
    }
    if (k1 == NULL) {
        twist = k0->twist;
        lean = k0->lean;
        lunge = k0->lunge / 100.0f;
    } else {
        t = BossGanon_Player_SmoothStep((frame - k0->frame) / (f32)(k1->frame - k0->frame));
        twist = k0->twist + (k1->twist - k0->twist) * t;
        lean = k0->lean + (k1->lean - k0->lean) * t;
        lunge = (k0->lunge + (k1->lunge - k0->lunge) * t) / 100.0f;
    }

    // Lunge: his legs in the walk's stride with the right foot forward (only on the ground)
    if (this->playerGrounded && (lunge > this->playerWalkWeight)) {
        savedFrame = this->playerWalkFrame;
        savedWeight = this->playerWalkWeight;
        savedStride = this->playerWalkStride;
        this->playerWalkFrame = GDF_LUNGE_FRAME;
        this->playerWalkWeight = lunge;
        this->playerWalkStride = 1.2f;
        BossGanon_Player_ApplyWalk(this);
        this->playerWalkFrame = savedFrame;
        this->playerWalkWeight = savedWeight;
        this->playerWalkStride = savedStride;
    }

    BossGanon_Player_TwistLean(this, twist, lean);
}


// ---- Jumping ------------------------------------------------------------------------------------
//
// Like Link's jumps (Link still does the jumping): a leap with both knees pulled up, but with Ganondorf's flair: arms
// spread wide (with the blade out, raised overhead), and after a big drop a heavy landing on one knee from his boss
// fight that shakes the ground, then he rises.

static const Vec3s sGdfJumpArmPoses[2][3] = {
    { { -30128, -32436, 27559 }, { 310, 6473, -7851 }, { 2331, 1984, -1421 } }, // right, spread
    { { -2258, 4986, -2650 }, { 1116, -5525, -6703 }, { -724, -575, -1517 } },   // left, spread
};

#define GDF_JUMP_LEFT_KNEE_FRAME 5   // walk frames with that knee highest
#define GDF_JUMP_RIGHT_KNEE_FRAME 20
#define GDF_BIG_LANDING_HEIGHT 100.0f

static s32 BossGanon_Player_IsJumping(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           !(player->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_HANGING_OFF_LEDGE |
                                    PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE)) &&
           !(player->stateFlags2 & PLAYER_STATE2_CRAWLING);
}

static void BossGanon_Player_ApplyJump(BossGanon* this, PlayState* play) {
    f32 weight;

    Math_ApproachF(&this->playerAirWeight, (this->playerGrounded && BossGanon_Player_IsJumping(play)) ? 1.0f : 0.0f,
                   1.0f, 0.2f);
    weight = this->playerAirWeight;
    if (weight <= 0.0f) {
        sGdfJumpFrame = 0.0f;
        return;
    }

    // The running jump: take-off and the leap up (its first 13 frames, played at its own speed), then held tucked
    // while he flies; falling, his legs reach down for the ground
    if (BossGanon_Player_IsJumping(play)) {
        static u32 sLastJumpFrame = 0;
        Player* player = GET_PLAYER(play);

        if (play->gameplayFrames - sLastJumpFrame > 1) {
            sGdfJumpFrame = 0.0f; // a new jump: from the take-off
        }
        sLastJumpFrame = play->gameplayFrames;
        if (sGdfJumpFrame < 13.0f) {
            sGdfJumpFrame = MIN(sGdfJumpFrame + 1.5f, 13.0f);
        } else if (player->actor.velocity.y < -4.0f) {
            sGdfJumpFrame = MIN(sGdfJumpFrame + 0.5f, 17.0f);
        }
    }
    BossGanon_Player_ApplyClip(this, sGdfClipJump, ARRAY_COUNT(sGdfClipJump), sGdfJumpFrame, false, weight, 0.6f);
}

void BossGanon_Player_SetupLand(BossGanon* this, PlayState* play, f32 height) {
    Vec3f pos = this->actor.world.pos;

    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLandAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfLandAnim, -2.0f);
    this->actionFunc = BossGanon_Player_Land;
    this->unk_1C2 = 0;
    this->playerAirWeight = 0.0f;
    this->playerWalkWeight = 0.0f;

    pos.y = this->playerFloorY;
    Actor_RequestQuakeAndRumble(&this->actor, play, (height > 250.0f) ? 5 : 3, 12);
    Actor_SpawnFloorDustRing(play, &this->actor, &pos, 40.0f, 10, 6.0f, 400, 25, true);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_HIT_GND));
}

// Down on one knee, a moment, then he rises
void BossGanon_Player_Land(BossGanon* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if ((sPlayerSwordState != GDF_SWORD_OFF) && !BossGanon_Player_IsPhantom()) {
        BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfSwordPoses[GDF_POSE_SHEATH_HOLD], 1.0f);
    }
    sCape->minDist = 0.0f;

    switch (this->unk_1C2) {
        case 0:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->timers[0] = 6;
                this->unk_1C2 = 1;
            }
            break;
        case 1:
            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp3Anim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, -4.0f);
                this->unk_1C2 = 2;
            }
            break;
        case 2:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_Player_SetupFloat(this, play);
                sGdfFastGroundTimer = 16; // through the 10-frame blend into his idle
            }
            break;
    }
}

// ---- Stance and attacks ------------------------------------------------------------------------

s32 BossGanon_Player_IsSwordGuarding(BossGanon* this) {
    return GDF_IS_PLAYER(this) && !this->playerHidden && (this->actionFunc == BossGanon_Player_SwordIdle) &&
           this->playerSwordGuard && (this->playerGuardWeight > 0.5f);
}

static void BossGanon_Player_SetSwordBody(BossGanon* this, PlayState* play) {
    if (this->playerGrounded) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -8.0f);
    } else {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, -8.0f);
    }
    this->playerStandAnim = this->playerGrounded;
}

void BossGanon_Player_SetupSwordIdle(BossGanon* this, PlayState* play) {
    BossGanon_Player_SetSwordBody(this, play);
    this->actionFunc = BossGanon_Player_SwordIdle;
    this->playerSwordAttack = GDF_SWORD_ATK_NONE;
    this->playerSwordQueued = false;
    this->playerSpinYaw = 0;
    this->playerBtnHeld = 0;
    this->useOpenHand = false;
}

static void BossGanon_Player_SetupSwordAttack(BossGanon* this, PlayState* play, u8 attack) {
    s32 i;

    // Invasion mode: his attacks cost magic (a swing 3, a big one 8)
    if ((attack >= GDF_SWORD_ATK_COMBO1) &&
        !GdfInvasion_PayAttack(((attack == GDF_SWORD_ATK_SPIN) || (attack == GDF_SWORD_ATK_SLAM) ||
                                (attack == GDF_SWORD_ATK_WAVE))
                                   ? 8
                                   : 3)) {
        return;
    }

    for (i = 0; i < 3; i++) {
        this->playerArmFrom[i] = this->skelAnime.jointTable[1 + GDF_RIGHT_ARM_LIMB + i];
    }
    this->playerSwordAttack = attack;
    this->playerSwordTimer = 0;
    this->playerSwordQueued = false;
    this->playerSwordGuard = false;
    this->playerGuardWeight = 0.0f;
    this->playerSwordArmWeight = 0.0f;
    this->actionFunc = BossGanon_Player_SwordAttack;

    // Attacks go the way he faces; the dark wave and the slam turn to what he's aiming at
    this->actor.yawTowardsPlayer = this->actor.shape.rot.y;
    if ((attack == GDF_SWORD_ATK_WAVE) || (attack == GDF_SWORD_ATK_SLAM)) {
        BossGanon_Player_UpdateAim(this, play);
    }
    if (attack == GDF_SWORD_ATK_SPIN) {
        this->playerDashYaw = this->actor.shape.rot.y;
        this->playerSpinYaw = 0;
    }
    if (attack == GDF_SWORD_ATK_DRAW) {
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PICKOUT_GANON);
    }
}

static void BossGanon_Player_SetupSwordDash(BossGanon* this, PlayState* play, s16 yaw) {
    Player* player = GET_PLAYER(play);

    this->playerDashYaw = yaw;
    this->playerDashSpeed = 26.0f;
    this->playerDashTimer = 0;
    this->playerSwordGuard = false;
    this->actor.yawTowardsPlayer = yaw;
    this->actionFunc = BossGanon_Player_SwordDash;
    // A moment where nothing can hurt him
    if (player->invincibilityTimer == 0) {
        player->invincibilityTimer = -8;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_FLOAT);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN));
}

// Stick direction in the world (camera-relative), or -1 when the stick is neutral
static s32 BossGanon_Player_StickYaw(PlayState* play) {
    Input* input = &play->state.input[0];
    f32 magnitude;
    s16 angle;

    func_80077D10(&magnitude, &angle, input);
    if (magnitude < 30.0f) {
        return -1;
    }
    return (u16)(Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + angle);
}

// D-Pad Left with the sword out or away, from his normal moveset
static s32 BossGanon_Player_TryToggleSword(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];

    if (!CVarGetInteger(CVAR_PLAY_AS_GDF("SwordStance"), 1) || !CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        return false;
    }
    // D-Pad Left cycles (as in Invasion games, and outside them too): once the insta-kill dark aura (B, held, unleashes it), twice the trident
    // stance, a third time back to normal (from the stance, in BossGanon_Player_SwordIdle)
    if (!sPlayerDarkAuraMode && (sPlayerSwordState == GDF_SWORD_OFF)) {
        sPlayerDarkAuraMode = true;
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
        BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
        BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
        return true;
    }
    sPlayerDarkAuraMode = false;
    if ((sPlayerSwordState == GDF_SWORD_OFF) && BossGanon_Player_HasTrident() && this->playerGrounded) {
        BossGanon_Player_SetupSummonTrident(this, play); // raised overhead, it bursts into his hand
        return true;
    }
    if (sPlayerSwordState == GDF_SWORD_OFF) {
        sPlayerSwordState = GDF_SWORD_SHEATHED;
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
        BossGanon_Player_SetupSwordIdle(this, play);
        return true;
    }
    return false;
}

void BossGanon_Player_SwordIdle(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];
    s32 stickYaw;

    if (this->playerStandAnim != this->playerGrounded) {
        BossGanon_Player_SetSwordBody(this, play);
    }

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;

    SkelAnime_Update(&this->skelAnime);
    if (this->playerGrounded) {
        BossGanon_Player_UpdateWalk(this, play);
    } else {
        this->playerWalkWeight = 0.0f;
        this->legSwayEnabled = true;
    }

    this->playerSwordGuard = (sPlayerSwordState == GDF_SWORD_DRAWN) && CHECK_BTN_ALL(input->cur.button, BTN_R) &&
                             (BossGanon_Player_StickYaw(play) < 0) && BossGanon_Player_CanStartAttack(this, play);
    BossGanon_Player_ApplyStanceArms(this);
    BossGanon_Player_ApplyTridentRun(this);
    BossGanon_Player_ApplyJump(this, play);

    if (!BossGanon_Player_CanStartAttack(this, play)) {
        return;
    }
    // Invasion mode: C-buttons with a Dark Art on them cast it (but with the trident in hand, C-Right spins it:
    // the fireball ring needs it)
    if (!(BossGanon_Player_HasTrident() && CHECK_BTN_ALL(play->state.input[0].press.button, BTN_CRIGHT)) &&
        GdfInvasion_HandleCButtons(play)) {
        return;
    }

    // Phantom Ganon: B slashes (B, B, then a stab), stick + R and B a lunging stab, R + B or C-Left a ball of light.
    // No Big Magic and no sword skills. D-Pad Left still toggles the dark aura (so no boss can leave him stuck).
    if (BossGanon_Player_IsPhantom()) {
        u8 useCButtons = !PlayAsGanondorf_KeepItems();

        if (GanondorfSongs_IsOcarinaOut()) {
            return;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
            sPlayerDarkAuraMode = !sPlayerDarkAuraMode;
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_SPARK);
            if (sPlayerDarkAuraMode) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_LAUGH);
                BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
            }
            return;
        }
        stickYaw = BossGanon_Player_StickYaw(play);
        if (CHECK_BTN_ALL(input->press.button, BTN_R) && (stickYaw >= 0)) {
            BossGanon_Player_SetupSwordDash(this, play, (s16)stickYaw);
            return;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
            if (sPlayerDarkAuraMode) {
                BossGanon_Player_SetupDarkAura(this, play);
            } else if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
                BossGanon_Player_StartAttack(this, BTN_B);
                BossGanon_Player_SetupChargeLightBall(this, play);
            } else {
                BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_COMBO1);
            }
        } else if (useCButtons && CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
            BossGanon_Player_StartAttack(this, BTN_CLEFT);
            BossGanon_Player_SetupChargeLightBall(this, play);
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
            BossGanon_Player_SetupLaugh(this, play);
        }
        return;
    }

    // D-Pad Left, the third press of the cycle: back to normal. On the ground the trident comes apart in his raised
    // hand; in the air it's simply gone
    if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        sPlayerDarkAuraMode = false;
        if ((sPlayerSwordState == GDF_SWORD_DRAWN) && BossGanon_Player_HasTrident() && this->playerGrounded) {
            BossGanon_Player_SetupUnsummonTrident(this, play);
            return;
        }
        sPlayerSwordState = GDF_SWORD_OFF;
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
        BossGanon_Player_SetupFloat(this, play);
        return;
    }

    // Stick + R: dash
    stickYaw = BossGanon_Player_StickYaw(play);
    if (CHECK_BTN_ALL(input->press.button, BTN_R) && (stickYaw >= 0)) {
        BossGanon_Player_SetupSwordDash(this, play, (s16)stickYaw);
        return;
    }

    if (sPlayerSwordState == GDF_SWORD_SHEATHED) {
        if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
            BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_DRAW);
        }
        return;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_COMBO1);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
        BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_SPIN);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
        BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_SLAM);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CRIGHT)) {
        if (BossGanon_Player_HasTrident()) {
            BossGanon_Player_SetupTridentSpin(this, play); // spun overhead, thrown on release
        } else {
            BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_WAVE);
        }
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        BossGanon_Player_SetupLaugh(this, play);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        BossGanon_Player_TryGrab(this, play); // by the throat
    }
}

// The ground slam: a quake, dust and a shockwave that hits everything around the impact
static void BossGanon_Player_SwordImpact(BossGanon* this, PlayState* play, f32 radius) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = this->playerBladeTip;

    pos.y = this->playerFloorY + 2.0f;
    Actor_RequestQuakeAndRumble(&this->actor, play, 6, 20);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_HIT_GND_IMP));
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_HAMMER_HIT);
    EffectSsBlast_SpawnWhiteShockwave(play, &pos, &zero, &zero);
    Actor_SpawnFloorDustRing(play, &this->actor, &pos, radius * 0.4f, 16, 8.0f, 700, 30, true);

    this->collider.dim.radius = (s16)radius;
    this->collider.dim.height = 80;
    this->collider.dim.yShift = 0;
    this->collider.dim.pos.x = (s16)pos.x;
    this->collider.dim.pos.y = (s16)(pos.y - 20.0f);
    this->collider.dim.pos.z = (s16)pos.z;
    BossGanon_Player_SetAT(this, play);
}

void BossGanon_Player_SwordAttack(BossGanon* this, PlayState* play) {
    const GdfSwordAttack* atk = &sGdfSwordAttacks[this->playerSwordAttack];
    Input* input = &play->state.input[0];
    s16 rotX;
    s16 rotY;
    s32 stickYaw;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->playerWalkWeight, 1.0f, 0.2f);
    if (this->playerGrounded) {
        BossGanon_Player_ApplyWalk(this);
    }

    // Left arm keeps the sheath, right arm follows the attack, and his whole body moves with it
    Math_ApproachF(&this->playerSheathArmWeight, BossGanon_Player_IsPhantom() ? 0.0f : 1.0f, 1.0f, 0.2f);
    BossGanon_Player_ApplyAttackBody(this, this->playerSwordTimer);
    BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfSwordPoses[GDF_POSE_SHEATH_HOLD],
                              this->playerSheathArmWeight);
    BossGanon_Player_ApplyAttackArm(this, this->playerSwordTimer);
    BossGanon_Player_ApplyTridentAttack(this);

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 1.0f;
    sCape->sideSwayMagnitude = -2.0f;
    sCape->minDist = 10.0f;

    // The blade leaves the sheath / goes back in
    if ((this->playerSwordAttack == GDF_SWORD_ATK_DRAW) && (this->playerSwordTimer == 6)) {
        sPlayerSwordState = GDF_SWORD_DRAWN;
    }
    if ((this->playerSwordAttack == GDF_SWORD_ATK_SHEATHE) && (this->playerSwordTimer == 13)) {
        sPlayerSwordState = GDF_SWORD_SHEATHED;
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PUTAWAY);
    }
    if ((this->playerSwordAttack == GDF_SWORD_ATK_DRAW) && (this->playerSwordTimer == 18)) {
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
    }

    if (this->playerSwordTimer == atk->activeStart) {
        Audio_PlayActorSound2(&this->actor, atk->swingSfx);
        if (BossGanon_Player_IsPhantom() && (this->playerSwordAttack == GDF_SWORD_ATK_THRUST)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_ATTACK);
        } else if (this->playerSwordAttack != GDF_SWORD_ATK_DRAW) {
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN));
        }
    }

    // The spin turns his whole body once around
    // (with the trident, the clip turns his body itself)
    if ((this->playerSwordAttack == GDF_SWORD_ATK_SPIN) && !BossGanon_Player_HasTrident()) {
        if ((this->playerSwordTimer >= 3) && (this->playerSwordTimer < 16)) {
            this->playerSpinYaw += 0x10000 / 13;
        } else if (this->playerSwordTimer >= 16) {
            this->playerSpinYaw = 0;
        }
    }

    if (this->playerSwordTimer == atk->impactFrame) {
        if (this->playerSwordAttack == GDF_SWORD_ATK_WAVE) {
            // Dark energy flies off the blade towards whatever he's facing
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DARKWAVE));
            BossGanon_Player_AimRot(&this->playerBladeTip, &this->playerAimPos, &rotX, &rotY);
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->playerBladeTip.x,
                               this->playerBladeTip.y, this->playerBladeTip.z, rotX, rotY, 0,
                               GDF_PARAMS_PLAYER_LIGHT_BALL);
        } else {
            BossGanon_Player_SwordImpact(this, play, (this->playerSwordAttack == GDF_SWORD_ATK_SLAM) ? 260.0f : 150.0f);
        }
    }
    // The slam's shockwave keeps hitting for a moment
    if ((this->playerSwordAttack == GDF_SWORD_ATK_SLAM) && (this->playerSwordTimer > atk->impactFrame) &&
        (this->playerSwordTimer < atk->impactFrame + 4)) {
        BossGanon_Player_SetAT(this, play);
    }

    // Next hit of the combo
    if ((atk->comboFrom >= 0) && (this->playerSwordTimer >= atk->comboFrom) &&
        CHECK_BTN_ALL(input->press.button, BTN_B)) {
        this->playerSwordQueued = true;
    }
    if (this->playerSwordQueued && (this->playerSwordTimer >= atk->activeEnd) &&
        BossGanon_Player_CanStartAttack(this, play)) {
        // Phantom Ganon finishes the combo with a stab of the spear instead of the overhead slam
        BossGanon_Player_SetupSwordAttack(this, play,
                                          (BossGanon_Player_UsesSpear() && (atk->next == GDF_SWORD_ATK_COMBO3))
                                              ? GDF_SWORD_ATK_THRUST
                                              : atk->next);
        return;
    }

    // Stick + R cancels the recovery into a dash
    stickYaw = BossGanon_Player_StickYaw(play);
    if ((this->playerSwordTimer > atk->activeEnd) && (sPlayerSwordState == GDF_SWORD_DRAWN) &&
        CHECK_BTN_ALL(input->press.button, BTN_R) && (stickYaw >= 0) && BossGanon_Player_CanStartAttack(this, play)) {
        BossGanon_Player_SetupSwordDash(this, play, (s16)stickYaw);
        return;
    }

    this->playerSwordTimer++;
    if (this->playerSwordTimer >= atk->length) {
        BossGanon_Player_SetupSwordIdle(this, play);
    }
}

#define GDF_DASH_FRAMES 9

// A quick dash; B during it turns into a lunging thrust
void BossGanon_Player_SwordDash(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->playerWalkWeight, 1.0f, 0.3f);
    BossGanon_Player_ApplyStanceArms(this);

    this->playerDashTimer++;
    if (this->playerDashTimer > GDF_DASH_FRAMES / 2) {
        Math_ApproachZeroF(&this->playerDashSpeed, 1.0f, 5.0f);
    }

    if ((sPlayerSwordState == GDF_SWORD_DRAWN) && CHECK_BTN_ALL(input->press.button, BTN_B) &&
        BossGanon_Player_CanStartAttack(this, play)) {
        this->playerDashSpeed = 30.0f;
        this->playerDashTimer = 0;
        BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_THRUST);
        return;
    }

    if (this->playerDashTimer >= GDF_DASH_FRAMES) {
        this->playerDashSpeed = 0.0f;
        BossGanon_Player_SetupSwordIdle(this, play);
    }
}

// Used by PlayAsGanondorf.cpp to move Link along with the dash and the thrust
s32 BossGanon_Player_GetDash(Actor* thisx, s16* yaw, f32* speed) {
    BossGanon* this = (BossGanon*)thisx;

    if ((thisx == NULL) || !GDF_IS_PLAYER(this)) {
        return false;
    }
    if (this->actionFunc == BossGanon_Player_SwordDash) {
        *yaw = this->playerDashYaw;
        *speed = this->playerDashSpeed;
        return this->playerDashSpeed > 0.0f;
    }
    if ((this->actionFunc == BossGanon_Player_SwordAttack) && (this->playerSwordAttack == GDF_SWORD_ATK_THRUST) &&
        (this->playerSwordTimer < 10)) {
        *yaw = this->actor.shape.rot.y;
        *speed = 30.0f - (this->playerSwordTimer * 3.0f);
        return true;
    }
    return false;
}

// ---- Drawing and hits --------------------------------------------------------------------------

// ---- Phantom Ganon skin ----
// Phantom Ganon's skeleton has the same build as Ganondorf's (bones along +x, arms 3-12, legs 17-24), so each of his
// limbs simply takes the world rotation of the matching Ganondorf limb. Only the hands are modelled differently
// (his point along +y, with the spear through the fist along +x), so they get a fixed turn that lines his spear up
// with where Ganondorf's sword blade would point. His legs are shorter: he's drawn bigger to stand as tall.
#define GDF_PG_SCALE (5132.0f / 4133.0f)
#define GDF_SPEAR_SCALE 0.75f
#define GDF_SPEAR_BASE_Z 1500.0f
#define GDF_SPEAR_TIP_Z 8520.0f

// Ganondorf limb for each Phantom Ganon limb, and Phantom Ganon's limb parents
static const s8 sGdfPgSource[25] = { 0, 1, 1, 3, 3, 4, 5, 7, 7, 8, 9, 9, 9, 1, 10, 18, 18, 19, 19, 20, 21, 22, 22, 23, 24 };
static const s8 sGdfPgParent[25] = { -1, 0, 1, 2, 3, 4, 5, 2, 7, 8, 9, 10, 11, 2, 2, 0, 15, 16, 17, 18, 19, 16, 21, 22, 23 };
// Row-major turns from his hand space into Ganondorf's (right hand: spear tip to the blade's -z; left: mirrored)
static const f32 sGdfPgHandR[3][3] = { { 0, 1, 0 }, { 0, 0, -1 }, { -1, 0, 0 } };
static const f32 sGdfPgHandL[3][3] = { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } };
// His feet point along their local +x (sole toward +y), Ganondorf's along +z (sole toward -y): turn them so his toes
// face where Ganondorf's do. Without it both feet pointed to his left.
static const f32 sGdfPgFoot[3][3] = { { 0, 0, 1 }, { 0, -1, 0 }, { 1, 0, 0 } };
// The spear in his hand, as his own animations always hold it
static const Vec3s sGdfPgSpearRot = { 16286, -16384, 16286 };

s32 BossGanon_Player_IsPhantom(void) {
    return CVarGetInteger(CVAR_PLAY_AS_GDF("PhantomGanon"), 0) != 0;
}

// A trident in his hands (the weapon menu): the spear's moveset (the combo ends in a stab) and no sheath
static s32 BossGanon_Player_HasTrident(void) {
    return !BossGanon_Player_IsPhantom() && (GanondorfTrident_GetSelected() != 0);
}

static s32 BossGanon_Player_UsesSpear(void) {
    return BossGanon_Player_IsPhantom() || BossGanon_Player_HasTrident();
}

typedef f32 GdfMat3[3][3];

static void BossGanon_Pg_FromMtxF(MtxF* mf, GdfMat3 out, f32* scale) {
    s32 r;
    s32 c;
    f32 len;

    // MtxF is column major: mf[c][r]
    for (c = 0; c < 3; c++) {
        len = sqrtf(SQ(mf->mf[c][0]) + SQ(mf->mf[c][1]) + SQ(mf->mf[c][2]));
        if (len < 1e-8f) {
            len = 1.0f;
        }
        for (r = 0; r < 3; r++) {
            out[r][c] = mf->mf[c][r] / len;
        }
        if ((c == 0) && (scale != NULL)) {
            *scale = len;
        }
    }
}

static void BossGanon_Pg_Mul(const GdfMat3 a, const GdfMat3 b, GdfMat3 out, s32 transposeA) {
    s32 r;
    s32 c;
    s32 k;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            f32 sum = 0.0f;

            for (k = 0; k < 3; k++) {
                sum += (transposeA ? a[k][r] : a[r][k]) * b[k][c];
            }
            out[r][c] = sum;
        }
    }
}

// R = Rz * Ry * Rx (the skeleton's rotation order)
static void BossGanon_Pg_FromRot(Vec3s* rot, GdfMat3 out) {
    f32 cx = Math_CosS(rot->x);
    f32 sx = Math_SinS(rot->x);
    f32 cy = Math_CosS(rot->y);
    f32 sy = Math_SinS(rot->y);
    f32 cz = Math_CosS(rot->z);
    f32 sz = Math_SinS(rot->z);

    out[0][0] = cz * cy;
    out[0][1] = cz * sy * sx - sz * cx;
    out[0][2] = cz * sy * cx + sz * sx;
    out[1][0] = sz * cy;
    out[1][1] = sz * sy * sx + cz * cx;
    out[1][2] = sz * sy * cx - cz * sx;
    out[2][0] = -sy;
    out[2][1] = cy * sx;
    out[2][2] = cy * cx;
}

static void BossGanon_Pg_ToRot(GdfMat3 m, Vec3s* rot) {
    f32 sy = CLAMP(-m[2][0], -1.0f, 1.0f);

    rot->x = (s16)(atan2f(m[2][1], m[2][2]) * (32768.0f / M_PI));
    rot->y = (s16)(asinf(sy) * (32768.0f / M_PI));
    rot->z = (s16)(atan2f(m[1][0], m[0][0]) * (32768.0f / M_PI));
}

static s32 BossGanon_Pg_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                         void* thisx) {
    // The spear is drawn in the post callback, a little smaller than his own
    if (limbIndex == 13) {
        *dList = NULL;
    }
    return false;
}

static void BossGanon_Pg_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sBase = { 0.0f, 0.0f, GDF_SPEAR_BASE_Z };
    static Vec3f sTip = { 0.0f, 0.0f, GDF_SPEAR_TIP_Z };
    BossGanon* this = (BossGanon*)thisx;

    if (limbIndex == 13) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Scale(GDF_SPEAR_SCALE, GDF_SPEAR_SCALE, GDF_SPEAR_SCALE, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gPhantomGanonSkelLimbsLimb_00C610DL_009298);
        Matrix_MultVec3f(&sBase, &this->playerSpearBase);
        Matrix_MultVec3f(&sTip, &this->playerSpearTip);
        this->playerSpearValid = true;
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static Gfx* BossGanon_Pg_NullDL(GraphicsContext* gfxCtx) {
    Gfx* dList = (Gfx*)Graph_Alloc(gfxCtx, sizeof(Gfx));

    gSPEndDisplayList(dList);
    return dList;
}

static void BossGanon_Player_DrawPhantom(BossGanon* this, PlayState* play, MtxF* base) {
    GdfMat3 baseRot;
    GdfMat3 limbRot;
    GdfMat3 gdfWorld[25];
    GdfMat3 world[25];
    GdfMat3 tmp;
    GdfMat3 local;
    f32 baseScale = 1.0f;
    Vec3f rootPos;
    Vec3f d;
    s32 i;

    if (!this->playerPgReady) {
        SkelAnime_Init(play, &this->playerPgSkel, (SkeletonHeader*)gPhantomGanonSkel, NULL, this->playerPgJoints,
                       this->playerPgMorph, 26);
        this->playerPgReady = true;
    }

    // Ganondorf's limbs, relative to his actor
    BossGanon_Pg_FromMtxF(base, baseRot, &baseScale);
    for (i = 0; i < 25; i++) {
        BossGanon_Pg_FromMtxF(&sGdfLimbMtx[i], limbRot, NULL);
        BossGanon_Pg_Mul(baseRot, limbRot, gdfWorld[i], true);
    }
    d.x = (sGdfLimbMtx[0].mf[3][0] - base->mf[3][0]) / baseScale;
    d.y = (sGdfLimbMtx[0].mf[3][1] - base->mf[3][1]) / baseScale;
    d.z = (sGdfLimbMtx[0].mf[3][2] - base->mf[3][2]) / baseScale;
    rootPos.x = baseRot[0][0] * d.x + baseRot[1][0] * d.y + baseRot[2][0] * d.z;
    rootPos.y = baseRot[0][1] * d.x + baseRot[1][1] * d.y + baseRot[2][1] * d.z;
    rootPos.z = baseRot[0][2] * d.x + baseRot[1][2] * d.y + baseRot[2][2] * d.z;

    // His limbs take the same world rotations, then back to rotations relative to each parent
    for (i = 0; i < 25; i++) {
        if (i == 6) {
            BossGanon_Pg_Mul(gdfWorld[sGdfPgSource[i]], sGdfPgHandL, world[i], false);
        } else if (i == 10) {
            BossGanon_Pg_Mul(gdfWorld[sGdfPgSource[i]], sGdfPgHandR, world[i], false);
        } else if ((i == 20) || (i == 24)) {
            BossGanon_Pg_Mul(gdfWorld[sGdfPgSource[i]], sGdfPgFoot, world[i], false);
        } else if (i == 11) {
            Vec3s spear = sGdfPgSpearRot;

            BossGanon_Pg_FromRot(&spear, tmp);
            BossGanon_Pg_Mul(world[10], tmp, world[i], false);
        } else if (i == 12) {
            memcpy(world[i], world[11], sizeof(GdfMat3));
        } else {
            memcpy(world[i], gdfWorld[sGdfPgSource[i]], sizeof(GdfMat3));
        }

        if (sGdfPgParent[i] < 0) {
            memcpy(local, world[i], sizeof(GdfMat3));
        } else {
            BossGanon_Pg_Mul(world[sGdfPgParent[i]], world[i], local, true);
        }
        BossGanon_Pg_ToRot(local, &this->playerPgJoints[1 + i]);
    }
    this->playerPgJoints[0].x = (s16)(rootPos.x / GDF_PG_SCALE);
    this->playerPgJoints[0].y = (s16)(rootPos.y / GDF_PG_SCALE);
    this->playerPgJoints[0].z = (s16)(rootPos.z / GDF_PG_SCALE);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Put(base);
    Matrix_Scale(GDF_PG_SCALE, GDF_PG_SCALE, GDF_PG_SCALE, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, BossGanon_Pg_NullDL(play->state.gfxCtx));
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    this->playerSpearValid = false;
    SkelAnime_DrawOpa(play, this->playerPgSkel.skeleton, this->playerPgJoints, BossGanon_Pg_OverrideLimbDraw,
                      BossGanon_Pg_PostLimbDraw, this);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// Door kick: while Link opens a door, Ganondorf raises his right leg straight out in front (the whole leg turns about
// the hip) and leans back, timed so the foot lands on the frame the door flies off (z_en_door.c)
#define GDF_KICK_LIFT_DEG 80.0f

// Raises his right leg straight out by `deg` (scaled by the weight `w`, 0 to 1) and leans back
static void BossGanon_Player_KickPose(BossGanon* this, f32 w, f32 deg) {
    Vec3s* joints = this->skelAnime.jointTable;
    GdfMat3 root;
    GdfMat3 pelvis;
    GdfMat3 hip;
    GdfMat3 thigh;
    GdfMat3 lift;
    GdfMat3 tmp;
    GdfMat3 tmp2;
    f32 a;

    w = w * w * (3.0f - 2.0f * w);
    BossGanon_Pg_FromRot(&joints[1 + 0], root);
    BossGanon_Pg_FromRot(&joints[1 + 18], tmp);
    BossGanon_Pg_Mul(root, tmp, pelvis, false);
    a = -deg * w * (M_PI / 180.0f);
    lift[0][0] = 1.0f; lift[0][1] = 0.0f;     lift[0][2] = 0.0f;
    lift[1][0] = 0.0f; lift[1][1] = cosf(a);  lift[1][2] = -sinf(a);
    lift[2][0] = 0.0f; lift[2][1] = sinf(a);  lift[2][2] = cosf(a);
    BossGanon_Pg_FromRot(&joints[1 + 22], thigh);
    BossGanon_Pg_Mul(pelvis, thigh, hip, false);
    BossGanon_Pg_Mul(lift, hip, tmp, false);
    BossGanon_Pg_Mul(pelvis, tmp, tmp2, true);
    BossGanon_Pg_ToRot(tmp2, &joints[1 + 22]);

    BossGanon_Player_TwistLean(this, 0.0f, -0x0C00 * w);
}

// ---- No door holds him ----
// Locked doors (small key or boss key), chained doors, barred doors waiting on a switch or a cleared room, the
// Gerudo cells, gates, grates and breakable walls: A in front of one and he kicks it in. A door he unlocks or unbars
// (and then kicks open as he goes through, like any door); a cell, gate, grate or wall he smashes to pieces.
#define GDF_BREAK_KICK_FRAMES 22
#define GDF_BREAK_KICK_IMPACT 8
#define GDF_BREAK_RANGE 95.0f

static s32 BossGanon_Player_IsSmashable(s16 id) {
    switch (id) {
        case ACTOR_DOOR_GERUDO:
        case ACTOR_BG_GATE_SHUTTER:
        case ACTOR_BG_SPOT12_GATE:
        case ACTOR_BG_SPOT12_SAKU:
        case ACTOR_BG_SPOT15_SAKU:
        case ACTOR_BG_HAKA_GATE:
        case ACTOR_BG_MIZU_SHUTTER:
        case ACTOR_BG_ICE_SHUTTER:
        case ACTOR_BG_SPOT18_SHUTTER:
        case ACTOR_BG_JYA_KANAAMI:
        case ACTOR_BG_BREAKWALL:
        case ACTOR_BG_HIDAN_KOWARERUKABE:
        case ACTOR_BG_INGATE:
            return true;
        default:
            return false;
    }
}

// Shut to him: locked, chained or barred (and so not opened the normal way)
static s32 BossGanon_Player_IsShut(PlayState* play, Actor* a) {
    if (a->id == ACTOR_EN_DOOR) {
        return ((EnDoor*)a)->lockTimer != 0;
    }
    if (a->id == ACTOR_BG_SPOT02_OBJECTS) {
        // The graveyard: the great stone door to the Shadow Temple (raised by a switch, the torches), and the royal
        // family's tombstone
        BgSpot02Objects* o = (BgSpot02Objects*)a;

        return ((a->params == 0) && !Flags_GetSwitch(play, o->unk_16B)) ||
               ((a->params == 2) && (play->sceneNum == SCENE_GRAVEYARD));
    }
    if (a->id == ACTOR_DOOR_SHUTTER) {
        DoorShutter* d = (DoorShutter*)a;
        s32 flag = a->params & 0x3F;

        if (d->unlockTimer != 0) {
            return true; // small key or boss key
        }
        switch (d->doorType) {
            case SHUTTER_FRONT_CLEAR:
                return !Flags_GetClear(play, a->room) && !Flags_GetTempClear(play, a->room);
            case SHUTTER_FRONT_SWITCH:
            case SHUTTER_FRONT_SWITCH_BACK_CLEAR:
                return !Flags_GetSwitch(play, flag);
            default:
                return false;
        }
    }
    return BossGanon_Player_IsSmashable(a->id);
}

static Actor* BossGanon_Player_FindShutDoor(PlayState* play) {
    // (the Kakariko gate to Death Mountain is an item-action actor)
    static const u8 sCats[] = { ACTORCAT_DOOR, ACTORCAT_BG, ACTORCAT_PROP, ACTORCAT_ITEMACTION };
    Player* player = GET_PLAYER(play);
    Actor* best = NULL;
    f32 bestDist = 1e9f;
    s32 c;

    for (c = 0; c < ARRAY_COUNT(sCats); c++) {
        Actor* a;

        for (a = play->actorCtx.actorLists[sCats[c]].head; a != NULL; a = a->next) {
            f32 d;
            s16 dyaw;

            if ((a->update == NULL) || !BossGanon_Player_IsShut(play, a)) {
                continue;
            }
            d = Math_Vec3f_DistXZ(&player->actor.world.pos, &a->world.pos);
            dyaw = Math_Vec3f_Yaw(&player->actor.world.pos, &a->world.pos) - player->actor.shape.rot.y;
            // (gates and grates are wide, and their origin may be at one end: further for them)
            if (d > ((BossGanon_Player_IsSmashable(a->id) || (a->id == ACTOR_BG_SPOT02_OBJECTS)) ? 170.0f
                                                                                                : GDF_BREAK_RANGE)) {
                continue;
            }
            if ((d < bestDist) && (ABS(dyaw) < 0x2C00) && (fabsf(a->world.pos.y - player->actor.world.pos.y) < 120.0f)) {
                best = a;
                bestDist = d;
            }
        }
    }
    return best;
}

// A in front of a shut door (from the input filter, before Link reads A): the kick starts
s32 BossGanon_Player_TryBreakKick(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    Player* player = GET_PLAYER(play);
    Actor* door;

    if ((sGdfBreakTimer > 0) || !this->playerGrounded || this->playerHidden || BossGanon_Player_IsBusy(thisx) ||
        !PlayAsGanondorf_ShouldKickDoors()) {
        return false;
    }
    door = BossGanon_Player_FindShutDoor(play);
    if (door == NULL) {
        return false;
    }
    sGdfBreakTarget = door;
    sGdfBreakTimer = 1;
    // Square up to it
    player->actor.shape.rot.y = player->actor.world.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &door->world.pos);
    player->linearVelocity = 0.0f;
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW)); // (one grunt, not a loop)
    return true;
}

static void BossGanon_Player_BreakDoor(BossGanon* this, PlayState* play, Actor* a) {
    Vec3f pos = a->world.pos;
    s32 flag = a->params & 0x3F;

    pos.y += 50.0f;
    Actor_RequestQuakeAndRumble(&this->actor, play, 5, 8);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_WOODBOX_BREAK);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_HAMMER_HIT);
    EffectSsHahen_SpawnBurst(play, &pos, 12.0f, 0, 12, 10, 24, HAHEN_OBJECT_DEFAULT, 10, NULL);

    if ((a->id == ACTOR_BG_SPOT02_OBJECTS) && (a->params == 0)) {
        // The Shadow Temple's stone door: it grinds up as if the torches were lit
        s32 doorFlag = ((BgSpot02Objects*)a)->unk_16B;

        Flags_SetSwitch(play, doorFlag);
        GdfInvasion_KeepKickedSwitch(doorFlag);
        return;
    }
    if (a->id == ACTOR_EN_DOOR) {
        // Unlocked for good (no key spent): he kicks it open as he goes through
        ((EnDoor*)a)->lockTimer = 0;
        Flags_SetSwitch(play, flag);
        GdfInvasion_KeepKickedSwitch(flag);
        Audio_PlayActorSound2(a, NA_SE_EV_CHAIN_KEY_UNLOCK);
    } else if (a->id == ACTOR_DOOR_SHUTTER) {
        DoorShutter* d = (DoorShutter*)a;

        if (d->unlockTimer != 0) {
            d->unlockTimer = 0; // the chains (or the boss lock) burst off
            Flags_SetSwitch(play, flag);
            GdfInvasion_KeepKickedSwitch(flag);
            Audio_PlayActorSound2(a, NA_SE_EV_CHAIN_KEY_UNLOCK);
        } else if (d->doorType == SHUTTER_FRONT_CLEAR) {
            Flags_SetTempClear(play, a->room); // the bars give way (until he leaves the room)
            GdfInvasion_KeepKickedClear(a->room);
        } else {
            Flags_SetSwitch(play, flag);
            GdfInvasion_KeepKickedSwitch(flag);
        }
    } else {
        // Smashed to pieces (back when the area is loaded again; a cell stays open)
        if (a->id == ACTOR_DOOR_GERUDO) {
            Flags_SetSwitch(play, flag);
            GdfInvasion_KeepKickedSwitch(flag);
        }
        EffectSsHahen_SpawnBurst(play, &pos, 18.0f, 0, 16, 14, 30, HAHEN_OBJECT_DEFAULT, 10, NULL);
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_WALL_BROKEN);
        Actor_Kill(a);
    }
}

static void BossGanon_Player_ApplyBreakKick(BossGanon* this, PlayState* play) {
    f32 w;

    if (sGdfBreakTimer <= 0) {
        return;
    }
    if (sGdfBreakTimer < GDF_BREAK_KICK_IMPACT) {
        w = sGdfBreakTimer / (f32)GDF_BREAK_KICK_IMPACT;
    } else if (sGdfBreakTimer < GDF_BREAK_KICK_IMPACT + 5) {
        w = 1.0f;
    } else {
        w = (GDF_BREAK_KICK_FRAMES - sGdfBreakTimer) / (f32)(GDF_BREAK_KICK_FRAMES - GDF_BREAK_KICK_IMPACT - 5);
    }
    BossGanon_Player_KickPose(this, CLAMP(w, 0.0f, 1.0f), GDF_KICK_LIFT_DEG);
    if ((sGdfBreakTimer == GDF_BREAK_KICK_IMPACT) && (sGdfBreakTarget != NULL) &&
        (sGdfBreakTarget->update != NULL)) {
        BossGanon_Player_BreakDoor(this, play, sGdfBreakTarget);
    }
    if (++sGdfBreakTimer > GDF_BREAK_KICK_FRAMES) {
        sGdfBreakTimer = 0;
        sGdfBreakTarget = NULL;
    }
}

// ---- The Master Sword: he kicks it out of the pedestal instead of drawing it ----
// The pedestal's sword (Bg_Toki_Swd) stops drawing the moment Link takes it. Here it stays in the stone until his
// foot meets it (the door kick, without a door), then the pedestal actor itself flies off spinning (its draw follows
// its position and rotation) and, once out of sight, is no longer drawn. The cutscene and the sword in his inventory
// go on as the game has them.
#define GDF_SWD_FLIGHT_FRAMES 50
static Actor* sGdfSwd = NULL;
static ActorFunc sGdfSwdDraw = NULL;
static s16 sGdfSwdTimer = -1; // -1: waiting; then the kick and the flight; 999: gone
static Vec3f sGdfSwdVel;

static void BossGanon_Player_MasterSwordKick(BossGanon* this, PlayState* play) {
    Actor* swd;

    if (play->sceneNum != SCENE_TEMPLE_OF_TIME) {
        sGdfSwd = NULL;
        sGdfSwdTimer = -1;
        return;
    }
    swd = Actor_Find(&play->actorCtx, ACTOR_BG_TOKI_SWD, ACTORCAT_PROP);
    if (swd != sGdfSwd) {
        sGdfSwd = swd;
        sGdfSwdDraw = NULL;
        sGdfSwdTimer = -1;
    }
    if (swd == NULL) {
        return;
    }
    if (sGdfSwdTimer < 0) {
        // In the stone: remember how it's drawn. Gone from it as a child: Link has just taken it
        if (swd->draw != NULL) {
            sGdfSwdDraw = swd->draw;
        } else if ((sGdfSwdDraw != NULL) && !LINK_IS_ADULT && !this->playerHidden) {
            swd->draw = sGdfSwdDraw;
            sGdfSwdTimer = 0;
            sGdfBreakTimer = 1; // the door kick's pose
            sGdfBreakTarget = NULL;
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW));
        }
        return;
    }
    if (sGdfSwdTimer >= 999) {
        return;
    }
    if (sGdfSwdTimer == GDF_BREAK_KICK_IMPACT) {
        s16 yaw = this->actor.shape.rot.y;

        // Out of the stone, forward and up over the pedestal
        sGdfSwdVel.x = Math_SinS(yaw) * 16.0f;
        sGdfSwdVel.y = 24.0f;
        sGdfSwdVel.z = Math_CosS(yaw) * 16.0f;
        Audio_PlayActorSound2(swd, NA_SE_IT_SHIELD_REFLECT_SW);
        Audio_PlayActorSound2(swd, NA_SE_IT_HAMMER_HIT);
    }
    if (sGdfSwdTimer > GDF_BREAK_KICK_IMPACT) {
        swd->world.pos.x += sGdfSwdVel.x;
        swd->world.pos.y += sGdfSwdVel.y;
        swd->world.pos.z += sGdfSwdVel.z;
        sGdfSwdVel.y -= 0.6f;
        swd->shape.rot.x += 0x2400; // end over end
        swd->shape.rot.y += 0x0600;
        if ((sGdfSwdTimer % 5) == 0) {
            Audio_PlayActorSound2(swd, NA_SE_IT_SWORD_SWING_HARD);
        }
    }
    if (++sGdfSwdTimer > GDF_BREAK_KICK_IMPACT + GDF_SWD_FLIGHT_FRAMES) {
        swd->draw = NULL; // off-screen and away
        sGdfSwdTimer = 999;
    }
}

// Chests: while Link opens one, he boots the lid open instead, the foot landing on the frame the lid bursts open
// (z_en_box.c, EN_BOX_GDF_KICK_FRAME). A lower kick than for doors, so the foot stops at the lid.
#define GDF_CHEST_KICK_DEG 58.0f

static void BossGanon_Player_ApplyChestKick(BossGanon* this, PlayState* play) {
    static Actor* sKickedBox = NULL;
    static s16 sAfterKick = 0; // frames since the foot landed
    Player* player = GET_PLAYER(play);
    Actor* actor;
    EnBox* opening = NULL;
    f32 w = 0.0f;

    if (!PlayAsGanondorf_ShouldKickDoors() || this->playerHidden) {
        sKickedBox = NULL;
        return;
    }
    for (actor = play->actorCtx.actorLists[ACTORCAT_CHEST].head; actor != NULL; actor = actor->next) {
        if ((actor->id == ACTOR_EN_BOX) && (((EnBox*)actor)->unk_1F4 > 0) &&
            (Actor_WorldDistXZToActor(actor, &player->actor) < 120.0f)) {
            opening = (EnBox*)actor;
            break;
        }
    }
    if (opening == NULL) {
        sKickedBox = NULL;
        return;
    }

    if (opening->skelanime.curFrame < EN_BOX_GDF_KICK_FRAME) {
        // Winding up, the foot coming up to the lid
        if ((sKickedBox != &opening->dyna.actor) && (opening->skelanime.curFrame >= 66.0f)) {
            w = (opening->skelanime.curFrame - 66.0f) / (EN_BOX_GDF_KICK_FRAME - 66.0f);
        }
    } else {
        if (sKickedBox != &opening->dyna.actor) {
            sKickedBox = &opening->dyna.actor;
            sAfterKick = 0;
        }
        // Held a moment as the lid flies, then back down
        sAfterKick++;
        w = (sAfterKick < 8) ? 1.0f : 1.0f - ((sAfterKick - 8) / 12.0f);
    }
    if (w > 0.0f) {
        BossGanon_Player_KickPose(this, CLAMP(w, 0.0f, 1.0f), GDF_CHEST_KICK_DEG);
    }
}

static void BossGanon_Player_ApplyDoorKick(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    EnDoor* door = (EnDoor*)player->doorActor;
    Vec3s* joints = this->skelAnime.jointTable;
    GdfMat3 root;
    GdfMat3 pelvis;
    GdfMat3 hip;
    GdfMat3 thigh;
    GdfMat3 lift;
    GdfMat3 tmp;
    GdfMat3 tmp2;
    f32 frame;
    f32 w;
    f32 a;

    if ((door == NULL) || (door->actor.id != ACTOR_EN_DOOR) || (door->actor.update == NULL) ||
        !door->playerIsOpening || !PlayAsGanondorf_ShouldKickDoors() || this->playerHidden) {
        return;
    }
    frame = door->skelAnime.curFrame;
    if (frame < 12.0f) {
        w = 0.0f;
    } else if (frame < 23.0f) {
        w = (frame - 12.0f) / 11.0f;
    } else if (frame < 30.0f) {
        w = 1.0f;
    } else if (frame < 42.0f) {
        w = 1.0f - (frame - 30.0f) / 12.0f;
    } else {
        w = 0.0f;
    }
    if (w <= 0.0f) {
        return;
    }
    w = w * w * (3.0f - 2.0f * w);

    // Turn the right thigh about the actor's sideways axis, through the root and pelvis
    BossGanon_Pg_FromRot(&joints[1 + 0], root);
    BossGanon_Pg_FromRot(&joints[1 + 18], tmp);
    BossGanon_Pg_Mul(root, tmp, pelvis, false);
    a = -GDF_KICK_LIFT_DEG * w * (M_PI / 180.0f);
    lift[0][0] = 1.0f; lift[0][1] = 0.0f;     lift[0][2] = 0.0f;
    lift[1][0] = 0.0f; lift[1][1] = cosf(a);  lift[1][2] = -sinf(a);
    lift[2][0] = 0.0f; lift[2][1] = sinf(a);  lift[2][2] = cosf(a);
    BossGanon_Pg_FromRot(&joints[1 + 22], thigh);
    BossGanon_Pg_Mul(pelvis, thigh, hip, false);      // thigh in actor space
    BossGanon_Pg_Mul(lift, hip, tmp, false);          // lifted
    BossGanon_Pg_Mul(pelvis, tmp, tmp2, true);        // back to pelvis space
    BossGanon_Pg_ToRot(tmp2, &joints[1 + 22]);

    BossGanon_Player_TwistLean(this, 0.0f, -0x0C00 * w);
}

static void BossGanon_Pg_RotX(f32 deg, GdfMat3 out) {
    f32 a = deg * (M_PI / 180.0f);

    out[0][0] = 1.0f; out[0][1] = 0.0f;     out[0][2] = 0.0f;
    out[1][0] = 0.0f; out[1][1] = cosf(a);  out[1][2] = -sinf(a);
    out[2][0] = 0.0f; out[2][1] = sinf(a);  out[2][2] = cosf(a);
}

static f32 BossGanon_Player_RunBump(f32 phase, f32 center, f32 width) {
    f32 d = phase - center;

    while (d > 0.5f) {
        d -= 1.0f;
    }
    while (d < -0.5f) {
        d += 1.0f;
    }
    if (fabsf(d) >= width) {
        return 0.0f;
    }
    return 0.5f * (1.0f + cosf(M_PI * d / width));
}

// Sprinting, from motion capture: a jogging man's run cycle from the CMU Graphics Lab Motion Capture Database
// (mocap.cs.cmu.edu, clips 16_35 and 16_36; the database was created with funding from NSF EIA-0196217; free for all
// uses), averaged over its two runs and retargeted to him. For
// each limb only its swing forward and back is taken (the angle off straight down, in his body's side view): his own
// spread (broad arms, wide stance) and twist stay his. The heel kicks up a little less than the runner's, for his
// weight. Phase 0 is the right heel coming down; tenths of a degree, forward positive.
#define GDF_MOCAP_RUN_SAMPLES 32

// Left thigh, left shin, right thigh, right shin, left upper arm, left forearm, right upper arm, right forearm
static const s16 sGdfMocapRun[GDF_MOCAP_RUN_SAMPLES][8] = {
    { -158, -493, 292, -24, -199, 889, -414, 749 },
    { -135, -539, 272, -23, -222, 843, -406, 766 },
    { -99, -573, 246, -33, -235, 818, -382, 791 },
    { -53, -597, 217, -57, -248, 794, -355, 817 },
    { -1, -610, 188, -95, -260, 773, -332, 844 },
    { 53, -611, 159, -142, -271, 757, -310, 873 },
    { 107, -599, 127, -191, -283, 742, -285, 904 },
    { 158, -571, 91, -234, -297, 726, -256, 938 },
    { 205, -525, 48, -266, -315, 712, -226, 973 },
    { 245, -463, -3, -291, -335, 705, -205, 1009 },
    { 274, -392, -54, -314, -352, 704, -193, 1042 },
    { 292, -314, -99, -339, -367, 707, -184, 1071 },
    { 301, -236, -133, -367, -381, 709, -177, 1090 },
    { 301, -165, -157, -395, -392, 710, -172, 1097 },
    { 296, -108, -170, -422, -399, 713, -173, 1091 },
    { 285, -63, -173, -450, -402, 720, -178, 1078 },
    { 274, -33, -162, -483, -400, 730, -183, 1059 },
    { 265, -22, -138, -517, -391, 743, -190, 1035 },
    { 256, -30, -101, -549, -378, 760, -198, 1006 },
    { 243, -53, -51, -576, -359, 781, -209, 971 },
    { 227, -89, 5, -594, -338, 802, -226, 931 },
    { 208, -136, 63, -599, -316, 825, -246, 892 },
    { 181, -187, 120, -591, -291, 850, -267, 852 },
    { 141, -234, 175, -565, -262, 877, -288, 811 },
    { 95, -273, 226, -520, -232, 902, -310, 774 },
    { 46, -304, 268, -456, -208, 926, -333, 750 },
    { -7, -326, 297, -378, -191, 949, -353, 740 },
    { -60, -343, 313, -296, -180, 967, -369, 736 },
    { -107, -361, 318, -216, -172, 980, -382, 736 },
    { -141, -382, 318, -144, -166, 985, -390, 737 },
    { -161, -407, 313, -84, -162, 982, -395, 740 },
    { -167, -443, 305, -43, -171, 951, -404, 743 },
};

// His stride is centred under his hips: the runner's legs reach on average 11 degrees ahead of straight down (under a
// torso leaning forward as far), which on him - his torso leans by its own amount - read as leaning back. Legs swing
// this much further back, arms this much further forward.
#define GDF_MOCAP_LEG_SHIFT_DEG -10.0f
#define GDF_MOCAP_ARM_SHIFT_DEG 8.0f

static f32 BossGanon_Player_MocapPitch(f32 phase, s32 bone) {
    f32 t = phase * GDF_MOCAP_RUN_SAMPLES;
    s32 i = (s32)floorf(t);
    f32 w = t - i;
    s32 a = ((i % GDF_MOCAP_RUN_SAMPLES) + GDF_MOCAP_RUN_SAMPLES) % GDF_MOCAP_RUN_SAMPLES;
    s32 b = (a + 1) % GDF_MOCAP_RUN_SAMPLES;
    f32 va = sGdfMocapRun[a][bone];
    f32 vb = sGdfMocapRun[b][bone];

    while (vb - va > 1800.0f) {
        vb -= 3600.0f;
    }
    while (vb - va < -1800.0f) {
        vb += 3600.0f;
    }
    f32 v = va + (vb - va) * w;

    if (bone >= 4) {
        // The jogger's arms barely swing (about 12 degrees each way, both behind him): on him that read as stiff, so
        // they swing wider about their middle - the upper arms about three times as far, the forearms half as far
        // again (they pump up as the arm comes forward)
        static const f32 sMean[4] = { -287.0f, 814.0f, -281.0f, 895.0f };
        f32 mean = sMean[bone - 4];
        f32 gain = (bone & 1) ? 1.5f : 3.0f;
        f32 shift = (bone & 1) ? 0.0f : 18.0f;

        return ((mean + (v - mean) * gain) * 0.1f + shift) * (M_PI / 180.0f);
    }
    return ((v * 0.1f) + GDF_MOCAP_LEG_SHIFT_DEG) * (M_PI / 180.0f);
}

// Turns `limb` (a limb's rotation in his model space; the bone runs along its x) so the bone swings to `pitch`, keeping
// how far out to the side it already leans; by `weight` of the way
static void BossGanon_Player_SwingBoneToward(GdfMat3 limb, const f32 want[3], f32 weight);

static void BossGanon_Player_SwingBoneTo(GdfMat3 limb, f32 pitch, f32 weight) {
    f32 side = CLAMP(limb[0][0], -0.95f, 0.95f);
    f32 r = sqrtf(1.0f - SQ(side));
    f32 want[3] = { side, -cosf(pitch) * r, sinf(pitch) * r };

    BossGanon_Player_SwingBoneToward(limb, want, weight);
}

// Turns `limb` (its bone along its x) so the bone points along `want` (a unit vector in his model space), by `weight`
static void BossGanon_Player_SwingBoneToward(GdfMat3 limb, const f32 want[3], f32 weight) {
    f32 cur[3] = { limb[0][0], limb[1][0], limb[2][0] };
    f32 axis[3] = { cur[1] * want[2] - cur[2] * want[1], cur[2] * want[0] - cur[0] * want[2],
                    cur[0] * want[1] - cur[1] * want[0] };
    f32 len = sqrtf(SQ(axis[0]) + SQ(axis[1]) + SQ(axis[2]));
    f32 dot = CLAMP(cur[0] * want[0] + cur[1] * want[1] + cur[2] * want[2], -1.0f, 1.0f);
    f32 angle = atan2f(len, dot) * weight;
    f32 c;
    f32 sn;
    f32 v;
    GdfMat3 rot;
    GdfMat3 out;

    if (len < 1e-5f) {
        return;
    }
    axis[0] /= len;
    axis[1] /= len;
    axis[2] /= len;
    c = cosf(angle);
    sn = sinf(angle);
    v = 1.0f - c;
    rot[0][0] = c + axis[0] * axis[0] * v;
    rot[0][1] = axis[0] * axis[1] * v - axis[2] * sn;
    rot[0][2] = axis[0] * axis[2] * v + axis[1] * sn;
    rot[1][0] = axis[1] * axis[0] * v + axis[2] * sn;
    rot[1][1] = c + axis[1] * axis[1] * v;
    rot[1][2] = axis[1] * axis[2] * v - axis[0] * sn;
    rot[2][0] = axis[2] * axis[0] * v - axis[1] * sn;
    rot[2][1] = axis[2] * axis[1] * v + axis[0] * sn;
    rot[2][2] = c + axis[2] * axis[2] * v;
    BossGanon_Pg_Mul(rot, limb, out, false);
    memcpy(limb, out, sizeof(GdfMat3));
}

// A two-bone chain (thigh and shin, or upper arm and forearm) under `parent`, swung to the run cycle's angles
static void BossGanon_Player_MocapChain(Vec3s* joints, GdfMat3 parent, s32 upper, s32 lower, f32 pitchUpper,
                                        f32 pitchLower, f32 weight) {
    GdfMat3 tmp;
    GdfMat3 up;
    GdfMat3 lo;
    GdfMat3 local;

    BossGanon_Pg_FromRot(&joints[1 + upper], tmp);
    BossGanon_Pg_Mul(parent, tmp, up, false);
    BossGanon_Pg_FromRot(&joints[1 + lower], local);
    BossGanon_Player_SwingBoneTo(up, pitchUpper, weight);
    BossGanon_Pg_Mul(up, local, lo, false); // the lower bone rides along, then swings on its own
    BossGanon_Player_SwingBoneTo(lo, pitchLower, weight);

    BossGanon_Pg_Mul(parent, up, local, true);
    BossGanon_Pg_ToRot(local, &joints[1 + upper]);
    BossGanon_Pg_Mul(up, lo, local, true);
    BossGanon_Pg_ToRot(local, &joints[1 + lower]);
}

static f32 BossGanon_Player_RunPhase(BossGanon* this) {
    f32 phase = (this->playerWalkFrame - 1.0f) / GDF_WALK_CYCLE_FRAMES; // the right heel comes down at walk frame 1

    return phase - floorf(phase);
}

static void BossGanon_Player_ApplyRunArms(BossGanon* this, f32 weight) {
    Vec3s* joints = this->skelAnime.jointTable;
    f32 phase = BossGanon_Player_RunPhase(this);
    GdfMat3 rootRot;
    GdfMat3 body;
    GdfMat3 tmp;
    GdfMat3 shoulder;

    if (weight <= 0.0f) {
        return;
    }
    BossGanon_Pg_FromRot(&joints[1 + 0], rootRot);
    BossGanon_Pg_FromRot(&joints[1 + 1], tmp);
    BossGanon_Pg_Mul(rootRot, tmp, body, false);

    // Left: shoulder 2, upper arm 3, forearm 4. Right: 6, 7, 8.
    BossGanon_Pg_FromRot(&joints[1 + 2], tmp);
    BossGanon_Pg_Mul(body, tmp, shoulder, false);
    BossGanon_Player_MocapChain(joints, shoulder, 3, 4, BossGanon_Player_MocapPitch(phase, 4),
                                BossGanon_Player_MocapPitch(phase, 5), weight);
    BossGanon_Pg_FromRot(&joints[1 + 6], tmp);
    BossGanon_Pg_Mul(body, tmp, shoulder, false);
    BossGanon_Player_MocapChain(joints, shoulder, 7, 8, BossGanon_Player_MocapPitch(phase, 6),
                                BossGanon_Player_MocapPitch(phase, 7), weight);
}

static void BossGanon_Player_ApplyRunLegs(BossGanon* this, f32 weight) {
    Vec3s* joints = this->skelAnime.jointTable;
    f32 phase = BossGanon_Player_RunPhase(this);
    GdfMat3 rootRot;
    GdfMat3 pelvis;
    GdfMat3 tmp;

    if (weight <= 0.0f) {
        return;
    }
    BossGanon_Pg_FromRot(&joints[1 + 0], rootRot);
    BossGanon_Pg_FromRot(&joints[1 + 18], tmp);
    BossGanon_Pg_Mul(rootRot, tmp, pelvis, false);

    // Left: thigh 19, shin 20. Right: 22, 23.
    BossGanon_Player_MocapChain(joints, pelvis, 19, 20, BossGanon_Player_MocapPitch(phase, 0),
                                BossGanon_Player_MocapPitch(phase, 1), weight);
    BossGanon_Player_MocapChain(joints, pelvis, 22, 23, BossGanon_Player_MocapPitch(phase, 2),
                                BossGanon_Player_MocapPitch(phase, 3), weight);
}

// ---- Summoning the trident ----
// D-Pad Left into the trident stance: Ganondorf thrusts his right arm up over his head, and the trident explodes into
// existence in his fist with a crack of thunder - dark fire, sparks and light flying off it - then he brings it down
// into his stance. Drawn from the start (no drawing it from his side first).
#define GDF_SUMMON_FRAMES 26
#define GDF_SUMMON_BURST_FRAME 9

static void BossGanon_Player_SetupSummonTrident(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -6.0f);
    this->actionFunc = BossGanon_Player_SummonTrident;
    this->playerSwordTimer = 0;
    this->playerWalkWeight = 0.0f;
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
}

// The burst (on his screen, `own`, with his own flair on top: his shock and rays of light; the others' screens get the
// fire, sparks, sound and shake)
static void BossGanon_Player_TridentBurst(BossGanon* this, PlayState* play, Vec3f* hand, s32 own) {
    static Color_RGBA8 sSparkPrim = { 255, 230, 255, 255 };
    static Color_RGBA8 sSparkEnv = { 170, 60, 255, 0 };
    Vec3f vel;
    Vec3f accel;
    Vec3f p;
    s32 i;

    Audio_PlayActorSound2(&this->actor, NA_SE_IT_EXPLOSION_LIGHT);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_LIGHTNING);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW_BIG));
    Actor_RequestQuakeAndRumble(&this->actor, play, 4, 10);

    // Dark fire bursting out of his fist
    for (i = 0; i < 24; i++) {
        vel.x = Rand_CenteredFloat(9.0f);
        vel.y = Rand_CenteredFloat(9.0f) + 1.5f;
        vel.z = Rand_CenteredFloat(9.0f);
        accel.x = vel.x * -0.08f;
        accel.y = 0.15f;
        accel.z = vel.z * -0.08f;
        EffectSsDeadDb_Spawn(play, hand, &vel, &accel, 60 + (s16)Rand_ZeroFloat(40.0f), -3, 200, 120, 255, 255, 90,
                             0, 170, 1, 10, false);
    }
    // Sparks flying off it
    for (i = 0; i < 20; i++) {
        vel.x = Rand_CenteredFloat(14.0f);
        vel.y = Rand_CenteredFloat(14.0f);
        vel.z = Rand_CenteredFloat(14.0f);
        accel.x = accel.z = 0.0f;
        accel.y = -0.3f;
        p = *hand;
        EffectSsKiraKira_SpawnDispersed(play, &p, &vel, &accel, &sSparkPrim, &sSparkEnv,
                                        (s16)(Rand_ZeroFloat(1000.0f) + 2000.0f), 20);
    }
    if (!own) {
        return;
    }
    // His own flair: the shock over his body and rays of light from his fist
    BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
    for (i = 0; i < 30; i++) {
        vel.x = Rand_CenteredFloat(20.0f);
        vel.y = Rand_CenteredFloat(20.0f);
        vel.z = Rand_CenteredFloat(20.0f);
        BossGanonEff_SpawnLightRay(play, hand, &vel, &sZeroVec, Rand_ZeroFloat(200.0f) + 300.0f, 10.0f, 0x1E);
    }
}

void BossGanon_Player_SummonTrident(BossGanon* this, PlayState* play) {
    s16 t = this->playerSwordTimer;
    f32 up;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->playerWalkWeight, 1.0f, 0.3f);

    // Up over his head (in 7 frames), held through the burst, then back down into his stance
    if (t < 7) {
        up = t / 7.0f;
    } else if (t < 17) {
        up = 1.0f;
    } else {
        up = (GDF_SUMMON_FRAMES - t) / (f32)(GDF_SUMMON_FRAMES - 17);
    }
    up = CLAMP(up, 0.0f, 1.0f);
    up = up * up * (3.0f - 2.0f * up);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfTridentArmPoses[0], up);

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.5f;
    sCape->sideSwayMagnitude = -1.0f;

    if (t == GDF_SUMMON_BURST_FRAME) {
        sPlayerSwordState = GDF_SWORD_DRAWN;
        BossGanon_Player_TridentBurst(this, play, &this->unk_2EC[8], true);
        sGdfSummonBurst = 8;
    }
    if (sGdfSummonBurst > 0) {
        sGdfSummonBurst--;
    }

    this->playerSwordTimer++;
    if (this->playerSwordTimer >= GDF_SUMMON_FRAMES) {
        sGdfSummonBurst = 0;
        BossGanon_Player_SetupSwordIdle(this, play);
    }
}

// D-Pad Left out of the trident stance (outside Invasion games): the summon in reverse. He raises the trident over his
// head and it comes apart into dark fire and sparks drawn in to his fist, then his empty hand comes down.
static void BossGanon_Player_SetupUnsummonTrident(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -6.0f);
    this->actionFunc = BossGanon_Player_UnsummonTrident;
    this->playerSwordTimer = 0;
    this->playerWalkWeight = 0.0f;
    this->playerSwordGuard = false;
    // His arm as it is in the stance: it goes up from there by the shortest turn (left to the animation's morph, which
    // mixes the joints' angles one by one, it tumbled on the way)
    for (s32 i = 0; i < 3; i++) {
        this->playerArmFrom[i] = this->skelAnime.jointTable[1 + GDF_RIGHT_ARM_LIMB + i];
    }
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
}

// The trident coming apart (on his screen, `own`, with his shock over it too)
static void BossGanon_Player_TridentVanish(BossGanon* this, PlayState* play, Vec3f* hand, s32 own) {
    static Color_RGBA8 sSparkPrim = { 255, 230, 255, 255 };
    static Color_RGBA8 sSparkEnv = { 170, 60, 255, 0 };
    static Vec3f sNoAccel = { 0.0f, 0.0f, 0.0f };
    Vec3f vel;
    Vec3f accel;
    Vec3f p;
    s32 i;

    Audio_PlayActorSound2(&this->actor, NA_SE_IT_EXPLOSION_LIGHT);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
    // Dark fire rising off where it was, slow
    for (i = 0; i < 14; i++) {
        vel.x = Rand_CenteredFloat(3.0f);
        vel.y = Rand_ZeroFloat(2.0f) + 1.0f;
        vel.z = Rand_CenteredFloat(3.0f);
        accel.x = accel.z = 0.0f;
        accel.y = 0.12f;
        p = *hand;
        p.y -= Rand_ZeroFloat(60.0f); // along the shaft, down from his fist
        EffectSsDeadDb_Spawn(play, &p, &vel, &accel, 50 + (s16)Rand_ZeroFloat(30.0f), -2, 200, 120, 255, 255, 90,
                             0, 170, 1, 10, false);
    }
    // Sparks drawn in to his fist from all round it
    for (i = 0; i < 18; i++) {
        s16 yaw = (s16)Rand_ZeroFloat(65536.0f);
        f32 r = 50.0f + Rand_ZeroFloat(30.0f);
        f32 dy = Rand_CenteredFloat(80.0f);

        p.x = hand->x + Math_SinS(yaw) * r;
        p.y = hand->y + dy;
        p.z = hand->z + Math_CosS(yaw) * r;
        vel.x = (hand->x - p.x) * 0.12f;
        vel.y = (hand->y - p.y) * 0.12f;
        vel.z = (hand->z - p.z) * 0.12f;
        EffectSsKiraKira_SpawnDispersed(play, &p, &vel, &sNoAccel, &sSparkPrim, &sSparkEnv,
                                        (s16)(Rand_ZeroFloat(1000.0f) + 1500.0f), 10);
    }
    if (own) {
        BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
    }
}

void BossGanon_Player_UnsummonTrident(BossGanon* this, PlayState* play) {
    s16 t = this->playerSwordTimer;
    f32 up;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->playerWalkWeight, 1.0f, 0.3f);

    if (t < 7) {
        up = t / 7.0f;
    } else if (t < 15) {
        up = 1.0f;
    } else {
        up = (GDF_SUMMON_FRAMES - t) / (f32)(GDF_SUMMON_FRAMES - 15);
    }
    up = CLAMP(up, 0.0f, 1.0f);
    up = up * up * (3.0f - 2.0f * up);
    if (t < 7) {
        // Up from the stance
        for (s32 i = 0; i < 3; i++) {
            Vec3s over = sGdfTridentArmPoses[0][i];

            BossGanon_Player_SlerpRot(&this->playerArmFrom[i], &over, up,
                                      &this->skelAnime.jointTable[1 + GDF_RIGHT_ARM_LIMB + i]);
        }
    } else if (t < 15) {
        BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfTridentArmPoses[0], 1.0f); // held
    } else {
        // Down the way it went up, to his side as it was in the stance, and only then hanging loose: straight from
        // overhead to hanging, the shortest turn swung it out sideways and rolled it, like out of its socket
        f32 s = (t - 15) / (f32)(GDF_SUMMON_FRAMES - 15);
        f32 down = CLAMP(s / 0.7f, 0.0f, 1.0f);
        f32 loose = CLAMP((s - 0.7f) / 0.3f, 0.0f, 1.0f);

        down = down * down * (3.0f - 2.0f * down);
        loose = loose * loose * (3.0f - 2.0f * loose);
        for (s32 i = 0; i < 3; i++) {
            Vec3s over = sGdfTridentArmPoses[0][i];
            Vec3s side;
            Vec3s* joint = &this->skelAnime.jointTable[1 + GDF_RIGHT_ARM_LIMB + i];

            BossGanon_Player_SlerpRot(&over, &this->playerArmFrom[i], down, &side);
            BossGanon_Player_SlerpRot(&side, joint, loose, joint);
        }
    }

    sCape->backPush = -1.0f;
    sCape->backSwayMagnitude = 0.3f;
    sCape->sideSwayMagnitude = -0.5f;

    if (t == GDF_SUMMON_BURST_FRAME) {
        sPlayerSwordState = GDF_SWORD_OFF;
        BossGanon_Player_TridentVanish(this, play, &this->unk_2EC[8], true);
        sGdfSummonBurst = GDF_VANISH_BURST + 8; // (the others' screens tell it from the summon by its count)
    }
    if (sGdfSummonBurst > GDF_VANISH_BURST) {
        sGdfSummonBurst--;
        if (sGdfSummonBurst == GDF_VANISH_BURST) {
            sGdfSummonBurst = 0;
        }
    }

    this->playerSwordTimer++;
    if (this->playerSwordTimer >= GDF_SUMMON_FRAMES) {
        sGdfSummonBurst = 0;
        BossGanon_Player_SetupFloat(this, play);
    }
}

// ---- The trident's moves, from motion capture ----
// Three of Mixamo's sword-and-shield clips (Adobe, free to use in games), retargeted offline to his skeleton: its slash
// (his first two hits: the second is the slash played back, a rising backhand), its running spin attack (his spin) and
// its run (how he runs with the trident), and its stab. Per frame: where his thighs, shins, upper arms and forearms point (x 30000,
// in his model space, facing +z) - left thigh, left shin, right thigh, right shin, left upper arm, left forearm, right
// upper arm, right forearm - then his pelvis, chest, head and feet as rotations in his model space. His hands keep his
// grip on the trident, and his shoulders their breadth.
static const Vec3s sGdfClipSlash[46][13] = {
    { { -991, -19568, 22718 }, { 4794, -29574, 1544 }, { -14664, -24194, -9981 }, { 8339, -24992, -14347 }, { 8749, -24256, 15333 }, { -28466, -2950, 9000 }, { -2404, -23224, -18838 }, { -25961, -13301, 7008 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { -1302, -19768, 22529 }, { 4807, -29542, 2040 }, { -15095, -24032, -9728 }, { 8362, -25061, -14214 }, { 7630, -22354, 18495 }, { -26148, -5318, 13711 }, { -4785, -22718, -18999 }, { -26747, -8943, 10229 }, { -10012, 384, -15721 }, { 12034, 3070, 15357 }, { 16258, -13878, 16591 }, { 60, 1969, 77 }, { 358, -10637, 47 } },
    { { -1724, -19913, 22372 }, { 4805, -29541, 2065 }, { -15365, -23848, -9757 }, { 8110, -25122, -14252 }, { 6729, -19424, 21850 }, { -21299, -6829, 19993 }, { -7854, -21775, -19083 }, { -26609, -4081, 13240 }, { -9914, 245, -15466 }, { 11565, 3293, 15280 }, { 15427, -13499, 17338 }, { 60, 1969, 77 }, { 243, -10662, 156 } },
    { { -2291, -19969, 22271 }, { 5105, -29501, 1906 }, { -15543, -23649, -9956 }, { 7682, -25194, -14362 }, { 6061, -16637, 24217 }, { -14942, -7735, 24837 }, { -10356, -19702, -20114 }, { -25559, -1730, 15612 }, { -9811, 104, -15219 }, { 11010, 3211, 15338 }, { 14674, -13334, 18070 }, { 221, 1880, 166 }, { 99, -10692, 293 } },
    { { -3930, -18716, 23114 }, { 7120, -29092, 1718 }, { -15603, -23435, -10360 }, { 7105, -25287, -14494 }, { 5385, -14270, 25833 }, { -8355, -8688, 27472 }, { -11877, -17048, -21640 }, { -24409, -1523, 17375 }, { -9675, -49, -14992 }, { 10372, 3007, 15397 }, { 13963, -13217, 18675 }, { 1838, 1261, 1385 }, { -52, -10722, 437 } },
    { { -5179, -17851, 23548 }, { 8924, -28592, 1684 }, { -15538, -23178, -11016 }, { 6453, -25374, -14646 }, { 4709, -12343, 26935 }, { -2229, -9977, 28204 }, { -13205, -14032, -22994 }, { -23692, -910, 18381 }, { -9468, -191, -14819 }, { 9650, 2793, 15396 }, { 13108, -13061, 19308 }, { 2240, 563, 1800 }, { -158, -10748, 533 } },
    { { -5533, -17223, 23932 }, { 10538, -28013, 2052 }, { -15179, -22852, -12139 }, { 5770, -25396, -14891 }, { 7044, -11516, 26791 }, { 840, -10427, 28117 }, { -13065, -10920, -24699 }, { -24556, 2776, 17009 }, { -9114, -311, -14738 }, { 8805, 2621, 15225 }, { 11972, -12797, 20007 }, { 2149, -180, 1700 }, { -176, -10766, 537 } },
    { { -4239, -16997, 24354 }, { 11558, -27613, 1984 }, { -14436, -22731, -13225 }, { 4872, -25208, -15519 }, { 9079, -10806, 26472 }, { 4680, -11007, 27512 }, { -12391, -7060, -26393 }, { -24776, 8537, 14603 }, { -8840, -348, -14493 }, { 8011, 2508, 15044 }, { 10985, -12465, 20498 }, { 1647, -774, 1699 }, { -171, -10770, 527 } },
    { { -879, -16339, 25145 }, { 11075, -27832, 1646 }, { -13610, -22643, -14215 }, { 3976, -24878, -16287 }, { 10422, -10316, 26172 }, { 9013, -11580, 26166 }, { -10834, -1764, -27920 }, { -23075, 14034, 13060 }, { -8606, -245, -14261 }, { 7384, 2292, 14892 }, { 10061, -12248, 20906 }, { 416, -95, 3104 }, { 359, -10608, 188 } },
    { { 1291, -16828, 24802 }, { 9964, -28125, 3117 }, { -13332, -22328, -14957 }, { 3691, -24269, -17246 }, { 12786, -10006, 25227 }, { 12603, -11842, 24514 }, { -7563, 3955, -28761 }, { -20597, 17506, 13011 }, { -8662, -67, -14054 }, { 7179, 2131, 14796 }, { 9817, -12126, 20671 }, { -327, 1450, 2902 }, { 3132, -9616, -1951 } },
    { { 1836, -18766, 23334 }, { 8491, -28093, 6221 }, { -13047, -22123, -15503 }, { 3140, -23535, -18337 }, { 16384, -9865, 23114 }, { 15182, -12342, 22741 }, { -4418, 7279, -28766 }, { -17664, 20562, 12853 }, { -9080, 204, -13937 }, { 7513, 2095, 14683 }, { 10469, -12050, 19555 }, { -390, 3146, 1501 }, { 5032, -8758, -3159 } },
    { { 2153, -18814, 23268 }, { 7162, -28632, 5378 }, { -12856, -22200, -15553 }, { 2431, -22876, -19255 }, { 21607, -9629, 18451 }, { 12202, -14279, 23393 }, { -3048, 7412, -28910 }, { -16107, 23125, 10286 }, { -9716, 545, -14106 }, { 8551, 2162, 14745 }, { 11283, -11860, 18610 }, { -70, 3595, 704 }, { 6902, -8893, -3958 } },
    { { 3026, -17925, 23865 }, { 5694, -29319, 2824 }, { -12132, -22719, -15384 }, { 1433, -22155, -20176 }, { 25224, -10560, 12339 }, { 8339, -15951, 24000 }, { -4693, 6100, -28996 }, { -14668, 24916, 8005 }, { -10661, 973, -14520 }, { 10346, 2394, 14939 }, { 13547, -11752, 16793 }, { -70, 3595, 706 }, { 9113, -9125, -5024 } },
    { { 3956, -17341, 24159 }, { 4281, -29689, 457 }, { -11198, -23103, -15520 }, { 111, -21616, -20802 }, { 26430, -12942, 5827 }, { 5269, -16888, 24228 }, { -13454, 2303, -26715 }, { -9319, 28061, 5072 }, { -12021, 1453, -15125 }, { 12922, 2649, 15467 }, { 16429, -11804, 14740 }, { -70, 3595, 707 }, { 12294, -8937, -6097 } },
    { { 4959, -16482, 24571 }, { 2810, -29796, -2073 }, { -9986, -23067, -16377 }, { -1146, -20621, -21760 }, { 25088, -16449, -145 }, { 4509, -17038, 24277 }, { -23531, 75, -18608 }, { -3517, 29762, -1368 }, { -14157, 1695, -15774 }, { 16247, 2644, 16453 }, { 19728, -11475, 12278 }, { -70, 3595, 707 }, { 14574, -7914, -5535 } },
    { { 5910, -15404, 25056 }, { 1452, -29656, -4292 }, { -10234, -23031, -16273 }, { -826, -18386, -23691 }, { 22899, -18659, -5242 }, { 3335, -14616, 25985 }, { -29452, -2852, -4944 }, { -5973, 29004, -4809 }, { -16445, 1652, -16241 }, { 19417, 2142, 17247 }, { 21992, -11254, 10602 }, { -70, 3595, 707 }, { 14701, -6097, -3964 } },
    { { 6513, -14371, 25516 }, { 560, -29429, -5797 }, { -10455, -23339, -15684 }, { -600, -15940, -25408 }, { 19173, -20522, -10547 }, { 7681, -12167, 26324 }, { -28988, -3601, 6834 }, { -12671, 27151, -1512 }, { -18317, 1488, -16167 }, { 21857, 1281, 17667 }, { 23060, -11524, 9829 }, { -70, 3595, 707 }, { 13971, -4355, -2498 } },
    { { 7074, -13487, 25847 }, { -114, -29186, -6939 }, { -10081, -23677, -15421 }, { -1010, -13691, -26675 }, { 14085, -21810, -15031 }, { 14546, -11082, 23782 }, { -22916, -7399, 17892 }, { -16237, 21046, 13908 }, { -19945, 1387, -15896 }, { 24030, 407, 17751 }, { 23892, -11801, 9031 }, { -70, 3595, 707 }, { 13055, -2486, -1224 } },
    { { 7780, -12991, 25898 }, { -671, -28963, -7791 }, { -9225, -23867, -15661 }, { -1784, -12338, -27287 }, { 8530, -21830, -18727 }, { 19625, -13576, 18180 }, { -7936, -12045, 26305 }, { 1317, 3066, 29814 }, { -21242, 1600, -15756 }, { 26139, -480, 17664 }, { 25057, -11943, 7849 }, { -70, 3595, 707 }, { 12177, -686, -189 } },
    { { 8595, -12832, 25719 }, { -1165, -28776, -8400 }, { -8341, -23902, -16098 }, { -2494, -11781, -27477 }, { 2022, -20839, -21486 }, { 21271, -18523, 10220 }, { 11330, -18816, 20435 }, { 17450, -11159, 21702 }, { -22238, 2009, -15827 }, { 28238, -1361, 17425 }, { 26757, -11828, 6126 }, { -70, 3595, 707 }, { 11543, 806, 615 } },
    { { 9271, -12870, 25464 }, { -1434, -28642, -8809 }, { -7713, -23964, -16317 }, { -2964, -11511, -27545 }, { -4444, -19239, -22586 }, { 18194, -23754, 2178 }, { 17377, -23584, 6470 }, { 23345, -16588, 8937 }, { -22979, 2512, -16000 }, { 29946, -2111, 16945 }, { 28599, -11428, 4061 }, { -70, 3595, 707 }, { 11375, 1756, 1231 } },
    { { 9634, -13050, 25236 }, { -1368, -28565, -9065 }, { -7350, -23720, -16833 }, { -3209, -11845, -27375 }, { -9618, -16926, -22826 }, { 12686, -26967, -3444 }, { 14938, -26008, -641 }, { 25866, -15169, 931 }, { -23457, 3061, -16230 }, { 31058, -2651, 16295 }, { 29978, -10719, 2236 }, { -70, 3595, 707 }, { 11856, 1443, 1395 } },
    { { 9857, -13319, 25009 }, { -1139, -28535, -9191 }, { -7227, -23847, -16706 }, { -3176, -11988, -27317 }, { -13187, -14033, -23004 }, { 8401, -27971, -6857 }, { 13118, -26898, -2102 }, { 25871, -13463, -7031 }, { -23770, 3651, -16512 }, { 31842, -3079, 15581 }, { 30635, -10315, 977 }, { -70, 3595, 707 }, { 12497, 1031, 1603 } },
    { { 10204, -13595, 24720 }, { -1033, -28528, -9223 }, { -7165, -24055, -16432 }, { -3077, -12081, -27287 }, { -15010, -11809, -23135 }, { 6759, -27755, -9164 }, { 13640, -26597, -2562 }, { 21886, -13444, -15500 }, { -24061, 4239, -16823 }, { 32588, -3441, 14847 }, { 31051, -9949, -63 }, { -70, 3595, 707 }, { 13150, 594, 1804 } },
    { { 10653, -13856, 24383 }, { -1052, -28544, -9173 }, { -7111, -24197, -16246 }, { -2955, -12245, -27227 }, { -15999, -10312, -23188 }, { 5449, -27473, -10749 }, { 13658, -26619, -2214 }, { 15620, -13440, -21804 }, { -24331, 4798, -17154 }, { -32250, -3728, 14120 }, { 31275, -9597, -903 }, { -70, 3595, 706 }, { 13676, 214, 1965 } },
    { { 11042, -14072, 24084 }, { -1094, -28576, -9066 }, { -7082, -24193, -16265 }, { -2806, -12561, -27099 }, { -16993, -9459, -22842 }, { 3289, -27145, -12343 }, { 13647, -26712, -504 }, { 10191, -13317, -24876 }, { -24506, 5272, -17475 }, { -31767, -3901, 13467 }, { 31257, -9319, -1464 }, { -70, 3595, 707 }, { 13920, -29, 2067 } },
    { { 11058, -14129, 24044 }, { -1100, -28633, -8885 }, { -7458, -24234, -16033 }, { -2487, -12804, -27016 }, { -16479, -10318, -22847 }, { 1903, -26997, -12944 }, { 12930, -26862, 3355 }, { 7572, -14510, -25142 }, { -24269, 5440, -17674 }, { -32158, -3817, 13123 }, { 30805, -9297, -1427 }, { -70, 3595, 707 }, { 13717, -191, 2186 } },
    { { 11047, -14133, 24046 }, { -1169, -28700, -8657 }, { -7831, -24361, -15660 }, { -2228, -12982, -26954 }, { -14673, -12577, -22946 }, { 1239, -27261, -12463 }, { 10963, -27101, 6733 }, { 7944, -15801, -24233 }, { -23909, 5527, -17829 }, { 32740, -3677, 12850 }, { 30287, -9309, -1288 }, { -70, 3595, 707 }, { 13454, -353, 2299 } },
    { { 10949, -14100, 24111 }, { -1235, -28768, -8420 }, { -8190, -24551, -15171 }, { -2038, -13080, -26922 }, { -12183, -15830, -22382 }, { 1336, -28053, -10548 }, { 8079, -27162, 9848 }, { 10217, -17342, -22246 }, { -23419, 5553, -17933 }, { 31879, -3509, 12637 }, { 29901, -9273, -1190 }, { -70, 3595, 707 }, { 13100, -478, 2408 } },
    { { 10416, -13991, 24408 }, { -1084, -28821, -8256 }, { -8877, -24891, -14199 }, { -1676, -13022, -26974 }, { -9299, -19352, -20953 }, { 2106, -29468, -5216 }, { 4149, -26670, 13095 }, { 13820, -19671, -17946 }, { -22591, 5429, -17905 }, { 30445, -3277, 12605 }, { 29970, -9095, -1200 }, { -70, 3595, 707 }, { 12690, -579, 2506 } },
    { { 9652, -13840, 24805 }, { -768, -28854, -8176 }, { -9468, -25160, -13317 }, { -1413, -12980, -27010 }, { -6002, -21837, -19676 }, { 3219, -29615, 3545 }, { -585, -25687, 15487 }, { 17192, -21659, -11633 }, { -21593, 5278, -17791 }, { 28798, -3100, 12682 }, { 30347, -8895, -1301 }, { -70, 3595, 707 }, { 12077, -616, 2507 } },
    { { 8149, -13492, 25526 }, { 97, -28823, -8319 }, { -10182, -25495, -12097 }, { -1174, -12649, -27178 }, { 266, -24631, -17124 }, { 3112, -26064, 14526 }, { -7143, -23550, 17157 }, { 19067, -23055, -2216 }, { -19969, 4883, -17467 }, { 26345, -2834, 13031 }, { 30439, -9406, -1061 }, { -70, 3595, 707 }, { 11076, -325, 2451 } },
    { { 6396, -13040, 26250 }, { 1637, -28671, -8676 }, { -10969, -25710, -10895 }, { -601, -12593, -27222 }, { 5732, -26941, -11887 }, { -930, -18046, 23947 }, { -14559, -20415, 16469 }, { 17457, -23238, 7432 }, { -18014, 4252, -17408 }, { 23706, -2452, 12981 }, { 30192, -10262, -924 }, { -70, 3595, 707 }, { 9903, -155, 2186 } },
    { { 4458, -12936, 26698 }, { 3589, -28487, -8694 }, { -12457, -25670, -9266 }, { 929, -13010, -27016 }, { 10359, -27235, -7136 }, { -6468, -10669, 27283 }, { -21351, -16635, 12938 }, { 13175, -21666, 16031 }, { -15977, 3605, -17387 }, { 20938, -2093, 12768 }, { 29506, -11592, -554 }, { -70, 3595, 707 }, { 9039, -1117, 1514 } },
    { { 3313, -13581, 26544 }, { 5088, -28595, -7510 }, { -13962, -25238, -8253 }, { 2979, -14382, -26159 }, { 13571, -26698, -1747 }, { -11139, -6038, 27193 }, { -25488, -13679, 7952 }, { 8475, -18285, 22222 }, { -14362, 2907, -17242 }, { 18617, -1617, 12745 }, { 28143, -12666, 796 }, { -70, 3595, 707 }, { 8660, -3214, 496 } },
    { { 2938, -14279, 26220 }, { 5938, -28951, -5155 }, { -15037, -24640, -8172 }, { 4679, -16206, -24808 }, { 14944, -25879, 2636 }, { -14764, -4759, 25678 }, { -27146, -12572, 2244 }, { 4574, -14599, 25806 }, { -13259, 2508, -16922 }, { 16946, -1245, 12861 }, { 26871, -13356, 2344 }, { -70, 3595, 707 }, { 8175, -5668, -719 } },
    { { 3175, -14954, 25813 }, { 6090, -29291, -2222 }, { -14976, -23786, -10486 }, { 5232, -18668, -22894 }, { 15627, -24848, 6194 }, { -16965, -4511, 24328 }, { -27638, -11398, -2492 }, { 344, -12245, 27385 }, { -12597, 2223, -16553 }, { 15869, -854, 13039 }, { 26042, -13706, 3593 }, { -70, 3595, 707 }, { 7316, -8168, -2123 } },
    { { 2809, -16084, 25168 }, { 6848, -29185, 1151 }, { -13739, -23311, -12955 }, { 4566, -20941, -20991 }, { 15669, -23718, 9588 }, { -18848, -3975, 22999 }, { -27322, -10039, -7259 }, { -4337, -10474, 27776 }, { -12040, 1926, -16203 }, { 14909, -429, 13149 }, { 25070, -14083, 4935 }, { -70, 3595, 707 }, { 7205, -10114, -3455 } },
    { { 2686, -17353, 24324 }, { 7155, -28735, 4806 }, { -11527, -23461, -14721 }, { 3111, -22598, -19485 }, { 15255, -22746, 12243 }, { -20753, -3298, 21411 }, { -26000, -9138, -11852 }, { -8987, -9221, 27096 }, { -11570, 1632, -15902 }, { 14090, -16, 13254 }, { 24015, -14464, 6377 }, { -47, 3612, 1069 }, { 5357, -11210, -3067 } },
    { { 2511, -18346, 23604 }, { 7550, -27638, 8894 }, { -9762, -23961, -15185 }, { 2141, -23551, -18460 }, { 14637, -22248, 13812 }, { -23366, -2865, 18596 }, { -23899, -9272, -15584 }, { -12871, -8475, 25739 }, { -11180, 1369, -15688 }, { 13549, 348, 13478 }, { 23655, -14770, 7248 }, { 1146, 3517, 1752 }, { 1607, -10981, -721 } },
    { { 2058, -18598, 23450 }, { 6878, -27960, 8423 }, { -10812, -24161, -14119 }, { 3581, -24001, -17639 }, { 13364, -22708, 14345 }, { -25839, -2490, 15037 }, { -21197, -10625, -18379 }, { -16242, -7788, 23990 }, { -10785, 1123, -15611 }, { 13128, 632, 13791 }, { 23575, -15107, 7891 }, { 1240, 2895, 1421 }, { 422, -10622, -13 } },
    { { 1092, -18090, 23908 }, { 5232, -29326, 3555 }, { -12220, -23893, -13409 }, { 5150, -24482, -16557 }, { 12800, -23583, 13415 }, { -27642, -2277, 11433 }, { -17594, -12974, -20546 }, { -19829, -8087, 21010 }, { -10565, 966, -15659 }, { 12866, 987, 14104 }, { 21696, -15210, 10099 }, { 977, 1945, 505 }, { 422, -10622, -13 } },
    { { 131, -18188, 23858 }, { 4464, -29650, 980 }, { -13092, -23870, -12603 }, { 6195, -24709, -15846 }, { 12759, -24242, 12228 }, { -28715, -2323, 8370 }, { -13555, -15611, -21738 }, { -22746, -9003, 17365 }, { -10423, 842, -15727 }, { 12680, 1392, 14411 }, { 19652, -15112, 12420 }, { 840, 1640, 252 }, { 422, -10622, -13 } },
    { { -666, -18824, 23350 }, { 4420, -29672, -215 }, { -13716, -23952, -11755 }, { 7001, -24808, -15348 }, { 12646, -24538, 11746 }, { -29069, -2809, 6863 }, { -9454, -18331, -21785 }, { -24693, -10339, 13541 }, { -10324, 743, -15813 }, { 12549, 1825, 14715 }, { 18173, -14885, 14146 }, { 225, 1904, 70 }, { 422, -10622, -14 } },
    { { -931, -19192, 23039 }, { 4440, -29669, -144 }, { -14208, -24086, -10863 }, { 7684, -24875, -14906 }, { 11105, -24937, 12444 }, { -29136, -2671, 6629 }, { -5659, -20929, -20736 }, { -25681, -11850, 10003 }, { -10239, 640, -15894 }, { 12449, 2271, 15015 }, { 17263, -14610, 15299 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { -991, -19568, 22718 }, { 4794, -29574, 1544 }, { -14664, -24194, -9981 }, { 8339, -24992, -14347 }, { 8749, -24256, 15333 }, { -28466, -2950, 9000 }, { -2404, -23224, -18838 }, { -25961, -13302, 7008 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16691, -14354, 16131 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
};

static const Vec3s sGdfClipSpin[40][13] = {
    { { 1553, -22943, 19266 }, { 1485, -8365, -28772 }, { -2442, -29892, 710 }, { -475, -21377, -21043 }, { 9985, -19833, -20173 }, { 9056, -27990, 5878 }, { -12330, -26709, -5882 }, { 5100, -12483, 26799 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { -9845, -20357, 19715 }, { 11104, -14425, -23846 }, { -6731, -29001, -3689 }, { 3086, -22309, -19819 }, { 19875, -14374, -17274 }, { -2663, -21677, 20568 }, { -11215, -27692, -2712 }, { 2965, -12202, 27245 }, { -7729, -164, -15984 }, { 9358, 4426, 14623 }, { 21358, -14460, 12707 }, { -413, -1928, 240 }, { -2438, -12619, 2379 } },
    { { -16628, -18220, 17075 }, { 17642, -19314, -14688 }, { -9171, -27133, -8929 }, { 3031, -23982, -17767 }, { 27099, -12078, -4445 }, { -17073, -16821, 18044 }, { -11434, -27603, 2705 }, { 1289, -10407, 28107 }, { -4696, -1204, -15918 }, { 6438, 6000, 13343 }, { 23021, -13500, 12126 }, { 313, -6042, -24 }, { -5775, -13868, 5201 } },
    { { -19352, -19572, 11934 }, { 21720, -20549, -2451 }, { -9578, -25375, -12821 }, { -195, -24924, -16697 }, { 22613, -14933, 12872 }, { -28095, -10151, 2763 }, { -14360, -25399, 6977 }, { -2432, -10004, 28178 }, { -798, -1904, -16567 }, { 2784, 6478, 11080 }, { 17779, -12579, 17156 }, { 2586, -10252, -2028 }, { -5023, -16073, 5343 } },
    { { -16444, -23462, 8896 }, { 21064, -19587, 8523 }, { -13009, -23461, -13430 }, { 2474, -25879, -14972 }, { 8261, -14072, 25174 }, { -27109, -9833, -8272 }, { -18643, -21662, 9123 }, { -9886, -10790, 26189 }, { 2862, -1334, -17553 }, { -998, 4789, 8626 }, { 12994, -10883, 20871 }, { 11016, -13052, -10661 }, { 28533, -9719, -28533 } },
    { { -10303, -26804, 8683 }, { 16111, -19387, 16267 }, { -14956, -20881, -15502 }, { 2788, -28279, -9618 }, { -2189, -10560, 27995 }, { -22962, -12899, -14366 }, { -23570, -18057, 4288 }, { -16750, -10387, 22617 }, { 6181, -17, -17698 }, { -4039, 1195, 7800 }, { 9796, -8933, 23120 }, { 21676, -10545, -22456 }, { 30238, -2085, -31914 } },
    { { -4134, -27780, 10543 }, { 9245, -20906, 19428 }, { -14766, -18319, -18611 }, { 4568, -29650, -190 }, { -11895, -8426, 26220 }, { -17294, -11280, -21764 }, { -26162, -14140, -3954 }, { -21624, -9278, 18609 }, { 9769, 598, -16443 }, { -5878, -2000, 8104 }, { 6998, -6692, 24803 }, { 23462, -6276, -26357 }, { 32408, 1380, -31999 } },
    { { -3986, -28318, 9067 }, { 6644, -22487, 18714 }, { -10819, -19270, -20288 }, { 6703, -26476, 12413 }, { -17756, -9434, 22265 }, { -7718, -8360, -27759 }, { -24107, -10981, -14081 }, { -25637, -9130, 12625 }, { 12223, 1066, -16450 }, { -7469, -4373, 9418 }, { 3613, -3063, 26611 }, { 23056, -3313, -27468 }, { -29784, 3423, -29376 } },
    { { -7585, -28873, 2967 }, { 8753, -22382, 17956 }, { -2638, -23209, -18825 }, { 5146, -19635, 22090 }, { -21368, -14192, 15556 }, { 5218, -4621, -29179 }, { -18829, -8083, -21912 }, { -28187, -9758, 3205 }, { 14357, 1377, -16281 }, { -9771, -6398, 10959 }, { -160, 1910, 26772 }, { 23145, -2519, -27609 }, { -25444, 6194, -24970 } },
    { { -9256, -28471, -1936 }, { 9873, -22900, 16677 }, { 5815, -26846, -12062 }, { 2000, -14583, 26141 }, { -22376, -19325, 5087 }, { 18009, -400, -23990 }, { -6378, -5583, -28778 }, { -27353, -9605, -7718 }, { 16688, 2057, -15452 }, { -13370, -8253, 13605 }, { -5410, 6599, 24165 }, { 23146, -2520, -27609 }, { -21797, 6777, -21547 } },
    { { -8015, -27616, -8552 }, { 7023, -23696, 17005 }, { 11814, -27252, -4214 }, { -1424, -14446, 26254 }, { -15806, -23475, -9956 }, { 27053, 6818, -11030 }, { 13253, -3296, -26711 }, { -18653, -7279, -22340 }, { 19327, 2218, -14271 }, { -18351, -9176, 17683 }, { -13674, 8627, 17674 }, { 23146, -2520, -27609 }, { -19855, 6732, -19565 } },
    { { -5382, -26211, -13566 }, { 2293, -25086, 16293 }, { 16276, -25042, 2830 }, { -4024, -18014, 23650 }, { -6555, -21710, -19640 }, { 29109, 5902, -4225 }, { 23132, -2258, -18969 }, { -7592, -4175, -28722 }, { 22732, 1666, -13197 }, { -24065, -8928, 22346 }, { -22405, 6681, 11210 }, { 23146, -2520, -27609 }, { -16236, 7835, -14278 } },
    { { -4211, -25082, -15911 }, { -1589, -26369, 14218 }, { 18530, -22158, 8101 }, { -3220, -22615, 19447 }, { 104, -18441, -23662 }, { 29745, 877, -3804 }, { 27020, -2806, -12730 }, { 3931, -2695, -29619 }, { 26648, 674, -12612 }, { -29419, -7292, 26442 }, { -27959, 2047, 8810 }, { 23146, -2520, -27609 }, { -10740, 7309, -6568 } },
    { { -320, -25273, -16160 }, { -9972, -26221, 10630 }, { 17633, -20741, 12605 }, { 1204, -25745, 15355 }, { -138, -19507, -22791 }, { 28915, -1719, -7810 }, { 28797, -2433, -8052 }, { 16668, -3382, -24713 }, { 31116, -113, -11982 }, { 32469, -2870, 29045 }, { -32459, -3487, 8918 }, { 23448, -1727, -28742 }, { -8462, 3737, -593 } },
    { { 1675, -26669, -13638 }, { -19128, -23053, 1635 }, { 16723, -19851, 15043 }, { 4298, -27080, 12175 }, { -500, -24038, -17943 }, { 23636, -10281, -15351 }, { 29773, -1155, -3494 }, { 24288, -844, -17589 }, { -29110, -684, -11641 }, { 29934, 1412, 29566 }, { 27543, -8705, 10750 }, { 26008, 307, -31482 }, { -7919, -828, 2397 } },
    { { -2485, -27741, -11146 }, { -21182, -18595, -10274 }, { 14770, -19778, 17048 }, { 7259, -27166, 10455 }, { 2622, -24295, -17403 }, { 9230, -22224, -17914 }, { 27725, 1887, 11303 }, { 28510, 3257, -8750 }, { -24180, -1292, -11729 }, { 27130, 5110, 28548 }, { 19955, -10943, 15517 }, { 29000, 6659, 28525 }, { -8266, -5204, 4874 } },
    { { -5029, -26733, -12650 }, { -19791, -13948, -17714 }, { 13196, -19193, 18907 }, { 8376, -27645, 8100 }, { 2734, -24571, -16994 }, { 3229, -23727, -18072 }, { 18126, -234, 23904 }, { 28237, 4615, 9022 }, { -20666, -1903, -12338 }, { 22867, 7804, 25604 }, { 15811, -11541, 17977 }, { 26271, 13375, 18441 }, { -9390, -8000, 7533 } },
    { { -4502, -26570, -13182 }, { -17761, -6857, -23185 }, { 10688, -18977, 20631 }, { 9516, -28322, 2700 }, { 4544, -24261, -17052 }, { 4606, -24216, -17099 }, { 9245, -2408, 28438 }, { 14355, -1005, 26323 }, { -18380, -2175, -13183 }, { 19573, 8858, 22899 }, { 13133, -12548, 20021 }, { 22790, 12170, 15414 }, { -7543, -10236, 6654 } },
    { { -3158, -26716, -13277 }, { -14867, -810, -26044 }, { 8385, -19817, 20904 }, { 10802, -27814, -3115 }, { 4482, -19807, -22082 }, { 11264, -26313, -8985 }, { -10320, -6880, 27316 }, { -4090, -6350, 29034 }, { -16786, -2298, -14450 }, { 17167, 8441, 20577 }, { 8667, -12567, 24643 }, { 16870, 8961, 10265 }, { -2900, -11489, 2508 } },
    { { -485, -27539, -11890 }, { -11678, 2669, -27505 }, { 9355, -20578, 19724 }, { 7668, -26886, -10878 }, { 8836, -16743, -23272 }, { 13071, -26537, -4996 }, { -25636, -8675, 12944 }, { -22679, -12367, 15255 }, { -16171, -2236, -15417 }, { 16386, 7426, 19293 }, { 7349, -12520, 25916 }, { 12367, 5747, 6440 }, { -2891, -11489, 2500 } },
    { { 1726, -29041, -7324 }, { -8686, 2041, -28642 }, { 9541, -22878, 16898 }, { 4409, -24875, -16180 }, { 12186, -16736, -21712 }, { 14114, -25831, -5793 }, { -27532, -4830, -10892 }, { -27641, -11551, -1603 }, { -15770, -1803, -15486 }, { 15294, 6832, 18283 }, { 6913, -12796, 25781 }, { 8944, 6999, 4708 }, { -2813, -11434, 2434 } },
    { { 2954, -29843, -818 }, { -6030, -1129, -29366 }, { 7687, -25144, 14447 }, { 3272, -22881, -19125 }, { 15155, -15363, -20840 }, { 13738, -26192, -5022 }, { -19935, -1604, -22361 }, { -24706, -9245, -14287 }, { -14350, -1352, -15459 }, { 13514, 6211, 16794 }, { 5784, -13033, 26081 }, { 6103, 6867, 2981 }, { -2829, -11491, 2449 } },
    { { 3220, -28945, 7199 }, { -3201, -7879, -28769 }, { 2744, -27123, 12523 }, { 5788, -21116, -20509 }, { 20358, -13968, -17043 }, { 13998, -26326, -3315 }, { -11145, -487, -27849 }, { -19955, -8184, -20853 }, { -11837, -1064, -15278 }, { 11372, 5863, 15440 }, { 5465, -13036, 26223 }, { 2910, 7833, 1694 }, { -3838, -12787, 3755 } },
    { { 2575, -25481, 15624 }, { -573, -16789, -24856 }, { -3224, -28082, 10051 }, { 9168, -20158, -20239 }, { 24017, -12499, -12921 }, { 14025, -26477, -1514 }, { -4377, -396, -29676 }, { -14360, -7592, -25222 }, { -9300, -921, -15039 }, { 9216, 5471, 14233 }, { 5556, -13275, 26122 }, { -1479, 8016, -489 }, { -6692, -14628, 7140 } },
    { { -370, -21159, 21264 }, { 2734, -24612, -16934 }, { -8271, -28025, 6795 }, { 11192, -20015, -19343 }, { 26544, -11198, -8368 }, { 14406, -26116, 3228 }, { 887, -837, -29975 }, { -11048, -11038, -25615 }, { -6927, -802, -14763 }, { 7007, 5220, 13141 }, { 5736, -13414, 26021 }, { -6491, 6633, -4070 }, { -24444, -15310, 25365 } },
    { { -5403, -18851, 22703 }, { 5626, -28217, -8493 }, { -12269, -27243, 2694 }, { 12801, -20282, -18022 }, { 27667, -10813, -4200 }, { 13983, -24742, 9608 }, { 2767, -2653, -29754 }, { -12732, -15801, -22096 }, { -4732, -875, -14670 }, { 4813, 4796, 12050 }, { 5689, -13395, 26274 }, { -8322, 2498, -4537 }, { -30663, -13046, 31884 } },
    { { -9296, -17981, 22142 }, { 7485, -29047, 514 }, { -15268, -25746, -2004 }, { 14736, -21263, -15190 }, { 27996, -10777, 325 }, { 13620, -22145, 14970 }, { 2311, -6221, -29257 }, { -16753, -18306, -16859 }, { -2541, -1076, -15064 }, { 2617, 3929, 11100 }, { 5209, -13062, 27137 }, { -10267, -1803, -3658 }, { -31900, -9382, 32739 } },
    { { -9776, -17805, 22077 }, { 6599, -28642, 6006 }, { -17408, -23655, -6115 }, { 16362, -22443, -11339 }, { 27149, -11419, 5703 }, { 11947, -18708, 20181 }, { -29, -11078, -27880 }, { -21115, -17416, -12282 }, { -475, -1184, -15540 }, { 694, 2890, 10236 }, { 4831, -12595, 27680 }, { -11447, -4293, -3569 }, { -32587, -5239, 32524 } },
    { { -8833, -17339, 22833 }, { 5347, -28596, 7326 }, { -18456, -21546, -9753 }, { 16491, -23556, -8554 }, { 25606, -11826, 10221 }, { 5146, -13459, 26313 }, { -5116, -14966, -25492 }, { -23517, -15750, -9944 }, { 1382, -1095, -15923 }, { -676, 1835, 9626 }, { 5025, -12076, 27463 }, { -9120, -4231, -5855 }, { 32579, -3183, 31830 } },
    { { -8331, -16663, 23514 }, { 4031, -29093, 6109 }, { -18574, -20362, -11848 }, { 15773, -24356, -7616 }, { 23494, -11976, 14303 }, { -4324, -7142, 28815 }, { -10523, -17799, -21736 }, { -23975, -15637, -8982 }, { 2392, -878, -16165 }, { -1137, 611, 9392 }, { 4900, -11724, 27482 }, { -6893, -3578, -5475 }, { 32480, -2923, 31635 } },
    { { -8628, -16900, 23237 }, { 3658, -29307, 5263 }, { -17997, -20469, -12535 }, { 14822, -24813, -8037 }, { 23605, -12365, 13781 }, { -14218, -2601, 26289 }, { -12622, -19918, -18546 }, { -25613, -14995, -4372 }, { 2206, -624, -16363 }, { -1158, -260, 9368 }, { 4952, -11502, 27320 }, { -6365, -3524, -5323 }, { 32480, -2923, 31635 } },
    { { -8881, -17345, 22810 }, { 3410, -29444, 4630 }, { -16820, -20964, -13326 }, { 13793, -25273, -8429 }, { 22133, -14441, 14198 }, { -23560, -786, 18555 }, { -13071, -20447, -17638 }, { -27418, -12024, 1909 }, { 1596, -338, -16640 }, { -939, -625, 9205 }, { 5554, -11326, 26467 }, { -6365, -3525, -5323 }, { 32480, -2922, 31635 } },
    { { -8722, -18280, 22130 }, { 3081, -29492, 4555 }, { -15439, -22000, -13327 }, { 12887, -25424, -9357 }, { 19120, -16693, 15993 }, { -28403, 1099, 9594 }, { -13684, -19663, -18058 }, { -27942, -7235, 8179 }, { 966, -186, -16728 }, { -426, -366, 8828 }, { 6977, -11131, 24769 }, { -6365, -3524, -5323 }, { 32479, -2924, 31634 } },
    { { -8637, -19227, 21348 }, { 3097, -29461, 4738 }, { -14199, -22786, -13386 }, { 12111, -25554, -10015 }, { 15397, -18478, 17930 }, { -29694, 2526, 3448 }, { -12071, -19296, -19544 }, { -27106, -3752, 12295 }, { 609, -185, -16687 }, { 48, 28, 8552 }, { 8093, -10860, 23733 }, { -6365, -3524, -5323 }, { 32480, -2925, 31635 } },
    { { -8493, -19599, 21065 }, { 3344, -29471, 4504 }, { -13367, -23269, -13412 }, { 11744, -25519, -10530 }, { 12214, -19596, 19152 }, { -29854, 2930, 409 }, { -9046, -19619, -20815 }, { -26429, -3859, 13661 }, { 538, -179, -16842 }, { 80, 304, 8351 }, { 7911, -10570, 24108 }, { -6365, -3524, -5323 }, { 32480, -2923, 31635 } },
    { { -8241, -19767, 21008 }, { 3551, -29503, 4118 }, { -12719, -23774, -13154 }, { 11553, -25307, -11228 }, { 10146, -21017, 18850 }, { -29887, 2593, 137 }, { -5547, -20120, -21551 }, { -26089, -4860, 13991 }, { 541, -162, -17050 }, { -34, 559, 8164 }, { 7374, -10291, 24816 }, { -6365, -3524, -5323 }, { 32480, -2923, 31635 } },
    { { -7919, -19776, 21123 }, { 3669, -29540, 3731 }, { -12302, -24210, -12749 }, { 11574, -24987, -11903 }, { 8504, -22280, 18201 }, { -29946, 1722, 519 }, { -1674, -20395, -21937 }, { -26287, -6491, 12918 }, { 543, -161, -17235 }, { -162, 874, 8008 }, { 6691, -10115, 25523 }, { -6365, -3524, -5323 }, { 32482, -2923, 31634 } },
    { { -7589, -19693, 21321 }, { 3751, -29573, 3369 }, { -12025, -24372, -12704 }, { 11706, -24824, -12112 }, { 7113, -23169, 17680 }, { -29985, 385, 859 }, { 1317, -21126, -21259 }, { -27117, -8791, 9349 }, { 546, -169, -17394 }, { -301, 1226, 7882 }, { 6123, -10051, 26194 }, { -6365, -3524, -5323 }, { 32481, -2923, 31635 } },
    { { -7298, -19612, 21497 }, { 3877, -29593, 3041 }, { -11740, -24329, -13048 }, { 11836, -24855, -11921 }, { 5730, -23795, 17349 }, { -29959, -1228, 977 }, { 3101, -22240, -19894 }, { -27496, -11099, 4560 }, { 548, -178, -17557 }, { -438, 1584, 7750 }, { 5777, -10097, 26781 }, { -6365, -3524, -5323 }, { 32480, -2922, 31635 } },
    { { -7152, -19568, 21586 }, { 4191, -29574, 2793 }, { -11396, -24252, -13490 }, { 11949, -24934, -11640 }, { 4209, -24250, 17153 }, { -29845, -2913, 891 }, { 2801, -23235, -18770 }, { -26902, -13273, -332 }, { 549, -185, -17754 }, { -569, 1942, 7578 }, { 5356, -10180, 27374 }, { -6365, -3524, -5323 }, { 32480, -2923, 31635 } },
};

static const Vec3s sGdfClipRun[21][13] = {
    { { 1658, -23256, 18879 }, { 1515, -7298, -29059 }, { -2385, -29903, 307 }, { -498, -21680, -20730 }, { 9985, -19833, -20173 }, { 9056, -27990, 5878 }, { -12330, -26709, -5882 }, { 5100, -12483, 26799 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { 637, -19647, 22663 }, { 1865, -12662, -27133 }, { -1812, -28479, -9256 }, { -1376, -22943, -19280 }, { 6125, -14575, -25496 }, { 10223, -28190, -896 }, { -13362, -26809, -1656 }, { 5762, -12492, 26660 }, { -9927, 386, -15676 }, { 14103, 3135, 16846 }, { 16871, -14595, 16405 }, { -996, 2842, -174 }, { 2848, -10404, -2246 } },
    { { 468, -15602, 25619 }, { 1637, -18296, -23718 }, { -816, -25085, -16434 }, { -2318, -24662, -16924 }, { 2513, -9253, -28427 }, { 9648, -27865, -5520 }, { -14730, -26028, 2360 }, { 6706, -13814, 25772 }, { -10024, 523, -15506 }, { 15461, 3381, 18049 }, { 17131, -14878, 16243 }, { -2289, 3642, -701 }, { 9883, -9224, -7561 } },
    { { 567, -13699, 26684 }, { 1277, -24577, -17156 }, { -926, -23229, -18962 }, { -2257, -22781, -19389 }, { 634, -5670, -29452 }, { 8391, -27778, -7615 }, { -15808, -24989, 5066 }, { 7033, -15473, 24721 }, { -10369, 794, -15684 }, { 16229, 3486, 18726 }, { 16695, -15066, 16666 }, { -6044, 3553, -2406 }, { 13602, -8344, -10554 } },
    { { 312, -15826, 25484 }, { 1246, -29640, -4462 }, { -2625, -25248, -15990 }, { -427, -15972, -25391 }, { 1000, -5439, -29486 }, { 6760, -28430, -6787 }, { -16628, -24381, 5392 }, { 6651, -16580, 24101 }, { -10749, 856, -15911 }, { 16481, 3528, 18880 }, { 15670, -15107, 17648 }, { -12627, 1291, -4590 }, { 16761, -6190, -12355 } },
    { { 264, -22885, 19397 }, { 903, -28840, 8213 }, { -2510, -26581, -13681 }, { -210, -6456, -29296 }, { 3261, -8797, -28495 }, { 4740, -29482, -2892 }, { -17305, -24181, 3978 }, { 5720, -16699, 24258 }, { -11119, 578, -16031 }, { 16316, 3577, 18560 }, { 15404, -15012, 17813 }, { -17753, -1368, -5137 }, { 18951, -2921, -12273 } },
    { { 19, -25113, 16412 }, { 714, -29609, 4778 }, { -3630, -27796, -10688 }, { 897, 3193, -29816 }, { 6547, -14713, -25311 }, { 2098, -29787, 2889 }, { -17684, -24133, 2199 }, { 4384, -16134, 24909 }, { -11471, 129, -15999 }, { 15756, 3621, 17798 }, { 16180, -14802, 16880 }, { -17455, -1211, -5619 }, { 20129, 2058, -9854 } },
    { { 640, -24543, 17240 }, { -300, -29825, -3218 }, { -4610, -28969, -6288 }, { 2168, 11237, -27731 }, { 9521, -21335, -18820 }, { -504, -28625, 8965 }, { -17965, -24017, 682 }, { 2910, -15715, 25388 }, { -11851, -213, -15831 }, { 14895, 3583, 16693 }, { 16707, -14472, 16192 }, { -15669, -175, -5890 }, { 20801, 4676, -7882 } },
    { { 518, -25465, 15852 }, { -496, -28237, -10120 }, { -6759, -29205, 1176 }, { 3717, 13965, -26290 }, { 10364, -26468, -9593 }, { -2690, -25816, 15043 }, { -18321, -23746, -677 }, { 1366, -15873, 25420 }, { -12357, -240, -15717 }, { 13895, 3467, 15451 }, { 16266, -14107, 16505 }, { -15669, -175, -5890 }, { 21241, 4087, -7698 } },
    { { 926, -27485, 11989 }, { -941, -26146, -14680 }, { -6933, -27310, 10302 }, { 3202, 9964, -28115 }, { 8526, -28760, -424 }, { -5320, -21201, 20548 }, { -18470, -23551, -2050 }, { -512, -16472, 25068 }, { -12954, 77, -15881 }, { 12815, 3346, 14240 }, { 15335, -13845, 17336 }, { -15669, -175, -5890 }, { 19172, 2240, -9009 } },
    { { 1748, -29496, 5191 }, { -1510, -24900, -16664 }, { -6333, -22556, 18739 }, { 2108, -1346, -29896 }, { 6138, -28701, 6209 }, { -8432, -15220, 24439 }, { -18091, -23678, -3476 }, { -2839, -17290, 24352 }, { -13551, 584, -16377 }, { 11596, 3205, 13113 }, { 14547, -13781, 18017 }, { -15669, -175, -5890 }, { 15729, -1689, -9384 } },
    { { 1566, -29843, -2640 }, { -1184, -24650, -17058 }, { -4869, -18824, 22846 }, { 292, -13780, -26646 }, { 5511, -27545, 10531 }, { -11269, -8548, 26456 }, { -17258, -24044, -4902 }, { -5254, -18141, 23308 }, { -13963, 1095, -16972 }, { 10242, 2993, 12078 }, { 13973, -13907, 18473 }, { -14372, 820, -5883 }, { 11291, -5239, -8218 } },
    { { 1011, -28652, -8833 }, { -687, -24717, -16989 }, { -3787, -17734, 23899 }, { -703, -22776, -19513 }, { 6561, -25915, 13614 }, { -13601, -2494, 26623 }, { -16484, -24214, -6476 }, { -7106, -18891, 22196 }, { -14016, 1520, -17337 }, { 8997, 2714, 11197 }, { 13331, -14096, 19066 }, { -10631, 3086, -5200 }, { 7165, -7857, -6251 } },
    { { 858, -27820, -11195 }, { -1428, -20622, -21742 }, { -3293, -18303, 23540 }, { -1178, -27392, -12179 }, { 7962, -24459, 15438 }, { -15347, 1025, 25757 }, { -16172, -23836, -8385 }, { -7901, -19475, 21408 }, { -13707, 1747, -17300 }, { 8199, 2438, 10582 }, { 12807, -14229, 19621 }, { -8199, 4959, -4283 }, { 3547, -9556, -3478 } },
    { { 312, -26652, -13768 }, { -1892, -14413, -26243 }, { -3061, -19774, 22352 }, { -1195, -29665, -4306 }, { 9034, -23927, 15680 }, { -16451, 1022, 25067 }, { -16257, -23180, -9919 }, { -7565, -19842, 21191 }, { -13212, 1609, -17036 }, { 7918, 2281, 10289 }, { 13102, -14308, 19353 }, { -5113, 6380, -2446 }, { -378, -10544, -238 } },
    { { 352, -26843, -13391 }, { -2350, -4460, -29573 }, { -2778, -22279, 19898 }, { -1367, -29800, 3174 }, { 10056, -24369, 14318 }, { -16785, -2301, 24758 }, { -16383, -23074, -9959 }, { -6394, -19900, 21520 }, { -12746, 1108, -16749 }, { 7971, 2344, 10294 }, { 14399, -14309, 18015 }, { -799, 7149, -820 }, { -4478, -10504, 3536 } },
    { { 1157, -27668, -11538 }, { -2691, 6530, -29157 }, { -2117, -25698, 15335 }, { -1777, -28483, 9249 }, { 11437, -25348, 11256 }, { -15752, -8077, 24220 }, { -16029, -23893, -8497 }, { -4597, -19517, 22315 }, { -12446, 478, -16541 }, { 8194, 2605, 10596 }, { 15620, -14172, 16711 }, { 5568, 7145, 2423 }, { -6530, -9922, 5507 } },
    { { 2173, -28832, -8001 }, { -2331, 14970, -25894 }, { -2325, -24614, 16993 }, { -1043, -29978, -487 }, { 12777, -26361, 6469 }, { -12853, -14665, 22797 }, { -14944, -25108, -6801 }, { -2156, -18523, 23500 }, { -12293, 27, -16480 }, { 8635, 2917, 11276 }, { 15730, -14016, 16566 }, { 11432, 5390, 4820 }, { -2729, -10789, 2348 } },
    { { 3557, -29696, -2340 }, { -1552, 18026, -23930 }, { -1818, -24727, 16890 }, { -966, -28115, -10423 }, { 13363, -26856, 424 }, { -8395, -20478, 20253 }, { -13540, -26096, -5973 }, { 594, -16972, 24730 }, { -12126, -90, -16504 }, { 9423, 3115, 12347 }, { 15164, -13986, 17223 }, { 13037, 4436, 3871 }, { -2737, -10788, 2355 } },
    { { 4258, -29105, 5896 }, { -638, 14829, -26071 }, { -1581, -26791, 13406 }, { -790, -25003, -16560 }, { 13087, -26255, -6279 }, { -2961, -24782, 16645 }, { -12535, -26623, -5839 }, { 2950, -15288, 25643 }, { -11732, 44, -16495 }, { 10480, 3145, 13570 }, { 15012, -14055, 17559 }, { 10234, 4141, 1755 }, { -2737, -10789, 2355 } },
    { { 2971, -26679, 13394 }, { 1407, 5468, -29464 }, { -2199, -29068, 7088 }, { -52, -22738, -19570 }, { 12096, -24016, -13301 }, { 3103, -27331, 11975 }, { -12217, -26778, -5802 }, { 4477, -13782, 26268 }, { -11031, 270, -16317 }, { 11538, 3020, 14608 }, { 15723, -14156, 17020 }, { 5695, 3094, 440 }, { -2736, -10789, 2354 } },
};

// Its stab (the combo's third hit, and the thrust out of a dash), up to where the thrust is held
static const Vec3s sGdfClipStab[32][13] = {
    { { -1824, -24192, 17647 }, { 10373, -27912, -3653 }, { -12797, -27118, -930 }, { 7715, -25031, -14626 }, { 12883, -20733, 17441 }, { -23395, 4836, 18146 }, { -7900, -14485, -25055 }, { -4032, -17570, 23980 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { -1919, -24008, 17886 }, { 10480, -27883, -3568 }, { -13202, -26931, -645 }, { 8161, -24982, -14467 }, { 12210, -19909, 18829 }, { -23507, 5588, 17782 }, { -7660, -14619, -25052 }, { -5139, -17802, 23594 }, { -9853, 531, -15996 }, { 12071, 2540, 15178 }, { 16071, -14477, 16650 }, { 147, 2145, -95 }, { 422, -10597, -14 } },
    { { -2314, -23709, 18235 }, { 10678, -27797, -3648 }, { -13712, -26679, -448 }, { 8683, -24881, -14337 }, { 11671, -18652, 20394 }, { -23518, 5873, 17675 }, { -7406, -14793, -25026 }, { -6340, -18054, 23105 }, { -9686, 548, -16024 }, { 11761, 2348, 15043 }, { 15335, -14607, 17293 }, { 160, 2300, -127 }, { 422, -10578, -14 } },
    { { -2949, -23176, 18819 }, { 10862, -27682, -3963 }, { -14308, -26367, -247 }, { 9328, -24626, -14372 }, { 11381, -16623, 22229 }, { -22846, 4865, 18825 }, { -7222, -15008, -24952 }, { -7751, -18351, 22431 }, { -9767, 608, -16077 }, { 11428, 2181, 14917 }, { 14608, -14781, 17868 }, { 159, 2438, -127 }, { 422, -10561, -14 } },
    { { -3497, -22272, 19792 }, { 10845, -27580, -4659 }, { -14898, -26039, 87 }, { 10109, -24132, -14678 }, { 11709, -13781, 23937 }, { -20434, 2447, 21828 }, { -6865, -15028, -25041 }, { -9130, -18742, 21573 }, { -10090, 765, -16188 }, { 11075, 2042, 14822 }, { 14150, -14974, 18043 }, { 157, 2585, -128 }, { 422, -10556, -14 } },
    { { -3535, -21069, 21062 }, { 10500, -27519, -5699 }, { -15364, -25763, 469 }, { 10930, -23443, -15197 }, { 13904, -10508, 24418 }, { -16080, 124, 25327 }, { -6211, -14664, -25424 }, { -10516, -19185, 20527 }, { -10482, 1029, -16351 }, { 10784, 1861, 14718 }, { 14160, -15163, 17577 }, { 154, 2776, -128 }, { 422, -10562, -14 } },
    { { -2947, -20638, 21573 }, { 10073, -27557, -6257 }, { -15090, -25925, 430 }, { 11034, -23110, -15626 }, { 18886, -9632, 21226 }, { -14613, -1370, 26164 }, { -4777, -14423, -25868 }, { -10486, -18692, 20992 }, { -10822, 1277, -16430 }, { 11128, 1845, 14628 }, { 15814, -15174, 15604 }, { 154, 2823, -128 }, { 422, -10569, -14 } },
    { { -2358, -20756, 21532 }, { 9671, -27700, -6260 }, { -14127, -26465, 103 }, { 10685, -23028, -15986 }, { 24055, -10136, 14785 }, { -15308, -2882, 25639 }, { -2729, -14715, -26001 }, { -9881, -16715, 22869 }, { -11489, 1546, -16515 }, { 12187, 2025, 14459 }, { 18777, -15024, 12456 }, { 153, 2855, -128 }, { 422, -10589, -14 } },
    { { -1846, -21448, 20894 }, { 9280, -27991, -5513 }, { -12157, -27422, -483 }, { 9703, -23216, -16337 }, { 26283, -13016, 6309 }, { -16562, -4275, 24646 }, { -1716, -15651, -25536 }, { -7364, -13577, 25718 }, { -12792, 1838, -16669 }, { 14133, 2381, 14406 }, { 21505, -14613, 9943 }, { 153, 2868, -128 }, { 422, -10653, -14 } },
    { { -1302, -22507, 19792 }, { 8861, -28355, -4177 }, { -9406, -28450, -1454 }, { 8040, -23641, -16627 }, { 24354, -17367, -2289 }, { -16765, -5167, 24336 }, { -5033, -16827, -24321 }, { -1199, -10507, 28074 }, { -14568, 2106, -16848 }, { 16898, 2529, 14678 }, { 23421, -14131, 8773 }, { 154, 2841, -128 }, { 422, -10590, -14 } },
    { { -648, -23482, 18659 }, { 8346, -28668, -2918 }, { -6706, -29109, -2778 }, { 5973, -24058, -16898 }, { 20164, -20299, -9020 }, { -14499, -5833, 25608 }, { -15000, -17561, -19148 }, { 8524, -8084, 27604 }, { -16328, 2334, -16975 }, { 20211, 2153, 15277 }, { 25639, -13779, 7569 }, { 154, 2773, -128 }, { 796, -9988, -428 } },
    { { 93, -23858, 18187 }, { 7625, -28907, -2498 }, { -4768, -29363, -3884 }, { 3973, -24030, -17514 }, { 16502, -21158, -13416 }, { -9873, -6813, 27497 }, { -25266, -15413, -4905 }, { 16049, -7177, 24309 }, { -17660, 2548, -17116 }, { 23624, 1239, 16154 }, { 28762, -13507, 5219 }, { 155, 2728, -128 }, { 1812, -8588, -1674 } },
    { { 952, -23540, 18573 }, { 6656, -29093, -3053 }, { -4019, -29334, -4833 }, { 2036, -23545, -18480 }, { 12880, -20828, -17329 }, { -4007, -7637, 28734 }, { -25232, -11707, 11237 }, { 17173, -6808, 23638 }, { -18426, 2745, -17288 }, { 26478, 65, 16941 }, { 31687, -13118, 2417 }, { 154, 2773, -128 }, { 2069, -6817, -1585 } },
    { { 1892, -22846, 19351 }, { 5609, -29187, -4082 }, { -4164, -29160, -5688 }, { 780, -22955, -19299 }, { 8635, -19123, -21442 }, { 1892, -8790, 28621 }, { -17876, -8837, 22413 }, { 13414, -7256, 25834 }, { -18831, 2916, -17453 }, { 28476, -843, 17390 }, { -32700, -12878, 974 }, { 153, 2883, -128 }, { 2074, -5349, -1144 } },
    { { 2698, -22223, 19971 }, { 4761, -29180, -5084 }, { -4563, -28894, -6658 }, { 252, -22512, -19828 }, { 4485, -15796, -25107 }, { 6836, -11224, 26968 }, { -8360, -7001, 27948 }, { 5943, -7487, 28436 }, { -19287, 3049, -17536 }, { 29897, -1457, 17590 }, { 32209, -13169, 1509 }, { 152, 2974, -129 }, { 1963, -4561, -701 } },
    { { 3075, -21869, 20305 }, { 4359, -29091, -5891 }, { -4760, -28637, -7569 }, { 51, -22051, -20341 }, { 378, -11391, -27751 }, { 11159, -14929, 23507 }, { -1663, -6255, 29294 }, { 2178, -6600, 29184 }, { -20067, 3150, -17565 }, { 31286, -2270, 17592 }, { 31314, -13490, 2816 }, { 151, 3019, -129 }, { 2003, -4318, -468 } },
    { { 2895, -21644, 20571 }, { 4538, -28961, -6379 }, { -4711, -28636, -7601 }, { -94, -21225, -21201 }, { -2938, -7943, -28780 }, { 13246, -17694, 20285 }, { 1041, -5838, 29408 }, { 1120, -5846, 29404 }, { -20985, 3086, -17671 }, { 31970, -2684, 17479 }, { 29148, -13944, 5419 }, { 151, 3030, -129 }, { 2473, -4265, -611 } },
    { { 2454, -21468, 20812 }, { 5017, -28804, -6722 }, { -4451, -28873, -6820 }, { -345, -20063, -22301 }, { -5442, -6512, -28775 }, { 14235, -18987, 18354 }, { 1829, -5829, 29371 }, { 1831, -5829, 29371 }, { -21711, 2903, -17774 }, { 32370, -2941, 17376 }, { 28608, -14080, 6272 }, { 151, 3026, -129 }, { 3347, -4172, -985 } },
    { { 2396, -21278, 21012 }, { 5122, -28711, -7032 }, { -4176, -29130, -5829 }, { -592, -18967, -23236 }, { -6527, -6530, -28544 }, { 14748, -19190, 17727 }, { 2291, -6098, 29284 }, { 2308, -6099, 29283 }, { -22100, 2776, -17785 }, { 32693, -3140, 17264 }, { 29893, -14006, 5125 }, { 151, 3014, -129 }, { 4258, -4056, -1334 } },
    { { 2416, -21124, 21164 }, { 5131, -28640, -7308 }, { -4118, -29275, -5103 }, { -605, -18214, -23830 }, { -6192, -7025, -28501 }, { 14731, -18944, 18003 }, { 2578, -6329, 29211 }, { 2588, -6329, 29210 }, { -22260, 2794, -17743 }, { -32699, -3178, 17122 }, { 30688, -13904, 4315 }, { 151, 3005, -129 }, { 4861, -4022, -1568 } },
    { { 2542, -20915, 21357 }, { 5015, -28578, -7625 }, { -4295, -29309, -4749 }, { -411, -17746, -24185 }, { -5300, -7731, -28498 }, { 14450, -18471, 18708 }, { 2875, -6439, 29159 }, { 2876, -6439, 29159 }, { -22306, 2882, -17719 }, { 32762, -3074, 16987 }, { 30483, -13901, 4435 }, { 151, 3012, -129 }, { 5092, -4061, -1670 } },
    { { 2651, -20607, 21641 }, { 4909, -28481, -8047 }, { -4516, -29301, -4590 }, { -274, -17348, -24474 }, { -4634, -8577, -28372 }, { 14252, -17931, 19374 }, { 3340, -6497, 29097 }, { 3350, -6498, 29096 }, { -22256, 2943, -17727 }, { 32572, -2953, 16876 }, { 29851, -13957, 4969 }, { 151, 3027, -129 }, { 5101, -4060, -1622 } },
    { { 2589, -20267, 21967 }, { 4960, -28326, -8547 }, { -4658, -29307, -4406 }, { -261, -16914, -24776 }, { -4213, -9432, -28165 }, { 14042, -17547, 19873 }, { 3954, -6571, 29003 }, { 3971, -6571, 29001 }, { -22078, 2960, -17730 }, { 32349, -2869, 16770 }, { 28990, -14071, 5738 }, { 151, 3025, -129 }, { 5056, -3995, -1550 } },
    { { 2356, -19973, 22260 }, { 5180, -28130, -9049 }, { -4749, -29339, -4085 }, { -147, -16488, -25062 }, { -3583, -10005, -28055 }, { 13583, -17457, 20267 }, { 4522, -6695, 28892 }, { 4523, -6695, 28891 }, { -21764, 2967, -17708 }, { 32050, -2810, 16633 }, { 27316, -14282, 7303 }, { 152, 2997, -129 }, { 5028, -3959, -1670 } },
    { { 2157, -19719, 22506 }, { 5413, -27937, -9499 }, { -4867, -29376, -3660 }, { 263, -16172, -25267 }, { -2263, -10051, -28176 }, { 12820, -17555, 20676 }, { 4827, -6844, 28807 }, { 4841, -6844, 28805 }, { -21363, 2986, -17710 }, { 31650, -2789, 16466 }, { 24228, -14471, 10260 }, { 152, 2968, -129 }, { 5011, -4009, -2036 } },
    { { 2186, -19450, 22736 }, { 5460, -27790, -9894 }, { -5105, -29377, -3309 }, { 917, -16071, -25316 }, { 35, -9771, -28364 }, { 11994, -17385, 21305 }, { 5026, -6955, 28747 }, { 5031, -6955, 28746 }, { -20922, 3021, -17782 }, { 31275, -2880, 16330 }, { 21169, -14405, 13214 }, { 152, 2969, -129 }, { 4914, -4104, -2447 } },
    { { 2383, -19232, 22901 }, { 5262, -27713, -10214 }, { -5561, -29295, -3295 }, { 1603, -16341, -25108 }, { 3468, -9981, -28078 }, { 11173, -16305, 22568 }, { 5637, -7087, 28601 }, { 5655, -7088, 28597 }, { -20405, 3045, -17872 }, { 31024, -3108, 16314 }, { 20388, -14134, 13945 }, { 151, 3008, -129 }, { 4539, -4202, -2623 } },
    { { 2369, -19286, 22857 }, { 5057, -27690, -10377 }, { -6214, -29105, -3777 }, { 2097, -17176, -24507 }, { 8164, -11953, -26277 }, { 9309, -13626, 25053 }, { 6942, -7290, 28261 }, { 6955, -7289, 28258 }, { -19673, 3028, -17855 }, { 30631, -3287, 16494 }, { 20954, -13713, 13267 }, { 151, 3067, -129 }, { 3771, -4290, -2412 } },
    { { 1839, -19761, 22497 }, { 5170, -27678, -10353 }, { -6990, -28810, -4598 }, { 2329, -18611, -23413 }, { 13199, -16295, -21453 }, { 4365, -8846, 28332 }, { 7654, -7472, 28028 }, { 8694, -7247, 27783 }, { -18506, 2953, -17668 }, { 29482, -3018, 16891 }, { 20271, -13247, 13569 }, { 150, 3097, -129 }, { 2779, -4386, -1893 } },
    { { 723, -20604, 21794 }, { 5881, -27564, -10278 }, { -7814, -28503, -5152 }, { 2386, -20237, -22018 }, { 15376, -20914, -15040 }, { -3425, -4191, 29508 }, { 4647, -8475, 28400 }, { 13208, -6443, 26154 }, { -16681, 2873, -17322 }, { 27558, -2330, 17233 }, { 18286, -13215, 14989 }, { 151, 3044, -129 }, { 1974, -4603, -1360 } },
    { { -736, -21723, 20677 }, { 7247, -27286, -10147 }, { -8787, -28222, -5131 }, { 2453, -21680, -20590 }, { 13827, -23909, -11711 }, { -11224, -2794, 27681 }, { -2068, -9597, 28348 }, { 17062, -7547, 23493 }, { -14280, 2733, -16927 }, { 25378, -1493, 17118 }, { 14707, -13889, 18202 }, { 153, 2909, -129 }, { 1671, -5075, -1008 } },
    { { -2187, -23086, 19032 }, { 9220, -26860, -9670 }, { -10029, -27856, -4843 }, { 2797, -22949, -19118 }, { 11862, -24556, -12502 }, { -18029, -5342, 23375 }, { -11801, -9672, 25830 }, { 14455, -11154, 23804 }, { -11582, 2380, -16661 }, { 22544, -555, 16280 }, { 3764, -13657, 28907 }, { 156, 2667, -128 }, { 1830, -5894, -749 } },
};


// Jumping off an edge: Mixamo's running jump, from the take-off to the landing (retargeted the same way)
static const Vec3s sGdfClipJump[21][13] = {
    { { -7754, -18489, 22317 }, { 5706, 4150, -29158 }, { -5307, -29308, 3587 }, { 9112, -17631, -22497 }, { 7783, -14040, -25344 }, { -4499, -27636, -10771 }, { 7723, -27072, 10367 }, { 15101, 1357, 25887 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { -12040, -1179, 27453 }, { 14529, -17153, -19866 }, { -4609, -28257, -8960 }, { 6716, -21494, -19822 }, { 1777, -17076, -24602 }, { -597, -26359, -14313 }, { 647, -23460, 18687 }, { 21537, 3782, 20539 }, { -7793, -720, -14453 }, { 14190, 3596, 15229 }, { 18665, -14270, 12233 }, { -4844, -217, 1013 }, { 4638, -10221, -3422 } },
    { { -14004, -442, 26527 }, { 19954, -21907, -4682 }, { -3117, -28024, -10245 }, { 5887, -22950, -18403 }, { -2018, -17197, -24498 }, { 842, -23927, -18078 }, { -2809, -15240, 25687 }, { 25764, -527, 15360 }, { -7166, -1802, -12693 }, { 16782, 3584, 15422 }, { 23598, -14458, 5765 }, { -12905, -3092, 4467 }, { 7301, -10496, -5471 } },
    { { -12289, -1777, 27310 }, { 19289, -22467, 4816 }, { -596, -28471, -9436 }, { 6240, -23131, -18056 }, { -843, -12938, -27054 }, { 8039, -20097, -20772 }, { -7438, -5595, 28520 }, { 20112, -4805, 21735 }, { -7645, -2161, -11434 }, { 18269, 2866, 15816 }, { 32253, -14802, -2421 }, { -21055, -2877, 8186 }, { 11147, -11682, -8227 } },
    { { -9904, -1074, 28298 }, { 15524, -23140, 11116 }, { 3237, -28317, -9364 }, { 7000, -22491, -18578 }, { 780, -8373, -28797 }, { 14802, -13496, -22333 }, { -12508, 1714, 27214 }, { 8198, -3461, 28650 }, { -8455, -2218, -11111 }, { 18563, 2261, 16172 }, { -26380, -14357, -7907 }, { -24976, -587, 8561 }, { 15541, -11911, -10281 } },
    { { -7508, 2730, 28917 }, { 9930, -23752, 15402 }, { 5285, -28015, -9338 }, { 9256, -19117, -21187 }, { 2833, -5958, -29266 }, { 18054, -8224, -22504 }, { -15317, 2387, 25685 }, { -1919, -1563, 29898 }, { -9272, -2284, -11600 }, { 18301, 1874, 16403 }, { -22673, -14221, -10558 }, { -25542, 1267, 6179 }, { 16268, -11288, -7770 } },
    { { -4218, 5215, 29241 }, { 4436, -23962, 17498 }, { 5465, -28537, -7468 }, { 11834, -13489, -24042 }, { 6170, -5328, -28871 }, { 20171, -5837, -21425 }, { -16761, -1258, 24849 }, { -8366, -3052, 28648 }, { -10056, -2508, -12139 }, { 18182, 1699, 16343 }, { -19862, -13890, -12986 }, { -25402, 1879, 3049 }, { 16526, -10751, -6327 } },
    { { -2122, 5248, 29461 }, { 1060, -23668, 18404 }, { 4477, -29346, -4335 }, { 13302, -7955, -25686 }, { 10240, -6032, -27546 }, { 22246, -5634, -19323 }, { -18811, -6304, 22504 }, { -11825, -7378, 26566 }, { -10568, -2521, -12398 }, { 18414, 1744, 16293 }, { -18189, -13446, -14646 }, { -25061, 1303, 1054 }, { 18858, -9990, -9335 } },
    { { -1775, 3818, 29703 }, { -448, -23202, 19012 }, { 1568, -29959, 166 }, { 14232, -3546, -26170 }, { 13805, -6718, -25774 }, { 24322, -6182, -16438 }, { -20661, -11103, 18705 }, { -14633, -12218, 23165 }, { -10627, -2464, -12509 }, { 18604, 1783, 16539 }, { -17138, -13149, -15569 }, { -24374, 808, 423 }, { 19750, -8717, -12057 } },
    { { -2350, 1683, 29860 }, { -562, -23406, 18757 }, { -2061, -29467, 5237 }, { 14278, -1409, -26347 }, { 16767, -6232, -24084 }, { 25883, -6465, -13722 }, { -21811, -15074, 14037 }, { -16593, -16581, 18701 }, { -10398, -2682, -12595 }, { 18391, 1564, 16873 }, { -16177, -12927, -16218 }, { -23592, 702, 800 }, { 18326, -7980, -11461 } },
    { { -2605, -825, 29875 }, { 81, -24012, 17984 }, { -4915, -27624, 10619 }, { 13120, -1569, -26933 }, { 19686, -4437, -22198 }, { 27306, -5755, -11011 }, { -22110, -18080, 9179 }, { -17668, -19916, 13827 }, { -10287, -3144, -12755 }, { 17722, 1214, 17092 }, { -15096, -12713, -16936 }, { -22607, 909, 1324 }, { 15681, -7898, -8921 } },
    { { -1852, -3646, 29720 }, { 401, -24749, 16951 }, { -6343, -24702, 15799 }, { 11045, -3823, -27629 }, { 22936, -1337, -19291 }, { 28731, -3628, -7835 }, { -21733, -20285, 4023 }, { -18007, -22390, 8627 }, { -10631, -3531, -12997 }, { 16807, 919, 17189 }, { -14126, -12477, -17584 }, { -22036, 1067, 1359 }, { 12689, -8299, -6821 } },
    { { -192, -6773, 29225 }, { 95, -25477, 15839 }, { -6529, -21243, 20152 }, { 8575, -7684, -27702 }, { 25951, 2905, -14768 }, { 29711, -210, -4145 }, { -21053, -21291, -1862 }, { -17624, -24052, 3297 }, { -11442, -3650, -13299 }, { 15906, 745, 17211 }, { -13598, -12325, -17901 }, { -21715, 1184, 831 }, { 9694, -8922, -5762 } },
    { { 1861, -10290, 28119 }, { -535, -26092, 14796 }, { -5995, -17767, 23418 }, { 5985, -12431, -26639 }, { 27789, 7364, -8576 }, { 29743, 3914, -229 }, { -20093, -20528, -8655 }, { -16636, -24892, -1912 }, { -12511, -3531, -13670 }, { 15164, 682, 17195 }, { -13461, -12341, -17957 }, { -21224, 1441, 23 }, { 6145, -9406, -4428 } },
    { { 3669, -13952, 26304 }, { -1209, -26655, 13714 }, { -5047, -14641, 25694 }, { 3266, -17454, -24180 }, { 28193, 10116, -1686 }, { 28858, 7127, 4055 }, { -17535, -18719, -15561 }, { -14887, -24988, -7347 }, { -13543, -3312, -14071 }, { 14576, 723, 17122 }, { -13576, -12431, -17956 }, { -20367, 1925, -860 }, { 1960, -9373, -2028 } },
    { { 4878, -17306, 24014 }, { -1991, -27535, 11740 }, { -3635, -12196, 27167 }, { 509, -22249, -20117 }, { 27750, 10702, 3927 }, { 27209, 6954, 10550 }, { -12212, -17629, -20979 }, { -11617, -24533, -12773 }, { -14308, -3076, -14417 }, { 13963, 854, 16938 }, { -13884, -12411, -17971 }, { -19115, 2724, -1651 }, { -2223, -8674, 769 } },
    { { 6115, -18752, 22605 }, { -3414, -28697, 8051 }, { -1953, -10289, 28113 }, { -1579, -26236, -14462 }, { 27500, 9428, 7409 }, { 21892, 2736, 20328 }, { -4789, -17724, -23726 }, { -6454, -23973, -16842 }, { -14743, -2889, -14710 }, { 13158, 1162, 16588 }, { -14223, -12388, -18071 }, { -17720, 3686, -2390 }, { -5781, -7706, 2881 } },
    { { 6172, -20649, 20869 }, { -4449, -29355, 4303 }, { -336, -8423, 28791 }, { -2346, -29030, -7195 }, { 27410, 5053, 11099 }, { 9718, -1303, 28352 }, { 3159, -17908, -23860 }, { -605, -23782, -18277 }, { -15076, -2810, -14792 }, { 12202, 1631, 15959 }, { -14138, -12733, -18492 }, { -16718, 4398, -3265 }, { -8980, -6918, 4581 } },
    { { 4061, -24937, 16176 }, { -3867, -29634, 2622 }, { 810, -6852, 29196 }, { -1563, -29934, 1234 }, { 25217, -729, 16235 }, { -5564, -1969, 29414 }, { 10153, -18350, -21452 }, { 3760, -24521, -16870 }, { -15537, -2930, -14531 }, { 11273, 2133, 14988 }, { -12994, -13718, -19587 }, { -16758, 4505, -4265 }, { -12002, -5749, 6660 } },
    { { 4086, -25610, 15080 }, { -7962, -28312, -5918 }, { 1265, -6247, 29315 }, { 329, -28593, 9072 }, { 19671, -5223, 22040 }, { -18435, 306, 23665 }, { 15070, -19183, -17461 }, { 4314, -26528, -13329 }, { -16029, -3170, -13964 }, { 10510, 2562, 13702 }, { -9860, -15044, -22466 }, { -20749, 3372, -649 }, { -14051, -3638, 8692 } },
    { { 3946, -26023, 14395 }, { -7840, -23982, -16230 }, { 1171, -6845, 29185 }, { 2368, -26843, 13186 }, { 12056, -8780, 26030 }, { -25806, 2904, 15021 }, { 18530, -19804, -12823 }, { 4249, -28652, -7811 }, { -16127, -3271, -13603 }, { 9911, 2902, 12890 }, { 728, -16032, -32734 }, { -19685, 3085, -723 }, { -14605, -2152, 10157 } },
};

// Ladders and vine walls: Mixamo's Climbing Ladder (one cycle, a hand and a foot each), and the end of its Climbing
// To Top, from the last rung over the top to standing (retargeted the same way)
static const Vec3s sGdfClipLadder[24][13] = {
    { { 3434, -18973, 22983 }, { -10578, -27983, 2254 }, { -13527, -11231, 24308 }, { 13018, -23731, -12937 }, { 13546, 6896, 25864 }, { -14125, 18064, 19343 }, { -9928, -17178, 22503 }, { 4570, 20677, 21250 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
    { { 3761, -19251, 22700 }, { -9827, -28303, 1543 }, { -16472, -7411, 23953 }, { 14439, -24033, -10675 }, { 13008, 4106, 26720 }, { -11512, 19650, 19528 }, { -10049, -16161, 23192 }, { 5536, 20185, 21492 }, { -9871, 267, -15838 }, { 12045, 2897, 15358 }, { 17671, -14285, 15155 }, { -493, 1194, -270 }, { -4354, -10009, 4770 } },
    { { 3421, -20283, 21838 }, { -8593, -28704, 1499 }, { -18588, -4723, 23069 }, { 14931, -24927, -7465 }, { 12434, 840, 27289 }, { -9289, 20663, 19666 }, { -10366, -16049, 23130 }, { 7469, 19749, 21311 }, { -9633, -20, -15736 }, { 11638, 3133, 15298 }, { 18267, -14281, 14460 }, { -1082, 530, -392 }, { -7783, -8617, 7835 } },
    { { 2414, -22036, 20213 }, { -7087, -29029, 2664 }, { -19641, -4352, 22255 }, { 14677, -25829, -4177 }, { 11925, -2884, 27377 }, { -8030, 20969, 19895 }, { -10816, -16931, 22279 }, { 9403, 19877, 20408 }, { -9251, -197, -15672 }, { 11127, 3304, 15096 }, { 19172, -14296, 13509 }, { -1004, 197, -292 }, { -9609, -7717, 9144 } },
    { { 168, -22789, 19509 }, { -4108, -29514, 3473 }, { -19477, -5909, 22040 }, { 13996, -26478, -1734 }, { 11303, -7108, 26865 }, { -7047, 21192, 20030 }, { -11436, -17570, 21459 }, { 11094, 21097, 18216 }, { -8825, -204, -15612 }, { 10522, 3337, 14691 }, { 20405, -14382, 12445 }, { 187, -91, -199 }, { -10033, -7892, 9062 } },
    { { -554, -22670, 19640 }, { -1642, -29691, 3970 }, { -17479, -8205, 22960 }, { 12476, -27279, -467 }, { 10427, -11355, 25736 }, { -5558, 21736, 19916 }, { -11626, -15317, 23026 }, { 12647, 23959, 12885 }, { -8674, -18, -15578 }, { 10015, 3170, 14130 }, { 21633, -14494, 11557 }, { 2146, -804, -251 }, { -9178, -8365, 7584 } },
    { { 1822, -22795, 19418 }, { -1568, -29578, 4766 }, { -14479, -10884, 23915 }, { 10506, -28100, 121 }, { 9494, -14932, 24226 }, { -3787, 21926, 20122 }, { -11632, -10673, 25510 }, { 14208, 25460, 7067 }, { -9080, 302, -15626 }, { 9964, 2849, 13624 }, { 22661, -14515, 10840 }, { 4004, -1868, -313 }, { -8025, -8268, 5402 } },
    { { 4761, -21479, 20396 }, { -2362, -29728, 3266 }, { -11623, -12836, 24498 }, { 8742, -28697, 188 }, { 8890, -16633, 23330 }, { -2444, 21856, 20405 }, { -12713, -7457, 26130 }, { 15744, 24856, 5858 }, { -9960, 525, -15839 }, { 10511, 2558, 13288 }, { 23197, -14418, 10488 }, { 5156, -2474, -316 }, { -7398, -7867, 3809 } },
    { { 7645, -18577, 22281 }, { -4324, -29681, -569 }, { -9895, -13807, 24728 }, { 7791, -28970, -219 }, { 8816, -16266, 23616 }, { -1984, 21656, 20666 }, { -14055, -3390, 26286 }, { 16807, 23830, 7048 }, { -10881, 434, -16115 }, { 11337, 2494, 13075 }, { 23187, -14304, 10639 }, { 5649, -1812, -161 }, { -7353, -7625, 3273 } },
    { { 10595, -15855, 23160 }, { -7515, -28782, -3888 }, { -8789, -14669, 24649 }, { 7316, -29089, -569 }, { 8956, -15095, 24330 }, { -2034, 21020, 21308 }, { -15257, 2156, 25741 }, { 18629, 21684, 9097 }, { -11497, 141, -16330 }, { 11963, 2604, 12811 }, { 22871, -14209, 11097 }, { 5949, -61, 177 }, { -7744, -7507, 3367 } },
    { { 13799, -12938, 23285 }, { -11402, -27075, -6079 }, { -7815, -15580, 24417 }, { 7051, -29147, -873 }, { 8892, -14377, 24784 }, { -1867, 20574, 21754 }, { -15267, 7507, 24710 }, { 20083, 18600, 12277 }, { -11850, -59, -16487 }, { 12143, 2748, 12378 }, { 22374, -14083, 11663 }, { 6095, 1729, 585 }, { -8334, -7334, 3521 } },
    { { 16736, -10107, 22754 }, { -14944, -25107, -6804 }, { -6344, -16191, 24446 }, { 6365, -29280, -1472 }, { 8691, -14701, 24665 }, { -1143, 20854, 21536 }, { -14020, 9900, 24606 }, { 19117, 17322, 15313 }, { -12183, -29, -16515 }, { 12003, 2828, 11867 }, { 21642, -13949, 12316 }, { 5586, 2705, 664 }, { -8829, -7135, 3460 } },
    { { 18957, -7327, 22067 }, { -17644, -23436, -6280 }, { -5607, -17015, 24064 }, { 6217, -29304, -1615 }, { 8899, -14798, 24532 }, { -1545, 20772, 21590 }, { -13836, 7808, 25448 }, { 16955, 17969, 17018 }, { -12567, 95, -16488 }, { 11891, 2879, 11478 }, { 20780, -13853, 13027 }, { 4150, 2914, 173 }, { -9174, -6948, 3266 } },
    { { 20157, -5720, 21470 }, { -19336, -22474, -4586 }, { -4346, -18213, 23439 }, { 5221, -29504, -1502 }, { 9617, -14242, 24590 }, { -3552, 20356, 21749 }, { -14622, 2967, 26027 }, { 14957, 18866, 17899 }, { -12962, 140, -16515 }, { 12145, 3028, 11350 }, { 20178, -13783, 13576 }, { 2225, 2899, -736 }, { -9326, -6818, 3084 } },
    { { 20420, -5698, 21227 }, { -20182, -22104, -2027 }, { -2766, -20062, 22133 }, { 3610, -29777, -525 }, { 10339, -12814, 25078 }, { -6612, 20032, 21331 }, { -14998, -1977, 25907 }, { 13398, 19649, 18287 }, { -13378, 131, -16636 }, { 12863, 3288, 11511 }, { 20100, -13767, 13714 }, { 449, 3011, -1788 }, { -8921, -6867, 2877 } },
    { { 19802, -6900, 21454 }, { -20249, -22120, 817 }, { -1331, -21907, 20453 }, { 1830, -29918, 1257 }, { 11017, -10182, 25980 }, { -9835, 20223, 19857 }, { -14850, -6995, 25110 }, { 11966, 20426, 18428 }, { -13856, 209, -16826 }, { 13813, 3508, 11937 }, { 20493, -13920, 13318 }, { -691, 3274, -2553 }, { -7619, -7103, 2339 } },
    { { 17926, -8669, 22439 }, { -19165, -22926, 2667 }, { 209, -23227, 18985 }, { -454, -29851, 2950 }, { 11914, -6846, 26668 }, { -12687, 20446, 17917 }, { -14059, -11962, 23649 }, { 10262, 21570, 18150 }, { -14306, 406, -17051 }, { 14627, 3553, 12564 }, { 21359, -14231, 12256 }, { -928, 3525, -2782 }, { -5705, -7066, 1033 } },
    { { 15043, -10716, 23640 }, { -17010, -24474, 3418 }, { 1307, -23839, 18166 }, { -2456, -29728, 3197 }, { 13047, -3174, 26827 }, { -15139, 20586, 15717 }, { -12757, -15842, 22052 }, { 8234, 22745, 17744 }, { -14464, 614, -17252 }, { 15056, 3328, 13239 }, { 22578, -14438, 10721 }, { -425, 3358, -2601 }, { -4284, -5578, -55 } },
    { { 11947, -13171, 24162 }, { -14646, -25930, 3624 }, { -113, -22966, 19302 }, { -1893, -29938, 346 }, { 14307, 586, 26362 }, { -17157, 20450, 13691 }, { -11356, -17922, 21210 }, { 6415, 23427, 17607 }, { -14110, 728, -17298 }, { 14987, 2921, 13854 }, { 23347, -14378, 9691 }, { 227, 2627, -2147 }, { -3752, -4544, -482 } },
    { { 9301, -15445, 23978 }, { -12988, -26833, 3359 }, { -2107, -21065, 21256 }, { 21, -29706, -4189 }, { 14641, 4188, 25848 }, { -17407, 21014, 12466 }, { -10321, -18408, 21322 }, { 5374, 23440, 17936 }, { -13293, 719, -17129 }, { 14422, 2601, 14316 }, { 23087, -14222, 9849 }, { 486, 1820, -1574 }, { -3136, -5200, -392 } },
    { { 7463, -16864, 23662 }, { -12280, -27207, 2999 }, { -4617, -18543, 23127 }, { 3152, -28638, -8364 }, { 13719, 8026, 25444 }, { -16415, 21359, 13204 }, { -9823, -17791, 22068 }, { 4950, 22795, 18865 }, { -12291, 629, -16818 }, { 13573, 2530, 14564 }, { 21670, -14169, 11311 }, { 407, 1565, -1094 }, { -2101, -7160, -300 } },
    { { 6195, -17590, 23499 }, { -11966, -27380, 2676 }, { -7670, -16066, 24146 }, { 6890, -26877, -11408 }, { 12516, 10597, 25121 }, { -15419, 20451, 15620 }, { -9632, -16950, 22802 }, { 4675, 21865, 20002 }, { -11390, 543, -16453 }, { 12807, 2645, 14701 }, { 19220, -14174, 13828 }, { 303, 1762, -734 }, { -873, -9007, -458 } },
    { { 4891, -18208, 23335 }, { -11366, -27657, 2426 }, { -10841, -13501, 24499 }, { 10362, -25037, -12877 }, { 12447, 10009, 25395 }, { -14844, 18815, 18046 }, { -9666, -16684, 22983 }, { 4534, 21089, 20849 }, { -10698, 509, -16133 }, { 12401, 2743, 14924 }, { 17093, -14177, 15889 }, { 197, 1940, -362 }, { 61, -10048, -432 } },
    { { 3434, -18973, 22983 }, { -10578, -27983, 2254 }, { -13527, -11231, 24308 }, { 13018, -23731, -12937 }, { 13453, 6934, 25903 }, { -14084, 18043, 19392 }, { -9943, -17131, 22531 }, { 4528, 20650, 21285 }, { -10133, 521, -15966 }, { 12350, 2716, 15304 }, { 16663, -14356, 16157 }, { 60, 1969, 77 }, { 422, -10622, -14 } },
};

static const Vec3s sGdfClipLadderTop[34][13] = {
    { { 1159, -24878, 16726 }, { -3257, -16231, -25019 }, { -1974, -24178, 17651 }, { 3213, -29779, 1698 }, { 12330, -27349, 2 }, { -28317, -8127, 5665 }, { -7945, -3479, -28719 }, { -5546, -16436, 24477 }, { -9355, 8262, -14606 }, { 14167, -7812, 15178 }, { -19234, -10040, -10020 }, { 12895, -3487, 1564 }, { -11103, -11203, 5030 } },
    { { 645, -23048, 19193 }, { -2025, -9501, -28384 }, { -1048, -25031, 16502 }, { 3440, -29752, 1733 }, { 13127, -26426, 5419 }, { -28459, -6989, 6421 }, { -5335, -803, -29511 }, { -5680, -19620, 21973 }, { -9794, 8039, -14672 }, { 14833, -7755, 15165 }, { -19813, -9883, -9423 }, { 13870, -3659, 678 }, { -10721, -11237, 4794 } },
    { { 1920, -19564, 22662 }, { -2355, -4494, -29568 }, { -405, -25390, 15975 }, { 3868, -29715, 1432 }, { 12007, -26554, 7120 }, { -28637, -4414, 7775 }, { -3444, 2090, -29728 }, { -4970, -22438, 19283 }, { -10149, 7960, -14587 }, { 15469, -7688, 15153 }, { -20516, -9390, -8708 }, { 13354, -2997, -241 }, { -10443, -11256, 4620 } },
    { { 3994, -12114, 27153 }, { -3325, -5930, -29220 }, { 78, -25333, 16070 }, { 4355, -29667, 941 }, { 9908, -27342, 7367 }, { -28507, -1454, 9233 }, { -3082, 4653, -29476 }, { -3382, -24357, 17184 }, { -10334, 8369, -14441 }, { 16028, -7628, 15155 }, { -21035, -8774, -8142 }, { 10770, -2378, -445 }, { -10298, -11273, 4532 } },
    { { 6021, 311, 29388 }, { -4302, -16500, -24683 }, { 566, -25271, 16157 }, { 4687, -29627, 500 }, { 7865, -27951, 7544 }, { -27995, 762, 10756 }, { -3947, 6275, -29070 }, { -1506, -25138, 16304 }, { -10352, 9122, -14259 }, { 16509, -7578, 15144 }, { -21150, -8177, -7827 }, { 6518, -2083, 399 }, { -10250, -11279, 4514 } },
    { { 6292, 7466, 28367 }, { -3740, -25733, -14960 }, { 1292, -25567, 15641 }, { 4752, -29610, 833 }, { 6778, -28066, 8147 }, { -27290, 1444, 12375 }, { -4945, 7393, -28651 }, { 223, -25329, 16075 }, { -10465, 9903, -14157 }, { 16892, -7532, 15124 }, { -20918, -7614, -7753 }, { 2501, -1503, 1760 }, { -10099, -11257, 4395 } },
    { { 5580, 7353, 28545 }, { -2905, -28766, -8006 }, { 2007, -25977, 14871 }, { 4751, -29597, 1203 }, { 6237, -28079, 8526 }, { -26808, 818, 13440 }, { -5566, 9353, -27956 }, { 1547, -25966, 14945 }, { -10658, 10398, -14315 }, { 17143, -7484, 15125 }, { -20598, -7062, -7874 }, { 559, -662, 3082 }, { -10117, -11287, 4383 } },
    { { 5343, 5845, 28936 }, { -2386, -29023, -7209 }, { 2651, -26943, 12925 }, { 4553, -29599, 1774 }, { 4888, -28502, 7984 }, { -26848, -98, 13385 }, { -5777, 12708, -26554 }, { 2250, -27650, 11421 }, { -10689, 10522, -14505 }, { 17281, -7464, 15056 }, { -20313, -6478, -8201 }, { 222, -414, 3362 }, { -10695, -11349, 4897 } },
    { { 5040, 2435, 29473 }, { -2256, -28029, -10453 }, { 3328, -28502, 8749 }, { 4403, -29655, 1080 }, { 2339, -29181, 6558 }, { -26945, -1259, 13129 }, { -6872, 13851, -25709 }, { 3331, -29231, 5868 }, { -10458, 10618, -14507 }, { 17387, -7566, 14652 }, { -19886, -5806, -8851 }, { 534, -529, 3326 }, { -8841, -10955, 3493 } },
    { { 4070, -3953, 29459 }, { -1951, -26278, -14341 }, { 3856, -29421, 4418 }, { 4861, -29369, -3721 }, { -958, -29667, 4353 }, { -26571, -3467, 13489 }, { -8337, 10141, -26975 }, { 3309, -29791, 1251 }, { -10137, 10674, -14483 }, { 17548, -7827, 13809 }, { -19165, -5044, -9826 }, { 1048, -706, 3298 }, { -3138, -9247, -670 } },
    { { 2884, -9613, 28271 }, { -1629, -25298, -16042 }, { 4435, -29657, -900 }, { 5225, -28577, -7487 }, { -4319, -29576, 2565 }, { -26130, -5643, 13615 }, { -8912, 5048, -28198 }, { 1230, -29917, -1865 }, { -9883, 10677, -14548 }, { 17761, -8158, 12777 }, { -18224, -4281, -10942 }, { 1628, -902, 3267 }, { 2045, -6890, -3338 } },
    { { 2077, -13375, 26773 }, { -1581, -25046, -16438 }, { 4470, -29662, -403 }, { 5491, -25969, -13980 }, { -7657, -28940, 1968 }, { -25743, -7736, 13321 }, { -10088, 357, -28251 }, { -813, -29639, -4567 }, { -9905, 10682, -14728 }, { 17980, -8368, 11872 }, { -17310, -3659, -11952 }, { 2137, -1070, 3237 }, { 5721, -5339, -4593 } },
    { { 1277, -16667, 24911 }, { -1331, -25118, -16349 }, { 4027, -29666, 1923 }, { 5150, -22654, -18980 }, { -10896, -27808, 2830 }, { -25305, -9439, 13059 }, { -11436, -4189, -27417 }, { -2361, -28905, -7675 }, { -10297, 10702, -15230 }, { 18062, -8334, 11222 }, { -16579, -3350, -12807 }, { 2434, -1167, 3218 }, { 7710, -4124, -4937 } },
    { { 971, -19853, 22470 }, { -1502, -25463, -15792 }, { 3246, -28924, 7272 }, { 3775, -19055, -22862 }, { -12397, -27051, 3816 }, { -25032, -9748, 13355 }, { -11245, -7610, -26751 }, { -3123, -27756, -10948 }, { -10804, 10501, -15880 }, { 17797, -8150, 10922 }, { -15925, -3369, -13574 }, { 2462, -1176, 3216 }, { 8466, -3055, -5169 } },
    { { 1227, -22920, 19317 }, { -1996, -25786, -15202 }, { 2797, -26317, 14128 }, { 1718, -15889, -25388 }, { -12448, -26906, 4593 }, { -25100, -8955, 13777 }, { -9596, -9428, -26815 }, { -2820, -26310, -14135 }, { -11363, 9882, -16487 }, { 17254, -7836, 10980 }, { -15386, -3511, -14175 }, { 2462, -1176, 3217 }, { 8554, -2376, -5601 } },
    { { 1765, -25708, 15362 }, { -2236, -26051, -14709 }, { 2472, -22467, 19726 }, { 388, -15623, -25608 }, { -10891, -27506, 4978 }, { -25531, -7055, 14085 }, { -7693, -9586, -27367 }, { -1783, -24948, -16565 }, { -11948, 8963, -16905 }, { 16758, -7272, 11125 }, { -15392, -3782, -14498 }, { 2462, -1176, 3217 }, { 8100, -2434, -5448 } },
    { { 2739, -27867, 10767 }, { -2459, -26445, -13950 }, { 2131, -19249, 22912 }, { 19, -19011, -23208 }, { -7150, -28672, 5177 }, { -26002, -4910, 14135 }, { -6743, -8652, -27923 }, { -1059, -23890, -18115 }, { -12709, 7949, -17526 }, { 16442, -6548, 11157 }, { -16246, -4448, -14599 }, { 2462, -1176, 3216 }, { 6917, -3652, -4214 } },
    { { 3309, -29298, 5538 }, { -2154, -27089, -12711 }, { 1309, -18067, 23914 }, { 574, -23448, -18704 }, { -4099, -29095, 6058 }, { -26423, -4830, 13360 }, { -5513, -7263, -28581 }, { 148, -23761, -18314 }, { -13230, 7077, -17995 }, { 16093, -5873, 11219 }, { -17572, -5619, -14544 }, { 2455, -1174, 3216 }, { 4090, -5872, -1926 } },
    { { 3285, -29784, 1460 }, { -1616, -27573, -11710 }, { -581, -18467, 23635 }, { 2207, -26933, -13029 }, { 1440, -29300, 6278 }, { -25538, -7234, 13981 }, { -3987, -6753, -28957 }, { 1628, -24488, -17254 }, { -13436, 6428, -18120 }, { 15569, -5284, 11498 }, { -18707, -7105, -14283 }, { 3565, -1517, 3057 }, { -1686, -7861, 1106 } },
    { { 3087, -29787, -1793 }, { -803, -27685, -11528 }, { -2261, -20259, 22011 }, { 3197, -29247, -5866 }, { 3884, -29162, 5876 }, { -23177, -12571, 14311 }, { -2850, -6928, -29049 }, { 2309, -25642, -15400 }, { -13616, 6056, -18092 }, { 15002, -4709, 11936 }, { -19595, -8720, -13714 }, { 5223, -2015, 2725 }, { -9450, -8828, 4168 } },
    { { 3377, -29673, -2844 }, { -633, -27104, -12844 }, { -1932, -23133, 19004 }, { 2194, -29892, 1276 }, { 4684, -29238, 4817 }, { -19152, -17941, 14537 }, { -1918, -6984, -29113 }, { 2971, -27041, -12646 }, { -13812, 5919, -18018 }, { 14573, -4153, 12451 }, { -20802, -10255, -12587 }, { 7185, -2543, 2250 }, { -14013, -9463, 5368 } },
    { { 4345, -29673, -787 }, { -1771, -25225, -16142 }, { -506, -26416, 14211 }, { 1110, -29185, 6855 }, { 5748, -29234, 3515 }, { -12850, -22642, 14908 }, { -610, -7134, -29133 }, { 3890, -28409, -8819 }, { -13873, 5700, -17900 }, { 14497, -3609, 13017 }, { -22948, -11313, -10415 }, { 9186, -3003, 1720 }, { -12807, -10187, 3645 } },
    { { 5752, -29271, 3182 }, { -4111, -21986, -19993 }, { -138, -27402, 12211 }, { 1204, -29020, 7509 }, { 7425, -28975, 2309 }, { -4115, -26351, 13736 }, { 1101, -7752, -28960 }, { 3662, -29414, -4630 }, { -13923, 5294, -17832 }, { 14942, -3060, 13560 }, { -25446, -11487, -7584 }, { 10942, -3364, 1281 }, { -8810, -10509, 593 } },
    { { 7485, -27820, 8367 }, { -8029, -19170, -21635 }, { -1391, -25882, 15105 }, { 2173, -29751, 3184 }, { 9645, -28387, 1082 }, { 4699, -28017, 9642 }, { 2140, -8763, -28612 }, { 2432, -29851, -1736 }, { -14056, 4880, -17782 }, { 15625, -2582, 13967 }, { -26414, -11028, -6018 }, { 11463, -3705, 1492 }, { -5358, -10385, -1632 } },
    { { 7777, -25515, 13729 }, { -9124, -19226, -21145 }, { -2938, -23583, 18309 }, { 3485, -29737, -1892 }, { 11025, -27888, 857 }, { 10835, -27217, 6466 }, { 815, -11889, -27532 }, { 1142, -29961, -1012 }, { -14083, 4658, -17718 }, { 16103, -2260, 14272 }, { -25621, -10446, -6226 }, { 8908, -3882, 2626 }, { -4118, -10250, -2219 } },
    { { 7341, -22850, 18000 }, { -6915, -21917, -19283 }, { -3581, -23099, 18805 }, { 4147, -29340, -4684 }, { 13356, -26840, -1112 }, { 14686, -25801, 4317 }, { -3278, -16760, -24665 }, { 289, -29992, -620 }, { -13805, 4574, -17559 }, { 16238, -2049, 14575 }, { -24167, -9880, -7228 }, { 4463, -3495, 4236 }, { -4254, -10245, -2104 } },
    { { 7286, -21125, 20016 }, { -2956, -24994, -16327 }, { -4322, -23587, 18027 }, { 4805, -28974, -6116 }, { 15567, -25351, -3870 }, { 17402, -24422, 857 }, { -7478, -20629, -20458 }, { 201, -29929, 2053 }, { -13293, 4489, -17263 }, { 16091, -1880, 14865 }, { -22943, -9455, -8127 }, { 926, -2701, 5472 }, { -4334, -10254, -2085 } },
    { { 7431, -21154, 19932 }, { 1489, -27513, -11866 }, { -4940, -24167, 17075 }, { 5059, -28667, -7251 }, { 16994, -23467, -7778 }, { 19145, -23035, -1692 }, { -11354, -24476, -13115 }, { -298, -28922, 7964 }, { -12681, 4355, -16903 }, { 15648, -1706, 15088 }, { -22179, -9420, -8707 }, { -961, -1644, 6399 }, { -4274, -10251, -2117 } },
    { { 7761, -22797, 17890 }, { 5751, -28670, -6706 }, { -4957, -24772, 16180 }, { 4353, -28491, -8326 }, { 17796, -22197, -9520 }, { 19752, -22580, -8 }, { -12037, -27204, -3881 }, { -2089, -27216, 12446 }, { -11995, 4190, -16558 }, { 14955, -1506, 15251 }, { -21684, -9830, -9107 }, { -1803, -597, 7165 }, { -4281, -10251, -2109 } },
    { { 8554, -24816, 14525 }, { 8577, -28672, -2081 }, { -4897, -25397, 15198 }, { 3131, -28393, -9166 }, { 18127, -22031, -9278 }, { 19474, -22524, 3665 }, { -10524, -27914, 3173 }, { -4822, -26404, 13401 }, { -11337, 4028, -16290 }, { 14238, -1328, 15362 }, { -21311, -10492, -9394 }, { -1087, -285, 7229 }, { -4295, -10252, -2103 } },
    { { 9207, -26321, 11065 }, { 9414, -28464, 1077 }, { -5196, -26030, 13979 }, { 1955, -28370, -9556 }, { 18137, -22135, -9005 }, { 18199, -22825, 6917 }, { -9792, -27762, 5779 }, { -5710, -25462, 14802 }, { -10886, 3869, -16124 }, { 13734, -1288, 15402 }, { -21084, -11097, -9542 }, { 795, -714, 6084 }, { -4293, -10253, -2105 } },
    { { 8931, -26951, 9688 }, { 8719, -28666, 1492 }, { -5792, -26576, 12655 }, { 883, -28426, -9550 }, { 17887, -22343, -8992 }, { 15696, -23472, 10134 }, { -10741, -27671, 4350 }, { -5524, -23217, 18178 }, { -10694, 3693, -16088 }, { 13485, -1437, 15380 }, { -21035, -11485, -9568 }, { 2127, -803, 4304 }, { -4296, -10253, -2103 } },
    { { 8216, -27059, 10017 }, { 7469, -29055, 93 }, { -6417, -26974, 11456 }, { -165, -28508, -9341 }, { 17402, -22791, -8817 }, { 12267, -24038, 13103 }, { -11974, -27507, 131 }, { -4684, -20144, 21732 }, { -10659, 3578, -16113 }, { 13384, -1638, 15343 }, { -21203, -11745, -9459 }, { 2280, -680, 3398 }, { -4301, -10253, -2101 } },
    { { 7754, -27078, 10327 }, { 6394, -29294, -991 }, { -6894, -27206, 10598 }, { -1135, -28522, -9231 }, { 16794, -23306, -8650 }, { 8974, -24090, 15465 }, { -12194, -27072, -4294 }, { -3819, -17798, 23846 }, { -10668, 3600, -16108 }, { 13332, -1737, 15335 }, { -21639, -11989, -9177 }, { 2207, -666, 3391 }, { -4305, -10254, -2100 } },
};

// ---- The dance emote (the Ganondorf menu) ----
// The Cucui Ganon dance, from MMadness' "Suavemente" motion kit for MikuMikuDance, retargeted offline to his skeleton
// like the Mixamo clips: the kit's intro and then its four dances, one after another, each blended into the next; after
// the fourth, the four again, until the song starts over. sGdfClipDanceHips: how far his hips sway (x, z in his model
// space) so his feet stay put. The song (the player's own .wav, see GanondorfDance.cpp) sets the time, so he keeps
// with it.
#include "gdf_dance_clip.h" // the dance itself: sGdfClipDance and sGdfClipDanceHips
#define GDF_DANCE_RATE 15.0f   // rows a second

void BossGanon_Player_Dance(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupFloat(BossGanon* this, PlayState* play);
static f32 sGdfDanceClock = 0.0f; // without the song: his own count, in seconds

void BossGanon_Player_SetupDance(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -6.0f);
    this->actionFunc = BossGanon_Player_Dance;
    this->playerSwordTimer = 0;
    this->playerWalkWeight = 0.0f;
    sGdfDanceClock = 0.0f;
    GanondorfDance_Start();
}

void BossGanon_Player_Dance(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];
    Vec3s* joints = this->skelAnime.jointTable;
    f32 t = GanondorfDance_Time();
    f32 phase;
    f32 frame;
    f32 weight;
    f32 w;
    s32 a;
    s32 b;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->playerWalkWeight, 1.0f, 0.3f);
    sGdfDanceClock += 1.0f / 20.0f;
    if (t < 0.0f) {
        t = sGdfDanceClock;
    }
    // The intro once, then the four dances round and round
    frame = t * GDF_DANCE_RATE;
    if (frame >= GDF_DANCE_ROWS - 1) {
        phase = (frame - GDF_DANCE_INTRO_ROWS) / (f32)(GDF_DANCE_ROWS - 1 - GDF_DANCE_INTRO_ROWS);
        phase -= floorf(phase);
        frame = GDF_DANCE_INTRO_ROWS + phase * (GDF_DANCE_ROWS - 1 - GDF_DANCE_INTRO_ROWS);
    }
    weight = CLAMP(this->playerSwordTimer / 6.0f, 0.0f, 1.0f); // (eased in from standing)
    BossGanon_Player_ApplyClip(this, sGdfClipDance, GDF_DANCE_ROWS, frame, false, weight, 1.0f);
    a = CLAMP((s32)frame, 0, GDF_DANCE_ROWS - 1);
    b = MIN(a + 1, GDF_DANCE_ROWS - 1);
    w = frame - floorf(frame);
    joints[0].x += (s16)((sGdfClipDanceHips[a][0] * (1.0f - w) + sGdfClipDanceHips[b][0] * w) * weight);
    joints[0].z += (s16)((sGdfClipDanceHips[a][1] * (1.0f - w) + sGdfClipDanceHips[b][1] * w) * weight);

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.6f;
    sCape->sideSwayMagnitude = -1.5f;

    if (this->playerSwordTimer < 100) {
        this->playerSwordTimer++;
    }
    // The stick or any button, or off the ground: he stops
    if (!this->playerGrounded || (ABS(input->rel.stick_x) > 20) || (ABS(input->rel.stick_y) > 20) ||
        CHECK_BTN_ANY(input->press.button, BTN_A | BTN_B | BTN_Z | BTN_R | BTN_L | BTN_CUP | BTN_CDOWN | BTN_CLEFT |
                                               BTN_CRIGHT | BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT)) {
        GanondorfDance_Stop();
        BossGanon_Player_SetupFloat(this, play);
    }
}

typedef struct {
    s8 t;   // the attack's frame (20 a second)
    s8 clip; // the clip's (30 a second)
} GdfClipKey;

// Applies a clip's frame (between frames: blended) to his pose, by `weight`
static void BossGanon_Player_BlendRot(GdfMat3 cur, GdfMat3 target, f32 weight, GdfMat3 out) {
    GdfMat3 delta;
    f32 axis[3];
    f32 len;
    f32 angle;
    f32 c;
    f32 sn;
    f32 v;
    GdfMat3 rot;

    BossGanon_Pg_Mul(cur, target, delta, true); // cur^T * target: the turn left, in cur's own frame
    axis[0] = delta[2][1] - delta[1][2];
    axis[1] = delta[0][2] - delta[2][0];
    axis[2] = delta[1][0] - delta[0][1];
    len = sqrtf(SQ(axis[0]) + SQ(axis[1]) + SQ(axis[2]));
    angle = atan2f(len * 0.5f, CLAMP((delta[0][0] + delta[1][1] + delta[2][2] - 1.0f) * 0.5f, -1.0f, 1.0f));
    if ((len < 1e-6f) || (weight >= 1.0f)) {
        memcpy(out, (weight >= 1.0f) ? target : cur, sizeof(GdfMat3));
        return;
    }
    axis[0] /= len;
    axis[1] /= len;
    axis[2] /= len;
    angle *= weight;
    c = cosf(angle);
    sn = sinf(angle);
    v = 1.0f - c;
    rot[0][0] = c + axis[0] * axis[0] * v;
    rot[0][1] = axis[0] * axis[1] * v - axis[2] * sn;
    rot[0][2] = axis[0] * axis[2] * v + axis[1] * sn;
    rot[1][0] = axis[1] * axis[0] * v + axis[2] * sn;
    rot[1][1] = c + axis[1] * axis[1] * v;
    rot[1][2] = axis[1] * axis[2] * v - axis[0] * sn;
    rot[2][0] = axis[2] * axis[0] * v - axis[1] * sn;
    rot[2][1] = axis[2] * axis[1] * v + axis[0] * sn;
    rot[2][2] = c + axis[2] * axis[2] * v;
    BossGanon_Pg_Mul(cur, rot, out, false);
}

static void BossGanon_Player_ClipDir(const Vec3s (*clip)[13], s32 a, s32 b, f32 w, s32 i, f32 out[3]) {
    f32 len;

    out[0] = clip[a][i].x + (clip[b][i].x - clip[a][i].x) * w;
    out[1] = clip[a][i].y + (clip[b][i].y - clip[a][i].y) * w;
    out[2] = clip[a][i].z + (clip[b][i].z - clip[a][i].z) * w;
    len = sqrtf(SQ(out[0]) + SQ(out[1]) + SQ(out[2]));
    if (len < 1.0f) {
        out[0] = 0.0f;
        out[1] = -1.0f;
        out[2] = 0.0f;
        return;
    }
    out[0] /= len;
    out[1] /= len;
    out[2] /= len;
}

static void BossGanon_Player_ClipRot(const Vec3s (*clip)[13], s32 a, s32 b, f32 w, s32 i, GdfMat3 out) {
    GdfMat3 ra;
    GdfMat3 rb;

    BossGanon_Pg_FromRot((Vec3s*)&clip[a][8 + i], ra);
    BossGanon_Pg_FromRot((Vec3s*)&clip[b][8 + i], rb);
    BossGanon_Player_BlendRot(ra, rb, w, out);
}

// `body`: how much of its pelvis, chest and head turns to take (less in the run: its runner's shoulders swing a lot
// for a man his size). His feet keep how they sit on his shins (the clip's are set for a different foot: angled
// wrong on his).
static void BossGanon_Player_ApplyClip(BossGanon* this, const Vec3s (*clip)[13], s32 count, f32 frame, s32 loop,
                                       f32 weight, f32 body) {
    static const s8 sLegs[2][4] = { { 19, 20, 21, 0 }, { 22, 23, 24, 2 } }; // thigh, shin, foot, first direction
    static const s8 sArms[2][4] = { { 2, 3, 4, 4 }, { 6, 7, 8, 6 } };       // shoulder, upper arm, forearm, direction
    Vec3s* joints = this->skelAnime.jointTable;
    s32 a;
    s32 b;
    f32 w;
    s32 side;
    GdfMat3 root;
    GdfMat3 cur;
    GdfMat3 target;
    GdfMat3 pelvis;
    GdfMat3 chest;
    GdfMat3 tmp;
    GdfMat3 local;
    f32 dir[3];

    if (weight <= 0.0f) {
        return;
    }
    weight = MIN(weight, 1.0f);
    if (loop) {
        frame = fmodf(frame, (f32)count);
        if (frame < 0.0f) {
            frame += count;
        }
    } else {
        frame = CLAMP(frame, 0.0f, count - 1.0f);
    }
    a = (s32)frame;
    w = frame - a;
    b = loop ? ((a + 1) % count) : MIN(a + 1, count - 1);

    BossGanon_Pg_FromRot(&joints[1 + 0], root);

    // Pelvis and chest (children of his root)
    BossGanon_Pg_FromRot(&joints[1 + 18], tmp);
    BossGanon_Pg_Mul(root, tmp, cur, false);
    BossGanon_Player_ClipRot(clip, a, b, w, 0, target);
    BossGanon_Player_BlendRot(cur, target, weight * body, pelvis);
    BossGanon_Pg_Mul(root, pelvis, local, true);
    BossGanon_Pg_ToRot(local, &joints[1 + 18]);

    BossGanon_Pg_FromRot(&joints[1 + 1], tmp);
    BossGanon_Pg_Mul(root, tmp, cur, false);
    BossGanon_Player_ClipRot(clip, a, b, w, 1, target);
    BossGanon_Player_BlendRot(cur, target, weight * body, chest);
    BossGanon_Pg_Mul(root, chest, local, true);
    BossGanon_Pg_ToRot(local, &joints[1 + 1]);

    // Head (the limb his face hangs from)
    BossGanon_Pg_FromRot(&joints[1 + 10], tmp);
    BossGanon_Pg_Mul(chest, tmp, cur, false);
    BossGanon_Player_ClipRot(clip, a, b, w, 2, target);
    BossGanon_Player_BlendRot(cur, target, weight * body, tmp);
    BossGanon_Pg_Mul(chest, tmp, local, true);
    BossGanon_Pg_ToRot(local, &joints[1 + 10]);

    for (side = 0; side < 2; side++) {
        GdfMat3 thigh;
        GdfMat3 shin;

        BossGanon_Pg_FromRot(&joints[1 + sLegs[side][0]], tmp);
        BossGanon_Pg_Mul(pelvis, tmp, thigh, false);
        BossGanon_Player_ClipDir(clip, a, b, w, sLegs[side][3], dir);
        BossGanon_Player_SwingBoneToward(thigh, dir, weight);
        BossGanon_Pg_FromRot(&joints[1 + sLegs[side][1]], tmp);
        BossGanon_Pg_Mul(thigh, tmp, shin, false);
        BossGanon_Player_ClipDir(clip, a, b, w, sLegs[side][3] + 1, dir);
        BossGanon_Player_SwingBoneToward(shin, dir, weight);

        BossGanon_Pg_Mul(pelvis, thigh, local, true);
        BossGanon_Pg_ToRot(local, &joints[1 + sLegs[side][0]]);
        BossGanon_Pg_Mul(thigh, shin, local, true);
        BossGanon_Pg_ToRot(local, &joints[1 + sLegs[side][1]]);
    }

    for (side = 0; side < 2; side++) {
        GdfMat3 shoulder;
        GdfMat3 upper;
        GdfMat3 fore;

        BossGanon_Pg_FromRot(&joints[1 + sArms[side][0]], tmp);
        BossGanon_Pg_Mul(chest, tmp, shoulder, false);
        BossGanon_Pg_FromRot(&joints[1 + sArms[side][1]], tmp);
        BossGanon_Pg_Mul(shoulder, tmp, upper, false);
        BossGanon_Player_ClipDir(clip, a, b, w, sArms[side][3], dir);
        BossGanon_Player_SwingBoneToward(upper, dir, weight);
        BossGanon_Pg_FromRot(&joints[1 + sArms[side][2]], tmp);
        BossGanon_Pg_Mul(upper, tmp, fore, false);
        BossGanon_Player_ClipDir(clip, a, b, w, sArms[side][3] + 1, dir);
        BossGanon_Player_SwingBoneToward(fore, dir, weight);

        BossGanon_Pg_Mul(shoulder, upper, local, true);
        BossGanon_Pg_ToRot(local, &joints[1 + sArms[side][1]]);
        BossGanon_Pg_Mul(upper, fore, local, true);
        BossGanon_Pg_ToRot(local, &joints[1 + sArms[side][2]]);
    }
}

// The attack's frame to the clip's, through the keys
static f32 BossGanon_Player_ClipFrame(const GdfClipKey* keys, s32 count, f32 t) {
    s32 i;

    if (t <= keys[0].t) {
        return keys[0].clip;
    }
    for (i = 0; i < count - 1; i++) {
        if (t <= keys[i + 1].t) {
            f32 u = (t - keys[i].t) / (f32)(keys[i + 1].t - keys[i].t);

            return keys[i].clip + (keys[i + 1].clip - keys[i].clip) * u;
        }
    }
    return keys[count - 1].clip;
}

// The trident's first hit: the slash, wound up from his right and cut across to his left (striking at frame 6)
static const GdfClipKey sGdfTridentSlash1[] = { { 0, 0 }, { 3, 10 }, { 6, 20 }, { 10, 30 }, { 18, 45 } };
// The second: the slash backwards, a rising backhand from his left up to his right (striking at frame 3)
static const GdfClipKey sGdfTridentSlash2[] = { { 0, 30 }, { 1, 27 }, { 3, 20 }, { 7, 12 }, { 15, 0 } };
// The spin: a full turn on the run (turning from frame 3 to 16)
// The third: the stab, drawn back and thrust out at frame 3, held to frame 11 (then blended back to his stance)
static const GdfClipKey sGdfTridentStab[] = { { 0, 6 }, { 2, 11 }, { 3, 15 }, { 11, 27 }, { 18, 31 } };
static const GdfClipKey sGdfTridentSpinClip[] = { { 0, 0 }, { 3, 2 }, { 16, 18 }, { 24, 39 } };

// With the trident in hand: his combo's slashes and his spin follow the clips (blending in and out of them)
static s32 BossGanon_Player_ApplyTridentAttack(BossGanon* this) {
    f32 t = this->playerSwordTimer;
    f32 length = sGdfSwordAttacks[this->playerSwordAttack].length;
    f32 weight = CLAMP(t / 2.0f, 0.0f, 1.0f) * CLAMP((length - t) / 4.0f, 0.0f, 1.0f);

    if (!BossGanon_Player_HasTrident()) {
        return false;
    }
    switch (this->playerSwordAttack) {
        case GDF_SWORD_ATK_COMBO1:
            BossGanon_Player_ApplyClip(this, sGdfClipSlash, ARRAY_COUNT(sGdfClipSlash),
                                       BossGanon_Player_ClipFrame(sGdfTridentSlash1, ARRAY_COUNT(sGdfTridentSlash1), t),
                                       false, weight, 1.0f);
            return true;
        case GDF_SWORD_ATK_COMBO2:
            BossGanon_Player_ApplyClip(this, sGdfClipSlash, ARRAY_COUNT(sGdfClipSlash),
                                       BossGanon_Player_ClipFrame(sGdfTridentSlash2, ARRAY_COUNT(sGdfTridentSlash2), t),
                                       false, weight, 1.0f);
            return true;
        case GDF_SWORD_ATK_THRUST:
            BossGanon_Player_ApplyClip(this, sGdfClipStab, ARRAY_COUNT(sGdfClipStab),
                                       BossGanon_Player_ClipFrame(sGdfTridentStab, ARRAY_COUNT(sGdfTridentStab), t),
                                       false, weight, 1.0f);
            return true;
        case GDF_SWORD_ATK_SPIN:
            BossGanon_Player_ApplyClip(this, sGdfClipSpin, ARRAY_COUNT(sGdfClipSpin),
                                       BossGanon_Player_ClipFrame(sGdfTridentSpinClip, ARRAY_COUNT(sGdfTridentSpinClip), t),
                                       false, weight, 1.0f);
            return true;
        default:
            return false;
    }
}

// Running with the trident: the clip's run, in step with his walk (its right heel comes down at its frame 17)
static void BossGanon_Player_ApplyTridentRun(BossGanon* this) {
    f32 phase;

    if (!BossGanon_Player_HasTrident() || !this->playerGrounded || (this->playerWalkWeight <= 0.0f)) {
        return;
    }
    phase = (this->playerWalkFrame - 1.0f) / GDF_WALK_CYCLE_FRAMES;
    phase -= floorf(phase);
    BossGanon_Player_ApplyClip(this, sGdfClipRun, ARRAY_COUNT(sGdfClipRun),
                               17.0f + phase * ARRAY_COUNT(sGdfClipRun), true, this->playerWalkWeight, 0.3f);
}

// The opening in Link's house: instead of lying in Link's bed, Ganondorf leans against the wall beside it, arms crossed
// and head bowed, brooding, for as long as the scene's cutscene runs. The arms were posed offline with IK on his
// skeleton (each forearm across his chest, the fist tucked by the other arm's biceps).
#define GDF_BROOD_LEAN_DEG 9.0f  // leaning back against the wall
#define GDF_BROOD_NOD_DEG 20.0f  // head bowed
#define GDF_BROOD_SPOT_X 74.0f   // beside the bed (the bed is against the back wall, x -35 to 35)
#define GDF_BROOD_SPOT_Z 104.0f

static const Vec3s sGdfBroodArms[8] = {
    { -7405, -16897, -9782 }, { 216, -3305, -14623 }, { 877, -5668, -15566 },  { -914, -765, -2736 }, // left 2-5
    { 8017, 21399, -8564 },   { 30700, 25088, 19765 }, { 589, 4570, -15445 }, { 2258, -798, -1675 }, // right 6-9
};

void BossGanon_Player_Brood(BossGanon* this, PlayState* play);

static s32 BossGanon_Player_ShouldBrood(PlayState* play) {
    return (play->sceneNum == SCENE_LINKS_HOUSE) && (play->csCtx.state != CS_STATE_IDLE);
}

static void BossGanon_Player_SetupBrood(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, 0.0f);
    this->actionFunc = BossGanon_Player_Brood;
    this->handLightBallScale = 0.0f;
    this->playerBtnHeld = 0;
    this->playerWalkWeight = 0.0f;
    this->playerSprintWeight = 0.0f;
}

void BossGanon_Player_Brood(BossGanon* this, PlayState* play) {
    Vec3s* joints = this->skelAnime.jointTable;
    GdfMat3 rootRot;
    GdfMat3 torso;
    GdfMat3 body;
    GdfMat3 neck;
    GdfMat3 rot;
    GdfMat3 tmp;
    s32 i;

    SkelAnime_Update(&this->skelAnime);
    this->legSwayEnabled = false;

    for (i = 0; i < 8; i++) {
        joints[1 + 2 + i] = sGdfBroodArms[i];
    }

    // Head bowed: the neck (limb 10, under the torso) turns forward about his sideways axis
    BossGanon_Pg_FromRot(&joints[1 + 0], rootRot);
    BossGanon_Pg_FromRot(&joints[1 + 1], tmp);
    BossGanon_Pg_Mul(rootRot, tmp, body, false);
    BossGanon_Pg_FromRot(&joints[1 + 10], tmp);
    BossGanon_Pg_Mul(body, tmp, neck, false);
    BossGanon_Pg_RotX(GDF_BROOD_NOD_DEG, rot);
    BossGanon_Pg_Mul(rot, neck, tmp, false);
    BossGanon_Pg_Mul(body, tmp, neck, true);
    BossGanon_Pg_ToRot(neck, &joints[1 + 10]);

    // Leaning back on the wall: the whole body tips back about his hips, feet a little forward
    BossGanon_Pg_RotX(-GDF_BROOD_LEAN_DEG, rot);
    BossGanon_Pg_Mul(rot, rootRot, torso, false);
    BossGanon_Pg_ToRot(torso, &joints[1 + 0]);
}

// Where he stands, facing into the room, his feet on the floor
static void BossGanon_Player_PlaceBrood(BossGanon* this, PlayState* play) {
    static Vec3f sCenter = { 0.0f, 0.0f, 0.0f };
    CollisionPoly* poly;
    Vec3f spot = { GDF_BROOD_SPOT_X, 100.0f, GDF_BROOD_SPOT_Z };
    f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &spot);

    // His ankles stand GDF_ANKLE_HEIGHT above the floor (from the last draw, as UpdateGroundOffset does)
    f32 ankleAboveRoot = MIN(this->unk_2EC[GDF_LEFT_FOOT_BODYPART].y, this->unk_2EC[GDF_RIGHT_FOOT_BODYPART].y) -
                         this->actor.world.pos.y;

    if ((ankleAboveRoot < -60.0f) || (ankleAboveRoot > 60.0f)) {
        ankleAboveRoot = GDF_ANKLE_HEIGHT; // first frame here (stale): stand on the floor
    }
    spot.y = ((floorY > BGCHECK_Y_MIN) ? floorY : 0.0f) + GDF_ANKLE_HEIGHT - ankleAboveRoot;
    this->actor.world.pos = spot;
    this->actor.prevPos = spot;
    this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer = Math_Vec3f_Yaw(&spot, &sCenter);
    this->playerAimYaw = this->actor.yawTowardsPlayer;
    this->actor.velocity.x = this->actor.velocity.y = this->actor.velocity.z = 0.0f;
}

// Blade (or spear) positions for the hitbox and the trail
static void BossGanon_Player_SetBlade(BossGanon* this, Vec3f* base, Vec3f* tip) {
    const GdfSwordAttack* atk = &sGdfSwordAttacks[this->playerSwordAttack];
    u8 swinging;

    if (this->playerBladeValid) {
        this->playerBladeBasePrev = this->playerBladeBase;
        this->playerBladeTipPrev = this->playerBladeTip;
    } else {
        this->playerBladeBasePrev = *base;
        this->playerBladeTipPrev = *tip;
    }
    this->playerBladeBase = *base;
    this->playerBladeTip = *tip;
    this->playerBladeValid = true;

    // Trail while swinging
    swinging = (this->actionFunc == BossGanon_Player_SwordAttack) &&
               (this->playerSwordTimer >= atk->activeStart - 1) && (this->playerSwordTimer <= atk->activeEnd + 1);
    if ((this->playerSwordBlure >= 0) && (this->playerSwordBlure < TOTAL_EFFECT_COUNT)) {
        if (swinging) {
            EffectBlure_AddVertex(Effect_GetByIndex(this->playerSwordBlure), tip, base);
        } else {
            EffectBlure_AddSpace(Effect_GetByIndex(this->playerSwordBlure));
        }
    }
}

// From the hand matrix (captured in the limb callback) to the sword's own frame
static void BossGanon_Player_SwordFrame(MtxF* hand, f32 gripZ) {
    f32 scale = CVarGetFloat(CVAR_PLAY_AS_GDF("SwordSize"), GDF_SWORD_SCALE);

    Matrix_Put(hand);
    Matrix_Translate(GDF_GRIP_X, GDF_GRIP_Y, GDF_GRIP_Z, MTXMODE_APPLY);
    Matrix_RotateX(M_PI, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, -gripZ, MTXMODE_APPLY);
}

static void BossGanon_Player_DrawSword(BossGanon* this, PlayState* play) {
    static Vec3f sBase = { 0.0f, 0.0f, GDF_BLADE_BASE_Z };
    static Vec3f sTip = { 0.0f, 0.0f, GDF_BLADE_TIP_Z };
    Gfx* swordDL;
    Gfx* sheathDL;

    // Phantom Ganon: his spear is part of his model
    if (BossGanon_Player_IsPhantom()) {
        if (!this->playerSpearValid || (sPlayerSwordState != GDF_SWORD_DRAWN)) {
            this->playerBladeValid = false;
            return;
        }
        BossGanon_Player_SetBlade(this, &this->playerSpearBase, &this->playerSpearTip);
        return;
    }

    if ((sPlayerSwordState == GDF_SWORD_OFF) || !this->playerHandMtxValid) {
        this->playerBladeValid = false;
        return;
    }

    // A trident: held at his side in his left hand while sheathed, in his right hand drawn
    if (BossGanon_Player_HasTrident()) {
        f32 headZ;
        f32 tipZ;
        Gfx* tridentDL = GanondorfTrident_GetDL(GanondorfTrident_GetSelected() - 1, &headZ, &tipZ);

        if (tridentDL != NULL) {
            Vec3f headLocal = { 0.0f, 0.0f, headZ };
            Vec3f tipLocal = { 0.0f, 0.0f, tipZ };

            OPEN_DISPS(play->state.gfxCtx);
            Matrix_Push();
            if (sGdfTridentState != GDF_TRIDENT_IN_HAND) {
                // Spun flat over his head (about his hand), or flying spinning about its middle
                f32 scale = this->actor.scale.x * CVarGetFloat(CVAR_PLAY_AS_GDF("SwordSize"), GDF_SWORD_SCALE);

                if (sGdfTridentState == GDF_TRIDENT_OVERHEAD) {
                    Matrix_Translate(this->unk_2EC[8].x, this->unk_2EC[8].y + 6.0f, this->unk_2EC[8].z, MTXMODE_NEW);
                    Matrix_RotateY(BINANG_TO_RAD(sGdfTridentSpin), MTXMODE_APPLY);
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    Matrix_Translate(0.0f, 0.0f, -GDF_TRIDENT_GRIP_Z, MTXMODE_APPLY);
                } else {
                    Matrix_Translate(sGdfTridentPos.x, sGdfTridentPos.y, sGdfTridentPos.z, MTXMODE_NEW);
                    Matrix_RotateY(BINANG_TO_RAD(sGdfTridentSpin), MTXMODE_APPLY);
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    Matrix_Translate(0.0f, 0.0f, -(headZ + GDF_TRIDENT_TAIL_Z) * 0.5f, MTXMODE_APPLY);
                }
                this->playerBladeValid = false;
            } else if ((sPlayerSwordState == GDF_SWORD_SHEATHED) ||
                       ((this->actionFunc == BossGanon_Player_SwordIdle) && (this->playerGuardWeight < 0.5f))) {
                // At rest the trident's set by his body, not his wrist (his fists aren't modelled around a shaft):
                // put away, carried level and pointing ahead in his left fist; drawn, in his right fist with the
                // prongs leaning forward and down toward the ground
                static Vec3f sGrip = { GDF_GRIP_X, GDF_GRIP_Y, GDF_GRIP_Z };
                u8 carried = sPlayerSwordState == GDF_SWORD_SHEATHED;
                f32 scale = this->actor.scale.x * CVarGetFloat(CVAR_PLAY_AS_GDF("SwordSize"), GDF_SWORD_SCALE);
                Vec3f grip;

                Matrix_Put(carried ? &this->playerLeftHandMtx : &this->playerRightHandMtx);
                Matrix_MultVec3f(&sGrip, &grip);
                Matrix_Translate(grip.x, grip.y, grip.z, MTXMODE_NEW);
                Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
                if (!carried) {
                    Matrix_RotateX(DEG_TO_RAD(32.0f), MTXMODE_APPLY);
                }
                Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                // Put away: level and straight ahead in his still left fist, held at the middle of the shaft
                Matrix_Translate(0.0f, 0.0f, carried ? -GDF_TRIDENT_CARRY_GRIP_Z : -GDF_TRIDENT_GRIP_Z, MTXMODE_APPLY);
                if (carried) {
                    this->playerBladeValid = false;
                } else {
                    Vec3f base;
                    Vec3f tip;

                    Matrix_MultVec3f(&headLocal, &base);
                    Matrix_MultVec3f(&tipLocal, &tip);
                    BossGanon_Player_SetBlade(this, &base, &tip);
                }
            } else if ((this->actionFunc == BossGanon_Player_SwordAttack) &&
                       (this->playerSwordAttack == GDF_SWORD_ATK_THRUST)) {
                // The stab: from his fist straight out along his center line (aimed at a point ahead of his chest),
                // not along his wrist, which turned it off to the side
                static Vec3f sGrip = { GDF_GRIP_X, GDF_GRIP_Y, GDF_GRIP_Z };
                f32 scale = this->actor.scale.x * CVarGetFloat(CVAR_PLAY_AS_GDF("SwordSize"), GDF_SWORD_SCALE);
                Vec3f grip;
                Vec3f aim;
                Vec3f base;
                Vec3f tip;
                f32 dx;
                f32 dy;
                f32 dz;

                Matrix_Put(&this->playerRightHandMtx);
                Matrix_MultVec3f(&sGrip, &grip);
                aim.x = this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * 220.0f;
                aim.y = grip.y - 10.0f;
                aim.z = this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * 220.0f;
                dx = aim.x - grip.x;
                dy = aim.y - grip.y;
                dz = aim.z - grip.z;
                Matrix_Translate(grip.x, grip.y, grip.z, MTXMODE_NEW);
                Matrix_RotateY(Math_Atan2F(dz, dx), MTXMODE_APPLY);
                Matrix_RotateX(-Math_Atan2F(sqrtf(SQ(dx) + SQ(dz)), dy), MTXMODE_APPLY);
                Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                Matrix_Translate(0.0f, 0.0f, -GDF_TRIDENT_ATTACK_GRIP_Z, MTXMODE_APPLY);
                Matrix_MultVec3f(&headLocal, &base);
                Matrix_MultVec3f(&tipLocal, &tip);
                BossGanon_Player_SetBlade(this, &base, &tip);
            } else {
                Vec3f base;
                Vec3f tip;

                // Attacking: held further down the shaft, so the butt end doesn't swing through his body
                BossGanon_Player_SwordFrame(&this->playerRightHandMtx, GDF_TRIDENT_ATTACK_GRIP_Z);
                Matrix_MultVec3f(&headLocal, &base);
                Matrix_MultVec3f(&tipLocal, &tip);
                BossGanon_Player_SetBlade(this, &base, &tip);
            }
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, tridentDL);
            Matrix_Pop();
            CLOSE_DISPS(play->state.gfxCtx);
            return;
        }
    }
    swordDL = GanondorfSword_GetSwordDL();
    sheathDL = GanondorfSword_GetSheathDL();
    if (swordDL == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();

    // Sheath in his left hand, with the sword in it when it's sheathed
    BossGanon_Player_SwordFrame(&this->playerLeftHandMtx, GDF_SHEATH_GRIP_Z);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (sheathDL != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, sheathDL);
    }
    if (sPlayerSwordState == GDF_SWORD_SHEATHED) {
        gSPDisplayList(POLY_OPA_DISP++, swordDL);
        this->playerBladeValid = false;
    } else {
        // Drawn, in his right hand
        Vec3f base;
        Vec3f tip;

        BossGanon_Player_SwordFrame(&this->playerRightHandMtx, GDF_SWORD_GRIP_Z);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, swordDL);

        Matrix_MultVec3f(&sBase, &base);
        Matrix_MultVec3f(&sTip, &tip);
        BossGanon_Player_SetBlade(this, &base, &tip);
    }

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// The blade's hitbox: the area it swept since the last frame
static void BossGanon_Player_UpdateSwordHit(BossGanon* this, PlayState* play) {
    const GdfSwordAttack* atk = &sGdfSwordAttacks[this->playerSwordAttack];

    if ((this->actionFunc != BossGanon_Player_SwordAttack) || !this->playerBladeValid || this->playerHidden) {
        return;
    }
    if ((this->playerSwordTimer < atk->activeStart) || (this->playerSwordTimer > atk->activeEnd)) {
        return;
    }
    Collider_SetQuadVertices(&this->playerSwordQuad, &this->playerBladeTipPrev, &this->playerBladeTip,
                             &this->playerBladeBasePrev, &this->playerBladeBase);
    BossGanon_Player_NoteSegment(&this->playerBladeBase, &this->playerBladeTip, 12.0f);
    BossGanon_Player_NoteSegment(&this->playerBladeBasePrev, &this->playerBladeTipPrev, 12.0f);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->playerSwordQuad.base);

    // He's far taller than most enemies, so the blade alone passes over small ones. While a slash is active, the
    // space in front of him from the floor up to the blade hits too, as far out as the blade reaches.
    if ((this->playerSwordAttack != GDF_SWORD_ATK_SLAM) && (this->playerSwordAttack != GDF_SWORD_ATK_COMBO3) &&
        (this->playerSwordAttack != GDF_SWORD_ATK_WAVE)) {
        f32 dx = this->playerBladeTip.x - this->actor.world.pos.x;
        f32 dz = this->playerBladeTip.z - this->actor.world.pos.z;
        f32 reach = sqrtf(SQ(dx) + SQ(dz));
        f32 sinY = Math_SinS(this->actor.shape.rot.y);
        f32 cosY = Math_CosS(this->actor.shape.rot.y);
        f32 top = MAX(this->playerBladeTip.y, this->playerBladeBase.y) - this->playerFloorY;

        reach = CLAMP(reach, 60.0f, 220.0f);
        this->collider.dim.radius = (s16)(reach * 0.55f);
        this->collider.dim.height = (s16)CLAMP(top + 10.0f, 50.0f, 160.0f);
        this->collider.dim.yShift = 0;
        this->collider.dim.pos.x = (s16)(this->actor.world.pos.x + sinY * reach * 0.5f);
        this->collider.dim.pos.y = (s16)(this->playerFloorY - 5.0f);
        this->collider.dim.pos.z = (s16)(this->actor.world.pos.z + cosY * reach * 0.5f);
        BossGanon_Player_SetAT(this, play);
    }
}

void BossGanon_Player_Init(BossGanon* this, PlayState* play) {
    Actor* thisx = &this->actor;
    Player* player = GET_PLAYER(play);
    s16 i;

    for (i = 0; i < ARRAY_COUNT(sPlayerEffects); i++) {
        sPlayerEffects[i].type = GDF_EFF_NONE;
    }
    sCurEffects = sPlayerEffects;

    sGanondorf = this;

    Actor_ProcessInitChain(thisx, sInitChain);
    // Not an enemy: no lock-on, no Navi, no battle music
    thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    thisx->flags |= ACTOR_FLAG_UPDATE_DURING_OCARINA;
    thisx->update = BossGanon_Player_Update;
    ActorShape_Init(&thisx->shape, 0, NULL, 0);
    Actor_SetScale(thisx, 0.01f);
    SkelAnime_InitFlex(play, &this->skelAnime, &gGanondorfSkel, NULL, NULL, NULL, 0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sPlayerPoundCylinderInit);

    // Follow Link through every room, and keep updating while he talks or gets items
    thisx->room = -1;
    thisx->parent = &player->actor;

    // Its own vertex buffers, so it can be drawn together with the real Ganondorf's cape
    sCape = (EnGanonMant*)Actor_SpawnAsChild(&play->actorCtx, thisx, play, ACTOR_EN_GANON_MANT, 0.0f, 0.0f, 0.0f, 0, 0,
                                             0, PLAYER_GANONDORF_CAPE_PARAMS);
    if (sCape == NULL) {
        Actor_Kill(thisx);
        return;
    }
    sCape->actor.room = -1;
    sPlayerCapeDraw = sCape->actor.draw;

    // Ganondorf goes in front of his cape in the update and draw order, like in the vanilla init.
    // MISC also isn't affected by things that freeze or look for bosses: his cape goes there too (first, so it stays
    // behind him in the list), or it stopped moving whenever the game freezes bosses (chests, some conversations).
    Actor_ChangeCategory(play, &play->actorCtx, &sCape->actor, ACTORCAT_MISC);
    Actor_ChangeCategory(play, &play->actorCtx, thisx, ACTORCAT_MISC);

    this->playerFloorY = player->actor.floorHeight;
    Collider_InitQuad(play, &this->playerSwordQuad);
    Collider_SetQuad(play, &this->playerSwordQuad, thisx, &sPlayerSwordQuadInit);
    this->playerSwordBlure = TOTAL_EFFECT_COUNT;
    Effect_Add(play, &this->playerSwordBlure, EFFECT_BLURE2, 0, 0, &sPlayerSwordBlureInit);
    if (!CVarGetInteger(CVAR_PLAY_AS_GDF("SwordStance"), 1)) {
        sPlayerSwordState = GDF_SWORD_OFF;
    }
    this->playerPrevInvincibilityTimer = player->invincibilityTimer;
    this->playerOrganSink = GDF_ORGAN_SINK_DEPTH;
    this->triforceType = GDF_TRIFORCE_DORF;
    BossGanon_Player_ManhuntSceneChange(play);
    BossGanon_Player_SnapToLink(this, play);
    BossGanon_Player_UpdateAim(this, play);
    this->playerAimYaw = this->actor.yawTowardsPlayer;
    BossGanon_Player_SetupFloat(this, play);
}

void BossGanon_PlayerProjectile_Init(BossGanon* this, PlayState* play) {
    Actor* thisx = &this->actor;
    s16 i;

    thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    thisx->room = -1;
    this->fwork[GDF_FWORK_1] = 255.0f;
    Collider_InitCylinder(play, &this->collider);

    if (thisx->params == GDF_PARAMS_PLAYER_LIGHT_BALL) {
        thisx->update = BossGanon_PlayerLightBall_Update;
        thisx->draw = BossGanon_LightBall_Draw;
        thisx->speedXZ = 14.0f;
        Actor_SetScale(thisx, 6.0f);
        this->timers[2] = 90; // lifetime
        Collider_SetCylinder(play, &this->collider, thisx, &sPlayerLightBallCylinderInit);
    } else {
        s16 index = thisx->params - GDF_PARAMS_PLAYER_BIG_MAGIC;

        thisx->update = BossGanon_PlayerBigMagic_Update;
        thisx->draw = func_808E324C;
        Actor_SetScale(thisx, 0.01f);
        this->unk_1C2 = 10;
        // Same phase offsets as the vanilla balls (params 0x104 - 0x108)
        this->unk_1A2 = 520 + (-(0x104 + index) * 2);

        for (i = 0; i < 15; i++) {
            this->unk_2EC[i] = thisx->world.pos;
            this->unk_3C4[i].x = BINANG_TO_RAD(thisx->world.rot.x);
            this->unk_3C4[i].y = BINANG_TO_RAD(thisx->world.rot.y);
        }

        this->timers[1] = 3;
        this->timers[2] = 75; // lifetime
        Collider_SetCylinder(play, &this->collider, thisx, &sPlayerBigMagicCylinderInit);
    }
}

// Phantom Ganon always has his spear out; turning the skin off puts it away again
static u8 sGdfPgSpearForced = false;

void BossGanon_Player_SetupFloat(BossGanon* this, PlayState* play) {
    if (BossGanon_Player_IsPhantom()) {
        sPlayerSwordState = GDF_SWORD_DRAWN;
        sGdfPgSpearForced = true;
    } else if (sGdfPgSpearForced) {
        sPlayerSwordState = GDF_SWORD_OFF;
        sGdfPgSpearForced = false;
    }

    // With the sword out, his idle is the sword stance
    if (sPlayerSwordState != GDF_SWORD_OFF) {
        BossGanon_Player_SetupSwordIdle(this, play);
        return;
    }
    if (this->playerGrounded) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -10.0f);
    } else {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, -10.0f);
    }
    this->playerStandAnim = this->playerGrounded; // which of the two is playing
    this->actionFunc = BossGanon_Player_Float;
    this->unk_1C2 = 0;
    this->playerBtnHeld = 0;
}

void BossGanon_Player_Float(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    // Landed, or started levitating: stand or float
    if (this->playerStandAnim != this->playerGrounded) {
        BossGanon_Player_SetupFloat(this, play);
    }

    if (this->playerGrounded) {
        sCape->backPush = -2.0f;
        sCape->backSwayMagnitude = 0.25f;
        sCape->sideSwayMagnitude = -1.0f;
        sCape->minDist = 0.0f;

        SkelAnime_Update(&this->skelAnime);
        BossGanon_Player_UpdateWalk(this, play);
        BossGanon_Player_ApplyJump(this, play);
        Math_ApproachZeroF(&this->handLightBallScale, 1.0f, 1.0f);
        if (GanondorfDance_TakeRequest()) {
            BossGanon_Player_SetupDance(this, play); // the menu's dance emote
            return;
        }
        BossGanon_Player_ReadButtons(this, play);
        return;
    }
    this->playerWalkWeight = 0.0f;

    this->legSwayEnabled = true;

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->handLightBallScale, 1.0f, 1.0f);

    // The swish of flying. Levitation moves Link itself and keeps his speedXZ at 0, so this goes by how far he
    // actually moved this frame (up and down too)
    if (Math_Vec3f_DistXYZ(&player->actor.world.pos, &player->actor.prevPos) > 1.0f) {
        Sfx_PlaySfxAtPos(&this->actor.projectedPos, NA_SE_EN_FANTOM_FLOAT - SFX_FLAG);
    }

    BossGanon_Player_ReadButtons(this, play);
}

void BossGanon_Player_SetupChargeLightBall(BossGanon* this, PlayState* play) {
    // Invasion mode: his attacks cost magic
    if (!GdfInvasion_PayAttack(5)) {
        return;
    }
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfChargeLightBallAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfChargeLightBallAnim, -3.0f);
    this->actionFunc = BossGanon_Player_ChargeLightBall;
    this->timers[0] = 25;
    this->unk_1C2 = 0;
    BossGanon_Player_UpdateAim(this, play);
}

void BossGanon_Player_ChargeLightBall(BossGanon* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    this->legSwayEnabled = true;
    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 1.25f;
    sCape->sideSwayMagnitude = -2.0f;
    sCape->minDist = 10.0f;

    BossGanon_Player_UpdateAim(this, play);

    if (this->timers[0] == 17) {
        this->unk_26C = 10;
        this->unk_270 = Rand_ZeroFloat(M_PI);
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
    }

    if (this->timers[0] < 10) {
        this->unk_258 += (Rand_ZeroFloat(M_PI / 2) + (M_PI / 2));
        Math_ApproachF(&this->handLightBallScale, 10.0f, 0.5f, 1.25f);

        if (this->timers[0] == 0) {
            // Still holding the button: charge Big Magic instead
            if ((this->playerBtnHeld != 0) && (this->playerHeldTimer >= 20) &&
                CVarGetInteger(CVAR_PLAY_AS_GDF("BigMagic"), 1)) {
                this->handLightBallScale = 0.0f;
                BossGanon_Player_SetupChargeBigMagic(this, play);
            } else {
                BossGanon_Player_SetupThrow(this, play);
            }
        }
    }
}

void BossGanon_Player_SetupThrow(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfThrowAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfThrowAnim, 0.0f);
    this->actionFunc = BossGanon_Player_Throw;
    this->unk_1C2 = 0;
}

void BossGanon_Player_Throw(BossGanon* this, PlayState* play) {
    s16 rotX;
    s16 rotY;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->handLightBallScale, 1.0f, 0.2f);

    if (this->skelAnime.curFrame < 11.0f) {
        BossGanon_Player_UpdateAim(this, play);
    }

    if (this->skelAnime.curFrame <= 12.0f) {
        this->lensFlareTimer = 2;
        this->lensFlareMode = 2;
        gCustomLensFlarePos = this->unk_260;
    }

    if (Animation_OnFrame(&this->skelAnime, 12.0f)) {
        this->handLightBallScale = 0.0f;
    }

    if (Animation_OnFrame(&this->skelAnime, 11.0f)) {
        this->unk_25C = 1;
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW));
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW_MASIC));
        BossGanon_Player_AimRot(&this->unk_260, &this->playerAimPos, &rotX, &rotY);
        Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_260.x, this->unk_260.y,
                           this->unk_260.z, rotX, rotY, 0, GDF_PARAMS_PLAYER_LIGHT_BALL);
    }

    if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
        BossGanon_Player_SetupFloat(this, play);
    }
}

void BossGanon_Player_SetupChargeBigMagic(BossGanon* this, PlayState* play) {
    // Invasion mode: his attacks cost magic
    if (!GdfInvasion_PayAttack(15)) {
        return;
    }
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->unk_1C2 = 0;
    this->timers[0] = 8;
    this->unk_1AA = Rand_ZeroFloat(20000.0f);
    this->unk_1AC = 0;
    this->actionFunc = BossGanon_Player_ChargeBigMagic;
    BossGanon_Player_UpdateAim(this, play);
}

// Vanilla BossGanon_ChargeBigMagic without the arena movement, and with a shorter charge
void BossGanon_Player_ChargeBigMagic(BossGanon* this, PlayState* play) {
    Vec3f sp80;
    Vec3f sp74;
    Vec3f sp68;
    s16 i;

    SkelAnime_Update(&this->skelAnime);

    if (this->unk_1C2 < 3) {
        BossGanon_Player_UpdateAim(this, play);
    }

    switch (this->unk_1C2) {
        case 0:
            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicChargeStartAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBigMagicChargeStartAnim, 0.0f);
                this->unk_1C2 = 1;
            }
            break;

        case 1:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicChargeHoldAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfBigMagicChargeHoldAnim, 0.0f);
                this->unk_1C2 = 2;
                // Vanilla charges for 100 frames. The thresholds below are the vanilla ones, so starting lower
                // only skips the first part of the charge.
                this->timers[0] = 60;
            }
            break;

        case 2:
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_CHARGE_MASIC - SFX_FLAG));
            this->unk_278.x = this->unk_2EC[0].x;
            this->unk_278.y = this->unk_2EC[0].y + 50.0f + 30.0f;
            this->unk_278.z = this->unk_2EC[0].z;

            Math_ApproachF(&this->unk_284, 0.25f, 0.1f, 0.006f);
            Math_ApproachF(&this->unk_288, 255.0f, 1.0f, 255.0f);
            Math_ApproachF(&this->unk_28C, 0.25f, 0.1f, 0.006f);

            if ((this->timers[0] > 20) && (this->timers[0] < 60)) {
                Math_ApproachF(&this->unk_290, 255.0f, 1.0f, 15.0f);
            }

            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicWindupAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBigMagicWindupAnim, 0.0f);
                this->unk_1C2 = 3;
                this->timers[0] = 6;
                this->timers[1] = 15;
                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DARKWAVE));
                break;
            }

            if ((this->timers[0] >= 7) && (this->timers[0] < 26)) {
                if (this->unk_1AC < ARRAY_COUNT(this->unk_294)) {
                    this->unk_1AC++;
                }

                for (i = 0; i < this->unk_1AC; i++) {
                    Math_ApproachF(&this->unk_294[i], 200.0f, 1.0f, 40.0f);
                }
            }

            if (this->timers[0] <= 30) {
                Math_ApproachF(&this->unk_284, 0.4f, 0.5f, 0.017f);
                this->unk_28C = this->unk_284;
                Math_ApproachF(&this->unk_2D0, 45.0f, 0.1f, 10.0f);
                this->lensFlareTimer = 1;
                this->lensFlareMode = 2;
                gCustomLensFlarePos = this->unk_278;
            }

            if ((this->timers[0] <= 47) && (this->timers[0] >= 42)) {
                this->unk_274 = 48 - this->timers[0];
            }

            if (this->timers[0] > 30) {
                sp74.x = 0.0f;
                sp74.y = Rand_ZeroFloat(10.0f) + 150.0f;
                sp74.z = 0.0f;

                Matrix_RotateY(BINANG_TO_RAD(this->actor.yawTowardsPlayer), MTXMODE_NEW);
                Matrix_RotateZ(Rand_ZeroFloat(65536.0f), MTXMODE_APPLY);
                Matrix_MultVec3f(&sp74, &sp68);

                sp80.x = this->unk_278.x + sp68.x;
                sp80.y = this->unk_278.y + sp68.y;
                sp80.z = this->unk_278.z + sp68.z;

                BossGanonEff_SpawnBlackDot(play, &sp80, 20.0f);
            }
            break;

        case 3:
            for (i = 0; i < ARRAY_COUNT(this->unk_294); i++) {
                Math_ApproachF(&this->unk_294[i], 0.0f, 1.0f, 40.0f);
            }

            if (this->timers[0] == 1) {
                sCape->attachLeftArmTimer = 15.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            }

            if (this->timers[0] == 0) {
                Math_ApproachZeroF(&this->unk_284, 1.0f, 0.08f);
                this->unk_28C = this->unk_284;
                Math_ApproachZeroF(&this->unk_2D0, 1.0f, 10.0f);
                Math_ApproachF(&this->unk_278.x, this->unk_1FC.x, 0.5f, 30.0f);
                Math_ApproachF(&this->unk_278.y, this->unk_1FC.y, 0.5f, 30.0f);
                Math_ApproachF(&this->unk_278.z, this->unk_1FC.z, 0.5f, 30.0f);
            }

            if (this->timers[1] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicThrowAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfBigMagicThrowAnim, 0.0f);
                this->unk_1C2 = 4;
                this->unk_288 = 0.0f;
                this->unk_290 = 0.0f;
                this->unk_284 = 0.0f;
                this->unk_28C = 0.0f;
            }
            break;

        case 4:
            if (Animation_OnFrame(&this->skelAnime, 5.0f)) {
                for (i = 0; i < GDF_PLAYER_BIG_MAGIC_COUNT; i++) {
                    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_1FC.x,
                                       this->unk_1FC.y, this->unk_1FC.z, 0, this->actor.yawTowardsPlayer, 0,
                                       GDF_PARAMS_PLAYER_BIG_MAGIC + i);
                }

                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_BIGMASIC));
                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW_BIG));
            }

            if (Animation_OnFrame(&this->skelAnime, 3.0f)) {
                sCape->attachShouldersTimer = 26.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            }

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBigMagicThrowEndAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfBigMagicThrowEndAnim, 0.0f);
                this->unk_1C2 = 5;
            }
            break;

        case 5:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_Player_SetupFloat(this, play);
            }
            break;
    }
}

void BossGanon_Player_SetupPound(BossGanon* this, PlayState* play) {
    // Invasion mode: his attacks cost magic
    if (!GdfInvasion_PayAttack(8)) {
        return;
    }
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->unk_1C2 = 0;
    this->timers[0] = 26;
    this->actionFunc = BossGanon_Player_Pound;
    this->actor.velocity.y = 0.0f;
    this->handLightBallScale = 0.0f;
    this->playerBtnHeld = 0;
}

// Vanilla BossGanon_PoundFloor, relative to the floor under Link
void BossGanon_Player_Pound(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 floorY = this->playerFloorY;
    f32 hover = BossGanon_Player_GetHoverHeight();
    f32 heightTarget;
    // On the ground he hops up and slams down onto his feet (his legs stand, see StandLegs); the floating heights
    // are for levitating, where he rises above his hover height and slams down to just over the floor
    u8 grounded = this->playerGrounded;
    f32 baseY = grounded ? player->actor.world.pos.y : (player->actor.world.pos.y + hover);
    f32 impactY = grounded ? player->actor.world.pos.y : (floorY + 60.0f);
    s16 i;
    Vec3f sp6C;
    Vec3f sp60;
    Vec3f sp54;

    SkelAnime_Update(&this->skelAnime);

    switch (this->unk_1C2) {
        case 0:
            if (this->timers[0] == 5) {
                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_HIT_GND));
            }

            if (this->timers[0] < 14) {
                heightTarget = baseY + 90.0f;
                this->unk_258 += (Rand_ZeroFloat(M_PI / 2) + (M_PI / 2));
                Math_ApproachF(&this->handLightBallScale, 7.0f, 0.5f, 1.0f);
            } else {
                heightTarget = baseY + 40.0f;
            }

            Math_ApproachF(&this->actor.world.pos.y, heightTarget, 0.1f, this->actor.velocity.y);
            Math_ApproachF(&this->actor.velocity.y, 20.0f, 1.0f, 1.0f);

            if (this->timers[0] == 14) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfPoundAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfPoundAnim, 0.0f);
                this->actor.velocity.y = 0.0f;
            }

            if (this->timers[0] == 0) {
                this->unk_1C2 = 1;
                this->actor.velocity.y = 0.0f;
            }
            break;

        case 1:
            sCape->gravity = -1.0f;
            Math_ApproachF(&this->actor.velocity.y, -50.0f, 1.0f, 10.0f);
            this->actor.world.pos.y += this->actor.velocity.y;

            if (this->actor.world.pos.y < impactY) {
                this->actor.world.pos.y = impactY;
                this->unk_1C2 = 2;
                this->timers[0] = 10;
                Actor_RequestQuakeAndRumble(&this->actor, play, 0xA, 0x14);
                this->unk_19C = 35;
                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_HIT_GND_IMP));
                this->handLightBallScale = 0.0f;
                sp60 = this->unk_260;
                sp60.y = floorY;

                for (i = 0; i < 80; i++) {
                    sp6C.x = Rand_CenteredFloat(25.0f);
                    sp6C.y = Rand_ZeroFloat(17.0f);
                    sp6C.z = Rand_CenteredFloat(25.0f);
                    BossGanonEff_SpawnLightRay(play, &sp60, &sp6C, &sZeroVec, Rand_ZeroFloat(300.0f) + 500.0f, 13.0f,
                                               0x1E);
                }
            }
            break;

        case 2:
            if (this->timers[0] == 0) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfPoundEndAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfPoundEndAnim, 0.0f);
                this->unk_1C2 = 3;
                this->unk_19F = 1;
                this->actor.velocity.y = 0.0f;
            }
            break;

        case 3:
        case 4:
            if (grounded) {
                this->actor.world.pos.y = baseY; // he landed on his feet
            } else {
                Math_ApproachF(&this->actor.world.pos.y, baseY, 0.1f, this->actor.velocity.y);
            }
            Math_ApproachF(&this->actor.velocity.y, 20.0f, 1.0f, 1.0f);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                if (this->unk_1C2 == 3) {
                    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp3Anim);
                    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, 0.0f);
                    SkelAnime_Update(&this->skelAnime);
                    sCape->attachShouldersTimer = 18.0f;
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                    this->unk_1C2 = 4;
                } else {
                    BossGanon_Player_SetupFloat(this, play);
                }
            }
            break;
    }

    if ((this->unk_19C == 35) || (this->unk_19C == 30) || (this->unk_19C == 25)) {
        sp54 = this->actor.world.pos;
        sp54.y = floorY;
        BossGanonEff_SpawnDustLight(play, &sp54, 0, 3.0f, this->unk_19C - 25);
    }

    if (this->unk_19C == 35) {
        sp54 = this->actor.world.pos;
        sp54.y = floorY;
        BossGanonEff_SpawnShockwave(play, &sp54, 0, 3.0f);
    }

    // The shockwave hurts enemies around Ganondorf in two pulses as it spreads
    if ((this->unk_19C == 35) || (this->unk_19C == 27)) {
        this->collider.dim.radius = (this->unk_19C == 35) ? 130 : 280;
        this->collider.dim.height = 80;
        this->collider.dim.yShift = 0;
        this->collider.dim.pos.x = this->actor.world.pos.x;
        this->collider.dim.pos.y = floorY - 20.0f;
        this->collider.dim.pos.z = this->actor.world.pos.z;
        BossGanon_Player_SetAT(this, play);
    }
}

// The cape swats the real Ganondorf's light balls and Big Magic back at him (the tennis from his fight)
static void BossGanon_Player_DeflectMagic(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f linkCenter = player->actor.world.pos;
    Actor* actor;
    s16 i;

    linkCenter.y += 40.0f;

    for (actor = play->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        BossGanon* ball = (BossGanon*)actor;
        BossGanon* dorf = (BossGanon*)actor->parent;
        f32 dist;
        u8 deflected = false;

        if ((actor->id != ACTOR_BOSS_GANON) || (actor->update == NULL) || (dorf == NULL) || (dorf == this) ||
            (dorf->actor.id != ACTOR_BOSS_GANON) || GDF_IS_PLAYER(dorf)) {
            continue;
        }

        dist = Math_Vec3f_DistXYZ(&actor->world.pos, &linkCenter);
        if (dist > 90.0f) {
            dist = Math_Vec3f_DistXYZ(&actor->world.pos, &this->unk_1FC);
        }
        if (dist > 90.0f) {
            continue;
        }

        if ((actor->params >= 0x64) && (actor->params < 0xC8) && (ball->unk_1A8 == 0) && (ball->unk_1C2 == 0)) {
            // Light ball: the same as a sword volley in BossGanon_LightBall_Update
            f32 dx = dorf->unk_1FC.x - actor->world.pos.x;
            f32 dy = dorf->unk_1FC.y - actor->world.pos.y;
            f32 dz = dorf->unk_1FC.z - actor->world.pos.z;

            actor->world.rot.y = Math_Atan2S(dz, dx);
            actor->world.rot.x = Math_Atan2S(sqrtf(SQ(dx) + SQ(dz)), dy);
            ball->unk_1A4++;
            ball->timers[1] = 2;

            if (((dorf->actor.xyzDistToPlayerSq > 62500.0f) && (ball->unk_1A4 < 3)) || (Rand_ZeroOne() < 0.7f)) {
                ball->unk_1C2 = 1;
            } else {
                ball->unk_1C2 = 3;
            }
            deflected = true;
        } else if ((actor->params >= 0x104) && (actor->params < 0x109) && (ball->unk_1C2 == 11)) {
            // Big Magic: sent straight back at him, like a spin attack does
            ball->unk_1C2 = 12;
            actor->speedXZ = -30.0f;
            Actor_UpdateVelocityXYZ(actor);
            Actor_UpdatePos(actor);
            ball->unk_1F0 = dorf->unk_1FC;
            deflected = true;
        }

        if (deflected) {
            Vec3f vel;

            Audio_PlayActorSound2(actor, NA_SE_IT_SWORD_REFLECT_MG);
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN)); // "Hah!"
            sCape->attachLeftArmTimer = 10.0f;

            for (i = 0; i < 30; i++) {
                vel.x = Rand_CenteredFloat(25.0f);
                vel.y = Rand_CenteredFloat(25.0f);
                vel.z = Rand_CenteredFloat(25.0f);
                BossGanonEff_SpawnLightRay(play, &actor->world.pos, &vel, &sZeroVec, Rand_ZeroFloat(100.0f) + 300.0f,
                                           10.0f, 0x1E);
            }
        }
    }
}

// The cape is up (the start of the block, before it's released)
s32 BossGanon_Player_IsCapeUp(Actor* thisx) {
    BossGanon* this = (BossGanon*)thisx;

    return (thisx != NULL) && GDF_IS_PLAYER(this) && !this->playerHidden &&
           (((this->actionFunc == BossGanon_Player_Block) && (this->unk_1C2 == 0)) ||
            BossGanon_Player_IsSwordGuarding(this));
}

// While the cape is up it works as Link's shield, for everything that checks for one: a large hard shield
// hitbox in front of Ganondorf (projectiles and attacks bounce off it) and the shield's position and angle
// (where reflected nuts, rocks, Twinrova's magic and mirror light go). Called right after Link's update, because
// Link's own update resets his shield collider.
void BossGanon_Player_UpdateCapeShield(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;
    f32 sinYaw = Math_SinS(yaw);
    f32 cosYaw = Math_CosS(yaw);
    f32 frontX = player->actor.world.pos.x + (sinYaw * 30.0f);
    f32 frontZ = player->actor.world.pos.z + (cosYaw * 30.0f);
    // Sideways, perpendicular to the facing direction
    f32 sideX = cosYaw * 55.0f;
    f32 sideZ = -sinYaw * 55.0f;
    // From Link's feet (what enemies aim at) to above Ganondorf's head
    f32 bottom = player->actor.world.pos.y - 10.0f;
    f32 top = this->actor.world.pos.y + 70.0f;
    Vec3f topLeft;
    Vec3f topRight;
    Vec3f bottomLeft;
    Vec3f bottomRight;

    if (top < bottom + 40.0f) {
        top = bottom + 40.0f;
    }

    topLeft.x = bottomLeft.x = frontX - sideX;
    topLeft.z = bottomLeft.z = frontZ - sideZ;
    topRight.x = bottomRight.x = frontX + sideX;
    topRight.z = bottomRight.z = frontZ + sideZ;
    topLeft.y = topRight.y = top;
    bottomLeft.y = bottomRight.y = bottom;

    player->shieldQuad.base.colType = COLTYPE_METAL;
    Collider_SetQuadVertices(&player->shieldQuad, &topLeft, &topRight, &bottomLeft, &bottomRight);
    CollisionCheck_SetAC(play, &play->colChkCtx, &player->shieldQuad.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &player->shieldQuad.base);

    // Like Link's shield it faces back at him; reflections go out the other way
    Matrix_Push();
    Matrix_Translate(frontX, this->unk_1FC.y + 30.0f, frontZ, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((s16)(yaw + 0x8000)), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(this->playerShieldPitch), MTXMODE_APPLY);
    Matrix_Get(&player->shieldMf);
    Matrix_Pop();
}

void BossGanon_Player_SetupBlock(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBlockAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBlockAnim, 0.0f);
    this->actionFunc = BossGanon_Player_Block;
    this->unk_1C2 = 0;
    sCape->attachLeftArmTimer = this->timers[0] = 10;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
    this->handLightBallScale = 0.0f;
    this->playerShieldPitch = 0;

    // Turn to face the closest enemy (a Deku Scrub, say) so the cape sends its shots straight back
    BossGanon_Player_UpdateAim(this, play);
    player->actor.shape.rot.y = player->yaw = player->actor.world.rot.y = this->actor.yawTowardsPlayer;
}

void BossGanon_Player_Block(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];

    SkelAnime_Update(&this->skelAnime);
    sCape->backPush = -9.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -2.0f;
    sCape->minDist = 13.0f;

    if (this->unk_1C2 == 0) {
        BossGanon_Player_DeflectMagic(this, play);

        // Ganondorf's cape guard: Link can't be hurt or knocked back while it's up (intangibility)
        if ((player->invincibilityTimer >= 0) && (player->invincibilityTimer < 2)) {
            player->invincibilityTimer = 2;
        }

        if (CHECK_BTN_ALL(input->cur.button, BTN_R) && BossGanon_Player_CanStartAttack(this, play)) {
            // Aim the cape like Link aims his shield: stick left/right turns, up/down tilts
            s16 targetPitch = (s16)(input->rel.stick_y * 200);

            player->actor.shape.rot.y -= (s16)(input->rel.stick_x * 40);
            player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y;
            this->actor.yawTowardsPlayer = player->actor.shape.rot.y;
            Math_ScaledStepToS(&this->playerShieldPitch, targetPitch, 0x800);

            if (this->timers[0] < 2) {
                this->timers[0] = 2;
            }
            sCape->attachLeftArmTimer = 10.0f;

            if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
                BossGanon_Player_SetupPound(this, play);
                return;
            }
        }

        if (this->timers[0] == 0) {
            this->unk_1C2 = 1;
            Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfBlockReleaseAnim, 0.0f);
            this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfBlockReleaseAnim);
            SkelAnime_Update(&this->skelAnime);
            sCape->attachShouldersTimer = 15.0f;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN)); // "Hah!"
        }
    } else {
        sCape->sideSwayMagnitude = -13.0f;

        if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
            BossGanon_Player_SetupFloat(this, play);
        }
    }
}

static void BossGanon_Player_CancelMagic(BossGanon* this) {
    this->handLightBallScale = 0.0f;
    this->unk_284 = 0.0f;
    this->unk_288 = 0.0f;
    this->unk_28C = 0.0f;
    this->unk_290 = 0.0f;
    this->unk_2D0 = 0.0f;
    this->unk_1AC = 0;
    this->unk_274 = 0;
    this->unk_26C = 0;
    this->lensFlareTimer = 0;
    this->playerBtnHeld = 0;
    sCape->attachRightArmTimer = sCape->attachLeftArmTimer = 0.0f;
}

void BossGanon_Player_SetupHurt(BossGanon* this, PlayState* play) {
    s16 i;

    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    BossGanon_Player_CancelMagic(this);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDamageAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDamageAnim, 0.0f);
    this->actionFunc = BossGanon_Player_Hurt;
    this->unk_1A6 = 15; // red flash

    // A short burst of the yellow shock he gets when hit by his own light ball
    for (i = 1; i < 15; i++) {
        this->unk_4E4[i] = D_808E4C58[i];
    }
    this->unk_2E6 = 30;
    this->unk_2E8 = 0;
    this->unk_508 = 3.0f;

    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DAMAGE1));
}

void BossGanon_Player_Hurt(BossGanon* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
        BossGanon_Player_SetupFloat(this, play);
    }
}

void BossGanon_Player_SetupDowned(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    BossGanon_Player_CancelMagic(this);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLightArrowHitAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfLightArrowHitAnim, 0.0f);
    this->actionFunc = BossGanon_Player_Downed;
    this->unk_1C2 = 0;
    this->actor.velocity.y = 0.0f;
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DAMAGE2));
}

static s32 BossGanon_Player_IsLinkDead(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return (gSaveContext.health <= 0) || (player->stateFlags1 & PLAYER_STATE1_DEAD);
}

// Falls to the floor when Link dies, gets back up if a fairy revives him
void BossGanon_Player_Downed(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 floorY = this->playerFloorY;

    SkelAnime_Update(&this->skelAnime);
    sCape->minDist = 0.0f;

    switch (this->unk_1C2) {
        case 0:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 1;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDownedAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDownedAnim, 0.0f);
            }
            break;

        case 1:
            this->actor.velocity.y -= 0.5f;
            this->actor.world.pos.y += this->actor.velocity.y;

            if (this->actor.world.pos.y < (floorY + 40.0f)) {
                this->actor.world.pos.y = floorY + 40.0f;
                this->actor.velocity.y = 0.0f;
                this->unk_1C2 = 2;
                this->unk_19F = 1;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfLandAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfLandAnim, 0.0f);
            }
            break;

        case 2:
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                this->unk_1C2 = 3;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfVulnerableAnim);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfVulnerableAnim, 0.0f);
            }
            break;

        case 3:
            this->actor.world.pos.y = floorY + 40.0f;

            if (!BossGanon_Player_IsLinkDead(play)) {
                this->unk_1C2 = 4;
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfGetUp3Anim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfGetUp3Anim, 0.0f);
                SkelAnime_Update(&this->skelAnime);
                sCape->attachShouldersTimer = 18.0f;
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_MANTLE);
                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_RESTORE));
            }
            break;

        case 4:
            sCape->minDist = 20.0f;
            Math_ApproachF(&this->actor.world.pos.y,
                           player->actor.world.pos.y + BossGanon_Player_GetHeightAboveLink(this), 0.1f, 10.0f);

            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_Player_SetupFloat(this, play);
            }
            break;
    }
}

static void BossGanon_Player_Update_Impl(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];
    f32 legRotX;
    f32 legRotY;
    f32 legRotZ;
    f32 sin;
    f32 cos;
    f32 targetLensFlareScale;
    f32 xOffset;
    f32 zOffset;
    Vec3f spE8;
    s16 i;
    u8 controlY;

    if (sCape == NULL) {
        Actor_Kill(thisx);
        return;
    }

    BossGanon_Player_SlayNpcs(this, play);
    BossGanon_Player_UpdateManhunt(this, play);

    // The engine sets yawTowardsPlayer to the direction of Link every frame, but Link stands right under him, so that
    // direction is meaningless and he'd turn to it mid-attack. Restore the aim from the last update.
    this->actor.yawTowardsPlayer = this->playerAimYaw;

    sCape->gravity = -3.0f;
    this->shockGlow = false;
    this->unk_1A2++;
    this->unk_1A4++;

    // The floor under Link stands in for the arena floor
    if (player->actor.floorHeight > (BGCHECK_Y_MIN + 1.0f)) {
        this->playerFloorY = player->actor.floorHeight;
    } else {
        this->playerFloorY = player->actor.world.pos.y;
    }
    // Walking about on his own in a cutscene: the floor under him instead
    BossGanon_Player_UpdateRoam(this, play);
    if (sGdfRoaming) {
        this->playerFloorY = sGdfRoamFloorY;
    }
    // The cape's hem rests on the floor under Link. Raising that floor gradually keeps the hem from jittering on
    // slopes and stairs; lowering it is instant, so the cape never gets pushed up when he drops off a ledge.
    {
        f32 capeFloor = this->playerFloorY + 2.0f;

        if ((capeFloor < sCape->minY) || (capeFloor > sCape->minY + 60.0f)) {
            sCape->minY = capeFloor;
        } else {
            Math_ApproachF(&sCape->minY, capeFloor, 0.3f, 4.0f);
        }
    }

    // On the ground he stands and walks; he only floats while levitating
    this->playerGrounded = CVarGetInteger(CVAR_PLAY_AS_GDF("WalkOnGround"), 1) && !PlayAsGanondorf_IsLevitating();

    // Jumps: remember the highest point, and land heavily after a big drop
    if (this->playerGrounded && BossGanon_Player_IsJumping(play)) {
        if (!this->playerWasAirborne) {
            this->playerWasAirborne = true;
            this->playerJumpTopY = player->actor.world.pos.y;
        }
        this->playerJumpTopY = MAX(this->playerJumpTopY, player->actor.world.pos.y);
    } else if (this->playerWasAirborne) {
        f32 drop = this->playerJumpTopY - player->actor.world.pos.y;

        this->playerWasAirborne = false;
        if (this->playerGrounded && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !this->playerHidden &&
            (drop > GDF_BIG_LANDING_HEIGHT) &&
            ((this->actionFunc == BossGanon_Player_Float) || (this->actionFunc == BossGanon_Player_SwordIdle))) {
            BossGanon_Player_SetupLand(this, play, drop);
        }
    }

    // Stepping or pulling himself up onto a ledge
    if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LEDGE) && !(player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) &&
        (this->actionFunc != BossGanon_Player_LedgeClimb) && (this->actionFunc != BossGanon_Player_Downed) &&
        (this->actionFunc != BossGanon_Player_Ride) && this->playerGrounded && !this->playerHidden) {
        BossGanon_Player_SetupLedgeClimb(this, play);
    }

    // Climbing a ladder or a vine wall
    if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) && (this->actionFunc != BossGanon_Player_Climb) &&
        (this->actionFunc != BossGanon_Player_Downed) && !this->playerHidden) {
        BossGanon_Player_ReleaseOrganCam(this, play);
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -5.0f);
        this->actionFunc = BossGanon_Player_Climb;
        this->playerWalkWeight = 0.0f;
        this->playerStandLegsWeight = 0.0f;
    }

    // Holding up something he found (a chest, a heart piece, ...), like Link does
    if ((player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) && (player->unk_862 != 0) &&
        (this->actionFunc != BossGanon_Player_ItemGet) && (this->actionFunc != BossGanon_Player_Downed) &&
        (this->actionFunc != BossGanon_Player_Ride) && !this->playerHidden) {
        BossGanon_Player_ReleaseOrganCam(this, play);
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -5.0f);
        this->actionFunc = BossGanon_Player_ItemGet;
        this->playerItemWeight = 0.0f;
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
    }

    // On his horse (or the Gerudo intro's)
    if (((player->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (gGdfIntroCs.active && (gGdfIntroCs.horse != NULL))) &&
        (this->actionFunc != BossGanon_Player_Ride) &&
        (this->actionFunc != BossGanon_Player_Downed)) {
        Vec3f seat;

        if (BossGanon_Player_GetSeat(play, &seat)) {
            BossGanon_Player_ReleaseOrganCam(this, play);
            BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
            Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -5.0f);
            this->actionFunc = BossGanon_Player_Ride;
            this->playerRideFrame = 0.0f;
            this->playerWalkWeight = 0.0f;
            this->playerStandLegsWeight = 0.0f;
        }
    }

    // The organ is his ocarina: when Link plays it, Ganondorf plays the organ
    if ((player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING) && PlayAsGanondorf_UseOrganOcarina() &&
        (this->actionFunc != BossGanon_Player_Organ) && (this->actionFunc != BossGanon_Player_Downed)) {
        BossGanon_Player_StopOrganMusic(this, play);
        BossGanon_Player_SetupOrgan(this, play);
        this->playerOrganOcarina = true;
        // Straight into playing: the first notes come right away
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfPlayOrganAnim, -5.0f);
    } else if (this->actionFunc != BossGanon_Player_Organ) {
        this->playerOrganOcarina = false;
    }

    this->playerHidden = BossGanon_Player_ShouldHide(this, play) || GdfInvasion_IsVanished();
    if (sCape->actor.update != NULL) {
        sCape->actor.draw = this->playerHidden ? NULL : sPlayerCapeDraw;
    }

    // Track how long the attack button is held (to charge Big Magic)
    if (this->playerBtnHeld != 0) {
        if (CHECK_BTN_ALL(input->cur.button, this->playerBtnHeld)) {
            this->playerHeldTimer++;
        } else {
            this->playerBtnHeld = 0;
        }
    }

    if (sGdfGrabState != GDF_GRAB_NONE) {
        sGdfGrabBusyFrame = play->gameplayFrames;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        sGdfLastBFrame = play->gameplayFrames; // (the volley)
    }
    // Invasion mode: pacified
    if (GdfInvasion_IsPacified()) {
        if (this->actionFunc != BossGanon_Player_Pacified) {
            BossGanon_Player_SetupPacified(this, play);
        }
    } else if (BossGanon_Player_IsLinkDead(play)) {
        if (this->actionFunc != BossGanon_Player_Downed) {
            BossGanon_Player_SetupDowned(this, play);
        }
    } else if ((player->invincibilityTimer > 0) && (this->playerPrevInvincibilityTimer <= 0) &&
               (this->actionFunc != BossGanon_Player_Downed)) {
        BossGanon_Player_SetupHurt(this, play);
    }

    this->actionFunc(this, play);
    BossGanon_Player_UpdateGrabbed(this, play);

    {
        // The dark aura handles his legs itself (it walks)
        u8 standLegs = (this->actionFunc != BossGanon_Player_DarkAura) && (this->actionFunc != BossGanon_Player_Climb) &&
                       this->playerGrounded && !this->playerHidden &&
                       ((this->actionFunc == BossGanon_Player_Block) ||
                        (this->actionFunc == BossGanon_Player_ChargeLightBall) ||
                        (this->actionFunc == BossGanon_Player_Throw) ||
                        (this->actionFunc == BossGanon_Player_ChargeBigMagic) ||
                        (this->actionFunc == BossGanon_Player_Laugh) || (this->actionFunc == BossGanon_Player_Hurt) ||
                        (this->actionFunc == BossGanon_Player_Pound));

        // In fast (the morph into the action already smooths it); out over ~13 frames, longer than the 10-frame
        // morph back to his standing idle, so the floating root height never shows
        Math_ApproachF(&this->playerStandLegsWeight, standLegs ? 1.0f : 0.0f, 1.0f, standLegs ? 0.5f : 0.08f);
        if ((this->actionFunc == BossGanon_Player_DarkAura) || (this->actionFunc == BossGanon_Player_Climb)) {
            this->playerStandLegsWeight = 0.0f;
        } else if (this->playerGrounded) {
            BossGanon_Player_StandLegs(this);
        } else {
            this->playerStandLegsWeight = 0.0f;
        }
    }

    // The Gerudo intro's direction: his arm pose, head and hand
    BossGanon_Player_ApplyIntroCs(this, play);

    // His taunt (D-pad Up) works in cutscenes and conversations too, where his other moves don't
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_DUP) && !this->playerHidden &&
        !GanondorfSongs_IsOcarinaOut() && !gGdfIntroCs.active &&
        ((player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING)) ||
         (play->csCtx.state != CS_STATE_IDLE)) &&
        ((this->actionFunc == BossGanon_Player_Float) || (this->actionFunc == BossGanon_Player_SwordIdle) ||
         (this->actionFunc == BossGanon_Player_Brood))) {
        BossGanon_Player_SetupLaugh(this, play);
    }

    // The opening in Link's house: brooding against the wall while the cutscene runs (a taunt can interrupt it)
    if (BossGanon_Player_ShouldBrood(play) && !this->playerHidden && !sGdfRoaming) {
        if ((this->actionFunc != BossGanon_Player_Brood) && (this->actionFunc != BossGanon_Player_Laugh)) {
            BossGanon_Player_SetupBrood(this, play);
            BossGanon_Player_Brood(this, play);
        }
    } else if (this->actionFunc == BossGanon_Player_Brood) {
        BossGanon_Player_SetupFloat(this, play);
    }

    // Anything that interrupts the throw (getting hurt, cutscenes, ...) puts the trident back in his hand
    if ((sGdfTridentState != GDF_TRIDENT_IN_HAND) && (this->actionFunc != BossGanon_Player_TridentSpin) &&
        (this->actionFunc != BossGanon_Player_TridentThrow)) {
        sGdfTridentState = GDF_TRIDENT_IN_HAND;
    }

    // He kicks doors open
    BossGanon_Player_ApplyDoorKick(this, play);
    BossGanon_Player_ApplyChestKick(this, play);
    BossGanon_Player_MasterSwordKick(this, play);
    BossGanon_Player_ApplyBreakKick(this, play);
    BossGanon_Player_ApplyDoorDuck(this, play);
    BossGanon_Player_UpdateBodyguards(this, play);

    // Into and out of a crawlspace: a puff of dark smoke as he turns into the flame and back
    {
        u8 flame = PlayAsGanondorf_IsCrawlFlame(play, player);

        if (flame != this->playerCrawlFlame) {
            Vec3f zero = { 0.0f, 0.0f, 0.0f };
            Vec3f pos = player->actor.world.pos;

            pos.y += 20.0f;
            EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 150, 0, 200, 120, 255, 255, 60, 0, 120, 1, 9, false);
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
            this->playerCrawlFlame = flame;
        }
    }

    // Link isn't drawn, so his body part positions (used by enemies for grabs, effects and aiming) would go stale.
    // Use Ganondorf's from his last draw instead.
    if (!this->playerHidden) {
        // Ganondorf's body parts (unk_2EC): 0 chest (torso + 30), 1 torso, 2 pelvis, 3-5 one arm, 6-8 the other,
        // 9-11 one leg, 12-14 the other. His head is captured on its own (sGdfPlayerHeadPos).
        static const u8 sLinkToDorfBodyPart[] = {
            2,          // waist: his pelvis
            12, 13, 14, // right leg
            9,  10, 11, // left leg
            0,  0,      // head, hat (replaced by his head below)
            0,          // collar
            3,  4,  5,  // left arm
            6,  7,  8,  // right arm
            2,  1,      // sheath, torso
        };

        for (i = 0; (i < ARRAY_COUNT(sLinkToDorfBodyPart)) && (i < ARRAY_COUNT(player->bodyPartsPos)); i++) {
            player->bodyPartsPos[i] = this->unk_2EC[sLinkToDorfBodyPart[i]];
        }
        // His head, not his hips: Navi/the Poe hovers by the hat, enemies aim at the focus, NPCs look at it
        if (sGdfPlayerHeadValid) {
            player->bodyPartsPos[PLAYER_BODYPART_HEAD] = sGdfPlayerHeadPos;
            player->bodyPartsPos[PLAYER_BODYPART_HAT] = sGdfPlayerHeadPos;
            player->bodyPartsPos[PLAYER_BODYPART_HAT].y += 15.0f;
            player->actor.focus.pos = sGdfPlayerHeadPos;
        }
    }

    // After the action, so the block's own intangibility doesn't count as getting hurt
    this->playerPrevInvincibilityTimer = player->invincibilityTimer;

    if (this->playerGrounded && (this->actionFunc != BossGanon_Player_Climb) &&
        (this->actionFunc != BossGanon_Player_LedgeClimb)) {
        BossGanon_Player_UpdateGroundOffset(this, play);
    }
    BossGanon_Player_UpdateSwordHit(this, play);

    controlY = (this->actionFunc != BossGanon_Player_Pound) && (this->actionFunc != BossGanon_Player_Downed) &&
               (this->actionFunc != BossGanon_Player_Organ);

    // The organ sinks back into the ground once he stops playing, and the area music comes back
    if (this->actionFunc != BossGanon_Player_Organ) {
        Math_ApproachZeroF(&this->actor.shape.yOffset, 0.3f, 700.0f);
        BossGanon_Player_StopOrganMusic(this, play);
        BossGanon_Player_ReleaseOrganCam(this, play);
        if (this->playerOrganSink < GDF_ORGAN_SINK_DEPTH) {
            // Silently (no more grinding sound, it kept looping)
            Math_ApproachF(&this->playerOrganSink, GDF_ORGAN_SINK_DEPTH, 1.0f, 25.0f);
        }
    }

    // Triforce of Power on his hand, from the laugh
    if ((this->actionFunc != BossGanon_Player_Laugh) && (this->actionFunc != BossGanon_Player_DarkAura)) {
        Math_ApproachZeroF(&this->fwork[GDF_TRIFORCE_PRIM_A], 1.0f, 25.0f);
    }
    // The aura fades once he lets go
    if (this->actionFunc != BossGanon_Player_DarkAura) {
        Math_ApproachZeroF(&this->playerAuraAlpha, 1.0f, 20.0f);
        Math_ApproachZeroF(&this->playerAuraScale, 1.0f, 0.02f);
    }
    if (sGdfRoaming) {
        // Where he walked to, standing on the floor there (the ground offset is relative to Link's height)
        this->actor.world.pos.y = player->actor.world.pos.y + this->playerGroundOffset;
    } else {
        BossGanon_Player_Follow(this, play, controlY);
    }

    if ((this->actionFunc == BossGanon_Player_SwordAttack) && (this->playerSwordAttack == GDF_SWORD_ATK_SPIN) &&
        (this->playerSpinYaw != 0)) {
        this->actor.shape.rot.y = this->playerDashYaw + this->playerSpinYaw;
    } else if (BossGanon_Player_IsBusy(thisx)) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x1000);
    } else if (this->actionFunc != BossGanon_Player_Downed) {
        Math_ApproachS(&this->actor.shape.rot.y, player->actor.shape.rot.y, 2, 0x2000);
        // Between attacks his aim is wherever Link faces, so an attack that doesn't aim itself goes straight ahead
        this->actor.yawTowardsPlayer = player->actor.shape.rot.y;
    }
    if (sGdfRoaming) {
        Math_ApproachS(&this->actor.shape.rot.y, sGdfRoamYaw, 3, 0x1800);
        this->actor.yawTowardsPlayer = this->actor.shape.rot.y;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->playerAimYaw = this->actor.yawTowardsPlayer;
    if ((this->actionFunc == BossGanon_Player_Brood) ||
        ((this->actionFunc == BossGanon_Player_Laugh) && BossGanon_Player_ShouldBrood(play) && !sGdfRoaming)) {
        BossGanon_Player_PlaceBrood(this, play);
    }

    // Used for the leg sway
    this->actor.velocity.x = this->actor.world.pos.x - this->actor.prevPos.x;
    this->actor.velocity.z = this->actor.world.pos.z - this->actor.prevPos.z;
    if (controlY) {
        this->actor.velocity.y = this->actor.world.pos.y - this->actor.prevPos.y;
    }

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    if (this->unk_1A6 != 0) {
        this->unk_1A6--;
    }

    if (this->unk_2D4 != 0) {
        this->unk_2D4--;
    }

    if (this->unk_2E8 != 0) {
        this->unk_2E8--;
    }

    if (this->unk_2E6 != 0) {
        this->unk_2E6--;
    }

    if (this->unk_19C != 0) {
        this->unk_19C--;
    }

    // Leg sway, from BossGanon_Update. Only while floating: on the ground it bent his standing legs into the floor
    if (this->legSwayEnabled && !this->playerGrounded) {
        f32 velX = CLAMP(this->actor.velocity.x, -6.0f, 6.0f);
        f32 velZ = CLAMP(this->actor.velocity.z, -6.0f, 6.0f);

        sin = Math_SinS(-this->actor.shape.rot.y);
        cos = Math_CosS(-this->actor.shape.rot.y);

        legRotX = ((velZ * sin) + (cos * velX)) * 300.0f;
        legRotY = ((-sin * velX) + (cos * velZ)) * 300.0f;
        legRotZ = (Math_SinS(this->unk_1A2 * 2268) * -500.0f) - 500.0f;
    } else {
        legRotY = legRotX = legRotZ = 0.0f;
    }

    this->legSwayEnabled = false;

    Math_SmoothStepToF(&this->legRot.x, legRotX, 1.0f, 600.0f, 0.0f);
    Math_SmoothStepToF(&this->legRot.y, legRotY, 1.0f, 600.0f, 0.0f);
    Math_SmoothStepToF(&this->legRot.z, legRotZ, 1.0f, 100.0f, 0.0f);

    // Electric shock on Ganondorf's body
    if (this->unk_2E6 != 0) {
        for (i = 1; i < ARRAY_COUNT(this->unk_49C); i++) {
            if (this->unk_4E4[i] != 0) {
                this->unk_4E4[i]--;
                Math_ApproachF(&this->unk_49C[i], this->unk_508, 1.0f, 2.0f);
            } else {
                Math_ApproachZeroF(&this->unk_49C[i], 1.0f, 0.2f);
            }
        }
    }

    // Dust when landing
    if (this->unk_19F != 0) {
        this->unk_19F = 0;
        spE8 = this->actor.world.pos;
        spE8.y = this->playerFloorY;
        BossGanonEff_SpawnDustDark(play, &spE8, 0.2, 0.7f);
        BossGanonEff_SpawnDustDark(play, &spE8, 0.3f, 0.8f);
    }

    // Lightning while charging a light ball
    if (this->unk_26C != 0) {
        this->unk_26C--;

        if (this->unk_26C == 0) {
            BossGanonEff_SpawnLightning(play, 1.0f, 0.0f, 0.0f);
        }

        BossGanonEff_SpawnLightning(play, 1.0f, D_808E4D44[this->unk_26C] * (M_PI / 5) + this->unk_270,
                                    Rand_CenteredFloat(M_PI / 5) + (M_PI / 2));
    }

    BossGanon_UpdateEffects(play);

    // Lens flare, from BossGanon_Update
    if (this->lensFlareTimer != 0) {
        this->lensFlareTimer--;

        if (this->lensFlareMode == 1) {
            targetLensFlareScale = 40.0f;
        } else if (this->lensFlareMode == 4) {
            targetLensFlareScale = 25.0f;
        } else {
            targetLensFlareScale = 10.0f;
        }

        Math_ApproachF(&this->lensFlareScale, targetLensFlareScale, 0.3f, 10.0f);
    } else {
        Math_ApproachZeroF(&this->lensFlareScale, 1.0f, 5.0f);

        if (this->lensFlareScale == 0.0f) {
            this->lensFlareMode = 0;
        }
    }

    if ((this->lensFlareMode != 0) && !this->playerHidden) {
        gCustomLensFlareOn = true;
        sPlayerLensFlareOn = true;

        if (this->lensFlareMode == 1) {
            gCustomLensFlarePos = this->actor.world.pos;
        }

        gLensFlareScale = this->lensFlareScale;
        gLensFlareColorIntensity = 10.0f;
        gLensFlareScreenFillAlpha = 0;
    } else if (sPlayerLensFlareOn) {
        gCustomLensFlareOn = false;
        sPlayerLensFlareOn = false;
    }

    // Light balls that fly into the Big Magic charge
    if (this->unk_274 != 0) {
        i = this->unk_274 - 1;

        this->unk_278.x = this->unk_2EC[0].x;
        this->unk_278.y = this->unk_2EC[0].y + 50.0f + 30.0f;
        this->unk_278.z = this->unk_2EC[0].z;

        xOffset = (sinf(i * 1.2566371f) * 600.0f);
        zOffset = (cosf(i * 1.2566371f) * 600.0f);

        Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON, this->unk_1FC.x + xOffset,
                           this->unk_1FC.y, this->unk_1FC.z + zOffset, 0, (s16)(i * 13107.2f) + 0x6000, 0, 0xFA + i);
        this->unk_274 = 0;
    }
}

// Ganondorf's light ball, thrown by the player. Flies straight and bursts on enemies, walls and floors.

// Points the ball at a spot (NULL: the nearest Link) at its speed
static void BossGanon_PlayerBall_Aim(BossGanon* this, PlayState* play, Vec3f* at) {
    Vec3f target;
    f32 dx;
    f32 dy;
    f32 dz;

    if (at == NULL) {
        Actor* best = NULL;
        f32 bestDist = 3000.0f;
        Actor* actor;

        for (actor = play->actorCtx.actorLists[ACTORCAT_NPC].head; actor != NULL; actor = actor->next) {
            f32 d;
            if ((actor->update == NULL) || (GdfInvasion_DummyClientId(actor) == 0)) {
                continue;
            }
            d = Math_Vec3f_DistXYZ(&this->actor.world.pos, &actor->world.pos);
            if (d < bestDist) {
                bestDist = d;
                best = actor;
            }
        }
        if (best == NULL) {
            return;
        }
        target = best->world.pos;
        target.y += 40.0f;
    } else {
        target = *at;
    }
    dx = target.x - this->actor.world.pos.x;
    dy = target.y - this->actor.world.pos.y;
    dz = target.z - this->actor.world.pos.z;
    this->actor.world.rot.y = Math_Atan2S(dz, dx);
    this->actor.world.rot.x = RADF_TO_BINANG(atan2f(dy, sqrtf(SQ(dx) + SQ(dz))));
}

static s32 BossGanon_PlayerBall_NearLink(BossGanon* this, PlayState* play) {
    Actor* actor;

    if (!GdfInvasion_CanHurtLinks()) {
        return false;
    }
    for (actor = play->actorCtx.actorLists[ACTORCAT_NPC].head; actor != NULL; actor = actor->next) {
        if ((actor->update != NULL) && (GdfInvasion_DummyClientId(actor) != 0) &&
            (Math_Vec3f_DistXYZ(&this->actor.world.pos, &actor->world.pos) < 180.0f)) {
            return true;
        }
    }
    return false;
}

// lightOnly: 1 his light balls, 0 those and his Big Magic
static BossGanon* BossGanon_Player_FindBallNear(Vec3f* pos, s32 lightOnly) {
    PlayState* play = gPlayState;
    BossGanon* best = NULL;
    f32 bestDist = 250.0f;
    Actor* actor;
    s32 i;

    if (play == NULL) {
        return NULL;
    }
    for (i = 0; i < ACTORCAT_MAX; i++) {
        for (actor = play->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            f32 d;
            if ((actor->id != ACTOR_BOSS_GANON) || (actor->update == NULL)) {
                continue;
            }
            if (actor->params == GDF_PARAMS_PLAYER_LIGHT_BALL) {
                if (lightOnly < 0) {
                    continue;
                }
                if (((BossGanon*)actor)->unk_1A8 != 0) {
                    continue; // bursting
                }
            } else if ((actor->params >= GDF_PARAMS_PLAYER_BIG_MAGIC) &&
                       (actor->params < GDF_PARAMS_PLAYER_BIG_MAGIC + GDF_PLAYER_BIG_MAGIC_COUNT)) {
                if ((lightOnly > 0) || (((BossGanon*)actor)->unk_1C2 == 1)) {
                    continue;
                }
            } else {
                continue;
            }
            d = Math_Vec3f_DistXYZ(pos, &actor->world.pos);
            if (d < bestDist) {
                bestDist = d;
                best = (BossGanon*)actor;
            }
        }
    }
    return best;
}

// A Link sent it back: at Ganondorf it goes
void BossGanon_Player_BallVolleyed(Vec3f* ballPos) {
    BossGanon* ball = BossGanon_Player_FindBallNear(ballPos, 1);

    if ((ball == NULL) || (ball->unk_1A6 != 0)) {
        return;
    }
    ball->unk_1A6 = 1;
    ball->unk_1A4++;
    ball->actor.speedXZ = MIN(ball->actor.speedXZ * 1.1f, 40.0f);
    ball->timers[2] = 90;
    SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &ball->actor.world.pos, 30, NA_SE_IT_SWORD_REFLECT_MG);
}

// A Link missed it (his game has hurt him): it bursts
void BossGanon_Player_BallBurstAt(Vec3f* ballPos) {
    BossGanon* ball = BossGanon_Player_FindBallNear(ballPos, 0);

    if (ball != NULL) {
        ball->timers[2] = 0; // bursts on its next update
    }
}

static void BossGanon_PlayerLightBall_Update_Impl(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    u8 burst = false;
    u8 hitSomething = false;
    s16 i;
    Vec3f vel;
    Vec3f accel;
    Vec3f pos;

    this->unk_1A2++;

    if (this->unk_1A8 != 0) {
        // Burst, from BossGanon_LightBall_Update
        this->actor.shape.rot.y += 0x1000;
        Math_ApproachZeroF(&this->fwork[GDF_FWORK_1], 1.0f, 30.0f);
        Math_ApproachF(&this->actor.scale.x, 20.0f, 0.5f, 100.0f);
        this->fwork[GDF_FWORK_0] += ((M_PI / 2) + Rand_ZeroFloat(M_PI / 4));
        Actor_SetScale(&this->actor, this->actor.scale.x);

        if (this->fwork[GDF_FWORK_1] == 0.0f) {
            Actor_Kill(&this->actor);
        }
        return;
    }

    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_FIRE - SFX_FLAG));
    Actor_SetScale(&this->actor, ((this->unk_1A2 % 2) != 0) ? 6.0f : 5.25f);
    if (this->actor.home.rot.z == 1) {
        BossGanon_FireballFlames(play, &this->actor.world.pos, this->unk_1A2); // the fireball ring's
    }
    this->actor.shape.rot.z += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    // The fireball ring: circling him while he spins the trident
    if (this->unk_1A6 == 2) {
        BossGanon* dorf = (BossGanon*)this->actor.parent;

        if ((dorf == NULL) || (dorf->actor.update == NULL) || (dorf->actionFunc != BossGanon_Player_TridentSpin)) {
            this->unk_1A6 = 0; // he stopped: it bursts
            this->timers[2] = 0;
        } else {
            s16 angle = (s16)(this->unk_1AC * (0x10000 / GDF_FIRE_RING_COUNT)) + sGdfTridentSpin / 2;

            this->actor.world.pos.x = dorf->actor.world.pos.x + Math_SinS(angle) * 120.0f;
            this->actor.world.pos.y = dorf->actor.world.pos.y + 80.0f + Math_SinS(angle * 2 + this->unk_1A2 * 0x400) * 10.0f;
            this->actor.world.pos.z = dorf->actor.world.pos.z + Math_CosS(angle) * 120.0f;
            this->timers[2] = 90;
            Collider_UpdateCylinder(&this->actor, &this->collider);
            if (!BossGanon_PlayerBall_NearLink(this, play)) {
                BossGanon_Player_SetAT(this, play);
            }
            return;
        }
    }

    Actor_UpdateVelocityXYZ(&this->actor);
    Actor_UpdatePos(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 12.0f, 12.0f, 1 | 2 | 4);

    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        hitSomething = true;
        burst = true;
    }
    if (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL | BGCHECKFLAG_CEILING)) {
        burst = true;
    }
    if (this->timers[2] == 0) {
        burst = true;
    }

    // Invasion mode, the volley: sent back by a Link, the ball flies at Ganondorf; B just as it reaches him sends it
    // back at a Link (a little faster each time), missing gets him hit
    if ((this->unk_1A6 != 0) && (sPlayerGanondorf != NULL) && (sPlayerGanondorf->actor.update != NULL)) {
        Vec3f him = sPlayerGanondorf->actor.world.pos;
        f32 dist;

        him.y += 70.0f;
        dist = Math_Vec3f_DistXYZ(&this->actor.world.pos, &him);
        if ((dist < 110.0f) && ((u32)(play->gameplayFrames - sGdfLastBFrame) < 8)) {
            BossGanon_PlayerBall_Aim(this, play, NULL);
            this->unk_1A6 = 0;
            this->unk_1A4++;
            this->actor.speedXZ = MIN(this->actor.speedXZ * 1.1f, 40.0f);
            this->timers[2] = 90;
            Audio_PlayActorSound2(&sPlayerGanondorf->actor, NA_SE_IT_SWORD_REFLECT_MG);
        } else if (dist < 50.0f) {
            GdfInvasion_BallHitGanondorf();
            burst = true;
            hitSomething = true;
        } else {
            BossGanon_PlayerBall_Aim(this, play, &him);
            this->timers[2] = MAX(this->timers[2], 30);
        }
    }

    // Also on the frame it bursts against a wall or floor: eye switches and the like sit right on the surface,
    // and the wall check can stop the ball before its hitbox reaches them. (In Invasion mode, near a Link it doesn't
    // hit here: his own game decides whether he sent it back)
    if (!hitSomething && (this->unk_1A6 == 0) && !BossGanon_PlayerBall_NearLink(this, play)) {
        Collider_UpdateCylinder(&this->actor, &this->collider);
        BossGanon_Player_SetAT(this, play);
    }

    if (!burst) {

        for (i = 0; i < 2; i++) {
            accel.x = accel.z = 0.0f;
            accel.y = 0.2f;
            vel.x = vel.y = vel.z = 0.0f;
            pos.x = Rand_CenteredFloat(30.0f) + this->actor.world.pos.x;
            pos.y = Rand_CenteredFloat(30.0f) + this->actor.world.pos.y;
            pos.z = Rand_CenteredFloat(30.0f) + this->actor.world.pos.z;
            BossGanonEff_SpawnSparkle(play, &pos, &vel, &accel, Rand_ZeroFloat(500.0f) + 700.0f, 0x1E);
        }
    } else {
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 80, NA_SE_EN_GANON_HIT_THUNDER);

        for (i = 0; i < 70; i++) {
            vel.x = Rand_CenteredFloat(30.0f);
            vel.y = Rand_CenteredFloat(30.0f);
            vel.z = Rand_CenteredFloat(30.0f);
            BossGanonEff_SpawnLightRay(play, &this->actor.world.pos, &vel, &sZeroVec, Rand_ZeroFloat(200.0f) + 500.0f,
                                       15.0f, 0x1E);
        }

        this->actor.speedXZ = 0.0f;
        this->unk_1A8 = 1;
    }
}

// Ganondorf's Big Magic, thrown by the player. Five balls that spread out and home in on enemies.
static void BossGanon_PlayerBigMagic_Update_Impl(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    BossGanon* dorf = (BossGanon*)this->actor.parent;
    u8 numEffects = 0;
    s16 index = this->actor.params - GDF_PARAMS_PLAYER_BIG_MAGIC;
    s16 i;
    s16 yawTarget;
    s16 pitchTarget;
    f32 dist;
    f32 wobble;
    Actor* target;
    Vec3f targetPos;
    Vec3f sp60;

    this->unk_1A2++;
    Actor_SetScale(&this->actor, 0.01f);

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    Actor_UpdateVelocityXYZ(&this->actor);
    Actor_UpdatePos(&this->actor);

    this->unk_1A6++;

    if (this->unk_1A6 >= 15) {
        this->unk_1A6 = 0;
    }

    this->unk_2EC[this->unk_1A6] = this->actor.world.pos;
    this->unk_3C4[this->unk_1A6].x = BINANG_TO_RAD(this->actor.world.rot.x);
    this->unk_3C4[this->unk_1A6].y = BINANG_TO_RAD(this->actor.world.rot.y);

    switch (this->unk_1C2) {
        case 1:
            Math_ApproachZeroF(&this->fwork[GDF_FWORK_1], 1.0f, 40.0f);

            if (this->fwork[GDF_FWORK_1] == 0.0f) {
                Actor_Kill(&this->actor);
            }
            break;

        case 10:
            this->unk_1C2 = 11;
            this->timers[0] = 14;
            this->actor.speedXZ = 20.0f;
            this->fwork[GDF_FWORK_1] = 255.0f;
            this->csCamMaxStepScale = 0.0f;
            this->unk_1F0 = (dorf != NULL) ? dorf->playerAimPos : this->actor.world.pos;
            // Fan out like the vanilla balls
            this->actor.shape.rot.y = this->actor.world.rot.y + (index * 0x2000) - 0x4000;
            this->actor.shape.rot.x = 0;
            // fallthrough
        case 11:
            // Spread out for a moment, then home in
            if (this->timers[0] < 8) {
                target = BossGanon_Player_FindTarget(play, &this->actor.world.pos, this->actor.shape.rot.y, 600.0f,
                                                     0x6000, false);
                targetPos = (target != NULL) ? target->focus.pos : this->unk_1F0;

                yawTarget = Math_Vec3f_Yaw(&this->actor.world.pos, &targetPos);
                dist = sqrtf(SQ(targetPos.x - this->actor.world.pos.x) + SQ(targetPos.z - this->actor.world.pos.z));
                pitchTarget = Math_Atan2S(dist, targetPos.y - this->actor.world.pos.y);

                Math_ApproachS(&this->actor.shape.rot.y, yawTarget, 1, this->csCamMaxStepScale);
                Math_ApproachS(&this->actor.shape.rot.x, pitchTarget, 1, this->csCamMaxStepScale);
                Math_ApproachF(&this->csCamMaxStepScale, 2048.0f, 1.0f, 128.0f);

                wobble = CLAMP(dist * 10.0f, 0.0f, 6144.0f);
            } else {
                wobble = 6144.0f;
            }

            this->actor.world.rot.x = (Math_CosS(this->unk_1A2 * 0x3400) * wobble * 0.1f) + this->actor.shape.rot.x;
            this->actor.world.rot.y = (Math_SinS(this->unk_1A2 * 0x1A00) * wobble * 0.3f) + this->actor.shape.rot.y;

            Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 10.0f, 10.0f, 1 | 2 | 4);

            if (this->collider.base.atFlags & AT_HIT) {
                this->collider.base.atFlags &= ~AT_HIT;
                numEffects = 40;
            } else if ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL | BGCHECKFLAG_CEILING)) ||
                       (this->timers[2] == 0)) {
                numEffects = 10;
            }

            if ((numEffects != 40) && !BossGanon_PlayerBall_NearLink(this, play)) {
                // Still hits what it touches on the frame it bursts against a wall (eye switches). (In Invasion mode,
                // near a Link his own game decides: only a spin attack stops it)
                Collider_UpdateCylinder(&this->actor, &this->collider);
                BossGanon_Player_SetAT(this, play);
            }
            if (numEffects != 0) {
                this->unk_1C2 = 1;
                this->actor.speedXZ = 0.0f;
            }
            break;
    }

    if (numEffects) {
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 80, NA_SE_EN_FANTOM_THUNDER);

        for (i = 0; i < numEffects; i++) {
            sp60.x = Rand_CenteredFloat(30.0f);
            sp60.y = Rand_CenteredFloat(30.0f);
            sp60.z = Rand_CenteredFloat(30.0f);

            BossGanonEff_SpawnLightRay(play, &this->actor.world.pos, &sp60, &sZeroVec, Rand_ZeroFloat(200.0f) + 500.0f,
                                       15.0f, 0x1E);
        }
    }
}

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", D-pad Up: laugh while showing the Triforce of Power (from his intro)
// ------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", insta-kill dark aura (D-Pad Left mode)
//
// The purple vortex and dark shocks he hits Link with when Link enters his throne room, around himself. D-Pad Left
// turns the mode on; then hold B: the aura surrounds him (move with the stick, on the ground or in the air) and every
// enemy caught in it dies. Its health drops to 1 and the aura hits it like any of his attacks, so it dies its own
// way (death animation, drops); one the aura can't hurt is destroyed after a moment. Bosses only take the hits.
// ------------------------------------------------------------------------------------------------

#define GDF_AURA_MAX_SCALE 0.24f // the vortex model is 600 wide and 3000 tall: radius 600 * scale
#define GDF_AURA_HEIGHT_SCALE 0.45f
#define GDF_AURA_FORCE_KILL_FRAMES 25
#define GDF_AURA_MAX_TRACKED 32

typedef struct {
    Actor* actor;
    s16 frames;
} GdfAuraVictim;

static GdfAuraVictim sGdfAuraVictims[GDF_AURA_MAX_TRACKED];

static f32 BossGanon_Player_AuraRadius(BossGanon* this) {
    return 600.0f * this->playerAuraScale;
}

// ---- The dark aura against bosses ----
// Some bosses can only be hurt or opened up with a weapon he doesn't have (hookshot for Morpha, arrows for Phantom
// Ganon in the paintings, boomerang for Barinade, the mirror shield for Twinrova). So no boss can leave him stuck,
// the dark aura stands in for whatever the boss needs. The boss's own code asks here, then fakes the hit with
// BossGanon_Player_AuraFakeHit so its damage logic runs as usual. Bosses get a wider reach than enemies: many fly or
// keep their distance.
static ColliderInfo sGdfAuraHitInfo;

s32 BossGanon_Player_AuraReaches(PlayState* play, Actor* boss, Vec3f* pos) {
    BossGanon* gdf = sPlayerGanondorf;
    Vec3f center;

    if ((gdf == NULL) || (gdf->actor.update == NULL) || (gdf->actionFunc != BossGanon_Player_DarkAura) ||
        (gdf->playerAuraScale < 0.1f) || (boss == NULL) || (pos == NULL)) {
        return false;
    }

    center = gdf->actor.world.pos;
    center.y += 80.0f;
    if (Math_Vec3f_DistXYZ(&center, pos) > (BossGanon_Player_AuraRadius(gdf) * 2.0f) + 220.0f) {
        return false;
    }

    if ((play->gameplayFrames % 6) == 0) {
        EffectSsFhgFlash_SpawnShock(play, boss, pos, 400, FHGFLASH_SHOCK_NO_ACTOR);
    }
    return true;
}

// Marks the collider as hit by the aura, carrying dmgFlags (elem: the element that was hit, may be NULL)
void BossGanon_Player_AuraFakeHit(Collider* col, ColliderInfo* elem, u32 dmgFlags) {
    if (sPlayerGanondorf == NULL) {
        return;
    }
    sGdfAuraHitInfo.toucher.dmgFlags = dmgFlags;
    sGdfAuraHitInfo.toucher.effect = 0;
    sGdfAuraHitInfo.toucher.damage = 2;
    col->acFlags |= AC_HIT;
    col->ac = &sPlayerGanondorf->actor;
    if (elem != NULL) {
        elem->acHitInfo = &sGdfAuraHitInfo;
        elem->bumperFlags |= BUMP_HIT;
    }
}

// The raised-hand animations (taunt, dark aura) are standing ones: their root sits a standing height above the
// floating animation's, so levitating he'd rise by that much. In the air, keep the floating root (and its dangling
// legs).
static void BossGanon_Player_KeepFloatRoot(BossGanon* this) {
    Vec3s floatPose[GDF_LIMB_COUNT_PLAYER + 1];
    Vec3s* joints = this->skelAnime.jointTable;
    s32 i;

    if (this->playerGrounded) {
        return;
    }
    SkelAnime_GetFrameData((AnimationHeader*)gGanondorfFloatAnim,
                           (s32)(this->unk_1A2 % (s32)Animation_GetLastFrame(&gGanondorfFloatAnim)),
                           GDF_LIMB_COUNT_PLAYER + 1, floatPose);
    joints[0] = floatPose[0];
    for (i = 18; i < GDF_LIMB_COUNT_PLAYER; i++) {
        joints[1 + i] = floatPose[1 + i]; // pelvis and legs
    }
}

void BossGanon_Player_SetupDarkAura(BossGanon* this, PlayState* play) {
    // Invasion mode: his attacks cost magic
    if (!GdfInvasion_PayAttack(20)) {
        return;
    }
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfRaiseHandLoopAnim, -5.0f);
    this->actionFunc = BossGanon_Player_DarkAura;
    this->playerAuraTimer = 0;
    memset(sGdfAuraVictims, 0, sizeof(sGdfAuraVictims));
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DARKWAVE));
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_TRIFORCE_MARK);
}

static GdfAuraVictim* BossGanon_Player_AuraVictim(Actor* actor) {
    GdfAuraVictim* freeSlot = NULL;
    s32 i;

    for (i = 0; i < GDF_AURA_MAX_TRACKED; i++) {
        if (sGdfAuraVictims[i].actor == actor) {
            return &sGdfAuraVictims[i];
        }
        if ((freeSlot == NULL) && (sGdfAuraVictims[i].actor == NULL)) {
            freeSlot = &sGdfAuraVictims[i];
        }
    }
    if (freeSlot != NULL) {
        freeSlot->actor = actor;
        freeSlot->frames = 0;
    }
    return freeSlot;
}

// Is this actor still in the enemy list (a tracked one may have died and been freed since)
static s32 BossGanon_Player_IsLiveEnemy(PlayState* play, Actor* actor) {
    Actor* it = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;

    while (it != NULL) {
        if (it == actor) {
            return true;
        }
        it = it->next;
    }
    return false;
}

static void BossGanon_Player_AuraKill(BossGanon* this, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f center = this->actor.world.pos;
    f32 radius = BossGanon_Player_AuraRadius(this);
    Actor* actor;
    s32 i;

    // Forget actors that are gone
    for (i = 0; i < GDF_AURA_MAX_TRACKED; i++) {
        if ((sGdfAuraVictims[i].actor != NULL) && !BossGanon_Player_IsLiveEnemy(play, sGdfAuraVictims[i].actor)) {
            sGdfAuraVictims[i].actor = NULL;
        }
    }

    actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;
    while (actor != NULL) {
        Actor* next = actor->next;
        f32 dx = actor->world.pos.x - center.x;
        f32 dz = actor->world.pos.z - center.z;
        f32 dy = actor->world.pos.y - (this->actor.world.pos.y - 40.0f);

        if ((actor->update != NULL) && (SQ(dx) + SQ(dz) < SQ(radius + 20.0f)) && (dy > -80.0f) && (dy < 300.0f)) {
            GdfAuraVictim* victim = BossGanon_Player_AuraVictim(actor);

            // Any hit now kills
            if (actor->colChkInfo.health > 1) {
                actor->colChkInfo.health = 1;
            }
            if ((victim != NULL) && (actor->colChkInfo.health != 0)) {
                victim->frames++;
                if ((victim->frames % 6) == 1) {
                    EffectSsFhgFlash_SpawnShock(play, actor, &actor->world.pos, 400, FHGFLASH_SHOCK_NO_ACTOR);
                }
                // The aura can't hurt it: destroy it
                if (victim->frames >= GDF_AURA_FORCE_KILL_FRAMES) {
                    Vec3f pos = actor->world.pos;

                    pos.y += 20.0f;
                    EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 120, 0, 200, 120, 255, 255, 60, 0, 120, 1, 9,
                                         true);
                    Item_DropCollectibleRandom(play, actor, &actor->world.pos, 0x10);
                    victim->actor = NULL;
                    Actor_Kill(actor);
                }
            }
        }
        actor = next;
    }

    // The aura itself hits everything in it, with all of his damage types
    this->collider.dim.radius = (s16)radius;
    this->collider.dim.height = 260;
    this->collider.dim.yShift = 0;
    this->collider.dim.pos.x = (s16)center.x;
    this->collider.dim.pos.y = (s16)(this->actor.world.pos.y - 80.0f);
    this->collider.dim.pos.z = (s16)center.z;
    BossGanon_Player_SetAT(this, play);
}

void BossGanon_Player_DarkAura(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];
    Player* player = GET_PLAYER(play);
    Vec3s arms[8];
    Vec3s* joints = this->skelAnime.jointTable;
    s32 i;

    SkelAnime_Update(&this->skelAnime);

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 1.0f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    // He faces where he moves
    this->actor.yawTowardsPlayer = player->actor.shape.rot.y;

    // On the ground he walks with the aura up: his arms keep the raised-hand pose, his legs stand and walk
    if (this->playerGrounded) {
        for (i = 0; i < 8; i++) {
            arms[i] = joints[1 + 2 + i];
        }
        this->playerStandLegsWeight = 1.0f;
        BossGanon_Player_StandLegs(this);
        BossGanon_Player_UpdateWalk(this, play);
        for (i = 0; i < 8; i++) {
            joints[1 + 2 + i] = arms[i];
        }
    } else {
        this->playerWalkWeight = 0.0f;
        BossGanon_Player_KeepFloatRoot(this);
    }

    // The Triforce of Power glows on his raised hand
    Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_A], 255.0f, 1.0f, 15.0f);
    Math_ApproachF(&this->fwork[GDF_TRIFORCE_SCALE], 0.6f, 1.0f, 0.3f);
    Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_B], 170.0f, 1.0f, 2.55f);
    Math_ApproachF(&this->fwork[GDF_TRIFORCE_ENV_G], 200.0f, 1.0f, 3.0f);

    Math_ApproachF(&this->playerAuraScale, GDF_AURA_MAX_SCALE, 1.0f, 0.02f);
    Math_ApproachF(&this->playerAuraAlpha, 255.0f, 1.0f, 20.0f);

    this->playerAuraTimer++;
    if ((this->playerAuraTimer % 2) == 0) {
        BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
    }
    if ((this->playerAuraTimer % 45) == 0) {
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DARKWAVE));
    }
    if ((this->playerAuraTimer % 9) == 0) {
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
    }

    if (this->playerAuraScale > 0.05f) {
        BossGanon_Player_AuraKill(this, play);
    }

    // Until B is let go
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B) || this->playerHidden || !sPlayerDarkAuraMode) {
        BossGanon_Player_SetupFloat(this, play);
    }
}

// The purple vortex from his throne room, standing around him
static void BossGanon_Player_DrawDarkAura(BossGanon* this, PlayState* play) {
    f32 scale = this->playerAuraScale;
    u32 frames = play->gameplayFrames;

    if ((this->playerAuraAlpha <= 0.0f) || (scale <= 0.0f)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, frames * -8, 0, 0x20, 0x40, 1, frames * -4, frames * -8,
                                  0x20, 0x20, -8, 0, -4, -8));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 100, 0, 200, (u8)this->playerAuraAlpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 130, 0, 0, 128);

    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y - 60.0f, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((s16)(frames * 0x300)), MTXMODE_APPLY);
    Matrix_Scale(scale, scale * GDF_AURA_HEIGHT_SCALE, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfVortexDL));

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// Crawlspaces: a flickering purple flame where Link is (gameplay_keep's torch flame, tinted)
static void BossGanon_Player_DrawCrawlFlame(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u32 frames = play->gameplayFrames;
    f32 flicker;

    if (!PlayAsGanondorf_IsCrawlFlame(play, player)) {
        return;
    }
    flicker = 1.0f + (Math_SinS((s16)(frames * 0x1C00)) * 0.08f);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0, (frames * -20) & 511, 32, 128, 0, 0, 0,
                                  -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 170, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 0, 255, 0);

    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 2.0f, player->actor.world.pos.z,
                     MTXMODE_NEW);
    // Always faces the camera, like a torch flame
    Matrix_RotateY(BINANG_TO_RAD((s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000)), MTXMODE_APPLY);
    Matrix_Scale(0.012f, 0.012f * flicker, 0.012f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", riding his horse (Epona, drawn as his black horse unless that's turned off)
//
// Seated on the saddle: young Ganondorf's horseback ride cycle on his legs (it straddles the horse), his own upright
// torso, and both fists forward on the reins (solved offline with inverse kinematics on his skeleton).
// ------------------------------------------------------------------------------------------------

#define GDF_RIDE_CYCLE_FRAMES 24.0f

static const Vec3s sGdfReinsPoses[2][3] = {
    { { -2360, 1441, -10464 }, { 937, -6137, -15592 }, { -1175, -1047, -4413 } }, // left arm
    { { 32515, 31667, 18489 }, { 149, 6346, -13011 }, { 2096, 1076, -3143 } }, // right arm
};

static s32 BossGanon_Player_GetSeat(PlayState* play, Vec3f* seat) {
    Player* player = GET_PLAYER(play);
    Actor* horse = player->rideActor;

    // The Gerudo intro: the black horse the director gave him (not Link's, Link is a child there)
    if (gGdfIntroCs.active && (gGdfIntroCs.horse != NULL)) {
        return EnHorseGanon_GetSeat(gGdfIntroCs.horse, seat);
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (horse == NULL) || (horse->id != ACTOR_EN_HORSE)) {
        return false;
    }
    // His black horse's saddle, or Epona's (where Link sits)
    if (!EnHorse_GetGanondorfSeat(horse, seat)) {
        *seat = ((EnHorse*)horse)->riderPos;
    }
    return true;
}

void BossGanon_Player_Ride(BossGanon* this, PlayState* play) {
    static Vec3s sRideA[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sRideB[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sRide[GDF_YOUNG_WALK_JOINTS];
    Player* player = GET_PLAYER(play);
    Vec3s* joints = this->skelAnime.jointTable;
    Actor* horse = (gGdfIntroCs.active && (gGdfIntroCs.horse != NULL)) ? gGdfIntroCs.horse : player->rideActor;
    Vec3f seat;
    f32 speed = (horse != NULL) ? fabsf(horse->speedXZ) : 0.0f;
    s32 frameA;
    s32 frameB;
    s32 b;

    if (!BossGanon_Player_GetSeat(play, &seat)) {
        BossGanon_Player_SetupFloat(this, play);
        return;
    }

    SkelAnime_Update(&this->skelAnime);
    this->playerWalkWeight = 0.0f;

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f + (speed * 0.08f);
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;

    // The ride cycle runs with the horse's speed, gently when it stands
    this->playerRideFrame += 0.35f + (speed * 0.12f);
    while (this->playerRideFrame >= GDF_RIDE_CYCLE_FRAMES) {
        this->playerRideFrame -= GDF_RIDE_CYCLE_FRAMES;
    }
    frameA = (s32)this->playerRideFrame;
    frameB = (frameA + 1) % (s32)GDF_RIDE_CYCLE_FRAMES;
    SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfHorsebackRideAnim, frameA, GDF_YOUNG_WALK_JOINTS, sRideA);
    SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfHorsebackRideAnim, frameB, GDF_YOUNG_WALK_JOINTS, sRideB);
    SkelAnime_InterpFrameTable(GDF_YOUNG_WALK_JOINTS, sRide, sRideA, sRideB, this->playerRideFrame - frameA);

    // Straddling legs (pelvis and legs from the ride; his own feet)
    for (b = 18; b < GDF_LIMB_COUNT_PLAYER; b++) {
        s8 y = sGdfBossToYoungLimb[b];

        if (y >= 0) {
            joints[1 + b] = sRide[1 + y];
        }
    }
    // Upright, facing forward
    joints[1] = (Vec3s){ 0, 0, 0 };

    // Fists forward on the reins
    BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfReinsPoses[0], 1.0f);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfReinsPoses[1], 1.0f);
}

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", climbing ladders and vine walls, and holding up found items
// ------------------------------------------------------------------------------------------------

// Arm poses solved offline with inverse kinematics on his skeleton: fists on the ladder high above his head and at
// chest height, and both fists raised together over his head holding an item
static const Vec3s sGdfClimbPoses[4][3] = {
    { { -6994, -3513, 7468 }, { 2852, -12664, -4315 }, { 4392, -9851, 1201 } }, // left arm, high
    { { -3521, 3968, -14824 }, { -2707, -7560, -24466 }, { -7238, -8633, -1420 } }, // left arm, low
    { { -28322, 19201, 21828 }, { 834, 7723, -14672 }, { 5723, -5859, -6160 } }, // right arm, high
    { { 32551, 25869, 21196 }, { -1118, 6437, -14874 }, { -1091, -2672, -4555 } }, // right arm, low
};
static const Vec3s sGdfItemPoses[2][3] = {
    { { 11691, 27520, -16011 }, { 3178, -1419, -16117 }, { 7921, -1511, -7597 } }, // left arm
    { { -28634, 15814, 19375 }, { 1338, 7492, -14082 }, { 8790, -4294, -8015 } }, // right arm
};

// ---- The Gerudo intro (GanondorfIntro.cpp directs it through gGdfIntroCs) ----

// Arm poses solved offline with inverse kinematics on his skeleton (upper arm, forearm, hand)
static const Vec3s sGdfCsWindowL[3] = { { -1225, 619, -9207 }, { 3703, -7871, -12831 }, { 5115, -4799, -3935 } };
static const Vec3s sGdfCsWindowR[3] = { { 21967, -31085, 19073 }, { -145, 2419, -16692 }, { 1837, -11818, -4437 } };
static const Vec3s sGdfCsTableR[3] = { { -31060, 32618, 18376 }, { 3977, 5726, -13603 }, { 15116, 628, -4743 } };
static const Vec3s sGdfCsLetterL[3] = { { -3540, 3760, -14628 }, { -2706, -7551, -23530 }, { -9620, -5459, 706 } };
static const Vec3s sGdfCsLetterR[3] = { { 32065, 25853, 20258 }, { -1346, 6368, -14674 }, { -1887, -2409, -4343 } };
static const Vec3s sGdfCsHandR[3] = { { -31675, 23256, 20189 }, { 1623, 6272, -13481 }, { 11884, -1114, -10070 } };

static f32 sGdfCsPoseWeight = 0.0f;
static u8 sGdfCsPoseCur = GDF_CSPOSE_NONE;
static f32 sGdfCsHeadYaw = 0.0f;
static f32 sGdfCsHeadPitch = 0.0f;
static u8 sGdfCsWasActive = false;
static f32 sGdfCsGlow = 0.0f;

static void BossGanon_Pg_RotY(f32 deg, GdfMat3 out) {
    f32 a = deg * (M_PI / 180.0f);

    out[0][0] = cosf(a);  out[0][1] = 0.0f; out[0][2] = sinf(a);
    out[1][0] = 0.0f;     out[1][1] = 1.0f; out[1][2] = 0.0f;
    out[2][0] = -sinf(a); out[2][1] = 0.0f; out[2][2] = cosf(a);
}

static void BossGanon_Player_ApplyCsPose(BossGanon* this, u8 pose, f32 w) {
    Vec3s* joints = this->skelAnime.jointTable;
    s32 i;

    switch (pose) {
        case GDF_CSPOSE_WINDOW:
            BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfCsWindowL, w);
            BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfCsWindowR, w);
            break;
        case GDF_CSPOSE_TABLE:
            BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfCsTableR, w);
            break;
        case GDF_CSPOSE_RAISE:
            BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfClimbPoses[2], w);
            break;
        case GDF_CSPOSE_LETTER:
            BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfCsLetterL, w);
            BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfCsLetterR, w);
            break;
        case GDF_CSPOSE_HAND:
            BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfCsHandR, w);
            break;
        case GDF_CSPOSE_CROSSED:
            for (i = 0; i < 8; i++) {
                Vec3s target = sGdfBroodArms[i];

                BossGanon_Player_SlerpRot(&joints[1 + 2 + i], &target, w, &joints[1 + 2 + i]);
            }
            break;
        default:
            break;
    }
}

// After his own animation: the director's arm pose, where he looks, his open hand, the glow in it
static void BossGanon_Player_ApplyIntroCs(BossGanon* this, PlayState* play) {
    GdfIntroCs* cs = &gGdfIntroCs;
    Vec3s* joints = this->skelAnime.jointTable;

    if (!cs->active) {
        if (sGdfCsWasActive) {
            sGdfCsWasActive = false;
            this->useOpenHand = false;
            sGdfCsPoseWeight = 0.0f;
            sGdfCsPoseCur = GDF_CSPOSE_NONE;
            sGdfCsHeadYaw = sGdfCsHeadPitch = 0.0f;
            sGdfCsGlow = 0.0f;
        }
        return;
    }
    sGdfCsWasActive = true;
    // Link is held up in the air while Ganondorf rides; that's no jump to land heavily from afterwards
    this->playerWasAirborne = false;
    // Drawn even when the camera looks away from him (the props are his)
    this->actor.flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED;

    // A new pose: the old one lets go first, then the new one comes in
    if (cs->pose != sGdfCsPoseCur) {
        Math_ApproachZeroF(&sGdfCsPoseWeight, 1.0f, 0.1f);
        if (sGdfCsPoseWeight <= 0.0f) {
            sGdfCsPoseCur = cs->pose;
        }
    } else {
        Math_ApproachF(&sGdfCsPoseWeight, (sGdfCsPoseCur != GDF_CSPOSE_NONE) ? 1.0f : 0.0f, 1.0f, 0.07f);
    }
    if (sGdfCsPoseWeight > 0.0f) {
        BossGanon_Player_ApplyCsPose(this, sGdfCsPoseCur, BossGanon_Player_SmoothStep(sGdfCsPoseWeight));
    }

    // He turns his head slowly
    Math_ApproachF(&sGdfCsHeadYaw, cs->headYaw, 0.08f, 2.0f);
    Math_ApproachF(&sGdfCsHeadPitch, cs->headPitch, 0.08f, 2.0f);
    if ((fabsf(sGdfCsHeadYaw) > 0.1f) || (fabsf(sGdfCsHeadPitch) > 0.1f)) {
        GdfMat3 rootRot;
        GdfMat3 body;
        GdfMat3 neck;
        GdfMat3 rot;
        GdfMat3 rotY;
        GdfMat3 rotX;
        GdfMat3 tmp;

        BossGanon_Pg_FromRot(&joints[1 + 0], rootRot);
        BossGanon_Pg_FromRot(&joints[1 + 1], tmp);
        BossGanon_Pg_Mul(rootRot, tmp, body, false);
        BossGanon_Pg_FromRot(&joints[1 + 10], tmp);
        BossGanon_Pg_Mul(body, tmp, neck, false);
        BossGanon_Pg_RotY(sGdfCsHeadYaw, rotY);
        BossGanon_Pg_RotX(sGdfCsHeadPitch, rotX);
        BossGanon_Pg_Mul(rotY, rotX, rot, false);
        BossGanon_Pg_Mul(rot, neck, tmp, false);
        BossGanon_Pg_Mul(body, tmp, neck, true);
        BossGanon_Pg_ToRot(neck, &joints[1 + 10]);
    }

    this->useOpenHand = cs->openHand;
    Math_ApproachF(&sGdfCsGlow, cs->handGlow, 0.3f, 0.4f);

    if (cs->laugh) {
        cs->laugh = false;
        if ((this->actionFunc == BossGanon_Player_Float) || (this->actionFunc == BossGanon_Player_SwordIdle) ||
            (this->actionFunc == BossGanon_Player_Brood)) {
            BossGanon_Player_SetupLaugh(this, play);
        }
    }
}

// A glowing billboard (his light ball's look) at pos
static void BossGanon_Player_DrawCsGlow(PlayState* play, Vec3f* pos, f32 scale, u8 r, u8 g, u8 b, u8 envR, u8 envG,
                                        u8 envB, u8 alpha, s16 spin) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, r, g, b, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, envR, envG, envB, 0);
    gSPDisplayList(POLY_XLU_DISP++, gGanondorfLightBallMaterialDL);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(spin), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, gGanondorfSquareDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// One face of a box, as two triangles, in the colour given
static void BossGanon_Player_CsQuad(Gfx** gfxP, Vtx* v, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, s16 x2,
                                    s16 y2, s16 z2, s16 x3, s16 y3, s16 z3) {
    Gfx* gfx = *gfxP;
    s16 p[4][3] = { { x0, y0, z0 }, { x1, y1, z1 }, { x2, y2, z2 }, { x3, y3, z3 } };
    s32 i;

    for (i = 0; i < 4; i++) {
        v[i].v.ob[0] = p[i][0];
        v[i].v.ob[1] = p[i][1];
        v[i].v.ob[2] = p[i][2];
        v[i].v.flag = 0;
        v[i].v.tc[0] = v[i].v.tc[1] = 0;
        v[i].v.cn[0] = v[i].v.cn[1] = v[i].v.cn[2] = v[i].v.cn[3] = 255;
    }
    gSPVertex(gfx++, v, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    *gfxP = gfx;
}

// A box from (x0,y0,z0) to (x1,y1,z1): its top, the four sides (shaded darker) and no bottom
static void BossGanon_Player_CsBox(PlayState* play, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, u8 r, u8 g,
                                   u8 b) {
    Vtx* v = Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * 20);

    OPEN_DISPS(play->state.gfxCtx);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, r, g, b, 255);
    BossGanon_Player_CsQuad(&POLY_OPA_DISP, &v[0], x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, r * 7 / 10, g * 7 / 10, b * 7 / 10, 255);
    BossGanon_Player_CsQuad(&POLY_OPA_DISP, &v[4], x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1);
    BossGanon_Player_CsQuad(&POLY_OPA_DISP, &v[8], x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, r * 55 / 100, g * 55 / 100, b * 55 / 100, 255);
    BossGanon_Player_CsQuad(&POLY_OPA_DISP, &v[12], x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0);
    BossGanon_Player_CsQuad(&POLY_OPA_DISP, &v[16], x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1);

    CLOSE_DISPS(play->state.gfxCtx);
}

// The war table: a stone table with the map of Hyrule on it (the pause screen's world map), and his black stone
static void BossGanon_Player_DrawCsTable(PlayState* play) {
    GdfIntroCs* cs = &gGdfIntroCs;
    s16 hw = GDF_TABLE_W / 2.0f;
    s16 hd = GDF_TABLE_D / 2.0f;
    s16 top = GDF_TABLE_H;
    Vtx* v;
    s32 k;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(cs->tablePos.x, cs->tablePos.y - GDF_TABLE_H, cs->tablePos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD((s16)(cs->tableYaw + 0x8000)), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // The stone: a slab on a thick pedestal
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BOTH | G_FOG);
    BossGanon_Player_CsBox(play, -hw - 6, top - 9, -hd - 6, hw + 6, top, hd + 6, 150, 118, 84);
    BossGanon_Player_CsBox(play, -hw + 18, 0, -hd + 14, hw - 18, top - 9, hd - 14, 120, 92, 64);

    // The map, in 15 strips of 9 texel rows (as the pause screen draws it)
    v = Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * 4 * 15);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 245, 225, 255);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);
    gDPLoadTLUT_pal256(POLY_OPA_DISP++, gWorldMapImageTLUT);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_RGBA16);
    for (k = 0; k < 15; k++) {
        s32 row0 = k * 9;
        s32 rows = (k < 14) ? 9 : 2;
        s16 z0 = -hd + (GDF_TABLE_D * row0) / 128.0f;
        s16 z1 = -hd + (GDF_TABLE_D * (row0 + rows)) / 128.0f;
        Vtx* q = &v[k * 4];
        s32 i;

        for (i = 0; i < 4; i++) {
            q[i].v.ob[0] = ((i == 0) || (i == 3)) ? -hw : hw;
            q[i].v.ob[1] = top + 1;
            q[i].v.ob[2] = (i < 2) ? z0 : z1;
            q[i].v.flag = 0;
            q[i].v.tc[0] = (((i == 0) || (i == 3)) ? 0 : 216) << 5;
            q[i].v.tc[1] = ((i < 2) ? 0 : rows) << 5;
            q[i].v.cn[0] = q[i].v.cn[1] = q[i].v.cn[2] = q[i].v.cn[3] = 255;
        }
        gDPLoadMultiTile(POLY_OPA_DISP++, gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, 216, 128, 0,
                         row0, 216 - 1, row0 + rows - 1, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (216 - 1) << G_TEXTURE_IMAGE_FRAC,
                       (rows - 1) << G_TEXTURE_IMAGE_FRAC);
        gSPVertex(POLY_OPA_DISP++, q, 4, 0);
        gSP2Triangles(POLY_OPA_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);

    // His black stone, set down on the map
    if (cs->stoneOn) {
        s16 sx = -hw + (cs->stoneU / 216.0f) * GDF_TABLE_W;
        s16 sz = -hd + (cs->stoneV / 128.0f) * GDF_TABLE_D;
        s16 sy = top + 1 + cs->stoneDrop;

        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        BossGanon_Player_CsBox(play, sx - 3, sy, sz - 3, sx + 3, sy + 5, sz + 3, 40, 30, 50);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void BossGanon_Player_DrawIntroProps(BossGanon* this, PlayState* play) {
    GdfIntroCs* cs = &gGdfIntroCs;

    if (!cs->active) {
        return;
    }

    if (cs->tableOn) {
        BossGanon_Player_DrawCsTable(play);
    }

    // The glow in his right hand (the vision touches him)
    if ((sGdfCsGlow > 0.01f) && !this->playerHidden) {
        BossGanon_Player_DrawCsGlow(play, &this->unk_260, sGdfCsGlow, 255, 255, 255, 60, 255, 90,
                                    ((this->unk_1A2 % 2) != 0) ? 150 : 110, this->unk_1A2 * 0x500);
    }

    // The king's letter, held before his chest
    if (cs->letterOn && !this->playerHidden) {
        Vec3f mid;

        mid.x = (this->unk_2EC[5].x + this->unk_2EC[8].x) * 0.5f;
        mid.y = (this->unk_2EC[5].y + this->unk_2EC[8].y) * 0.5f + 4.0f;
        mid.z = (this->unk_2EC[5].z + this->unk_2EC[8].z) * 0.5f;
        Matrix_Translate(mid.x, mid.y, mid.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(-0.9f, MTXMODE_APPLY);
        Matrix_Scale(0.3f, 0.3f, 0.3f, MTXMODE_APPLY);
        GetItem_Draw(play, GID_LETTER_ZELDA);
    }

    // The boy's ocarina, glowing blue
    if (cs->ocarinaOn) {
        Matrix_Translate(cs->ocarinaPos.x, cs->ocarinaPos.y, cs->ocarinaPos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD((s16)(play->gameplayFrames * 300)), MTXMODE_APPLY);
        Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        GetItem_Draw(play, GID_OCARINA_TIME);
        BossGanon_Player_DrawCsGlow(play, &cs->ocarinaPos, 2.2f, 200, 230, 255, 40, 90, 255,
                                    ((play->gameplayFrames % 2) != 0) ? 140 : 100, play->gameplayFrames * 0x300);
    }

    // The vision: a black void, a golden light in it, and the three triangles turning in the dark
    if (cs->voidAlpha > 0) {
        Environment_FillScreen(play->state.gfxCtx, 0, 0, 0, cs->voidAlpha, FILL_SCREEN_XLU);
    }
    if (cs->goldLight > 0.0f) {
        BossGanon_Player_DrawCsGlow(play, &cs->goldPos, cs->goldLight, 255, 255, 200, 255, 190, 40, 255,
                                    play->gameplayFrames * 0x120);
        BossGanon_Player_DrawCsGlow(play, &cs->goldPos, cs->goldLight * 0.55f, 255, 255, 255, 255, 230, 120, 255,
                                    play->gameplayFrames * -0x200);
    }
    if ((cs->triforceScale > 0.0f) && (cs->triforceAlpha > 0)) {
        OPEN_DISPS(play->state.gfxCtx);

        gDPPipeSync(POLY_XLU_DISP++);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, cs->triforceAlpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 200, 0, 128);
        Matrix_Translate(cs->triforcePos.x, cs->triforcePos.y, cs->triforcePos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(cs->triforceYaw), MTXMODE_APPLY);
        Matrix_Scale(cs->triforceScale, cs->triforceScale, 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfTriforceDL));

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

// Hand over hand: each arm reaches up while the other pulls, the knee on the same side lifts with the reaching arm.
// The cycle follows how far Link actually climbs.
// Link's climbing off the top of a ladder or a wall: how far along he is (or -1, still climbing)
static f32 BossGanon_Player_LadderTopProgress(PlayState* play) {
    Player* player = GET_PLAYER(play);
    void* anim = player->skelAnime.animation;

    if ((anim == gPlayerAnim_link_normal_climb_upL) || (anim == gPlayerAnim_link_normal_climb_upR) ||
        (anim == gPlayerAnim_clink_normal_climb_upL) || (anim == gPlayerAnim_clink_normal_climb_upR) ||
        (anim == gPlayerAnim_link_normal_Fclimb_upL) || (anim == gPlayerAnim_link_normal_Fclimb_upR)) {
        f32 end = player->skelAnime.endFrame;

        return (end > 0.0f) ? CLAMP(player->skelAnime.curFrame / end, 0.0f, 1.0f) : 1.0f;
    }
    return -1.0f;
}

void BossGanon_Player_Climb(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 top;

    if (!(player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) || this->playerHidden) {
        BossGanon_Player_SetupFloat(this, play);
        return;
    }

    SkelAnime_Update(&this->skelAnime);
    this->playerWalkWeight = 0.0f;
    this->playerStandLegsWeight = 0.0f;
    Math_ApproachZeroF(&this->playerGroundOffset, 1.0f, 4.0f);

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;

    top = BossGanon_Player_LadderTopProgress(play);
    if (top >= 0.0f) {
        // Over the top: the last rung, a knee up onto it, and standing up, timed by Link's climb
        BossGanon_Player_ApplyClip(this, sGdfClipLadderTop, ARRAY_COUNT(sGdfClipLadderTop),
                                   top * (ARRAY_COUNT(sGdfClipLadderTop) - 1), false, 1.0f, 1.0f);
        return;
    }
    // Hand over hand, as far as Link has climbed (a cycle every 25 units, as before)
    this->playerClimbPhase += Math_Vec3f_DistXYZ(&player->actor.world.pos, &player->actor.prevPos) * 0.25f;
    BossGanon_Player_ApplyClip(this, sGdfClipLadder, ARRAY_COUNT(sGdfClipLadder),
                               this->playerClimbPhase * (ARRAY_COUNT(sGdfClipLadder) / (2.0f * M_PI)), true, 1.0f,
                               1.0f); // (on a ladder he faces the way Link does, toward it)
}

// Ledges: while Link steps or pulls himself up onto a ledge (Link is already up there, his animation just shows him
// climbing), Ganondorf plants both fists on the ledge above him, hauls himself up with a knee coming up onto it, and
// steps forward onto the top, timed by Link's climbing animation.
static Vec3f sGdfLedgeStart;
static Vec3f sGdfLedgeTop;

static f32 BossGanon_Player_LedgeProgress(PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 end = player->skelAnime.endFrame;

    return (end > 0.0f) ? CLAMP(player->skelAnime.curFrame / end, 0.0f, 1.0f) : 1.0f;
}

void BossGanon_Player_SetupLedgeClimb(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    BossGanon_Player_ReleaseOrganCam(this, play);
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -4.0f);
    this->actionFunc = BossGanon_Player_LedgeClimb;
    this->playerWalkWeight = 0.0f;
    this->playerSprintWeight = 0.0f;
    this->playerStandLegsWeight = 0.0f;

    sGdfLedgeStart = this->actor.world.pos;
    sGdfLedgeTop = player->actor.world.pos;
    sGdfLedgeTop.y += this->playerGroundOffset;
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DAMAGE1));
}

void BossGanon_Player_LedgeClimb(BossGanon* this, PlayState* play) {
    static Vec3s sKneeR[GDF_YOUNG_WALK_JOINTS];
    static u8 sKneeRead = false;
    Player* player = GET_PLAYER(play);
    Vec3s* joints = this->skelAnime.jointTable;
    Vec3s armL[3];
    Vec3s armR[3];
    f32 t = BossGanon_Player_LedgeProgress(play);
    f32 reach;
    f32 weight;
    f32 knee;
    s32 i;

    if (!(player->stateFlags1 & PLAYER_STATE1_CLIMBING_LEDGE) || this->playerHidden) {
        BossGanon_Player_SetupFloat(this, play);
        return;
    }
    if (!sKneeRead) {
        SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, 20, GDF_YOUNG_WALK_JOINTS, sKneeR);
        sKneeRead = true;
    }

    SkelAnime_Update(&this->skelAnime);
    this->legSwayEnabled = false;
    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;

    // Fists up on the ledge first, then pushing down on it as he rises, then back to his sides on top
    reach = 1.0f - BossGanon_Player_SmoothStep((t - 0.35f) / 0.35f);
    weight = 1.0f - BossGanon_Player_SmoothStep((t - 0.8f) / 0.2f);
    for (i = 0; i < 3; i++) {
        BossGanon_Player_SlerpRot((Vec3s*)&sGdfClimbPoses[1][i], (Vec3s*)&sGdfClimbPoses[0][i], reach, &armL[i]);
        BossGanon_Player_SlerpRot((Vec3s*)&sGdfClimbPoses[3][i], (Vec3s*)&sGdfClimbPoses[2][i], reach, &armR[i]);
    }
    BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, armL, weight);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, armR, weight);

    // The right knee comes up onto the ledge while he hauls himself up
    knee = BossGanon_Player_RunBump(t, 0.55f, 0.35f);
    for (i = 22; i <= 23; i++) {
        BossGanon_Player_SlerpRot(&joints[1 + i], &sKneeR[1 + sGdfBossToYoungLimb[i]], knee, &joints[1 + i]);
    }

    // Leaning into the wall as he pulls
    BossGanon_Player_TwistLean(this, 0.0f, 0x1800 * BossGanon_Player_RunBump(t, 0.45f, 0.45f));
}

// His path up: rising through the middle of the climb, forward onto the top in the second half
static void BossGanon_Player_PlaceLedgeClimb(BossGanon* this, PlayState* play) {
    f32 t = BossGanon_Player_LedgeProgress(play);
    f32 up = BossGanon_Player_SmoothStep((t - 0.1f) / 0.6f);
    f32 forward = BossGanon_Player_SmoothStep((t - 0.4f) / 0.5f);

    this->actor.world.pos.x = sGdfLedgeStart.x + (sGdfLedgeTop.x - sGdfLedgeStart.x) * forward;
    this->actor.world.pos.y = sGdfLedgeStart.y + (sGdfLedgeTop.y - sGdfLedgeStart.y) * up;
    this->actor.world.pos.z = sGdfLedgeStart.z + (sGdfLedgeTop.z - sGdfLedgeStart.z) * forward;
}

// His Gerudo bodyguards (option "Gerudo Bodyguards"): three Gerudo fighters, in green, blue and red, who follow him and
// fight the enemies around him (their behavior is in z_en_geldb.c). They drop in near him when the option is turned
// on and in every new area; turning it off sends them away.
static void BossGanon_Player_UpdateBodyguards(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 want = CVarGetInteger(CVAR_PLAY_AS_GDF("Bodyguards"), 0) && !this->playerHidden && !gGdfIntroCs.active &&
              (gSaveContext.gameMode == GAMEMODE_NORMAL) && (play->csCtx.state == CS_STATE_IDLE) &&
              (play->transitionTrigger == TRANS_TRIGGER_OFF);
    Actor* found[3] = { NULL, NULL, NULL };
    Actor* actor;
    s32 i;

    // In the NPC list once they're set up, in the enemy list until then
    for (i = 0; i < 2; i++) {
        for (actor = play->actorCtx.actorLists[(i == 0) ? ACTORCAT_NPC : ACTORCAT_ENEMY].head; actor != NULL;
             actor = actor->next) {
            if ((actor->id == ACTOR_EN_GELDB) && EN_GELDB_IS_ALLY(actor->params)) {
                found[actor->params & 3] = actor;
            }
        }
    }
    if (!CVarGetInteger(CVAR_PLAY_AS_GDF("Bodyguards"), 0) || gGdfIntroCs.active) {
        for (i = 0; i < 3; i++) {
            if (found[i] != NULL) {
                Vec3f zero = { 0.0f, 0.0f, 0.0f };
                Vec3f pos = found[i]->world.pos;

                pos.y += 30.0f;
                EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 100, 0, 255, 255, 255, 255, 200, 0, 0, 1, 9, true);
                Actor_Kill(found[i]);
            }
        }
        return;
    }
    if (!want) {
        return;
    }
    if (Object_GetIndex(&play->objectCtx, OBJECT_GELDB) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_GELDB);
        return;
    }
    for (i = 0; i < 3; i++) {
        if (found[i] == NULL) {
            s16 yaw = player->actor.shape.rot.y + 0x8000 + (i - 1) * 0x2400;

            Actor_Spawn(&play->actorCtx, play, ACTOR_EN_GELDB, player->actor.world.pos.x + Math_SinS(yaw) * 95.0f,
                        player->actor.world.pos.y, player->actor.world.pos.z + Math_CosS(yaw) * 95.0f, 0,
                        player->actor.shape.rot.y, 0, EN_GELDB_PARAMS_ALLY + i);
        }
    }
}

// Through a doorway he's kicked open: he's too tall for the frame, so he grabs it with his right hand, ducks his head
// under it and pulls himself through. Weighted by how close he is to the doorway's plane.
#define GDF_DOOR_DUCK_LEAN 0x2400 // about 50 degrees

static void BossGanon_Player_ApplyDoorDuck(BossGanon* this, PlayState* play) {
    Actor* actor;
    f32 w = 0.0f;

    if (!PlayAsGanondorf_ShouldKickDoors() || this->playerHidden || !this->playerGrounded ||
        (this->actionFunc == BossGanon_Player_Brood) || (this->actionFunc == BossGanon_Player_LedgeClimb)) {
        return;
    }
    for (actor = play->actorCtx.actorLists[ACTORCAT_DOOR].head; actor != NULL; actor = actor->next) {
        f32 dx;
        f32 dz;
        f32 across;
        f32 along;

        if ((actor->id != ACTOR_EN_DOOR) || (((EnDoor*)actor)->gdfKickState == 0)) {
            continue;
        }
        dx = this->actor.world.pos.x - actor->world.pos.x;
        dz = this->actor.world.pos.z - actor->world.pos.z;
        across = (dx * Math_SinS(actor->shape.rot.y)) + (dz * Math_CosS(actor->shape.rot.y)); // through the doorway
        along = (dx * Math_CosS(actor->shape.rot.y)) - (dz * Math_SinS(actor->shape.rot.y));  // sideways in it
        if (fabsf(along) > 90.0f) {
            continue;
        }
        w = MAX(w, 1.0f - CLAMP((fabsf(across) - 10.0f) / 55.0f, 0.0f, 1.0f));
    }
    // Not while the kick itself is still going (it ends at door frame 42, see ApplyDoorKick)
    {
        Player* player = GET_PLAYER(play);
        EnDoor* door = (EnDoor*)player->doorActor;

        if ((door != NULL) && (door->actor.id == ACTOR_EN_DOOR) && (door->actor.update != NULL) &&
            door->playerIsOpening) {
            f32 frame = door->skelAnime.curFrame;

            w *= CLAMP((frame - 36.0f) / 10.0f, 0.0f, 1.0f);
        }
    }
    if (w <= 0.0f) {
        return;
    }
    w = w * w * (3.0f - 2.0f * w);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfClimbPoses[2], w);
    BossGanon_Player_TwistLean(this, 0.0f, GDF_DOOR_DUCK_LEAN * w);
}

// The trident throw, from his last battle in A Link to the Past: he stands his ground spinning the trident flat over
// his head (C-Right, held), then hurls it (let go): it flies spinning at the nearest enemy in front of him (or the one
// he's targeting), homing in, hits everything in its way, and comes back to his hand.
#define GDF_TRIDENT_THROW_SPEED 24.0f
#define GDF_TRIDENT_RETURN_SPEED 28.0f
#define GDF_TRIDENT_RANGE 900.0f
#define GDF_TRIDENT_HIT_RADIUS 45.0f

void BossGanon_Player_SetupTridentSpin(BossGanon* this, PlayState* play) {
    // Invasion mode: his attacks cost magic
    if (!GdfInvasion_PayAttack(10)) {
        return;
    }
    if (this->playerGrounded) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfStandIdleAnim, -5.0f);
    } else {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, -5.0f);
    }
    this->actionFunc = BossGanon_Player_TridentSpin;
    sGdfTridentState = GDF_TRIDENT_OVERHEAD;
    sGdfTridentTimer = 0;
    this->playerWalkWeight = 0.0f;
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN));
}

// Right arm poses for the trident, solved offline with inverse kinematics on his skeleton: the arm straight up, the fist
// well above his head (spinning it), and thrown, the fist out ahead of him at shoulder height after it
static const Vec3s sGdfTridentArmPoses[2][3] = {
    { { -21258, -30829, -19024 }, { 343, 8936, -9851 }, { 2132, 4505, -2113 } },   // overhead
    { { -31207, 24093, 16322 }, { 242, 6426, -12825 }, { 2172, 1324, -2589 } },    // thrown
};

static void BossGanon_Player_TridentArms(BossGanon* this, f32 overhead) {
    Vec3s arm[3];
    s32 i;

    for (i = 0; i < 3; i++) {
        BossGanon_Player_SlerpRot((Vec3s*)&sGdfTridentArmPoses[1][i], (Vec3s*)&sGdfTridentArmPoses[0][i], overhead,
                                  &arm[i]);
    }
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, arm, 1.0f);
}

void BossGanon_Player_TridentSpin(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];
    Player* player = GET_PLAYER(play);
    Vec3f from;
    Actor* target;

    SkelAnime_Update(&this->skelAnime);
    BossGanon_Player_KeepFloatRoot(this);
    BossGanon_Player_TridentArms(this, 1.0f);
    this->actor.yawTowardsPlayer = this->actor.shape.rot.y; // he stands his ground

    sGdfTridentTimer++;
    // Spinning up: faster and faster
    sGdfTridentSpin += (s16)(0x0C00 + MIN(sGdfTridentTimer, 12) * 0x0180);
    if ((sGdfTridentTimer % 5) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
    }
    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 1.0f;
    sCape->sideSwayMagnitude = -3.0f;

    // Five seconds of spinning: the fireball ring (costs magic)
    if ((sGdfTridentTimer == GDF_FIRE_RING_FRAMES) && GdfInvasion_PayAttack(GDF_FIRE_RING_COST)) {
        s32 k;

        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_FIRE);
        for (k = 0; k < GDF_FIRE_RING_COUNT; k++) {
            BossGanon* ball = (BossGanon*)Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_BOSS_GANON,
                                                             this->actor.world.pos.x, this->actor.world.pos.y + 80.0f,
                                                             this->actor.world.pos.z, 0, 0, 1,
                                                             GDF_PARAMS_PLAYER_LIGHT_BALL); // (rot z 1: a fireball)
            sGdfFireRing[k] = (ball != NULL) ? &ball->actor : NULL;
            if (ball != NULL) {
                ball->unk_1A6 = 2; // circling him
                ball->unk_1AC = k;
                ball->actor.speedXZ = 0.0f;
            }
        }
    }

    if (CHECK_BTN_ALL(input->cur.button, BTN_CRIGHT) || (sGdfTridentTimer < 10)) {
        return;
    }

    // The ring scatters: each fireball flies at a Link (or outward)
    {
        s32 k;

        for (k = 0; k < GDF_FIRE_RING_COUNT; k++) {
            BossGanon* ball = (BossGanon*)sGdfFireRing[k];

            if ((ball != NULL) && (ball->actor.update != NULL) && (ball->unk_1A6 == 2)) {
                ball->unk_1A6 = 0;
                ball->actor.speedXZ = 16.0f;
                ball->timers[2] = 90;
                ball->actor.world.rot.x = 0;
                ball->actor.world.rot.y = (s16)(ball->unk_1AC * (0x10000 / GDF_FIRE_RING_COUNT)) + sGdfTridentSpin;
                BossGanon_PlayerBall_Aim(ball, play, NULL);
            }
            sGdfFireRing[k] = NULL;
        }
    }

    // Let go: the throw, at what he's targeting or the nearest enemy in front of him
    from = this->actor.world.pos;
    target = ((player->focusActor != NULL) && (player->focusActor != &player->actor)) ? player->focusActor
                                                                                       : BossGanon_Player_FindTarget(
                                                                                             play, &from,
                                                                                             this->actor.shape.rot.y,
                                                                                             GDF_TRIDENT_RANGE, 0x3000,
                                                                                             true);
    sGdfTridentTarget = target;
    sGdfTridentPos = this->unk_2EC[8];
    sGdfTridentPos.y += 6.0f;
    sGdfTridentVel.x = Math_SinS(this->actor.shape.rot.y) * GDF_TRIDENT_THROW_SPEED;
    sGdfTridentVel.y = 0.0f;
    sGdfTridentVel.z = Math_CosS(this->actor.shape.rot.y) * GDF_TRIDENT_THROW_SPEED;
    sGdfTridentTravel = 0.0f;
    sGdfTridentTimer = 0;
    sGdfTridentState = GDF_TRIDENT_OUT;
    this->actionFunc = BossGanon_Player_TridentThrow;
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOOMERANG_THROW);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW));
}

void BossGanon_Player_TridentThrow(BossGanon* this, PlayState* play) {
    Vec3f prev = sGdfTridentPos;
    Vec3f aim;
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;
    f32 dist;

    SkelAnime_Update(&this->skelAnime);
    BossGanon_Player_KeepFloatRoot(this);
    // He's free to move while it flies: his legs walk, his right arm stays out after it
    if (this->playerGrounded) {
        BossGanon_Player_UpdateWalk(this, play);
    } else {
        this->playerWalkWeight = 0.0f;
    }
    BossGanon_Player_TridentArms(this, 0.0f);

    sGdfTridentTimer++;
    sGdfTridentSpin += 0x2400;
    if ((sGdfTridentTimer % 6) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOOMERANG_FLY);
    }

    if (sGdfTridentState == GDF_TRIDENT_OUT) {
        // Homing: turn toward the target as it flies
        if ((sGdfTridentTarget != NULL) && (sGdfTridentTarget->update != NULL)) {
            aim.x = sGdfTridentTarget->focus.pos.x - sGdfTridentPos.x;
            aim.y = sGdfTridentTarget->focus.pos.y - sGdfTridentPos.y;
            aim.z = sGdfTridentTarget->focus.pos.z - sGdfTridentPos.z;
            dist = sqrtf(SQ(aim.x) + SQ(aim.y) + SQ(aim.z));
            if (dist > 1.0f) {
                sGdfTridentVel.x += ((aim.x / dist) * GDF_TRIDENT_THROW_SPEED - sGdfTridentVel.x) * 0.3f;
                sGdfTridentVel.y += ((aim.y / dist) * GDF_TRIDENT_THROW_SPEED - sGdfTridentVel.y) * 0.3f;
                sGdfTridentVel.z += ((aim.z / dist) * GDF_TRIDENT_THROW_SPEED - sGdfTridentVel.z) * 0.3f;
            }
            this->actor.yawTowardsPlayer = Math_Vec3f_Yaw(&this->actor.world.pos, &sGdfTridentPos);
        } else {
            sGdfTridentTarget = NULL;
        }
        sGdfTridentTravel += GDF_TRIDENT_THROW_SPEED;
        if ((sGdfTridentTravel > GDF_TRIDENT_RANGE) || (sGdfTridentTimer > 50)) {
            sGdfTridentState = GDF_TRIDENT_RETURNING;
        }
    } else {
        // Back to his hand
        aim.x = this->unk_2EC[8].x - sGdfTridentPos.x;
        aim.y = this->unk_2EC[8].y - sGdfTridentPos.y;
        aim.z = this->unk_2EC[8].z - sGdfTridentPos.z;
        dist = sqrtf(SQ(aim.x) + SQ(aim.y) + SQ(aim.z));
        if ((dist < 35.0f) || (sGdfTridentTimer > 120)) {
            // Caught
            sGdfTridentState = GDF_TRIDENT_IN_HAND;
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PICKOUT);
            BossGanon_Player_SetupSwordIdle(this, play);
            return;
        }
        sGdfTridentVel.x = (aim.x / dist) * GDF_TRIDENT_RETURN_SPEED;
        sGdfTridentVel.y = (aim.y / dist) * GDF_TRIDENT_RETURN_SPEED;
        sGdfTridentVel.z = (aim.z / dist) * GDF_TRIDENT_RETURN_SPEED;
        this->actor.yawTowardsPlayer = Math_Vec3f_Yaw(&this->actor.world.pos, &sGdfTridentPos);
    }

    sGdfTridentPos.x += sGdfTridentVel.x;
    sGdfTridentPos.y += sGdfTridentVel.y;
    sGdfTridentPos.z += sGdfTridentVel.z;

    // Into a wall: it glances off and comes back
    if ((sGdfTridentState == GDF_TRIDENT_OUT) &&
        BgCheck_EntityLineTest1(&play->colCtx, &prev, &sGdfTridentPos, &hit, &poly, true, true, true, true, &bgId)) {
        sGdfTridentPos = hit;
        sGdfTridentState = GDF_TRIDENT_RETURNING;
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
        EffectSsHitMark_SpawnFixedScale(play, 3, &hit); // the metal spark
    }

    // It hits everything it passes through, out and back
    this->collider.dim.radius = (s16)GDF_TRIDENT_HIT_RADIUS;
    this->collider.dim.height = (s16)(GDF_TRIDENT_HIT_RADIUS * 2.0f);
    this->collider.dim.yShift = 0;
    this->collider.dim.pos.x = (s16)sGdfTridentPos.x;
    this->collider.dim.pos.y = (s16)(sGdfTridentPos.y - GDF_TRIDENT_HIT_RADIUS);
    this->collider.dim.pos.z = (s16)sGdfTridentPos.z;
    BossGanon_Player_SetAT(this, play);
    if ((sGdfTridentState == GDF_TRIDENT_OUT) && (this->collider.base.atFlags & AT_HIT)) {
        this->collider.base.atFlags &= ~AT_HIT;
        sGdfTridentState = GDF_TRIDENT_RETURNING; // the first thing it strikes sends it back
    }
}

// ---- Grabbing by the throat (D-Pad Down) ----
// He seizes the person or monster in front of him by the throat and lifts them off their feet (they hang still in his
// fist, their own doings stopped); he can walk about with them. D-Pad Down or B again: he swings them up and slams
// them down on the ground ahead of him. People aren't hurt (they lie there a moment, then brush themselves off and
// get up); monsters are hurt badly.
#define GDF_GRAB_RANGE 150.0f
#define GDF_GRAB_MAX_HEIGHT 160.0f // taller than that (a Goron elder, a Big Goron) he can't lift
#define GDF_GRAB_DOWN_FRAMES 35
#define GDF_GRAB_THROW_FRAMES 18
#define GDF_GRAB_RELEASE_FRAME 9


// Still in the scene (actors can be killed or unloaded while he holds one)
static s32 BossGanon_Player_GrabAlive(PlayState* play, Actor* target) {
    static const u8 sCats[] = { ACTORCAT_NPC, ACTORCAT_ENEMY, ACTORCAT_CHEST };
    s32 i;
    Actor* actor;

    if (target == NULL) {
        return false;
    }
    for (i = 0; i < ARRAY_COUNT(sCats); i++) {
        for (actor = play->actorCtx.actorLists[sCats[i]].head; actor != NULL; actor = actor->next) {
            if (actor == target) {
                return actor->update != NULL;
            }
        }
    }
    return false;
}

static Actor* BossGanon_Player_FindGrabTarget(BossGanon* this, PlayState* play) {
    static const u8 sCats[] = { ACTORCAT_NPC, ACTORCAT_ENEMY };
    Actor* best = NULL;
    f32 bestScore = 0.0f;
    s32 i;
    Actor* actor;

    for (i = 0; i < ARRAY_COUNT(sCats); i++) {
        for (actor = play->actorCtx.actorLists[sCats[i]].head; actor != NULL; actor = actor->next) {
            f32 dist = Actor_WorldDistXZToActor(&this->actor, actor);
            f32 height = actor->focus.pos.y - actor->world.pos.y;
            s16 yawDiff = Actor_WorldYawTowardActor(&this->actor, actor) - this->actor.shape.rot.y;
            f32 score;

            if ((actor->update == NULL) || (actor->draw == NULL) || (actor->id == ACTOR_BOSS_GANON) ||
                (actor->id == ACTOR_EN_GANON_MANT) || (actor->id == ACTOR_EN_HORSE)) {
                continue;
            }
            // Another player's Link: only Ganondorf in Invasion mode (after the truce), and his game does the rest
            if ((GdfInvasion_DummyClientId(actor) != 0) && !GdfInvasion_CanGrabLink(GdfInvasion_DummyClientId(actor))) {
                continue;
            }
            if ((dist > GDF_GRAB_RANGE) || (ABS(yawDiff) > 0x3000) || (height > GDF_GRAB_MAX_HEIGHT) ||
                (fabsf(actor->world.pos.y - this->playerFloorY) > 80.0f)) {
                continue;
            }
            // The one he's turned toward wins over one closer off to the side
            score = (f32)ABS(yawDiff) + (dist * 12.0f);
            if ((best == NULL) || (score < bestScore)) {
                best = actor;
                bestScore = score;
            }
        }
    }
    return best;
}

static void BossGanon_Player_GrabToIdle(BossGanon* this, PlayState* play) {
    if (sPlayerSwordState != GDF_SWORD_OFF) {
        BossGanon_Player_SetupSwordIdle(this, play);
    } else {
        BossGanon_Player_SetupFloat(this, play);
    }
}

// Back on their feet, themselves again
static void BossGanon_Player_GrabRelease(PlayState* play) {
    Actor* actor = sGdfGrabActor;

    if (sGdfGrabNetClient != 0) {
        GdfInvasion_NetGrabEnd(sGdfGrabNetClient, false, 0);
        sGdfGrabNetClient = 0;
        sGdfGrabActor = NULL;
        sGdfGrabState = GDF_GRAB_NONE;
        return;
    }
    if ((actor != NULL) && BossGanon_Player_GrabAlive(play, actor)) {
        actor->shape.rot.x = actor->world.rot.x = 0;
        actor->shape.rot.z = actor->world.rot.z = 0;
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
        actor->speedXZ = 0.0f;
        actor->prevPos = actor->world.pos;
        actor->home.pos = actor->world.pos;
        actor->freezeTimer = 0;
        if (actor->category != sGdfGrabOrigCategory) {
            Actor_ChangeCategory(play, &play->actorCtx, actor, sGdfGrabOrigCategory);
        }
    }
    sGdfGrabActor = NULL;
    sGdfGrabState = GDF_GRAB_NONE;
}

// Invasion mode, world sync: who he's holding (a copy of the host Link's monster or NPC is moved there too)
Actor* BossGanon_Player_GetGrabbedActor(void) {
    return (sGdfGrabState != GDF_GRAB_NONE) ? sGdfGrabActor : NULL;
}

s32 BossGanon_Player_TryGrab(BossGanon* this, PlayState* play) {
    Actor* target;

    if ((sGdfGrabState != GDF_GRAB_NONE) || this->playerHidden || GanondorfSongs_IsOcarinaOut()) {
        return false;
    }
    // Invasion mode: 6 s after he lets go before he can grab again
    if ((GdfInvasion_Role() != GDF_INVASION_OFF) && (play->gameplayFrames - sGdfGrabBusyFrame < 6 * 20)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return false;
    }
    target = BossGanon_Player_FindGrabTarget(this, play);
    if (target == NULL) {
        return false;
    }
    sGdfGrabActor = target;
    sGdfGrabState = GDF_GRAB_HELD;
    sGdfGrabNetClient = GdfInvasion_DummyClientId(target);
    if (sGdfGrabNetClient != 0) {
        // Another player's Link: his own game lifts him into this fist (sent every frame with this pose)
        sGdfGrabIsEnemy = false;
        sGdfGrabNeck = 40.0f;
        sGdfGrabArmWeight = 0.0f;
        this->actionFunc = BossGanon_Player_Grab;
        Audio_PlayActorSound2(&this->actor, NA_SE_PL_PULL_UP_ROCK);
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN));
        return true;
    }
    sGdfGrabIsEnemy = target->category == ACTORCAT_ENEMY;
    // Drawn after him for as long as he has them (the last category drawn), so they can be put in his fist as his
    // hand is drawn, this frame (see BossGanon_Player_GrabFollowHand): drawn before him, they trailed a frame behind
    sGdfGrabOrigCategory = target->category;
    Actor_ChangeCategory(play, &play->actorCtx, target, ACTORCAT_CHEST);
    sGdfGrabNeck = CLAMP(target->focus.pos.y - target->world.pos.y, 15.0f, 110.0f);
    sGdfGrabArmWeight = 0.0f;
    target->freezeTimer = 2;
    this->actionFunc = BossGanon_Player_Grab;
    Audio_PlayActorSound2(&this->actor, NA_SE_PL_PULL_UP_ROCK);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_AT_RETURN));
    return true;
}

// Holding them up: he walks about freely, his right arm out with them dangling from his fist
void BossGanon_Player_Grab(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];

    SkelAnime_Update(&this->skelAnime);
    BossGanon_Player_KeepFloatRoot(this);
    if (this->playerGrounded) {
        BossGanon_Player_UpdateWalk(this, play);
    } else {
        this->playerWalkWeight = 0.0f;
    }
    Math_ApproachF(&sGdfGrabArmWeight, 1.0f, 1.0f, 0.25f);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfTridentArmPoses[1], sGdfGrabArmWeight);
    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.5f;

    if (sGdfGrabState != GDF_GRAB_HELD) {
        BossGanon_Player_GrabToIdle(this, play);
        return;
    }
    // A cutscene or conversation: he lets go
    if (!BossGanon_Player_LinkCanAttack(play)) {
        BossGanon_Player_GrabRelease(play);
        BossGanon_Player_GrabToIdle(this, play);
        return;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN) || CHECK_BTN_ALL(input->press.button, BTN_B)) {
        this->actionFunc = BossGanon_Player_GrabThrow;
        sGdfGrabTimer = 0;
        this->actor.yawTowardsPlayer = this->actor.shape.rot.y;
    }
}

// The slam: up over his head, then down hard on the ground in front of him
void BossGanon_Player_GrabThrow(BossGanon* this, PlayState* play) {
    Vec3s arm[3];
    f32 t;
    s32 i;

    SkelAnime_Update(&this->skelAnime);
    BossGanon_Player_KeepFloatRoot(this);
    Math_ApproachZeroF(&this->playerWalkWeight, 1.0f, 0.25f);
    if (this->playerGrounded) {
        BossGanon_Player_ApplyWalk(this);
    }
    sGdfGrabTimer++;
    if (sGdfGrabTimer <= 6) {
        // Up
        t = sGdfGrabTimer / 6.0f;
        for (i = 0; i < 3; i++) {
            BossGanon_Player_SlerpRot((Vec3s*)&sGdfTridentArmPoses[1][i], (Vec3s*)&sGdfTridentArmPoses[0][i], t,
                                      &arm[i]);
        }
    } else {
        // Down and through
        t = CLAMP((sGdfGrabTimer - 6) / 4.0f, 0.0f, 1.0f);
        for (i = 0; i < 3; i++) {
            BossGanon_Player_SlerpRot((Vec3s*)&sGdfTridentArmPoses[0][i], (Vec3s*)&sGdfClimbPoses[3][i], t,
                                      &arm[i]);
        }
    }
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, arm, 1.0f);
    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 1.0f;

    if (sGdfGrabTimer == 4) {
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_THROW));
    }
    if ((sGdfGrabTimer == GDF_GRAB_RELEASE_FRAME) && (sGdfGrabState == GDF_GRAB_HELD)) {
        // Let go on the way down: hurled at the ground ahead of him
        if (sGdfGrabNetClient != 0) {
            // His game throws him (no harm: he's knocked down and gets up)
            GdfInvasion_NetGrabEnd(sGdfGrabNetClient, true, this->actor.shape.rot.y);
            sGdfGrabNetClient = 0;
            sGdfGrabActor = NULL;
            sGdfGrabState = GDF_GRAB_NONE;
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
        }
    }
    if ((sGdfGrabTimer == GDF_GRAB_RELEASE_FRAME) && (sGdfGrabState == GDF_GRAB_HELD)) {
        sGdfGrabState = GDF_GRAB_FLYING;
        sGdfGrabVel.x = Math_SinS(this->actor.shape.rot.y) * 9.0f;
        sGdfGrabVel.z = Math_CosS(this->actor.shape.rot.y) * 9.0f;
        sGdfGrabVel.y = -6.0f;
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
    }
    if (sGdfGrabTimer >= GDF_GRAB_THROW_FRAMES) {
        BossGanon_Player_GrabToIdle(this, play);
    }
}

// As his right hand is drawn (they're drawn after him): the one he holds hangs by the throat from his fist
static void BossGanon_Player_GrabFollowHand(BossGanon* this) {
    static Vec3f sGrip = { GDF_GRIP_X, GDF_GRIP_Y, GDF_GRIP_Z };
    Actor* actor = sGdfGrabActor;
    Vec3f hand;

    if ((sGdfGrabState != GDF_GRAB_HELD) || (actor == NULL) || (actor->update == NULL)) {
        return;
    }
    Matrix_MultVec3f(&sGrip, &hand);
    if (sGdfGrabNetClient != 0) {
        GdfInvasion_NetGrabHold(sGdfGrabNetClient, &hand, sGdfGrabNeck, this->actor.shape.rot.y);
        return;
    }
    actor->world.pos.x = hand.x;
    actor->world.pos.y = hand.y - sGdfGrabNeck;
    actor->world.pos.z = hand.z;
    actor->focus.pos = hand;
}

// Runs every frame while someone's held, flying or lying where they landed
static void BossGanon_Player_UpdateGrabbed(BossGanon* this, PlayState* play) {
    static Vec3f sGrip = { GDF_GRIP_X, GDF_GRIP_Y, GDF_GRIP_Z };
    Actor* actor = sGdfGrabActor;
    Vec3f hand = this->unk_2EC[8];
    CollisionPoly* poly;
    Vec3f probe;
    f32 floorY;

    if (sGdfGrabState == GDF_GRAB_NONE) {
        return;
    }
    if (!BossGanon_Player_GrabAlive(play, actor)) {
        if (sGdfGrabNetClient != 0) {
            GdfInvasion_NetGrabEnd(sGdfGrabNetClient, false, 0);
            sGdfGrabNetClient = 0;
        }
        sGdfGrabActor = NULL;
        sGdfGrabState = GDF_GRAB_NONE;
        return;
    }
    // Another player's Link: nothing to do here but let go if he's interrupted (his game holds him)
    if (sGdfGrabNetClient != 0) {
        if ((sGdfGrabState != GDF_GRAB_HELD) ||
            ((this->actionFunc != BossGanon_Player_Grab) && (this->actionFunc != BossGanon_Player_GrabThrow))) {
            BossGanon_Player_GrabRelease(play);
        }
        return;
    }
    // Their own update is held off the whole time: no walking off, no attacking
    actor->freezeTimer = 2;

    // Inside his right fist (where the trident's held), from the hand as last drawn, carried along by however far
    // he's moved since: they're drawn before him, so this keeps them in his grip instead of trailing behind it
    if (this->playerHandMtxValid) {
        Matrix_Push();
        Matrix_Put(&this->playerRightHandMtx);
        Matrix_MultVec3f(&sGrip, &hand);
        Matrix_Pop();
        hand.x += this->actor.world.pos.x - this->actor.prevPos.x;
        hand.y += this->actor.world.pos.y - this->actor.prevPos.y;
        hand.z += this->actor.world.pos.z - this->actor.prevPos.z;
    }

    switch (sGdfGrabState) {
        case GDF_GRAB_HELD:
            // Hanging by the throat from his fist, facing him, legs kicking a little
            if ((this->actionFunc != BossGanon_Player_Grab) && (this->actionFunc != BossGanon_Player_GrabThrow)) {
                // Interrupted (hurt, ...): dropped
                sGdfGrabState = GDF_GRAB_FLYING;
                sGdfGrabVel.x = sGdfGrabVel.z = 0.0f;
                sGdfGrabVel.y = 0.0f;
                break;
            }
            actor->world.pos.x = hand.x;
            actor->world.pos.y = hand.y - sGdfGrabNeck;
            actor->world.pos.z = hand.z;
            actor->shape.rot.y = actor->world.rot.y = this->actor.shape.rot.y + 0x8000;
            actor->shape.rot.z = (s16)(Math_SinS(play->gameplayFrames * 0x1800) * 0x300);
            actor->shape.rot.x = 0;
            actor->focus.pos = hand;
            break;

        case GDF_GRAB_FLYING:
            actor->world.pos.x += sGdfGrabVel.x;
            actor->world.pos.y += sGdfGrabVel.y;
            actor->world.pos.z += sGdfGrabVel.z;
            sGdfGrabVel.y -= 2.5f;
            Math_ApproachS(&actor->shape.rot.x, -0x4000, 2, 0x1800); // thrown over onto their back
            actor->shape.rot.z = 0;
            probe = actor->world.pos;
            probe.y += 60.0f;
            floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
            if (floorY <= BGCHECK_Y_MIN) {
                floorY = this->playerFloorY;
            }
            if (actor->world.pos.y <= floorY) {
                // The slam
                Vec3f zero = { 0.0f, 0.0f, 0.0f };

                actor->world.pos.y = floorY;
                actor->shape.rot.x = -0x4000;
                Actor_RequestQuakeAndRumble(&this->actor, play, 4, 12);
                Audio_PlayActorSound2(actor, NA_SE_IT_HAMMER_HIT);
                Audio_PlayActorSound2(actor, NA_SE_PL_BODY_HIT);
                Actor_SpawnFloorDustRing(play, actor, &actor->world.pos, 30.0f, 12, 6.0f, 500, 20, true);
                EffectSsHitMark_SpawnFixedScale(play, 0, &actor->world.pos);
                (void)zero;
                sGdfGrabState = GDF_GRAB_DOWN;
                sGdfGrabTimer = GDF_GRAB_DOWN_FRAMES;
            }
            actor->prevPos = actor->world.pos;
            actor->focus.pos = actor->world.pos;
            break;

        case GDF_GRAB_DOWN:
            // Lying there, then up again (a monster takes the blow as it comes to)
            if (--sGdfGrabTimer <= 0) {
                Vec3f pos = actor->world.pos;

                BossGanon_Player_GrabRelease(play);
                if (sGdfGrabIsEnemy && (actor->colChkInfo.health > 0)) {
                    // Monsters are hurt badly: most of their health goes in the slam, and the blow that lands as
                    // they come to finishes the weaker ones (people just pick themselves up and dust themselves off)
                    actor->colChkInfo.health = MAX(actor->colChkInfo.health - 6, 1);
                    this->collider.dim.radius = 50;
                    this->collider.dim.height = 80;
                    this->collider.dim.yShift = 0;
                    this->collider.dim.pos.x = (s16)pos.x;
                    this->collider.dim.pos.y = (s16)(pos.y - 20.0f);
                    this->collider.dim.pos.z = (s16)pos.z;
                    BossGanon_Player_SetAT(this, play);
                } else {
                    Actor_SpawnFloorDustRing(play, actor, &pos, 20.0f, 6, 3.0f, 300, 15, false);
                }
            }
            break;
    }
}

// Both fists up over his head with the item between them, like Link holding up what he found
void BossGanon_Player_ItemGet(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!(player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) || (player->unk_862 == 0) || this->playerHidden) {
        BossGanon_Player_SetupFloat(this, play);
        return;
    }

    SkelAnime_Update(&this->skelAnime);
    this->playerWalkWeight = 0.0f;
    Math_ApproachF(&this->playerItemWeight, 1.0f, 1.0f, 0.2f);
    BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfItemPoses[0], this->playerItemWeight);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfItemPoses[1], this->playerItemWeight);

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;
}

// Where the item he's holding up is drawn: between his fists (from the last draw)
s32 BossGanon_Player_GetItemHoldPos(Actor* thisx, Vec3f* pos) {
    BossGanon* this = (BossGanon*)thisx;

    if ((thisx == NULL) || !GDF_IS_PLAYER(this) || (this->actionFunc != BossGanon_Player_ItemGet) ||
        this->playerHidden || (this->playerItemWeight < 0.5f)) {
        return false;
    }
    pos->x = (this->unk_238.x + this->unk_22C.x) * 0.5f;
    pos->y = (this->unk_238.y + this->unk_22C.y) * 0.5f;
    pos->z = (this->unk_238.z + this->unk_22C.z) * 0.5f;
    return true;
}

// King Dodongo swallows his light balls like bombs (he can't throw bombs): a player light ball or big magic ball
// near the mouth of something that eats explosives is used up and counts as one
s32 BossGanon_Player_SwallowLightBall(PlayState* play, Vec3f* pos, f32 radius) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_BOSS].head;

    while (actor != NULL) {
        if ((actor->id == ACTOR_BOSS_GANON) && (actor->update != NULL) &&
            ((actor->params == GDF_PARAMS_PLAYER_LIGHT_BALL) ||
             ((actor->params >= GDF_PARAMS_PLAYER_BIG_MAGIC) && (actor->params < GDF_PARAMS_PLAYER_BIG_MAGIC + 5))) &&
            (Math_Vec3f_DistXYZ(&actor->world.pos, pos) < radius)) {
            Actor_Kill(actor);
            return true;
        }
        actor = actor->next;
    }
    return false;
}

void BossGanon_Player_SetupLaugh(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfRaiseHandStartAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfRaiseHandStartAnim, -5.0f);
    this->actionFunc = BossGanon_Player_Laugh;
    this->unk_1C2 = 0;
    this->timers[0] = 75;
    this->handLightBallScale = 0.0f;
    this->playerBtnHeld = 0;
    this->triforceType = GDF_TRIFORCE_DORF;

    if (this->fwork[GDF_TRIFORCE_PRIM_A] <= 0.0f) {
        this->fwork[GDF_TRIFORCE_SCALE] = 10.0f;
        this->fwork[GDF_TRIFORCE_PRIM_B] = 255.0f;
        this->fwork[GDF_TRIFORCE_ENV_G] = 100.0f;
    }

    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
}

void BossGanon_Player_Laugh(BossGanon* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    BossGanon_Player_KeepFloatRoot(this);

    this->legSwayEnabled = true;
    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    // Keep facing the same way
    this->actor.yawTowardsPlayer = this->actor.shape.rot.y;

    if ((this->unk_1C2 == 0) && Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfRaiseHandLoopAnim, -5.0f);
        this->unk_1C2 = 1;
    }

    if (this->timers[0] == 60) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_TRIFORCE_MARK);
    }

    if (this->timers[0] < 62) {
        Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_A], 255.0f, 1.0f, 10.0f);
        Math_ApproachF(&this->fwork[GDF_TRIFORCE_SCALE], 0.6f, 1.0f, 0.3f);
        Math_ApproachF(&this->fwork[GDF_TRIFORCE_PRIM_B], 170.0f, 1.0f, 2.55f);
        Math_ApproachF(&this->fwork[GDF_TRIFORCE_ENV_G], 200.0f, 1.0f, 3.0f);
    }

    if (this->timers[0] == 0) {
        BossGanon_Player_SetupFloat(this, play);
    }
}

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", L3: his organ rises out of the ground and he plays Ganondorf's Theme
// ------------------------------------------------------------------------------------------------

// In his intro Ganondorf sits at (0, 112, -333) facing +z, and the organ is drawn at the room origin.
#define GDF_ORGAN_SEAT_HEIGHT 112.0f
#define GDF_ORGAN_SEAT_DIST 333.0f
// CRC64 of "overlays/ovl_En_Ganon_Organ/sRoomCarpetTex". The organ model also contains the long carpet of his
// chamber. The carpet on the raised stone slab (its top, the steps and the hanging part behind the organ) stays, so
// he has something to stand on; the flat run of carpet beyond the slab, across the chamber floor, is left out. That
// run is the one batch of carpet vertices loaded from this byte offset of the organ's vertex data.
#define GDF_ORGAN_CARPET_TEX_HASH 0x556DB73EBD8DB734ULL
#define GDF_ORGAN_CARPET_RUN_VTX_OFFSET 0x2C0
#define GDF_ORGAN_VTX_PATH "__OTR__overlays/ovl_En_Ganon_Organ/ovl_En_Ganon_OrganVtx_0019E8"

static s32 BossGanon_Player_IsTwoPartGfx(u8 opcode) {
    return (opcode == G_SETTIMG_OTR_HASH) || (opcode == G_DL_OTR_HASH) || (opcode == G_VTX_OTR_HASH) ||
           (opcode == G_BRANCH_Z_OTR) || (opcode == G_MARKER) || (opcode == G_MTX_OTR) || (opcode == G_MOVEMEM_OTR);
}

// Copy of sRoomOrganAndFloorDL without the carpet beyond the stone slab
static Gfx* BossGanon_Player_BuildOrganDL(PlayState* play) {
    Gfx* src = ResourceMgr_LoadGfxByName(sRoomOrganAndFloorDL);
    Gfx* dst;
    s32 count = 0;
    s32 i;
    u8 isCarpet = false;
    u8 isCarpetRun = false;

    if (src == NULL) {
        return NULL;
    }

    // Find the end, stepping over the second half of two-part commands
    while (count < 4096) {
        u8 opcode = (u8)(src[count].words.w0 >> 24);

        if (opcode == G_ENDDL) {
            break;
        }
        count += BossGanon_Player_IsTwoPartGfx(opcode) ? 2 : 1;
    }
    if (count >= 4096) {
        return NULL;
    }

    dst = Graph_Alloc(play->state.gfxCtx, (count + 1) * sizeof(Gfx));

    for (i = 0; i <= count; i++) {
        u8 opcode = (u8)(src[i].words.w0 >> 24);

        dst[i] = src[i];

        if (BossGanon_Player_IsTwoPartGfx(opcode)) {
            dst[i + 1] = src[i + 1];

            if (opcode == G_SETTIMG_OTR_HASH) {
                u64 hash = ((u64)(u32)src[i + 1].words.w0 << 32) | (u64)(u32)src[i + 1].words.w1;
                isCarpet = (hash == GDF_ORGAN_CARPET_TEX_HASH);
            } else if (opcode == G_VTX_OTR_HASH) {
                uintptr_t offset = src[i].words.w1;

                // Once the real organ has been drawn, the renderer has replaced the offset with a pointer
                if (offset > 0xFFFFF) {
                    Vtx* base = ResourceMgr_LoadVtxByName(GDF_ORGAN_VTX_PATH);

                    offset = (base != NULL) ? (offset - (uintptr_t)base) : 0;
                }
                isCarpetRun = (offset == GDF_ORGAN_CARPET_RUN_VTX_OFFSET);
            }
            i++;
            continue;
        }

        if (isCarpet && isCarpetRun && ((opcode == G_TRI1) || (opcode == G_TRI2) || (opcode == G_QUAD))) {
            dst[i].words.w0 = (uintptr_t)G_NOOP << 24;
            dst[i].words.w1 = 0;
        }
    }

    return dst;
}

static void BossGanon_Player_DrawOrgan(BossGanon* this, PlayState* play) {
    Gfx* organDL;
    Gfx* emptyDL;

    if (this->playerOrganSink >= GDF_ORGAN_SINK_DEPTH) {
        return;
    }

    organDL = BossGanon_Player_BuildOrganDL(play);
    if (organDL == NULL) {
        return;
    }

    emptyDL = Graph_Alloc(play->state.gfxCtx, sizeof(Gfx));
    gSPEndDisplayList(emptyDL);

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // Used by the organ actor to fade parts of the room, not needed here
    gSPSegment(POLY_OPA_DISP++, 0x08, emptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, emptyDL);

    // Keep Ganondorf's own matrix (with his 0.01 scale) for when he's drawn right after this
    Matrix_Push();
    Matrix_Translate(this->playerOrganPos.x, this->playerOrganPos.y - this->playerOrganSink, this->playerOrganPos.z,
                     MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->playerOrganYaw), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, organDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void BossGanon_Player_StartOrganMusic(BossGanon* this) {
    if (!this->playerOrganMusic) {
        this->playerPrevBgm = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_GANON_TOWER);
        this->playerOrganMusic = true;
    }
}

void BossGanon_Player_StopOrganMusic(BossGanon* this, PlayState* play) {
    u16 prev = this->playerPrevBgm;

    if (!this->playerOrganMusic) {
        return;
    }
    this->playerOrganMusic = false;

    if ((prev & 0xFF) == NA_BGM_NATURE_AMBIENCE) {
        Audio_PlayNatureAmbienceSequence(play->sequenceCtx.natureAmbienceId);
    } else if ((prev == NA_BGM_DISABLED) || ((prev & 0xFF) == 0xFF)) {
        Audio_QueueSeqCmd(NA_BGM_STOP);
    } else {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | prev);
    }
}

void BossGanon_Player_SetupOrgan(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actionFunc = BossGanon_Player_Organ;
    this->unk_1C2 = 0;
    this->playerBtnHeld = 0;
    this->handLightBallScale = 0.0f;

    // A new spot, unless the last organ hasn't finished sinking yet
    if (this->playerOrganSink >= GDF_ORGAN_SINK_DEPTH - 1.0f) {
        // He plays with his back to the camera, so the organ appears in front of Link
        this->playerOrganYaw = player->actor.shape.rot.y + 0x8000;
        this->playerOrganPos.x = player->actor.world.pos.x + (Math_SinS(this->playerOrganYaw) * GDF_ORGAN_SEAT_DIST);
        this->playerOrganPos.y = player->actor.world.pos.y;
        this->playerOrganPos.z = player->actor.world.pos.z + (Math_CosS(this->playerOrganYaw) * GDF_ORGAN_SEAT_DIST);
    }

    Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, -5.0f);
    Actor_RequestQuakeAndRumble(&this->actor, play, 6, 20);
}

// Close-up camera while he plays: as if a phone were lying on the keys a little to his right, looking up at him
#define GDF_ORGAN_CAM_SIDE 75.0f    // organ space: he faces the keys along -z, so +x is his right
#define GDF_ORGAN_CAM_HEIGHT 150.0f // above the keyboard (y 110), level with his chest
#define GDF_ORGAN_CAM_Z -405.0f     // just past the keys (z -397 to -373), in front of the pipes; he sits at z -333

static void BossGanon_Player_ReleaseOrganCam(BossGanon* this, PlayState* play) {
    if (this->playerOrganCam != 0) {
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        Play_ClearCamera(play, this->playerOrganCam);
        this->playerOrganCam = 0;
    }
}

// A cutscene is starting: give the camera back first, or the cutscene records the organ camera as the one to return
// to, and that camera is gone by the time it does (a crash after songs that start cutscenes)
void BossGanon_Player_ReleaseCameraForCutscene(PlayState* play) {
    if ((sPlayerGanondorf != NULL) && (sPlayerGanondorf->playerOrganCam != 0)) {
        BossGanon_Player_ReleaseOrganCam(sPlayerGanondorf, play);
    }
}

// Organ space (the organ model's own coordinates) to the world
static void BossGanon_Player_OrganToWorld(BossGanon* this, f32 x, f32 y, f32 z, Vec3f* out) {
    f32 s = Math_SinS(this->playerOrganYaw);
    f32 c = Math_CosS(this->playerOrganYaw);

    out->x = this->playerOrganPos.x + (x * c) + (z * s);
    out->y = this->playerOrganPos.y - this->playerOrganSink + y;
    out->z = this->playerOrganPos.z - (x * s) + (z * c);
}

static void BossGanon_Player_UpdateOrganCam(BossGanon* this, PlayState* play) {
    u8 closeUp = CVarGetInteger(CVAR_PLAY_AS_GDF("OrganCamera"), 1);
    Vec3f eye;
    Vec3f at;

    // As his ocarina the game's own ocarina close-up would sit inside his legs (it's made for Link's height), so the
    // organ always gets its own camera then: the close-up, or with that off a wide shot from behind his right
    // A song can start a cutscene (the Door of Time, Darunia's dance): the cutscene owns the camera from then on
    if ((!closeUp && !this->playerOrganOcarina) || this->playerHidden || (play->csCtx.state != CS_STATE_IDLE)) {
        BossGanon_Player_ReleaseOrganCam(this, play);
        return;
    }
    if (this->playerOrganCam == 0) {
        this->playerOrganCam = Play_CreateSubCamera(play);
        if (this->playerOrganCam == 0) {
            return;
        }
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, this->playerOrganCam, CAM_STAT_ACTIVE);
    }

    // Where the organ ends up, so the camera is already in place while it rises
    if (closeUp) {
        BossGanon_Player_OrganToWorld(this, GDF_ORGAN_CAM_SIDE, GDF_ORGAN_CAM_HEIGHT, GDF_ORGAN_CAM_Z, &eye);
        eye.y += this->playerOrganSink;
        // His face: unk_208 is where his eyes are drawn (body part 2, used before, is his pelvis)
        at = this->unk_208;
        at.y -= 10.0f;
    } else {
        BossGanon_Player_OrganToWorld(this, 170.0f, 210.0f, -110.0f, &eye);
        BossGanon_Player_OrganToWorld(this, 0.0f, 150.0f, -380.0f, &at);
        eye.y += this->playerOrganSink;
        at.y += this->playerOrganSink;
    }
    Play_CameraSetAtEye(play, this->playerOrganCam, &at, &eye);
    Play_CameraSetFov(play, this->playerOrganCam, 70.0f);
}

void BossGanon_Player_Organ(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];

    SkelAnime_Update(&this->skelAnime);

    this->actor.yawTowardsPlayer = this->playerOrganYaw;
    Math_ApproachF(&this->actor.world.pos.y, this->playerOrganPos.y + GDF_ORGAN_SEAT_HEIGHT, 0.2f, 10.0f);
    // Seated on the bench: his intro lowers the model by 70 with this offset
    Math_ApproachF(&this->actor.shape.yOffset, -7000.0f, 0.3f, 700.0f);

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    switch (this->unk_1C2) {
        case 0: // the organ rises
            BossGanon_Player_UpdateOrganCam(this, play);
            // Rises silently, then slams into place
            Math_ApproachZeroF(&this->playerOrganSink, 1.0f, 25.0f);

            if (this->playerOrganSink == 0.0f) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_STONE_BOUND);
                Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_HIT_GND_IMP));
                Actor_RequestQuakeAndRumble(&this->actor, play, 6, 12);
                BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfPlayOrganAnim, -5.0f);
                // As his ocarina, the songs are the music
                if (!this->playerOrganOcarina) {
                    BossGanon_Player_StartOrganMusic(this);
                }
                this->unk_1C2 = 1;
            }
            break;

        case 1: // playing, until L3, B or A (as his ocarina: until Link puts it away)
            BossGanon_Player_UpdateOrganCam(this, play);
            if (this->playerOrganOcarina ? !(GET_PLAYER(play)->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING)
                                         : (CHECK_BTN_ANY(input->press.button, BTN_B | BTN_A) ||
                                            PlayAsGanondorf_OrganButtonPressed() ||
                                            !BossGanon_Player_CanStartAttack(this, play))) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfStopPlayingOrganAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfStopPlayingOrganAnim, -5.0f);
                BossGanon_Player_StopOrganMusic(this, play);
                BossGanon_Player_ReleaseOrganCam(this, play);
                this->unk_1C2 = 2;
            }
            break;

        case 2: // stops, then the organ sinks again
            if (Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
                BossGanon_Player_SetupFloat(this, play);
            }
            break;
    }
}

// ================================================================================================
// SoH: "Play as Ganondorf", entry points. Each one switches sGanondorf/sCape/sCurEffects to the Ganondorf the actor
// belongs to, so the real fight and the player-controlled Ganondorf don't overwrite each other's state.
// ================================================================================================

// 1 = belongs to the player-controlled Ganondorf, 0 = to the real one, -1 = can't tell yet
static s32 BossGanon_GetOwner(BossGanon* this) {
    Actor* parent = this->actor.parent;

    if (BossGanon_IsPlayerAttack(&this->actor) || (this->actor.params == GDF_PARAMS_PUPPET)) {
        return 1;
    }
    if (this->actor.params < 0x64) {
        return 0;
    }
    // Projectiles and effects get their parent right after their init
    if ((parent != NULL) && (parent->id == ACTOR_BOSS_GANON)) {
        return (parent->params == GDF_PARAMS_PLAYER) || (parent->params == GDF_PARAMS_PUPPET);
    }
    return -1;
}

#define BOSS_GANON_CONTEXT_ENTRY(name)                                \
    void name(Actor* thisx, PlayState* play) {                        \
        BossGanon* this = (BossGanon*)thisx;                          \
        BossGanonContext saved = BossGanon_SaveContext();             \
        s32 owner = BossGanon_GetOwner(this);                         \
                                                                      \
        if (owner >= 0) {                                             \
            BossGanon_UseContext(owner);                              \
        }                                                             \
        name##_Impl(thisx, play);                                     \
        /* Ganondorf himself sets sGanondorf and sCape in his init */ \
        if ((owner == 1) && GDF_IS_PLAYER(this)) {                    \
            sPlayerGanondorf = sGanondorf;                            \
            sPlayerCape = sCape;                                      \
        } else if ((owner == 0) && (this->actor.params < 0x64)) {     \
            sVanillaGanondorf = sGanondorf;                           \
            sVanillaCape = sCape;                                     \
        }                                                             \
        BossGanon_RestoreContext(saved);                              \
    }

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf", Invasion mode: another player's Ganondorf on this screen. He's drawn exactly as the
// player-controlled one (cape, sword, hands), but all he does is take his pose from the network every frame.
// ------------------------------------------------------------------------------------------------

// What his cape, glows, aura and organ are doing, packed into numbers (and back), in this order
#define GDF_FX_SHOCK 48 // 18 shock sizes from here
static void BossGanon_Puppet_PackFx(BossGanon* this, EnGanonMant* cape, f32* fx) {
    s32 i;

    memset(fx, 0, GDF_NET_FX * sizeof(f32));
    if (cape != NULL) {
        fx[0] = cape->backPush;
        fx[1] = cape->backSwayMagnitude;
        fx[2] = cape->sideSwayMagnitude;
        fx[3] = cape->minDist;
        fx[4] = cape->minY;
        fx[5] = cape->attachLeftArmTimer;
        fx[6] = cape->attachRightArmTimer;
        fx[7] = cape->attachShouldersTimer;
        fx[47] = cape->gravity;
    }
    fx[8] = this->shockGlow;
    fx[9] = this->unk_2E6;
    fx[10] = this->unk_2E8;
    fx[11] = this->handLightBallScale;
    fx[12] = this->unk_258;
    fx[13] = this->unk_260.x;
    fx[14] = this->unk_260.y;
    fx[15] = this->unk_260.z;
    fx[16] = this->unk_278.x;
    fx[17] = this->unk_278.y;
    fx[18] = this->unk_278.z;
    fx[19] = this->unk_284;
    fx[20] = this->unk_288;
    fx[21] = this->unk_28C;
    fx[22] = this->unk_290;
    fx[23] = this->unk_1AA;
    fx[24] = this->unk_2D0;
    for (i = 0; i < GDF_FWORK_MAX; i++) {
        fx[25 + i] = this->fwork[i];
    }
    fx[35] = this->triforcePos.x;
    fx[36] = this->triforcePos.y;
    fx[37] = this->triforcePos.z;
    fx[38] = this->triforceType;
    fx[39] = this->csState;
    fx[40] = this->playerAuraScale;
    fx[41] = this->playerAuraAlpha;
    fx[42] = this->playerOrganSink;
    fx[43] = this->playerOrganPos.x;
    fx[44] = this->playerOrganPos.y;
    fx[45] = this->playerOrganPos.z;
    fx[46] = this->playerOrganYaw;
    for (i = 0; i < 18; i++) {
        fx[GDF_FX_SHOCK + i] = this->unk_49C[i];
    }
    // The trident spun overhead or thrown
    fx[66] = sGdfTridentState;
    fx[67] = sGdfTridentPos.x;
    fx[68] = sGdfTridentPos.y;
    fx[69] = sGdfTridentPos.z;
    fx[70] = sGdfTridentSpin;
    fx[71] = this->playerOrganMusic; // his organ's music, heard by the Links too
    fx[72] = this->actor.shape.yOffset; // lowered onto the organ bench (and the like)
    fx[73] = sGdfSummonBurst;           // the trident bursting into his hand
}

static void BossGanon_Puppet_UnpackFx(BossGanon* this, EnGanonMant* cape, const f32* fx) {
    s32 i;

    if (cape != NULL) {
        cape->backPush = fx[0];
        cape->backSwayMagnitude = fx[1];
        cape->sideSwayMagnitude = fx[2];
        cape->minDist = fx[3];
        cape->minY = fx[4];
        cape->attachLeftArmTimer = fx[5];
        cape->attachRightArmTimer = fx[6];
        cape->attachShouldersTimer = fx[7];
        cape->gravity = (fx[47] != 0.0f) ? fx[47] : -3.0f;
    }
    this->shockGlow = (u8)fx[8];
    this->unk_2E6 = (s16)fx[9];
    this->unk_2E8 = (s16)fx[10];
    this->handLightBallScale = fx[11];
    this->unk_258 = fx[12];
    this->unk_260.x = fx[13];
    this->unk_260.y = fx[14];
    this->unk_260.z = fx[15];
    this->unk_278.x = fx[16];
    this->unk_278.y = fx[17];
    this->unk_278.z = fx[18];
    this->unk_284 = fx[19];
    this->unk_288 = fx[20];
    this->unk_28C = fx[21];
    this->unk_290 = fx[22];
    this->unk_1AA = (s16)fx[23];
    this->unk_2D0 = fx[24];
    for (i = 0; i < GDF_FWORK_MAX; i++) {
        this->fwork[i] = fx[25 + i];
    }
    this->triforcePos.x = fx[35];
    this->triforcePos.y = fx[36];
    this->triforcePos.z = fx[37];
    this->triforceType = (s16)fx[38];
    this->csState = (s16)fx[39];
    sGdfTridentState = (u8)fx[66];
    sGdfTridentPos.x = fx[67];
    sGdfTridentPos.y = fx[68];
    sGdfTridentPos.z = fx[69];
    sGdfTridentSpin = (s16)fx[70];
    this->playerAuraScale = fx[40];
    this->playerAuraAlpha = fx[41];
    this->playerOrganSink = fx[42];
    this->playerOrganPos.x = fx[43];
    this->playerOrganPos.y = fx[44];
    this->playerOrganPos.z = fx[45];
    this->playerOrganYaw = (s16)fx[46];
    this->actor.shape.yOffset = fx[72]; // seated at his organ, not standing on its keys
    {
        // The trident bursting into his hand: the burst on this screen too, once
        static u8 sPrevBurst = 0;
        u8 burst = (u8)fx[73];

        if ((burst > sPrevBurst) && (sPrevBurst == 0)) {
            if (burst > GDF_VANISH_BURST) {
                BossGanon_Player_TridentVanish(this, gPlayState, &this->unk_2EC[8], false);
            } else {
                BossGanon_Player_TridentBurst(this, gPlayState, &this->unk_2EC[8], false);
            }
        }
        sPrevBurst = burst;
    }
    for (i = 0; i < 18; i++) {
        this->unk_49C[i] = fx[GDF_FX_SHOCK + i];
    }
}

// His light balls and Big Magic in flight
static void BossGanon_Puppet_PackBalls(BossGanon* this, GdfNetPose* out) {
    PlayState* play = gPlayState;
    s32 cat;
    Actor* actor;

    out->ballCount = 0;
    if (play == NULL) {
        return;
    }
    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (actor = play->actorCtx.actorLists[cat].head; actor != NULL; actor = actor->next) {
            BossGanon* ball = (BossGanon*)actor;
            GdfNetBall* b;

            if ((actor->id != ACTOR_BOSS_GANON) || (actor->update == NULL) ||
                (actor->params < GDF_PARAMS_PLAYER_LIGHT_BALL) ||
                (actor->params >= GDF_PARAMS_PLAYER_BIG_MAGIC + GDF_PLAYER_BIG_MAGIC_COUNT) ||
                (out->ballCount >= GDF_NET_BALLS)) {
                continue;
            }
            b = &out->balls[out->ballCount++];
            b->kind = (actor->params != GDF_PARAMS_PLAYER_LIGHT_BALL) ? 1 : (actor->home.rot.z == 1) ? 2 : 0;
            b->pos = actor->world.pos;
            b->rot = actor->world.rot;
            b->scale = actor->scale.x;
            b->fwork0 = ball->fwork[GDF_FWORK_0];
            b->fwork1 = ball->fwork[GDF_FWORK_1];
            b->floorY = actor->floorHeight;
        }
    }
}

// The local player's Ganondorf, as sent to the others
s32 BossGanon_Player_GetNetPose(GdfNetPose* out) {
    BossGanon* this = sPlayerGanondorf;
    s32 n;

    if ((this == NULL) || (this->actor.update == NULL) || (this->actor.params != GDF_PARAMS_PLAYER)) {
        return false;
    }
    out->valid = true;
    out->hidden = this->playerHidden;
    out->swordState = sPlayerSwordState;
    out->pos = this->actor.world.pos;
    out->rotY = this->actor.shape.rot.y;
    out->floorY = this->playerFloorY;
    n = MIN(this->skelAnime.limbCount, GDF_NET_JOINTS);
    memcpy(out->joints, this->skelAnime.jointTable, n * sizeof(Vec3s));
    BossGanon_Puppet_PackFx(this, sPlayerCape, out->fx);
    out->fxCount = GDF_NET_FX;
    BossGanon_Puppet_PackBalls(this, out);
    return true;
}

// His light balls and Big Magic on the others' screens: just for show (they don't hit anything here, he can't hurt
// the Links), each one placed where his is
static void BossGanon_PuppetBall_Update(Actor* thisx, PlayState* play) {
    // Moved by the puppet
}

// The fireball ring's balls burn: flames licking up and trailing off them
static void BossGanon_FireballFlames(PlayState* play, Vec3f* pos, s16 frame) {
    Vec3f vel = { 0.0f, 1.5f, 0.0f };
    Vec3f accel = { 0.0f, 0.4f, 0.0f };
    Vec3f p;
    s32 i;

    for (i = 0; i < 2; i++) {
        p.x = pos->x + Rand_CenteredFloat(22.0f);
        p.y = pos->y + Rand_CenteredFloat(22.0f);
        p.z = pos->z + Rand_CenteredFloat(22.0f);
        EffectSsDeadDb_Spawn(play, &p, &vel, &accel, 35 + (s16)Rand_ZeroFloat(25.0f), -2, 255, 220, 60, 255, 255, 40,
                             0, 1, 8, false);
    }
}

// A Link's game: his sword swung at the ball as it reaches him sends it back to Ganondorf; if it reaches him first,
// he's hit
static void BossGanon_PuppetBall_Volley(BossGanon* ball, PlayState* play) {
    Player* link = GET_PLAYER(play);
    Vec3f center;
    f32 dist;

    if (ball->timers[0] > 0) {
        ball->timers[0]--;
        return;
    }
    if ((link == NULL) || (gSaveContext.health <= 0) || (link->stateFlags1 & PLAYER_STATE1_DEAD)) {
        return;
    }
    center = link->actor.world.pos;
    center.y += 40.0f;
    dist = Math_Vec3f_DistXYZ(&ball->actor.world.pos, &center);
    if ((link->meleeWeaponState != 0) && (dist < 110.0f)) {
        GdfInvasion_SendVolley(&ball->actor.world.pos);
        Audio_PlayActorSound2(&link->actor, NA_SE_IT_SWORD_REFLECT_MG);
        ball->timers[0] = 20;
    } else if (dist < 45.0f) {
        GdfInvasion_SendBallHit(&ball->actor.world.pos);
        GdfInvasion_BallHurtLink(play);
        ball->timers[0] = 30;
    }
}

// A Link's game: Big Magic can't be sent back; only a spin attack breaks it, anything else and he's hit
static void BossGanon_PuppetBall_BigMagic(BossGanon* ball, PlayState* play) {
    Player* link = GET_PLAYER(play);
    Vec3f center;
    f32 dist;
    u8 spinning;

    if (ball->timers[0] > 0) {
        ball->timers[0]--;
        return;
    }
    if ((link == NULL) || (gSaveContext.health <= 0) || (link->stateFlags1 & PLAYER_STATE1_DEAD)) {
        return;
    }
    center = link->actor.world.pos;
    center.y += 40.0f;
    dist = Math_Vec3f_DistXYZ(&ball->actor.world.pos, &center);
    spinning = (link->meleeWeaponState != 0) && (link->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H) &&
               (link->meleeWeaponAnimation <= PLAYER_MWA_BIG_SPIN_2H);
    if (spinning && (dist < 160.0f)) {
        GdfInvasion_SendBallHit(&ball->actor.world.pos); // broken
        Audio_PlayActorSound2(&link->actor, NA_SE_IT_SWORD_REFLECT_MG);
        ball->timers[0] = 30;
    } else if (dist < 55.0f) {
        GdfInvasion_SendBallHit(&ball->actor.world.pos);
        GdfInvasion_BallHurtLink(play);
        ball->timers[0] = 30;
    }
}

static void BossGanon_Puppet_UpdateBalls(BossGanon* this, PlayState* play, GdfNetPose* pose) {
    s32 i;
    s32 j;

    for (i = 0; i < GDF_NET_BALLS; i++) {
        BossGanon* ball = sPuppetBalls[i];

        if ((ball != NULL) && (ball->actor.update == NULL)) {
            ball = sPuppetBalls[i] = NULL;
        }
        if (i >= pose->ballCount) {
            if (ball != NULL) {
                Actor_Kill(&ball->actor);
                sPuppetBalls[i] = NULL;
            }
            continue;
        }
        GdfNetBall* b = &pose->balls[i];
        if (ball == NULL) {
            ball = (BossGanon*)Actor_Spawn(&play->actorCtx, play, ACTOR_BOSS_GANON, b->pos.x, b->pos.y, b->pos.z,
                                           b->rot.x, b->rot.y, b->rot.z, GDF_PARAMS_PUPPET_BALL);
            if (ball == NULL) {
                continue;
            }
            sPuppetBalls[i] = ball;
            for (j = 0; j < 15; j++) {
                ball->unk_2EC[j] = b->pos;
                ball->unk_3C4[j].x = BINANG_TO_RAD(b->rot.x);
                ball->unk_3C4[j].y = BINANG_TO_RAD(b->rot.y);
            }
        }
        ball->actor.draw = (b->kind != 1) ? BossGanon_LightBall_Draw : func_808E324C;
        if (b->kind == 2) {
            BossGanon_FireballFlames(play, &b->pos, ball->unk_1A2);
        }
        ball->actor.world.pos = b->pos;
        ball->actor.world.rot = ball->actor.shape.rot = b->rot;
        Actor_SetScale(&ball->actor, b->scale);
        ball->actor.floorHeight = b->floorY;
        ball->fwork[GDF_FWORK_0] = b->fwork0;
        ball->fwork[GDF_FWORK_1] = b->fwork1;
        ball->unk_1A2++;
        if ((b->kind != 1) && (b->scale < 6.5f)) {
            // In flight (not bursting): it flickers, spins and sheds sparkles here as it does in his game, where
            // those are made each frame rather than sent
            static Color_RGBA8 sSparklePrim = { 255, 255, 200, 255 };
            static Color_RGBA8 sSparkleEnv = { 255, 255, 0, 0 };
            Vec3f vel = { 0.0f, 0.0f, 0.0f };
            Vec3f accel = { 0.0f, 0.2f, 0.0f };
            Vec3f p;

            Actor_SetScale(&ball->actor, ((ball->unk_1A2 % 2) != 0) ? 6.0f : 5.25f);
            ball->actor.shape.rot.z += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;
            for (j = 0; j < 2; j++) {
                p.x = b->pos.x + Rand_CenteredFloat(30.0f);
                p.y = b->pos.y + Rand_CenteredFloat(30.0f);
                p.z = b->pos.z + Rand_CenteredFloat(30.0f);
                EffectSsKiraKira_SpawnDispersed(play, &p, &vel, &accel, &sSparklePrim, &sSparkleEnv,
                                                (s16)(Rand_ZeroFloat(500.0f) + 1500.0f), 15);
            }
        }
        // Big Magic's trail
        if (++ball->unk_1A6 >= 15) {
            ball->unk_1A6 = 0;
        }
        ball->unk_2EC[ball->unk_1A6] = b->pos;
        ball->unk_3C4[ball->unk_1A6].x = BINANG_TO_RAD(b->rot.x);
        ball->unk_3C4[ball->unk_1A6].y = BINANG_TO_RAD(b->rot.y);
        // The volley: this Link's game decides whether he sent it back or was hit
        if (GdfInvasion_IsHero() && !GdfInvasion_LinkAtPeace(0)) {
            if (b->kind != 1) {
                BossGanon_PuppetBall_Volley(ball, play);
            } else {
                BossGanon_PuppetBall_BigMagic(ball, play);
            }
        }
    }
}

void BossGanon_PuppetBall_Init(BossGanon* this, PlayState* play) {
    Actor* thisx = &this->actor;

    thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    thisx->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    thisx->room = -1;
    thisx->update = BossGanon_PuppetBall_Update;
    thisx->draw = NULL; // set by the puppet
    this->fwork[GDF_FWORK_1] = 255.0f;
    Collider_InitCylinder(play, &this->collider);
}

// ---- Invasion mode: the Links' hits on him, and his casting ----
s32 BossGanon_Player_IsBlockingNet(void) {
    return (sPlayerGanondorf != NULL) && (sPlayerGanondorf->actor.update != NULL) &&
           (sPlayerGanondorf->actionFunc == BossGanon_Player_Block);
}

// A Link's hit landed: he flashes and grunts (his Resolve is kept by GanondorfInvasion.cpp)
void BossGanon_Player_NetHurt(s32 heavy) {
    BossGanon* this = sPlayerGanondorf;

    if ((this == NULL) || (this->actor.update == NULL)) {
        return;
    }
    Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, heavy ? 16 : 8);
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(heavy ? NA_SE_EN_GANON_DAMAGE2 : NA_SE_EN_GANON_DAMAGE1));
}

// Casting a Dark Art: his laugh (when he's free to)
void BossGanon_Player_CastPose(void) {
    BossGanon* this = sPlayerGanondorf;

    if ((this == NULL) || (this->actor.update == NULL) || (gPlayState == NULL)) {
        return;
    }
    if ((this->actionFunc == BossGanon_Player_Float) || (this->actionFunc == BossGanon_Player_SwordIdle)) {
        BossGanon_Player_SetupLaugh(this, gPlayState);
    } else {
        Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
    }
}

// Phantom Step: he appears where Link (his own) was just put
void BossGanon_Player_NetSnap(void) {
    if ((sPlayerGanondorf != NULL) && (sPlayerGanondorf->actor.update != NULL) && (gPlayState != NULL)) {
        BossGanon_Player_SnapToLink(sPlayerGanondorf, gPlayState);
    }
}

// Pacified: his voice cracks and he goes up in dark smoke
void BossGanon_Player_NetPacified(void) {
    BossGanon* this = sPlayerGanondorf;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos;
    s32 i;

    if ((this == NULL) || (this->actor.update == NULL) || (gPlayState == NULL)) {
        return;
    }
    Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_DEAD));
    // He kneels (BossGanon_Player_Update starts it), then vanishes in smoke (BossGanon_Player_Pacified)
    (void)zero;
    (void)pos;
    (void)i;
}

// Invasion mode, pacified: down on one knee as when he's beaten, then gone in a burst of smoke until he's back
static void BossGanon_Player_SetupPacified(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
    BossGanon_Player_CancelMagic(this);
    this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfDefeatedStartAnim);
    Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfDefeatedStartAnim, -5.0f);
    this->actionFunc = BossGanon_Player_Pacified;
    this->unk_1C2 = 0;
    this->actor.velocity.y = 0.0f;
}

static void BossGanon_Player_Pacified(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos;
    s32 i;

    SkelAnime_Update(&this->skelAnime);
    // Down to the ground under him
    Math_ApproachF(&this->actor.world.pos.y, player->actor.world.pos.y, 0.3f, 12.0f);
    if ((this->unk_1C2 == 0) && Animation_OnFrame(&this->skelAnime, this->fwork[GDF_FWORK_1])) {
        BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
        Animation_MorphToLoop(&this->skelAnime, &gGanondorfDefeatedLoopAnim, 0.0f);
        this->unk_1C2 = 1;
    }
    if ((this->unk_1C2 < 2) && GdfInvasion_IsVanished()) {
        this->unk_1C2 = 2;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_LAUGH);
        for (i = 0; i < 16; i++) {
            pos = this->actor.world.pos;
            pos.x += Rand_CenteredFloat(100.0f);
            pos.y += Rand_ZeroFloat(150.0f);
            pos.z += Rand_CenteredFloat(100.0f);
            EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 180, 0, 60, 0, 90, 255, 30, 0, 60, 1, 9, false);
        }
    }
    if (!GdfInvasion_IsPacified()) {
        BossGanon_Player_SetupFloat(this, play);
    }
}

void BossGanon_Puppet_Init(BossGanon* this, PlayState* play) {
    Actor* thisx = &this->actor;
    s16 i;

    for (i = 0; i < ARRAY_COUNT(sPlayerEffects); i++) {
        sPlayerEffects[i].type = GDF_EFF_NONE;
    }
    sCurEffects = sPlayerEffects;
    sGanondorf = this;

    Actor_ProcessInitChain(thisx, sInitChain);
    // Not an enemy, not something to lock on to, and always updated and drawn wherever he is in the scene
    thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    thisx->flags |= ACTOR_FLAG_UPDATE_DURING_OCARINA | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                    ACTOR_FLAG_DRAW_CULLING_DISABLED;
    thisx->update = BossGanon_Puppet_Update;
    ActorShape_Init(&thisx->shape, 0, NULL, 0);
    Actor_SetScale(thisx, 0.01f);
    SkelAnime_InitFlex(play, &this->skelAnime, &gGanondorfSkel, NULL, NULL, NULL, 0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sPlayerPoundCylinderInit);
    thisx->room = -1;

    sCape = (EnGanonMant*)Actor_SpawnAsChild(&play->actorCtx, thisx, play, ACTOR_EN_GANON_MANT, 0.0f, 0.0f, 0.0f, 0, 0,
                                             0, PLAYER_GANONDORF_CAPE_PARAMS);
    if (sCape == NULL) {
        Actor_Kill(thisx);
        return;
    }
    sCape->actor.room = -1;
    sPlayerCapeDraw = sCape->actor.draw;
    Actor_ChangeCategory(play, &play->actorCtx, &sCape->actor, ACTORCAT_MISC);
    Actor_ChangeCategory(play, &play->actorCtx, thisx, ACTORCAT_MISC);

    this->playerFloorY = thisx->world.pos.y;
    Collider_InitQuad(play, &this->playerSwordQuad);
    Collider_SetQuad(play, &this->playerSwordQuad, thisx, &sPlayerSwordQuadInit);
    this->playerSwordBlure = TOTAL_EFFECT_COUNT;
    Effect_Add(play, &this->playerSwordBlure, EFFECT_BLURE2, 0, 0, &sPlayerSwordBlureInit);
    this->playerOrganSink = GDF_ORGAN_SINK_DEPTH;
    this->triforceType = GDF_TRIFORCE_DORF;
    this->playerHidden = true; // until his first pose arrives
    this->actionFunc = BossGanon_Player_Float; // never run, but the draw looks at it
}

void BossGanon_Puppet_Update_Impl(Actor* thisx, PlayState* play) {
    BossGanon* this = (BossGanon*)thisx;
    GdfNetPose pose;
    s32 n;

    if (!GdfInvasion_GetPuppetPose(thisx, &pose) || !pose.valid) {
        this->playerHidden = true;
        if (sCape != NULL) {
            sCape->actor.draw = NULL;
        }
        return;
    }
    thisx->prevPos = thisx->world.pos;
    thisx->world.pos = pose.pos;
    thisx->shape.rot.y = thisx->world.rot.y = pose.rotY;
    thisx->focus.pos = pose.pos;
    thisx->focus.pos.y += 120.0f;
    this->playerFloorY = pose.floorY;
    this->playerHidden = pose.hidden;
    // A Link can Z-target him: the one drawn (his hits still land on Ganondorf's hitbox, the player dummy)
    if (GdfInvasion_IsHero() && !pose.hidden) {
        thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
        thisx->flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
        thisx->targetMode = 5;
    } else {
        thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    }
    sPlayerSwordState = pose.swordState;
    n = MIN(this->skelAnime.limbCount, GDF_NET_JOINTS);
    memcpy(this->skelAnime.jointTable, pose.joints, n * sizeof(Vec3s));
    // Used for the cape's sway
    thisx->velocity.x = thisx->world.pos.x - thisx->prevPos.x;
    thisx->velocity.z = thisx->world.pos.z - thisx->prevPos.z;
    // His cape moves as his does (its physics run here, with his settings), and his glows, aura and organ show
    this->unk_1A2++;
    this->unk_1A4++;
    if (pose.fxCount >= GDF_NET_FX) {
        BossGanon_Puppet_UnpackFx(this, sCape, pose.fx);
        // His organ's music (Ganon's Tower theme)
        if ((pose.fx[71] != 0.0f) && !this->playerOrganMusic) {
            BossGanon_Player_StartOrganMusic(this);
        } else if ((pose.fx[71] == 0.0f) && this->playerOrganMusic) {
            BossGanon_Player_StopOrganMusic(this, play);
        }
    } else if (sCape != NULL) {
        sCape->gravity = -3.0f;
        sCape->minY = pose.floorY + 2.0f;
    }
    if (sCape != NULL) {
        sCape->actor.draw = this->playerHidden ? NULL : sPlayerCapeDraw;
    }
    BossGanon_Puppet_UpdateBalls(this, play, &pose);
}

BOSS_GANON_CONTEXT_ENTRY(BossGanon_Init)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Draw)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_LightBall_Update)
BOSS_GANON_CONTEXT_ENTRY(func_808E1EB4)
BOSS_GANON_CONTEXT_ENTRY(func_808E2544)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Player_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_PlayerLightBall_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_PlayerBigMagic_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Puppet_Update)

#include "overlays/ovl_Boss_Ganon/ovl_Boss_Ganon.h"

void BossGanon_Reset(void) {
    sGdfWanted = false; // SoH: the manhunt ends with a reset
    sGdfPlayerHeadValid = false;
    sGdfRoaming = false;
    sGdfTridentState = GDF_TRIDENT_IN_HAND;
    sGdfLastScene = -1;
    sSeed1 = 0;
    sSeed2 = 0;
    sSeed3 = 0;
    sGanondorf = NULL;
    sZelda = NULL;
    sCape = NULL;
    memset(sEffects, 0, sizeof(sEffects));
    memset(sPlayerEffects, 0, sizeof(sPlayerEffects));
    sCurEffects = sEffects;
    sVanillaGanondorf = NULL;
    sVanillaCape = NULL;
    sPlayerGanondorf = NULL;
    sPlayerCape = NULL;
}
