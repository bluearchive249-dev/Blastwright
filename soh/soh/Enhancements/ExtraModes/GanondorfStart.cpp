// SoH: "Play as Ganondorf", experiment: start in the Gerudo Fortress.
//
// With the option on, a new file starts at the Gerudo Fortress instead of Link's house (no Deku Tree opening). A short
// narration and the Poe minion's greeting play when he first arrives. The file remembers that it's a Gerudo start (save
// section "ganondorfStart"): it continues at the fortress like a normal file continues at Link's house, and the Gerudo
// treat him as their King (no guards catching him).

#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "PlayAsGanondorf.h"
extern PlayState* gPlayState;
}

#include "GanondorfStart.h"
#include "GanondorfIntro.h"

#define GDF_START_TEXT_NARRATION 0x7E00
#define GDF_START_TEXT_POE 0x7E01

static bool sGerudoFile = false;  // this file started at the Gerudo Fortress
static bool sIntroShown = false;  // the arrival narration has played
static int sFramesInScene = 0;

static bool IsOptionOn() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.GerudoStart"), 0);
}

extern "C" int32_t GanondorfStart_IsGerudoFile(void) {
    return sGerudoFile && CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1);
}

extern "C" void GanondorfStart_InitNewSave(void) {
    sGerudoFile = false;
    sIntroShown = false;
    if (!IsOptionOn()) {
        return;
    }
    sGerudoFile = true;
    gSaveContext.entranceIndex = ENTR_GERUDOS_FORTRESS_EAST_EXIT; // in through the gate from the valley
    gSaveContext.cutsceneIndex = 0;                                // no Deku Tree opening
}

extern "C" int16_t GanondorfStart_GetContinueEntrance(int16_t entrance) {
    return GanondorfStart_IsGerudoFile() ? ENTR_GERUDOS_FORTRESS_EAST_EXIT : entrance;
}

// ---- Save section ----

static void SaveStart(const SaveContext& saveContext, int sectionID, bool fullSave) {
    SaveManager::Instance->SaveData("gerudo", (uint8_t)sGerudoFile);
    SaveManager::Instance->SaveData("introShown", (uint8_t)sIntroShown);
}

static void LoadStart() {
    uint8_t gerudo = 0;
    uint8_t shown = 0;

    SaveManager::Instance->LoadData("gerudo", gerudo, (uint8_t)0);
    SaveManager::Instance->LoadData("introShown", shown, (uint8_t)0);
    sGerudoFile = gerudo != 0;
    sIntroShown = shown != 0;
}

// ---- The arrival ----

static const char* sNarration =
    "Far to the west of Hyrule, beyond a deep valley, lies the fortress of the Gerudo, a people of thieves and "
    "warriors.^"
    "All Gerudo are women... save one. Once every hundred years a single man is born to them, and by their law he is "
    "to be their King.^"
    "The desert sun has risen over the fortress. Its King, the man the Gerudo call Lord @, has returned "
    "home.";

static const char* sPoeGreeting =
    "Master! Welcome home! Your Gerudo stand ready, and the desert is yours.^"
    "But you didn't come back for sand, did you? Hehehe... I hear the whispers too, Master.^"
    "A Sacred Realm, hidden away somewhere in Hyrule... and in it, the %rTriforce%w, the power of the gods!^"
    "Hyrule lies to the east, past the valley. Shall we go and take what's yours?";

static void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if (!GanondorfStart_IsGerudoFile()) {
        return;
    }
    if (*textId == GDF_START_TEXT_NARRATION) {
        CustomMessage msg = CustomMessage(sNarration);
        msg.SetTextBoxType(TEXTBOX_TYPE_BLACK);
        msg.SetTextBoxPosition(TEXTBOX_POS_MIDDLE);
        msg.AutoFormat();
        if (PlayAsGanondorf_UsePoeNavi()) {
            // On to the Poe's greeting (the "next message" code in place of the end)
            msg.Replace("\x02", std::string("\x07\x7E\x01", 3));
        }
        *loadFromMessageTable = false;
        msg.LoadIntoFont();
    } else if (*textId == GDF_START_TEXT_POE) {
        CustomMessage msg = CustomMessage(sPoeGreeting);
        msg.SetTextBoxType(TEXTBOX_TYPE_BLUE);
        msg.SetTextBoxPosition(TEXTBOX_POS_BOTTOM);
        msg.AutoFormat();
        *loadFromMessageTable = false;
        msg.LoadIntoFont();
    }
}

static void OnSceneInit(int16_t sceneNum) {
    sFramesInScene = 0;
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || !GanondorfStart_IsGerudoFile() || sIntroShown) {
        return;
    }
    sFramesInScene++;
    if ((play->sceneNum != SCENE_GERUDOS_FORTRESS) || (sFramesInScene < (CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.GerudoIntro"), 1) ? 1 : 30)) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->msgCtx.msgMode != MSGMODE_NONE)) {
        return;
    }
    sIntroShown = true;
    // His own intro cutscene (it ends with the Poe's greeting), or the short narration
    if (CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.GerudoIntro"), 1)) {
        GanondorfIntro_Start(true);
        return;
    }
    Message_StartTextbox(play, GDF_START_TEXT_NARRATION, NULL);
}

static void RegisterGanondorfStart() {
    SaveManager::Instance->AddInitFunction([](bool isDebug) {
        sGerudoFile = false;
        sIntroShown = false;
    });
    SaveManager::Instance->AddLoadFunction("ganondorfStart", 1, LoadStart);
    SaveManager::Instance->AddSaveFunction("ganondorfStart", 1, SaveStart, true, SECTION_PARENT_NONE);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);

    // The Gerudo serve their King: friendly from the start on a Gerudo file
    REGISTER_VB_SHOULD(VB_GERUDOS_BE_FRIENDLY, {
        if (GanondorfStart_IsGerudoFile()) {
            *should = true;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterGanondorfStart, {});
