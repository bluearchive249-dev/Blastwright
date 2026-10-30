#pragma once

// SoH: "Play as Ganondorf", Invasion mode: online (Anchor) co-op where one player is the Hero (Link, playing the game)
// and the other the Invader (Ganondorf, making it harder). See GanondorfInvasion.cpp.
// Include after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

#define GDF_INVASION_OFF 0
#define GDF_INVASION_HERO 1
#define GDF_INVASION_INVADER 2

// Ganondorf's pose as sent to the other players (PLAYER_UPDATE), and what his puppet on their screen is drawn with
#define GDF_NET_JOINTS 26 // gGanondorfSkel: the root translation and 25 limbs
typedef struct {
    u8 valid;
    u8 hidden;     // not drawn (first person, a crawlspace, ...)
    u8 swordState; // GDF_SWORD_OFF / SHEATHED / DRAWN
    Vec3f pos;
    s16 rotY;
    f32 floorY;
    Vec3s joints[GDF_NET_JOINTS];
} GdfNetPose;

// The local player's role (0 when the mode is off or Anchor isn't on)
s32 GdfInvasion_Role(void);
s32 GdfInvasion_IsInvader(void);
s32 GdfInvasion_IsHero(void);
// The truce: until Link has entered the Deku Tree, nothing Ganondorf does can touch him
s32 GdfInvasion_AdventureStarted(void);

// Ganondorf's puppet (a BossGanon with GDF_PARAMS_PUPPET) on the Hero's screen
s32 GdfInvasion_GetPuppetPose(Actor* puppet, GdfNetPose* out);
void GdfInvasion_PuppetDestroyed(Actor* puppet);

// ovl_Boss_Ganon: the local Ganondorf's pose to send
s32 BossGanon_Player_GetNetPose(GdfNetPose* out);

#ifdef __cplusplus
}

#include <nlohmann/json.hpp>
struct Actor;

// Anchor's side
void GdfInvasion_WritePose(nlohmann::json& payload);
void GdfInvasion_ReadPose(uint32_t clientId, const nlohmann::json& payload);
// Called from the Anchor dummy of each other player: true when that player is Ganondorf (the dummy Link isn't drawn)
bool GdfInvasion_UpdateDummy(Actor* dummy, uint32_t clientId, bool inScene);
bool GdfInvasion_IsClientGanondorf(uint32_t clientId);
// The lobby (seats, start) and options, in the Anchor menu
void GdfInvasion_DrawMenu();
// The lobby's seat (client state) and the game's start (room state), along with Anchor's own
void GdfInvasion_WriteClientState(nlohmann::json& state);
void GdfInvasion_ReadClientState(uint32_t clientId, const nlohmann::json& state);
void GdfInvasion_WriteRoomState(nlohmann::json& state);
void GdfInvasion_ReadRoomState(const nlohmann::json& state);
// The Invader's HUD (where Link is), added with the other GUI windows
#include <memory>
namespace Ship {
class GuiWindow;
}
std::shared_ptr<Ship::GuiWindow> GdfInvasion_CreateHudWindow();
#endif
