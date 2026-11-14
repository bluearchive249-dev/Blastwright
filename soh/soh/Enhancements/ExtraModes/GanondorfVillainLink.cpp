// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// SoH: "Play as Ganondorf", cutscenes with the roles swapped: Link plays the villain's part (z_en_viewer.c draws him in
// Ganondorf's place). Here: what the villain says. Ganondorf's lines become Link's own: no words, just his shouts and
// grunts, in his voice.
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "GanondorfText.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
s32 GdfVillain_Active(PlayState* play);
}

static const struct {
    const char* text;
    u16 sfx;
} sVillainLines[] = {
    { "Hyah!", NA_SE_VO_LI_SWORD_N },
    { "Hup... %rHYAAAH!%w", NA_SE_VO_LI_SWORD_L },
    { "Hmph!", NA_SE_VO_LI_AUTO_JUMP },
    { "...Hah!", NA_SE_VO_LI_SWORD_N },
};
static s32 sVillainLine = 0;

static void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    PlayState* play = gPlayState;

    if ((play == NULL) || !GdfVillain_Active(play) || (play->csCtx.state == CS_STATE_IDLE)) {
        return;
    }
    // Every line in this cutscene is the rider's (Ganondorf's, now Link's)
    s32 i = sVillainLine++ % ARRAY_COUNT(sVillainLines);
    CustomMessage msg = CustomMessage(GdfFitBoxes(sVillainLines[i].text), TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM);
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
    Audio_PlaySfxGeneral(sVillainLines[i].sfx, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void OnSceneInit(int16_t) {
    sVillainLine = 0;
}

static void RegisterVillainLink() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
}

static RegisterShipInitFunc initFunc(RegisterVillainLink, {});
