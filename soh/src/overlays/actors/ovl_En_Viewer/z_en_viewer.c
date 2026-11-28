/*
 * File: z_en_viewer.c
 * Overlay: ovl_En_Viewer
 * Description: Cutscene Actors
 */

#include "z_en_viewer.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "objects/object_zl4/object_zl4.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_horse_zelda/object_horse_zelda.h"
#include "objects/object_horse_ganon/object_horse_ganon.h"
#include "objects/object_im/object_im.h"
#include "objects/object_gndd/object_gndd.h"
#include "objects/object_ganon/object_ganon.h"
#include "objects/object_opening_demo1/object_opening_demo1.h"
#include "soh/frame_interpolation.h"
#include "objects/object_horse/object_horse.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "soh/Enhancements/ExtraModes/PlayAsGanondorf.h"
#include <assert.h>

// SoH: "Play as Ganondorf", the drawbridge escape (Hyrule Field, layer 5) with the roles swapped: adult Link rides out
// on Epona in Ganondorf's place, answers in his own voice, and instead of Ganondorf's dark magic fires a Light Arrow
// that drops Ganondorf (the player) to his knees. The rest of the scene (the cutscene's paths, camera and timing) is
// unchanged.
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);

// Link plays the villain in Ganondorf's cutscenes (Play as Ganondorf + the option)
s32 GdfVillain_On(PlayState* play) {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.VillainLink"), 1);
}

// The rider's cutscenes: the drawbridge escape (layer 5) and the opening nightmare (layer 4)
s32 GdfVillain_Active(PlayState* play) {
    if (!GdfVillain_On(play) || (play->sceneNum != SCENE_HYRULE_FIELD)) {
        return false;
    }
    if (gSaveContext.sceneLayer == 4) {
        return true; // (the nightmare: none of the player's own on screen)
    }
    return PlayAsGanondorf_ShouldHideLink(play, GET_PLAYER(play)) && (gSaveContext.sceneLayer == 5);
}

// Cutscenes where every line spoken is the villain's (so Link's grunts can take their place): the drawbridge escape,
// and the Temple of Time when the Master Sword is first pulled (the giant who thanks him for the way to the Sacred
// Realm, En_Viewer type 7)
s32 GdfVillain_SpeaksAlone(PlayState* play) {
    Actor* a;

    if (GdfVillain_Active(play)) {
        return true;
    }
    if (!GdfVillain_On(play) || (play->sceneNum != SCENE_TEMPLE_OF_TIME) || (play->csCtx.state == CS_STATE_IDLE)) {
        return false;
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; a != NULL; a = a->next) {
        if ((a->id == ACTOR_EN_VIEWER) && ((a->params >> 8) == ENVIEWER_TYPE_7_GANONDORF)) {
            return true;
        }
    }
    return false;
}

// Ganondorf on foot in a cutscene (the courtyard's kneeling, the laughing giant, and the rest): Link in his place
static s32 GdfVillain_Standing(PlayState* play, u8 type) {
    return GdfVillain_On(play) && ((type == ENVIEWER_TYPE_5_GANONDORF) || (type == ENVIEWER_TYPE_7_GANONDORF) ||
                                   (type == ENVIEWER_TYPE_8_GANONDORF) || (type == ENVIEWER_TYPE_9_GANONDORF));
}
void BossGanon_Player_CsStagger(s16 frames);
void GdfVillain_NoteShown(Vec3f* pos, f32 scale, s32 rider);

static SkelAnime sVlSkel;       // the riding Link
static Vec3s sVlJoints[PLAYER_LIMB_BUF_COUNT];
static Vec3s sVlMorph[PLAYER_LIMB_BUF_COUNT];
static SkelAnime sVlBowSkel;    // his arms with the bow, over the riding
static Vec3s sVlBowJoints[PLAYER_LIMB_BUF_COUNT];
static Vec3s sVlBowMorph[PLAYER_LIMB_BUF_COUNT];
static u8 sVlReady = false;
static u8 sVlBow = 0;           // 0 riding, 1 raising the bow, 2 aiming, 3 the shot
static Vec3f sVlRider;          // the saddle (from Epona's draw)
static u8 sVlRiderValid = false;
static Vec3f sVlBowHand;
static Vec3f sVlArrow;          // the Light Arrow, flying
static Vec3f sVlArrowFrom;
static s16 sVlArrowT = -1;
static s16 sVlArrowHitT = 1;    // the frame the Light Arrow reaches him (its distance over the arrow's speed)
static s16 sVlHorseYaw = 0;     // Epona's facing (from her draw)
static s16 sVlAimYaw = 0;       // his upper body turned toward Ganondorf, from Epona's facing
static SkelAnime sVsSkel;       // Link on foot, in Ganondorf's place
static Vec3s sVsJoints[PLAYER_LIMB_BUF_COUNT];
static Vec3s sVsMorph[PLAYER_LIMB_BUF_COUNT];
static u8 sVsReady = false;
static u8 sVsState = 0;
#define GDF_VL_SEAT 7.0f        // his hips this far above the saddle (as the game seats Link: saddle - 27 + his hips)
#define GDF_VL_ARROW_SPEED 150.0f // (an arrow's, see EnArrow_Shoot)

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void EnViewer_Init(Actor* thisx, PlayState* play);
void EnViewer_Destroy(Actor* thisx, PlayState* play);
void EnViewer_Update(Actor* thisx, PlayState* play);
void EnViewer_Draw(Actor* thisx, PlayState* play);

void EnViewer_UpdatePosition(EnViewer* this, PlayState* play);
void EnViewer_DrawFireEffects(EnViewer* this2, PlayState* play);
void EnViewer_UpdateGanondorfCape(PlayState* play, EnViewer* this);
void EnViewer_InitImpl(EnViewer* this, PlayState* play);
void EnViewer_UpdateImpl(EnViewer* this, PlayState* play);

static u8 sHorseSfxPlayed = false;

const ActorInit En_Viewer_InitVars = {
    ACTOR_EN_VIEWER,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnViewer),
    (ActorFunc)EnViewer_Init,
    (ActorFunc)EnViewer_Destroy,
    (ActorFunc)EnViewer_Update,
    (ActorFunc)EnViewer_Draw,
    NULL,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneScale, 300, ICHAIN_STOP),
};

