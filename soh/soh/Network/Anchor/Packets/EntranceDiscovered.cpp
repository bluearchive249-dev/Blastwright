#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/ExtraModes/GanondorfInvasion.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"

/**
 * ENTRANCE_DISCOVERED
 */

void Anchor::SendPacket_EntranceDiscovered(u16 entranceIndex) {
    // Invasion mode: the Invader's own progress isn't shared (he only receives the Hero's)
    if (GdfInvasion_IsInvader()) {
        return;
    }
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = ENTRANCE_DISCOVERED;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["entranceIndex"] = entranceIndex;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_EntranceDiscovered(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    u16 entranceIndex = payload.at("entranceIndex").get<u16>();
    Entrance_SetEntranceDiscovered(entranceIndex, 1);
}
