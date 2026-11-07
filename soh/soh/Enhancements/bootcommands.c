#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "bootcommands.h"
#include "soh/cvar_prefixes.h"

void BootCommands_Init() {
    // Clears vars to prevent randomizer menu from being disabled
    CVarClear(CVAR_GENERAL("NewSeedGenerated"));
    CVarClear(CVAR_GENERAL("OnFileSelectNameEntry")); // Clear when soh is killed on the file name entry page
    CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQMode"));
    CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQModeScene"));
    // Play as Ganondorf's defaults: set on a fresh setup only (a choice already saved in the settings is kept)
    CVarRegisterInteger(CVAR_SETTING("A11yDisableIdleCam"), 1);                      // Disable Idle Camera Re-Centering
    CVarRegisterInteger(CVAR_SETTING("MatchRefreshRate"), 1);                        // Match Refresh Rate
    CVarRegisterInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.KeepItems"), 1);           // Keep C-Button Items
    CVarRegisterInteger(CVAR_ENHANCEMENT("TimeSavers.SkipForcedDialog"), 3);         // Skip Forced Dialog: All
    CVarRegisterInteger(CVAR_ENHANCEMENT("SkipText"), 1);                            // Skip Text (hold B)
#if defined(__SWITCH__) || defined(__WIIU__)
    CVarRegisterInteger(CVAR_IMGUI_CONTROLLER_NAV, 1); // always enable controller nav on switch/wii u
#endif
}