static EnViewerInitData sInitData[] = {
    /* ENVIEWER_TYPE_0_HORSE_ZELDA */
    { OBJECT_HORSE_ZELDA, OBJECT_HORSE_ZELDA, 1, 0, ENVIEWER_SHADOW_HORSE, 20, ENVIEWER_DRAW_HORSE, &gHorseZeldaSkel,
      &gHorseZeldaGallopingAnim },
    /* ENVIEWER_TYPE_1_IMPA */
    { OBJECT_IM, OBJECT_OPENING_DEMO1, 1, 0, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_IMPA, &gImpaSkel,
      &object_opening_demo1_Anim_0029CC },
    /* ENVIEWER_TYPE_2_ZELDA */
    { OBJECT_ZL4, OBJECT_OPENING_DEMO1, 1, 0, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_ZELDA, &gChildZeldaSkel,
      &object_opening_demo1_Anim_000450 },
    /* ENVIEWER_TYPE_3_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfHorsebackIdleAnim },
    /* ENVIEWER_TYPE_4_HORSE_GANONDORF */
    { OBJECT_HORSE_GANON, OBJECT_HORSE_GANON, 1, 0, ENVIEWER_SHADOW_HORSE, 20, ENVIEWER_DRAW_HORSE, &gHorseGanonSkel,
      &gHorseGanonRearingAnim },
    /* ENVIEWER_TYPE_5_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfHorsebackRideAnim },
    /* ENVIEWER_TYPE_6_HORSE_GANONDORF */
    { OBJECT_HORSE_GANON, OBJECT_HORSE_GANON, 1, 0, ENVIEWER_SHADOW_HORSE, 20, ENVIEWER_DRAW_HORSE, &gHorseGanonSkel,
      &gHorseGanonGallopingAnim },
    /* ENVIEWER_TYPE_7_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfArmsCrossedAnim },
    /* ENVIEWER_TYPE_8_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfWalkAnim },
    /* ENVIEWER_TYPE_9_GANONDORF */
    { OBJECT_GANON, OBJECT_GANON, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gGanondorfSkel,
      &gGanondorfEndingFloatAnim },
};

static EnGanonMant* sGanondorfCape;

static Vec3f sGanondorfNeckWorldPos;

void EnViewer_SetupAction(EnViewer* this, EnViewerActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnViewer_Init(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;
    u8 type;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    EnViewer_SetupAction(this, EnViewer_InitImpl);
    sHorseSfxPlayed = false;
    type = this->actor.params >> 8;
    this->unused = 0;
    this->state = 0;
    this->isVisible = false;
    if (GdfVillain_Active(play) && (type == ENVIEWER_TYPE_4_HORSE_GANONDORF) &&
        (Object_GetIndex(&play->objectCtx, OBJECT_HORSE) < 0)) {
        Object_Spawn(&play->objectCtx, OBJECT_HORSE); // (Epona)
    }
    if (GdfVillain_Standing(play, type)) {
        sVsReady = false; // (Link stands in instead: no cape)
        sVsState = 0;
    } else if (GdfVillain_Active(play) && (type == ENVIEWER_TYPE_3_GANONDORF)) {
        sVlReady = false; // (Link rides instead: no cape)
        sVlBow = 0;
        sVlArrowT = -1;
        sVlRiderValid = false;
        sVlAimYaw = 0;
    } else if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF ||
               type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF ||
               type == ENVIEWER_TYPE_9_GANONDORF) {
        sGanondorfCape = (EnGanonMant*)Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_GANON_MANT,
                                                          0.0f, 0.0f, 0.0f, 0, 0, 0, 35);
    }
}

void EnViewer_Destroy(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;

    Skin_Free(play, &this->skin);
}

void EnViewer_InitAnimGanondorfOrZelda(EnViewer* this, PlayState* play, void* skeletonHeaderSeg,
                                       AnimationHeader* anim) {
    s16 type = this->actor.params >> 8;

    if (type == ENVIEWER_TYPE_2_ZELDA || type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF ||
        type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF || type == ENVIEWER_TYPE_9_GANONDORF) {
        SkelAnime_InitFlex(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    } else {
        SkelAnime_Init(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    }

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF ||
        type == ENVIEWER_TYPE_9_GANONDORF) {
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 1.0f);
    } else {
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 3.0f);
    }
}

void EnViewer_InitAnimImpa(EnViewer* this, PlayState* play, void* skeletonHeaderSeg, AnimationHeader* anim) {
    SkelAnime_InitFlex(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 3.0f);
}

void EnViewer_InitAnimHorse(EnViewer* this, PlayState* play, void* skeletonHeaderSeg, AnimationHeader* anim) {
    u8 type;

    Skin_Init(play, &this->skin, skeletonHeaderSeg, anim);
    type = this->actor.params >> 8;
    if (!(type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_4_HORSE_GANONDORF ||
          type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF ||
          type == ENVIEWER_TYPE_9_GANONDORF)) {
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 3.0f);
    } else {
        Animation_PlayOnceSetSpeed(&this->skin.skelAnime, anim, 1.0f);
    }
}

static EnViewerInitAnimFunc sInitAnimFuncs[] = {
    EnViewer_InitAnimGanondorfOrZelda,
    EnViewer_InitAnimHorse,
    EnViewer_InitAnimGanondorfOrZelda,
    EnViewer_InitAnimImpa,
};

static ActorShadowFunc sShadowDrawFuncs[] = {
    NULL,
    ActorShadow_DrawCircle,
    ActorShadow_DrawHorse,
};

void EnViewer_InitImpl(EnViewer* this, PlayState* play) {
    EnViewerInitData* initData = &sInitData[this->actor.params >> 8];
    EnViewerInitData epona;
    s32 skelObjBankIndex;

    if (GdfVillain_Active(play) && ((this->actor.params >> 8) == ENVIEWER_TYPE_4_HORSE_GANONDORF)) {
        epona = *initData;
        epona.skeletonObject = epona.animObject = OBJECT_HORSE;
        epona.skeletonHeaderSeg = (void*)gEponaSkel;
        epona.anim = (AnimationHeader*)gEponaRearingAnim;
        initData = &epona;
    }
    skelObjBankIndex = Object_GetIndex(&play->objectCtx, initData->skeletonObject);

    assert(skelObjBankIndex >= 0);

    this->animObjBankIndex = Object_GetIndex(&play->objectCtx, initData->animObject);
    assert(this->animObjBankIndex >= 0);

    if (!Object_IsLoaded(&play->objectCtx, skelObjBankIndex) ||
        !Object_IsLoaded(&play->objectCtx, this->animObjBankIndex)) {
        this->actor.flags &= ~ACTOR_FLAG_INSIDE_CULLING_VOLUME;
        return;
    }

    this->isVisible = true;
    this->actor.objBankIndex = skelObjBankIndex;
    Actor_SetObjectDependency(play, &this->actor);
    Actor_SetScale(&this->actor, initData->scale / 100.0f);
    ActorShape_Init(&this->actor.shape, initData->yOffset * 100, sShadowDrawFuncs[initData->shadowType],
                    initData->shadowScale);
    this->drawFuncIndex = initData->drawType;
    sInitAnimFuncs[this->drawFuncIndex](this, play, initData->skeletonHeaderSeg, initData->anim);
    EnViewer_SetupAction(this, EnViewer_UpdateImpl);
}

static s16 sTimer = 0;

// The villain's ride: the cutscene's moments for Ganondorf, as Link's (see GdfVillain_Active)
static void EnViewer_VillainLinkAnim(PlayState* play, LinkAnimationHeader* anim, f32 speed, s32 loop) {
    if (sVlSkel.animation != anim) {
        LinkAnimation_Change(play, &sVlSkel, anim, speed, 0.0f, Animation_GetLastFrame(anim),
                             loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -6.0f);
    }
}

