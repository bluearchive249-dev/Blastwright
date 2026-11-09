// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// SoH: "Play as Ganondorf", Invasion mode.
//
// Online co-op over Anchor (Network > Anchor) where one player is the Hero, Link, who plays the game, and the other the
// Invader, Ganondorf, whose job is to make the Hero's playthrough harder. The role is chosen in the Anchor menu.
//
// This first part:
//  - Roles. The Invader plays as Ganondorf (and starts a new file in Kokiri Forest like Link, not at the Gerudo
//    Fortress); the Hero plays as Link (Play as Ganondorf off).
//  - On the Hero's screen the Invader is Ganondorf, not a Link: his pose (skeleton, sword) goes along with Anchor's
//    PLAYER_UPDATE packets, and a puppet Ganondorf (ovl_Boss_Ganon, GDF_PARAMS_PUPPET) is posed from it, cape and all.
//  - Ganondorf can't hurt Link in any way: no PvP damage from him, his attacks pass through Link's dummy.
//  - The Invader's own progress (flags, items, checks) isn't shared with the team; he still receives the Hero's.
//  - The truce: nothing he does can touch Link until Link has set foot in the Deku Tree (the adventure starts).
//  - A small HUD for the Invader: where the Hero is (his area, and an arrow toward him in the same area).
// See the project doc "PlayAsGanondorf-InvasionMode" for the rest of the plan (Resolve, the Dark Arts menu, ...).

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <vector>
#include <string>

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/bridge/controllerbridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/GuiWindow.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/util.h"
#include "soh/Notification/Notification.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "src/overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h"
#include "src/overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "src/overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "src/overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"
#include "src/overlays/actors/ovl_En_Fire_Rock/z_en_fire_rock.h"
#include "src/overlays/actors/ovl_Bg_Hidan_Curtain/z_bg_hidan_curtain.h"
#include "src/overlays/actors/ovl_En_Ik/z_en_ik.h"
#include "src/overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
#include "src/overlays/actors/ovl_En_Firefly/z_en_firefly.h"
#include "src/overlays/actors/ovl_En_Test/z_en_test.h"
extern PlayState* gPlayState;
}

#include "GanondorfInvasion.h"
#include "GanondorfArtIcons.h"
#include <fast/Fast3dGui.h>

#define CVAR_INVASION(var) CVAR_ENHANCEMENT("PlayAsGanondorf.Invasion." var)

// ---- Roles ----

static bool AnchorOn() {
    return (Anchor::Instance != nullptr) && Anchor::Instance->isEnabled;
}

// Dev test mode (Extra Modes > Ganondorf): alone, offline, he's Ganondorf and the Link his arts land on at once
static bool TestModeOn() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.InvasionTest"), 0) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           !(AnchorOn() && CVarGetInteger(CVAR_INVASION("Enabled"), 0));
}

extern "C" s32 GdfInvasion_TestMode(void) {
    return TestModeOn();
}

// In his own game nothing but the Links can touch him (not in test mode, where his arts' monsters are for him)
extern "C" s32 GdfInvasion_ShieldedFromWorld(void) {
    return GdfInvasion_IsInvader() && !TestModeOn();
}

// The role is set when the host starts the game from the lobby (from the seat each player took)
extern "C" s32 GdfInvasion_Role(void) {
    if (TestModeOn()) {
        return GDF_INVASION_INVADER;
    }
    if (!AnchorOn() || !CVarGetInteger(CVAR_INVASION("Enabled"), 0)) {
        return GDF_INVASION_OFF;
    }
    return CVarGetInteger(CVAR_INVASION("Role"), GDF_INVASION_OFF);
}

extern "C" s32 GdfInvasion_IsInvader(void) {
    return GdfInvasion_Role() == GDF_INVASION_INVADER;
}

extern "C" s32 GdfInvasion_IsHero(void) {
    return (GdfInvasion_Role() == GDF_INVASION_HERO) || TestModeOn();
}

// No truce: only Kokiri Forest is at peace, for a young Link who has never yet had both a sword and a shield on.
// Once he has, it's gone for good (losing the shield doesn't bring it back). Each Link remembers it for himself
// (reset with a new game) and sends it with his pose.
extern "C" s32 GdfInvasion_AdventureStarted(void) {
    return true;
}

static bool IsKokiriScene(s16 scene) {
    switch (scene) {
        case SCENE_KOKIRI_FOREST:
        case SCENE_LINKS_HOUSE:
        case SCENE_KNOW_IT_ALL_BROS_HOUSE:
        case SCENE_TWINS_HOUSE:
        case SCENE_MIDOS_HOUSE:
        case SCENE_SARIAS_HOUSE:
        case SCENE_KOKIRI_SHOP:
            return true;
    }
    return false;
}

static bool IsSword(s32 item) {
    return (item == ITEM_SWORD_KOKIRI) || (item == ITEM_SWORD_MASTER) || (item == ITEM_SWORD_BGS) ||
           (item == ITEM_SWORD_KNIFE);
}

static bool OwnArmedOnce() {
    return CVarGetInteger(CVAR_INVASION("Armed"), 0) != 0;
}

static bool sClientArmed[256];
static bool ClientArmedOnce(uint32_t clientId);

// This Link (0: the local one) is in peaceful Kokiri Forest
extern "C" s32 GdfInvasion_LinkAtPeace(uint32_t clientId) {
    if ((Anchor::Instance == nullptr) || TestModeOn()) {
        return false;
    }
    bool self = (clientId == 0) || (Anchor::Instance->clients.count(clientId) && Anchor::Instance->clients[clientId].self);
    if (self) {
        return GdfInvasion_IsHero() && !OwnArmedOnce() && (gPlayState != nullptr) &&
               IsKokiriScene(gPlayState->sceneNum) && (gSaveContext.linkAge == LINK_AGE_CHILD);
    }
    if (!Anchor::Instance->clients.count(clientId)) {
        return false;
    }
    auto& client = Anchor::Instance->clients[clientId];
    return !ClientArmedOnce(clientId) && IsKokiriScene(client.sceneNum) && (client.linkAge == LINK_AGE_CHILD);
}

// ---- Ganondorf's pose over the network ----

#define GDF_NET_RESOLVE_FULL 120.0f // (GDF_RESOLVE_MAX)
struct ClientGdf {
    GdfNetPose pose = {};
    u32 lastFrame = 0; // when the last pose came in
    bool isGanondorf = false;
    Actor* puppet = nullptr;
    GdfNetGanon ganon = {}; // his Ganon form, while he's in it
    u32 ganonFrame = 0;
    Actor* ganonPuppet = nullptr;
    float resolve = -1.0f; // his Resolve (and how deep it is), for the Links' HUD
    float resolveMax = 0.0f;
    u32 hurtFrame = 0;
    int downLeft = 0; // a fallen Link: seconds left to revive him
    u32 downFrame = 0;
    int reviveProgress = 0; // this Link standing by him
};

static std::map<uint32_t, ClientGdf> sClients;
static u32 sFrame = 0;
#define GDF_DOWN_SECONDS 30
#define GDF_REVIVE_SECONDS 3
#define GDF_PKT_REVIVE "GDF_REVIVE"
static u32 sDownedSince = 0;      // this Link: when he fell
static u32 sDownSeen = 0;         // the last frame he lay there
static bool sReviveCalled = false; // another Link has revived him
static bool sRevivedByAlly = false;
static bool IsDowned() {
    return (sDownSeen != 0) && (sFrame - sDownSeen <= 5);
}
static u32 sVeilUntil = 0; // Shadow Veil (Ganondorf): unseen and unheard by the Links
static u32 sPacifiedFrame = 0;     // Ganondorf: when he was pacified
static u32 sHeroPacifiedFrame = 0; // a Link: when word came that he was
static bool GanonFresh(const ClientGdf& c) {
    return c.ganon.valid && (sFrame - c.ganonFrame < 10);
}
// Dark Link: Ganondorf drives him in the Link's game for a minute (his own Ganondorf stands still meanwhile)
static u32 sPossessStart = 0;
static u32 sPossessUntil = 0;
static s16 sPossessCam = -1;
static s16 sPossessYaw = 0;  // the way his camera looks
static uint32_t sPossessTarget = 0; // the Link whose game Dark Link is in
static s16 sDlYaw = 0;       // his stick, as a direction in the world
static s16 sDlTilt = 0;
static u16 sDlButtons = 0;
// The Link's side: what Ganondorf last sent for his Dark Link
static Actor* sDarkLink = nullptr;
static u32 sDarkLinkUntil = 0;
static u32 sDlHeard = 0;
static Actor* GhostDarkLink();

// What Ganondorf's Dark Arts made in a Link's game, sent with that Link's pose so Ganondorf sees it too
struct Mirrored {
    Actor* actor = nullptr;
    int key = 0;
};
static Mirrored sMirrored[40];
static int sMirrorKey = 0;
static void Mirror(Actor* a) {
    if (a == nullptr) {
        return;
    }
    for (Mirrored& m : sMirrored) {
        if (m.actor == nullptr) {
            m.actor = a;
            m.key = ++sMirrorKey;
            return;
        }
    }
}
static void WriteMirror(nlohmann::json& payload);
extern "C" s32 BossGanon_Player_IsWanted(void);
static bool sGanonWanted = false; // (a Link's game) Ganondorf is wanted in this market: the bell rings here too
static u32 sGanonWantedHeard = 0;
static bool InSameScene(uint32_t clientId) {
    return (gPlayState != nullptr) && (Anchor::Instance != nullptr) && Anchor::Instance->clients.count(clientId) &&
           (Anchor::Instance->clients[clientId].sceneNum == gPlayState->sceneNum);
}
static void ReadMirror(uint32_t from, const nlohmann::json& list);
// The shared world: the host Link's monsters and NPCs, for everyone in his area
static bool IsWorldHost();
static void WriteWorld(nlohmann::json& payload);
static void ReadWorld(uint32_t from, const nlohmann::json& world);
static void NoteHostClaim(uint32_t clientId, const nlohmann::json& world);

static void WriteGrab(nlohmann::json& g);
static void ReadGrab(uint32_t from, const nlohmann::json& g);
bool GdfInvasion_IsHeld();

// Sounds his Ganondorf made since the last packet went out (with the frame, so sounds nobody was around to hear
// aren't all played at once when someone walks in)
static std::vector<std::pair<u16, u32>> sSfxQueue;

extern "C" void GdfInvasion_NoteSfx(u16 sfxId) {
    if (!GdfInvasion_IsInvader() || (sFrame < sVeilUntil)) {
        return; // not heard while veiled
    }
    std::erase_if(sSfxQueue, [](const std::pair<u16, u32>& q) { return sFrame - q.second > 3; });
    for (auto& q : sSfxQueue) {
        if (q.first == sfxId) {
            return; // the same sound twice in a frame (a looping one) is one sound
        }
    }
    if (sSfxQueue.size() < GDF_NET_SFX) {
        sSfxQueue.push_back({ sfxId, sFrame });
    }
}

void GdfInvasion_WritePose(nlohmann::json& payload) {
    GdfNetPose pose;

    if (GdfInvasion_IsHero()) {
        payload["gArmed"] = OwnArmedOnce();
        if (IsDowned()) {
            payload["gDown"] = MAX(1, (int)(GDF_DOWN_SECONDS - (sFrame - sDownedSince) / 20)); // (20 frames a second)
        }
        WriteMirror(payload);
        if (IsWorldHost()) {
            WriteWorld(payload);
        }
        return;
    }

    if (!GdfInvasion_IsInvader()) {
        return;
    }
    payload["gdfRole"] = GDF_INVASION_INVADER;
    WriteMirror(payload); // his hunting guards
    if (BossGanon_Player_IsWanted()) {
        payload["gwant"] = true;
    }
    if (sFrame < sPossessUntil) {
        payload["dl"] = nlohmann::json::array({ (int)sDlYaw, (int)sDlTilt, (int)sDlButtons });
    }
    // His Ganon form
    GdfNetGanon ganon;
    if (BossGanon2_Player_GetNetPose(&ganon)) {
        std::vector<int> j;
        j.reserve(ganon.jointCount * 3);
        for (int i = 0; i < ganon.jointCount; i++) {
            j.insert(j.end(), { ganon.joints[i].x, ganon.joints[i].y, ganon.joints[i].z });
        }
        payload["gn"] = { { "f", ganon.form },
                          { "h", ganon.hidden || (sFrame < sVeilUntil) },
                          { "w", ganon.swords },
                          { "e", ganon.eye },
                          { "p", { ganon.pos.x, ganon.pos.y, ganon.pos.z } },
                          { "r", ganon.rotY },
                          { "y", ganon.yOffset },
                          { "g", ganon.glow },
                          { "j", j } };
    }
    if (!BossGanon_Player_GetNetPose(&pose)) {
        return;
    }
    if (sFrame < sVeilUntil) {
        pose.hidden = true; // Shadow Veil
        pose.ballCount = 0;
    }
    nlohmann::json g;
    g["h"] = pose.hidden;
    g["s"] = pose.swordState;
    g["p"] = { pose.pos.x, pose.pos.y, pose.pos.z };
    g["r"] = pose.rotY;
    g["f"] = pose.floorY;
    std::vector<int> joints;
    joints.reserve(GDF_NET_JOINTS * 3);
    for (int i = 0; i < GDF_NET_JOINTS; i++) {
        joints.push_back(pose.joints[i].x);
        joints.push_back(pose.joints[i].y);
        joints.push_back(pose.joints[i].z);
    }
    g["j"] = joints;
    // His cape, glows, aura and organ
    std::vector<float> fx(pose.fx, pose.fx + pose.fxCount);
    g["x"] = fx;
    // His light balls and Big Magic in flight: kind, position, rotation, scale, alpha, floor
    std::vector<float> balls;
    for (int i = 0; i < pose.ballCount; i++) {
        const GdfNetBall& b = pose.balls[i];
        balls.insert(balls.end(), { (float)b.kind, b.pos.x, b.pos.y, b.pos.z, (float)b.rot.x, (float)b.rot.y,
                                    (float)b.rot.z, b.scale, b.fwork0, b.fwork1, b.floorY });
    }
    g["b"] = balls;
    // The sounds he made
    std::vector<int> sfx;
    for (auto& q : sSfxQueue) {
        if (sFrame - q.second <= 3) {
            sfx.push_back(q.first);
        }
    }
    g["s2"] = sfx;
    // His Resolve, shown to the Links
    g["rv"] = GdfInvasion_ResolveFraction() * GDF_NET_RESOLVE_FULL;
    g["rm"] = GdfInvasion_ResolveMeterLength() * GDF_NET_RESOLVE_FULL;
    sSfxQueue.clear();
    WriteGrab(g);
    payload["gdf"] = g;
}

void GdfInvasion_ReadPose(uint32_t clientId, const nlohmann::json& payload) {
    ClientGdf& c = sClients[clientId];

    if (payload.contains("gDown")) {
        c.downLeft = payload.value("gDown", 0);
        c.downFrame = sFrame;
    } else {
        c.downLeft = 0;
    }
    if (payload.contains("gArmed") && (clientId < 256)) {
        sClientArmed[clientId] |= payload.value("gArmed", false); // once armed, for good
    }
    if (payload.contains("gm") && InSameScene(clientId) &&
        (GdfInvasion_IsInvader() || (GdfInvasion_IsHero() && (payload.value("gdfRole", 0) == GDF_INVASION_INVADER)))) {
        ReadMirror(clientId, payload["gm"]); // (a Link's: his arts' monsters; Ganondorf's: his hunting guards)
    }
    if (payload.value("gdfRole", 0) == GDF_INVASION_INVADER) {
        sGanonWanted = payload.value("gwant", false) && InSameScene(clientId);
        sGanonWantedHeard = sFrame;
    }
    if (payload.contains("gw")) {
        NoteHostClaim(clientId, payload["gw"]);
    }
    if (payload.contains("gw") && !IsWorldHost()) {
        ReadWorld(clientId, payload["gw"]);
    }

    c.isGanondorf = payload.value("gdfRole", 0) == GDF_INVASION_INVADER;
    if (c.isGanondorf && payload.contains("dl") && payload["dl"].is_array() && (payload["dl"].size() >= 3)) {
        sDlYaw = (s16)payload["dl"][0].get<int>();
        sDlTilt = (s16)payload["dl"][1].get<int>();
        sDlButtons = (u16)payload["dl"][2].get<int>();
        sDlHeard = sFrame;
    }
    if (c.isGanondorf && payload.contains("gn")) {
        try {
            const nlohmann::json& n = payload["gn"];
            std::vector<float> p = n.value("p", std::vector<float>{ 0.0f, 0.0f, 0.0f });
            std::vector<int> j = n.value("j", std::vector<int>{});
            p.resize(3);
            c.ganon.form = (u8)n.value("f", 0);
            c.ganon.hidden = n.value("h", false) ? 1 : 0;
            c.ganon.swords = (u8)n.value("w", 0);
            c.ganon.eye = (u8)n.value("e", 0);
            c.ganon.pos = { p[0], p[1], p[2] };
            c.ganon.rotY = (s16)n.value("r", 0);
            c.ganon.yOffset = n.value("y", 0.0f);
            c.ganon.glow = n.value("g", 0.0f);
            c.ganon.jointCount = (u8)MIN((int)(j.size() / 3), GDF_NET_GANON_JOINTS);
            for (int i = 0; i < c.ganon.jointCount; i++) {
                c.ganon.joints[i] = { (s16)j[i * 3], (s16)j[i * 3 + 1], (s16)j[i * 3 + 2] };
            }
            c.ganon.valid = true;
            c.ganonFrame = sFrame;
        } catch (...) {
            c.ganon.valid = false;
        }
    }
    if (!c.isGanondorf || !payload.contains("gdf")) {
        c.pose.valid = false;
        return;
    }
    const nlohmann::json& g = payload["gdf"];
    try {
        std::vector<float> p = g.value("p", std::vector<float>{ 0.0f, 0.0f, 0.0f });
        std::vector<int> j = g.value("j", std::vector<int>{});

        p.resize(3);
        j.resize(GDF_NET_JOINTS * 3);
        c.pose.hidden = g.value("h", 0);
        c.pose.swordState = g.value("s", 0);
        c.pose.pos = { p[0], p[1], p[2] };
        c.pose.rotY = (s16)g.value("r", 0);
        c.pose.floorY = g.value("f", p[1]);
        for (int i = 0; i < GDF_NET_JOINTS; i++) {
            c.pose.joints[i] = { (s16)j[i * 3], (s16)j[i * 3 + 1], (s16)j[i * 3 + 2] };
        }
        std::vector<float> fx = g.value("x", std::vector<float>{});
        c.pose.fxCount = (u8)MIN((int)fx.size(), GDF_NET_FX);
        for (int i = 0; i < c.pose.fxCount; i++) {
            c.pose.fx[i] = fx[i];
        }
        std::vector<float> balls = g.value("b", std::vector<float>{});
        c.pose.ballCount = (u8)MIN((int)(balls.size() / 11), GDF_NET_BALLS);
        for (int i = 0; i < c.pose.ballCount; i++) {
            const float* v = &balls[i * 11];
            GdfNetBall& b = c.pose.balls[i];
            b.kind = (u8)v[0];
            b.pos = { v[1], v[2], v[3] };
            b.rot = { (s16)v[4], (s16)v[5], (s16)v[6] };
            b.scale = v[7];
            b.fwork0 = v[8];
            b.fwork1 = v[9];
            b.floorY = v[10];
        }
        c.pose.valid = true;
        c.lastFrame = sFrame;
        {
            float r = g.value("rv", -1.0f);
            if ((c.resolve >= 0.0f) && (r >= 0.0f) && (r < c.resolve - 0.01f)) {
                c.hurtFrame = sFrame; // he's just been hurt: his bar shows over him
            }
            c.resolve = r;
        }
        c.resolveMax = g.value("rm", 0.0f);
        // Whether he has this player's Link by the throat
        ReadGrab(clientId, g);
        // His sounds, from his puppet
        if (c.puppet != nullptr) {
            for (int sfx : g.value("s2", std::vector<int>{})) {
                Audio_PlayActorSound2(c.puppet, (u16)sfx);
            }
        }
    } catch (...) {
        c.pose.valid = false;
    }
}

bool GdfInvasion_IsClientGanondorf(uint32_t clientId) {
    auto it = sClients.find(clientId);
    return (it != sClients.end()) && it->second.isGanondorf;
}

// From the Anchor dummy of each other player, every frame: when that player is Ganondorf, his puppet stands in for the
// dummy Link (spawned here when needed, removed when he's gone)
bool GdfInvasion_UpdateDummy(Actor* dummy, uint32_t clientId, bool inScene) {
    PlayState* play = gPlayState;
    auto it = sClients.find(clientId);

    if ((it == sClients.end()) || !it->second.isGanondorf || (play == nullptr)) {
        return false;
    }
    ClientGdf& c = it->second;
    bool fresh = c.pose.valid && (sFrame - c.lastFrame < 60);

    // His Ganon form
    if (inScene && GanonFresh(c)) {
        if (c.ganonPuppet == nullptr) {
            c.ganonPuppet = Actor_Spawn(&play->actorCtx, play, ACTOR_BOSS_GANON2, c.ganon.pos.x, c.ganon.pos.y,
                                        c.ganon.pos.z, 0, c.ganon.rotY, 0, GANON2_PARAMS_PUPPET);
        }
    } else if (c.ganonPuppet != nullptr) {
        Actor_Kill(c.ganonPuppet);
        c.ganonPuppet = nullptr;
    }

    if (!inScene || !fresh) {
        if (c.puppet != nullptr) {
            Actor_Kill(c.puppet);
            c.puppet = nullptr;
        }
        return true;
    }
    if (c.puppet == nullptr) {
        c.puppet = Actor_Spawn(&play->actorCtx, play, ACTOR_BOSS_GANON, c.pose.pos.x, c.pose.pos.y, c.pose.pos.z, 0,
                               c.pose.rotY, 0, GDF_PARAMS_PUPPET);
    }
    return true;
}

extern "C" s32 GdfInvasion_GetPuppetPose(Actor* puppet, GdfNetPose* out) {
    for (auto& [clientId, c] : sClients) {
        if (c.puppet == puppet) {
            *out = c.pose;
            return c.pose.valid;
        }
    }
    return false;
}

extern "C" s32 GdfInvasion_GetGanonPose(Actor* puppet, GdfNetGanon* out) {
    for (auto& [clientId, c] : sClients) {
        if (c.ganonPuppet == puppet) {
            *out = c.ganon;
            return GanonFresh(c);
        }
    }
    return false;
}

extern "C" void GdfInvasion_GanonPuppetDestroyed(Actor* puppet) {
    for (auto& [clientId, c] : sClients) {
        if (c.ganonPuppet == puppet) {
            c.ganonPuppet = nullptr;
        }
    }
}

extern "C" void GdfInvasion_PuppetDestroyed(Actor* puppet) {
    for (auto& [clientId, c] : sClients) {
        if (c.puppet == puppet) {
            c.puppet = nullptr;
        }
    }
}

// ---- Each frame ----

static void OnGameFrameUpdate() {
    sFrame++;
    if (!AnchorOn()) {
        return;
    }
    // A young Link with a sword and a shield on: Kokiri Forest's peace is over for him
    if (GdfInvasion_IsHero() && !OwnArmedOnce() && Anchor::Instance->IsSaveLoaded() &&
        IsSword(gSaveContext.equips.buttonItems[0]) && (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE)) {
        CVarSetInteger(CVAR_INVASION("Armed"), 1);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
}

static void RegisterGanondorfInvasion() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnGameFrameUpdate);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfInvasion, {});

// ---- The Invader's HUD ----
// Top centre, small and see-through: each Link's name and area, and in the same area an arrow (in his colour) pointing
// toward him from where the camera looks. Hidden in the pause menu and while text is up.

class GdfInvasionHud : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;
};

static void DrawArrow(ImDrawList* list, ImVec2 c, float angle, float size, ImU32 col) {
    // Pointing up at angle 0, turning clockwise
    float s = sinf(angle);
    float co = cosf(angle);
    auto rot = [&](float x, float y) { return ImVec2(c.x + x * co - y * s, c.y + x * s + y * co); };
    list->AddTriangleFilled(rot(0.0f, -size), rot(size * 0.7f, size * 0.7f), rot(-size * 0.7f, size * 0.7f), col);
}

