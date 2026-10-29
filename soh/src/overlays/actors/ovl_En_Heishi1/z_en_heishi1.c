/*
 * File: z_en_heishi1.c
 * Overlay: ovl_En_Heishi1
 * Description: Courtyard Guards
 */

#include "z_en_heishi1.h"
#include "objects/object_sd/object_sd.h"
#include "vt.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/savestate_serialize.h"
#include "overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h" // SoH: guards hunting the player Ganondorf

// SoH: "Play as Ganondorf", the guards fear him (GanondorfGuards.cpp)
s32 GanondorfGuards_Fear(void);
void GanondorfGuards_Cower(PlayState* play, Actor* guard);
void GanondorfGuards_Tremble(PlayState* play, Actor* guard);

// SoH: frozen in fear of him, facing him and trembling
static void EnHeishi1_Cower(EnHeishi1* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    GanondorfGuards_Tremble(play, &this->actor);
}

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void EnHeishi1_Init(Actor* thisx, PlayState* play);
void EnHeishi1_Destroy(Actor* thisx, PlayState* play);
void EnHeishi1_Update(Actor* thisx, PlayState* play);
void EnHeishi1_Draw(Actor* thisx, PlayState* play);
void EnHeishi1_Reset(void);

void EnHeishi1_SetupWait(EnHeishi1* this, PlayState* play);
void EnHeishi1_SetupWalk(EnHeishi1* this, PlayState* play);
void EnHeishi1_SetupMoveToLink(EnHeishi1* this, PlayState* play);
void EnHeishi1_SetupTurnTowardLink(EnHeishi1* this, PlayState* play);
void EnHeishi1_SetupKick(EnHeishi1* this, PlayState* play);
void EnHeishi1_SetupWaitNight(EnHeishi1* this, PlayState* play);

void EnHeishi1_Wait(EnHeishi1* this, PlayState* play);
void EnHeishi1_Walk(EnHeishi1* this, PlayState* play);
void EnHeishi1_MoveToLink(EnHeishi1* this, PlayState* play);
void EnHeishi1_TurnTowardLink(EnHeishi1* this, PlayState* play);
void EnHeishi1_Kick(EnHeishi1* this, PlayState* play);
void EnHeishi1_WaitNight(EnHeishi1* this, PlayState* play);

static s32 sPlayerIsCaught = false;

#define EN_HEISHI1_SHIP_SAVESTATE_FIELDS(F) F(sPlayerIsCaught)

SHIP_SAVESTATE_DEFINE(EnHeishi1, EN_HEISHI1_SHIP_SAVESTATE_FIELDS)

const ActorInit En_Heishi1_InitVars = {
    ACTOR_EN_HEISHI1,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_SD,
    sizeof(EnHeishi1),
    (ActorFunc)EnHeishi1_Init,
    (ActorFunc)EnHeishi1_Destroy,
    (ActorFunc)EnHeishi1_Update,
    (ActorFunc)EnHeishi1_Draw,
    (ActorResetFunc)EnHeishi1_Reset,
};

static f32 sAnimParamsInit[][8] = {
    { 1.0f, -10.0f, 3.0f, 0.5f, 1000.0f, 200.0f, 0.3f, 1000.0f },
    { 3.0f, -3.0f, 6.0f, 0.8f, 2000.0f, 400.0f, 0.5f, 2000.0f },
    { 1.0f, -10.0f, 3.0f, 0.5f, 1000.0f, 200.0f, 0.3f, 1000.0f },
    { 3.0f, -3.0f, 6.0f, 0.8f, 2000.0f, 400.0f, 0.5f, 2000.0f },
};

static s16 sBaseHeadTimers[] = { 20, 10, 20, 10, 13, 0 };

static Vec3f sRupeePositions[] = {
    { 0.0f, 0.0f, 90.0f },  { -55.0f, 0.0f, 90.0f }, { -55.0f, 0.0f, 30.0f }, { -55.0f, 0.0f, -30.0f },
    { 0.0f, 0.0f, -30.0f }, { 55.0f, 0.0f, -30.0f }, { 55.0f, 0.0f, 30.0f },  { 55.0f, 0.0f, 90.0f },
};