static void EnViewer_VillainBowAnim(PlayState* play, LinkAnimationHeader* anim, s32 loop) {
    if (sVlBowSkel.animation != anim) {
        LinkAnimation_Change(play, &sVlBowSkel, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                             loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -6.0f);
    }
}

static void EnViewer_VillainVoice(EnViewer* this, u16 sfx) {
    Audio_PlayActorSound2(&this->actor, sfx);
}

static void EnViewer_UpdateVillainLink(EnViewer* this, PlayState* play) {
    u16 csFrames = play->csCtx.frames;
    s16 action = ((play->csCtx.state != CS_STATE_IDLE) && (play->csCtx.npcActions[1] != NULL))
                     ? play->csCtx.npcActions[1]->action
                     : 0;
    Player* player = GET_PLAYER(play);

    if (!sVlReady) {
        s32 age = gSaveContext.linkAge;

        gSaveContext.linkAge = LINK_AGE_ADULT;
        SkelAnime_InitLink(play, &sVlSkel, gPlayerSkelHeaders[LINK_AGE_ADULT], (LinkAnimationHeader*)gPlayerAnim_link_uma_wait_1, 9,
                           sVlJoints, sVlMorph, PLAYER_LIMB_MAX);
        SkelAnime_InitLink(play, &sVlBowSkel, gPlayerSkelHeaders[LINK_AGE_ADULT], (LinkAnimationHeader*)gPlayerAnim_link_bow_bow_wait, 9,
                           sVlBowJoints, sVlBowMorph, PLAYER_LIMB_MAX);
        gSaveContext.linkAge = age;
        sVlReady = true;
    }

    // His riding: still, rearing with Epona, looking about, and off at a gallop
    if (action == 12) {
        EnViewer_VillainLinkAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_uma_anim_fastrun, 1.5f, true);
    } else if (action == 1) {
        EnViewer_VillainLinkAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_uma_anim_stop, 1.0f, false);
    } else if (action == 4) {
        EnViewer_VillainLinkAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_uma_wait_2, 1.0f, true);
    } else {
        EnViewer_VillainLinkAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_uma_wait_1, 1.0f, true);
    }
    // The bow: raised (where Ganondorf charged his magic), held drawn, loosed, and lowered again as he rides off
    switch (sVlBow) {
        case 0:
            if (action == 5) {
                sVlBow = 1;
                EnViewer_VillainBowAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_bow_bow_ready, false);
                Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOW_DRAW);
            }
            break;
        case 1:
            if (sVlBowSkel.curFrame >= sVlBowSkel.endFrame - 1.0f) {
                sVlBow = 2;
                EnViewer_VillainBowAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_bow_bow_wait, true);
            }
            break;
        case 2:
            break;
        case 3:
            if ((action == 8) || (action == 12)) {
                sVlBow = 0;
            }
            break;
    }
    if ((csFrames == 1508) && (gSaveContext.sceneLayer == 5)) {
        EnViewer_VillainVoice(this, NA_SE_VO_LI_SWORD_L); // (where Ganondorf laughed: a battle cry)
    }
    // His upper body turns to aim at Ganondorf while the bow is up (the bow points where his chest faces)
    {
        s16 target = 0;

        if ((sVlBow != 0) && sVlRiderValid) {
            // (the drawbridge: at Ganondorf; the nightmare: at whoever watches it, the camera)
            Vec3f to = (gSaveContext.sceneLayer == 4) ? play->view.eye : player->actor.world.pos;

            target = Math_Vec3f_Yaw(&sVlRider, &to) - sVlHorseYaw;
            target = CLAMP(target, -0x5000, 0x5000); // (as far round as he can twist in the saddle)
        }
        Math_SmoothStepToS(&sVlAimYaw, target, 3, 0x800, 0x40);
    }
    // The shot (where Ganondorf's magic flew): the game's own Light Arrow, loosed from his bow at Ganondorf's chest
    if ((csFrames == 1545) && (sVlArrowT < 0) && (gSaveContext.sceneLayer == 5)) {
        Vec3f to = player->actor.world.pos;
        f32 dx, dy, dz, dxz;
        Actor* arrow;

        to.y += 90.0f; // (his chest)
        sVlBow = 3;
        EnViewer_VillainBowAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_bow_bow_shoot, false);
        EnViewer_VillainVoice(this, NA_SE_VO_LI_SWORD_N);
        sVlArrowFrom = sVlArrow = sVlBowHand;
        dx = to.x - sVlArrowFrom.x;
        dy = to.y - sVlArrowFrom.y;
        dz = to.z - sVlArrowFrom.z;
        dxz = sqrtf(SQ(dx) + SQ(dz));
        sVlArrowHitT = (s16)CLAMP(sqrtf(SQ(dxz) + SQ(dy)) / GDF_VL_ARROW_SPEED + 0.5f, 1.0f, 11.0f);
        // (an arrow flies only while the player has just shot one, during a cutscene: see EnArrow_Update)
        player->unk_A73 = 4;
        arrow = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, sVlArrowFrom.x, sVlArrowFrom.y, sVlArrowFrom.z,
                            RADF_TO_BINANG(Math_FAtan2F(-dy, dxz)), RADF_TO_BINANG(Math_FAtan2F(dx, dz)), 0,
                            ARROW_LIGHT);
        if (arrow == NULL) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_MAGIC_ARROW_SHOT);
        }
        sVlArrowT = 0;
    }
    if (sVlArrowT >= 0) {
        Vec3f to = player->actor.world.pos;

        to.y += 90.0f; // (his chest)
        sVlArrowT++;
        player->unk_A73 = MAX(player->unk_A73, 2); // (keeping the arrow in flight through the cutscene)
        if (sVlArrowT == sVlArrowHitT) {
            // It strikes him: a burst of light, and he drops to his knees
            EffectSsHitMark_SpawnFixedScale(play, 3, &to);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_EXPLOSION_LIGHT);
            Audio_PlayActorSound2(&player->actor, NA_SE_EN_GANON_DAMAGE2);
            BossGanon_Player_CsStagger(120);
        }
        if (sVlArrowT > sVlArrowHitT + 14) {
            sVlArrowT = -2; // (spent)
        }
    }
    if (action == 12) {
        static u8 sCried = false;

        if (!sCried) {
            EnViewer_VillainVoice(this, NA_SE_VO_LI_AUTO_JUMP); // off at a gallop
            sCried = true;
        }
    }

    LinkAnimation_Update(play, &sVlSkel);
    if (sVlBow != 0) {
        LinkAnimation_Update(play, &sVlBowSkel);
    }
}

// Link on foot where Ganondorf was: the cutscene's moments for him, as Link's
static void EnViewer_VillainStandAnim(PlayState* play, LinkAnimationHeader* anim, f32 speed, s32 loop) {
    if (sVsSkel.animation != anim) {
        LinkAnimation_Change(play, &sVsSkel, anim, speed, 0.0f, Animation_GetLastFrame(anim),
                             loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -8.0f);
    }
}

static s32 EnViewer_VillainStandDone(void) {
    return sVsSkel.curFrame >= sVsSkel.endFrame - 1.0f;
}

