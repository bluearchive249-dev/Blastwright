// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
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
extern PlayState* gPlayState;
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

// His own part of the conversation: a greeting before the NPC's line and a goodbye after it, written for that one
// conversation (Kokiri Forest so far; every other conversation takes a pair from its people's lines, see PoolFor)
struct NpcTalk {
    const char* greeting;
    const char* goodbye;
};
const std::unordered_map<uint16_t, NpcTalk> sNpcTalk = {
#include "GanondorfNpcTalk.inc"
};

// For every other conversation: greetings and goodbyes by the people he's speaking to (their attitude to him, and his
// to them), a pair picked by the line's id (so a conversation always has the same pair, and its neighbours others)
#include "GanondorfNpcPools.inc"

struct TalkPool {
    const char* const* greet;
    size_t numGreet;
    const char* const* bye;
    size_t numBye;
};
#define POOL(name) { s##name##Greet, sizeof(s##name##Greet) / sizeof(char*), s##name##Bye, sizeof(s##name##Bye) / sizeof(char*) }

const TalkPool* PoolFor(uint16_t textId) {
    static const TalkPool kokiri = POOL(Kokiri), ranch = POOL(Ranch), goron = POOL(Goron), zora = POOL(Zora),
                          kakariko = POOL(Kakariko), gerudo = POOL(Gerudo), hylian = POOL(Hylian);

    switch (textId >> 8) {
        case 0x10:
            return &kokiri;
        case 0x20:
            return &ranch;
        case 0x30:
            return &goron;
        case 0x40:
            return &zora;
        case 0x50:
            return &kakariko;
        case 0x60:
            return &gerudo;
        case 0x70:
        case 0x71:
            return &hylian;
        default:
            return nullptr;
    }
}

// Does the line end the talk as far as its text shows? (not one with a choice, a follow-on line, an event, a shop's
// or a timed box: his last word would come in the middle)
bool EndsPlainly(const std::string& text) {
    return text.find_first_of("\x07\x0A\x0B\x0E\x10\x1B\x1C") == std::string::npos;
}

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
    std::string text = it->second.text;
    auto talk = sNpcTalk.find(*textId);
    const TalkPool* pool = PoolFor(*textId);
    // (a new conversation: no text box is up yet; a line that follows another in the same talk gets nothing)
    bool opening = (gPlayState != nullptr) && (gPlayState->msgCtx.msgMode == MSGMODE_NONE);

    if ((talk == sNpcTalk.end()) && (pool != nullptr) && opening) {
        uint16_t id = *textId;
        size_t end = text.rfind('\x02');

        if (end != std::string::npos) {
            std::string body = text.substr(0, end);
            std::string out = std::string(pool->greet[(id * 7 + (id >> 4)) % pool->numGreet]) + "\x04" + body;

            if (EndsPlainly(body)) {
                out += std::string("\x04") + pool->bye[(id * 13 + 5) % pool->numBye];
            }
            text = out + text.substr(end);
        }
    } else if (talk != sNpcTalk.end()) {
        // his greeting as the first box, their line, then his goodbye as the last box (\x04 starts a new box, \x02
        // ends the message)
        size_t end = text.rfind('\x02');
        if (end != std::string::npos) {
            text = std::string(talk->second.greeting) + "\x04" + text.substr(0, end) + "\x04" + talk->second.goodbye +
                   text.substr(end);
        }
    }
    CustomMessage msg = CustomMessage(text, (TextBoxType)it->second.type, (TextBoxPosition)it->second.position);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void RegisterNpcs() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
}

} // namespace

static RegisterShipInitFunc initFunc(RegisterNpcs, {});