static s32 sCamDataIdxs[] = {
    7, 7, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 4, 4, 5, 6,
};

static s16 sWaypoints[] = { 0, 4, 1, 5, 2, 6, 3, 7 };

void EnHeishi1_Reset(void) {
    sPlayerIsCaught = false;
}

// ------------------------------------------------------------------------------------------------
// SoH: "Play as Ganondorf". After a murder in the market, guards hunt him: they run at him and stab with their
// spears (1 heart a stab). Spawned and replaced by z_boss_ganon.c (BossGanon_Player_UpdateManhunt).
// ------------------------------------------------------------------------------------------------
static void EnHeishi1_HunterChase(EnHeishi1* this, PlayState* play);
static void EnHeishi1_HunterStab(EnHeishi1* this, PlayState* play);

static ColliderCylinderInit sHunterBodyInit = {
    { COLTYPE_HIT0, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 18, 70, 0, { 0, 0, 0 } },
};

// A spear stab: one heart
static ColliderCylinderInit sHunterSpearInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x10 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE,
      OCELEM_NONE },
    { 22, 60, 10, { 0, 0, 0 } },
};

#define HUNTER_RUN_SPEED 5.5f
#define HUNTER_STAB_RANGE 75.0f
#define HUNTER_STAB_HIT_START 6
#define HUNTER_STAB_HIT_END 11

static void EnHeishi1_SetupHunterChase(EnHeishi1* this, PlayState* play) {
    Animation_Change(&this->skelAnime, &gEnHeishiWalkAnim, 2.2f, 0.0f, Animation_GetLastFrame(&gEnHeishiWalkAnim),
                     ANIMMODE_LOOP, -4.0f);
    this->hunterStabbing = false;
    this->actionFunc = EnHeishi1_HunterChase;
}

static void EnHeishi1_HunterChase(EnHeishi1* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x1200, 0x100);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Math_ApproachF(&this->actor.speedXZ, HUNTER_RUN_SPEED, 1.0f, 0.6f);

    if (Animation_OnFrame(&this->skelAnime, 1.0f) || Animation_OnFrame(&this->skelAnime, 9.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_KNIGHT_WALK);
    }

    if ((this->hunterTimer == 0) && (this->actor.xzDistToPlayer < HUNTER_STAB_RANGE) &&
        (fabsf(this->actor.yDistToPlayer) < 60.0f)) {
        Animation_Change(&this->skelAnime, &gEnHeishiSlamSpearAnim, 1.6f, 0.0f,
                         Animation_GetLastFrame(&gEnHeishiSlamSpearAnim), ANIMMODE_ONCE, -3.0f);
        this->actor.speedXZ = 0.0f;
        this->hunterStabbing = true;
        this->actionFunc = EnHeishi1_HunterStab;
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING);
    }
}

static void EnHeishi1_HunterStab(EnHeishi1* this, PlayState* play) {
    s32 frame;

    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x600, 0x100);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Math_ApproachZeroF(&this->actor.speedXZ, 1.0f, 1.0f);

    frame = (s32)this->skelAnime.curFrame;
    if (this->hunterStabbing && (frame >= HUNTER_STAB_HIT_START) && (frame <= HUNTER_STAB_HIT_END)) {
        // The spear point, in front of him
        this->hunterSpear.dim.pos.x = (s16)(this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * 45.0f);
        this->hunterSpear.dim.pos.y = (s16)this->actor.world.pos.y;
        this->hunterSpear.dim.pos.z = (s16)(this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * 45.0f);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->hunterSpear.base);
    }
    if (this->hunterSpear.base.atFlags & AT_HIT) {
        this->hunterSpear.base.atFlags &= ~AT_HIT;
        this->hunterStabbing = false; // one hit a stab
    }

    if (Animation_OnFrame(&this->skelAnime, Animation_GetLastFrame(&gEnHeishiSlamSpearAnim))) {
        this->hunterTimer = 12 + (s16)Rand_ZeroFloat(10.0f);
        EnHeishi1_SetupHunterChase(this, play);
    }
}

