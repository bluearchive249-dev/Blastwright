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

#include <cmath>
#include <deque>
#include <map>
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
extern PlayState* gPlayState;
}

#include "GanondorfInvasion.h"

#define CVAR_INVASION(var) CVAR_ENHANCEMENT("PlayAsGanondorf.Invasion." var)

// ---- Roles ----

static bool AnchorOn() {
    return (Anchor::Instance != nullptr) && Anchor::Instance->isEnabled;
}

// The role is set when the host starts the game from the lobby (from the seat each player took)
extern "C" s32 GdfInvasion_Role(void) {
    if (!AnchorOn() || !CVarGetInteger(CVAR_INVASION("Enabled"), 0)) {
        return GDF_INVASION_OFF;
    }
    return CVarGetInteger(CVAR_INVASION("Role"), GDF_INVASION_OFF);
}

extern "C" s32 GdfInvasion_IsInvader(void) {
    return GdfInvasion_Role() == GDF_INVASION_INVADER;
}

extern "C" s32 GdfInvasion_IsHero(void) {
    return GdfInvasion_Role() == GDF_INVASION_HERO;
}

// The truce lasts until the Hero has entered the Deku Tree. Remembered (per player) once it happens; the Anchor menu
// can reset it for a new playthrough.
extern "C" s32 GdfInvasion_AdventureStarted(void) {
    return CVarGetInteger(CVAR_INVASION("Started"), 0) != 0;
}

// ---- Ganondorf's pose over the network ----

struct ClientGdf {
    GdfNetPose pose = {};
    u32 lastFrame = 0; // when the last pose came in
    bool isGanondorf = false;
    Actor* puppet = nullptr;
};

static std::map<uint32_t, ClientGdf> sClients;
static u32 sFrame = 0;

// Sounds his Ganondorf made since the last packet went out (with the frame, so sounds nobody was around to hear
// aren't all played at once when someone walks in)
static std::vector<std::pair<u16, u32>> sSfxQueue;

