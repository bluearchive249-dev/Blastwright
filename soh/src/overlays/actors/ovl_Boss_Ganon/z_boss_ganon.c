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
#include "soh/Enhancements/ExtraModes/PlayAsGanondorf.h"
#include "assets/scenes/dungeons/ganon_boss/ganon_boss_scene.h"
#include "overlays/ovl_En_Ganon_Organ/ovl_En_Ganon_Organ.h" // SoH: his organ, for "Play as Ganondorf"
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
static void BossGanon_Init_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Update_Impl(Actor* thisx, PlayState* play);
static void BossGanon_Draw_Impl(Actor* thisx, PlayState* play);
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
        BossGanon_Player_StopOrganMusic(this, play);

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

    if (limbIndex == 2) {
        Matrix_MultVec3f(&D_808E4DB8, &this->unk_1FC);
    } else if (limbIndex == 19) {
        Matrix_MultVec3f(&D_808E4DB8, &this->actor.focus.pos);
    } else if (limbIndex == 11) {
        OPEN_DISPS(play->state.gfxCtx);

        Matrix_MultVec3f(&D_808E4DB8, &this->unk_208);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfEyesDL));

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
        BossGanon_DrawEffects(play);
        return;
    }

    if (GDF_IS_PLAYER(this)) {
        BossGanon_Player_DrawOrgan(this, play);
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

    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, BossGanon_OverrideLimbDraw, BossGanon_PostLimbDraw, &this->actor);

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
static void BossGanon_Player_DeflectMagic(BossGanon* this, PlayState* play);
static void BossGanon_Player_DrawOrgan(BossGanon* this, PlayState* play);
static s32 BossGanon_Player_CanStartAttack(BossGanon* this, PlayState* play);
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
           (this->actionFunc == BossGanon_Player_Organ);
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
    this->actor.world.pos.y += BossGanon_Player_GetHoverHeight();
    this->actor.prevPos = this->actor.world.pos;
    this->actor.shape.rot.y = this->actor.world.rot.y = player->actor.shape.rot.y;
}

static void BossGanon_Player_Follow(BossGanon* this, PlayState* play, u8 controlY) {
    Player* player = GET_PLAYER(play);
    Vec3f target;
    f32 dx;
    f32 dy;
    f32 dz;

    target.x = player->actor.world.pos.x;
    target.y = player->actor.world.pos.y + BossGanon_Player_GetHoverHeight() + (Math_SinS(this->unk_1A2 * 1500) * 3.0f);
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
        this->actor.world.pos.y += dy * 0.25f;
    }
}

static s32 BossGanon_Player_ShouldHide(BossGanon* this, PlayState* play) {
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
    if (CVarGetInteger(CVAR_PLAY_AS_GDF("ShowLinkInCutscenes"), 1) && (play->csCtx.state != CS_STATE_IDLE)) {
        return true;
    }

    return false;
}

static s32 BossGanon_Player_CanStartAttack(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->playerHidden || Player_InCsMode(play) || (play->csCtx.state != CS_STATE_IDLE)) {
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

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
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
    this->playerPrevInvincibilityTimer = player->invincibilityTimer;
    this->playerOrganSink = GDF_ORGAN_SINK_DEPTH;
    this->triforceType = GDF_TRIFORCE_DORF;
    BossGanon_Player_SnapToLink(this, play);
    BossGanon_Player_UpdateAim(this, play);
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

void BossGanon_Player_SetupFloat(BossGanon* this, PlayState* play) {
    BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME1);
    Animation_MorphToLoop(&this->skelAnime, &gGanondorfFloatAnim, -10.0f);
    this->actionFunc = BossGanon_Player_Float;
    this->unk_1C2 = 0;
    this->playerBtnHeld = 0;
}