static void EnHeishi1_HunterUpdate(EnHeishi1* this, PlayState* play) {
    if (this->hunterTimer != 0) {
        this->hunterTimer--;
    }
    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 20.0f, 60.0f, 0x1D); // walls, floor, water
    Actor_SetFocus(&this->actor, 60.0f);

    Collider_UpdateCylinder(&this->actor, &this->hunterBody);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->hunterBody.base);
}

static void EnHeishi1_HunterInit(EnHeishi1* this, PlayState* play) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sAnimParamsInit[0]); i++) {
        this->animParams[i] = sAnimParamsInit[1][i];
    }
    this->actor.gravity = -1.0f;
    this->actor.flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
    Collider_InitCylinder(play, &this->hunterBody);
    Collider_SetCylinder(play, &this->hunterBody, &this->actor, &sHunterBodyInit);
    Collider_InitCylinder(play, &this->hunterSpear);
    Collider_SetCylinder(play, &this->hunterSpear, &this->actor, &sHunterSpearInit);
    this->hunterTimer = 10;
    EnHeishi1_SetupHunterChase(this, play);
}

void EnHeishi1_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    EnHeishi1* this = (EnHeishi1*)thisx;
    Vec3f rupeePos;
    s32 i;

    Actor_SetScale(&this->actor, 0.01f);
    SkelAnime_Init(play, &this->skelAnime, &gEnHeishiSkel, &gEnHeishiIdleAnim, this->jointTable, this->morphTable, 17);

    this->type = (this->actor.params >> 8) & 0xFF;
    this->path = this->actor.params & 0xFF;

    if (this->type == EN_HEISHI1_TYPE_HUNTER) {
        EnHeishi1_HunterInit(this, play);
        return;
    }

    for (i = 0; i < ARRAY_COUNT(sAnimParamsInit[0]); i++) {
        this->animParams[i] = sAnimParamsInit[this->type][i];
    }

    // "type"
    osSyncPrintf(VT_FGCOL(GREEN) " 種類☆☆☆☆☆☆☆☆☆☆☆☆☆ %d\n" VT_RST, this->type);
    // "path data"
    osSyncPrintf(VT_FGCOL(YELLOW) " れえるでぇたぁ☆☆☆☆☆☆☆☆ %d\n" VT_RST, this->path);
    osSyncPrintf(VT_FGCOL(PURPLE) " anime_frame_speed ☆☆☆☆☆☆ %f\n" VT_RST, this->animSpeed);
    // "interpolation frame"
    osSyncPrintf(VT_FGCOL(PURPLE) " 補間フレーム☆☆☆☆☆☆☆☆☆ %f\n" VT_RST, this->transitionRate);
    // "targeted movement speed value between points"
    osSyncPrintf(VT_FGCOL(PURPLE) " point間の移動スピード目標値 ☆ %f\n" VT_RST, this->moveSpeedTarget);
    // "maximum movement speed value between points"
    osSyncPrintf(VT_FGCOL(PURPLE) " point間の移動スピード最大 ☆☆ %f\n" VT_RST, this->moveSpeedMax);
    // "(body) targeted turning angle speed value"
    osSyncPrintf(VT_FGCOL(PURPLE) " (体)反転アングルスピード目標値 %f\n" VT_RST, this->bodyTurnSpeedTarget);
    // "(body) maximum turning angle speed"
    osSyncPrintf(VT_FGCOL(PURPLE) " (体)反転アングルスピード最大☆ %f\n" VT_RST, this->bodyTurnSpeedMax);
    // "(head) targeted turning angle speed value"
    osSyncPrintf(VT_FGCOL(PURPLE) " (頭)反転アングルスピード加算値 %f\n" VT_RST, this->headTurnSpeedScale);
    // "(head) maximum turning angle speed"
    osSyncPrintf(VT_FGCOL(PURPLE) " (頭)反転アングルスピード最大☆ %f\n" VT_RST, this->headTurnSpeedMax);
    osSyncPrintf(VT_FGCOL(GREEN) " 今時間 %d\n" VT_RST, ((void)0, gSaveContext.dayTime)); // "current time"
    osSyncPrintf(VT_FGCOL(YELLOW) " チェック時間 %d\n" VT_RST, 0xBAAA);                   // "check time"
    osSyncPrintf("\n\n");

    if (this->path == 3) {
        for (i = 0; i < ARRAY_COUNT(sRupeePositions); i++) {
            rupeePos = sRupeePositions[i];
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_EX_RUPPY, rupeePos.x, rupeePos.y,
                               rupeePos.z, 0, 0, 0, 3);
        }
    }

    // eventChkInf[4] & 1 = Got Zelda's Letter
    // eventChkInf[5] & 0x200 = Got item from impa
    // eventChkInf[8] & 1 = Ocarina thrown in moat
    bool metZelda = (Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_ZELDAS_LETTER)) &&
                    (Flags_GetEventChkInf(EVENTCHKINF_LEARNED_ZELDAS_LULLABY));

    if (this->type != 5) {
        if (GameInteractor_Should(VB_WONDER_HEISHI_PATROLLING,
                                  (gSaveContext.dayTime < 0xB888 || IS_DAY) &&
                                      ((!IS_RANDO && !Flags_GetEventChkInf(EVENTCHKINF_ZELDA_FLED_HYRULE_CASTLE)) ||
                                       (IS_RANDO && !metZelda)),
                                  this)) {
            this->actionFunc = EnHeishi1_SetupWalk;
        } else {
            Actor_Kill(&this->actor);
        }
    } else {
        if (GameInteractor_Should(VB_WONDER_HEISHI_PATROLLING,
                                  (gSaveContext.dayTime >= 0xB889) || !IS_DAY ||
                                      (!IS_RANDO && Flags_GetEventChkInf(EVENTCHKINF_ZELDA_FLED_HYRULE_CASTLE)) ||
                                      (IS_RANDO && metZelda),
                                  this)) {
            this->actionFunc = EnHeishi1_SetupWaitNight;
        } else {
            Actor_Kill(&this->actor);
        }
    }
}