static void EnViewer_UpdateVillainStanding(EnViewer* this, PlayState* play, u8 type) {
    s16 action = ((play->csCtx.state != CS_STATE_IDLE) && (play->csCtx.npcActions[1] != NULL))
                     ? play->csCtx.npcActions[1]->action
                     : 0;

    if (!sVsReady) {
        s32 age = gSaveContext.linkAge;

        gSaveContext.linkAge = LINK_AGE_ADULT;
        SkelAnime_InitLink(play, &sVsSkel, gPlayerSkelHeaders[LINK_AGE_ADULT],
                           (LinkAnimationHeader*)gPlayerAnim_link_normal_wait, 9, sVsJoints, sVsMorph,
                           PLAYER_LIMB_MAX);
        gSaveContext.linkAge = age;
        sVsReady = true;
        sVsState = 0;
    }
    if (type == ENVIEWER_TYPE_8_GANONDORF) {
        // Zelda's window: he walks in, kneels before the king, and lifts his face to glare up at her
        switch (sVsState) {
            case 0:
                EnViewer_VillainStandAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_normal_wait, 1.0f, true);
                if (action == 9) {
                    sVsState = 1;
                }
                break;
            case 1:
                EnViewer_VillainStandAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_normal_walk, 1.0f, true);
                if (action == 10) {
                    sVsState = 2;
                }
                break;
            case 2:
                EnViewer_VillainStandAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_magic_tame, 1.0f, false);
                if (action == 4) {
                    sVsState = 3;
                }
                break;
            case 3:
                EnViewer_VillainStandAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_demo_kaoage, 1.0f, false);
                if (EnViewer_VillainStandDone()) {
                    sVsState = 4;
                }
                break;
            default:
                EnViewer_VillainStandAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_demo_kaoage_wait, 1.0f, true);
                break;
        }
    } else if (type == ENVIEWER_TYPE_7_GANONDORF) {
        // The giant who laughed: Link triumphant, his battle cry for the laugh
        if ((sVsState == 0) && (action == 7)) {
            sVsState = 1;
            Audio_PlaySfxGeneral(NA_SE_VO_LI_SWORD_L, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        EnViewer_VillainStandAnim(play,
                                  (LinkAnimationHeader*)((sVsState == 0) ? gPlayerAnim_link_normal_wait
                                                                         : gPlayerAnim_link_demo_get_itemA),
                                  1.0f, sVsState == 0);
    } else {
        EnViewer_VillainStandAnim(play, (LinkAnimationHeader*)gPlayerAnim_link_normal_wait, 1.0f, true);
    }
    LinkAnimation_Update(play, &sVsSkel);
}

void EnViewer_UpdateImpl(EnViewer* this, PlayState* play) {
    u8 type = this->actor.params >> 8;
    u16 csFrames;
    s32 animationEnded;
    u8 villain = GdfVillain_Active(play);
    // (Epona in place of Ganondorf's horse)
    AnimationHeader* horseRear = villain ? (AnimationHeader*)gEponaRearingAnim : (AnimationHeader*)gHorseGanonRearingAnim;
    AnimationHeader* horseIdle = villain ? (AnimationHeader*)gEponaIdleAnim : (AnimationHeader*)gHorseGanonIdleAnim;
    AnimationHeader* horseGallop = villain ? (AnimationHeader*)gEponaGallopingAnim : (AnimationHeader*)gHorseGanonGallopingAnim;

    if (villain && (type == ENVIEWER_TYPE_3_GANONDORF)) {
        EnViewer_UpdateVillainLink(this, play);
    }
    if (GdfVillain_Standing(play, type)) {
        EnViewer_UpdateVillainStanding(this, play, type);
    }

    if (type == ENVIEWER_TYPE_2_ZELDA) {
        if (gSaveContext.sceneLayer == 5) {
            csFrames = play->csCtx.frames;
            if (csFrames == 792) {
                Audio_PlayActorSound2(&this->actor, NA_SE_VO_Z0_SURPRISE);
            } else if (csFrames == 845) {
                Audio_PlayActorSound2(&this->actor, NA_SE_VO_Z0_THROW);
            }
        }
    } else if (type == ENVIEWER_TYPE_7_GANONDORF) {
        Actor_SetScale(&this->actor, 0.3f);
        this->actor.uncullZoneForward = 10000.0f;
        this->actor.uncullZoneScale = 10000.0f;
        this->actor.uncullZoneDownward = 10000.0f;
    } else if (type == ENVIEWER_TYPE_3_GANONDORF) {
        if (gSaveContext.sceneLayer == 4) {
            switch (play->csCtx.frames) {
                case 20:
                case 59:
                case 71:
                case 129:
                case 140:
                case 219:
                case 280:
                case 320:
                case 380:
                case 409:
                case 438:
                    Audio_PlaySfxGeneral(NA_SE_SY_DEMO_CUT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    break;
            }
        }
        if ((gSaveContext.sceneLayer == 5) && !villain) {
            if (play->csCtx.frames == 1508) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_ST_LAUGH);
            }
            if (play->csCtx.frames == 1545) {
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_DEMO_6K, 32.0f, 101.0f, 1226.0f, 0, 0, 0,
                                   0xC);
            }
        }
        if (play->csCtx.frames == 1020) {
            Audio_QueueSeqCmd(SEQ_PLAYER_FANFARE << 24 | NA_BGM_OPENING_GANON);
        }
        if (play->csCtx.frames == 960) {
            Audio_PlaySfxGeneral(NA_SE_EV_HORSE_GROAN, &this->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else if (type == ENVIEWER_TYPE_6_HORSE_GANONDORF) {
        if (gSaveContext.sceneLayer == 5 || gSaveContext.sceneLayer == 10) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN_LEVEL - SFX_FLAG);
        }
    } else if (type == ENVIEWER_TYPE_4_HORSE_GANONDORF) {
        s16 curFrame = this->skin.skelAnime.curFrame;

        if (this->skin.skelAnime.animation == horseRear) {
            if (curFrame == 8) {
                Audio_PlayActorSound2(&this->actor, villain ? NA_SE_EV_HORSE_NEIGH : NA_SE_EV_GANON_HORSE_NEIGH);
            }
            if (curFrame == 30) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND2);
            }
        } else if (this->skin.skelAnime.animation == horseIdle) {
            if (curFrame == 25) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_SANDDUST);
            }
        } else if (this->skin.skelAnime.animation == horseGallop) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN_LEVEL - SFX_FLAG);
        }
    }

    if (sTimer != 0) {
        sTimer--;
    }

    EnViewer_UpdatePosition(this, play);
    Actor_MoveXZGravity(&this->actor); // has no effect, speed/velocity and gravity are 0

    animationEnded = SkelAnime_Update(&this->skin.skelAnime);
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_4_HORSE_GANONDORF) {
        if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL) {
            if (play->csCtx.npcActions[1]->action == 2 && sTimer == 0) {
                if (type == ENVIEWER_TYPE_3_GANONDORF) {
                    if (this->skin.skelAnime.animation != &gYoungGanondorfHorsebackIdleAnim) {
                        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfHorsebackIdleAnim, 1.0f);
                    }
                } else if (this->skin.skelAnime.animation != horseIdle) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, horseIdle, 1.0f);
                }
            } else if (play->csCtx.npcActions[1]->action == 1) {
                sTimer = 100;
                if (type == ENVIEWER_TYPE_3_GANONDORF) {
                    if (this->skin.skelAnime.animation != &gYoungGanondorfHorsebackRearAnim) {
                        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfHorsebackRearAnim, 1.0f);
                    }
                } else if (this->skin.skelAnime.animation != horseRear) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, horseRear, 1.0f);
                }
            } else if (type == ENVIEWER_TYPE_3_GANONDORF) {
                switch (this->state) {
                    case 0:
                        if (play->csCtx.npcActions[1]->action == 4) {
                            Animation_MorphToPlayOnce(&this->skin.skelAnime,
                                                      &gYoungGanondorfHorsebackLookSidewaysStartAnim, -5.0f);
                            this->state++;
                        }
                        break;
                    case 1:
                        if (animationEnded) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackLookSidewaysLoopAnim,
                                                  -5.0f);
                            this->state++;
                        }
                        break;
                    case 2:
                        if (play->csCtx.npcActions[1]->action == 5) {
                            Animation_MorphToPlayOnce(&this->skin.skelAnime,
                                                      &gYoungGanondorfHorsebackMagicChargeUpStartAnim, -5.0f);
                            this->state++;
                        }
                        break;
                    case 3:
                        if (animationEnded) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackMagicChargeUpLoopAnim,
                                                  -5.0f);
                            this->state++;
                        }
                        break;
                    case 4:
                        if (play->csCtx.npcActions[1]->action == 11) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackLookSidewaysLoopAnim,
                                                  -20.0f);
                            this->state++;
                        }
                        break;
                    case 5:
                        if (play->csCtx.npcActions[1]->action == 8) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackIdleAnim, -15.0f);
                            this->state++;
                        }
                        break;
                    case 6:
                        if (play->csCtx.npcActions[1]->action == 12) {
                            if (!villain) {
                                Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_VOICE_DEMO);
                            }
                            Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfHorsebackRideAnim, 3.0f);
                            this->state++;
                        }
                        break;
                    case 7:
                        this->state = 0;
                        break;
                }
            } else if (this->skin.skelAnime.animation != horseGallop &&
                       play->csCtx.npcActions[1]->action == 12) {
                Animation_PlayLoopSetSpeed(&this->skin.skelAnime, horseGallop, 3.0f);
            }
        }
    } else if (type == ENVIEWER_TYPE_1_IMPA) {
        if (gSaveContext.sceneLayer == 5) {
            if (play->csCtx.frames == 845) {
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_ITEM_OCARINA, 4.0f, 81.0f, 2600.0f, 0, 0,
                                   0, 0);
            }
        } else {
            if (play->csCtx.frames == 195) {
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_ITEM_OCARINA, 4.0f, 81.0f, 2035.0f, 0, 0,
                                   0, 1);
            }
        }
        switch (this->state) {
            case 0:
                if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL &&
                    play->csCtx.npcActions[0]->action == 6 &&
                    this->skin.skelAnime.animation != &object_opening_demo1_Anim_002574) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_002574, 1.5f);
                    this->state++;
                }
                break;
            case 1:
                if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL &&
                    play->csCtx.npcActions[0]->action == 2 &&
                    this->skin.skelAnime.animation != &object_opening_demo1_Anim_0029CC) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_0029CC, 3.0f);
                    this->state++;
                }
                break;
        }
    } else if (type == ENVIEWER_TYPE_2_ZELDA) {
        if (play->sceneNum == SCENE_HYRULE_FIELD) { // Hyrule Field
            switch (this->state) {
                case 0:
                    if (play->csCtx.state != CS_STATE_IDLE) {
                        if (play->csCtx.npcActions[0] != NULL && play->csCtx.npcActions[0]->action == 6 &&
                            this->skin.skelAnime.animation != &object_opening_demo1_Anim_001410) {
                            Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_001410, 1.5f);
                            this->state++;
                        }
                    }
                    break;
                case 1:
                    if (play->csCtx.state != CS_STATE_IDLE) {
                        if (play->csCtx.npcActions[0] != NULL && play->csCtx.npcActions[0]->action == 2 &&
                            this->skin.skelAnime.animation != &object_opening_demo1_Anim_000450) {
                            Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_000450, 3.0f);
                            this->state++;
                        }
                    }
                    break;
            }
        } else {
            Audio_SetBaseFilter(0);
            switch (this->state) {
                case 0:
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_00504C, 1.0f);
                    this->state++;
                    break;
                case 1:
                    if (play->csCtx.npcActions[0]->action == 11) {
                        Animation_MorphToPlayOnce(&this->skin.skelAnime, &object_opening_demo1_Anim_00420C, -5.0f);
                        this->state++;
                    }
                    break;
                case 2:
                    if (animationEnded) {
                        Animation_MorphToLoop(&this->skin.skelAnime, &object_opening_demo1_Anim_0048FC, -5.0f);
                        this->state++;
                    }
                    break;
                case 3:
                    break;
            }
        }
    } else if (type == ENVIEWER_TYPE_7_GANONDORF) {
        switch (this->state) {
            case 0:
                if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL &&
                    play->csCtx.npcActions[1]->action == 7) {
                    if (!GdfVillain_Standing(play, type)) {
                        Audio_PlaySfxGeneral(NA_SE_EN_GANON_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    }
                    Animation_MorphToPlayOnce(&this->skin.skelAnime, &gYoungGanondorfLaughStartAnim, -5.0f);
                    this->state++;
                }
                break;
            case 1:
                if (animationEnded) {
                    Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfLaughLoopAnim, -5.0f);
                    this->state++;
                }
                break;
        }
    } else if (type == ENVIEWER_TYPE_8_GANONDORF) {
        switch (this->state) {
            case 0:
                if (play->csCtx.state != CS_STATE_IDLE) {
                    if (play->csCtx.npcActions[1] != NULL && play->csCtx.npcActions[1]->action == 9) {
                        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfWalkAnim, 1.0f);
                        this->state++;
                    }
                }
                break;
            case 1:
                if (play->csCtx.npcActions[1]->action == 10) {
                    Animation_MorphToPlayOnce(&this->skin.skelAnime, &gYoungGanondorfKneelStartAnim, -10.0f);
                    this->state++;
                }
                break;
            case 2:
                if (animationEnded) {
                    Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfKneelLoopAnim, -5.0f);
                    this->state++;
                }
                break;
            case 3:
                if (play->csCtx.npcActions[1]->action == 4) {
                    Animation_MorphToPlayOnce(&this->skin.skelAnime, &gYoungGanondorfKneelLookSidewaysAnim, -5.0f);
                    this->state++;
                }
                break;
            default:
                this->state = 0;
                break;
        }
    }
}

