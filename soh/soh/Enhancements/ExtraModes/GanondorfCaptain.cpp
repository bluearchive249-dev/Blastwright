// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// Play as Ganondorf: the captain of his guard. In the Gerudo Fortress a Gerudo warrior with her two scimitars waits
// for him where he arrives; speaking with her calls his bodyguards to his side, or sends them back to their posts
// (the "Gerudo Bodyguards" setting, whose behaviour is in z_en_geldb.c and z_boss_ganon.c).
// She is drawn from the game's own Gerudo fighter (object_geldb, from the player's ROM).

#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/ActorDB.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
s32 PlayAsGanondorf_ShouldHideLink(PlayState* play, Player* player);
extern PlayState* gPlayState;
}

#define CAPTAIN_TEXT_ID 0x0206
#define CAPTAIN_LIMB_MAX 24
#define CAPTAIN_LIMB_NECK 3
#define CAPTAIN_LIMB_HEAD 6
#define CAPTAIN_LIMB_R_SWORD 11
#define CAPTAIN_LIMB_L_SWORD 16
#define BODYGUARDS_CVAR CVAR_ENHANCEMENT("PlayAsGanondorf.Bodyguards")

namespace {

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[CAPTAIN_LIMB_MAX];
    Vec3s morphTable[CAPTAIN_LIMB_MAX];
    s16 blink;
    s16 blinkTimer;
    s16 headYaw;
    u8 talking;
} GdfCaptain;

s16 sCaptainId = -1;
bool sSpawnedHere = false;

// Her words, and his order, by whether his guard is with him (calling them, or sending them back)
const char* sCallLines[] = {
    "%rCaptain:%w My king. Your blades are idle, and your road is long.^%rGanondorf:%w Then let them walk it with "
    "me. Choose three of your best, Captain. I would have the desert at my back.^%rCaptain:%w They are chosen. "
    "Green, blue and red, sworn to your shadow. Where you go, they go.",
    "%rCaptain:%w Lord Ganondorf. The sisters ask to ride with their king.^%rGanondorf:%w Let them. Hyrule should "
    "learn what it means to face the Gerudo in earnest.^%rCaptain:%w Three swords for the king. They will not fall "
    "behind.",
};
const char* sDismissLines[] = {
    "%rCaptain:%w My king. Your guard has fought well at your side.^%rGanondorf:%w Well enough. Send them back to "
    "the walls, Captain. A fortress without its sisters is only stone.^%rCaptain:%w As you command. Call, and they "
    "will come again.",
    "%rCaptain:%w The three are weary, lord, though they would never say it.^%rGanondorf:%w Then let them rest. I "
    "walk alone a while. It suits me.^%rCaptain:%w They return to their posts. The fortress will be ready when you "
    "are.",
};
int sLine = 0;

void Captain_Init(Actor* thisx, PlayState* play) {
    GdfCaptain* c = (GdfCaptain*)thisx;

    SkelAnime_InitFlex(play, &c->skelAnime, (FlexSkeletonHeader*)"__OTR__objects/object_geldb/gGerudoRedSkel",
                       (AnimationHeader*)"__OTR__objects/object_geldb/gGerudoRedNeutralAnim", c->jointTable,
                       c->morphTable, CAPTAIN_LIMB_MAX);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 30.0f);
    Actor_SetScale(thisx, 0.012f);
    thisx->targetMode = 6;
    thisx->textId = CAPTAIN_TEXT_ID;
    thisx->gravity = -1.0f;
    c->blinkTimer = 40;
}

void Captain_Destroy(Actor* thisx, PlayState* play) {
}

