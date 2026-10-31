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
#include <map>
#include <string>

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
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
        c.pose.valid = true;
        c.lastFrame = sFrame;
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

void GdfInvasionHud::Draw() {
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
    state["gdfStart"] = CVarGetInteger(CVAR_INVASION("StartCount"), 0);
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

static void SeatButton(int seat) {
    uint32_t holder = SeatHolder(seat);
    bool mine = (holder != 0) && (holder == Anchor::Instance->ownClientId);
    bool taken = (holder != 0) && !mine;
    const SeatInfo& info = sSeats[seat];
    std::string who = mine ? "You" : taken ? Anchor::Instance->clients[holder].name : "Free";
    ImVec4 col(info.color.r / 255.0f, info.color.g / 255.0f, info.color.b / 255.0f, 1.0f);
    float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

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
        if (Anchor::Instance->roomState.ownerClientId == Anchor::Instance->ownClientId) {
            if (ImGui::Button("Back to the lobby (everyone)")) {
                CVarSetInteger(CVAR_INVASION("StartCount"), 0);
                SaveCVars();
                Anchor::Instance->SendPacket_UpdateRoomState();
            }
        }
        return;
    }

    ImGui::Text("Lobby: %d / %d players", online, GDF_MAX_PLAYERS);
    ImGui::TextDisabled("Take a seat: one Ganondorf and up to six Links.");
    SeatButton(GDF_SEAT_GANONDORF);
    for (int seat = GDF_SEAT_FIRST_LINK; seat < GDF_SEAT_COUNT; seat++) {
        SeatButton(seat);
        if (((seat - GDF_SEAT_FIRST_LINK) % 2) == 0) {
            ImGui::SameLine();
        }
    }
    ImGui::NewLine();

    if (online > GDF_MAX_PLAYERS) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Too many players: at most %d (1 Ganondorf + 6 Links).",
                           GDF_MAX_PLAYERS);
    }
    bool ready = ganondorf && (links > 0) && (seated == online) && (online <= GDF_MAX_PLAYERS);
    if (Anchor::Instance->roomState.ownerClientId == Anchor::Instance->ownClientId) {
        if (!ready) {
            ImGui::TextDisabled(!ganondorf    ? "Waiting for someone to be Ganondorf..."
                                : (links == 0) ? "Waiting for at least one Link..."
                                : (seated < online) ? "Waiting for everyone to take a seat..."
                                                    : "Too many players.");
        }
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Start the game", ImVec2(-1.0f, 0.0f))) {
            CVarSetInteger(CVAR_INVASION("StartCount"), CVarGetInteger(CVAR_INVASION("StartCount"), 0) + 1);
            SaveCVars();
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
