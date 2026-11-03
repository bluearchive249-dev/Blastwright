#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/ExtraModes/GanondorfInvasion.h"
#include "soh/Notification/Notification.h"
#include "soh/ShipUtils.h"

const std::string gameCompleteMessages[] = {
    "killed Ganon",           "saved Zelda",         "proved their Courage",
    "collected the Triforce", "is the Hero of Time", "proved Mido wrong",
};

/**
 * GAME_COMPLETE
 */

void Anchor::SendPacket_GameComplete() {
    // Invasion mode: the Invader's own progress isn't shared (he only receives the Hero's)
    if (GdfInvasion_IsInvader()) {
        return;
    }
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = GAME_COMPLETE;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_GameComplete(nlohmann::json payload) {
    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    if (!clients.contains(clientId)) {
        return;
    }

    AnchorClient& anchorClient = clients[clientId];
    anchorClient.isGameComplete = true;
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    Notification::Emit({
        .prefix = isGlobalRoom ? "Someone" : anchorClient.name,
        .message = ShipUtils::RandomElement(gameCompleteMessages),
    });
}