// The Links' names, areas and arrows (Ganondorf only)
static void DrawLinkLocator() {
    PlayState* play = gPlayState;

    if (!GdfInvasion_IsInvader() || !Anchor::Instance->isConnected || !Anchor::Instance->IsSaveLoaded() ||
        (play == nullptr) || !CVarGetInteger(CVAR_INVASION("Hud"), 1)) {
        return;
    }
    if ((play->pauseCtx.state != 0) || (play->msgCtx.msgMode != MSGMODE_NONE) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float scale = vp->Size.y / 480.0f;
    float y = vp->Pos.y + 10.0f * scale;
    Player* link = GET_PLAYER(play);
    Camera* cam = GET_ACTIVE_CAM(play);

    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (client.self || !client.online) {
            continue;
        }
        std::string where = client.isSaveLoaded ? SohUtils::GetSceneName(client.sceneNum) : "File select";
        std::string text = client.name + "  -  " + where;
        bool sameScene = client.isSaveLoaded && (client.sceneNum == play->sceneNum) && (client.player != nullptr);
        ImU32 textCol = IM_COL32(235, 225, 255, 150);
        ImU32 shadowCol = IM_COL32(0, 0, 0, 110);
        float fontSize = ImGui::GetFontSize() * 0.85f * fmaxf(scale, 1.0f);
        ImVec2 size = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
        float arrowSpace = sameScene ? fontSize * 1.2f : 0.0f;
        float x = vp->Pos.x + (vp->Size.x - size.x - arrowSpace) * 0.5f;

        list->AddRectFilled(ImVec2(x - 8.0f, y - 3.0f), ImVec2(x + size.x + arrowSpace + 8.0f, y + size.y + 3.0f),
                            IM_COL32(20, 0, 30, 70), 6.0f);
        if (sameScene && (cam != nullptr)) {
            Vec3f from = link->actor.world.pos;
            Vec3f to = client.posRot.pos;
            s16 yaw = Math_Vec3f_Yaw(&from, &to) - Camera_GetCamDirYaw(cam);
            float dist = Math_Vec3f_DistXZ(&from, &to);
            // Faint when he's close (he can see him), clearer the further away
            int alpha = (int)CLAMP(60.0f + dist * 0.12f, 60.0f, 190.0f);

            // In his tunic's colour (each Link has his own)
            DrawArrow(list, ImVec2(x + fontSize * 0.45f, y + size.y * 0.5f), -(yaw / 32768.0f) * (float)M_PI,
                      fontSize * 0.42f, IM_COL32(client.color.r, client.color.g, client.color.b, alpha));
        }
        if (!sameScene) {
            // A dot in his colour instead of the arrow
            list->AddCircleFilled(ImVec2(x - 2.0f, y + size.y * 0.5f), fontSize * 0.2f,
                                  IM_COL32(client.color.r, client.color.g, client.color.b, 150));
        }
        list->AddText(ImGui::GetFont(), fontSize, ImVec2(x + arrowSpace + 1.0f, y + 1.0f), shadowCol, text.c_str());
        list->AddText(ImGui::GetFont(), fontSize, ImVec2(x + arrowSpace, y), textCol, text.c_str());
        y += size.y + 8.0f;
    }

}

std::shared_ptr<Ship::GuiWindow> GdfInvasion_CreateHudWindow() {
    return std::make_shared<GdfInvasionHud>(CVAR_WINDOW("GdfInvasionHud"), "Invasion HUD");
}

// ---- The lobby (in the Anchor menu) ----
// Up to 7 players: one Ganondorf and six Links, each Link in his own colour. Everyone in the room takes a seat; the
// host (the room's owner) starts the game once there's a Ganondorf, at least one Link, and everyone has a seat. The seat
// each player took goes along with Anchor's client state ("gdfSeat"), the start with the room state ("gdfStart").

#define GDF_SEAT_NONE 0
#define GDF_SEAT_GANONDORF 1
#define GDF_SEAT_FIRST_LINK 2
#define GDF_SEAT_COUNT 8 // none, Ganondorf, 6 Links
#define GDF_MAX_PLAYERS 7

struct SeatInfo {
    const char* name;
    Color_RGB8 color;
};

static const SeatInfo sSeats[GDF_SEAT_COUNT] = {
    { "No seat", { 128, 128, 128 } },     { "Ganondorf", { 150, 30, 40 } },   { "Green Link", { 40, 160, 40 } },
    { "Red Link", { 200, 30, 30 } },      { "Blue Link", { 40, 70, 220 } },   { "Purple Link", { 130, 50, 200 } },
    { "White Link", { 240, 240, 240 } }, { "Black Link", { 25, 25, 25 } },
};

static std::map<uint32_t, int> sClientSeats; // what each other player took
static int sAppliedStart = -1;                // the room's start count we last acted on
static int sRoomStart = 0;                    // the room's start count (0 = in the lobby)
static u32 sLastClientStateFrame = 0;
static int sStartToSend = 0; // what this player, as host, puts in the room state (not saved: a new room starts in the lobby)

static int OwnSeat() {
    return CVarGetInteger(CVAR_INVASION("Seat"), GDF_SEAT_NONE);
}

static bool InvasionOn() {
    return AnchorOn() && CVarGetInteger(CVAR_INVASION("Enabled"), 0);
}

void GdfInvasion_WriteClientState(nlohmann::json& state) {
    if (CVarGetInteger(CVAR_INVASION("Enabled"), 0)) {
        state["gdfSeat"] = OwnSeat();
    }
}

void GdfInvasion_ReadClientState(uint32_t clientId, const nlohmann::json& state) {
    // A state without a seat (an older game, or Invasion mode off) leaves what we knew
    if (state.is_object() && state.contains("gdfSeat")) {
        sClientSeats[clientId] = state.value("gdfSeat", GDF_SEAT_NONE);
    }
}

void GdfInvasion_WriteRoomState(nlohmann::json& state) {
    state["gdfStart"] = sStartToSend;
}

// Who holds a seat: the player with the lowest id who took it (two taking it at once, the other is bumped)
static uint32_t SeatHolder(int seat) {
    uint32_t holder = 0;

    if (seat == GDF_SEAT_NONE) {
        return 0;
    }
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        int taken = client.self ? OwnSeat() : (sClientSeats.count(clientId) ? sClientSeats[clientId] : 0);

        if (client.online && (taken == seat) && ((holder == 0) || (clientId < holder))) {
            holder = clientId;
        }
    }
    return holder;
}

// The lobby's host: the room's owner while he's here; otherwise (he left, or the room was made in an earlier session)
// the player with the lowest id, so there's always someone who can start the game
static bool IsLobbyHost() {
    uint32_t owner = Anchor::Instance->roomState.ownerClientId;
    uint32_t lowest = 0;

    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (!client.online) {
            continue;
        }
        if (clientId == owner) {
            return owner == Anchor::Instance->ownClientId;
        }
        if ((lowest == 0) || (clientId < lowest)) {
            lowest = clientId;
        }
    }
    return lowest == Anchor::Instance->ownClientId;
}

// Players on a different build can't play together (Anchor ignores most of their packets)
static bool SameBuild(const AnchorClient& client) {
    return client.self || (client.clientVersion == Anchor::clientVersion);
}

static void SendOwnState() {
    if (Anchor::Instance->isConnected) {
        Anchor::Instance->SendPacket_UpdateClientState();
    }
}

static void SaveCVars() {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// The host has started the game: each player becomes what his seat says
static void ApplyStart() {
    int seat = OwnSeat();
    int role = GDF_INVASION_OFF;

    if (seat == GDF_SEAT_GANONDORF) {
        role = GDF_INVASION_INVADER;
        CVarSetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1);
    } else if (seat >= GDF_SEAT_FIRST_LINK) {
        role = GDF_INVASION_HERO;
        CVarSetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 0);
        // His tunic (Anchor colours every player's tunic with his colour)
        Color_RGBA8 c = { sSeats[seat].color.r, sSeats[seat].color.g, sSeats[seat].color.b, 255 };
        CVarSetColor(CVAR_REMOTE_ANCHOR("Color.Value"), c);
    }
    CVarSetInteger(CVAR_INVASION("Role"), role);
    // Everyone in one team: the Links share their progress (and Ganondorf receives it)
    CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), "invasion");
    CVarSetInteger(CVAR_INVASION("Armed"), 0);
    CVarSetInteger(CVAR_INVASION("FullResolve"), 0); // and his Resolve is half until every Link has grown up // a new game: Kokiri Forest is at peace until he takes up sword and shield
    SaveCVars();
    SendOwnState();
    Notification::Emit({
        .prefix = "Invasion",
        .message = (role == GDF_INVASION_INVADER) ? "The game has started! You are Ganondorf. Start a new file."
                   : (role == GDF_INVASION_HERO)  ? std::string("The game has started! You are ") + sSeats[seat].name +
                                                       ". Start a new file."
                                                  : "The game has started without you (no seat).",
    });
}

void GdfInvasion_ReadRoomState(const nlohmann::json& state) {
    if (!state.is_object()) {
        return;
    }
    sRoomStart = state.value("gdfStart", 0);
    if (!CVarGetInteger(CVAR_INVASION("Enabled"), 0)) {
        return;
    }
    if ((sRoomStart > 0) && (sRoomStart != sAppliedStart)) {
        sAppliedStart = sRoomStart;
        ApplyStart();
    } else if (sRoomStart == 0) {
        // Back in the lobby
        sAppliedStart = 0;
        if (CVarGetInteger(CVAR_INVASION("Role"), 0) != GDF_INVASION_OFF) {
            CVarSetInteger(CVAR_INVASION("Role"), GDF_INVASION_OFF);
            SaveCVars();
        }
    }
}

// In the lobby, everyone's seat is sent again now and then (the server may not keep it in what it sends newcomers)
static void UpdateLobby() {
    if (!InvasionOn() || !Anchor::Instance->isConnected || (sRoomStart > 0)) {
        return;
    }
    if (sFrame - sLastClientStateFrame > 120) {
        sLastClientStateFrame = sFrame;
        SendOwnState();
    }
    // Bumped from a seat someone else got first
    int seat = OwnSeat();
    if ((seat != GDF_SEAT_NONE) && (SeatHolder(seat) != Anchor::Instance->ownClientId) && (SeatHolder(seat) != 0)) {
        CVarSetInteger(CVAR_INVASION("Seat"), GDF_SEAT_NONE);
        SaveCVars();
        SendOwnState();
    }
}

static void SeatButton(int seat, float w) {
    uint32_t holder = SeatHolder(seat);
    bool mine = (holder != 0) && (holder == Anchor::Instance->ownClientId);
    bool taken = (holder != 0) && !mine;
    const SeatInfo& info = sSeats[seat];
    std::string who = mine ? "You" : taken ? Anchor::Instance->clients[holder].name : "Free";
    ImVec4 col(info.color.r / 255.0f, info.color.g / 255.0f, info.color.b / 255.0f, 1.0f);
    ImGui::PushID(seat);
    ImGui::ColorButton("##swatch", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                       ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
    ImGui::SameLine();
    ImGui::BeginDisabled(taken || (sRoomStart > 0));
    std::string label = std::string(info.name) + ": " + who;
    if (mine) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.15f, 0.45f, 1.0f));
    }
    if (ImGui::Button(label.c_str(), ImVec2(w - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x, 0))) {
        CVarSetInteger(CVAR_INVASION("Seat"), mine ? GDF_SEAT_NONE : seat);
        SaveCVars();
        SendOwnState();
    }
    if (mine) {
        ImGui::PopStyleColor();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}

void GdfInvasion_DrawMenu() {
    bool enabled = CVarGetInteger(CVAR_INVASION("Enabled"), 0) != 0;

    ImGui::SeparatorText("Invasion (Links vs Ganondorf)");
    if (ImGui::Checkbox("Invasion mode", &enabled)) {
        CVarSetInteger(CVAR_INVASION("Enabled"), enabled ? 1 : 0);
        if (!enabled) {
            CVarSetInteger(CVAR_INVASION("Role"), GDF_INVASION_OFF);
        }
        SaveCVars();
        SendOwnState();
    }
    if (!enabled) {
        ImGui::TextWrapped("Up to six Links play the game together while one player invades as Ganondorf to make "
                           "it harder. His attacks and Dark Arts cost magic.");
        return;
    }
    bool chat = CVarGetInteger(CVAR_INVASION("Chat"), 1) != 0;
    if (ImGui::Checkbox("Chat (press T in game)", &chat)) {
        CVarSetInteger(CVAR_INVASION("Chat"), chat ? 1 : 0);
        SaveCVars();
    }
    if (!Anchor::Instance->isEnabled || !Anchor::Instance->isConnected) {
        ImGui::TextWrapped("Connect to a room (above) to open the lobby. Everyone uses the same Room ID.");
        return;
    }

    // Who's here
    int online = 0;
    int seated = 0;
    int links = 0;
    bool ganondorf = false;
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (!client.online) {
            continue;
        }
        online++;
        for (int seat = GDF_SEAT_GANONDORF; seat < GDF_SEAT_COUNT; seat++) {
            if (SeatHolder(seat) == clientId) {
                seated++;
                if (seat == GDF_SEAT_GANONDORF) {
                    ganondorf = true;
                } else {
                    links++;
                }
            }
        }
    }

    if (sRoomStart > 0) {
        int role = CVarGetInteger(CVAR_INVASION("Role"), GDF_INVASION_OFF);
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "The game is on.");
        ImGui::Text("You are: %s", (role == GDF_INVASION_INVADER) ? "Ganondorf"
                                   : (role == GDF_INVASION_HERO)  ? sSeats[CLAMP(OwnSeat(), 0, 7)].name
                                                                  : "watching (no seat)");
        ImGui::TextWrapped("Kokiri Forest is at peace for a young Link until he has a sword and a shield on.");
        if (role == GDF_INVASION_INVADER) {
            bool hud = CVarGetInteger(CVAR_INVASION("Hud"), 1) != 0;
            if (ImGui::Checkbox("Show where the Links are (top of the screen)", &hud)) {
                CVarSetInteger(CVAR_INVASION("Hud"), hud ? 1 : 0);
                SaveCVars();
            }
        }
        if (IsLobbyHost()) {
            if (ImGui::Button("Back to the lobby (everyone)")) {
                sStartToSend = 0;
                Anchor::Instance->SendPacket_UpdateRoomState();
            }
        }
        return;
    }

    ImGui::Text("Lobby: %d / %d players", online, GDF_MAX_PLAYERS);
    // Who's in the room, and anyone on another build
    bool mismatch = false;
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (!client.online) {
            continue;
        }
        if (!SameBuild(client)) {
            mismatch = true;
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s is on a different build of the game",
                               client.name.c_str());
        }
    }
    if (mismatch) {
        ImGui::TextWrapped("Everyone needs the exact same build (download it from the same place, the same run).");
    }
    ImGui::TextDisabled("Take a seat: one Ganondorf and up to six Links.");
    // Two seats to a row, all the same width
    float seatWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    SeatButton(GDF_SEAT_GANONDORF, seatWidth);
    for (int seat = GDF_SEAT_FIRST_LINK; seat < GDF_SEAT_COUNT; seat++) {
        SeatButton(seat, seatWidth);
        if (((seat - GDF_SEAT_FIRST_LINK) % 2) == 0) {
            ImGui::SameLine();
        }
    }

    if (online > GDF_MAX_PLAYERS) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Too many players: at most %d (1 Ganondorf + 6 Links).",
                           GDF_MAX_PLAYERS);
    }
    bool ready = ganondorf && (links > 0) && (seated == online) && (online <= GDF_MAX_PLAYERS) && !mismatch;
    if (IsLobbyHost()) {
        if (!ready) {
            ImGui::TextDisabled(mismatch           ? "Everyone needs the same build to start."
                                : !ganondorf        ? "Waiting for someone to be Ganondorf..."
                                : (links == 0)      ? "Waiting for at least one Link..."
                                : (seated < online) ? "Waiting for everyone to take a seat..."
                                                    : "Too many players.");
        }
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Start the game", ImVec2(-1.0f, 0.0f))) {
            sStartToSend = MAX(sRoomStart, sAppliedStart) + 1;
            Anchor::Instance->SendPacket_UpdateRoomState();
        }
        ImGui::EndDisabled();
    } else {
        ImGui::TextDisabled("Waiting for the host to start the game...");
    }
}

static void RegisterGanondorfInvasionLobby() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(UpdateLobby);
}

static RegisterShipInitFunc lobbyInitFunc(RegisterGanondorfInvasionLobby, {});

// =================================================================================================
// Resolve, the Dark Arts and the chat
// =================================================================================================
//
// Resolve: the Links' hits on Ganondorf wear down his Resolve (120). At 0 he's pacified: banished for 30 s (45, 60
// when it keeps happening), then he comes back at the area's entrance. It refills slowly when he isn't being hit.
//
// The Dark Arts: Ganondorf's item menu (START; Z + START still pauses), a 6 x 4 grid, one row per kind. He puts arts
// on C-Left / C-Down / C-Right like Link's items and casts them with those buttons, paying with his magic bar (which
// refills on its own in Invasion mode). The curses work now: they're cast on the nearest Link in his area and run
// in that Link's game. Nothing can be cast during the truce.
//
// The chat: T opens it (Enter sends, Escape closes); everyone in the room sees the messages.

#define GDF_FPS 20
#define GDF_RESOLVE_MAX 120.0f
#define GDF_CAST_RANGE 2000.0f

#define GDF_PKT_HIT "GDF_HIT"
#define GDF_PKT_CURSE "GDF_CURSE"
#define GDF_PKT_CURSE_ACK "GDF_CURSE_ACK"
#define GDF_PKT_PACIFIED "GDF_PACIFIED"
#define GDF_PKT_CHAT "GDF_CHAT"
#define GDF_PKT_DUNGEON_DONE "GDF_DUNGEON_DONE"
#define GDF_PKT_WORLD_HIT "GDF_WORLD_HIT"
#define GDF_PKT_WORLD_HOLD "GDF_WORLD_HOLD"
#define GDF_PKT_DOOR_KICK "GDF_DOOR_KICK"
#define GDF_PKT_SONG "GDF_SONG"
static void HandleSong(const nlohmann::json& payload);
#define GDF_PKT_BOSS_HIT "GDF_BOSS_HIT"
static void HandleBossHit(const nlohmann::json& payload);
static void HandleWorldHold(const nlohmann::json& payload);
#define GDF_PKT_VOLLEY "GDF_VOLLEY"
#define GDF_PKT_BALL_HIT "GDF_BALL_HIT"
static void HandleWorldHit(const nlohmann::json& payload);

// A dungeon (its rooms and its boss's room), and where outside it Ganondorf is put once a Link has beaten it
struct DungeonExit {
    s16 scene;
    s16 bossScene;
    s32 exit;
};
static const DungeonExit sDungeonExits[] = {
    { SCENE_DEKU_TREE, SCENE_DEKU_TREE_BOSS, ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE },
    { SCENE_DODONGOS_CAVERN, SCENE_DODONGOS_CAVERN_BOSS, ENTR_DEATH_MOUNTAIN_TRAIL_OUTSIDE_DODONGOS_CAVERN },
    { SCENE_JABU_JABU, SCENE_JABU_JABU_BOSS, ENTR_ZORAS_FOUNTAIN_OUTSIDE_JABU_JABU },
    { SCENE_FOREST_TEMPLE, SCENE_FOREST_TEMPLE_BOSS, ENTR_SACRED_FOREST_MEADOW_OUTSIDE_TEMPLE },
    { SCENE_FIRE_TEMPLE, SCENE_FIRE_TEMPLE_BOSS, ENTR_DEATH_MOUNTAIN_CRATER_OUTSIDE_TEMPLE },
    { SCENE_WATER_TEMPLE, SCENE_WATER_TEMPLE_BOSS, ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE },
    { SCENE_SPIRIT_TEMPLE, SCENE_SPIRIT_TEMPLE_BOSS, ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE },
    { SCENE_SHADOW_TEMPLE, SCENE_SHADOW_TEMPLE_BOSS, ENTR_GRAVEYARD_OUTSIDE_TEMPLE },
};

static int DungeonOf(s16 scene) {
    for (int i = 0; i < (int)ARRAY_COUNT(sDungeonExits); i++) {
        if ((sDungeonExits[i].scene == scene) || (sDungeonExits[i].bossScene == scene)) {
            return i;
        }
    }
    return -1;
}

static int sPendingDungeonExit = -1; // Ganondorf: a Link beat this dungeon, out he goes
static void UpdateDungeonExit(PlayState* play);

enum DarkArtKind { ART_OBSTACLE, ART_MONSTER, ART_CURSE, ART_OWN };

struct DarkArt {
    const char* name;
    DarkArtKind kind;
    int cost;     // magic (the full bar is 96)
    int cooldown; // seconds
    int duration; // seconds (curses)
    bool ready;   // built yet
    const char* desc;
};

#define GDF_ART_COLS 6
#define GDF_ART_ROWS 6
#define GDF_ART_COUNT (GDF_ART_COLS * GDF_ART_ROWS)

// Row by row: obstacles, monsters, curses, his own. Curses are index 12 onward.
static const DarkArt sArts[GDF_ART_COUNT] = {
    { "Beamos Watch", ART_OBSTACLE, 18, 25, 25, true, "A Beamos rises ahead of Link and burns him with its eye." },
    { "Wall of Flame", ART_OBSTACLE, 16, 10, 15, true, "A line of fire walls ahead of Link that erupt as he nears." },
    { "Rock Fall", ART_OBSTACLE, 20, 15, 0, true, "Burning rocks crash down from the sky around Link." },
    { "Bomb Rain", ART_OBSTACLE, 26, 30, 3, true, "Over twenty bombs rain down around Link." },
    { "Sealed Door", ART_OBSTACLE, 14, 20, 15, true, "Red ice seals the door nearest to Link." },
    { "Quicksand", ART_OBSTACLE, 18, 20, 15, true, "Link's feet sink: he moves at half speed." },
    { "Ambush Ring", ART_MONSTER, 18, 15, 0, true, "Four Deku Babas rise in a ring around Link." },
    { "Floormasters", ART_MONSTER, 26, 35, 0, true, "Three Floormasters crawl out around Link and lunge." },
    { "Swarm", ART_MONSTER, 20, 20, 0, true, "Five Keese burst out above Link." },
    { "Waves", ART_MONSTER, 32, 40, 0, true, "Three waves of Stalchildren, one every few seconds." },
    { "Sentinel", ART_MONSTER, 26, 30, 0, true, "A Like Like waits on Link's path." },
    { "Duel", ART_MONSTER, 40, 60, 40, true, "A Stalfos challenges Link (once per area)." },
    { "Backwards Path", ART_CURSE, 18, 30, 12, true, "Link's controls are reversed." },
    { "Frozen Ground", ART_CURSE, 12, 25, 15, true, "The ground under Link turns slippery." },
    { "Iron Weight", ART_CURSE, 14, 25, 15, true, "Link is weighed down by iron (not in water)." },
    { "Shrink", ART_CURSE, 16, 30, 20, true, "Link shrinks to a fraction of his size." },
    { "Sealed Hand", ART_CURSE, 20, 40, 20, true, "One of Link's C-button items is sealed." },
    { "Withering", ART_CURSE, 16, 35, 15, true, "Link can't heal, and loses a quarter heart every 3 s (never his last)." },
    { "Lost Bearings", ART_CURSE, 10, 30, 15, true, "Link can't Z-target." },
    { "Ghostly Gale", ART_CURSE, 14, 30, 15, true, "Gusts of wind push Link about." },
    { "Phantom Step", ART_OWN, 16, 30, 0, true, "Ganondorf vanishes and steps out somewhere out of sight." },
    { "Eye of Ganon", ART_OWN, 8, 10, 10, true, "Watch the nearest Link from above for 10 s." },
    { "Shadow Veil", ART_OWN, 24, 45, 15, true, "Ganondorf is invisible (and silent) to the Links." },
    { "Rupee Tithe", ART_OWN, 12, 60, 0, true, "Takes up to 20 rupees from Link (never his last 10)." },
    { "Moblin Guard", ART_MONSTER, 30, 40, 0, true, "Two spear Moblins appear ahead of Link and charge him." },
    { "Disarm", ART_CURSE, 24, 60, 15, true, "Link's sword is knocked from his hand: no sword for 15 s." },
    { "Dark Pact", ART_OWN, 0, 120, 0, true, "Trade 30 of your Resolve for 40 magic." },
    { "Dark Link", ART_MONSTER, 40, 120, 60, true, "Dark Link rises before Link, and you control him for a minute." },
    { "Wallmaster", ART_MONSTER, 24, 30, 0, true, "A Wallmaster drops on Link and drags him back to the entrance (2 at most)." },
    { "Iron Knuckle", ART_MONSTER, 40, 90, 0, true, "An Iron Knuckle bars Link's way (one at a time)." },
    { "Fire Pillar", ART_OBSTACLE, 26, 40, 15, true, "A wide pillar of fire rises around Link: he's trapped inside for 15 s." },
    { "Aero Alchemy", ART_CURSE, 30, 60, 30, true, "The air around Link turns to water: he swims for 30 s." },
    { "The Haunting", ART_CURSE, 34, 90, 20, true, "Darkness swallows Link and two ReDeads rise behind him, for 20 s." },
    { "Lava Floor", ART_CURSE, 28, 60, 30, true, "The floor burns Link for 30 s, or until he defeats an enemy." },
    { "Bomb Flower Hat", ART_CURSE, 40, 120, 30, true, "A bomb flower on a random Link's head: 30 s to hit you, or it blows." },
    { "Earthquake", ART_CURSE, 36, 90, 10, true, "Every Link is thrown off his feet for 10 s as boulders rain down (they don't hurt)." },
};

#define GDF_FIRST_CURSE 12
#define GDF_ART_RED_ICE 0
#define GDF_ART_FLAME_WALL 1
#define GDF_ART_ROCK_FALL 2
#define GDF_ART_BOMB_RAIN 3
#define GDF_ART_SEALED_DOOR 4
#define GDF_ART_QUICKSAND 5
#define GDF_ART_AMBUSH 6
#define GDF_ART_FLOORMASTERS 7
#define GDF_ART_SWARM 8
#define GDF_ART_WAVES 9
#define GDF_ART_SENTINEL 10
#define GDF_ART_DUEL 11
#define GDF_ART_PHANTOM_STEP 20
#define GDF_ART_EYE 21
#define GDF_ART_VEIL 22
#define GDF_ART_TITHE 23
#define GDF_ART_MOBLINS 24
#define GDF_ART_AMMO 25
#define GDF_ART_PACT 26
#define GDF_ART_DARKLINK 27
#define GDF_ART_WALLMASTER 28
#define GDF_ART_IRON_KNUCKLE 29
#define GDF_ART_FIRE_PILLAR 30
#define GDF_ART_AERO 31
#define GDF_ART_HAUNTING 32
#define GDF_ART_LAVA 33
#define GDF_ART_BOMB_HAT 34
#define GDF_ART_EARTHQUAKE 35
static const char* sCurseMessages[8] = {
    "has reversed your path!", "has frozen the ground!",  "weighs you down with iron!", "has shrunk you!",
    "has sealed your hand!",   "has cast a blinding gloom!", "has stolen your bearings!", "summons a ghostly gale!",
};

