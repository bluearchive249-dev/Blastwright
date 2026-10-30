// Ganondorf: Invasion, the multiplayer edition of "Play as Ganondorf".
//
// The online Invasion mode (up to six Links against one Ganondorf, over Anchor) with Ganondorf's full moveset and
// fixes, and nothing else: the single-player extras (the Gerudo start and intro, Hyrule Castle's interior, the guards'
// fear and hunt, killing people, the song player, his bodyguards, the title screen and logo) and the game's other extra
// modes (Rupee Dash, Shadow Tag, traps, the enemy randomizer, ...) are off, and out of the menu. Invasion mode is on.

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

static void ApplyInvasionEdition() {
    static const char* sOff[] = {
        CVAR_ENHANCEMENT("PlayAsGanondorf.GerudoStart"),    CVAR_ENHANCEMENT("PlayAsGanondorf.GerudoIntro"),
        CVAR_ENHANCEMENT("PlayAsGanondorf.CastleInterior"), CVAR_ENHANCEMENT("PlayAsGanondorf.GuardsFear"),
        CVAR_ENHANCEMENT("PlayAsGanondorf.KillNpcs"),       CVAR_ENHANCEMENT("PlayAsGanondorf.Songs.Enabled"),
        CVAR_ENHANCEMENT("PlayAsGanondorf.Bodyguards"),     CVAR_ENHANCEMENT("PlayAsGanondorf.TitleScreen"),
        CVAR_ENHANCEMENT("PlayAsGanondorf.TitleLogo"),      CVAR_ENHANCEMENT("RupeeDash"),
        CVAR_ENHANCEMENT("ShadowTag"),                      CVAR_ENHANCEMENT("HurtContainer"),
        CVAR_ENHANCEMENT("ExtraTraps.Enabled"),             CVAR_ENHANCEMENT("RandomizedEnemies"),
        CVAR_ENHANCEMENT("RandomizedEnemySizes"),           CVAR_ENHANCEMENT("MirroredWorldMode"),
        CVAR_ENHANCEMENT("BounceOffWalls"),                 CVAR_ENHANCEMENT("IvanCoopModeEnabled"),
        CVAR_ENHANCEMENT("DogFollowsEverywhere"),
    };

    for (const char* cvar : sOff) {
        if (CVarGetInteger(cvar, 0) != 0) {
            CVarSetInteger(cvar, 0);
        }
    }
    CVarSetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Invasion.Enabled"), 1);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

static RegisterShipInitFunc initFunc(ApplyInvasionEdition, {});
