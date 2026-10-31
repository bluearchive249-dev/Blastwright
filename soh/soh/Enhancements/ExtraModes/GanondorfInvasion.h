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
#define GDF_NET_BALLS 8   // his light balls and Big Magic in flight
#define GDF_NET_FX 72     // his effect state (cape, glows, aura, organ, ...), packed as numbers
#define GDF_NET_SFX 16    // sounds he made since the last packet

typedef struct {
    u8 kind; // 0 = light ball, 1 = Big Magic
    Vec3f pos;
    Vec3s rot;
    f32 scale;
    f32 fwork0;
    f32 fwork1;
    f32 floorY;
} GdfNetBall;

typedef struct {
    u8 valid;
    u8 hidden;     // not drawn (first person, a crawlspace, ...)
    u8 swordState; // GDF_SWORD_OFF / SHEATHED / DRAWN
    Vec3f pos;
    s16 rotY;
    f32 floorY;
    Vec3s joints[GDF_NET_JOINTS];
    u8 fxCount;
    f32 fx[GDF_NET_FX];
    u8 ballCount;
    GdfNetBall balls[GDF_NET_BALLS];
} GdfNetPose;

// ovl_Boss_Ganon: a sound the local Ganondorf (or his light balls) made, to be heard on the others' screens too
void GdfInvasion_NoteSfx(u16 sfxId);

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
// ovl_Boss_Ganon: blocking (a Link's hits do half), a hit's flinch, casting, and being pacified
s32 BossGanon_Player_IsBlockingNet(void);
void BossGanon_Player_NetHurt(s32 heavy);
void BossGanon_Player_CastPose(void);
void BossGanon_Player_NetPacified(void);

// Ganondorf may start an attack (not while his Dark Arts menu is open or he's pacified)
s32 GdfInvasion_CanAct(void);
// His C-buttons: true when a Dark Art on it was cast (or tried), so the button does nothing else
s32 GdfInvasion_HandleCButtons(PlayState* play);
// From KaleidoSetup_Update: true when START opened or closed his Dark Arts menu instead of pausing
s32 GdfInvasion_KaleidoHook(PlayState* play);
// From Player: what the Dark Arts menu, banishment and curses do to the local Link's input
void GdfInvasion_FilterLinkInput(PlayState* play, Input* input);

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
// A Link's side: the dummy of the Ganondorf player (his hitbox for the Links' attacks on his Resolve)
bool GdfInvasion_UpdateGanondorfDummy(Actor* actor, uint32_t clientId);
// Invasion packets (GDF_*): hits, curses, pacification, chat
void GdfInvasion_HandlePacket(const nlohmann::json& payload);
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