extern "C" void GdfInvasion_NoteSfx(u16 sfxId) {
    if (!GdfInvasion_IsInvader()) {
        return;
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

    if (!GdfInvasion_IsInvader()) {
        return;
    }
    payload["gdfRole"] = GDF_INVASION_INVADER;
    if (!BossGanon_Player_GetNetPose(&pose)) {
        return;
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
    sSfxQueue.clear();
    payload["gdf"] = g;
}

void GdfInvasion_ReadPose(uint32_t clientId, const nlohmann::json& payload) {
    ClientGdf& c = sClients[clientId];

    c.isGanondorf = payload.value("gdfRole", 0) == GDF_INVASION_INVADER;
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
    // The adventure starts when the Hero (any other player, for the Invader) is inside the Deku Tree
    if (!GdfInvasion_AdventureStarted() && (GdfInvasion_Role() != GDF_INVASION_OFF)) {
        bool started = false;

        for (auto& [clientId, client] : Anchor::Instance->clients) {
            bool isHero = client.self ? GdfInvasion_IsHero() : !GdfInvasion_IsClientGanondorf(clientId);

            if (!isHero || !client.online || !(client.self ? Anchor::Instance->IsSaveLoaded() : client.isSaveLoaded)) {
                continue;
            }
            s16 scene = client.self ? ((gPlayState != nullptr) ? gPlayState->sceneNum : -1) : client.sceneNum;
            if (scene == SCENE_DEKU_TREE) {
                started = true;
            }
        }
        if (started) {
            CVarSetInteger(CVAR_INVASION("Started"), 1);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
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

    // The truce
    if (!GdfInvasion_AdventureStarted()) {
        const char* truce = "Truce until Link enters the Deku Tree";
        float fontSize = ImGui::GetFontSize() * 0.7f * fmaxf(scale, 1.0f);
        ImVec2 size = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, truce);
        float x = vp->Pos.x + (vp->Size.x - size.x) * 0.5f;

        list->AddText(ImGui::GetFont(), fontSize, ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 90), truce);
        list->AddText(ImGui::GetFont(), fontSize, ImVec2(x, y), IM_COL32(200, 200, 200, 110), truce);
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
    CVarSetInteger(CVAR_INVASION("Started"), 0); // a new game: the truce holds until the Deku Tree
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
                           "it harder. Ganondorf can never hurt a Link directly.");
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
        ImGui::Text("Adventure: %s", GdfInvasion_AdventureStarted() ? "started (a Link has entered the Deku Tree)"
                                                                     : "truce until a Link enters the Deku Tree");
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
#define GDF_ART_ROWS 4
#define GDF_ART_COUNT (GDF_ART_COLS * GDF_ART_ROWS)

// Row by row: obstacles, monsters, curses, his own. Curses are index 12 onward.
static const DarkArt sArts[GDF_ART_COUNT] = {
    { "Red Ice Wall", ART_OBSTACLE, 14, 8, 20, false, "A wall of red ice across a path. Melts with fire." },
    { "Wall of Flame", ART_OBSTACLE, 19, 10, 15, false, "A line of rising and falling fire." },
    { "Rolling Boulders", ART_OBSTACLE, 24, 15, 0, false, "Three boulders roll toward the marker." },
    { "Spike Trap", ART_OBSTACLE, 14, 8, 30, false, "A sliding blade guards a spot." },
    { "Iron Bars", ART_OBSTACLE, 19, 20, 15, false, "The nearest door to Link is barred shut." },
    { "Quicksand", ART_OBSTACLE, 24, 20, 20, false, "A circle of sinking sand." },
    { "Ambush Ring", ART_MONSTER, 19, 15, 0, false, "Four small monsters rise around Link." },
    { "Flank", ART_MONSTER, 24, 20, 0, false, "Two monsters, ahead of and behind Link." },
    { "Swarm", ART_MONSTER, 24, 20, 0, false, "Keese circle above Link, then dive." },
    { "Waves", ART_MONSTER, 34, 40, 0, false, "Three waves of three monsters." },
    { "Sentinel", ART_MONSTER, 29, 30, 0, false, "One tough monster on Link's path." },
    { "Duel", ART_MONSTER, 48, 60, 40, false, "Link is walled in with a Stalfos." },
    { "Backwards Path", ART_CURSE, 19, 30, 12, true, "Link's controls are reversed." },
    { "Frozen Ground", ART_CURSE, 14, 25, 15, true, "The ground under Link turns slippery." },
    { "Iron Weight", ART_CURSE, 14, 25, 15, true, "Link is weighed down by iron (not in water)." },
    { "Shrink", ART_CURSE, 19, 30, 20, true, "Link shrinks to a fraction of his size." },
    { "Sealed Hand", ART_CURSE, 24, 40, 20, true, "One of Link's C-button items is sealed." },
    { "Blinding Gloom", ART_CURSE, 19, 30, 25, true, "Darkness closes in around Link." },
    { "Lost Bearings", ART_CURSE, 14, 30, 15, true, "Link can't Z-target." },
    { "Ghostly Gale", ART_CURSE, 19, 30, 15, true, "Gusts of wind push Link about." },
    { "Phantom Step", ART_OWN, 19, 30, 0, false, "Ganondorf steps out of sight." },
    { "Eye of Ganon", ART_OWN, 10, 10, 10, false, "Watch Link from afar." },
    { "Shadow Veil", ART_OWN, 24, 45, 15, false, "Ganondorf turns invisible to Link." },
    { "Rupee Tithe", ART_OWN, 14, 60, 0, false, "Takes 20 rupees from Link." },
};

#define GDF_FIRST_CURSE 12
static const char* sCurseMessages[8] = {
    "has reversed your path!", "has frozen the ground!",  "weighs you down with iron!", "has shrunk you!",
    "has sealed your hand!",   "has cast a blinding gloom!", "has stolen your bearings!", "summons a ghostly gale!",
};

static const u16 sCButtons[3] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
static const char* sCNames[3] = { "C-Left", "C-Down", "C-Right" };

// ---- Ganondorf's side ----
static float sResolve = GDF_RESOLVE_MAX;
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

static bool GameStarted() {
    return GdfInvasion_Role() != GDF_INVASION_OFF;
}

extern "C" s32 GdfInvasion_CanAct(void) {
    if (!GdfInvasion_IsInvader()) {
        return true;
    }
    return !sMenuOpen && !IsBanished();
}

static void Flash(const std::string& message) {
    sCastMessage = message;
    sCastMessageUntil = sFrame + 3 * GDF_FPS;
}

// The Link a Dark Art goes to: the nearest one in his area, close enough
static uint32_t FindTargetLink(PlayState* play) {
    Player* self = GET_PLAYER(play);
    uint32_t best = 0;
    float bestDist = GDF_CAST_RANGE;

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

static void TryCast(PlayState* play, int slot) {
    int art = SlotArt(slot);

    if ((art < 0) || (art >= GDF_ART_COUNT)) {
        return;
    }
    const DarkArt& a = sArts[art];
    if (!a.ready) {
        Flash(std::string(a.name) + " isn't ready yet (coming in a later version)");
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    if (!GdfInvasion_AdventureStarted()) {
        Flash("The truce holds until a Link enters the Deku Tree");
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
    uint32_t target = FindTargetLink(play);
    if (target == 0) {
        Flash("No Link close enough");
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
    payload["art"] = art;
    payload["duration"] = a.duration;
    Anchor::Instance->SendJsonToRemote(payload);
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

    if ((play == nullptr) || (player == nullptr) || Player_InCsMode(play) ||
        (play->csCtx.state != CS_STATE_IDLE) || (play->msgCtx.msgMode != MSGMODE_NONE) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->pauseCtx.state != 0)) {
        return false;
    }
    return !(player->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_GETTING_ITEM |
                                     PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE));
}

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
            // Drawn by the HUD (the darkness)
            break;
        case 6:
            GameInteractor::State::DisableZTargetingActive = on;
            break;
        case 7:
            GameInteractor::RawAction::SetRandomWind(on);
            break;
    }
}

static void HandleCurse(const nlohmann::json& payload) {
    PlayState* play = gPlayState;
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
    Anchor::Instance->SendJsonToRemote(ack);
}

static void UpdateCurses() {
    for (int i = 0; i < 8; i++) {
        if (sCurses[i].active && ((sFrame >= sCurses[i].until) || !GdfInvasion_IsHero())) {
            sCurses[i].active = false;
            SetCurse(i, false);
        }
    }
}

extern "C" void GdfInvasion_FilterLinkInput(PlayState* play, Input* input) {
    // Ganondorf: his menu open, or banished
    if (GdfInvasion_IsInvader() && (sMenuOpen || IsBanished())) {
        memset(input, 0, sizeof(*input));
        return;
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
    /* Deku stick    */ DMG_ENTRY(2, 0),
    /* Slingshot     */ DMG_ENTRY(1, 0),
    /* Explosive     */ DMG_ENTRY(4, 0),
    /* Boomerang     */ DMG_ENTRY(0, 1),
    /* Normal arrow  */ DMG_ENTRY(2, 0),
    /* Hammer swing  */ DMG_ENTRY(5, 3),
    /* Hookshot      */ DMG_ENTRY(0, 1),
    /* Kokiri sword  */ DMG_ENTRY(2, 0),
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
    /* Kokiri spin   */ DMG_ENTRY(4, 0),
    /* Giant spin    */ DMG_ENTRY(10, 3),
    /* Master spin   */ DMG_ENTRY(6, 0),
    /* Kokiri jump   */ DMG_ENTRY(4, 0),
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
    bool hittable = (it != sClients.end()) && it->second.pose.valid && !it->second.pose.hidden && GameStarted();

    actor->shape.shadowAlpha = 0;
    if (!hittable) {
        actor->flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
        return true;
    }
    actor->flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED; // Links can lock on to him
    actor->colChkInfo.damageTable = (DamageTable*)&sResolveTable;
    dummy->cylinder.info.bumperFlags &= ~BUMP_NO_HITMARK;
    dummy->cylinder.dim.radius = 40;
    dummy->cylinder.dim.height = 125;

    if ((dummy->cylinder.base.acFlags & AC_HIT) && (sFrame >= sHitCooldown[clientId])) {
        nlohmann::json payload;
        payload["type"] = GDF_PKT_HIT;
        payload["targetClientId"] = clientId;
        payload["amount"] = actor->colChkInfo.damage;
        payload["effect"] = actor->colChkInfo.damageEffect;
        Anchor::Instance->SendJsonToRemote(payload);
        sHitCooldown[clientId] = sFrame + 8;
    }
    Collider_UpdateCylinder(actor, &dummy->cylinder);
    dummy->cylinder.dim.radius = 40;
    dummy->cylinder.dim.height = 125;
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
    sResolve = GDF_RESOLVE_MAX;
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
        if (!payload.value("ok", false) && (art >= 0) && (art < GDF_ART_COUNT)) {
            // Refused (that Link was in a cutscene, talking, ...): the magic comes back, and the cooldown resets
            gSaveContext.magic = MIN(gSaveContext.magic + sArts[art].cost, gSaveContext.magicCapacity);
            sArtReady[art] = sFrame;
            Flash(std::string(sArts[art].name) + " didn't take hold (try again)");
        }
    } else if (type == GDF_PKT_PACIFIED) {
        if (GdfInvasion_IsHero()) {
            Notification::Emit({ .prefix = "Invasion",
                                 .message = payload.value("by", std::string("A Link")) +
                                            " pacified Ganondorf! He's gone for " +
                                            std::to_string(payload.value("seconds", 30)) + " seconds." });
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
    // His magic: the full bar, refilling on its own (faster when he keeps away from the Links)
    if (!gSaveContext.isMagicAcquired || !gSaveContext.isDoubleMagicAcquired) {
        gSaveContext.isMagicAcquired = true;
        gSaveContext.isDoubleMagicAcquired = true;
        gSaveContext.magicLevel = 2;
        gSaveContext.magicFillTarget = 0x60;
        gSaveContext.magicCapacity = 0x60;
    }
    if (!IsBanished() && (gSaveContext.magicState == MAGIC_STATE_IDLE) &&
        (gSaveContext.magic < gSaveContext.magicCapacity)) {
        bool far = FindTargetLink(play) == 0;
        if ((sFrame % (far ? GDF_FPS : (GDF_FPS * 3 / 2))) == 0) {
            gSaveContext.magic++;
        }
    }
    // Resolve refills once he hasn't been hit for 5 s
    if (!IsBanished() && (sResolve < GDF_RESOLVE_MAX) && (sFrame - sLastHitFrame > 5 * GDF_FPS)) {
        sResolve = MIN(GDF_RESOLVE_MAX, sResolve + 2.0f / GDF_FPS);
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

static void RegisterInvasionPlay() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(TickInvasion);
}

static RegisterShipInitFunc playInitFunc(RegisterInvasionPlay, {});

// ---- HUD parts ----

static ImU32 Col(int r, int g, int b, int a) {
    return IM_COL32(r, g, b, a);
}

static void DrawText(ImDrawList* list, float size, ImVec2 pos, ImU32 col, const std::string& text) {
    list->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1, pos.y + 1), Col(0, 0, 0, 140), text.c_str());
    list->AddText(ImGui::GetFont(), size, pos, col, text.c_str());
}

// Ganondorf: his Resolve, his C-button arts, banishment, and messages
static void DrawInvaderHud(ImGuiViewport* vp, float scale) {
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.8f * fmaxf(scale, 1.0f);

    // Resolve, under the hearts and magic
    float x = vp->Pos.x + 26.0f * scale;
    float y = vp->Pos.y + 62.0f * scale;
    float w = 110.0f * scale;
    float h = 6.0f * scale;
    float f = CLAMP(sResolve / GDF_RESOLVE_MAX, 0.0f, 1.0f);
    list->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), Col(0, 0, 0, 120), 2.0f);
    list->AddRectFilled(ImVec2(x, y), ImVec2(x + w * f, y + h), Col(200, 40, 70, 210), 2.0f);
    DrawText(list, font * 0.8f, ImVec2(x + w + 6.0f, y - font * 0.25f), Col(230, 200, 210, 170), "Resolve");

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
        DrawText(list, font * 0.85f, ImVec2(cx, cy), c, text);
        cy += font;
    }

    // Banished
    if (IsBanished()) {
        list->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), Col(10, 0, 20, 150));
        std::string t = "Pacified... back in " + std::to_string((sBanishedUntil - sFrame) / GDF_FPS + 1) + " s";
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(font * 1.6f, FLT_MAX, 0.0f, t.c_str());
        DrawText(list, font * 1.6f,
                 ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, vp->Pos.y + (vp->Size.y - sz.y) * 0.5f),
                 Col(230, 200, 255, 230), t);
    }

    // A message about his last cast
    if (sFrame < sCastMessageUntil) {
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, sCastMessage.c_str());
        DrawText(list, font, ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, vp->Pos.y + vp->Size.y * 0.72f),
                 Col(230, 210, 255, 220), sCastMessage);
    }
}