void EnHeishi1_Destroy(Actor* thisx, PlayState* play) {
    EnHeishi1* this = (EnHeishi1*)thisx;

    if (this->type == EN_HEISHI1_TYPE_HUNTER) {
        Collider_DestroyCylinder(play, &this->hunterBody);
        Collider_DestroyCylinder(play, &this->hunterSpear);
    }
}

void EnHeishi1_SetupWalk(EnHeishi1* this, PlayState* play) {
    f32 frameCount = Animation_GetLastFrame(&gEnHeishiWalkAnim);

    Animation_Change(&this->skelAnime, &gEnHeishiWalkAnim, this->animSpeed, 0.0f, (s16)frameCount, ANIMMODE_LOOP,
                     this->transitionRate);
    this->bodyTurnSpeed = 0.0f;
    this->moveSpeed = 0.0f;
    this->headDirection = Rand_ZeroFloat(1.99f);
    this->actionFunc = EnHeishi1_Walk;
}

void EnHeishi1_Walk(EnHeishi1* this, PlayState* play) {
    Path* path;
    Vec3s* pointPos;
    f32 pathDiffX;
    f32 pathDiffZ;
    s16 randOffset;

    SkelAnime_Update(&this->skelAnime);

    if (Animation_OnFrame(&this->skelAnime, 1.0f) || Animation_OnFrame(&this->skelAnime, 17.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_KNIGHT_WALK);
    }

    if (!sPlayerIsCaught) {
        path = &play->setupPathList[this->path];
        pointPos = SEGMENTED_TO_VIRTUAL(path->points);
        pointPos += this->waypoint;

        Math_ApproachF(&this->actor.world.pos.x, pointPos->x, 1.0f, this->moveSpeed);
        Math_ApproachF(&this->actor.world.pos.z, pointPos->z, 1.0f, this->moveSpeed);

        Math_ApproachF(&this->moveSpeed, this->moveSpeedTarget, 1.0f, this->moveSpeedMax);

        pathDiffX = pointPos->x - this->actor.world.pos.x;
        pathDiffZ = pointPos->z - this->actor.world.pos.z;
        Math_SmoothStepToS(&this->actor.shape.rot.y, (Math_FAtan2F(pathDiffX, pathDiffZ) * (0x8000 / M_PI)), 3,
                           this->bodyTurnSpeed, 0);

        Math_ApproachF(&this->bodyTurnSpeed, this->bodyTurnSpeedTarget, 1.0f, this->bodyTurnSpeedMax);

        if (this->headTimer == 0) {
            this->headDirection++;
            this->headAngleTarget = 0x2000;
            // if headDirection is odd, face 45 degrees left
            if ((this->headDirection & 1) != 0) {
                this->headAngleTarget *= -1;
            }
            randOffset = Rand_ZeroFloat(30.0f);
            this->headTimer = sBaseHeadTimers[this->type] + randOffset;
        }

        Math_ApproachF(&this->headAngle, this->headAngleTarget, this->headTurnSpeedScale, this->headTurnSpeedMax);

        if ((this->path == BREG(1)) && (BREG(0) != 0)) {
            osSyncPrintf(VT_FGCOL(RED) " 種類  %d\n" VT_RST, this->path);
            osSyncPrintf(VT_FGCOL(RED) " ぱす  %d\n" VT_RST, this->waypoint);
            osSyncPrintf(VT_FGCOL(RED) " 反転  %d\n" VT_RST, this->bodyTurnSpeed);
            osSyncPrintf(VT_FGCOL(RED) " 時間  %d\n" VT_RST, this->waypointTimer);
            osSyncPrintf(VT_FGCOL(RED) " 点座  %d\n" VT_RST, path->count);
            osSyncPrintf("\n\n");
        }

        // when 20 units away from a middle waypoint, decide whether or not to skip it
        if ((fabsf(pathDiffX) < 20.0f) && (fabsf(pathDiffZ) < 20.0f)) {
            if (this->waypointTimer == 0) {
                if (this->type >= 2) {
                    if ((this->waypoint >= 4) && (Rand_ZeroFloat(1.99f) > 1.0f)) {
                        if (this->waypoint == 7) {
                            this->waypoint = 0;
                        }
                        if (this->waypoint >= 4) {
                            this->waypoint -= 3;
                        }
                        this->waypointTimer = 5;
                        return;
                    }
                }
                this->actionFunc = EnHeishi1_SetupWait;
            }
        }
    }
}

