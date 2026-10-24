// SoH: "Play as Ganondorf", killing NPCs.
//
// With the option on, his attacks kill NPCs (z_boss_ganon.c). Each one killed is remembered by a key made of the
// scene, the actor, its parameters and its spawn position. The keys are saved in the save file ("ganondorfKills"
// section), and an NPC whose key is in the list is stopped from spawning. That is permanent: once the game is saved,
// there is no way back, even with the option turned off. Story characters included, so it can break the game.

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "GanondorfKills.h"

static std::unordered_set<uint32_t> sKilled;
// Each NPC's key from when it spawned (some move their home position later)
static std::unordered_map<Actor*, uint32_t> sSpawnKeys;

static uint32_t KeyOf(s16 scene, Actor* actor) {
    // FNV-1a over the things that stay the same every time the scene spawns this NPC
    s16 parts[6] = { scene,
                     (s16)actor->id,
                     (s16)actor->params,
                     (s16)(actor->home.pos.x / 10.0f),
                     (s16)(actor->home.pos.y / 10.0f),
                     (s16)(actor->home.pos.z / 10.0f) };
    uint32_t h = 2166136261u;

    for (s16 p : parts) {
        h = (h ^ (uint8_t)(p & 0xFF)) * 16777619u;
        h = (h ^ (uint8_t)((p >> 8) & 0xFF)) * 16777619u;
    }
    return h;
}

extern "C" int GanondorfKills_IsEnabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.KillNpcs"), 0);
}

extern "C" void GanondorfKills_Record(Actor* actor) {
    if ((actor == NULL) || (gPlayState == NULL)) {
        return;
    }
    auto it = sSpawnKeys.find(actor);

    if (it != sSpawnKeys.end()) {
        sKilled.insert(it->second);
        sSpawnKeys.erase(it);
    } else {
        sKilled.insert(KeyOf(gPlayState->sceneNum, actor));
    }
}

extern "C" int GanondorfKills_Count(void) {
    return (int)sKilled.size();
}

static void SaveKills(const SaveContext& saveContext, int sectionID, bool fullSave) {
    std::vector<uint32_t> keys(sKilled.begin(), sKilled.end());

    SaveManager::Instance->SaveData("count", (uint32_t)keys.size());
    SaveManager::Instance->SaveArray("keys", keys.size(),
                                     [&](size_t i) { SaveManager::Instance->SaveData("", keys[i]); });
}

static void LoadKills() {
    uint32_t count = 0;

    sKilled.clear();
    SaveManager::Instance->LoadData("count", count, (uint32_t)0);
    SaveManager::Instance->LoadArray("keys", count, [](size_t i) {
        uint32_t key = 0;

        SaveManager::Instance->LoadData("", key, (uint32_t)0);
        if (key != 0) {
            sKilled.insert(key);
        }
    });
}

static void RegisterGanondorfKills() {
    // Every file starts with nobody dead; loading a file then fills the list from its save
    SaveManager::Instance->AddInitFunction([](bool isDebug) { sKilled.clear(); });
    SaveManager::Instance->AddLoadFunction("ganondorfKills", 1, LoadKills);
    SaveManager::Instance->AddSaveFunction("ganondorfKills", 1, SaveKills, true, SECTION_PARENT_NONE);

    // The dead stay dead, whether or not the option is still on
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorInit>([](void* refActor, bool* result) {
        Actor* actor = (Actor*)refActor;

        uint32_t key;

        if ((gPlayState == NULL) || (actor->category != ACTORCAT_NPC)) {
            return;
        }
        key = KeyOf(gPlayState->sceneNum, actor);
        if (sKilled.count(key) != 0) {
            *result = false;
        } else {
            sSpawnKeys[actor] = key;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterGanondorfKills, {});