static const u16 sCButtons[3] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
static const char* sCNames[3] = { "C-Left", "C-Down", "C-Right" };

// ---- Ganondorf's side ----
static float sResolve = GDF_RESOLVE_MAX;

// His Resolve is half as deep while the Links are young; once every Link is an adult it's full for good (until a new
// game)
static float ResolveMax() {
    return CVarGetInteger(CVAR_INVASION("FullResolve"), 0) ? GDF_RESOLVE_MAX : (GDF_RESOLVE_MAX * 0.5f);
}

static void UpdateResolveMax() {
    if (CVarGetInteger(CVAR_INVASION("FullResolve"), 0) || (Anchor::Instance == nullptr)) {
        return;
    }
    int links = 0;
    int adults = 0;
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (client.self || !client.online || !client.isSaveLoaded || GdfInvasion_IsClientGanondorf(clientId)) {
            continue;
        }
        links++;
        adults += (client.linkAge == LINK_AGE_ADULT) ? 1 : 0;
    }
    if ((links > 0) && (adults == links)) {
        CVarSetInteger(CVAR_INVASION("FullResolve"), 1);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        sResolve = GDF_RESOLVE_MAX;
        Notification::Emit({ .prefix = "Invasion", .message = "Every Link has grown up: your Resolve is whole." });
    }
}
static u32 sLastHitFrame = 0;
static u32 sBanishedUntil = 0;
static u32 sStunnedUntil = 0;
static int sPacifyCount = 0;
static u32 sLastPacifyFrame = 0;
static u32 sArtReady[GDF_ART_COUNT]; // frame each art can be cast again
static bool sMenuOpen = false;
static int sCursor = GDF_FIRST_CURSE;
static u32 sMenuMoveFrame = 0;
static std::string sCastMessage;
static u32 sCastMessageUntil = 0;
static u32 sEyeUntil = 0;    // Eye of Ganon: watching a Link
static uint32_t sEyeTarget = 0;
static s16 sEyeCam = -1;

static int SlotArt(int slot) {
    static const char* sCVars[3] = { CVAR_INVASION("Slot0"), CVAR_INVASION("Slot1"), CVAR_INVASION("Slot2") };
    return CVarGetInteger(sCVars[slot], -1);
}

static void SetSlotArt(int slot, int art) {
    static const char* sCVars[3] = { CVAR_INVASION("Slot0"), CVAR_INVASION("Slot1"), CVAR_INVASION("Slot2") };
    for (int i = 0; i < 3; i++) {
        if ((i != slot) && (SlotArt(i) == art)) {
            CVarSetInteger(sCVars[i], -1); // an art sits on one button at a time
        }
    }
    CVarSetInteger(sCVars[slot], art);
    SaveCVars();
}

static bool IsBanished() {
    return sFrame < sBanishedUntil;
}

extern "C" s32 GdfInvasion_IsPacified(void) {
    return GdfInvasion_IsInvader() && IsBanished();
}

extern "C" s32 GdfInvasion_IsVanished(void) {
    return GdfInvasion_IsPacified() && (sFrame - sPacifiedFrame >= 3 * GDF_FPS);
}

static void Flash(const std::string& message);

// Ganon form: 25 Resolve and 40 magic to become him, then 2 magic a second while he's out
#define GDF_GANON_RESOLVE 25.0f
#define GDF_GANON_MAGIC 40
extern "C" s32 GdfInvasion_PayGanonForm(void) {
    if (!GdfInvasion_IsInvader() || (GdfInvasion_Role() == GDF_INVASION_OFF)) {
        return true;
    }
    if (gSaveContext.magic < GDF_GANON_MAGIC) {
        Flash("Ganon form needs 40 magic");
    } else if (sResolve <= GDF_GANON_RESOLVE + 1.0f) {
        Flash("Ganon form needs more Resolve");
    } else {
        gSaveContext.magic -= GDF_GANON_MAGIC;
        sResolve -= GDF_GANON_RESOLVE;
        return true;
    }
    Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
    return false;
}

extern "C" s32 GdfInvasion_GanonFormUpkeep(void) {
    if (!GdfInvasion_IsInvader() || (GdfInvasion_Role() == GDF_INVASION_OFF)) {
        return true;
    }
    if ((sFrame % (GDF_FPS / 2)) == 0) {
        if (gSaveContext.magic <= 0) {
            Flash("Out of magic: Ganon fades");
            return false;
        }
        gSaveContext.magic--;
    }
    return true;
}

static bool GameStarted() {
    return GdfInvasion_Role() != GDF_INVASION_OFF;
}

static void Flash(const std::string& message);

extern "C" s32 GdfInvasion_PayAttack(s32 cost) {
    if (!GdfInvasion_IsInvader() || !GameStarted() || (cost <= 0)) {
        return true;
    }
    if (gSaveContext.magic < cost) {
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Flash("Not enough magic");
        return false;
    }
    gSaveContext.magic -= cost;
    return true;
}

extern "C" s32 GdfInvasion_CanHurtLink(uint32_t clientId) {
    return GdfInvasion_IsInvader() && GameStarted() && !GdfInvasion_LinkAtPeace(clientId);
}

extern "C" s32 GdfInvasion_CanHurtLinks(void) {
    return GdfInvasion_IsInvader() && GameStarted();
}

extern "C" s32 GdfInvasion_CanAct(void) {
    if (!GdfInvasion_IsInvader()) {
        return true;
    }
    return !sMenuOpen && !IsBanished() && (sFrame >= sEyeUntil) && (sFrame >= sPossessUntil);
}

static void Flash(const std::string& message) {
    sCastMessage = message;
    sCastMessageUntil = sFrame + 3 * GDF_FPS;
}

// The Link a Dark Art goes to: the nearest one in his area, close enough
static uint32_t FindTargetLink(PlayState* play, float range = GDF_CAST_RANGE) {
    Player* self = GET_PLAYER(play);
    uint32_t best = 0;
    float bestDist = range;

    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (client.self || !client.online || !client.isSaveLoaded || (client.sceneNum != play->sceneNum) ||
            GdfInvasion_IsClientGanondorf(clientId) || (client.player == nullptr)) {
            continue;
        }
        Vec3f to = client.posRot.pos;
        float d = Math_Vec3f_DistXYZ(&self->actor.world.pos, &to);
        if (d < bestDist) {
            bestDist = d;
            best = clientId;
        }
    }
    return best;
}

static bool GroundSpotAround(PlayState* play, Vec3f center, float minDist, float maxDist, Vec3f* out);
static void Smoke(PlayState* play, Vec3f* pos);

static void HandleCurse(const nlohmann::json& payload);

static void TryCast(PlayState* play, int slot) {
    int art = SlotArt(slot);

    if ((art < 0) || (art >= GDF_ART_COUNT)) {
        return;
    }
    const DarkArt& a = sArts[art];
    if (sFrame < sPossessUntil) {
        return; // he's busy being Dark Link
    }
    if (!a.ready) {
        Flash(std::string(a.name) + " isn't ready yet (coming in a later version)");
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    if (sFrame < sStunnedUntil) {
        return;
    }
    if (sFrame < sArtReady[art]) {
        Flash(std::string(a.name) + ": " + std::to_string((sArtReady[art] - sFrame) / GDF_FPS + 1) + " s");
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    if (gSaveContext.magic < a.cost) {
        Flash("Not enough magic");
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    // His own arts: worked here, in his game
    if (art == GDF_ART_PACT) {
        if (sResolve <= 31.0f) {
            Flash("Not enough Resolve for the pact");
            return;
        }
        sResolve -= 30.0f;
        gSaveContext.magic = MIN(gSaveContext.magic + 40, gSaveContext.magicCapacity);
        sArtReady[art] = sFrame + a.cooldown * GDF_FPS;
        Flash("Dark Pact: 30 Resolve for 40 magic");
        BossGanon_Player_CastPose();
        return;
    }
    if (art == GDF_ART_VEIL) {
        gSaveContext.magic -= a.cost;
        sArtReady[art] = sFrame + a.cooldown * GDF_FPS;
        sVeilUntil = sFrame + a.duration * GDF_FPS;
        Flash("Veiled: the Links can't see or hear you");
        return;
    }
    if (TestModeOn() && (art == GDF_ART_EYE)) {
        Flash("Eye of Ganon needs another player");
        return;
    }
    if (TestModeOn() && (art == GDF_ART_PHANTOM_STEP)) {
        Player* self = GET_PLAYER(play);
        Vec3f spot;
        if (GroundSpotAround(play, self->actor.world.pos, 400.0f, 700.0f, &spot)) {
            Smoke(play, &self->actor.world.pos);
            self->actor.world.pos = spot;
            self->actor.prevPos = spot;
            BossGanon_Player_NetSnap();
            Smoke(play, &spot);
            gSaveContext.magic -= a.cost;
            sArtReady[art] = sFrame + a.cooldown * GDF_FPS;
        }
        return;
    }
    if ((art == GDF_ART_PHANTOM_STEP) || (art == GDF_ART_EYE)) {
        uint32_t who = FindTargetLink(play, (art == GDF_ART_EYE) ? 100000.0f : GDF_CAST_RANGE * 2.0f);
        if (who == 0) {
            Flash("No Link in this area");
            Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return;
        }
        Player* self = GET_PLAYER(play);
        if (art == GDF_ART_PHANTOM_STEP) {
            // Out of the Link's sight: somewhere on the ground well away from him
            Vec3f spot;
            Vec3f linkPos = Anchor::Instance->clients[who].posRot.pos;
            if (!GroundSpotAround(play, linkPos, 700.0f, 1100.0f, &spot)) {
                Flash("Nowhere to step to here");
                return;
            }
            Smoke(play, &self->actor.world.pos);
            self->actor.world.pos = spot;
            self->actor.prevPos = spot;
            self->actor.shape.rot.y = self->actor.world.rot.y = self->yaw = Math_Vec3f_Yaw(&spot, &linkPos);
            BossGanon_Player_NetSnap();
            Smoke(play, &spot);
        } else {
            sEyeTarget = who;
            sEyeUntil = sFrame + a.duration * GDF_FPS;
        }
        gSaveContext.magic -= a.cost;
        sArtReady[art] = sFrame + a.cooldown * GDF_FPS;
        Flash(std::string(a.name));
        return;
    }
    if (sFrame < sVeilUntil) {
        Flash("Not while veiled");
        return;
    }
    // Test mode: on himself (he's the Link here too)
    if (TestModeOn()) {
        gSaveContext.magic -= a.cost;
        sArtReady[art] = sFrame + a.cooldown * GDF_FPS;
        if (art == GDF_ART_DARKLINK) {
            sPossessStart = sFrame;
            sPossessTarget = 0;
            sPossessUntil = sFrame + (a.duration + 1) * GDF_FPS;
        }
        nlohmann::json payload;
        payload["curse"] = art - GDF_FIRST_CURSE;
        payload["art"] = art;
        payload["duration"] = a.duration;
        payload["clientId"] = 0;
        HandleCurse(payload);
        BossGanon_Player_CastPose();
        Flash(std::string(a.name) + " (test)");
        return;
    }
    uint32_t target = FindTargetLink(play);
    if (art == GDF_ART_BOMB_HAT) {
        // A random Link, wherever he is
        std::vector<uint32_t> links;
        for (auto& [clientId, client] : Anchor::Instance->clients) {
            if (!client.self && client.online && client.isSaveLoaded && !GdfInvasion_IsClientGanondorf(clientId) &&
                !GdfInvasion_LinkAtPeace(clientId)) {
                links.push_back(clientId);
            }
        }
        target = links.empty() ? 0 : links[(size_t)(Rand_ZeroOne() * links.size()) % links.size()];
    }
    if (target == 0) {
        Flash("No Link close enough");
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    if (GdfInvasion_LinkAtPeace(target)) {
        Flash("Kokiri Forest is at peace until he takes up sword and shield");
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    // Paid now (given back if that Link's game refuses it)
    gSaveContext.magic -= a.cost;
    sArtReady[art] = sFrame + a.cooldown * GDF_FPS;
    nlohmann::json payload;
    payload["type"] = GDF_PKT_CURSE;
    payload["targetClientId"] = target;
    payload["curse"] = art - GDF_FIRST_CURSE;
    if (art == GDF_ART_DARKLINK) {
        sPossessStart = sFrame;
        sPossessTarget = target;
        sPossessUntil = sFrame + (a.duration + 1) * GDF_FPS;
        Flash("You are Dark Link for a minute");
    }
    payload["art"] = art;
    payload["duration"] = a.duration;
    Anchor::Instance->SendJsonToRemote(payload);
    if (art == GDF_ART_EARTHQUAKE) {
        // Every other Link too
        for (auto& [clientId, client] : Anchor::Instance->clients) {
            if ((clientId != target) && !client.self && client.online && client.isSaveLoaded &&
                !GdfInvasion_IsClientGanondorf(clientId)) {
                payload["targetClientId"] = clientId;
                Anchor::Instance->SendJsonToRemote(payload);
            }
        }
    }
    BossGanon_Player_CastPose();
    Flash(std::string(a.name) + " on " + Anchor::Instance->clients[target].name);
}

extern "C" s32 GdfInvasion_HandleCButtons(PlayState* play) {
    Input* input = &play->state.input[0];

    if (!GdfInvasion_IsInvader() || !GameStarted()) {
        return false;
    }
    for (int slot = 0; slot < 3; slot++) {
        if (CHECK_BTN_ALL(input->press.button, sCButtons[slot]) && (SlotArt(slot) >= 0)) {
            TryCast(play, slot);
            return true;
        }
    }
    return false;
}

// START opens and closes the menu; in it the D-pad or stick moves, a C-button puts the art on that button
extern "C" s32 GdfInvasion_KaleidoHook(PlayState* play) {
    Input* input = &play->state.input[0];

    if (!GdfInvasion_IsInvader() || !GameStarted()) {
        sMenuOpen = false;
        return false;
    }
    if (!sMenuOpen) {
        if (CHECK_BTN_ALL(input->press.button, BTN_START) && !CHECK_BTN_ALL(input->cur.button, BTN_Z) &&
            (play->pauseCtx.state == 0) && !Player_InCsMode(play)) {
            sMenuOpen = true;
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return true;
        }
        return false;
    }
    // Closing
    if (CHECK_BTN_ALL(input->press.button, BTN_START) || CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sMenuOpen = false;
        Audio_PlaySfxGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return true;
    }
    // Moving
    int dx = 0;
    int dy = 0;
    if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        dx = -1;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        dx = 1;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        dy = -1;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        dy = 1;
    } else if (sFrame - sMenuMoveFrame > 4) {
        if (input->rel.stick_x < -40) {
            dx = -1;
        } else if (input->rel.stick_x > 40) {
            dx = 1;
        } else if (input->rel.stick_y > 40) {
            dy = -1;
        } else if (input->rel.stick_y < -40) {
            dy = 1;
        }
    }
    if ((dx != 0) || (dy != 0)) {
        int col = (sCursor % GDF_ART_COLS + dx + GDF_ART_COLS) % GDF_ART_COLS;
        int row = (sCursor / GDF_ART_COLS + dy + GDF_ART_ROWS) % GDF_ART_ROWS;
        sCursor = row * GDF_ART_COLS + col;
        sMenuMoveFrame = sFrame;
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
    // Onto a C-button
    for (int slot = 0; slot < 3; slot++) {
        if (CHECK_BTN_ALL(input->press.button, sCButtons[slot])) {
            if (sArts[sCursor].name[0] == '\0') {
                continue; // an empty cell
            }
            SetSlotArt(slot, sCursor);
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }
    return true; // no pausing while it's open
}

// ---- A Link's side: the curses on him ----
struct ActiveCurse {
    u32 until = 0;
    bool active = false;
};
static ActiveCurse sCurses[8];
static u16 sSealedButton = 0;
static s8 sBootsBefore = EQUIP_VALUE_BOOTS_KOKIRI;
static uint32_t sCurseFrom = 0;

static bool LinkCanBeCursed(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (GdfInvasion_LinkAtPeace(0)) {
        return false; // peaceful Kokiri Forest
    }

    if ((play == nullptr) || (player == nullptr) || Player_InCsMode(play) ||
        (play->csCtx.state != CS_STATE_IDLE) || (play->msgCtx.msgMode != MSGMODE_NONE) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->pauseCtx.state != 0)) {
        return false;
    }
    return !(player->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_GETTING_ITEM |
                                     PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE));
}

// ---- A Link's side: what Ganondorf summons and raises around him ----
#define GDF_MAX_SUMMONED 6
#define GDF_MAX_OBSTACLES 4
struct Summon {
    int art = -1;
    u32 start = 0;
    int spawned = 0;
    Vec3f spots[4];
    int spotCount = 0;
};
static Summon sSummon;
static Actor* sSummoned[GDF_MAX_SUMMONED];
struct Obstacle {
    Actor* actor = nullptr;
    u32 until = 0;
};
static Obstacle sObstacles[GDF_MAX_OBSTACLES * 5];
static u32 sQuicksandUntil = 0;
static int sTitheTaken = 0;

static bool ActorAlive(Actor* actor) {
    PlayState* play = gPlayState;

    if ((actor == nullptr) || (play == nullptr)) {
        return false;
    }
    for (int c = 0; c < ACTORCAT_MAX; c++) {
        for (Actor* a = play->actorCtx.actorLists[c].head; a != nullptr; a = a->next) {
            if (a == actor) {
                return a->update != nullptr;
            }
        }
    }
    return false;
}

static int LiveSummoned() {
    int n = 0;
    for (int i = 0; i < GDF_MAX_SUMMONED; i++) {
        if (!ActorAlive(sSummoned[i])) {
            sSummoned[i] = nullptr;
        } else {
            n++;
        }
    }
    return n;
}

static void Track(Actor* a) {
    Mirror(a);
    for (int j = 0; (a != nullptr) && (j < GDF_MAX_SUMMONED); j++) {
        if (sSummoned[j] == nullptr) {
            sSummoned[j] = a;
            return;
        }
    }
}

static void AddObstacle(Actor* a, int seconds) {
    Mirror(a);
    for (Obstacle& o : sObstacles) {
        if ((o.actor == nullptr) || !ActorAlive(o.actor)) {
            o.actor = a;
            o.until = sFrame + seconds * GDF_FPS;
            return;
        }
    }
}

// A spot on the ground around `center` (between minDist and maxDist from it)
static bool GroundSpotAround(PlayState* play, Vec3f center, float minDist, float maxDist, Vec3f* out) {
    for (int tries = 0; tries < 10; tries++) {
        s16 yaw = (s16)(Rand_ZeroOne() * 0x10000);
        float d = minDist + Rand_ZeroOne() * (maxDist - minDist);
        Vec3f p = center;
        p.x += Math_SinS(yaw) * d;
        p.z += Math_CosS(yaw) * d;
        p.y += 80.0f;
        CollisionPoly* poly;
        s32 bgId;
        float floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &p);
        if ((floorY > BGCHECK_Y_MIN) && (fabsf(floorY - center.y) < 120.0f)) {
            p.y = floorY;
            *out = p;
            return true;
        }
    }
    return false;
}

static bool GroundSpot(PlayState* play, float minDist, float maxDist, Vec3f* out) {
    return GroundSpotAround(play, GET_PLAYER(play)->actor.world.pos, minDist, maxDist, out);
}

// Ahead of Link (where he's facing), on the ground
static Vec3f AheadOfLink(PlayState* play, float dist) {
    Player* player = GET_PLAYER(play);
    Vec3f p = player->actor.world.pos;
    p.x += Math_SinS(player->actor.shape.rot.y) * dist;
    p.z += Math_CosS(player->actor.shape.rot.y) * dist;
    p.y += 80.0f;
    CollisionPoly* poly;
    s32 bgId;
    float floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &p);
    p.y = (floorY > BGCHECK_Y_MIN) ? floorY : player->actor.world.pos.y;
    return p;
}

static void Smoke(PlayState* play, Vec3f* pos) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    EffectSsDeadDb_Spawn(play, pos, &zero, &zero, 140, 0, 60, 0, 90, 255, 30, 0, 60, 1, 9, true);
}

static const char* ArtMessage(int art) {
    switch (art) {
        case GDF_ART_RED_ICE: return "sets a Beamos to watch you!";
        case GDF_ART_FLAME_WALL: return "raises a wall of flame!";
        case GDF_ART_ROCK_FALL: return "brings the sky down on you!";
        case GDF_ART_BOMB_RAIN: return "rains bombs upon you!";
        case GDF_ART_SEALED_DOOR: return "has sealed the door!";
        case GDF_ART_QUICKSAND: return "drags at your feet!";
        case GDF_ART_AMBUSH: return "springs an ambush!";
        case GDF_ART_FLOORMASTERS: return "sends the Floormasters!";
        case GDF_ART_SWARM: return "sends a swarm of Keese!";
        case GDF_ART_WAVES: return "sends wave after wave!";
        case GDF_ART_SENTINEL: return "sets a sentinel in your path!";
        case GDF_ART_DUEL: return "challenges you to a duel!";
        case GDF_ART_TITHE: return "takes his tithe!";
        case GDF_ART_MOBLINS: return "sends his Moblin guard!";
        case GDF_ART_AMMO: return "knocks the sword from your hand!";
        case GDF_ART_DARKLINK: return "sends your shadow against you!";
        case GDF_ART_WALLMASTER: return "sends a Wallmaster for you!";
        case GDF_ART_IRON_KNUCKLE: return "sends an Iron Knuckle!";
        case GDF_ART_FIRE_PILLAR: return "traps you in fire!";
        case GDF_ART_LAVA: return "turns the floor to lava!";
        case GDF_ART_EARTHQUAKE: return "shakes the earth!";
        case GDF_ART_AERO: return "turns the air to water!";
        case GDF_ART_HAUNTING: return "...something is behind you.";
        case GDF_ART_BOMB_HAT: return "crowns you with a bomb flower! Hit him within 30 s!";
    }
    return "has cursed you!";
}

static int MonstersFor(int art) {
    switch (art) {
        case GDF_ART_AMBUSH: return 4;
        case GDF_ART_FLOORMASTERS: return 3;
        case GDF_ART_SWARM: return 5;
        case GDF_ART_WAVES: return 3;
        case GDF_ART_SENTINEL:
        case GDF_ART_DUEL: return 1;
        case GDF_ART_MOBLINS: return 2;
        case GDF_ART_DARKLINK: return 1;
        case GDF_ART_WALLMASTER: return 1;
        case GDF_ART_HAUNTING: return 2;
        case GDF_ART_IRON_KNUCKLE: return 1;
    }
    return 0;
}

// ---- The Elements row ----
static u32 sElementTimerUntil = 0; // the game's own timer shows the time left
static u32 sLavaUntil = 0;
#define GDF_PILLAR_SCALE 2.1f // (81 wide as a vanilla pillar: 170 here)
static Actor* sPillar = nullptr;
static Vec3f sPillarCenter;
static u32 sAeroUntil = 0;
static u32 sHauntUntil = 0;
static u32 sDisarmUntil = 0;
static u32 sQuakeUntil = 0;
static u32 sQuakeNextFall = 0;
static std::vector<Actor*> sQuakeRocks;
static Actor* sBombHat = nullptr;
static u32 sBombHatUntil = 0;

static void StartZeldaTimer(s16 seconds) {
    Interface_SetTimer(seconds);
}

static void StopZeldaTimer() {
    if ((gSaveContext.timerState >= TIMER_STATE_DOWN_INIT) && (gSaveContext.timerState <= TIMER_STATE_DOWN_TICK)) {
        gSaveContext.timerState = TIMER_STATE_OFF;
    }
}

// The bomb flower hat: disarmed when this Link hurts Ganondorf
static void DisarmBombHat() {
    if (ActorAlive(sBombHat)) {
        Actor_Kill(sBombHat);
        StopZeldaTimer();
        sElementTimerUntil = 0;
        Notification::Emit({ .prefix = "Invasion", .message = "The bomb flower is disarmed!" });
    }
    sBombHat = nullptr;
}

static void UpdateElements(PlayState* play) {
    Player* player = (play != nullptr) ? GET_PLAYER(play) : nullptr;

    // His Iron Knuckle doesn't sit waiting to be struck: it stands up and comes for Link at once
    for (Actor* a : sSummoned) {
        if (ActorAlive(a) && (a->id == ACTOR_EN_IK) && (((EnIk*)a)->unk_2F8 == 3) &&
            (((EnIk*)a)->skelAnime.playSpeed == 0.0f)) {
            ((EnIk*)a)->skelAnime.playSpeed = 1.0f;
        }
    }

    // Fire Pillar: trapped inside it
    if (sPillar != nullptr) {
        if (!ActorAlive(sPillar) || (player == nullptr)) {
            sPillar = nullptr;
        } else {
            f32 radius = 81.0f * GDF_PILLAR_SCALE;
            f32 dx = player->actor.world.pos.x - sPillarCenter.x;
            f32 dz = player->actor.world.pos.z - sPillarCenter.z;
            f32 d = sqrtf(dx * dx + dz * dz);
            ((BgHidanCurtain*)sPillar)->collider.base.atFlags &= ~AT_ON;
            if ((d > radius - 25.0f) && (d > 1.0f)) {
                // Into the flames: burned, and back inside
                if (player->invincibilityTimer <= 0) {
                    GameInteractor::RawAction::BurnPlayer();
                }
                player->actor.world.pos.x = sPillarCenter.x + dx / d * (radius - 30.0f);
                player->actor.world.pos.z = sPillarCenter.z + dz / d * (radius - 30.0f);
                player->linearVelocity = 0.0f;
            }
        }
    }

    // Earthquake: thrown off his feet again and again, the ground rumbling, harmless boulders raining down
    if ((sQuakeUntil != 0) && (player != nullptr)) {
        if (sFrame >= sQuakeUntil) {
            sQuakeUntil = 0;
        } else {
            if ((sFrame % 20) == 0) {
                Actor_RequestQuakeAndRumble(&player->actor, play, 5, 25);
            }
            Audio_PlaySfxGeneral(NA_SE_EV_EARTHQUAKE - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            if ((sFrame >= sQuakeNextFall) && (gSaveContext.health > 0) &&
                !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DEAD))) {
                player->actor.colChkInfo.damage = 0;
                func_80837C0C(play, player, PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE, 0.0f, 4.0f,
                              player->actor.shape.rot.y + 0x8000, 0);
                sQuakeNextFall = sFrame + 50;
            }
            if ((sFrame % 4) == 0) {
                Vec3f p = player->actor.world.pos;
                p.x += Rand_CenteredFloat(600.0f);
                p.z += Rand_CenteredFloat(600.0f);
                p.y += 400.0f;
                Actor* rock = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_FIRE_ROCK, p.x, p.y, p.z, 0, 0, 0,
                                          FIRE_ROCK_SPAWNED_FALLING2);
                if (rock != nullptr) {
                    sQuakeRocks.push_back(rock);
                }
            }
        }
    }
    // The boulders never hurt
    for (auto it = sQuakeRocks.begin(); it != sQuakeRocks.end();) {
        if (!ActorAlive(*it)) {
            it = sQuakeRocks.erase(it);
            continue;
        }
        ((EnFireRock*)*it)->collider.base.atFlags &= ~AT_ON;
        ((EnFireRock*)*it)->collider.base.acFlags &= ~AC_ON;
        ++it;
    }

    if ((sElementTimerUntil != 0) && (sFrame >= sElementTimerUntil)) {
        sElementTimerUntil = 0;
        StopZeldaTimer();
    }
    // Lava Floor: the ground itself burns him (not ladders, vines, chests, platforms or anything that isn't floor)
    if (sLavaUntil != 0) {
        if ((sFrame >= sLavaUntil) || (player == nullptr)) {
            sLavaUntil = 0;
        } else if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (player->actor.floorBgId == BGCHECK_SCENE) &&
                   !(player->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_IN_WATER)) &&
                   (player->invincibilityTimer <= 0) && ((sFrame % 10) == 0)) {
            GameInteractor::RawAction::BurnPlayer();
        }
    }
    // The bomb flower hat: on his head, until he hits Ganondorf or it blows
    if (sBombHat != nullptr) {
        if (!ActorAlive(sBombHat) || (player == nullptr)) {
            sBombHat = nullptr;
        } else if (sFrame >= sBombHatUntil) {
            ((EnBom*)sBombHat)->timer = 1; // blows
            sBombHat = nullptr;
            gSaveContext.health = 0;
        } else {
            sBombHat->world.pos = player->actor.world.pos;
            sBombHat->world.pos.y += 75.0f;
            sBombHat->velocity = { 0.0f, 0.0f, 0.0f };
            sBombHat->gravity = 0.0f;
            ((EnBom*)sBombHat)->timer = 200;
        }
    }
}

