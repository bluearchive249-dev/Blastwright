#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/ExtraModes/GanondorfInvasion.h"

extern "C" {
#include "macros.h"
#include "functions.h"
extern PlayState* gPlayState;
void func_80838280(Player* player);
}

/**
 * DAMAGE_PLAYER
 */

void Anchor::SendPacket_DamagePlayer(u32 clientId, u8 damageEffect, u8 damage) {
    if (GdfInvasion_IsInvader() && !GdfInvasion_CanHurtLink(clientId)) {
        return;
    }
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = DAMAGE_PLAYER;
    payload["targetClientId"] = clientId;
    payload["damageEffect"] = damageEffect;
    payload["damage"] = damage;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_DamagePlayer(nlohmann::json payload) {
    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    // Invasion mode: Ganondorf's hits only after the truce (they cost him magic), and the Links' hits on Ganondorf go
    // to his Resolve, not his hearts
    if (GdfInvasion_IsInvader() || (GdfInvasion_IsClientGanondorf(clientId) && GdfInvasion_LinkAtPeace(0))) {
        return;
    }
    if (!clients.contains(clientId) || clients[clientId].player == nullptr) {
        return;
    }

    AnchorClient& anchorClient = clients[clientId];
    Player* otherPlayer = anchorClient.player;
    Player* self = GET_PLAYER(gPlayState);

    // Prevent incoming damage during cutscenes or item get sequences
    if (Player_InBlockingCsMode(gPlayState, self) || self->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS ||
        self->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) {
        return;
    }

    u8 damageEffect = payload.at("damageEffect").get<u8>();
    u8 damage = payload.at("damage").get<u8>();

    self->actor.colChkInfo.damage = damage * 8; // Arbitrary number currently, need to fine tune

    // Invasion mode: Ganondorf hits a Link with fewer heart containers less hard (full force at 20 hearts), and no
    // single hit of his (the dark aura included) takes more than a third of his hearts
    if (GdfInvasion_IsClientGanondorf(clientId)) {
        s32 dmg = (s32)(damage * 8) * gSaveContext.healthCapacity / (20 * 16);
        dmg = CLAMP(dmg, 4, MAX(gSaveContext.healthCapacity / 3, 4));
        self->actor.colChkInfo.damage = (u8)MIN(dmg, 255);
    }

    if (damageEffect == DUMMY_PLAYER_HIT_RESPONSE_FIRE) {
        for (int i = 0; i < ARRAY_COUNT(self->bodyFlameTimers); i++) {
            self->bodyFlameTimers[i] = static_cast<uint8_t>(Rand_S16Offset(0, 200));
        }
        self->bodyIsBurning = true;
    } else if (damageEffect == DUMMY_PLAYER_HIT_RESPONSE_STUN) {
        self->actor.freezeTimer = 20;
        Actor_SetColorFilter(&self->actor, 0, 0xFF, 0, 24);
        return;
    }

    func_80837C0C(gPlayState, self, damageEffect, 4.0f, 5.0f,
                  Actor_WorldYawTowardActor(&otherPlayer->actor, &self->actor), 20);
}
