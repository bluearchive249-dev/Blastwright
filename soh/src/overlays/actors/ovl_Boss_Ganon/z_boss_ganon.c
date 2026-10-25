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
#include "soh/Enhancements/ExtraModes/PlayAsGanondorf.h"
#include "soh/Enhancements/ExtraModes/GanondorfSongs.h"
#include "soh/Enhancements/ExtraModes/GanondorfKills.h"
#include "overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "assets/scenes/dungeons/ganon_boss/ganon_boss_scene.h"
#include "overlays/ovl_En_Ganon_Organ/ovl_En_Ganon_Organ.h" // SoH: his organ, for "Play as Ganondorf"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h" // SoH: riding his horse
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
#define GDF_IS_PLAYER(dorf) ((dorf)->actor.params == GDF_PARAMS_PLAYER)
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
static u8 sGdfPgCapture = false;
static MtxF sGdfLimbMtx[25];
static void BossGanon_LightBall_Update_Impl(Actor* thisx, PlayState* play);
static void func_808E1EB4_Impl(Actor* thisx, PlayState* play);
static void func_808E2544_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Player_Update_Impl(Actor* thisx, PlayState* play);
static void BossGanon_PlayerLightBall_Update_Impl(Actor* thisx, PlayState* play);
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
    if (GDF_IS_PLAYER(this)) {
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

    GanondorfKills_Record(npc);
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

            // Not the one he's carrying (Ruto), and nothing already gone
            if ((npc->update != NULL) && (npc != player->heldActor) && (npc->init == NULL)) {
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
void BossGanon_Player_SetupLaugh(BossGanon* this, PlayState* play);
void BossGanon_Player_Laugh(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupOrgan(BossGanon* this, PlayState* play);
void BossGanon_Player_Organ(BossGanon* this, PlayState* play);
void BossGanon_Player_SetupDarkAura(BossGanon* this, PlayState* play);
void BossGanon_Player_Ride(BossGanon* this, PlayState* play);
void BossGanon_Player_Climb(BossGanon* this, PlayState* play);
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

s32 BossGanon_Player_IsBusy(Actor* thisx) {
    BossGanon* this = (BossGanon*)thisx;

    if ((thisx == NULL) || !GDF_IS_PLAYER(this)) {
        return false;
    }

    return (this->actionFunc == BossGanon_Player_ChargeLightBall) || (this->actionFunc == BossGanon_Player_Throw) ||
           (this->actionFunc == BossGanon_Player_ChargeBigMagic) || (this->actionFunc == BossGanon_Player_Pound) ||
           (this->actionFunc == BossGanon_Player_Block) || (this->actionFunc == BossGanon_Player_Laugh) ||
           (this->actionFunc == BossGanon_Player_Organ) || (this->actionFunc == BossGanon_Player_SwordAttack) ||
           (this->actionFunc == BossGanon_Player_SwordDash) || (this->actionFunc == BossGanon_Player_Land);
    // Not the dark aura: he moves freely while holding it
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

// Standing and walking, while not levitating: the walk cycle follows Link's speed
static void BossGanon_Player_UpdateWalk(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 speed = fabsf(player->linearVelocity);
    f32 rate;
    u8 moving = (speed > 0.5f) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    Math_ApproachF(&this->playerWalkWeight, moving ? 1.0f : 0.0f, 1.0f, 0.15f);
    // Sprinting (holding A): a faster cycle, longer strides and a forward lean
    Math_ApproachF(&this->playerSprintWeight, (moving && PlayAsGanondorf_IsSprinting()) ? 1.0f : 0.0f, 1.0f, 0.1f);

    rate = CLAMP(speed / GDF_WALK_NATURAL_SPEED, 0.5f, GDF_WALK_MAX_RATE + this->playerSprintWeight);
    this->playerWalkStride = speed / (GDF_WALK_NATURAL_SPEED * GDF_WALK_MAX_RATE);
    if (moving || (this->playerWalkWeight > 0.0f)) {
        this->playerWalkFrame += rate;
        while (this->playerWalkFrame >= GDF_WALK_CYCLE_FRAMES) {
            this->playerWalkFrame -= GDF_WALK_CYCLE_FRAMES;
        }
    }

    BossGanon_Player_ApplyWalk(this);
    if (this->playerSprintWeight > 0.0f) {
        BossGanon_Player_TwistLean(this, 0.0f, 0x0C00 * this->playerSprintWeight * this->playerWalkWeight);
    }
}

// Keeps his lowest ankle or knee at the floor (from the last draw), so any animation stands on the ground. Knees
// count too: kneeling (the heavy landing) puts a knee lower than either foot, and his knee pad is about as thick as
// his sole.
static void BossGanon_Player_UpdateGroundOffset(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 lowestAnkle = MIN(this->unk_2EC[GDF_LEFT_FOOT_BODYPART].y, this->unk_2EC[GDF_RIGHT_FOOT_BODYPART].y);
    f32 lowestKnee = MIN(this->unk_2EC[GDF_LEFT_KNEE_BODYPART].y, this->unk_2EC[GDF_RIGHT_KNEE_BODYPART].y);
    f32 ankleAboveRoot = MIN(lowestAnkle, lowestKnee) - this->actor.world.pos.y;
    f32 target = GDF_ANKLE_HEIGHT - ankleAboveRoot + (this->playerFloorY - player->actor.world.pos.y);

    if (this->playerHidden || (ankleAboveRoot < -300.0f) || (ankleAboveRoot > 100.0f)) {
        return; // not drawn (stale), or not a sensible pose
    }

    target = CLAMP(target, -20.0f, BossGanon_Player_GetHoverHeight() + 60.0f);
    // The landing drops him onto one knee in two frames: follow it at once, or he sinks into the floor first
    if (this->actionFunc == BossGanon_Player_Land) {
        Math_ApproachF(&this->playerGroundOffset, target, 1.0f, 40.0f);
    } else {
        Math_ApproachF(&this->playerGroundOffset, target, 0.5f, 6.0f);
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
    return !this->playerHidden && BossGanon_Player_LinkCanAttack(play);
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
    u8 useCButtons = !CVarGetInteger(CVAR_PLAY_AS_GDF("KeepItems"), 0);
    u8 bigMagicEnabled = CVarGetInteger(CVAR_PLAY_AS_GDF("BigMagic"), 1);

    if (!BossGanon_Player_CanStartAttack(this, play)) {
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
        BossGanon_Player_SetupLaugh(this, play);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN) &&
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

    // Phantom Ganon has no sheath: his left arm stays free
    Math_ApproachF(&this->playerSheathArmWeight,
                   ((sPlayerSwordState != GDF_SWORD_OFF) && !BossGanon_Player_IsPhantom()) ? 1.0f : 0.0f, 1.0f, 0.2f);
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
        BossGanon_Player_SlerpRot(&rest, &swing[i], this->playerWalkWeight, &pose[i]);
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
    static Vec3s sKneeL[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sKneeR[GDF_YOUNG_WALK_JOINTS];
    static u8 sKneesRead = false;
    static const s8 sLegLimbs[] = { 18, 19, 20, 22, 23 }; // pelvis, left thigh and shin, right thigh and shin
    Vec3s* joints = this->skelAnime.jointTable;
    f32 weight;
    s32 i;

    Math_ApproachF(&this->playerAirWeight, (this->playerGrounded && BossGanon_Player_IsJumping(play)) ? 1.0f : 0.0f,
                   1.0f, 0.2f);
    weight = this->playerAirWeight;
    if (weight <= 0.0f) {
        return;
    }

    if (!sKneesRead) {
        SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, GDF_JUMP_LEFT_KNEE_FRAME,
                               GDF_YOUNG_WALK_JOINTS, sKneeL);
        SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, GDF_JUMP_RIGHT_KNEE_FRAME,
                               GDF_YOUNG_WALK_JOINTS, sKneeR);
        sKneesRead = true;
    }

    // Both knees up: each leg from the walk frame where its knee is highest
    for (i = 0; i < ARRAY_COUNT(sLegLimbs); i++) {
        s32 limb = sLegLimbs[i];
        Vec3s* src = (limb >= 22) ? sKneeR : sKneeL;

        BossGanon_Player_SlerpRot(&joints[1 + limb], &src[1 + sGdfBossToYoungLimb[limb]], weight, &joints[1 + limb]);
    }

    // Arms
    if (sPlayerSwordState == GDF_SWORD_DRAWN) {
        BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfSwordPoses[GDF_POSE_H3_RAISE], weight);
    } else {
        BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, sGdfJumpArmPoses[0], weight);
    }
    if (sPlayerSwordState == GDF_SWORD_OFF) {
        BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, sGdfJumpArmPoses[1], weight);
    }
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
    // Insta-kill dark aura mode: D-Pad Left toggles it, then B (held) unleashes the aura
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("DPadLeftMode"), 0) == 0) {
        sPlayerDarkAuraMode = !sPlayerDarkAuraMode;
        if (sPlayerDarkAuraMode) {
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_LAUGH));
            BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
            BossGanonEff_SpawnShock(play, 700.0f, GDF_SHOCK_PLAYER_PURPLE);
        } else {
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
        }
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
    BossGanon_Player_ApplyJump(this, play);

    if (!BossGanon_Player_CanStartAttack(this, play)) {
        return;
    }

    // Phantom Ganon: B slashes (B, B, then a stab), stick + R and B a lunging stab, R + B or C-Left a ball of light.
    // No Big Magic and no sword skills. D-Pad Left still toggles the dark aura (so no boss can leave him stuck).
    if (BossGanon_Player_IsPhantom()) {
        u8 useCButtons = !CVarGetInteger(CVAR_PLAY_AS_GDF("KeepItems"), 0);

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

    // D-Pad Left: blade back into the sheath, or the sword away
    if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        if (sPlayerSwordState == GDF_SWORD_DRAWN) {
            BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_SHEATHE);
        } else {
            sPlayerSwordState = GDF_SWORD_OFF;
            Audio_PlayActorSound2(&this->actor, BossGanon_Player_Voice(NA_SE_EN_GANON_SPARK));
            BossGanon_Player_SetupFloat(this, play);
        }
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
        BossGanon_Player_SetupSwordAttack(this, play, GDF_SWORD_ATK_WAVE);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        BossGanon_Player_SetupLaugh(this, play);
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
    if (this->playerSwordAttack == GDF_SWORD_ATK_SPIN) {
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
                                          (BossGanon_Player_IsPhantom() && (atk->next == GDF_SWORD_ATK_COMBO3))
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
    // MISC also isn't affected by things that freeze or look for bosses.
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
                heightTarget = player->actor.world.pos.y + hover + 90.0f;
                this->unk_258 += (Rand_ZeroFloat(M_PI / 2) + (M_PI / 2));
                Math_ApproachF(&this->handLightBallScale, 7.0f, 0.5f, 1.0f);
            } else {
                heightTarget = player->actor.world.pos.y + hover + 40.0f;
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

            if (this->actor.world.pos.y < (floorY + 60.0f)) {
                this->actor.world.pos.y = floorY + 60.0f;
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
            Math_ApproachF(&this->actor.world.pos.y, player->actor.world.pos.y + hover, 0.1f, this->actor.velocity.y);
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

    // On his horse
    if ((player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && (this->actionFunc != BossGanon_Player_Ride) &&
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

    this->playerHidden = BossGanon_Player_ShouldHide(this, play);
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

    // React to Link getting hurt or dying
    if (BossGanon_Player_IsLinkDead(play)) {
        if (this->actionFunc != BossGanon_Player_Downed) {
            BossGanon_Player_SetupDowned(this, play);
        }
    } else if ((player->invincibilityTimer > 0) && (this->playerPrevInvincibilityTimer <= 0) &&
               (this->actionFunc != BossGanon_Player_Downed)) {
        BossGanon_Player_SetupHurt(this, play);
    }

    this->actionFunc(this, play);

    {
        // The dark aura handles his legs itself (it walks)
        u8 standLegs = (this->actionFunc != BossGanon_Player_DarkAura) && (this->actionFunc != BossGanon_Player_Climb) &&
                       this->playerGrounded && !this->playerHidden &&
                       ((this->actionFunc == BossGanon_Player_Block) ||
                        (this->actionFunc == BossGanon_Player_ChargeLightBall) ||
                        (this->actionFunc == BossGanon_Player_Throw) ||
                        (this->actionFunc == BossGanon_Player_ChargeBigMagic) ||
                        (this->actionFunc == BossGanon_Player_Laugh) || (this->actionFunc == BossGanon_Player_Hurt));

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
        static const u8 sLinkToDorfBodyPart[] = {
            1,          // waist
            12, 13, 14, // right leg
            9,  10, 11, // left leg
            2,  2,      // head, hat
            0,          // collar
            3,  4,  5,  // left arm
            6,  7,  8,  // right arm
            0,  0,      // sheath, torso
        };

        for (i = 0; (i < ARRAY_COUNT(sLinkToDorfBodyPart)) && (i < ARRAY_COUNT(player->bodyPartsPos)); i++) {
            player->bodyPartsPos[i] = this->unk_2EC[sLinkToDorfBodyPart[i]];
        }
    }

    // After the action, so the block's own intangibility doesn't count as getting hurt
    this->playerPrevInvincibilityTimer = player->invincibilityTimer;

    if (this->playerGrounded && (this->actionFunc != BossGanon_Player_Climb)) {
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
    BossGanon_Player_Follow(this, play, controlY);

    if ((this->actionFunc == BossGanon_Player_SwordAttack) && (this->playerSwordAttack == GDF_SWORD_ATK_SPIN) &&
        (this->playerSpinYaw != 0)) {
        this->actor.shape.rot.y = this->playerDashYaw + this->playerSpinYaw;
    } else if (BossGanon_Player_IsBusy(thisx)) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x1000);
    } else if (this->actionFunc != BossGanon_Player_Downed) {
        Math_ApproachS(&this->actor.shape.rot.y, player->actor.shape.rot.y, 2, 0x2000);
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->playerAimYaw = this->actor.yawTowardsPlayer;

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
    this->actor.shape.rot.z += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
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

    // Also on the frame it bursts against a wall or floor: eye switches and the like sit right on the surface,
    // and the wall check can stop the ball before its hitbox reaches them
    if (!hitSomething) {
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

            if (numEffects != 40) {
                // Still hits what it touches on the frame it bursts against a wall (eye switches)
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

void BossGanon_Player_SetupDarkAura(BossGanon* this, PlayState* play) {
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
    Actor* horse = player->rideActor;
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

// Hand over hand: each arm reaches up while the other pulls, the knee on the same side lifts with the reaching arm.
// The cycle follows how far Link actually climbs.
void BossGanon_Player_Climb(BossGanon* this, PlayState* play) {
    static Vec3s sKneeL[GDF_YOUNG_WALK_JOINTS];
    static Vec3s sKneeR[GDF_YOUNG_WALK_JOINTS];
    static u8 sKneesRead = false;
    Player* player = GET_PLAYER(play);
    Vec3s* joints = this->skelAnime.jointTable;
    Vec3s armL[3];
    Vec3s armR[3];
    f32 t;
    s32 i;

    if (!(player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) || this->playerHidden) {
        BossGanon_Player_SetupFloat(this, play);
        return;
    }
    if (!sKneesRead) {
        SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, 5, GDF_YOUNG_WALK_JOINTS, sKneeL);
        SkelAnime_GetFrameData((AnimationHeader*)gYoungGanondorfWalkAnim, 20, GDF_YOUNG_WALK_JOINTS, sKneeR);
        sKneesRead = true;
    }

    SkelAnime_Update(&this->skelAnime);
    this->playerWalkWeight = 0.0f;
    this->playerStandLegsWeight = 0.0f;
    Math_ApproachZeroF(&this->playerGroundOffset, 1.0f, 4.0f);

    sCape->backPush = -2.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -1.0f;
    sCape->minDist = 0.0f;

    this->playerClimbPhase += Math_Vec3f_DistXYZ(&player->actor.world.pos, &player->actor.prevPos) * 0.25f;
    t = 0.5f + (0.5f * Math_SinF(this->playerClimbPhase));

    for (i = 0; i < 3; i++) {
        BossGanon_Player_SlerpRot((Vec3s*)&sGdfClimbPoses[1][i], (Vec3s*)&sGdfClimbPoses[0][i], 1.0f - t, &armL[i]);
        BossGanon_Player_SlerpRot((Vec3s*)&sGdfClimbPoses[3][i], (Vec3s*)&sGdfClimbPoses[2][i], t, &armR[i]);
    }
    BossGanon_Player_BlendArm(this, GDF_LEFT_ARM_LIMB, armL, 1.0f);
    BossGanon_Player_BlendArm(this, GDF_RIGHT_ARM_LIMB, armR, 1.0f);

    // Knees: the left one up with the left arm, the right one with the right arm
    for (i = 19; i <= 20; i++) {
        BossGanon_Player_SlerpRot(&joints[1 + i], &sKneeL[1 + sGdfBossToYoungLimb[i]], (1.0f - t) * 0.8f,
                                  &joints[1 + i]);
    }
    for (i = 22; i <= 23; i++) {
        BossGanon_Player_SlerpRot(&joints[1 + i], &sKneeR[1 + sGdfBossToYoungLimb[i]], t * 0.8f, &joints[1 + i]);
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
// SoH: "Play as Ganondorf", D-pad Down: his organ rises out of the ground and he plays Ganondorf's Theme
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
    if ((!closeUp && !this->playerOrganOcarina) || this->playerHidden) {
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

        case 1: // playing, until D-pad Down, B or A (as his ocarina: until Link puts it away)
            BossGanon_Player_UpdateOrganCam(this, play);
            if (this->playerOrganOcarina ? !(GET_PLAYER(play)->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING)
                                         : (CHECK_BTN_ANY(input->press.button, BTN_DDOWN | BTN_B | BTN_A) ||
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

    if (BossGanon_IsPlayerAttack(&this->actor)) {
        return 1;
    }
    if (this->actor.params < 0x64) {
        return 0;
    }
    // Projectiles and effects get their parent right after their init
    if ((parent != NULL) && (parent->id == ACTOR_BOSS_GANON)) {
        return parent->params == GDF_PARAMS_PLAYER;
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

BOSS_GANON_CONTEXT_ENTRY(BossGanon_Init)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Draw)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_LightBall_Update)
BOSS_GANON_CONTEXT_ENTRY(func_808E1EB4)
BOSS_GANON_CONTEXT_ENTRY(func_808E2544)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_Player_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_PlayerLightBall_Update)
BOSS_GANON_CONTEXT_ENTRY(BossGanon_PlayerBigMagic_Update)

#include "overlays/ovl_Boss_Ganon/ovl_Boss_Ganon.h"

void BossGanon_Reset(void) {
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