void BossGanon_Player_Float(BossGanon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->legSwayEnabled = true;

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->handLightBallScale, 1.0f, 1.0f);

    if (player->actor.speedXZ > 1.0f) {
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
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_SPARK);
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
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_THROW);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_THROW_MASIC);
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
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_HIT_GND);
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
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_HIT_GND_IMP);
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
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
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
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_AT_RETURN); // "Hah!"
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
           (this->actionFunc == BossGanon_Player_Block) && (this->unk_1C2 == 0);
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
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_AT_RETURN); // "Hah!"
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

    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DAMAGE1);
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
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_DAMAGE2);
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
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_RESTORE);
            }
            break;

        case 4:
            sCape->minDist = 20.0f;
            Math_ApproachF(&this->actor.world.pos.y, player->actor.world.pos.y + BossGanon_Player_GetHoverHeight(),
                           0.1f, 10.0f);

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

    controlY = (this->actionFunc != BossGanon_Player_Pound) && (this->actionFunc != BossGanon_Player_Downed) &&
               (this->actionFunc != BossGanon_Player_Organ);

    // The organ sinks back into the ground once he stops playing, and the area music comes back
    if (this->actionFunc != BossGanon_Player_Organ) {
        BossGanon_Player_StopOrganMusic(this, play);
        Math_ApproachF(&this->playerOrganSink, GDF_ORGAN_SINK_DEPTH, 1.0f, 25.0f);
    }

    // Triforce of Power on his hand, from the laugh
    if (this->actionFunc != BossGanon_Player_Laugh) {
        Math_ApproachZeroF(&this->fwork[GDF_TRIFORCE_PRIM_A], 1.0f, 25.0f);
    }
    BossGanon_Player_Follow(this, play, controlY);

    if (BossGanon_Player_IsBusy(thisx)) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x1000);
    } else if (this->actionFunc != BossGanon_Player_Downed) {
        Math_ApproachS(&this->actor.shape.rot.y, player->actor.shape.rot.y, 2, 0x2000);
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;

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

    // Leg sway, from BossGanon_Update
    if (this->legSwayEnabled) {
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

    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_FIRE - SFX_FLAG);
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
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
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
                CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
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

    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_LAUGH);
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
// chamber; its triangles are left out so only the organ on its dais appears.
#define GDF_ORGAN_CARPET_TEX_HASH 0x556DB73EBD8DB734ULL

static s32 BossGanon_Player_IsTwoPartGfx(u8 opcode) {
    return (opcode == G_SETTIMG_OTR_HASH) || (opcode == G_DL_OTR_HASH) || (opcode == G_VTX_OTR_HASH) ||
           (opcode == G_BRANCH_Z_OTR) || (opcode == G_MARKER) || (opcode == G_MTX_OTR) || (opcode == G_MOVEMEM_OTR);
}

// Copy of sRoomOrganAndFloorDL without the carpet
static Gfx* BossGanon_Player_BuildOrganDL(PlayState* play) {
    Gfx* src = ResourceMgr_LoadGfxByName(sRoomOrganAndFloorDL);
    Gfx* dst;
    s32 count = 0;
    s32 i;
    u8 isCarpet = false;

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
            }
            i++;
            continue;
        }

        if (isCarpet && ((opcode == G_TRI1) || (opcode == G_TRI2) || (opcode == G_QUAD))) {
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

    Matrix_Translate(this->playerOrganPos.x, this->playerOrganPos.y - this->playerOrganSink, this->playerOrganPos.z,
                     MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->playerOrganYaw), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, organDL);

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
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_STONE_STATUE_OPEN);
}

void BossGanon_Player_Organ(BossGanon* this, PlayState* play) {
    Input* input = &play->state.input[0];

    SkelAnime_Update(&this->skelAnime);

    this->actor.yawTowardsPlayer = this->playerOrganYaw;
    Math_ApproachF(&this->actor.world.pos.y, this->playerOrganPos.y + GDF_ORGAN_SEAT_HEIGHT, 0.2f, 10.0f);

    sCape->backPush = -3.0f;
    sCape->backSwayMagnitude = 0.25f;
    sCape->sideSwayMagnitude = -3.0f;
    sCape->minDist = 20.0f;

    switch (this->unk_1C2) {
        case 0: // the organ rises
            Math_ApproachZeroF(&this->playerOrganSink, 1.0f, 25.0f);
            Sfx_PlaySfxAtPos(&this->actor.projectedPos, NA_SE_EV_ELEVATOR_MOVE3 - SFX_FLAG);

            if (this->playerOrganSink == 0.0f) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_STONE_BOUND);
                Actor_RequestQuakeAndRumble(&this->actor, play, 4, 10);
                BossGanon_SetAnimationObject(this, play, OBJECT_GANON_ANIME2);
                Animation_MorphToLoop(&this->skelAnime, &gGanondorfPlayOrganAnim, -5.0f);
                BossGanon_Player_StartOrganMusic(this);
                this->unk_1C2 = 1;
            }
            break;

        case 1: // playing, until D-pad Down, B or A
            if (CHECK_BTN_ANY(input->press.button, BTN_DDOWN | BTN_B | BTN_A) ||
                !BossGanon_Player_CanStartAttack(this, play)) {
                this->fwork[GDF_FWORK_1] = Animation_GetLastFrame(&gGanondorfStopPlayingOrganAnim);
                Animation_MorphToPlayOnce(&this->skelAnime, &gGanondorfStopPlayingOrganAnim, -5.0f);
                BossGanon_Player_StopOrganMusic(this, play);
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