void EnViewer_Update(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    this->actionFunc(this, play);
}

s32 EnViewer_Ganondorf3OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    if (gSaveContext.sceneLayer == 4) {
        if (play->csCtx.frames >= 400) {
            if (limbIndex == 5) {
                *dList = gYoungGanondorfOpenLeftHandDL;
            }
        }
    } else {
        if (play->csCtx.frames >= 1510 && play->csCtx.frames <= 1650) {
            if (limbIndex == 5) {
                *dList = gYoungGanondorfOpenLeftHandDL;
            }
        }
    }
    return false;
}

void EnViewer_Ganondorf9PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (limbIndex == 11) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfEyesDL));
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnViewer_GanondorfPostLimbDrawUpdateCapeVec(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == 15) {
        Matrix_MultVec3f(&zeroVec, &sGanondorfNeckWorldPos);
    }
}

static void EnViewer_DrawVillainLink(EnViewer* this, PlayState* play);
static void EnViewer_DrawVillainStanding(EnViewer* this, PlayState* play);

void EnViewer_DrawGanondorf(EnViewer* this, PlayState* play) {
    s16 frames = 0;
    s16 type;

    if (GdfVillain_Active(play) && ((this->actor.params >> 8) == ENVIEWER_TYPE_3_GANONDORF)) {
        GdfVillain_NoteShown(&this->actor.world.pos, 1.0f, true); // (for the cutscene camera: framed for him)
        EnViewer_DrawVillainLink(this, play);
        return;
    }
    if (GdfVillain_Standing(play, (u8)(this->actor.params >> 8))) {
        GdfVillain_NoteShown(&this->actor.world.pos, this->actor.scale.x / 0.1f, false);
        EnViewer_DrawVillainStanding(this, play);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    type = this->actor.params >> 8;
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF || type == ENVIEWER_TYPE_7_GANONDORF ||
        type == ENVIEWER_TYPE_8_GANONDORF) {
        if (gSaveContext.sceneLayer != 4) {
            frames = 149;
        }

        if (frames + 1127 >= play->csCtx.frames) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeOpenTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeOpenTex));

        } else if (frames + 1128 >= play->csCtx.frames) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeHalfTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeHalfTex));

        } else if (frames + 1129 >= play->csCtx.frames) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeClosedTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeClosedTex));

        } else {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeLookingDownTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeLookingDownTex));
        }
    } else if (type == ENVIEWER_TYPE_9_GANONDORF) {
        gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gGanondorfCrazedEyeTex));
    }

    if (type == ENVIEWER_TYPE_9_GANONDORF) {
        SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, EnViewer_Ganondorf9PostLimbDraw, this);
    } else if (type == ENVIEWER_TYPE_3_GANONDORF) {
        SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_Ganondorf3OverrideLimbDraw,
                                  EnViewer_GanondorfPostLimbDrawUpdateCapeVec, this);
        EnViewer_UpdateGanondorfCape(play, this);
    } else if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF ||
               type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF) {
        if ((play->csCtx.state != CS_STATE_IDLE) && (play->csCtx.npcActions[1] != NULL)) {
            SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, EnViewer_GanondorfPostLimbDrawUpdateCapeVec,
                                      this);
            EnViewer_UpdateGanondorfCape(play, this);
        }
    } else {
        SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, NULL, this);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_EponaOverrideLimbDraw(Actor* thisx, PlayState* play, s32 limbIndex, Skin* skin) {
    if (limbIndex == 13) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gEponaEyeOpenTex));
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return true;
}

