// Play as Ganondorf: what the people of Hyrule say to him. Hylians fear him, the Gerudo respect their king, the Gorons
// and Zoras hate the man who cursed their homes, the Kokiri and the Sheikah are wary of him or stand up to him. Each
// line replaces the game's own text with the same id; its sounds, colours and follow-on lines are kept, and every
// hint or direction it gave is still in it. English only (other languages keep the game's text).
#include <string>
#include <unordered_map>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "variables.h"
}

namespace {

struct NpcLine {
    uint8_t type;     // the box, as the game's message table has it
    uint8_t position;
    std::string text;
};

const std::unordered_map<uint16_t, NpcLine> sNpcLines = {
#include "GanondorfNpcLines.inc"
};

void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) ||
        !CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.NpcDialogue"), 1) ||
        (gSaveContext.language != LANGUAGE_ENG) || !*loadFromMessageTable) {
        return; // (another of his hooks already gave this line its own text)
    }
    auto it = sNpcLines.find(*textId);
    if (it == sNpcLines.end()) {
        return;
    }
    CustomMessage msg =
        CustomMessage(it->second.text, (TextBoxType)it->second.type, (TextBoxPosition)it->second.position);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void RegisterNpcs() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
}

} // namespace

static RegisterShipInitFunc initFunc(RegisterNpcs, {});
