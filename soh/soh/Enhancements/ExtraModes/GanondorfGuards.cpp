// SoH: "Play as Ganondorf", the guards of Hyrule fear him.
//
// The soldiers in the castle grounds don't catch the King of Evil and throw him out: when one spots him he freezes,
// turns to face him, trembles and says something fitting, and from then on keeps out of his way
// (EnHeishi1/EnHeishi3 call GanondorfGuards_Cower). The town and gate guards' lines are rewritten to match.
// While he's wanted for murder in Castle Town (the guard hunt) they still fight him instead.

#include <string>

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
#include "PlayAsGanondorf.h"
s32 BossGanon_Player_IsWanted(void);
extern PlayState* gPlayState;
}

#define GDF_TEXT_COWER_FIRST 0x7E40
#define GDF_COWER_LINES 6

static const char* sCowerLines[GDF_COWER_LINES] = {
    "W-wait... that armor... You're the Gerudo King!&S-sir! I didn't see anything!",
    "Th-the King of Thieves?! In the castle?!&Nobody pays me enough for this...",
    "Ah! P-please, go right ahead, my lord!&I'm just... inspecting this hedge.",
    "D-don't look at me like that!&I'll pretend you're not here, I swear!",
    "Stop! Who goes th-- oh no.&Oh no, no, no...",
    "Y-you're bigger up close...&I'll... I'll just stand very still.",
};

// The other guards' lines, as they'd say them to him
struct GuardLine {
    u16 id;
    const char* text;
};

static const GuardLine sGuardLines[] = {
    // The castle gate guard on the road up from town
    { 0x7006,
      "Th-the castle is closed, my lord! Even a dog c-can't...^...that is, the gate is chained shut, I couldn't "
      "open it if I wanted to!^And I definitely never noticed those vines on the wall over there. Nope." },
    // Thrown out after being caught (shouldn't happen now, just in case)
    { 0x7029, "Y-you want to see Princess Zelda?^...The guards on duty today are all very sick. Very, very sick. Off "
              "you go, my lord!" },
    // The town guards
    { 0x7002, "W-welcome to Hyrule Castle Town, my lord...^It's a peaceful, prosperous town.^Please let it stay that "
              "way." },
    { 0x7003, "It's not safe in town at night...&...f-for everyone else, I mean. Not for you. Obviously." },
    { 0x7004, "Man, I am SO bored...^Things would sure be more interesting if there were more... troubles in the "
              "world...^...I take it back. I take it all back. Please don't look at me." },
    { 0x709A, "The night watch is so boring!&I wish something scary would come out...^...and something did. Hi."
              "&I'll be over here." },
};

static u8 sNextCower = 0;

extern "C" s32 GanondorfGuards_Fear(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.GuardsFear"), 1) && !BossGanon_Player_IsWanted();
}

// A guard has spotted him: a line, never the same twice in a row
extern "C" void GanondorfGuards_Cower(PlayState* play, Actor* guard) {
    if (play->msgCtx.msgMode == MSGMODE_NONE) {
        Message_StartTextbox(play, GDF_TEXT_COWER_FIRST + sNextCower, NULL);
    }
    sNextCower = (sNextCower + 1 + (u8)(Rand_ZeroOne() * (GDF_COWER_LINES - 2))) % GDF_COWER_LINES;
}

// Facing him and trembling, where the guard stands
extern "C" void GanondorfGuards_Tremble(PlayState* play, Actor* guard) {
    Math_SmoothStepToS(&guard->shape.rot.y, guard->yawTowardsPlayer, 4, 0x800, 0x80);
    guard->world.rot.y = guard->shape.rot.y;
    guard->shape.rot.z = (s16)((Rand_ZeroOne() - 0.5f) * 500.0f);
    guard->speedXZ = 0.0f;
}

static void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    const char* text = NULL;

    if (!CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) ||
        !CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.GuardsFear"), 1)) {
        return;
    }
    if ((*textId >= GDF_TEXT_COWER_FIRST) && (*textId < GDF_TEXT_COWER_FIRST + GDF_COWER_LINES)) {
        text = sCowerLines[*textId - GDF_TEXT_COWER_FIRST];
    } else {
        for (const GuardLine& line : sGuardLines) {
            if (line.id == *textId) {
                text = line.text;
                break;
            }
        }
    }
    if (text == NULL) {
        return;
    }
    CustomMessage msg = CustomMessage(text);
    msg.SetTextBoxType(TEXTBOX_TYPE_BLACK);
    msg.SetTextBoxPosition(TEXTBOX_POS_BOTTOM);
    msg.AutoFormat();
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

static void RegisterGanondorfGuards() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfGuards, {});