static void EnViewer_EponaPostDraw(Actor* thisx, PlayState* play, Skin* skin) {
    Vec3f riderOffset = { 600.0f, -1670.0f, 0.0f }; // (EnHorse's: the saddle)

    Skin_GetLimbPos(skin, 30, &riderOffset, &sVlRider);
    sVlRiderValid = true;
}

void EnViewer_DrawHorse(EnViewer* this, PlayState* play) {
    if (GdfVillain_Active(play) && ((this->actor.params >> 8) == ENVIEWER_TYPE_4_HORSE_GANONDORF)) {
        // As EnHorse draws Epona: her eyes set right before her head's limb, the saddle read while she's drawn
        func_800A6360(&this->actor, play, &this->skin, EnViewer_EponaPostDraw, EnViewer_EponaOverrideLimbDraw, true);
        sVlHorseYaw = this->actor.shape.rot.y;
        return;
    }
    func_800A6330(&this->actor, play, &this->skin, NULL, true);
}

// The villain Link's hands: closed on the reins, or holding the bow (in his right hand, as he does)
static s32 EnViewer_VillainOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        *dList = gLinkAdultLeftHandClosedNearDL;
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        *dList = (sVlBow != 0) ? gLinkAdultRightHandHoldingBowNearDL : gLinkAdultRightHandClosedNearDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = gLinkAdultHylianShieldSwordAndSheathNearDL;
    } else if ((limbIndex == PLAYER_LIMB_UPPER) && (sVlAimYaw != 0)) {
        Matrix_RotateY(BINANG_TO_RAD(sVlAimYaw), MTXMODE_APPLY); // (turned toward Ganondorf, as Link aims)
    }
    return false;
}

static void EnViewer_VillainPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZero = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == PLAYER_LIMB_R_HAND) {
        Matrix_MultVec3f(&sZero, &sVlBowHand);
        if (sVlBow != 0) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gLinkAdultBowStringDL);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}

static void EnViewer_DrawVillainLink(EnViewer* this, PlayState* play) {
    Actor* horse = NULL;
    Actor* a;
    s32 age = gSaveContext.linkAge;
    s32 i;

    if (!sVlReady) {
        return;
    }
    for (a = play->actorCtx.actorLists[this->actor.category].head; a != NULL; a = a->next) {
        if ((a->id == ACTOR_EN_VIEWER) && ((a->params >> 8) == ENVIEWER_TYPE_4_HORSE_GANONDORF)) {
            horse = a;
        }
    }
    // His arms from the bow's animation while he has it up (over the riding's legs and body)
    if (sVlBow != 0) {
        for (i = PLAYER_LIMB_UPPER; i < PLAYER_LIMB_TORSO; i++) {
            sVlJoints[i] = sVlBowJoints[i];
        }
    }
    OPEN_DISPS(play->state.gfxCtx);
    if ((horse != NULL) && sVlRiderValid) {
        // His hips on the saddle, whatever height his riding animation carries them at
        Matrix_Translate(sVlRider.x, sVlRider.y + GDF_VL_SEAT - sVlJoints[0].y * 0.01f, sVlRider.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(horse->shape.rot.y), MTXMODE_APPLY);
    } else {
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    }
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x0C, gCullBackDList);
    gSPSegment(POLY_XLU_DISP++, 0x0C, gCullBackDList);
    gSaveContext.linkAge = LINK_AGE_ADULT; // (his textures and parts, whatever age the player is)
    Player_DrawImpl(play, sVlSkel.skeleton, sVlJoints, sVlSkel.dListCount, 0, PLAYER_TUNIC_KOKIRI, 0, 0,
                    EnViewer_VillainOverrideLimbDraw, EnViewer_VillainPostLimbDraw, &this->actor);
    gSaveContext.linkAge = age;

    CLOSE_DISPS(play->state.gfxCtx);
}

