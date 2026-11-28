// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// SoH: "Play as Ganondorf", the traveller's Malon. The public edition has no Extra Modes menu with its test buttons, so
// in Link's house adult Malon (the game's own model and idle, object_ma2) stands waiting: talk to her and she takes
// you straight to the mod's scenes: the opening nightmare, the drawbridge escape, Zelda's courtyard, and the Hero at
// the top of Ganon's Tower. Your save's story flags are left alone.
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/object_ma2/object_ma2.h"
#include "soh/frame_interpolation.h"
extern PlayState* gPlayState;
}

namespace {
struct TravelMalon;
}
// (outside the namespace: OPEN_DISPS declares its frame interpolation calls in place)
static void Malon_Draw(Actor* thisx, PlayState* play);

namespace {

#define TEXT_MALON_MENU 0x7A40   // where to (the first page)
#define TEXT_MALON_SCENES 0x7A41 // which scene

struct TravelMalon {
    Actor actor;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    u8 talking;
};

s16 sActorId = -1;
s16 sSpawnedScene = -1;
s16 sWaitFrames = 0;
u16 sPage = TEXT_MALON_MENU;

ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 18, 46, 0, { 0, 0, 0 } },
};

bool MalonOn() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.TravelMalon"), 1);
}

void GoTo(PlayState* play, s32 entrance, u16 cutscene) {
    play->nextEntranceIndex = (s16)entrance;
    gSaveContext.nextCutsceneIndex = cutscene;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
}

void Malon_Init(Actor* thisx, PlayState* play) {
    TravelMalon* self = (TravelMalon*)thisx;

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 18.0f);
    SkelAnime_InitFlex(play, &self->skelAnime, (FlexSkeletonHeader*)gMalonAdultSkel, NULL, NULL, NULL, 0);
    Animation_PlayLoop(&self->skelAnime, (AnimationHeader*)gMalonAdultIdleAnim);
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, thisx, &sCylinderInit);
    Actor_SetScale(thisx, 0.01f);
    thisx->targetMode = 6;
    thisx->gravity = -1.0f;
    thisx->room = -1;
    self->talking = false;
}

void Malon_Destroy(Actor* thisx, PlayState* play) {
    TravelMalon* self = (TravelMalon*)thisx;

    SkelAnime_Free(&self->skelAnime, play);
    Collider_DestroyCylinder(play, &self->collider);
}

void Malon_Choose(TravelMalon* self, PlayState* play) {
    u8 choice = play->msgCtx.choiceIndex;

    if (sPage == TEXT_MALON_MENU) {
        if (choice == 0) {
            sPage = TEXT_MALON_SCENES;
            Message_ContinueTextbox(play, TEXT_MALON_SCENES);
            return;
        }
        Message_CloseTextbox(play);
        self->talking = false;
        if (choice == 1) {
            GoTo(play, ENTR_GANONDORF_BOSS_0, 0xFFEF); // the Hero at the tower top
        }
        return;
    }
    Message_CloseTextbox(play);
    self->talking = false;
    switch (choice) {
        case 0:
            GoTo(play, ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN, 0xFFF0); // the opening nightmare
            break;
        case 1:
            GoTo(play, ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN, 0xFFF1); // the drawbridge escape
            break;
        default:
            GoTo(play, ENTR_CASTLE_COURTYARD_ZELDA_0, 0xFFEF); // Zelda's courtyard
            break;
    }
}

void Malon_Update(Actor* thisx, PlayState* play) {
    TravelMalon* self = (TravelMalon*)thisx;

    SkelAnime_Update(&self->skelAnime);
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 20.0f, 4); // (stands on the floor)
    Math_SmoothStepToS(&thisx->shape.rot.y, thisx->yawTowardsPlayer, 4, 0x600, 0x80);
    thisx->world.rot.y = thisx->shape.rot.y;
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 60.0f;

    if (self->talking) {
        if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
            Malon_Choose(self, play);
        } else if ((Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) ||
                   (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
            self->talking = false;
        }
    } else if (Actor_ProcessTalkRequest(thisx, play)) {
        self->talking = true;
        sPage = TEXT_MALON_MENU;
    } else {
        thisx->textId = TEXT_MALON_MENU;
        Actor_OfferTalk(thisx, play, 110.0f);
    }

    Collider_UpdateCylinder(thisx, &self->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &self->collider.base);
}


// In Link's house, a moment after arriving: a step in front of where the player came in
void SpawnInLinksHouse() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (play->sceneNum != SCENE_LINKS_HOUSE) || !MalonOn()) {
        return;
    }
    if (sSpawnedScene == play->sceneNum) {
        return;
    }
    if (++sWaitFrames < 3) {
        return;
    }
    if (sActorId < 0) {
        ActorDBInit init;
        init.name = "Gdf_TravelMalon";
        init.desc = "Play as Ganondorf: Malon, who takes you to the mod's scenes";
        init.category = ACTORCAT_NPC;
        init.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                     ACTOR_FLAG_DRAW_CULLING_DISABLED;
        init.objectId = OBJECT_MA2;
        init.instanceSize = sizeof(TravelMalon);
        init.init = Malon_Init;
        init.destroy = Malon_Destroy;
        init.update = Malon_Update;
        init.draw = Malon_Draw;
        sActorId = (s16)ActorDB::Instance->AddEntry(init).entry.id;
    }
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;
    f32 ahead = 100.0f;

    Actor_Spawn(&play->actorCtx, play, sActorId, player->actor.world.pos.x + Math_SinS(yaw) * ahead,
                player->actor.world.pos.y + 20.0f, player->actor.world.pos.z + Math_CosS(yaw) * ahead, 0,
                (s16)(yaw + 0x8000), 0, 0);
    sSpawnedScene = play->sceneNum;
}

void ShowText(const char* text) {
    CustomMessage msg = CustomMessage(text, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM);
    msg.Format();
    msg.LoadIntoFont();
}

void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if ((*textId != TEXT_MALON_MENU) && (*textId != TEXT_MALON_SCENES)) {
        return;
    }
    if (*textId == TEXT_MALON_MENU) {
        ShowText((std::string("Where to, Mister?&") + CustomMessage::THREE_WAY_CHOICE() +
                  "%gThe cutscenes&The Hero at the tower top&Nowhere, thanks%w")
                     .c_str());
    } else {
        ShowText((std::string("Which one?&") + CustomMessage::THREE_WAY_CHOICE() +
                  "%gThe opening nightmare&The drawbridge escape&Zelda's courtyard%w")
                     .c_str());
    }
    *loadFromMessageTable = false;
}

void RegisterTravelMalon() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sSpawnedScene = -1;
        sWaitFrames = 0;
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() { SpawnInLinksHouse(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
}

RegisterShipInitFunc initFunc(RegisterTravelMalon, {});

} // namespace

static void Malon_Draw(Actor* thisx, PlayState* play) {
    TravelMalon* self = (TravelMalon*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)gMalonAdultMouthHappyTex);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gMalonAdultEyeOpenTex);
    SkelAnime_DrawSkeletonOpa(play, &self->skelAnime, NULL, NULL, self);
    CLOSE_DISPS(play->state.gfxCtx);
}