static bool DarkLinkHere(PlayState* play) {
    for (int cat : { ACTORCAT_BOSS, ACTORCAT_ENEMY }) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
            if (a->id == ACTOR_EN_TORCH2) {
                return true;
            }
        }
    }
    return false;
}

// Starts an art in this Link's game; false when it can't happen now (the magic goes back)
static bool StartArt(PlayState* play, int art) {
    Player* player = GET_PLAYER(play);

    if (sSummon.art >= 0) {
        return false; // one at a time
    }
    int monsters = MonstersFor(art);
    if ((monsters > 0) && (LiveSummoned() + monsters > GDF_MAX_SUMMONED)) {
        return false; // too many of his monsters about already
    }
    switch (art) {
        case GDF_ART_RED_ICE: {
            // Beamos Watch (it took the Red Ice Wall's place): one on Link's path, its eye sweeping for him
            Vec3f p = AheadOfLink(play, 260.0f);
            Smoke(play, &p);
            Actor* beamos = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_VM, p.x, p.y, p.z, 0,
                                        Math_Vec3f_Yaw(&p, &player->actor.world.pos), 0, (0x0C << 8) | 0);
            if (beamos == nullptr) {
                return false;
            }
            AddObstacle(beamos, 25);
            break;
        }
        case GDF_ART_SEALED_DOOR: {
            // The door nearest to Link (within reach)
            Actor* best = nullptr;
            float bestDist = 800.0f;
            for (Actor* a = play->actorCtx.actorLists[ACTORCAT_DOOR].head; a != nullptr; a = a->next) {
                float d = Actor_WorldDistXZToActor(&player->actor, a);
                if ((a->update != nullptr) && (d < bestDist)) {
                    best = a;
                    bestDist = d;
                }
            }
            if (best == nullptr) {
                return false;
            }
            // In front of it, on Link's side
            s16 yaw = Actor_WorldYawTowardActor(best, &player->actor);
            Vec3f p = best->world.pos;
            p.x += Math_SinS(yaw) * 25.0f;
            p.z += Math_CosS(yaw) * 25.0f;
            Actor* ice = Actor_Spawn(&play->actorCtx, play, ACTOR_BG_ICE_SHELTER, p.x, p.y, p.z, 0, yaw, 0,
                                     (RED_ICE_WALL << 8) | 0x40 | 0x3F);
            if (ice == nullptr) {
                return false;
            }
            AddObstacle(ice, 15);
            break;
        }
        case GDF_ART_FLAME_WALL: {
            // Five fire walls in a line across Link's path
            Vec3f c = AheadOfLink(play, 160.0f);
            s16 across = player->actor.shape.rot.y + 0x4000;
            for (int i = -2; i <= 2; i++) {
                Vec3f p = c;
                p.x += Math_SinS(across) * i * 55.0f;
                p.z += Math_CosS(across) * i * 55.0f;
                Actor* fire = Actor_Spawn(&play->actorCtx, play, ACTOR_BG_HIDAN_FIREWALL, p.x, p.y, p.z, 0, 0, 0, 0);
                if (fire != nullptr) {
                    AddObstacle(fire, 15);
                }
            }
            break;
        }
        case GDF_ART_FIRE_PILLAR: {
            // One wide pillar of fire (Dodongo's Cavern's, Ganon's Castle's) rises around him: inside he's safe, but
            // its wall burns him and throws him back in
            Vec3f c = player->actor.world.pos;
            Actor* pillar = Actor_Spawn(&play->actorCtx, play, ACTOR_BG_HIDAN_CURTAIN, c.x, c.y, c.z, 0, 0, 0,
                                        (3 << 12) | 0x3F);
            if (pillar == nullptr) {
                return false;
            }
            Actor_SetScale(pillar, 0.09f * GDF_PILLAR_SCALE);
            ((BgHidanCurtain*)pillar)->collider.base.atFlags &= ~AT_ON; // (the wall's own check, UpdateElements)
            AddObstacle(pillar, 15);
            sPillar = pillar;
            sPillarCenter = c;
            StartZeldaTimer(15);
            sElementTimerUntil = sFrame + 15 * GDF_FPS;
            break;
        }
        case GDF_ART_HAUNTING: {
            // The lights go out, the ReDead cry rings out, and two of them stand up right behind him
            sHauntUntil = sFrame + 20 * GDF_FPS;
            StartZeldaTimer(20);
            sElementTimerUntil = sHauntUntil;
            Audio_PlaySfxGeneral(NA_SE_EN_REDEAD_CRY, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            s16 behind = player->actor.shape.rot.y + 0x8000;
            for (int i = 0; i < 2; i++) {
                s16 yaw = behind + (i ? 0x1400 : -0x1400);
                Vec3f p = player->actor.world.pos;
                p.x += Math_SinS(yaw) * 110.0f;
                p.z += Math_CosS(yaw) * 110.0f;
                Track(Actor_Spawn(&play->actorCtx, play, ACTOR_EN_RD, p.x, p.y, p.z, 0,
                                  Math_Vec3f_Yaw(&p, &player->actor.world.pos), 0, 1));
            }
            break;
        }
        case GDF_ART_AERO:
            sAeroUntil = sFrame + 30 * GDF_FPS;
            StartZeldaTimer(30);
            sElementTimerUntil = sAeroUntil;
            break;
        case GDF_ART_EARTHQUAKE:
            sQuakeUntil = sFrame + 10 * GDF_FPS;
            sQuakeNextFall = sFrame;
            StartZeldaTimer(10);
            sElementTimerUntil = sQuakeUntil;
            break;
        case GDF_ART_LAVA:
            sLavaUntil = sFrame + 30 * GDF_FPS;
            StartZeldaTimer(30);
            sElementTimerUntil = sLavaUntil;
            break;
        case GDF_ART_BOMB_HAT:
            if (ActorAlive(sBombHat)) {
                return false;
            }
            sBombHat = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, player->actor.world.pos.x,
                                   player->actor.world.pos.y + 80.0f, player->actor.world.pos.z, 0, 0, 0, BOMB_BODY);
            if (sBombHat == nullptr) {
                return false;
            }
            sBombHat->room = -1;
            sBombHatUntil = sFrame + 30 * GDF_FPS;
            StartZeldaTimer(30);
            sElementTimerUntil = sBombHatUntil;
            break;
        case GDF_ART_WALLMASTER:
        case GDF_ART_IRON_KNUCKLE: {
            int same = 0;
            s16 id = (art == GDF_ART_WALLMASTER) ? ACTOR_EN_WALLMAS : ACTOR_EN_IK;
            for (Actor* a : sSummoned) {
                same += (ActorAlive(a) && (a->id == id)) ? 1 : 0;
            }
            if (same >= ((art == GDF_ART_WALLMASTER) ? 2 : 1)) {
                return false; // at most two Wallmasters, one Iron Knuckle
            }
            if (art == GDF_ART_WALLMASTER) {
                // Its shadow grows under him, then it drops
                Track(Actor_Spawn(&play->actorCtx, play, ACTOR_EN_WALLMAS, player->actor.world.pos.x,
                                  player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 0));
                break;
            }
            sSummon.art = art;
            sSummon.start = sFrame;
            sSummon.spawned = 0;
            sSummon.spotCount = 0;
            break;
        }
        case GDF_ART_QUICKSAND:
            sQuicksandUntil = sFrame + 15 * GDF_FPS;
            GameInteractor::State::MovementSpeedMultiplier = 0.5f;
            break;
        case GDF_ART_AMMO: // Disarm (took Ammo Blight's place): no sword (B) for 15 s
            sDisarmUntil = sFrame + 15 * GDF_FPS;
            StartZeldaTimer(15);
            sElementTimerUntil = sDisarmUntil;
            break;
        case GDF_ART_TITHE: {
            int take = CLAMP(gSaveContext.rupees - 10, 0, 20);
            if (take <= 0) {
                return false;
            }
            Rupees_ChangeBy(-take);
            sTitheTaken = take;
            break;
        }
        default:
            if ((art == GDF_ART_DARKLINK) && (ActorAlive(sDarkLink) || DarkLinkHere(play))) {
                return false; // one shadow at a time
            }
            if ((art == GDF_ART_DUEL) || (art == GDF_ART_MOBLINS) || (monsters > 0) || (art == GDF_ART_BOMB_RAIN) ||
                (art == GDF_ART_ROCK_FALL)) {
                sSummon.art = art;
                sSummon.start = sFrame;
                sSummon.spawned = 0;
                sSummon.spotCount = 0;
                break;
            }
            return false;
    }
    if (art == GDF_ART_HAUNTING) {
        return true; // no word, no laugh: he finds out the hard way
    }
    Audio_PlaySfxGeneral(NA_SE_EN_GANON_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    Notification::Emit({ .prefix = "Ganondorf", .message = ArtMessage(art) });
    return true;
}

// Monsters come up a second after dark smoke marks their spots
static void SpawnMonstersAt(PlayState* play, s16 actorId, s16 params, int count, float minDist, float maxDist,
                            bool flying) {
    Player* player = GET_PLAYER(play);
    u32 t = sFrame - sSummon.start;

    if (sSummon.spotCount == 0) {
        for (int i = 0; i < count; i++) {
            if (!GroundSpot(play, minDist, maxDist, &sSummon.spots[i])) {
                sSummon.spots[i] = player->actor.world.pos;
                sSummon.spots[i].x += minDist * Math_SinS(i * (0x10000 / count));
                sSummon.spots[i].z += minDist * Math_CosS(i * (0x10000 / count));
            }
            if (flying) {
                sSummon.spots[i].y += 120.0f;
            }
            Smoke(play, &sSummon.spots[i]);
        }
        sSummon.spotCount = count;
        sSummon.start = sFrame;
        return;
    }
    if (t >= GDF_FPS) {
        for (int i = 0; i < sSummon.spotCount; i++) {
            Vec3f* p = &sSummon.spots[i];
            Track(Actor_Spawn(&play->actorCtx, play, actorId, p->x, p->y, p->z, 0,
                              Math_Vec3f_Yaw(p, &player->actor.world.pos), 0, params));
        }
        sSummon.spotCount = 0;
        sSummon.spawned++;
    }
}

static void UpdateSummons() {
    PlayState* play = gPlayState;

    // Timed obstacles melt away
    for (Obstacle& o : sObstacles) {
        if ((o.actor != nullptr) && ((sFrame >= o.until) || !ActorAlive(o.actor))) {
            if (ActorAlive(o.actor)) {
                Actor_Kill(o.actor);
            }
            o.actor = nullptr;
        }
    }
    UpdateElements(play);
    // Dark Link's minute is up
    if (sDarkLink != nullptr) {
        if (!ActorAlive(sDarkLink)) {
            sDarkLink = nullptr;
        } else if ((play != nullptr) && (sFrame >= sDarkLinkUntil)) {
            Smoke(play, &sDarkLink->world.pos);
            Actor_Kill(sDarkLink);
            sDarkLink = nullptr;
        }
    }
    if ((sQuicksandUntil != 0) && (sFrame >= sQuicksandUntil)) {
        sQuicksandUntil = 0;
        GameInteractor::State::MovementSpeedMultiplier = 1.0f;
    }
    if (sSummon.art < 0) {
        return;
    }
    if ((play == nullptr) || !GdfInvasion_IsHero() || (sFrame - sSummon.start > 20 * GDF_FPS)) {
        sSummon.art = -1;
        return;
    }
    Player* player = GET_PLAYER(play);
    u32 t = sFrame - sSummon.start;

    switch (sSummon.art) {
        case GDF_ART_BOMB_RAIN:
        case GDF_ART_ROCK_FALL: {
            // Falling from above over three seconds, around him and on him
            bool bombs = sSummon.art == GDF_ART_BOMB_RAIN;
            int total = bombs ? 24 : 14;
            while ((sSummon.spawned < total) && (sSummon.spawned < (int)(t * total / (3 * GDF_FPS)) + 1)) {
                s16 yaw = (s16)(Rand_ZeroOne() * 0x10000);
                float d = Rand_ZeroOne() * 260.0f;
                Vec3f p = player->actor.world.pos;
                p.x += Math_SinS(yaw) * d;
                p.z += Math_CosS(yaw) * d;
                p.y += 350.0f + Rand_ZeroOne() * 150.0f;
                if (bombs) {
                    Actor* bomb = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, p.x, p.y, p.z, 0, 0, 0, BOMB_BODY);
                    if (bomb != nullptr) {
                        ((EnBom*)bomb)->timer = 45 + (s16)(Rand_ZeroOne() * 25.0f);
                    }
                    Mirror(bomb);
                } else {
                    Mirror(Actor_Spawn(&play->actorCtx, play, ACTOR_EN_FIRE_ROCK, p.x, p.y, p.z, 0, 0, 0,
                                       FIRE_ROCK_SPAWNED_FALLING2));
                }
                sSummon.spawned++;
            }
            if (sSummon.spawned >= total) {
                sSummon.art = -1;
            }
            break;
        }
        case GDF_ART_FLOORMASTERS:
            SpawnMonstersAt(play, ACTOR_EN_FLOORMAS, 0, 3, 220.0f, 360.0f, false);
            if (sSummon.spawned > 0) {
                sSummon.art = -1;
            }
            break;
        case GDF_ART_AMBUSH:
            SpawnMonstersAt(play, ACTOR_EN_DEKUBABA, DEKUBABA_NORMAL, 4, 150.0f, 220.0f, false);
            if (sSummon.spawned > 0) {
                sSummon.art = -1;
            }
            break;
        case GDF_ART_SWARM:
            SpawnMonstersAt(play, ACTOR_EN_FIREFLY, KEESE_NORMAL_FLY, 4, 120.0f, 260.0f, true);
            if (sSummon.spawned > 0) {
                sSummon.art = -1;
            }
            break;
        case GDF_ART_WAVES:
            // A wave of Stalchildren, then the next a few seconds after
            if ((sSummon.spotCount != 0) || (t >= 5 * GDF_FPS) || (sSummon.spawned == 0)) {
                if ((sSummon.spawned > 0) && (sSummon.spotCount == 0)) {
                    if (LiveSummoned() > GDF_MAX_SUMMONED - 3) {
                        break; // the last wave is still about
                    }
                }
                SpawnMonstersAt(play, ACTOR_EN_SKB, 0, 3, 200.0f, 320.0f, false);
            }
            if (sSummon.spawned >= 3) {
                sSummon.art = -1;
            }
            break;
        case GDF_ART_IRON_KNUCKLE:
            SpawnMonstersAt(play, ACTOR_EN_IK, 2, 1, 220.0f, 320.0f, false); // a black Iron Knuckle, standing
            if (sSummon.spawned > 0) {
                sSummon.art = -1;
            }
            break;
        case GDF_ART_DARKLINK: {
            if (sSummon.spotCount == 0) {
                sSummon.spots[0] = AheadOfLink(play, 220.0f);
                sSummon.spotCount = 1;
                sSummon.start = sFrame;
                Smoke(play, &sSummon.spots[0]);
                break;
            }
            if (t >= GDF_FPS) {
                Vec3f* p = &sSummon.spots[0];
                sDarkLink = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_TORCH2, p->x, p->y, p->z, 0,
                                        Math_Vec3f_Yaw(p, &player->actor.world.pos), 0, 0);
                Track(sDarkLink);
                sDarkLinkUntil = sFrame + 60 * GDF_FPS;
                sSummon.art = -1;
                sSummon.spotCount = 0;
            }
            break;
        }
        case GDF_ART_MOBLINS: {
            // Two spear Moblins, ahead of him and to either side
            if (sSummon.spotCount == 0) {
                Vec3f c = AheadOfLink(play, 320.0f);
                s16 across = player->actor.shape.rot.y + 0x4000;
                for (int i = 0; i < 2; i++) {
                    sSummon.spots[i] = c;
                    sSummon.spots[i].x += Math_SinS(across) * (i ? 90.0f : -90.0f);
                    sSummon.spots[i].z += Math_CosS(across) * (i ? 90.0f : -90.0f);
                    Smoke(play, &sSummon.spots[i]);
                }
                sSummon.spotCount = 2;
                sSummon.start = sFrame;
                break;
            }
            if (t >= GDF_FPS) {
                for (int i = 0; i < 2; i++) {
                    Vec3f* p = &sSummon.spots[i];
                    Track(Actor_Spawn(&play->actorCtx, play, ACTOR_EN_MB, p->x, p->y, p->z, 0,
                                      Math_Vec3f_Yaw(p, &player->actor.world.pos), 0, -1)); // a spear Moblin on guard
                }
                sSummon.art = -1;
                sSummon.spotCount = 0;
            }
            break;
        }
        case GDF_ART_SENTINEL:
        case GDF_ART_DUEL: {
            if (sSummon.spotCount == 0) {
                sSummon.spots[0] = AheadOfLink(play, 260.0f);
                sSummon.spotCount = 1;
                sSummon.start = sFrame;
                Smoke(play, &sSummon.spots[0]);
                break;
            }
            if (t >= GDF_FPS) {
                Vec3f* p = &sSummon.spots[0];
                bool duel = sSummon.art == GDF_ART_DUEL;
                Track(Actor_Spawn(&play->actorCtx, play, duel ? ACTOR_EN_TEST : ACTOR_EN_RR, p->x, p->y, p->z, 0,
                                  Math_Vec3f_Yaw(p, &player->actor.world.pos), 0, duel ? STALFOS_TYPE_1 : 0));
                sSummon.art = -1;
                sSummon.spotCount = 0;
            }
            break;
        }
        default:
            sSummon.art = -1;
            break;
    }
}

static s16 sWitherHealth = 0; // Withering: his health may only go down

static void SetCurse(int curse, bool on) {
    PlayState* play = gPlayState;

    switch (curse) {
        case 0:
            GameInteractor::State::ReverseControlsActive = on;
            break;
        case 1:
            GameInteractor::State::SlipperyFloorActive = on;
            break;
        case 2:
            if (play != nullptr) {
                if (on) {
                    sBootsBefore = (s8)(GET_PLAYER(play)->currentBoots + 1);
                    GameInteractor::RawAction::ForceEquipBoots(EQUIP_VALUE_BOOTS_IRON);
                } else {
                    GameInteractor::RawAction::ForceEquipBoots(sBootsBefore);
                }
            }
            break;
        case 3:
            GameInteractor::State::LinkSize = on ? GI_LINK_SIZE_MINISH : GI_LINK_SIZE_RESET;
            break;
        case 4:
            sSealedButton = on ? sCButtons[(int)(Rand_ZeroOne() * 2.99f)] : 0;
            break;
        case 5:
            // Withering (UpdateCurses): no healing, and his hearts slowly drain
            sWitherHealth = gSaveContext.health;
            break;
        case 6:
            GameInteractor::State::DisableZTargetingActive = on;
            break;
        case 7:
            GameInteractor::RawAction::SetRandomWind(on);
            break;
    }
}

static bool StartArt(PlayState* play, int art);