void EnHeishi1_SetupMoveToLink(EnHeishi1* this, PlayState* play) {
    f32 frameCount = Animation_GetLastFrame(&gEnHeishiWalkAnim);

    Animation_Change(&this->skelAnime, &gEnHeishiWalkAnim, 3.0f, 0.0f, (s16)frameCount, ANIMMODE_LOOP, -3.0f);
    this->bodyTurnSpeed = 0.0f;
    this->moveSpeed = 0.0f;
    Message_StartTextbox(play, 0x702D, &this->actor);
    Interface_SetDoAction(play, DO_ACTION_STOP);
    this->actionFunc = EnHeishi1_MoveToLink;
}

void EnHeishi1_MoveToLink(EnHeishi1* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachF(&this->actor.world.pos.x, player->actor.world.pos.x, 1.0f, this->moveSpeed);
    Math_ApproachF(&this->actor.world.pos.z, player->actor.world.pos.z, 1.0f, this->moveSpeed);
    Math_ApproachF(&this->moveSpeed, 6.0f, 1.0f, 0.4f);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, this->bodyTurnSpeed, 0);
    Math_ApproachF(&this->bodyTurnSpeed, 3000.0f, 1.0f, 300.0f);
    Math_ApproachZeroF(&this->headAngle, 0.5f, 2000.0f);

    if (this->actor.xzDistToPlayer < 70.0f) {
        this->actionFunc = EnHeishi1_SetupTurnTowardLink;
    }
}