void Captain_Update(Actor* thisx, PlayState* play) {
    GdfCaptain* c = (GdfCaptain*)thisx;
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&c->skelAnime);
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 30.0f, 20.0f, 60.0f, 5);
    // She faces him as he comes near, her head first
    if (thisx->xzDistToPlayer < 300.0f) {
        Math_SmoothStepToS(&thisx->shape.rot.y, thisx->yawTowardsPlayer, 6, 0x300, 0x40);
    }
    Math_SmoothStepToS(&c->headYaw, CLAMP((s16)(thisx->yawTowardsPlayer - thisx->shape.rot.y), -0x2000, 0x2000), 4,
                       0x400, 0x40);
    if (c->blinkTimer-- <= 0) {
        if (++c->blink > 3) {
            c->blink = 0;
            c->blinkTimer = Rand_S16Offset(30, 60);
        }
    }
    Actor_SetFocus(thisx, 70.0f);

    if (c->talking) {
        // The talk is over: his order carried out
        if (play->msgCtx.msgMode == MSGMODE_NONE) {
            bool on = CVarGetInteger(BODYGUARDS_CVAR, 0) != 0;

            c->talking = false;
            CVarSetInteger(BODYGUARDS_CVAR, on ? 0 : 1);
            CVarSave();
            sLine++;
            Audio_PlayActorSound2(thisx, on ? NA_SE_IT_SWORD_PUTAWAY : NA_SE_IT_SWORD_PICKOUT);
        }
        return;
    }
    if (Actor_ProcessTalkRequest(thisx, play)) {
        c->talking = true;
    } else {
        Actor_OfferTalk(thisx, play, 120.0f);
    }
    (void)player;
}

s32 Captain_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    GdfCaptain* c = (GdfCaptain*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (limbIndex == CAPTAIN_LIMB_NECK) {
        rot->x += c->headYaw;
    } else if (limbIndex == CAPTAIN_LIMB_HEAD) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 80, 60, 10, 255);
    } else if ((limbIndex == CAPTAIN_LIMB_R_SWORD) || (limbIndex == CAPTAIN_LIMB_L_SWORD)) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 210, 175, 60, 255); // (a captain's blades: gilded)
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    } else {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 70, 20, 90, 255); // (her garb: the royal purple of the king's guard)
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

void Captain_Draw(Actor* thisx, PlayState* play) {
    static const char* sEyes[] = { "__OTR__objects/object_geldb/gGerudoRedEyeOpenTex",
                                   "__OTR__objects/object_geldb/gGerudoRedEyeHalfTex",
                                   "__OTR__objects/object_geldb/gGerudoRedEyeShutTex",
                                   "__OTR__objects/object_geldb/gGerudoRedEyeHalfTex" };
    GdfCaptain* c = (GdfCaptain*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyes[c->blink & 3]);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &c->skelAnime, Captain_OverrideLimbDraw, NULL, c);
}

void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if ((gPlayState == nullptr) || (*textId != CAPTAIN_TEXT_ID) || (gPlayState->sceneNum != SCENE_GERUDOS_FORTRESS)) {
        return;
    }
    bool on = CVarGetInteger(BODYGUARDS_CVAR, 0) != 0;
    const char* line = on ? sDismissLines[sLine % ARRAY_COUNT(sDismissLines)]
                          : sCallLines[sLine % ARRAY_COUNT(sCallLines)];
    CustomMessage msg = CustomMessage(line, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM);

    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// She waits where he arrives in the fortress, a few steps to his side
void OnFrame() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (play->sceneNum != SCENE_GERUDOS_FORTRESS) || !PlayAsGanondorf_ShouldHideLink(play, GET_PLAYER(play))) {
        sSpawnedHere = false;
        return;
    }
    if (sSpawnedHere || (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->csCtx.state != CS_STATE_IDLE)) {
        return;
    }
    if (sCaptainId < 0) {
        ActorDBInit i;

        i.name = "Gdf_GuardCaptain";
        i.desc = "The captain of Ganondorf's guard (Play as Ganondorf)";
        i.category = ACTORCAT_NPC;
        i.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        i.objectId = OBJECT_GAMEPLAY_KEEP;
        i.instanceSize = sizeof(GdfCaptain);
        i.init = Captain_Init;
        i.destroy = Captain_Destroy;
        i.update = Captain_Update;
        i.draw = Captain_Draw;
        sCaptainId = ActorDB::Instance->AddEntry(i).entry.id;
    }
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;
    Vec3f pos = player->actor.world.pos;

    pos.x += Math_SinS(yaw + 0x3000) * 110.0f;
    pos.z += Math_CosS(yaw + 0x3000) * 110.0f;
    Actor_Spawn(&play->actorCtx, play, sCaptainId, pos.x, pos.y + 10.0f, pos.z, 0, (s16)(yaw + 0x8000), 0, 0);
    sSpawnedHere = true;
}

void RegisterGanondorfCaptain() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnFrame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { sSpawnedHere = false; });
}

static RegisterShipInitFunc initFunc(RegisterGanondorfCaptain, {});

} // namespace