static void HandleCurse(const nlohmann::json& payload) {
    PlayState* play = gPlayState;
    int art = payload.value("art", -1);
    if ((art >= 0) && ((art < GDF_FIRST_CURSE) || (art >= GDF_FIRST_CURSE + 8))) {
        sTitheTaken = 0;
        bool ok = GdfInvasion_IsHero() && LinkCanBeCursed(play) && StartArt(play, art);
        nlohmann::json ack;
        ack["type"] = GDF_PKT_CURSE_ACK;
        ack["targetClientId"] = payload.value("clientId", 0u);
        ack["ok"] = ok;
        ack["art"] = art;
        ack["rupees"] = sTitheTaken;
        if (!TestModeOn()) {
            Anchor::Instance->SendJsonToRemote(ack);
        } else if (!ok) {
            Flash(std::string(sArts[art].name) + " didn't take hold here");
        }
        return;
    }
    int curse = payload.value("curse", -1);
    int duration = payload.value("duration", 10);
    uint32_t from = payload.value("clientId", 0u);
    bool ok = (curse >= 0) && (curse < 8) && GdfInvasion_IsHero() && LinkCanBeCursed(play) && !sCurses[curse].active;

    if (ok && (curse == 2) && (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        ok = false; // no iron in water
    }
    if (ok) {
        sCurses[curse].active = true;
        sCurses[curse].until = sFrame + duration * GDF_FPS;
        sCurseFrom = from;
        SetCurse(curse, true);
        Audio_PlaySfxGeneral(NA_SE_EN_GANON_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Notification::Emit({ .prefix = "Ganondorf", .message = sCurseMessages[curse] });
    }
    nlohmann::json ack;
    ack["type"] = GDF_PKT_CURSE_ACK;
    ack["targetClientId"] = from;
    ack["ok"] = ok;
    ack["art"] = payload.value("art", -1);
    if (!TestModeOn()) {
        Anchor::Instance->SendJsonToRemote(ack);
    }
}

static void UpdateCurses() {
    // Withering: whatever would heal him doesn't, and a quarter heart goes every 3 s (never his last heart)
    if (sCurses[5].active && GdfInvasion_IsHero() && (gSaveContext.health > 0)) {
        gSaveContext.healthAccumulator = 0;
        if (gSaveContext.health > sWitherHealth) {
            gSaveContext.health = sWitherHealth;
        }
        if (((sFrame % (3 * GDF_FPS)) == 0) && (gSaveContext.health > 16)) {
            gSaveContext.health = MAX(gSaveContext.health - 4, 16);
            Audio_PlaySfxGeneral(NA_SE_EN_GANON_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        sWitherHealth = gSaveContext.health;
    }
    for (int i = 0; i < 8; i++) {
        if (sCurses[i].active && ((sFrame >= sCurses[i].until) || !GdfInvasion_IsHero())) {
            sCurses[i].active = false;
            SetCurse(i, false);
        }
    }
}

extern "C" void GdfInvasion_FilterLinkInput(PlayState* play, Input* input) {
    // Ganondorf as Dark Link: his controller drives Dark Link, not himself
    if (GdfInvasion_IsInvader() && (sFrame < sPossessUntil)) {
        // From the controller itself: his own moveset has already taken B, R and Z out of the input
        Input* raw = &play->state.input[0];
        float sx = raw->cur.stick_x;
        float sy = raw->cur.stick_y;
        float tilt = sqrtf(sx * sx + sy * sy);
        sDlTilt = (tilt < 8.0f) ? 0 : (s16)fminf(tilt, 127.0f);
        sDlYaw = sPossessYaw - (s16)(atan2f(sx, sy) * (32768.0f / 3.14159265f));
        sDlButtons = raw->cur.button & (BTN_A | BTN_B | BTN_R | BTN_Z);
        sDlHeard = sFrame; // (test mode: heard straight away)
        memset(input, 0, sizeof(*input));
        return;
    }
    // Ganondorf: his menu open, or banished
    if (GdfInvasion_IsInvader() && (sMenuOpen || IsBanished() || (sFrame < sEyeUntil))) {
        memset(input, 0, sizeof(*input));
        return;
    }
    // A Link held by the throat: no control
    if (GdfInvasion_IsHeld()) {
        memset(input, 0, sizeof(*input));
        return;
    }
    // A Link in the earthquake: down, he can't get going
    if (GdfInvasion_IsHero() && (sQuakeUntil != 0) && (sFrame < sQuakeUntil)) {
        memset(input, 0, sizeof(*input));
        return;
    }
    // A Link disarmed: no sword
    if (GdfInvasion_IsHero() && (sFrame < sDisarmUntil)) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;
    }
    // A Link: a sealed C-button
    if (sSealedButton != 0) {
        input->cur.button &= ~sSealedButton;
        input->press.button &= ~sSealedButton;
    }
}

// ---- Resolve ----
static const DamageTable sResolveTable = {
    /* Deku nut      */ DMG_ENTRY(0, 1), // stuns him
    /* Deku stick    */ DMG_ENTRY(3, 0),
    /* Slingshot     */ DMG_ENTRY(1, 0),
    /* Explosive     */ DMG_ENTRY(4, 0),
    /* Boomerang     */ DMG_ENTRY(0, 1),
    /* Normal arrow  */ DMG_ENTRY(2, 0),
    /* Hammer swing  */ DMG_ENTRY(5, 3),
    /* Hookshot      */ DMG_ENTRY(0, 1),
    /* Kokiri sword  */ DMG_ENTRY(5, 0), // (a young Link's main weapon: 12 hits empty his half meter)
    /* Master sword  */ DMG_ENTRY(3, 0),
    /* Giant's Knife */ DMG_ENTRY(5, 0),
    /* Fire arrow    */ DMG_ENTRY(5, 0),
    /* Ice arrow     */ DMG_ENTRY(5, 0),
    /* Light arrow   */ DMG_ENTRY(15, 2),
    /* Unk arrow 1   */ DMG_ENTRY(2, 0),
    /* Unk arrow 2   */ DMG_ENTRY(2, 0),
    /* Unk arrow 3   */ DMG_ENTRY(2, 0),
    /* Fire magic    */ DMG_ENTRY(8, 3),
    /* Ice magic     */ DMG_ENTRY(5, 0),
    /* Light magic   */ DMG_ENTRY(8, 2),
    /* Shield        */ DMG_ENTRY(0, 0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0),
    /* Kokiri spin   */ DMG_ENTRY(8, 0),
    /* Giant spin    */ DMG_ENTRY(10, 3),
    /* Master spin   */ DMG_ENTRY(6, 0),
    /* Kokiri jump   */ DMG_ENTRY(8, 0),
    /* Giant jump    */ DMG_ENTRY(10, 3),
    /* Master jump   */ DMG_ENTRY(6, 3),
    /* Unknown 1     */ DMG_ENTRY(0, 0),
    /* Unblockable   */ DMG_ENTRY(0, 0),
    /* Hammer jump   */ DMG_ENTRY(10, 3),
    /* Unknown 2     */ DMG_ENTRY(0, 0),
};

static std::map<uint32_t, u32> sHitCooldown;

// A Link's side: his dummy of Ganondorf was hit
bool GdfInvasion_UpdateGanondorfDummy(Actor* actor, uint32_t clientId) {
    PlayState* play = gPlayState;
    Player* dummy = (Player*)actor;
    auto it = sClients.find(clientId);
    // Ganondorf, or his Ganon form (bigger)
    bool ganon = (it != sClients.end()) && GanonFresh(it->second) && !it->second.ganon.hidden;
    bool hittable =
        (it != sClients.end()) && ((it->second.pose.valid && !it->second.pose.hidden) || ganon) && GameStarted();
    float radius = ganon ? 85.0f : 40.0f;
    float height = ganon ? 260.0f : 125.0f;

    actor->shape.shadowAlpha = 0;
    if (!hittable) {
        actor->flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
        return true;
    }
    // Links can Z-target him (and Ganon): a hostile target, from further off, aimed at his chest or Ganon's
    // (his puppet is the Z-target while he's Ganondorf; as Ganon this hitbox is)
    if (ganon) {
        actor->flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
        actor->flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    } else {
        actor->flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
        actor->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    }
    actor->targetMode = 5; // (from far off: Ganon too, whose bulk keeps a Link at a distance)
    actor->focus.pos = actor->world.pos;
    actor->focus.pos.y += ganon ? 200.0f : 110.0f;
    actor->colChkInfo.damageTable = (DamageTable*)&sResolveTable;
    dummy->cylinder.info.bumperFlags &= ~BUMP_NO_HITMARK;
    dummy->cylinder.dim.radius = (s16)radius;
    dummy->cylinder.dim.height = (s16)height;

    if ((dummy->cylinder.base.acFlags & AC_HIT) && (sFrame >= sHitCooldown[clientId])) {
        nlohmann::json payload;
        payload["type"] = GDF_PKT_HIT;
        payload["targetClientId"] = clientId;
        payload["amount"] = actor->colChkInfo.damage;
        payload["effect"] = actor->colChkInfo.damageEffect;
        Anchor::Instance->SendJsonToRemote(payload);
        sHitCooldown[clientId] = sFrame + 8;
        DisarmBombHat(); // hurting him disarms the bomb flower hat
    }
    Collider_UpdateCylinder(actor, &dummy->cylinder);
    dummy->cylinder.dim.radius = (s16)radius;
    dummy->cylinder.dim.height = (s16)height;
    CollisionCheck_SetAC(play, &play->colChkCtx, &dummy->cylinder.base);
    Collider_ResetCylinderAC(play, &dummy->cylinder.base);
    return true;
}

static void HandleHit(const nlohmann::json& payload) {
    float amount = (float)payload.value("amount", 0);
    int effect = payload.value("effect", 0);

    if (!GdfInvasion_IsInvader() || IsBanished()) {
        return;
    }
    if (effect == 1) {
        sStunnedUntil = sFrame + 2 * GDF_FPS; // a Deku Nut: no casting for a moment
    }
    if (BossGanon_Player_IsBlockingNet()) {
        amount *= 0.5f; // his cape takes half
    }
    if (amount <= 0.0f) {
        BossGanon_Player_NetHurt(false);
        return;
    }
    sResolve -= amount;
    sLastHitFrame = sFrame;
    BossGanon_Player_NetHurt(effect >= 2);
    if (sResolve > 0.0f) {
        return;
    }
    // Pacified
    if (sFrame - sLastPacifyFrame > 5 * 60 * GDF_FPS) {
        sPacifyCount = 0;
    }
    sPacifyCount++;
    sLastPacifyFrame = sFrame;
    int seconds = (sPacifyCount == 1) ? 30 : (sPacifyCount == 2) ? 45 : 60;
    sBanishedUntil = sFrame + seconds * GDF_FPS;
    sPacifiedFrame = sFrame;
    sResolve = ResolveMax();
    sMenuOpen = false;
    BossGanon_Player_NetPacified();
    nlohmann::json out;
    out["type"] = GDF_PKT_PACIFIED;
    out["by"] = Anchor::Instance->clients.count(payload.value("clientId", 0u))
                    ? Anchor::Instance->clients[payload.value("clientId", 0u)].name
                    : std::string("a Link");
    out["seconds"] = seconds;
    Anchor::Instance->SendJsonToRemote(out);
    Notification::Emit({ .prefix = "Invasion",
                         .message = "You've been pacified! Back in " + std::to_string(seconds) + " seconds." });
}

// ---- The chat ----
struct ChatLine {
    std::string name;
    std::string text;
    Color_RGB8 color;
    u32 frame;
};
static std::deque<ChatLine> sChat;
static bool sChatOpen = false;
static bool sChatFocus = false;
static char sChatInput[160] = "";
#define GDF_CHAT_INPUT_BLOCK 0x4744

static void AddChat(const std::string& name, const std::string& text, Color_RGB8 color) {
    sChat.push_back({ name, text, color, sFrame });
    while (sChat.size() > 8) {
        sChat.pop_front();
    }
}

static void SendChat(const std::string& text) {
    nlohmann::json payload;
    payload["type"] = GDF_PKT_CHAT;
    payload["name"] = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
    payload["text"] = text;
    Anchor::Instance->SendJsonToRemote(payload);
    Color_RGBA8 c = CVarGetColor(CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100, 255 });
    AddChat(CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "You"), text, { c.r, c.g, c.b });
}

// ---- Packets from the others ----
void GdfInvasion_HandlePacket(const nlohmann::json& payload) {
    std::string type = payload.value("type", std::string());
    uint32_t from = payload.value("clientId", 0u);

    if (type == GDF_PKT_HIT) {
        HandleHit(payload);
    } else if (type == GDF_PKT_CURSE) {
        HandleCurse(payload);
    } else if (type == GDF_PKT_CURSE_ACK) {
        int art = payload.value("art", -1);
        if (payload.value("ok", false) && (payload.value("rupees", 0) > 0)) {
            Rupees_ChangeBy((s16)payload.value("rupees", 0)); // his tithe
            Flash("Took " + std::to_string(payload.value("rupees", 0)) + " rupees");
        }
        if (!payload.value("ok", false) && (art >= 0) && (art < GDF_ART_COUNT) && (art != GDF_ART_EARTHQUAKE)) {
            // Refused (that Link was in a cutscene, talking, ...): the magic comes back, and the cooldown resets
            gSaveContext.magic = MIN(gSaveContext.magic + sArts[art].cost, gSaveContext.magicCapacity);
            sArtReady[art] = sFrame;
            if (art == GDF_ART_DARKLINK) {
                sPossessUntil = 0;
            }
            Flash(std::string(sArts[art].name) + " didn't take hold (try again)");
        }
    } else if (type == GDF_PKT_PACIFIED) {
        if (GdfInvasion_IsHero()) {
            sHeroPacifiedFrame = sFrame;
            Notification::Emit({ .prefix = "Invasion",
                                 .message = payload.value("by", std::string("A Link")) +
                                            " pacified Ganondorf! He's gone for " +
                                            std::to_string(payload.value("seconds", 30)) + " seconds." });
        }
    } else if (type == GDF_PKT_REVIVE) {
        if (GdfInvasion_IsHero() && IsDowned()) {
            sReviveCalled = true;
            std::string by = Anchor::Instance->clients.count(from) ? Anchor::Instance->clients[from].name : "A Link";
            Notification::Emit({ .prefix = "Invasion", .message = by + " revived you!" });
        }
    } else if (type == GDF_PKT_VOLLEY) {
        if (GdfInvasion_IsInvader()) {
            Vec3f pos = { payload.value("x", 0.0f), payload.value("y", 0.0f), payload.value("z", 0.0f) };
            BossGanon_Player_BallVolleyed(&pos);
        }
    } else if (type == GDF_PKT_BALL_HIT) {
        if (GdfInvasion_IsInvader()) {
            Vec3f pos = { payload.value("x", 0.0f), payload.value("y", 0.0f), payload.value("z", 0.0f) };
            BossGanon_Player_BallBurstAt(&pos);
        }
    } else if (type == GDF_PKT_SONG) {
        HandleSong(payload);
    } else if (type == GDF_PKT_DOOR_KICK) {
        PlayState* play = gPlayState;
        std::vector<float> d = payload.value("d", std::vector<float>{});
        std::vector<float> f = payload.value("f", std::vector<float>{});
        if ((play != nullptr) && !GdfInvasion_IsInvader() && (d.size() >= 3) && (f.size() >= 3) &&
            (payload.value("scene", -1) == play->sceneNum)) {
            Vec3f door = { d[0], d[1], d[2] };
            Vec3f from = { f[0], f[1], f[2] };
            for (Actor* a = play->actorCtx.actorLists[ACTORCAT_DOOR].head; a != nullptr; a = a->next) {
                if ((a->id == ACTOR_EN_DOOR) && (Math_Vec3f_DistXYZ(&a->world.pos, &door) < 40.0f)) {
                    EnDoor_RemoteKick(a, &from);
                    break;
                }
            }
        }
    } else if (type == GDF_PKT_BOSS_HIT) {
        HandleBossHit(payload);
    } else if (type == GDF_PKT_WORLD_HOLD) {
        HandleWorldHold(payload);
    } else if (type == GDF_PKT_WORLD_HIT) {
        HandleWorldHit(payload);
    } else if (type == GDF_PKT_DUNGEON_DONE) {
        int dungeon = payload.value("dungeon", -1);
        if (GdfInvasion_IsInvader() && (dungeon >= 0) && (dungeon < (int)ARRAY_COUNT(sDungeonExits))) {
            sPendingDungeonExit = dungeon;
        }
    } else if (type == GDF_PKT_CHAT) {
        std::string name = payload.value("name", std::string("?"));
        Color_RGB8 color = { 255, 255, 255 };
        if (Anchor::Instance->clients.count(from)) {
            color = Anchor::Instance->clients[from].color;
            if (GdfInvasion_IsClientGanondorf(from)) {
                color = { 220, 80, 255 };
            }
        }
        AddChat(name, payload.value("text", std::string()), color);
    }
}

// ---- Each frame ----
static void TickInvasion() {
    PlayState* play = gPlayState;

    UpdateCurses();
    UpdateSummons();
    if (GdfInvasion_IsHero() && (play != nullptr)) {
        bool ganonOut = false;
        for (auto& [clientId, c] : sClients) {
            ganonOut |= (c.ganonPuppet != nullptr) && GanonFresh(c) && (c.ganon.form != 0);
            // Pacified: his puppet kneels (his pose), then vanishes in smoke
            if ((sHeroPacifiedFrame != 0) && (sFrame - sHeroPacifiedFrame == 3 * GDF_FPS) && (c.puppet != nullptr)) {
                Vec3f zero = { 0.0f, 0.0f, 0.0f };
                for (int i = 0; i < 16; i++) {
                    Vec3f pos = c.puppet->world.pos;
                    pos.x += Rand_CenteredFloat(100.0f);
                    pos.y += Rand_ZeroFloat(150.0f);
                    pos.z += Rand_CenteredFloat(100.0f);
                    EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 180, 0, 60, 0, 90, 255, 30, 0, 60, 1, 9, false);
                }
                Audio_PlayActorSound2(c.puppet, NA_SE_EN_GANON_LAUGH);
            }
        }
        // Ganon's battle theme while the Invader's Ganon is here
        if (!TestModeOn()) {
            PlayAsGanondorf_SetGanonMusic(play, ganonOut);
        }

        // Reviving a fallen Link: stand by him for a few seconds
        Player* self = GET_PLAYER(play);
        for (auto& [clientId, c] : sClients) {
            bool closeBy = false;
            if ((c.downLeft > 0) && (sFrame - c.downFrame < 10) && !IsDowned() && (self != nullptr) &&
                (gSaveContext.health > 0) && Anchor::Instance->clients.count(clientId) &&
                (Anchor::Instance->clients[clientId].sceneNum == play->sceneNum)) {
                closeBy = Math_Vec3f_DistXYZ(&self->actor.world.pos, &Anchor::Instance->clients[clientId].posRot.pos) <
                       100.0f;
            }
            c.reviveProgress = closeBy ? (c.reviveProgress + 1) : 0;
            if (c.reviveProgress >= GDF_REVIVE_SECONDS * GDF_FPS) {
                c.reviveProgress = 0;
                nlohmann::json out;
                out["type"] = GDF_PKT_REVIVE;
                out["targetClientId"] = clientId;
                Anchor::Instance->SendJsonToRemote(out);
                Audio_PlaySfxGeneral(NA_SE_EV_FIATY_HEAL - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        }
    }
    if (!GdfInvasion_IsInvader() || (play == nullptr) || !GameStarted()) {
        return;
    }
    // His menu closes with anything that takes over the screen
    if (sMenuOpen && ((play->msgCtx.msgMode != MSGMODE_NONE) || Player_InCsMode(play) ||
                      (play->transitionTrigger != TRANS_TRIGGER_OFF))) {
        sMenuOpen = false;
    }
    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    UpdateDungeonExit(play);
    // Eye of Ganon: the camera hangs over the Link he's watching
    if ((sFrame < sEyeUntil) && Anchor::Instance->clients.count(sEyeTarget) &&
        (Anchor::Instance->clients[sEyeTarget].sceneNum == play->sceneNum)) {
        if (sEyeCam < 0) {
            sEyeCam = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, sEyeCam, CAM_STAT_ACTIVE);
        }
        PosRot& pr = Anchor::Instance->clients[sEyeTarget].posRot;
        Vec3f at = pr.pos;
        at.y += 40.0f;
        Vec3f eye = at;
        eye.x -= Math_SinS(pr.rot.y) * 260.0f;
        eye.z -= Math_CosS(pr.rot.y) * 260.0f;
        eye.y += 220.0f;
        Play_CameraSetAtEye(play, sEyeCam, &at, &eye);
    } else if (sEyeCam >= 0) {
        sEyeUntil = 0;
        Play_ClearCamera(play, sEyeCam);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        sEyeCam = -1;
    }
    // Dark Link: the camera follows him (as he is in the Link's game); it ends early if he falls
    Actor* dl = (sFrame < sPossessUntil) ? GhostDarkLink() : nullptr;
    if ((sFrame < sPossessUntil) && (dl == nullptr) && (sFrame - sPossessStart > 4 * GDF_FPS)) {
        sPossessUntil = 0;
        Flash("Dark Link is gone");
    }
    if ((sFrame < sPossessUntil) && (dl != nullptr)) {
        if (sPossessCam < 0) {
            sPossessCam = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, sPossessCam, CAM_STAT_ACTIVE);
            sPossessYaw = dl->shape.rot.y;
            if (Anchor::Instance->clients.count(sPossessTarget)) {
                sPossessYaw = Math_Vec3f_Yaw(&dl->world.pos, &Anchor::Instance->clients[sPossessTarget].posRot.pos);
            }
        }
        // From behind Dark Link toward the Link he's fighting (not his own facing, which turned the camera with every
        // step he took and the stick with it)
        s16 wantYaw = dl->shape.rot.y;
        if (Anchor::Instance->clients.count(sPossessTarget)) {
            Vec3f linkPos = Anchor::Instance->clients[sPossessTarget].posRot.pos;
            if (Math_Vec3f_DistXZ(&dl->world.pos, &linkPos) > 40.0f) {
                wantYaw = Math_Vec3f_Yaw(&dl->world.pos, &linkPos);
            }
        }
        Math_SmoothStepToS(&sPossessYaw, wantYaw, 10, 0x400, 0x20);
        Vec3f at = dl->world.pos;
        at.y += 45.0f;
        Vec3f eye = at;
        eye.x -= Math_SinS(sPossessYaw) * 230.0f;
        eye.z -= Math_CosS(sPossessYaw) * 230.0f;
        eye.y += 90.0f;
        Play_CameraSetAtEye(play, sPossessCam, &at, &eye);
    } else if (sPossessCam >= 0) {
        sPossessUntil = 0;
        Play_ClearCamera(play, sPossessCam);
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        sPossessCam = -1;
    }
    // His magic: the full bar, refilling on its own (faster when he keeps away from the Links)
    if (!gSaveContext.isMagicAcquired || !gSaveContext.isDoubleMagicAcquired) {
        gSaveContext.isMagicAcquired = true;
        gSaveContext.isDoubleMagicAcquired = true;
        gSaveContext.magicLevel = 2;
        gSaveContext.magicFillTarget = 0x60;
        gSaveContext.magicCapacity = 0x60;
    }
    if (!IsBanished() && (gSaveContext.magicState == MAGIC_STATE_IDLE) && !PlayAsGanondorf_IsGanonOut(play) &&
        (gSaveContext.magic < gSaveContext.magicCapacity)) {
        bool farAway = FindTargetLink(play) == 0;
        // About 1.3 a second near the Links, 2 a second away from them
        if ((sFrame % (farAway ? (GDF_FPS / 2) : (GDF_FPS * 3 / 4))) == 0) {
            gSaveContext.magic++;
        }
    }
    // Resolve refills once he hasn't been hit for 5 s
    UpdateResolveMax();
    sResolve = MIN(sResolve, ResolveMax());
    if (!IsBanished() && (sResolve < ResolveMax()) && (sFrame - sLastHitFrame > 5 * GDF_FPS)) {
        sResolve = MIN(ResolveMax(), sResolve + 2.0f / GDF_FPS);
    }
    // Back from banishment: at the area's entrance
    static bool sWasBanished = false;
    if (sWasBanished && !IsBanished()) {
        play->nextEntranceIndex = gSaveContext.entranceIndex;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.respawnFlag = 0;
        gSaveContext.cutsceneIndex = 0;
    }
    sWasBanished = IsBanished();
}

// A Link beat a dungeon's boss: everyone hears, and Ganondorf, if he's in that dungeon, is thrown out of it
static void OnInvasionBossDefeat(void* bossActor) {
    PlayState* play = gPlayState;

    if (!GdfInvasion_IsHero() || (play == nullptr) || !AnchorOn()) {
        return;
    }
    int dungeon = DungeonOf(play->sceneNum);
    if (dungeon < 0) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = GDF_PKT_DUNGEON_DONE;
    payload["dungeon"] = dungeon;
    Anchor::Instance->SendJsonToRemote(payload);
}

// Ganondorf: put outside the dungeon a Link has just beaten (as soon as nothing's in the way)
static void UpdateDungeonExit(PlayState* play) {
    if (sPendingDungeonExit < 0) {
        return;
    }
    if (DungeonOf(play->sceneNum) != sPendingDungeonExit) {
        sPendingDungeonExit = -1; // he isn't in it
        return;
    }
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->msgCtx.msgMode != MSGMODE_NONE)) {
        return;
    }
    play->nextEntranceIndex = sDungeonExits[sPendingDungeonExit].exit;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.respawnFlag = 0;
    gSaveContext.cutsceneIndex = 0;
    sPendingDungeonExit = -1;
    sMenuOpen = false;
    Notification::Emit({ .prefix = "Invasion", .message = "A Link has conquered this dungeon: you're cast out of it." });
}

static void OnInvasionEnemyDefeat(void* actor) {
    if (GdfInvasion_IsHero() && (sLavaUntil != 0)) {
        sLavaUntil = 0; // Lava Floor ends with a defeated enemy
        if (sElementTimerUntil != 0) {
            sElementTimerUntil = 0;
            StopZeldaTimer();
        }
    }
}

static void RegisterInvasionPlay() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnEnemyDefeat>(OnInvasionEnemyDefeat);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnBossDefeat>(OnInvasionBossDefeat);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(TickInvasion);
}

static RegisterShipInitFunc playInitFunc(RegisterInvasionPlay, {});

// ---- HUD parts ----

static ImU32 Col(int r, int g, int b, int a) {
    return IM_COL32(r, g, b, a);
}

static void DrawHudText(ImDrawList* list, float size, ImVec2 pos, ImU32 col, const std::string& text) {
    list->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1, pos.y + 1), Col(0, 0, 0, 140), text.c_str());
    list->AddText(ImGui::GetFont(), size, pos, col, text.c_str());
}

static ImTextureID ArtIcon(int art);

// Ganondorf: his Resolve, his C-button arts, banishment, and messages
static void DrawInvaderHud(ImGuiViewport* vp, float scale) {
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.8f * fmaxf(scale, 1.0f);

    // Resolve, under the hearts and magic
    float x = vp->Pos.x + 26.0f * scale;
    float y = vp->Pos.y + 122.0f * scale; // under his Resolve meter
    float w = 110.0f * scale;
    float h = 6.0f * scale;
    float f = CLAMP(sResolve / GDF_RESOLVE_MAX, 0.0f, 1.0f);
    // (the bar itself is drawn by the game, white, under his magic meter: Interface_DrawMagicBar)
    (void)w;
    (void)f;

    if (sFrame < sVeilUntil) {
        DrawHudText(list, font * 0.8f, ImVec2(x, y + h + 4.0f * scale), Col(200, 160, 255, 200),
                    "Veiled " + std::to_string((sVeilUntil - sFrame) / GDF_FPS + 1) + "s");
    }

    // The arts on his C-buttons, under the buttons
    float cx = vp->Pos.x + vp->Size.x - 190.0f * scale;
    float cy = vp->Pos.y + 70.0f * scale;
    for (int slot = 0; slot < 3; slot++) {
        int art = SlotArt(slot);
        if ((art < 0) || (art >= GDF_ART_COUNT)) {
            continue;
        }
        const DarkArt& a = sArts[art];
        bool cooling = sFrame < sArtReady[art];
        bool affordable = gSaveContext.magic >= a.cost;
        std::string text = std::string(sCNames[slot]) + ": " + a.name + " (" + std::to_string(a.cost) + ")";
        if (cooling) {
            text += "  " + std::to_string((sArtReady[art] - sFrame) / GDF_FPS + 1) + "s";
        }
        ImU32 c = (!a.ready || cooling || !affordable) ? Col(170, 160, 180, 120) : Col(225, 190, 255, 200);
        ImTextureID icon = ArtIcon(art);
        if (icon != (ImTextureID)0) {
            list->AddImage(icon, ImVec2(cx - font * 1.25f, cy - font * 0.1f), ImVec2(cx - font * 0.15f, cy + font),
                           ImVec2(0, 0), ImVec2(1, 1),
                           (!a.ready || cooling || !affordable) ? Col(130, 120, 140, 170) : Col(255, 255, 255, 255));
        }
        DrawHudText(list, font * 0.85f, ImVec2(cx, cy), c, text);
        cy += font;
    }

    // Banished
    if (IsBanished()) {
        list->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), Col(10, 0, 20, 150));
        std::string t = "Pacified... back in " + std::to_string((sBanishedUntil - sFrame) / GDF_FPS + 1) + " s";
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(font * 1.6f, FLT_MAX, 0.0f, t.c_str());
        DrawHudText(list, font * 1.6f,
                 ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, vp->Pos.y + (vp->Size.y - sz.y) * 0.5f),
                 Col(230, 200, 255, 230), t);
    }

    // A message about his last cast
    if (sFrame < sCastMessageUntil) {
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, sCastMessage.c_str());
        DrawHudText(list, font, ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, vp->Pos.y + vp->Size.y * 0.72f),
                 Col(230, 210, 255, 220), sCastMessage);
    }
}