// Link on foot: hands open, his sword and shield on his back
static s32 EnViewer_VillainStandOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                 void* thisx) {
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        *dList = gLinkAdultLeftHandNearDL;
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        *dList = gLinkAdultRightHandClosedNearDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = gLinkAdultHylianShieldSwordAndSheathNearDL;
    }
    return false;
}

static void EnViewer_DrawVillainStanding(EnViewer* this, PlayState* play) {
    s32 age = gSaveContext.linkAge;
    // (Link's size against Ganondorf's: the cutscene's 0.1 is his normal size, so the laughing giant stays a giant)
    f32 scale = 0.01f * (this->actor.scale.x / 0.1f);

    if (!sVsReady || (play->csCtx.state == CS_STATE_IDLE)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x0C, gCullBackDList);
    gSPSegment(POLY_XLU_DISP++, 0x0C, gCullBackDList);
    gSaveContext.linkAge = LINK_AGE_ADULT; // (his textures and parts, whatever age the player is)
    Player_DrawImpl(play, sVsSkel.skeleton, sVsJoints, sVsSkel.dListCount, 0, PLAYER_TUNIC_KOKIRI, 0, 0,
                    EnViewer_VillainStandOverrideLimbDraw, NULL, &this->actor);
    gSaveContext.linkAge = age;
    CLOSE_DISPS(play->state.gfxCtx);
}

s32 EnViewer_ZeldaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    if (play->sceneNum == SCENE_HYRULE_FIELD) { // Hyrule Field
        if (limbIndex == 2) {
            *dList = gChildZeldaCutsceneDressDL;
        }
        if (limbIndex == 7) {
            *dList = NULL;
        }
        if (limbIndex == 8) {
            *dList = NULL;
        }
        if (limbIndex == 9) {
            *dList = NULL;
        }
        if (limbIndex == 3) {
            *dList = NULL;
        }
        if (limbIndex == 5) {
            *dList = NULL;
        }
    }
    return false;
}

void EnViewer_ZeldaPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    s32 pad;

    if (play->sceneNum == SCENE_TEMPLE_OF_TIME) {
        if (limbIndex == 16) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, gChildZeldaOcarinaOfTimeDL);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}

void EnViewer_DrawZelda(EnViewer* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    if (play->sceneNum == SCENE_HYRULE_FIELD) { // Hyrule Field
        if (play->csCtx.frames < 771) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeInTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeOutTex));
        } else if (play->csCtx.frames < 772) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
        } else if (play->csCtx.frames < 773) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        } else if (play->csCtx.frames < 791) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeWideTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeWideTex));
        } else if (play->csCtx.frames < 792) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
        } else if (play->csCtx.frames < 793) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        } else {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeInTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeOutTex));
        }

        if (gSaveContext.sceneLayer == 6) {
            gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthSurprisedTex));
        } else {
            if (play->csCtx.frames < 758) {
                gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthWorriedTex));
            } else if (play->csCtx.frames < 848) {
                gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthSurprisedTex));
            } else {
                gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthWorriedTex));
            }
        }
    } else {
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthWorriedTex));
    }
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_ZeldaOverrideLimbDraw, EnViewer_ZeldaPostLimbDraw,
                              this);
    CLOSE_DISPS(play->state.gfxCtx);
}

s32 EnViewer_ImpaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    if (limbIndex == 16) {
        *dList = gImpaHeadMaskedDL;
    }
    return false;
}

void EnViewer_DrawImpa(EnViewer* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gImpaEyeOpenTex));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gImpaEyeOpenTex));
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x0C, &D_80116280[2]);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_ImpaOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static EnViewerDrawFunc sDrawFuncs[] = {
    EnViewer_DrawGanondorf,
    EnViewer_DrawHorse,
    EnViewer_DrawZelda,
    EnViewer_DrawImpa,
};

void EnViewer_Draw(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;
    s32 pad;
    s16 type;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->isVisible) {
        type = this->actor.params >> 8;
        if (type <= ENVIEWER_TYPE_2_ZELDA) { // zelda's horse, impa and zelda
            if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL) {
                Gfx_SetupDL_25Opa(play->state.gfxCtx);
                sDrawFuncs[this->drawFuncIndex](this, play);
            }
        } else if ((play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL) ||
                   type == ENVIEWER_TYPE_9_GANONDORF) {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            sDrawFuncs[this->drawFuncIndex](this, play);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewer_UpdatePosition(EnViewer* this, PlayState* play) {
    Vec3f startPos;
    Vec3f endPos;
    f32 lerpFactor;
    s16 type = this->actor.params >> 8;

    if (type <= ENVIEWER_TYPE_2_ZELDA) { // zelda's horse, impa and zelda
        if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL &&
            play->csCtx.frames < play->csCtx.npcActions[0]->endFrame) {
            if (type == ENVIEWER_TYPE_0_HORSE_ZELDA) {
                if (!sHorseSfxPlayed) {
                    sHorseSfxPlayed = true;
                    Audio_PlaySfxGeneral(NA_SE_EV_HORSE_NEIGH, &this->actor.projectedPos, 4,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                }
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN_LEVEL - SFX_FLAG);
            }

            startPos.x = play->csCtx.npcActions[0]->startPos.x;
            startPos.y = play->csCtx.npcActions[0]->startPos.y;
            startPos.z = play->csCtx.npcActions[0]->startPos.z;
            endPos.x = play->csCtx.npcActions[0]->endPos.x;
            endPos.y = play->csCtx.npcActions[0]->endPos.y;
            endPos.z = play->csCtx.npcActions[0]->endPos.z;
            lerpFactor = Environment_LerpWeight(play->csCtx.npcActions[0]->endFrame,
                                                play->csCtx.npcActions[0]->startFrame, play->csCtx.frames);
            this->actor.world.pos.x = (endPos.x - startPos.x) * lerpFactor + startPos.x;
            this->actor.world.pos.y = (endPos.y - startPos.y) * lerpFactor + startPos.y;
            this->actor.world.pos.z = (endPos.z - startPos.z) * lerpFactor + startPos.z;
        }
    } else { // ganondorf and ganondorf's horse
        if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL &&
            play->csCtx.frames < play->csCtx.npcActions[1]->endFrame) {
            startPos.x = play->csCtx.npcActions[1]->startPos.x;
            startPos.y = play->csCtx.npcActions[1]->startPos.y;
            startPos.z = play->csCtx.npcActions[1]->startPos.z;
            endPos.x = play->csCtx.npcActions[1]->endPos.x;
            endPos.y = play->csCtx.npcActions[1]->endPos.y;
            endPos.z = play->csCtx.npcActions[1]->endPos.z;
            lerpFactor = Environment_LerpWeight(play->csCtx.npcActions[1]->endFrame,
                                                play->csCtx.npcActions[1]->startFrame, play->csCtx.frames);
            this->actor.world.pos.x = (endPos.x - startPos.x) * lerpFactor + startPos.x;
            this->actor.world.pos.y = (endPos.y - startPos.y) * lerpFactor + startPos.y;
            this->actor.world.pos.z = (endPos.z - startPos.z) * lerpFactor + startPos.z;

            if (play->csCtx.npcActions[1]->action == 12) {
                s16 yaw = Math_Vec3f_Yaw(&startPos, &endPos);

                Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 0xA, 0x3E8, 1);
                Math_SmoothStepToS(&this->actor.shape.rot.y, yaw, 0xA, 0x3E8, 1);
            }

            if (type == ENVIEWER_TYPE_9_GANONDORF) {
                this->actor.world.rot.x = play->csCtx.npcActions[1]->urot.x;
                this->actor.world.rot.y = play->csCtx.npcActions[1]->urot.y;
                this->actor.world.rot.z = play->csCtx.npcActions[1]->urot.z;
                this->actor.shape.rot.x = play->csCtx.npcActions[1]->urot.x;
                this->actor.shape.rot.y = play->csCtx.npcActions[1]->urot.y;
                this->actor.shape.rot.z = play->csCtx.npcActions[1]->urot.z;
            }
        }
        if (type == ENVIEWER_TYPE_5_GANONDORF) {
            Audio_PlaySfxGeneral(NA_SE_EV_BURNING - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            EnViewer_DrawFireEffects(this, play);
        }
    }
}