// Ganondorf's Dark Arts menu
static void DrawArtsMenu(ImGuiViewport* vp, float scale) {
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.75f * fmaxf(scale, 1.0f);
    float cellW = 92.0f * scale;
    float cellH = 40.0f * scale;
    float gap = 4.0f * scale;
    float gridW = GDF_ART_COLS * cellW + (GDF_ART_COLS - 1) * gap;
    float gridH = GDF_ART_ROWS * cellH + (GDF_ART_ROWS - 1) * gap;
    float x0 = vp->Pos.x + (vp->Size.x - gridW) * 0.5f;
    float y0 = vp->Pos.y + (vp->Size.y - gridH) * 0.5f;
    static const char* sRows[GDF_ART_ROWS] = { "Obstacles", "Monsters", "Curses", "His own" };

    list->AddRectFilled(ImVec2(x0 - 16 * scale, y0 - 46 * scale),
                        ImVec2(x0 + gridW + 16 * scale, y0 + gridH + 52 * scale), Col(14, 4, 24, 215), 8.0f);
    DrawText(list, font * 1.4f, ImVec2(x0, y0 - 40 * scale), Col(220, 170, 255, 255), "Dark Arts");
    DrawText(list, font * 0.9f, ImVec2(x0 + 120 * scale, y0 - 34 * scale), Col(200, 190, 210, 190),
             "D-Pad: choose   C: put on that button   START/B: close");

    for (int i = 0; i < GDF_ART_COUNT; i++) {
        const DarkArt& a = sArts[i];
        int col = i % GDF_ART_COLS;
        int row = i / GDF_ART_COLS;
        ImVec2 p0(x0 + col * (cellW + gap), y0 + row * (cellH + gap));
        ImVec2 p1(p0.x + cellW, p0.y + cellH);
        bool selected = i == sCursor;
        ImU32 bg = a.ready ? Col(60, 20, 80, 220) : Col(35, 30, 40, 200);
        list->AddRectFilled(p0, p1, bg, 4.0f);
        if (selected) {
            list->AddRect(p0, p1, Col(255, 220, 120, 255), 4.0f, 0, 2.0f * scale);
        }
        DrawText(list, font * 0.85f, ImVec2(p0.x + 4 * scale, p0.y + 3 * scale),
                 a.ready ? Col(240, 225, 255, 255) : Col(150, 140, 160, 200), a.name);
        DrawText(list, font * 0.75f, ImVec2(p0.x + 4 * scale, p0.y + cellH - font * 0.95f),
                 Col(160, 200, 255, 200), std::to_string(a.cost) + " magic");
        for (int slot = 0; slot < 3; slot++) {
            if (SlotArt(slot) == i) {
                static const char* sTags[3] = { "<", "v", ">" };
                DrawText(list, font * 0.9f, ImVec2(p1.x - 12 * scale, p0.y + 2 * scale), Col(255, 200, 60, 255),
                         sTags[slot]);
            }
        }
    }
    // Rows and the selected art
    for (int row = 0; row < GDF_ART_ROWS; row++) {
        DrawText(list, font * 0.7f, ImVec2(x0 + gridW + 4 * scale, y0 + row * (cellH + gap) + 4 * scale),
                 Col(180, 160, 200, 160), sRows[row]);
    }
    const DarkArt& sel = sArts[sCursor];
    std::string desc = std::string(sel.desc) + "  (" + std::to_string(sel.cost) + " magic, " +
                       std::to_string(sel.cooldown) + " s)" + (sel.ready ? "" : "  - coming later");
    DrawText(list, font * 0.9f, ImVec2(x0, y0 + gridH + 12 * scale), Col(220, 210, 230, 230), desc);
    if (!GdfInvasion_AdventureStarted()) {
        DrawText(list, font * 0.85f, ImVec2(x0, y0 + gridH + 30 * scale), Col(255, 180, 120, 220),
                 "Truce: nothing can be cast until a Link enters the Deku Tree.");
    }
}