// Each Dark Art's icon (our own 16x16 pixel art, GanondorfArtIcons.h), uploaded once and drawn sharp
static ImTextureID ArtIcon(int art) {
    if ((art < 0) || (art >= (int)(sizeof(sGdfArtIcons) / sizeof(sGdfArtIcons[0])))) {
        return (ImTextureID)0;
    }
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui == nullptr) {
        return (ImTextureID)0;
    }
    std::string name = "GDF_ART_ICON_" + std::to_string(art);
    if (!gui->HasTextureByName(name)) {
        std::vector<uint8_t> pixels(16 * 16 * 4, 0);
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                char key = sGdfArtIcons[art][y][x];
                for (const GdfIconColor& c : sGdfIconPalette) {
                    if (c.key == key) {
                        uint8_t* px = &pixels[(y * 16 + x) * 4];
                        px[0] = c.r;
                        px[1] = c.g;
                        px[2] = c.b;
                        px[3] = 255;
                        break;
                    }
                }
            }
        }
        Fast::Texture tex;
        tex.Type = Fast::TextureType::RGBA32bpp;
        tex.Width = 16;
        tex.Height = 16;
        tex.ImageDataSize = (uint32_t)pixels.size();
        tex.ImageData = pixels.data();
        gui->LoadGuiTexture(name, tex);
        tex.ImageData = nullptr; // ours, not the texture's to free
    }
    return gui->GetTextureByName(name);
}

// Ganondorf's Dark Arts menu
static void DrawArtsMenu(ImGuiViewport* vp, float scale) {
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.75f * fmaxf(scale, 1.0f);
    float cellW = 50.0f * scale;
    float cellH = 50.0f * scale;
    float gap = 6.0f * scale;
    float gridW = GDF_ART_COLS * cellW + (GDF_ART_COLS - 1) * gap;
    float gridH = GDF_ART_ROWS * cellH + (GDF_ART_ROWS - 1) * gap;
    float x0 = vp->Pos.x + (vp->Size.x - gridW) * 0.5f;
    float y0 = vp->Pos.y + (vp->Size.y - gridH) * 0.5f;
    static const char* sRows[GDF_ART_ROWS] = { "Obstacles", "Monsters", "Curses", "Curses / his own", "More",
                                                    "Elements" };

    list->AddRectFilled(ImVec2(x0 - 70 * scale, y0 - 46 * scale),
                        ImVec2(x0 + gridW + 110 * scale, y0 + gridH + 64 * scale), Col(14, 4, 24, 225), 10.0f * scale);
    list->AddRect(ImVec2(x0 - 70 * scale, y0 - 46 * scale), ImVec2(x0 + gridW + 110 * scale, y0 + gridH + 64 * scale),
                  Col(150, 110, 60, 230), 10.0f * scale, 0, 2.5f * scale);
    DrawHudText(list, font * 1.4f, ImVec2(x0 - 60 * scale, y0 - 40 * scale), Col(220, 170, 255, 255), "Dark Arts");
    DrawHudText(list, font * 0.9f, ImVec2(x0 + 60 * scale, y0 - 34 * scale), Col(200, 190, 210, 190),
             "D-Pad: choose   C: put on that button   START/B: close");

    for (int i = 0; i < GDF_ART_COUNT; i++) {
        const DarkArt& a = sArts[i];
        int col = i % GDF_ART_COLS;
        if (a.name[0] == '\0') {
            ImVec2 e0(x0 + col * (cellW + gap), y0 + (i / GDF_ART_COLS) * (cellH + gap));
            list->AddRectFilled(e0, ImVec2(e0.x + cellW, e0.y + cellH), Col(28, 20, 36, 180), 6.0f * scale);
            if (i == sCursor) {
                list->AddRect(e0, ImVec2(e0.x + cellW, e0.y + cellH), Col(255, 220, 120, 200), 4.0f, 0, 2.0f * scale);
            }
            continue;
        }
        int row = i / GDF_ART_COLS;
        ImVec2 p0(x0 + col * (cellW + gap), y0 + row * (cellH + gap));
        ImVec2 p1(p0.x + cellW, p0.y + cellH);
        bool selected = i == sCursor;
        bool affordable = gSaveContext.magic >= a.cost;
        // A sunken stone slot, as on the item screen, with the art's icon in it
        list->AddRectFilled(p0, p1, Col(40, 28, 52, 230), 6.0f * scale);
        list->AddRect(ImVec2(p0.x + 1, p0.y + 1), ImVec2(p1.x - 1, p1.y - 1), Col(120, 90, 150, 200), 6.0f * scale, 0,
                      1.5f * scale);
        ImTextureID icon = ArtIcon(i);
        if (icon != (ImTextureID)0) {
            float m = 7.0f * scale;
            ImU32 tint = !a.ready ? Col(90, 90, 100, 160) : affordable ? Col(255, 255, 255, 255) : Col(150, 140, 160, 220);
            list->AddImage(icon, ImVec2(p0.x + m, p0.y + m), ImVec2(p1.x - m, p1.y - m), ImVec2(0, 0), ImVec2(1, 1), tint);
        }
        // Its cost, where an item's ammo count would be
        std::string cost = std::to_string(a.cost);
        ImVec2 csz = ImGui::GetFont()->CalcTextSizeA(font * 0.8f, FLT_MAX, 0.0f, cost.c_str());
        DrawHudText(list, font * 0.8f, ImVec2(p1.x - csz.x - 3 * scale, p1.y - csz.y - 1 * scale),
                    affordable ? Col(140, 255, 140, 255) : Col(255, 120, 120, 255), cost);
        for (int slot = 0; slot < 3; slot++) {
            if (SlotArt(slot) == i) {
                static const char* sTags[3] = { "<", "v", ">" };
                DrawHudText(list, font * 0.9f, ImVec2(p0.x + 3 * scale, p0.y + 1 * scale), Col(255, 220, 60, 255),
                            sTags[slot]);
            }
        }
        if (selected) {
            // The item screen's cursor: a bright frame with corners
            ImU32 cur = Col(255, 230, 120, 255);
            list->AddRect(ImVec2(p0.x - 2 * scale, p0.y - 2 * scale), ImVec2(p1.x + 2 * scale, p1.y + 2 * scale), cur,
                          7.0f * scale, 0, 2.5f * scale);
        }
    }
    // Rows and the selected art
    for (int row = 0; row < GDF_ART_ROWS; row++) {
        DrawHudText(list, font * 0.7f, ImVec2(x0 + gridW + 4 * scale, y0 + row * (cellH + gap) + 4 * scale),
                 Col(180, 160, 200, 160), sRows[row]);
    }
    const DarkArt& sel = sArts[sCursor];
    if (sel.name[0] != '\0') {
        ImVec2 nsz = ImGui::GetFont()->CalcTextSizeA(font * 1.25f, FLT_MAX, 0.0f, sel.name);
        DrawHudText(list, font * 1.25f, ImVec2(x0 + (gridW - nsz.x) * 0.5f, y0 + gridH + 6 * scale),
                    Col(255, 235, 200, 255), sel.name);
    }
    std::string desc = std::string(sel.desc) + "  (" + std::to_string(sel.cost) + " magic, " +
                       std::to_string(sel.cooldown) + " s)" + (sel.ready ? "" : "  - coming later");
    DrawHudText(list, font * 0.9f, ImVec2(x0 - 60 * scale, y0 + gridH + 26 * scale), Col(220, 210, 230, 230), desc);
    DrawHudText(list, font * 0.85f, ImVec2(x0 - 60 * scale, y0 + gridH + 42 * scale), Col(255, 180, 120, 200),
                "Kokiri Forest is at peace for a young Link without both sword and shield.");
}

// A Link: what Ganondorf has cursed him with, and the gloom
static void DrawHeroHud(ImGuiViewport* vp, float scale) {
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.8f * fmaxf(scale, 1.0f);

    // Revival: this Link down, or another Link down nearby
    {
        std::string t;
        if (IsDowned()) {
            int left = MAX(0, (int)(GDF_DOWN_SECONDS - (sFrame - sDownedSince) / GDF_FPS));
            t = "You're down! Another Link can revive you by standing at your side: " + std::to_string(left) + " s";
        } else {
            for (auto& [clientId, c] : sClients) {
                if ((c.downLeft > 0) && (sFrame - c.downFrame < 10) && Anchor::Instance->clients.count(clientId)) {
                    std::string who = Anchor::Instance->clients[clientId].name;
                    t = (c.reviveProgress > 0)
                            ? ("Reviving " + who + "... " +
                               std::to_string(c.reviveProgress * 100 / (GDF_REVIVE_SECONDS * GDF_FPS)) + "%")
                            : (who + " is down! Stand at their side to revive them (" + std::to_string(c.downLeft) +
                               " s)");
                    break;
                }
            }
        }
        if (!t.empty()) {
            ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(font * 1.1f, FLT_MAX, 0.0f, t.c_str());
            DrawHudText(list, font * 1.1f, ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, vp->Pos.y + vp->Size.y * 0.3f),
                        Col(255, 230, 150, 235), t);
        }
    }

    // Ganondorf's Resolve: a white bar over his head when he's been hurt, fading away a few seconds later
    PlayState* play = gPlayState;
    for (auto& [clientId, c] : sClients) {
        Actor* him = (c.ganonPuppet != nullptr) ? c.ganonPuppet : c.puppet;
        u32 since = sFrame - c.hurtFrame;
        if ((play == nullptr) || !c.isGanondorf || (him == nullptr) || (c.resolve < 0.0f) || (c.resolveMax <= 0.0f) ||
            (c.hurtFrame == 0) || (since > 4 * GDF_FPS)) {
            continue;
        }
        int alpha = (since < 3 * GDF_FPS) ? 255 : 255 - (int)((since - 3 * GDF_FPS) * 255 / GDF_FPS);
        // Over his head, on the screen
        Vec3f top = him->world.pos;
        top.y += (c.ganonPuppet != nullptr) ? 290.0f : 140.0f;
        MtxF* m = &play->viewProjectionMtxF;
        float cx = m->xx * top.x + m->xy * top.y + m->xz * top.z + m->xw;
        float cy = m->yx * top.x + m->yy * top.y + m->yz * top.z + m->yw;
        float cw = m->wx * top.x + m->wy * top.y + m->wz * top.z + m->ww;
        if (cw <= 1.0f) {
            continue; // behind the camera
        }
        float sx = vp->Pos.x + (cx / cw * 0.5f + 0.5f) * vp->Size.x;
        float sy = vp->Pos.y + (0.5f - cy / cw * 0.5f) * vp->Size.y;
        float w = 90.0f * scale * (c.resolveMax / GDF_NET_RESOLVE_FULL);
        float h = 6.0f * scale;
        float bx = sx - w * 0.5f;
        float by = sy - h;
        float f = CLAMP(c.resolve / c.resolveMax, 0.0f, 1.0f);
        list->AddRectFilled(ImVec2(bx - 2.0f * scale, by - 2.0f * scale), ImVec2(bx + w + 2.0f * scale, by + h + 2.0f * scale),
                            Col(20, 10, 30, alpha * 200 / 255), 3.0f * scale);
        list->AddRectFilled(ImVec2(bx, by), ImVec2(bx + w * f, by + h), Col(245, 245, 245, alpha * 235 / 255),
                            2.0f * scale);
        list->AddRect(ImVec2(bx - 2.0f * scale, by - 2.0f * scale), ImVec2(bx + w + 2.0f * scale, by + h + 2.0f * scale),
                      Col(200, 170, 120, alpha * 220 / 255), 3.0f * scale, 0, 1.2f * scale);
    }

    if ((sHauntUntil != 0) && (sFrame < sHauntUntil)) {
        // The Haunting: darkness closing in from the edges
        float w = vp->Size.x;
        float h = vp->Size.y;
        ImVec2 o = vp->Pos;
        ImU32 dark = Col(0, 0, 0, 235);
        ImU32 clear = Col(0, 0, 0, 0);
        list->AddRectFilledMultiColor(o, ImVec2(o.x + w * 0.32f, o.y + h), dark, clear, clear, dark);
        list->AddRectFilledMultiColor(ImVec2(o.x + w * 0.68f, o.y), ImVec2(o.x + w, o.y + h), clear, dark, dark,
                                      clear);
        list->AddRectFilledMultiColor(o, ImVec2(o.x + w, o.y + h * 0.3f), dark, dark, clear, clear);
        list->AddRectFilledMultiColor(ImVec2(o.x, o.y + h * 0.7f), ImVec2(o.x + w, o.y + h), clear, clear, dark,
                                      dark);
        list->AddRectFilled(o, ImVec2(o.x + w, o.y + h), Col(0, 0, 0, 90));
    }
    float y = vp->Pos.y + vp->Size.y * 0.16f;
    for (int i = 0; i < 8; i++) {
        if (!sCurses[i].active) {
            continue;
        }
        std::string t = std::string(sArts[GDF_FIRST_CURSE + i].name) + "  " +
                        std::to_string((sCurses[i].until - sFrame) / GDF_FPS + 1) + "s";
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, t.c_str());
        DrawHudText(list, font, ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, y), Col(220, 120, 255, 220), t);
        y += font * 1.1f;
    }
}

// The chat: T to talk
static void DrawChat(ImGuiViewport* vp, float scale) {
    if (!CVarGetInteger(CVAR_INVASION("Chat"), 1) || !Anchor::Instance->isConnected) {
        if (sChatOpen) {
            sChatOpen = false;
            ControllerUnblockGameInput(GDF_CHAT_INPUT_BLOCK);
        }
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (!sChatOpen && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_T, false)) {
        sChatOpen = true;
        sChatFocus = true;
        sChatInput[0] = '\0';
        ControllerBlockGameInput(GDF_CHAT_INPUT_BLOCK);
    }

    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.85f * fmaxf(scale, 1.0f);
    float x = vp->Pos.x + 16.0f * scale;
    float y = vp->Pos.y + vp->Size.y * 0.62f;
    auto lineAlpha = [](const ChatLine& line) {
        u32 age = sFrame - line.frame;
        return sChatOpen ? 230 : (age < 10 * GDF_FPS) ? 220 : (age < 12 * GDF_FPS) ? 220 - (int)(age - 10 * GDF_FPS) * 5 : 0;
    };
    // Long messages wrap onto more rows to fit the screen (the first row after the name, the rest from the left)
    ImFont* imFont = ImGui::GetFont();
    float maxW = vp->Size.x - 32.0f * scale;
    float lineH = font * 1.15f;
    auto wrap = [&](const std::string& text, float firstW) {
        std::vector<std::string> rows;
        const char* p = text.c_str();
        const char* end = p + text.size();
        float w = firstW;
        while (p < end) {
            const char* cut = imFont->CalcWordWrapPositionA(font / imFont->FontSize, p, end, fmaxf(w, font));
            if (cut == p) {
                cut = p + 1; // a single character wider than the row
            }
            rows.emplace_back(p, cut);
            p = cut;
            while ((p < end) && (*p == ' ')) {
                p++;
            }
            w = maxW;
        }
        if (rows.empty()) {
            rows.emplace_back();
        }
        return rows;
    };
    struct ShownLine {
        const ChatLine* line;
        int alpha;
        float nameW;
        std::vector<std::string> rows;
    };
    std::vector<ShownLine> shown;
    float boxW = 0.0f;
    int rowCount = 0;
    int boxAlpha = 0;
    for (const ChatLine& line : sChat) {
        int alpha = lineAlpha(line);
        if (alpha <= 0) {
            continue;
        }
        std::string name = line.name + ": ";
        float nameW = imFont->CalcTextSizeA(font, FLT_MAX, 0.0f, name.c_str()).x;
        ShownLine sl = { &line, alpha, nameW, wrap(line.text, maxW - nameW) };
        for (size_t r = 0; r < sl.rows.size(); r++) {
            float rw = imFont->CalcTextSizeA(font, FLT_MAX, 0.0f, sl.rows[r].c_str()).x + ((r == 0) ? nameW : 0.0f);
            boxW = fmaxf(boxW, rw);
        }
        rowCount += (int)sl.rows.size();
        boxAlpha = std::max(boxAlpha, alpha);
        shown.push_back(std::move(sl));
    }
    // Kept above the bottom of the screen however many rows there are
    y = fminf(y, vp->Pos.y + vp->Size.y * 0.84f - rowCount * lineH);
    // A dark box behind the lines shown, so they read over any scene; it fades with them
    if (rowCount > 0) {
        float pad = 6.0f * scale;
        list->AddRectFilled(ImVec2(x - pad, y - pad), ImVec2(x + boxW + pad, y + rowCount * lineH + pad * 0.6f),
                            Col(0, 0, 0, boxAlpha * 200 / 230), 4.0f * scale);
    }
    for (const ShownLine& sl : shown) {
        const ChatLine& line = *sl.line;
        // The name with an outline that sets it off: white around a dark name (Black Link), black around a pale one
        // (White Link)
        {
            std::string name = line.name + ": ";
            int lum = (line.color.r * 299 + line.color.g * 587 + line.color.b * 114) / 1000;
            ImU32 edge = (lum < 110) ? Col(255, 255, 255, sl.alpha) : Col(0, 0, 0, sl.alpha);
            float o = fmaxf(1.0f, 1.2f * scale);
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if ((dx != 0) || (dy != 0)) {
                        list->AddText(imFont, font, ImVec2(x + dx * o, y + dy * o), edge, name.c_str());
                    }
                }
            }
            list->AddText(imFont, font, ImVec2(x, y), Col(line.color.r, line.color.g, line.color.b, sl.alpha),
                          name.c_str());
        }
        for (size_t r = 0; r < sl.rows.size(); r++) {
            DrawHudText(list, font, ImVec2(x + ((r == 0) ? sl.nameW : 0.0f), y), Col(240, 240, 240, sl.alpha),
                        sl.rows[r]);
            y += lineH;
        }
    }

    if (!sChatOpen) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(x, y + 4.0f));
    ImGui::SetNextWindowSize(ImVec2(fminf(vp->Size.x * 0.45f, 520.0f * scale), 0));
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("##GdfChat", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoDocking);
    if (sChatFocus) {
        ImGui::SetKeyboardFocusHere();
        sChatFocus = false;
    }
    ImGui::SetNextItemWidth(-1);
    bool send = ImGui::InputText("##GdfChatInput", sChatInput, sizeof(sChatInput),
                                 ImGuiInputTextFlags_EnterReturnsTrue);
    bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    ImGui::End();
    if (send || cancel) {
        if (send && (sChatInput[0] != '\0')) {
            SendChat(sChatInput);
        }
        sChatOpen = false;
        ControllerUnblockGameInput(GDF_CHAT_INPUT_BLOCK);
    }
}

void GdfInvasionHud::Draw() {
    PlayState* play = gPlayState;

    bool test = TestModeOn();
    if (!test && ((Anchor::Instance == nullptr) || !Anchor::Instance->isEnabled)) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float scale = vp->Size.y / 480.0f;

    if (!test) {
        DrawChat(vp, scale);
    }
    if ((play == nullptr) || (!test && !Anchor::Instance->IsSaveLoaded()) || (play->pauseCtx.state != 0) ||
        (test && (gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2))) {
        return;
    }
    if (test) {
        // Test mode: his HUD and menu, and what his arts do to "Link" (himself)
        DrawInvaderHud(vp, scale);
        DrawHeroHud(vp, scale);
        if (sMenuOpen) {
            DrawArtsMenu(vp, scale);
        }
        DrawHudText(ImGui::GetForegroundDrawList(vp), ImGui::GetFontSize() * 0.75f,
                    ImVec2(vp->Pos.x + 8 * scale, vp->Pos.y + vp->Size.y - 22 * scale), Col(255, 200, 120, 200),
                    "Invasion test mode (offline): Dark Arts land on yourself");
        return;
    }
    if (GdfInvasion_IsInvader() && GameStarted()) {
        DrawInvaderHud(vp, scale);
        if (sMenuOpen) {
            DrawArtsMenu(vp, scale);
        }
    } else if (GdfInvasion_IsHero()) {
        DrawHeroHud(vp, scale);
    }
    DrawLinkLocator();
}

// =================================================================================================
// The throat grab on a Link: Ganondorf's game decides it (he grabs that Link's dummy) and sends where his fist is
// with his pose every frame; the Link's own game hangs him there (no control) and, on the throw, knocks him down
// ahead of Ganondorf, unhurt. Not during the truce.
// =================================================================================================

struct NetGrabOut {
    uint32_t target = 0;
    u8 state = 0; // 1 held, 2 thrown, 0 let go
    Vec3f hand = { 0, 0, 0 };
    float neck = 40.0f;
    s16 yaw = 0;
    u32 frame = 0;
    int count = 0; // which grab this is (so a throw is acted on once)
};
static NetGrabOut sGrabOut;

extern "C" u32 GdfInvasion_DummyClientId(Actor* actor) {
    if ((Anchor::Instance == nullptr) || (actor == nullptr) || (actor->update != DummyPlayer_Update)) {
        return 0;
    }
    return Anchor::Instance->GetDummyPlayerClientId(actor);
}

extern "C" s32 GdfInvasion_CanGrabLink(uint32_t clientId) {
    return GdfInvasion_IsInvader() && GameStarted() && !GdfInvasion_LinkAtPeace(clientId);
}

extern "C" s32 GdfInvasion_CanGrabLinks(void) {
    return GdfInvasion_IsInvader() && GameStarted();
}

extern "C" void GdfInvasion_NetGrabHold(u32 clientId, Vec3f* hand, f32 neck, s16 yaw) {
    if ((sGrabOut.target != clientId) || (sGrabOut.state != 1)) {
        sGrabOut.count++;
    }
    sGrabOut.target = clientId;
    sGrabOut.state = 1;
    sGrabOut.hand = *hand;
    sGrabOut.neck = neck;
    sGrabOut.yaw = yaw;
    sGrabOut.frame = sFrame;
}

extern "C" void GdfInvasion_NetGrabEnd(u32 clientId, s32 thrown, s16 yaw) {
    sGrabOut.target = clientId;
    sGrabOut.state = thrown ? 2 : 0;
    sGrabOut.yaw = yaw;
    sGrabOut.frame = sFrame;
}

// Added to his pose packet: the grab, for a short while after it changes (so a throw isn't missed)
static void WriteGrab(nlohmann::json& g) {
    if ((sGrabOut.target == 0) || (sFrame - sGrabOut.frame > 10)) {
        return;
    }
    g["gg"] = { sGrabOut.target, sGrabOut.state, sGrabOut.hand.x, sGrabOut.hand.y, sGrabOut.hand.z, sGrabOut.neck,
                sGrabOut.yaw, sGrabOut.count };
}

// ---- The Link being held ----
struct HeldBy {
    uint32_t from = 0;
    bool held = false;
    Vec3f hand = { 0, 0, 0 };
    float neck = 40.0f;
    u32 lastFrame = 0;
    int count = -1;
    int thrownCount = -1;
};
static HeldBy sHeld;

static void ReadGrab(uint32_t from, const nlohmann::json& g) {
    PlayState* play = gPlayState;

    if (!g.contains("gg") || !GdfInvasion_IsHero() || (play == nullptr)) {
        return;
    }
    std::vector<float> v = g["gg"].get<std::vector<float>>();
    if ((v.size() < 8) || ((uint32_t)v[0] != Anchor::Instance->ownClientId)) {
        return;
    }
    int state = (int)v[1];
    int count = (int)v[7];
    if (state == 1) {
        if (!sHeld.held) {
            Audio_PlaySfxGeneral(NA_SE_VO_LI_DAMAGE_S, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        sHeld.held = true;
        sHeld.from = from;
        sHeld.hand = { v[2], v[3], v[4] };
        sHeld.neck = v[5];
        sHeld.lastFrame = sFrame;
        sHeld.count = count;
    } else if (sHeld.held && (sHeld.from == from)) {
        sHeld.held = false;
        if ((state == 2) && (sHeld.thrownCount != count)) {
            // Thrown: knocked down, flying away from him, no damage
            Player* player = GET_PLAYER(play);
            sHeld.thrownCount = count;
            player->actor.world.pos = { v[2], v[3] - v[5], v[4] };
            Actor_SetPlayerKnockbackLargeNoDamage(play, &player->actor, 12.0f, (s16)v[6], 9.0f);
        }
    }
}

// Every frame: while held, he hangs from the fist
static void UpdateHeld() {
    PlayState* play = gPlayState;

    if (!sHeld.held) {
        return;
    }
    if ((play == nullptr) || !GdfInvasion_IsHero() || (sFrame - sHeld.lastFrame > 15)) {
        sHeld.held = false; // Ganondorf's gone (or the game left the area)
        return;
    }
    Player* player = GET_PLAYER(play);
    player->actor.world.pos = { sHeld.hand.x, sHeld.hand.y - sHeld.neck, sHeld.hand.z };
    player->actor.velocity.y = 0.0f;
    player->linearVelocity = 0.0f;
}

static void OnHeroPlayerUpdate() {
    UpdateHeld();
}

static void RegisterInvasionGrab() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnHeroPlayerUpdate);
}