void EnHeishi1_SetupWait(EnHeishi1* this, PlayState* play) {
    s16 rand;
    f32 frameCount = Animation_GetLastFrame(&gEnHeishiIdleAnim);

    Animation_Change(&this->skelAnime, &gEnHeishiIdleAnim, this->animSpeed, 0.0f, (s16)frameCount, ANIMMODE_LOOP,
                     this->transitionRate);
    this->headBehaviorDecided = false;
    this->headDirection = Rand_ZeroFloat(1.99f);
    rand = Rand_ZeroFloat(50.0f);
    this->waitTimer = rand + 50;
    this->actionFunc = EnHeishi1_Wait;
}

void EnHeishi1_Wait(EnHeishi1* this, PlayState* play) {
    s16 randOffset;
    s32 i;

    SkelAnime_Update(&this->skelAnime);
    if (!sPlayerIsCaught) {
        switch (this->headBehaviorDecided) {
            case false:
                this->headDirection++;
                // if headDirection is odd, face 52 degrees left
                this->headAngleTarget = (this->headDirection & 1) ? 0x2500 : -0x2500;
                randOffset = Rand_ZeroFloat(30.0f);
                this->headTimer = sBaseHeadTimers[this->type] + randOffset;
                this->headBehaviorDecided = true;
                break;
            case true:
                if (this->headTimer == 0) {
                    if (this->waitTimer == 0) {
                        if ((this->type == 0) || (this->type == 1)) {
                            this->waypoint++;
                            if (this->waypoint >= 4) {
                                this->waypoint = 0;
                            }
                        } else {
                            // waypoints are defined with corners as 0-3 and middle points as 4-7
                            // to choose the next waypoint, the order "04152637" is hardcoded in an array
                            for (i = 0; i < ARRAY_COUNT(sWaypoints); i++) {
                                if (this->waypoint == sWaypoints[i]) {
                                    i++;
                                    if (i >= ARRAY_COUNT(sWaypoints)) {
                                        i = 0;
                                    }
                                    this->waypoint = sWaypoints[i];
                                    break;
                                }
                            }
                            this->waypointTimer = 5;
                        }
                        this->actionFunc = EnHeishi1_SetupWalk;
                    } else {
                        this->headBehaviorDecided = false;
                    }
                }
                break;
        }
        Math_ApproachF(&this->headAngle, this->headAngleTarget, this->headTurnSpeedScale,
                       this->headTurnSpeedMax + this->headTurnSpeedMax);

        if ((this->path == BREG(1)) && (BREG(0) != 0)) {
            osSyncPrintf(VT_FGCOL(GREEN) " 種類  %d\n" VT_RST, this->path);
            osSyncPrintf(VT_FGCOL(GREEN) " ぱす  %d\n" VT_RST, this->waypoint);
            osSyncPrintf(VT_FGCOL(GREEN) " 反転  %d\n" VT_RST, this->bodyTurnSpeed);
            osSyncPrintf(VT_FGCOL(GREEN) " 時間  %d\n" VT_RST, this->waypointTimer);
            osSyncPrintf("\n\n");
        }
    }
}

void EnHeishi1_SetupTurnTowardLink(EnHeishi1* this, PlayState* play) {
    f32 frameCount = Animation_GetLastFrame(&gEnHeishiIdleAnim);

    Animation_Change(&this->skelAnime, &gEnHeishiIdleAnim, 1.0f, 0.0f, (s16)frameCount, ANIMMODE_LOOP, -10.0f);
    this->kickTimer = 30;
    this->actionFunc = EnHeishi1_TurnTowardLink;
}