void EnViewer_InitFireEffect(EnViewer* this, PlayState* play, s16 i) {
    EnViewerFireEffect* eff;

    if ((i % 2) == 0) {
        eff = &this->fireEffects[i];
        eff->startPos.x = 100.0f;
        eff->startPos.y = -420.0f;
        eff->startPos.z = 400.0f;
        eff->endPos.x = 100.0f;
        eff->endPos.y = -420.0f;
        eff->endPos.z = -400.0f;
        eff->scale = (Rand_ZeroOne() * 5.0f + 12.0f) * 0.001f;
        eff->epoch++;
    } else {
        eff = &this->fireEffects[i];
        eff->startPos.x = -100.0f;
        eff->startPos.y = -420.0f;
        eff->startPos.z = 400.0f;
        eff->endPos.x = -100.0f;
        eff->endPos.y = -420.0f;
        eff->endPos.z = -400.0f;
        eff->scale = (Rand_ZeroOne() * 5.0f + 12.0f) * 0.001f;
        eff->epoch++;
    }
    if (this) {}
}

void EnViewer_DrawFireEffects(EnViewer* this2, PlayState* play) {
    EnViewer* this = this2;
    s16 i;

    OPEN_DISPS(play->state.gfxCtx);
    for (i = 0; i < ARRAY_COUNT(this->fireEffects); i++) {
        switch (this->fireEffects[i].state) {
            case 0:
                EnViewer_InitFireEffect(this, play, i);
                this->fireEffects[i].lerpFactor = (i >> 1) * 0.1f;
                this->fireEffects[i].lerpFactorSpeed = 0.01f;
                this->fireEffects[i].state++;
                break;
            case 1:
                Math_SmoothStepToF(&this->fireEffects[i].lerpFactor, 1.0f, 1.0f, this->fireEffects[i].lerpFactorSpeed,
                                   this->fireEffects[i].lerpFactorSpeed);
                this->fireEffects[i].pos.x =
                    this->fireEffects[i].startPos.x +
                    (this->fireEffects[i].endPos.x - this->fireEffects[i].startPos.x) * this->fireEffects[i].lerpFactor;
                this->fireEffects[i].pos.y =
                    this->fireEffects[i].startPos.y +
                    (this->fireEffects[i].endPos.y - this->fireEffects[i].startPos.y) * this->fireEffects[i].lerpFactor;
                this->fireEffects[i].pos.z =
                    this->fireEffects[i].startPos.z +
                    (this->fireEffects[i].endPos.z - this->fireEffects[i].startPos.z) * this->fireEffects[i].lerpFactor;
                if (this->fireEffects[i].lerpFactor >= 1.0f) {
                    this->fireEffects[i].state++;
                }
                break;
            case 2:
                EnViewer_InitFireEffect(this, play, i);
                this->fireEffects[i].lerpFactor = 0.0f;
                this->fireEffects[i].lerpFactorSpeed = 0.01f;
                this->fireEffects[i].state--;
                break;
        }

        FrameInterpolation_RecordOpenChild(&this->fireEffects[i], this->fireEffects[i].epoch);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_Translate(this->fireEffects[i].pos.x, this->fireEffects[i].pos.y, this->fireEffects[i].pos.z,
                         MTXMODE_NEW);
        Matrix_Scale(this->fireEffects[i].scale, this->fireEffects[i].scale, this->fireEffects[i].scale, MTXMODE_APPLY);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                      (10 * i - 20 * play->state.frames) % 512, 32, 128, 0, 0, 0, -20));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 50, 00, 255);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPMatrix(POLY_XLU_DISP++, SEG_ADDR(1, 0), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
        FrameInterpolation_RecordCloseChild();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewer_UpdateGanondorfCape(PlayState* play, EnViewer* this) {
    static s16 yOscillationPhase = 0;
    Vec3f forearmModelOffset;
    Vec3f forearmWorldOffset;

    if ((this->actor.params >> 8) == ENVIEWER_TYPE_5_GANONDORF) {
        sGanondorfCape->backPush = BREG(54) / 10.0f;
        sGanondorfCape->backSwayMagnitude = (BREG(60) + 25) / 100.0f;
        sGanondorfCape->sideSwayMagnitude = (BREG(55) - 45) / 10.0f;
        sGanondorfCape->minY = -10000.0f;
        sGanondorfCape->minDist = 0.0f;
        sGanondorfCape->gravity = (BREG(67) - 10) / 10.0f;
        forearmModelOffset.x = KREG(16) - 13.0f;
        forearmModelOffset.y = KREG(17) + 3.0f + Math_SinS(yOscillationPhase) * KREG(20);
        forearmModelOffset.z = KREG(18) - 10.0f;
        yOscillationPhase += KREG(19) * 0x1000 + 0x2000;

        Matrix_RotateY((this->actor.shape.rot.y / (f32)0x8000) * M_PI, MTXMODE_NEW);
        Matrix_MultVec3f(&forearmModelOffset, &forearmWorldOffset);
        sGanondorfCape->rightForearmPos.x = sGanondorfNeckWorldPos.x + forearmWorldOffset.x;
        sGanondorfCape->rightForearmPos.y = sGanondorfNeckWorldPos.y + forearmWorldOffset.y;
        sGanondorfCape->rightForearmPos.z = sGanondorfNeckWorldPos.z + forearmWorldOffset.z;
        forearmModelOffset.x = -(KREG(16) - 13.0f);
        Matrix_MultVec3f(&forearmModelOffset, &forearmWorldOffset);
        sGanondorfCape->leftForearmPos.x = sGanondorfNeckWorldPos.x + forearmWorldOffset.x;
        sGanondorfCape->leftForearmPos.y = sGanondorfNeckWorldPos.y + forearmWorldOffset.y;
        sGanondorfCape->leftForearmPos.z = sGanondorfNeckWorldPos.z + forearmWorldOffset.z;
    }
}