static RegisterShipInitFunc grabInitFunc(RegisterInvasionGrab, {});

bool GdfInvasion_IsHeld() {
    return sHeld.held;
}


// =================================================================================================
// Ganondorf sees his own Dark Arts: a Link's game sends what his arts made there (kind, position, facing) with its
// pose; Ganondorf's game shows copies in the same places. The copies come up as themselves for a moment, then only
// follow the Link's game (they don't fight). Bombs explode when the Link's do.
// =================================================================================================

static void WriteMirror(nlohmann::json& payload) {
    nlohmann::json list = nlohmann::json::array();

    for (Mirrored& m : sMirrored) {
        if ((m.actor == nullptr) || !ActorAlive(m.actor)) {
            m.actor = nullptr;
            continue;
        }
        Actor* a = m.actor;
        list.push_back({ m.key, a->id, a->params, a->world.pos.x, a->world.pos.y, a->world.pos.z, a->shape.rot.y });
    }
    if (!list.empty()) {
        payload["gm"] = list;
    }
}

// ---- Smoothing: copies glide toward where the other game says they are, instead of jumping with each update ----
// Each update gives a target and, from the last two, a speed; between updates the target runs on at that speed for a
// few frames, and the copy closes most of the gap every frame. Far off (a warp, a respawn) it's put there at once.
struct Smooth {
    Vec3f target = { 0.0f, 0.0f, 0.0f };
    Vec3f vel = { 0.0f, 0.0f, 0.0f };
    Vec3s rot = { 0, 0, 0 };
    u32 heard = 0;
    bool init = false;
};
static std::map<Actor*, Smooth> sSmooth;

static void SmoothNote(Actor* a, Vec3f pos, Vec3s rot) {
    Smooth& sm = sSmooth[a];
    if (sm.init) {
        float dt = (float)CLAMP((int)(sFrame - sm.heard), 1, 6);
        Vec3f v = { (pos.x - sm.target.x) / dt, (pos.y - sm.target.y) / dt, (pos.z - sm.target.z) / dt };
        if (sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) > 60.0f) {
            v = { 0.0f, 0.0f, 0.0f }; // a jump, not a speed
        }
        sm.vel = v;
    } else {
        a->world.pos = pos;
        a->shape.rot = rot;
    }
    sm.target = pos;
    sm.rot = rot;
    sm.heard = sFrame;
    sm.init = true;
}

static s16 SmoothAngle(s16 cur, s16 target) {
    s16 d = (s16)(target - cur);
    return (s16)(cur + d / 2);
}

static void SmoothApply(Actor* a, bool allAxes) {
    auto it = sSmooth.find(a);
    if (it == sSmooth.end()) {
        return;
    }
    Smooth& sm = it->second;
    float ahead = (float)MIN(sFrame - sm.heard, 4u);
    Vec3f goal = { sm.target.x + sm.vel.x * ahead, sm.target.y + sm.vel.y * ahead, sm.target.z + sm.vel.z * ahead };
    float d = Math_Vec3f_DistXYZ(&goal, &a->world.pos);
    if (d > 250.0f) {
        a->world.pos = goal;
    } else {
        a->world.pos.x += (goal.x - a->world.pos.x) * 0.45f;
        a->world.pos.y += (goal.y - a->world.pos.y) * 0.45f;
        a->world.pos.z += (goal.z - a->world.pos.z) * 0.45f;
    }
    a->shape.rot.y = SmoothAngle(a->shape.rot.y, sm.rot.y);
    a->world.rot.y = a->shape.rot.y;
    if (allAxes) {
        a->shape.rot.x = SmoothAngle(a->shape.rot.x, sm.rot.x);
        a->shape.rot.z = SmoothAngle(a->shape.rot.z, sm.rot.z);
        a->world.rot.x = a->shape.rot.x;
        a->world.rot.z = a->shape.rot.z;
    }
}

struct Ghost {
    Actor* actor = nullptr;
    s16 id = 0;
    u32 born = 0;
    u32 seen = 0;
    bool world = false; // the host Link's (the shared world), not one of Ganondorf's arts
    u8 hostHp = 0;      // its health in the host's game
    u8 lastHp = 255;    // its health here, last frame
    u8 category = 0;
    u32 hurt = 0;       // when it last lost health here
};
static u32 sWorldHeard = 0; // when the room's host last sent his world
static std::map<std::pair<uint32_t, int>, u32> sGhostRetry; // couldn't be made yet: try again after this frame
#define GDF_WORLD_KEY 1000000 // the shared world's keys, apart from the arts' ones
static void SendWorldHit(uint32_t host, int key, u8 hp);
static std::map<std::pair<uint32_t, int>, Ghost> sGhosts;
static std::map<std::pair<uint32_t, int>, bool> sGhostsGone; // killed in his game: not brought back

static void ReadMirror(uint32_t from, const nlohmann::json& list) {
    PlayState* play = gPlayState;

    if ((play == nullptr) || !list.is_array()) {
        return;
    }
    for (const auto& e : list) {
        if (!e.is_array() || (e.size() < 7)) {
            continue;
        }
        auto key = std::make_pair(from, e[0].get<int>());
        if (sGhostsGone.count(key)) {
            continue;
        }
        Ghost& g = sGhosts[key];
        if ((g.actor != nullptr) && (g.id != (s16)e[1].get<int>())) {
            Actor_Kill(g.actor); // not the same one after all (a host who came in fresh): make it anew
            g.actor = nullptr;
        }
        Vec3f pos = { e[3].get<float>(), e[4].get<float>(), e[5].get<float>() };
        s16 rotY = (s16)e[6].get<int>();
        if (g.actor == nullptr) {
            g.id = (s16)e[1].get<int>();
            g.actor = Actor_Spawn(&play->actorCtx, play, g.id, pos.x, pos.y, pos.z, 0, rotY, 0, (s16)e[2].get<int>());
            g.born = sFrame;
            if (g.actor == nullptr) {
                sGhostsGone[key] = true;
                sGhosts.erase(key);
                continue;
            }
            if (g.id == ACTOR_EN_BOM) {
                ((EnBom*)g.actor)->timer = 0x7FFF; // goes off with the Link's
            }
        }
        g.seen = sFrame;
        if (ActorAlive(g.actor)) {
            SmoothNote(g.actor, pos, { g.actor->shape.rot.x, rotY, g.actor->shape.rot.z });
        }
    }
}

static bool TakeSlain(Actor* actor);
static void UpdateGhosts() {
    PlayState* play = gPlayState;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (auto it = sGhosts.begin(); it != sGhosts.end();) {
        Ghost& g = it->second;
        bool alive = ActorAlive(g.actor);
        if (!alive) {
            // Killed here (in a fight, or a pickup taken): gone for everyone. One that only took itself away in this
            // game (an NPC not meant for this player's age or time of day) stays in everyone else's.
            bool killed = ((g.category == ACTORCAT_ENEMY) && ((g.lastHp == 0) || (sFrame - g.hurt < 60))) ||
                          TakeSlain(g.actor); // (or an NPC Ganondorf cut down)
            bool taken = (g.id == ACTOR_EN_ITEM00) && (sFrame - g.born > 2);
            if (g.world && (killed || taken)) {
                SendWorldHit(it->first.first, it->first.second, 0);
            }
            sGhostsGone[it->first] = true;
            it = sGhosts.erase(it);
            continue;
        }
        if (g.actor->colChkInfo.health < g.lastHp) {
            g.hurt = sFrame;
        }
        g.lastHp = g.actor->colChkInfo.health;
        g.category = g.actor->category;
        if (g.world && (g.actor->colChkInfo.health < g.hostHp)) {
            // Hurt here: hurt in the host's game too
            g.hostHp = g.actor->colChkInfo.health;
            SendWorldHit(it->first.first, it->first.second, g.hostHp);
        }
        if (g.world && (play != nullptr) && (sFrame - g.seen > 30) && (sFrame - sWorldHeard > 30)) {
            // The host has gone quiet (paused, loading, a lag spike): hold it where it is, it isn't gone. After
            // 10 seconds of quiet it becomes this game's own, so the room is never left empty.
            if (sFrame - g.seen > 200) {
                g.actor->room = play->roomCtx.curRoom.num;
                sGhostsGone[it->first] = true; // (not made a second time if the host comes back)
                it = sGhosts.erase(it);
                continue;
            }
            g.actor->freezeTimer = 2;
            ++it;
            continue;
        }
        if ((play == nullptr) || (sFrame - g.seen > (g.world ? 30u : 10u))) {
            // Gone in the Link's game
            if (g.id == ACTOR_EN_BOM) {
                Vec3f pos = g.actor->world.pos;
                EffectSsBomb2_SpawnLayered(play, &pos, &zero, &zero, 100, 19);
                SoundSource_PlaySfxAtFixedWorldPos(play, &pos, 30, NA_SE_IT_BOMB_EXPLOSION);
            }
            Actor_Kill(g.actor);
            sGhostsGone[it->first] = true;
            it = sGhosts.erase(it);
            continue;
        }
        // They run as themselves (animating) and follow the Link's game: they can't hurt him or go after him (monsters
        // in his game leave him alone), so nothing needs freezing. Only the bombs wait for the Link's to go off.
        if (g.id == ACTOR_EN_BOM) {
            g.actor->freezeTimer = 2;
        }
        if (g.actor != BossGanon_Player_GetGrabbedActor()) {
            SmoothApply(g.actor, false);
        }
        ++it;
    }
    if (sGhostsGone.size() > 2000) {
        sGhostsGone.clear();
    }
}

static void RegisterInvasionGhosts() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(UpdateGhosts);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sGhosts.clear();
        sGhostRetry.clear();
    });
}

static RegisterShipInitFunc ghostInitFunc(RegisterInvasionGhosts, {});

extern "C" s32 GdfInvasion_IsSummoned(Actor* actor) {
    for (Actor* a : sSummoned) {
        if (a == actor) {
            return true;
        }
    }
    for (auto& [key, g] : sGhosts) {
        if (g.actor == actor) {
            return true;
        }
    }
    return false;
}

static Actor* GhostDarkLink() {
    if (TestModeOn()) {
        return ActorAlive(sDarkLink) ? sDarkLink : nullptr;
    }
    for (auto& [key, g] : sGhosts) {
        if ((g.id == ACTOR_EN_TORCH2) && ActorAlive(g.actor)) {
            return g.actor;
        }
    }
    return nullptr;
}

// Dark Link's "controller": Ganondorf's, while he drives him (in the Link's game, and his copy in Ganondorf's)
extern "C" s32 GdfInvasion_DarkLinkInput(Actor* actor, Input* input, s16 camYaw) {
    if (GdfInvasion_IsInvader()) {
        if ((sFrame >= sPossessUntil) || (actor != GhostDarkLink())) {
            return false;
        }
        camYaw = sPossessYaw;
    } else if ((actor != sDarkLink) || (sFrame - sDlHeard > 10)) {
        return false;
    }
    s16 rel = camYaw - sDlYaw;
    input->cur.stick_x = (s8)(sDlTilt * Math_SinS(rel));
    input->cur.stick_y = (s8)(sDlTilt * Math_CosS(rel));
    input->cur.button = sDlButtons;
    return true;
}

extern "C" s32 GdfInvasion_DarkLinkPossessed(Actor* actor) {
    if (GdfInvasion_IsInvader()) {
        return (sFrame < sPossessUntil) && (actor == GhostDarkLink());
    }
    return (actor == sDarkLink) && (sFrame - sDlHeard <= 10);
}

static bool ClientArmedOnce(uint32_t clientId) {
    return (clientId < 256) && sClientArmed[clientId];
}

extern "C" f32 GdfInvasion_ResolveFraction(void) {
    return CLAMP(sResolve / GDF_RESOLVE_MAX, 0.0f, 1.0f);
}

// How long his Resolve meter is (half while the Links are young)
extern "C" f32 GdfInvasion_ResolveMeterLength(void) {
    return ResolveMax() / GDF_RESOLVE_MAX;
}

extern "C" s32 GdfInvasion_ShowResolveMeter(void) {
    return GdfInvasion_IsInvader() && GameStarted();
}


// =================================================================================================
// The shared world. The host is Green Link (or the first Link seated, without him): in the area he's in, everyone
// else (Ganondorf too) sees his monsters and NPCs instead of their own. Their own in his rooms are removed; his are
// shown as copies that move where his do and fight as themselves. Damage and kills on a copy are sent to his game,
// and his health for each comes back, so they're shared.
// =================================================================================================

// Everyone hosts: each room has its own host, the first Link to settle in it. Whoever walks in later joins his
// world. When the host leaves the room (or the game), the next Link there takes over the world as it stands.
struct HostClaim {
    s16 scene = -1;
    s8 room = -1;
    u32 heard = 0;
    bool cs = false; // in a cutscene: his world (the cutscene's actors) is shown to the others in the room
};
static std::map<uint32_t, HostClaim> sClaims; // other Links hosting: where, and when we last heard
static bool sClaiming = false;
static s16 sClaimScene = -1;
static s8 sClaimRoom = -1;
static u32 sRoomSince = 0;
static u32 sHostLastHeard = 0;
static uint32_t sWorldFrom = 0;
static void AdoptWorld();

static int ClientSeat(uint32_t clientId) {
    return sClientSeats.count(clientId) ? sClientSeats[clientId] : GDF_SEAT_NONE;
}

static void NoteHostClaim(uint32_t clientId, const nlohmann::json& world) {
    if (!Anchor::Instance->clients.count(clientId) || !world.is_object()) {
        return;
    }
    HostClaim& c = sClaims[clientId];
    c.scene = Anchor::Instance->clients[clientId].sceneNum;
    auto r = world.value("r", nlohmann::json::array());
    c.room = (r.is_array() && !r.empty()) ? (s8)r[0].get<int>() : -1;
    c.heard = sFrame;
    c.cs = world.value("cs", false);
}

static bool InCutscene(PlayState* play) {
    return (play != nullptr) && ((play->csCtx.state != CS_STATE_IDLE) || Player_InCsMode(play));
}

// The other Link hosting the room this player is in (the lowest seat, if two claim it at once)
static uint32_t RoomHostClient() {
    PlayState* play = gPlayState;
    if ((Anchor::Instance == nullptr) || (play == nullptr)) {
        return 0;
    }
    int bestSeat = 2 * GDF_SEAT_COUNT;
    uint32_t best = 0;
    for (auto& [clientId, c] : sClaims) {
        if ((sFrame - c.heard > 20) || (c.scene != play->sceneNum) || (c.room != play->roomCtx.curRoom.num) ||
            !Anchor::Instance->clients.count(clientId) || !Anchor::Instance->clients[clientId].online ||
            Anchor::Instance->clients[clientId].self) {
            continue;
        }
        int seat = ClientSeat(clientId);
        if (seat < GDF_SEAT_FIRST_LINK) {
            continue;
        }
        seat += c.cs ? 0 : GDF_SEAT_COUNT; // one in a cutscene comes first
        if (seat < bestSeat) {
            bestSeat = seat;
            best = clientId;
        }
    }
    return best;
}

static bool IsWorldHost() {
    return GdfInvasion_IsHero() && sClaiming;
}

static void UpdateHostClaim() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (Anchor::Instance == nullptr) || (GdfInvasion_Role() == GDF_INVASION_OFF) ||
        !GdfInvasion_IsHero() || (OwnSeat() < GDF_SEAT_FIRST_LINK)) {
        sClaiming = false;
        return;
    }
    if ((play->sceneNum != sClaimScene) || (play->roomCtx.curRoom.num != sClaimRoom)) {
        sClaimScene = play->sceneNum;
        sClaimRoom = play->roomCtx.curRoom.num;
        sRoomSince = sFrame;
        sClaiming = false; // a new room: see first whether someone hosts it
    }
    uint32_t other = RoomHostClient();
    bool myCs = InCutscene(play);
    if (other != 0) {
        sHostLastHeard = sFrame;
        bool otherCs = sClaims[other].cs;
        if (myCs && !otherCs) {
            if (!sClaiming) {
                sClaiming = true; // a cutscene here: the others watch it from this game
                AdoptWorld();
            }
            return;
        }
        if (sClaiming && ((otherCs && !myCs) || ((otherCs == myCs) && (ClientSeat(other) < OwnSeat())))) {
            sClaiming = false; // both took it at once: the lower seat keeps it
        }
        return;
    }
    if (!sClaiming && (sFrame - sRoomSince >= 40) && (sFrame - sHostLastHeard >= 10)) {
        sClaiming = true; // nobody hosts this room (anymore): this player does, with the world as it stands
        AdoptWorld();
    }
}

static bool IsWorldActorKind(Actor* a) {
    return (a->category == ACTORCAT_ENEMY) || (a->category == ACTORCAT_NPC);
}

static bool SkipForWorld(Actor* a) {
    if ((a->update == nullptr) || (a->id == ACTOR_PLAYER) || (a->id == ACTOR_BOSS_GANON) ||
        (a->id == ACTOR_BOSS_GANON2) || (a->id == ACTOR_EN_TORCH2) || (Anchor::Instance->GetDummyPlayerClientId(a) != 0)) {
        return true;
    }
    for (Mirrored& m : sMirrored) {
        if (m.actor == a) {
            return true; // his arts' own (sent apart)
        }
    }
    return false;
}

static std::map<Actor*, int> sWorldKeys;
static nlohmann::json WriteBosses(PlayState* play);
static nlohmann::json WriteMovers(PlayState* play);
static void ReadMovers(const nlohmann::json& list);
static void ReadSceneFlags(const nlohmann::json& f);
static void ReadBosses(const nlohmann::json& list);
static int sWorldNextKey = GDF_WORLD_KEY;

static void WriteWorld(nlohmann::json& payload) {
    PlayState* play = gPlayState;

    if (play == nullptr) {
        return;
    }
    nlohmann::json list = nlohmann::json::array();
    std::map<Actor*, int> keep;
    for (int cat : { ACTORCAT_ENEMY, ACTORCAT_NPC, ACTORCAT_MISC }) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
            if (SkipForWorld(a) || ((cat == ACTORCAT_MISC) && (a->id != ACTOR_EN_ITEM00))) {
                continue; // (of the misc ones, only pickups: rupees, hearts, ammo)
            }
            auto it = sWorldKeys.find(a);
            int key = (it != sWorldKeys.end()) ? it->second : sWorldNextKey++;
            keep[a] = key;
            list.push_back({ key, a->id, a->params, a->world.pos.x, a->world.pos.y, a->world.pos.z, a->shape.rot.y,
                             a->colChkInfo.health });
        }
    }
    sWorldKeys.swap(keep);
    payload["gw"] = { { "r", { play->roomCtx.curRoom.num, play->roomCtx.prevRoom.num } }, { "a", list },
                      { "b", WriteBosses(play) }, { "m", WriteMovers(play) },
                      { "f",
                        { play->actorCtx.flags.swch, play->actorCtx.flags.tempSwch, play->actorCtx.flags.clear,
                          play->actorCtx.flags.tempClear } },
                      { "cs", InCutscene(play) } };
}

static std::vector<s8> sWorldRooms;

static void ReadWorld(uint32_t from, const nlohmann::json& world) {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (from != RoomHostClient()) || !world.is_object() ||
        !Anchor::Instance->clients.count(from) || (Anchor::Instance->clients[from].sceneNum != play->sceneNum)) {
        return;
    }
    if ((sWorldFrom != 0) && (sWorldFrom != from)) {
        // A new host took the room over: his world is the old one (same keys), so the copies carry on
        std::map<std::pair<uint32_t, int>, Ghost> moved;
        for (auto it = sGhosts.begin(); it != sGhosts.end();) {
            if (it->second.world && (it->first.first == sWorldFrom)) {
                moved[{ from, it->first.second }] = it->second;
                it = sGhosts.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& [k, g] : moved) {
            sGhosts[k] = g;
        }
    }
    sWorldHeard = sFrame;
    sWorldFrom = from;
    ReadBosses(world.value("b", nlohmann::json::array()));
    ReadMovers(world.value("m", nlohmann::json::array()));
    ReadSceneFlags(world.value("f", nlohmann::json::array()));
    sWorldRooms.clear();
    for (const auto& r : world.value("r", nlohmann::json::array())) {
        sWorldRooms.push_back((s8)r.get<int>());
    }
    for (const auto& e : world.value("a", nlohmann::json::array())) {
        if (!e.is_array() || (e.size() < 8)) {
            continue;
        }
        auto key = std::make_pair(from, e[0].get<int>());
        if (sGhostsGone.count(key) || (sGhostRetry.count(key) && (sFrame < sGhostRetry[key]))) {
            continue;
        }
        Ghost& g = sGhosts[key];
        if ((g.actor != nullptr) && (g.id != (s16)e[1].get<int>())) {
            Actor_Kill(g.actor); // not the same one after all (a host who came in fresh): make it anew
            g.actor = nullptr;
        }
        Vec3f pos = { e[3].get<float>(), e[4].get<float>(), e[5].get<float>() };
        s16 rotY = (s16)e[6].get<int>();
        u8 hp = (u8)e[7].get<int>();
        if (g.actor == nullptr) {
            g.id = (s16)e[1].get<int>();
            g.world = true;
            g.actor = Actor_Spawn(&play->actorCtx, play, g.id, pos.x, pos.y, pos.z, 0, rotY, 0, (s16)e[2].get<int>());
            g.born = sFrame;
            if (g.actor == nullptr) {
                sGhostRetry[key] = sFrame + 60; // (its graphics not loaded here yet, or too many actors)
                sGhosts.erase(key);
                continue;
            }
            sGhostRetry.erase(key);
            g.actor->room = -1; // kept whichever room this player is in
            g.hostHp = hp;
        }
        g.seen = sFrame;
        if (ActorAlive(g.actor) && (g.actor == BossGanon_Player_GetGrabbedActor())) {
            continue; // in Ganondorf's fist: his game moves it (and tells the host where)
        }
        if (ActorAlive(g.actor)) {
            SmoothNote(g.actor, pos, { g.actor->shape.rot.x, rotY, g.actor->shape.rot.z });
            // Hurt in the host's game (or by another player): hurt here too
            if (hp < g.actor->colChkInfo.health) {
                g.actor->colChkInfo.health = hp;
            }
            g.hostHp = MIN(g.hostHp, hp);
        }
    }
}

static void SendWorldHit(uint32_t host, int key, u8 hp) {
    if (key < GDF_WORLD_KEY) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = GDF_PKT_WORLD_HIT;
    payload["targetClientId"] = host;
    payload["key"] = key;
    payload["hp"] = hp;
    Anchor::Instance->SendJsonToRemote(payload);
}

// The host: another player hurt or killed one of his monsters (or NPCs)
static void HandleWorldHit(const nlohmann::json& payload) {
    PlayState* play = gPlayState;
    int key = payload.value("key", 0);
    int hp = payload.value("hp", 255);

    if ((play == nullptr) || !IsWorldHost()) {
        return;
    }
    for (auto& [actor, k] : sWorldKeys) {
        if ((k != key) || !ActorAlive(actor)) {
            continue;
        }
        if (hp < actor->colChkInfo.health) {
            actor->colChkInfo.health = (u8)hp;
        }
        if (hp <= 0) {
            Vec3f zero = { 0.0f, 0.0f, 0.0f };
            Vec3f pos = actor->world.pos;
            if (actor->category == ACTORCAT_ENEMY) {
                EffectSsDeadDb_Spawn(play, &pos, &zero, &zero, 100, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9, true);
                Item_DropCollectibleRandom(play, nullptr, &pos, 0x10);
                Enemy_StartFinishingBlow(play, actor);
            }
            Actor_Kill(actor);
        }
        return;
    }
}

// Not the host, in his area: this game's own monsters and NPCs in the rooms he has loaded give way to his
static void SuppressLocalWorld() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || IsWorldHost() || (GdfInvasion_Role() == GDF_INVASION_OFF) || (sWorldHeard == 0) ||
        (sFrame - sWorldHeard > 30) || !Anchor::Instance->clients.count(sWorldFrom) ||
        (Anchor::Instance->clients[sWorldFrom].sceneNum != play->sceneNum)) {
        return;
    }
    for (int cat : { ACTORCAT_ENEMY, ACTORCAT_NPC, ACTORCAT_MISC }) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
            if (SkipForWorld(a) || GdfInvasion_IsSummoned(a) || (a->room < 0) ||
                ((cat == ACTORCAT_MISC) && (a->id != ACTOR_EN_ITEM00))) {
                continue; // (pickups too: the host's are everyone's, once)
            }
            if (std::find(sWorldRooms.begin(), sWorldRooms.end(), a->room) != sWorldRooms.end()) {
                Actor_Kill(a);
            }
        }
    }
}

// Taking a room over: the old host's copies here become this game's own, under the same keys
static void AdoptWorld() {
    PlayState* play = gPlayState;

    for (auto it = sGhosts.begin(); it != sGhosts.end();) {
        Ghost& g = it->second;
        if (g.world && (it->first.first == sWorldFrom) && ActorAlive(g.actor) && (play != nullptr)) {
            g.actor->room = play->roomCtx.curRoom.num;
            sWorldKeys[g.actor] = it->first.second;
            sWorldNextKey = MAX(sWorldNextKey, it->first.second + 1);
            it = sGhosts.erase(it);
        } else {
            ++it;
        }
    }
    sWorldHeard = 0;
}

static void RegisterInvasionWorld() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(UpdateHostClaim);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(SuppressLocalWorld);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sWorldKeys.clear();
        sWorldHeard = 0;
    });
}

static RegisterShipInitFunc worldInitFunc(RegisterInvasionWorld, {});

// ---- Revival ----