void EnHeishi1_TurnTowardLink(EnHeishi1* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (this->type != 5) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, this->bodyTurnSpeed, 0);
        Math_ApproachF(&this->bodyTurnSpeed, 3000.0f, 1.0f, 300.0f);
        Math_ApproachZeroF(&this->headAngle, 0.5f, 2000.0f);
    }

    if (this->kickTimer == 0) {
        this->actionFunc = EnHeishi1_SetupKick;
    }
}

void EnHeishi1_SetupKick(EnHeishi1* this, PlayState* play) {
    f32 frameCount = Animation_GetLastFrame(&gEnHeishiIdleAnim);

    Animation_Change(&this->skelAnime, &gEnHeishiIdleAnim, 1.0f, 0.0f, (s16)frameCount, ANIMMODE_LOOP, -10.0f);
    this->actionFunc = EnHeishi1_Kick;
}

void EnHeishi1_Kick(EnHeishi1* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (!this->loadStarted) {
        // if dialog state is 5 and textbox has been advanced, kick player out
        if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
            Message_CloseTextbox(play);
            if (!this->loadStarted) {
                Flags_SetEventChkInf(EVENTCHKINF_CAUGHT_BY_CASTLE_GUARDS);
                play->nextEntranceIndex = ENTR_HYRULE_CASTLE_3;
                play->transitionTrigger = TRANS_TRIGGER_START;
                this->loadStarted = true;
                sPlayerIsCaught = false;
                play->transitionType = TRANS_TYPE_CIRCLE(TCA_STARBURST, TCC_WHITE, TCS_FAST);
                gSaveContext.nextTransitionType = TRANS_TYPE_CIRCLE(TCA_STARBURST, TCC_WHITE, TCS_FAST);
            }
        }
    }
}

void EnHeishi1_SetupWaitNight(EnHeishi1* this, PlayState* play) {
    f32 frameCount = Animation_GetLastFrame(&gEnHeishiIdleAnim);

    Animation_Change(&this->skelAnime, &gEnHeishiIdleAnim, 1.0f, 0.0f, (s16)frameCount, ANIMMODE_LOOP, -10.0f);
    this->actionFunc = EnHeishi1_WaitNight;
}

void EnHeishi1_WaitNight(EnHeishi1* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (this->actor.xzDistToPlayer < 100.0f) {
        Message_StartTextbox(play, 0x702D, &this->actor);
        Sfx_PlaySfxCentered(NA_SE_SY_FOUND);
        osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ 発見！ ☆☆☆☆☆ \n" VT_RST); // "Discovered!"
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
        this->actionFunc = EnHeishi1_SetupKick;
    }
}