// A Link: what Ganondorf has cursed him with, and the gloom
static void DrawHeroHud(ImGuiViewport* vp, float scale) {
    ImDrawList* list = ImGui::GetForegroundDrawList(vp);
    float font = ImGui::GetFontSize() * 0.8f * fmaxf(scale, 1.0f);

    if (sCurses[5].active) {
        // Darkness closing in from the edges
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
        DrawText(list, font, ImVec2(vp->Pos.x + (vp->Size.x - sz.x) * 0.5f, y), Col(220, 120, 255, 220), t);
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
    for (const ChatLine& line : sChat) {
        u32 age = sFrame - line.frame;
        int alpha = sChatOpen ? 230 : (age < 10 * GDF_FPS) ? 220 : (age < 12 * GDF_FPS) ? 220 - (age - 10 * GDF_FPS) * 5 : 0;
        if (alpha <= 0) {
            continue;
        }
        std::string name = line.name + ": ";
        ImVec2 nsz = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, name.c_str());
        DrawText(list, font, ImVec2(x, y), Col(line.color.r, line.color.g, line.color.b, alpha), name);
        DrawText(list, font, ImVec2(x + nsz.x, y), Col(240, 240, 240, alpha), line.text);
        y += font * 1.15f;
    }

    if (!sChatOpen) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(x, y + 4.0f));
    ImGui::SetNextWindowSize(ImVec2(fminf(vp->Size.x * 0.45f, 520.0f * scale), 0));
    ImGui::SetNextWindowBgAlpha(0.6f);
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

    if ((Anchor::Instance == nullptr) || !Anchor::Instance->isEnabled) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float scale = vp->Size.y / 480.0f;

    DrawChat(vp, scale);
    if ((play == nullptr) || !Anchor::Instance->IsSaveLoaded() || (play->pauseCtx.state != 0)) {
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