static bool OtherLinksOnline() {
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (!client.self && client.online && client.isSaveLoaded && !GdfInvasion_IsClientGanondorf(clientId)) {
            return true;
        }
    }
    return false;
}

// From Link's death (his "fairy or Game Over" moment): true keeps him down a while longer
extern "C" s32 GdfInvasion_HoldDowned(PlayState* play, Player* player) {
    if (!GdfInvasion_IsHero() || (Anchor::Instance == nullptr) || !OtherLinksOnline()) {
        return false;
    }
    if (!IsDowned()) {
        sDownedSince = sFrame; // just fell
        sReviveCalled = false;
    }
    sDownSeen = sFrame;
    if (sReviveCalled) {
        // Back up, as from a fairy, with 3 hearts
        sReviveCalled = false;
        sRevivedByAlly = true;
        sDownSeen = 0;
        player->av2.actionVar2 = 20;
        return true;
    }
    return (sFrame - sDownedSince) < (u32)(GDF_DOWN_SECONDS * GDF_FPS);
}

extern "C" s32 GdfInvasion_ReviveHealth(s32 full) {
    if (sRevivedByAlly) {
        sRevivedByAlly = false;
        return 3 * 16;
    }
    return full;
}

// ---- The light ball volley ----

static uint32_t GanondorfClient() {
    for (auto& [clientId, c] : sClients) {
        if (c.isGanondorf) {
            return clientId;
        }
    }
    return 0;
}

static void SendBallPacket(const char* type, Vec3f* pos) {
    uint32_t to = GanondorfClient();
    if ((to == 0) || (Anchor::Instance == nullptr)) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = type;
    payload["targetClientId"] = to;
    payload["x"] = pos->x;
    payload["y"] = pos->y;
    payload["z"] = pos->z;
    Anchor::Instance->SendJsonToRemote(payload);
}

extern "C" void GdfInvasion_SendVolley(Vec3f* ballPos) {
    SendBallPacket(GDF_PKT_VOLLEY, ballPos);
}

extern "C" void GdfInvasion_SendBallHit(Vec3f* ballPos) {
    SendBallPacket(GDF_PKT_BALL_HIT, ballPos);
}

// A Link's game: the ball got him (as Ganondorf's hits do: less with fewer hearts, never more than a third)
extern "C" void GdfInvasion_BallHurtLink(PlayState* play) {
    Player* self = GET_PLAYER(play);
    if ((self == nullptr) || (self->invincibilityTimer > 0)) {
        return;
    }
    s32 dmg = 32 * gSaveContext.healthCapacity / (20 * 16);
    dmg = CLAMP(dmg, 4, MAX(gSaveContext.healthCapacity / 3, 4));
    self->actor.colChkInfo.damage = (u8)dmg;
    func_80837C0C(play, self, PLAYER_HIT_RESPONSE_ELECTRIFIED, 4.0f, 5.0f, self->actor.shape.rot.y + 0x8000, 20);
}

// Ganondorf's game: he missed his own ball sent back at him
extern "C" void GdfInvasion_BallHitGanondorf(void) {
    if (!GdfInvasion_IsInvader() || IsBanished()) {
        return;
    }
    nlohmann::json hit;
    hit["amount"] = 20;
    hit["effect"] = 2;
    HandleHit(hit);
}

extern "C" s32 GdfInvasion_AeroAlchemyActive(void) {
    return GdfInvasion_IsHero() && (sAeroUntil != 0) && (sFrame < sAeroUntil);
}

// ---- World sync, stage 1: Ganondorf holding the host's monsters and NPCs ----

// Ganondorf's game: while he holds a copy of the host's actor, the host's real one is put where his fist has it
static void SendWorldHold() {
    Actor* held = BossGanon_Player_GetGrabbedActor();

    if (!GdfInvasion_IsInvader() || (held == nullptr) || (Anchor::Instance == nullptr)) {
        return;
    }
    for (auto& [key, g] : sGhosts) {
        if (!g.world || (g.actor != held)) {
            continue;
        }
        nlohmann::json payload;
        payload["type"] = GDF_PKT_WORLD_HOLD;
        payload["targetClientId"] = key.first;
        payload["key"] = key.second;
        payload["p"] = { held->world.pos.x, held->world.pos.y, held->world.pos.z };
        payload["r"] = held->shape.rot.y;
        Anchor::Instance->SendJsonToRemote(payload);
        return;
    }
}

struct WorldHeld {
    Actor* actor = nullptr;
    Vec3f pos;
    s16 rotY = 0;
    u32 frame = 0;
};
static WorldHeld sWorldHeld;

// The host: Ganondorf has this one by the throat
static void HandleWorldHold(const nlohmann::json& payload) {
    if (!IsWorldHost()) {
        return;
    }
    int key = payload.value("key", 0);
    std::vector<float> p = payload.value("p", std::vector<float>{});
    if (p.size() < 3) {
        return;
    }
    for (auto& [actor, k] : sWorldKeys) {
        if ((k == key) && ActorAlive(actor)) {
            sWorldHeld.actor = actor;
            sWorldHeld.pos = { p[0], p[1], p[2] };
            sWorldHeld.rotY = (s16)payload.value("r", 0);
            sWorldHeld.frame = sFrame;
            return;
        }
    }
}

static void UpdateWorldHeld() {
    SendWorldHold();
    if ((sWorldHeld.actor == nullptr) || !ActorAlive(sWorldHeld.actor) || (sFrame - sWorldHeld.frame > 10)) {
        sWorldHeld.actor = nullptr;
        return;
    }
    // Held where his fist is, and doing nothing of its own meanwhile
    Actor* a = sWorldHeld.actor;
    a->world.pos = sWorldHeld.pos;
    a->prevPos = sWorldHeld.pos;
    a->shape.rot.y = a->world.rot.y = sWorldHeld.rotY;
    a->velocity = { 0.0f, 0.0f, 0.0f };
    a->speedXZ = 0.0f;
    a->freezeTimer = 2;
}

static void RegisterWorldHeld() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(UpdateWorldHeld);
}

static RegisterShipInitFunc worldHeldInitFunc(RegisterWorldHeld, {});

// ---- World sync, stage 2: the doors he kicks in fly off their hinges for everyone ----
extern "C" void GdfInvasion_NoteDoorKick(Vec3f* doorPos, Vec3f* fromPos) {
    if (!GdfInvasion_IsInvader() || TestModeOn() || (gPlayState == nullptr) || !AnchorOn()) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = GDF_PKT_DOOR_KICK;
    payload["scene"] = gPlayState->sceneNum;
    payload["d"] = { doorPos->x, doorPos->y, doorPos->z };
    payload["f"] = { fromPos->x, fromPos->y, fromPos->z };
    Anchor::Instance->SendJsonToRemote(payload);
}

// ---- World sync, stage 3: the host Link's bosses for everyone ----
// Each player's game runs its own boss, but away from the host it's made to follow his: where it is, which way it
// faces, its pose (its skeleton's joints) and its health. Hits on it anywhere are sent to the host's boss.

extern "C" {
Actor* gGdfInitActor = nullptr;
}
static std::map<Actor*, SkelAnime*> sActorSkel; // each actor's (first) skeleton, noted as it's set up

extern "C" void GdfInvasion_NoteSkel(SkelAnime* skelAnime) {
    if ((gGdfInitActor != nullptr) && !sActorSkel.count(gGdfInitActor)) {
        sActorSkel[gGdfInitActor] = skelAnime;
    }
}

static std::map<Actor*, u32> sBossNoted; // the host update each boss copy last took
static std::set<Actor*> sMovedBg; // the host's: moving parts that have moved since the area loaded

extern "C" void GdfInvasion_ForgetActor(Actor* actor) {
    sActorSkel.erase(actor);
    sMovedBg.erase(actor);
    sSmooth.erase(actor);
    sBossNoted.erase(actor);
}

static bool IsSyncedBoss(Actor* a) {
    return (a->update != nullptr) && (a->category == ACTORCAT_BOSS) && (a->id != ACTOR_BOSS_GANON) &&
           (a->id != ACTOR_BOSS_GANON2) && (a->id != ACTOR_EN_TORCH2);
}

static nlohmann::json WriteBosses(PlayState* play) {
    nlohmann::json list = nlohmann::json::array();
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].head; a != nullptr; a = a->next) {
        if (!IsSyncedBoss(a)) {
            continue;
        }
        std::vector<int> joints;
        auto it = sActorSkel.find(a);
        if ((it != sActorSkel.end()) && (it->second->jointTable != nullptr)) {
            SkelAnime* sk = it->second;
            for (int i = 0; i < sk->limbCount && i < 80; i++) {
                joints.insert(joints.end(), { sk->jointTable[i].x, sk->jointTable[i].y, sk->jointTable[i].z });
            }
        }
        list.push_back({ { "i", a->id },
                         { "p", { a->world.pos.x, a->world.pos.y, a->world.pos.z } },
                         { "r", { a->shape.rot.x, a->shape.rot.y, a->shape.rot.z } },
                         { "h", a->colChkInfo.health },
                         { "j", joints } });
    }
    return list;
}

struct HostBoss {
    s16 id;
    Vec3f pos;
    Vec3s rot;
    u8 health;
    std::vector<int> joints;
};
static std::vector<HostBoss> sHostBosses;
static u32 sHostBossFrame = 0;
static std::map<Actor*, u8> sBossHostHp; // what the host last said each local boss's health was

static void ReadBosses(const nlohmann::json& list) {
    sHostBosses.clear();
    for (const auto& b : list) {
        try {
            HostBoss hb;
            std::vector<float> p = b.value("p", std::vector<float>{ 0, 0, 0 });
            std::vector<int> r = b.value("r", std::vector<int>{ 0, 0, 0 });
            p.resize(3);
            r.resize(3);
            hb.id = (s16)b.value("i", 0);
            hb.pos = { p[0], p[1], p[2] };
            hb.rot = { (s16)r[0], (s16)r[1], (s16)r[2] };
            hb.health = (u8)b.value("h", 0);
            hb.joints = b.value("j", std::vector<int>{});
            sHostBosses.push_back(std::move(hb));
        } catch (...) {
        }
    }
    sHostBossFrame = sFrame;
}

// After a boss's own update, away from the host: it's put where the host's is, posed as his, with his health
extern "C" void GdfInvasion_AfterBossUpdate(Actor* actor) {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (GdfInvasion_Role() == GDF_INVASION_OFF) || IsWorldHost() || !IsSyncedBoss(actor) ||
        (sFrame - sHostBossFrame > 20) || !Anchor::Instance->clients.count(sWorldFrom) ||
        (Anchor::Instance->clients[sWorldFrom].sceneNum != play->sceneNum)) {
        return;
    }
    const HostBoss* best = nullptr;
    float bestDist = 1e9f;
    for (const HostBoss& hb : sHostBosses) {
        if (hb.id != actor->id) {
            continue;
        }
        Vec3f hbPos = hb.pos;
        float d = Math_Vec3f_DistXYZ(&hbPos, &actor->world.pos);
        if (d < bestDist) {
            bestDist = d;
            best = &hb;
        }
    }
    if (best == nullptr) {
        return;
    }
    if (sBossNoted[actor] != sHostBossFrame) {
        sBossNoted[actor] = sHostBossFrame;
        SmoothNote(actor, best->pos, best->rot);
    }
    SmoothApply(actor, true);
    auto it = sActorSkel.find(actor);
    if ((it != sActorSkel.end()) && (it->second->jointTable != nullptr)) {
        SkelAnime* sk = it->second;
        int n = (int)MIN((size_t)sk->limbCount, best->joints.size() / 3);
        for (int i = 0; i < n; i++) {
            Vec3s& j = sk->jointTable[i];
            if (i == 0) {
                // the root limb carries position: eased like the rest
                j.x += (s16)(((s16)best->joints[0] - j.x) / 2);
                j.y += (s16)(((s16)best->joints[1] - j.y) / 2);
                j.z += (s16)(((s16)best->joints[2] - j.z) / 2);
                continue;
            }
            j.x = SmoothAngle(j.x, (s16)best->joints[i * 3]);
            j.y = SmoothAngle(j.y, (s16)best->joints[i * 3 + 1]);
            j.z = SmoothAngle(j.z, (s16)best->joints[i * 3 + 2]);
        }
    }
    // Hurt here: the host's boss takes it too
    auto hp = sBossHostHp.find(actor);
    if ((hp != sBossHostHp.end()) && (actor->colChkInfo.health < hp->second)) {
        nlohmann::json payload;
        payload["type"] = GDF_PKT_BOSS_HIT;
        payload["targetClientId"] = sWorldFrom;
        payload["id"] = actor->id;
        payload["hp"] = actor->colChkInfo.health;
        payload["p"] = { best->pos.x, best->pos.y, best->pos.z };
        Anchor::Instance->SendJsonToRemote(payload);
    }
    if ((best->health > 0) || (actor->colChkInfo.health > 0)) {
        actor->colChkInfo.health = MIN(actor->colChkInfo.health, best->health);
    }
    sBossHostHp[actor] = actor->colChkInfo.health;
}

// The host: another player hurt his boss
static void HandleBossHit(const nlohmann::json& payload) {
    PlayState* play = gPlayState;
    if ((play == nullptr) || !IsWorldHost()) {
        return;
    }
    std::vector<float> p = payload.value("p", std::vector<float>{ 0, 0, 0 });
    p.resize(3);
    Vec3f at = { p[0], p[1], p[2] };
    s16 id = (s16)payload.value("id", 0);
    int hp = payload.value("hp", 255);
    Actor* best = nullptr;
    float bestDist = 1e9f;
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BOSS].head; a != nullptr; a = a->next) {
        if ((a->id == id) && IsSyncedBoss(a)) {
            float d = Math_Vec3f_DistXYZ(&a->world.pos, &at);
            if (d < bestDist) {
                bestDist = d;
                best = a;
            }
        }
    }
    if ((best != nullptr) && (hp < best->colChkInfo.health)) {
        best->colChkInfo.health = (u8)MAX(hp, 1); // (the last blow is his game's own, so the boss dies properly)
    }
}

static void RegisterWorldBosses() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sHostBosses.clear();
        sBossHostHp.clear();
    });
}

static RegisterShipInitFunc worldBossInitFunc(RegisterWorldBosses, {});

// ---- World sync, stage 5: moving parts (blocks, platforms, elevators) and the area's switches ----
// A moving part is the same in every game: same kind, same place in the area's data. The host sends those that
// have moved; everyone else's are put where his are (and carry whoever stands on them). Ganondorf, who has no flag
// sync of his own, also takes the host's switch and room-clear flags, so his doors and bars match.

static long long MoverKey(s16 id, const Vec3f& home) {
    long long x = (long long)lroundf(home.x) & 0xFFFFF;
    long long y = (long long)lroundf(home.y) & 0xFFFFF;
    long long z = (long long)lroundf(home.z) & 0xFFFFF;
    return ((long long)(u16)id << 60) ^ (x << 40) ^ (y << 20) ^ z;
}

static nlohmann::json WriteMovers(PlayState* play) {
    nlohmann::json list = nlohmann::json::array();
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != nullptr; a = a->next) {
        if ((a->update == nullptr) || GdfInvasion_IsSummoned(a)) {
            continue;
        }
        bool moved = (Math_Vec3f_DistXYZ(&a->world.pos, &a->home.pos) > 1.0f) || (a->shape.rot.x != a->home.rot.x) ||
                     (a->shape.rot.y != a->home.rot.y) || (a->shape.rot.z != a->home.rot.z);
        if (moved) {
            sMovedBg.insert(a);
        }
        if (!sMovedBg.count(a)) {
            continue;
        }
        list.push_back({ a->id, a->home.pos.x, a->home.pos.y, a->home.pos.z, a->world.pos.x, a->world.pos.y,
                         a->world.pos.z, a->shape.rot.x, a->shape.rot.y, a->shape.rot.z });
    }
    return list;
}

struct HostMover {
    Vec3f pos;
    Vec3s rot;
    bool fresh = true; // not yet handed to the smoother
};
static std::map<long long, HostMover> sHostMovers;
static u32 sHostMoverFrame = 0;

static void ReadMovers(const nlohmann::json& list) {
    sHostMovers.clear();
    for (const auto& e : list) {
        if (!e.is_array() || (e.size() < 10)) {
            continue;
        }
        Vec3f home = { e[1].get<float>(), e[2].get<float>(), e[3].get<float>() };
        HostMover m;
        m.pos = { e[4].get<float>(), e[5].get<float>(), e[6].get<float>() };
        m.rot = { (s16)e[7].get<int>(), (s16)e[8].get<int>(), (s16)e[9].get<int>() };
        sHostMovers[MoverKey((s16)e[0].get<int>(), home)] = m;
    }
    sHostMoverFrame = sFrame;
}

static bool FollowingHost(PlayState* play) {
    return (play != nullptr) && (GdfInvasion_Role() != GDF_INVASION_OFF) && !IsWorldHost() &&
           (sFrame - sHostMoverFrame <= 20) && Anchor::Instance->clients.count(sWorldFrom) &&
           (Anchor::Instance->clients[sWorldFrom].sceneNum == play->sceneNum);
}

extern "C" void GdfInvasion_AfterBgUpdate(Actor* actor) {
    PlayState* play = gPlayState;

    if (!FollowingHost(play) || GdfInvasion_IsSummoned(actor)) {
        return;
    }
    auto it = sHostMovers.find(MoverKey(actor->id, actor->home.pos));
    if (it == sHostMovers.end()) {
        return;
    }
    if (it->second.fresh) {
        SmoothNote(actor, it->second.pos, it->second.rot);
        it->second.fresh = false;
    }
    SmoothApply(actor, true);
}

static u32 sKickedSwch = 0;      // doors he kicked open in this area (his own game only)
static u32 sKickedTempSwch = 0;
static u32 sKickedTempClear = 0;

extern "C" void GdfInvasion_KeepKickedSwitch(s32 flag) {
    if (flag < 0x20) {
        sKickedSwch |= 1u << flag;
    } else if (flag < 0x40) {
        sKickedTempSwch |= 1u << (flag - 0x20);
    }
}

extern "C" void GdfInvasion_KeepKickedClear(s32 room) {
    if ((room >= 0) && (room < 32)) {
        sKickedTempClear |= 1u << room;
    }
}

static void ReadSceneFlags(const nlohmann::json& f) {
    PlayState* play = gPlayState;

    if (!GdfInvasion_IsInvader() || (play == nullptr) || !f.is_array() || (f.size() < 4) ||
        !Anchor::Instance->clients.count(sWorldFrom) ||
        (Anchor::Instance->clients[sWorldFrom].sceneNum != play->sceneNum)) {
        return;
    }
    play->actorCtx.flags.swch = f[0].get<u32>() | sKickedSwch;
    play->actorCtx.flags.tempSwch = f[1].get<u32>() | sKickedTempSwch;
    play->actorCtx.flags.clear = f[2].get<u32>();
    play->actorCtx.flags.tempClear = f[3].get<u32>() | sKickedTempClear;
}

static void RegisterWorldMovers() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sHostMovers.clear();
        sMovedBg.clear();
        sSmooth.clear();
        sKickedSwch = sKickedTempSwch = sKickedTempClear = 0;
    });
}

static RegisterShipInitFunc worldMoverInitFunc(RegisterWorldMovers, {});

// ---- The guard hunt, in the Links' games ----

extern "C" void GdfInvasion_MirrorActor(Actor* actor) {
    if (GdfInvasion_IsInvader()) {
        Mirror(actor);
    }
}

static std::set<Actor*> sSlain;

extern "C" void GdfInvasion_NoteSlain(Actor* actor) {
    if (GdfInvasion_Role() != GDF_INVASION_OFF) {
        sSlain.insert(actor);
    }
}

static bool TakeSlain(Actor* actor) {
    return sSlain.erase(actor) > 0;
}

extern "C" s32 GdfInvasion_IsRemoteCopy(Actor* actor) {
    if (GdfInvasion_Role() == GDF_INVASION_OFF) {
        return false;
    }
    for (auto& [key, g] : sGhosts) {
        if (g.actor == actor) {
            return true;
        }
    }
    return false;
}

extern "C" s32 GdfInvasion_GanondorfPos(Vec3f* out) {
    if ((Anchor::Instance == nullptr) || (gPlayState == nullptr)) {
        return false;
    }
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (!client.self && GdfInvasion_IsClientGanondorf(clientId) && InSameScene(clientId)) {
            *out = client.posRot.pos;
            return true;
        }
    }
    return false;
}

static bool IsMarket(s16 scene) {
    switch (scene) {
        case SCENE_MARKET_DAY:
        case SCENE_MARKET_NIGHT:
        case SCENE_MARKET_ENTRANCE_DAY:
        case SCENE_MARKET_ENTRANCE_NIGHT:
        case SCENE_BACK_ALLEY_DAY:
        case SCENE_BACK_ALLEY_NIGHT:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT:
            return true;
        default:
            return false;
    }
}

// The same slow, low bell Ganondorf hears, while he's wanted in the market this Link is in
static void RingAlarmBell() {
    static s16 sBellTimer = 20;
    static f32 sBellFreq = 0.55f;
    static f32 sBellVol = 1.0f;
    PlayState* play = gPlayState;

    if ((play == nullptr) || !GdfInvasion_IsHero() || GdfInvasion_IsInvader() || !sGanonWanted ||
        (sFrame - sGanonWantedHeard > 40) || !IsMarket(play->sceneNum)) {
        sBellTimer = 20;
        return;
    }
    if (--sBellTimer <= 0) {
        sBellTimer = 75;
        Audio_PlaySfxGeneral(NA_SE_EV_SHIP_BELL, &gSfxDefaultPos, 4, &sBellFreq, &sBellVol, &gSfxDefaultReverb);
    }
}

static void RegisterGuardHunt() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(RingAlarmBell);
}

static RegisterShipInitFunc guardHuntInitFunc(RegisterGuardHunt, {});

// ---- Songs: a MIDI song played on the ocarina (or his organ) is heard by everyone in the area ----

void GdfInvasion_SendSong(const OcarinaNote* notes, int count) {
    if ((GdfInvasion_Role() == GDF_INVASION_OFF) || !AnchorOn() || (gPlayState == nullptr)) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = GDF_PKT_SONG;
    payload["scene"] = gPlayState->sceneNum;
    payload["organ"] = GdfInvasion_IsInvader(); // his ocarina is his pipe organ
    nlohmann::json list = nlohmann::json::array();
    if (notes != nullptr) {
        for (int i = 0; (i < count) && (i < 3000); i++) {
            list.push_back({ notes[i].pitch, notes[i].length, notes[i].volume, notes[i].vibrato, notes[i].bend });
        }
    }
    payload["n"] = list;
    Anchor::Instance->SendJsonToRemote(payload);
}

static std::vector<OcarinaNote> sHeardNotes; // must stay put while it plays
static uint32_t sHeardFrom = 0;
static bool sHearing = false;

bool GdfInvasion_HearingSong() {
    return sHearing;
}

static bool OwnOcarinaBusy(PlayState* play) {
    Player* player = GET_PLAYER(play);
    return ((player != nullptr) && (player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING)) ||
           (play->msgCtx.msgMode != MSGMODE_NONE) || (play->csCtx.state != CS_STATE_IDLE);
}

static void StopHearing() {
    if (!sHearing) {
        return;
    }
    sHearing = false;
    AudioOcarina_PlayCustomSong(NULL);
    if ((gPlayState != nullptr) && !OwnOcarinaBusy(gPlayState)) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
    }
}

static void HandleSong(const nlohmann::json& payload) {
    PlayState* play = gPlayState;
    uint32_t from = payload.value("clientId", 0u);
    auto list = payload.value("n", nlohmann::json::array());

    if (list.empty()) {
        if (from == sHeardFrom) {
            StopHearing();
        }
        return;
    }
    if ((play == nullptr) || (payload.value("scene", -1) != play->sceneNum) || OwnOcarinaBusy(play)) {
        return; // elsewhere, or this player's own ocarina (or a cutscene's) has the ocarina channel
    }
    sHeardNotes.clear();
    for (const auto& e : list) {
        if (!e.is_array() || (e.size() < 5)) {
            continue;
        }
        OcarinaNote n;
        n.pitch = (u8)e[0].get<int>();
        n.length = (u16)e[1].get<int>();
        n.volume = (u8)e[2].get<int>();
        n.vibrato = (u8)e[3].get<int>();
        n.bend = (s8)e[4].get<int>();
        n.bFlat4Flag = 0;
        sHeardNotes.push_back(n);
    }
    if (sHeardNotes.empty() || (sHeardNotes.back().length != 0)) {
        sHeardNotes.push_back({ OCARINA_PITCH_NONE, 0, 0, 0, 0, 0 }); // (the end)
    }
    AudioOcarina_SetRemoteInstrument(payload.value("organ", false));
    AudioOcarina_PlayCustomSong(sHeardNotes.data());
    sHeardFrom = from;
    sHearing = true;
}

// It ends, the player who plays it leaves the area (or goes offline), or this player takes out his own ocarina
static void UpdateHearing() {
    PlayState* play = gPlayState;

    if (!sHearing) {
        return;
    }
    if ((play == nullptr) || !AudioOcarina_IsPlayingBack() || !InSameScene(sHeardFrom) ||
        !Anchor::Instance->clients[sHeardFrom].online) {
        StopHearing();
        return;
    }
    Player* player = GET_PLAYER(play);
    if ((player != nullptr) && (player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING)) {
        sHearing = false; // his own ocarina takes the channel over
        AudioOcarina_PlayCustomSong(NULL);
    }
}

static void RegisterSongHearing() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(UpdateHearing);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { StopHearing(); });
}

static RegisterShipInitFunc songHearingInitFunc(RegisterSongHearing, {});