void EnHeishi1_Update(Actor* thisx, PlayState* play) {
    EnHeishi1* this = (EnHeishi1*)thisx;
    s16 path;
    u8 i;
    s32 pad;
    Player* player = GET_PLAYER(play);
    s32 pad2;
    Camera* activeCam;

    if (this->type == EN_HEISHI1_TYPE_HUNTER) {
        EnHeishi1_HunterUpdate(this, play);
        return;
    }

    this->activeTimer++;

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }

    if (this->waypointTimer != 0) {
        this->waypointTimer--;
    }

    activeCam = GET_ACTIVE_CAM(play);

    if (player->actor.freezeTimer == 0) {

        this->actionFunc(this, play);

        this->actor.uncullZoneForward = 550.0f;
        this->actor.uncullZoneScale = 350.0f;
        this->actor.uncullZoneDownward = 700.0f;

        if (this->type != 5) {
            path = this->path * 2;
            if ((sCamDataIdxs[path] == activeCam->camDataIdx) || (sCamDataIdxs[path + 1] == activeCam->camDataIdx)) {
                // SoH: "Play as Ganondorf", while he's wanted the guards fight him instead of sending him out
                if (!sPlayerIsCaught && !BossGanon_Player_IsWanted()) {
                    if ((this->actionFunc == EnHeishi1_Walk) || (this->actionFunc == EnHeishi1_Wait)) {
                        Vec3f searchBallVel;
                        Vec3f searchBallAccel = { 0.0f, 0.0f, 0.0f };
                        Vec3f searchBallMult = { 0.0f, 0.0f, 20.0f };
                        Vec3f searchBallPos;

                        searchBallPos.x = this->actor.world.pos.x;
                        searchBallPos.y = this->actor.world.pos.y + 60.0f;
                        searchBallPos.z = this->actor.world.pos.z;

                        Matrix_Push();
                        Matrix_RotateY(((this->actor.shape.rot.y + this->headAngle) / 32768.0f) * M_PI, MTXMODE_NEW);
                        searchBallMult.z = 30.0f;
                        Matrix_MultVec3f(&searchBallMult, &searchBallVel);
                        Matrix_Pop();

                        EffectSsSolderSrchBall_Spawn(play, &searchBallPos, &searchBallVel, &searchBallAccel, 2,
                                                     &this->linkDetected);

                        if (this->actor.xzDistToPlayer < 60.0f) {
                            this->linkDetected = true;
                        } else if (this->actor.xzDistToPlayer < 70.0f) {
                            // this case probably exists to detect link making a jump sound
                            // from slightly further away than the previous 60 unit check
                            if (player->actor.velocity.y > -4.0f) {
                                this->linkDetected = true;
                            }
                        }

                        if (this->linkDetected) {
                            //! @bug This appears to be a check to make sure that link is standing on the ground
                            // before getting caught. However this is an issue for two reasons:
                            // 1: When doing a backflip or falling from the upper path, links y velocity will reach
                            // less than -4.0 before even touching the ground.
                            // 2: There is one frame when landing from a sidehop where you can sidehop again without
                            // letting y velocity reach -4.0 or less. This enables the player to do frame perfect
                            // sidehops onto the next screen and prevent getting caught.
                            if (!(player->actor.velocity.y > -3.9f)) {
                                this->linkDetected = false;
                                // this 60 unit height check is so the player doesnt get caught when on the upper path
                                if ((fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 60.0f) &&
                                    GanondorfGuards_Fear()) {
                                    // SoH: the King of Evil isn't caught: the guard freezes in fear
                                    this->gdfCowed = true;
                                    GanondorfGuards_Cower(play, &this->actor);
                                    this->actionFunc = EnHeishi1_Cower;
                                } else if (fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 60.0f) {
                                    Sfx_PlaySfxCentered(NA_SE_SY_FOUND);
                                    // "Discovered!"
                                    osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ 発見！ ☆☆☆☆☆ \n" VT_RST);
                                    Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
                                    sPlayerIsCaught = true;
                                    this->actionFunc = EnHeishi1_SetupMoveToLink;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

s32 EnHeishi1_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnHeishi1* this = (EnHeishi1*)thisx;

    // turn the guards head to match the direction he is looking
    if (limbIndex == 16) {
        rot->x += (s16)this->headAngle;
    }

    return false;
}

void EnHeishi1_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    EnHeishi1* this = (EnHeishi1*)thisx;
    Vec3f matrixScale = { 0.3f, 0.3f, 0.3f };

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, EnHeishi1_OverrideLimbDraw, NULL, this);
    func_80033C30(&this->actor.world.pos, &matrixScale, 0xFF, play);

    if ((this->path == BREG(1)) && (BREG(0) != 0)) {
        DebugDisplay_AddObject(this->actor.world.pos.x, this->actor.world.pos.y + 100.0f, this->actor.world.pos.z,
                               17000, this->actor.world.rot.y, this->actor.world.rot.z, 1.0f, 1.0f, 1.0f, 255, 0, 0,
                               255, 4, play->state.gfxCtx);
    }
}
