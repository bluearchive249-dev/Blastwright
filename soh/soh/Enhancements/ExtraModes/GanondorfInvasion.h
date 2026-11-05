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
#define GDF_NET_BALLS 12  // his light balls and Big Magic in flight
#define GDF_NET_FX 74     // his effect state (cape, glows, aura, organ, ...), packed as numbers
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
// Dev test mode: offline, his Dark Arts land on himself; and whether his own game's monsters leave him alone
s32 GdfInvasion_TestMode(void);
s32 GdfInvasion_ShieldedFromWorld(void);
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
// Phantom Step: he's moved with Link (snapped, not walked over)
void BossGanon_Player_NetSnap(void);
// The actor in his fist (NULL when he holds no one)
Actor* BossGanon_Player_GetGrabbedActor(void);
// World sync: Ganondorf kicked a door in his game (where it is, where he kicked from); the Links' doors fly too
void GdfInvasion_NoteDoorKick(Vec3f* doorPos, Vec3f* fromPos);
void EnDoor_RemoteKick(Actor* door, Vec3f* from);
// World sync, bosses: each actor's skeleton (noted as it's set up), and the boss following the host's
extern Actor* gGdfInitActor;
void GdfInvasion_NoteSkel(SkelAnime* skelAnime);
void GdfInvasion_ForgetActor(Actor* actor);
void GdfInvasion_AfterBossUpdate(Actor* actor);
void GdfInvasion_AfterBgUpdate(Actor* actor);
// A door he kicked open (unlocked or unbarred) stays so in his game, whatever the host's flags say
void GdfInvasion_KeepKickedSwitch(s32 flag);
void GdfInvasion_KeepKickedClear(s32 room);

// The throat grab on another player's Link: whether he may, which player a dummy is, and what his fist does
u32 GdfInvasion_DummyClientId(Actor* actor);
s32 GdfInvasion_CanGrabLinks(void);
void GdfInvasion_NetGrabHold(u32 clientId, Vec3f* hand, f32 neck, s16 yaw);
void GdfInvasion_NetGrabEnd(u32 clientId, s32 thrown, s16 yaw);

// Invasion mode: his attacks cost magic (false: not enough, the attack doesn't start), and can hurt the Links
s32 GdfInvasion_PayAttack(s32 cost);
// Made by a Dark Art (or its copy in his game): stays put by day and away from its home (Stalchildren)
s32 GdfInvasion_IsSummoned(Actor* actor);
// The guard hunt: his hunters and the alarm bell shown in the Links' games too
void GdfInvasion_MirrorActor(Actor* actor);
void GdfInvasion_NoteSlain(Actor* actor);
s32 GdfInvasion_IsRemoteCopy(Actor* actor);
s32 GdfInvasion_GanondorfPos(Vec3f* out);
// Pacified: he kneels as if beaten, then is gone in a burst of smoke until he's back
s32 GdfInvasion_IsPacified(void);
s32 GdfInvasion_IsVanished(void);
// Ganon form costs Resolve and magic up front and drains magic while he's out (false: he can't, or it ran dry)
s32 GdfInvasion_PayGanonForm(void);
s32 GdfInvasion_GanonFormUpkeep(void);
// Ganon (the Invader's Ganon form) as sent to the Links, and his puppet on their screens (BossGanon2 puppet)
#define GDF_NET_GANON_JOINTS 80
typedef struct {
    u8 valid;
    u8 form; // 0: Ganondorf transforming, 1/2: Ganon
    u8 hidden;
    u8 swords;
    u8 eye;
    u8 jointCount;
    Vec3f pos;
    s16 rotY;
    f32 yOffset;
    f32 glow;
    Vec3s joints[GDF_NET_GANON_JOINTS];
} GdfNetGanon;
s32 BossGanon2_Player_GetNetPose(GdfNetGanon* out);
s32 GdfInvasion_GetGanonPose(Actor* puppet, GdfNetGanon* out);
void GdfInvasion_GanonPuppetDestroyed(Actor* puppet);
// PlayAsGanondorf.cpp: Ganon's battle theme (on for the Links while the Invader's Ganon is about)
void PlayAsGanondorf_SetGanonMusic(PlayState* play, s32 on);
s32 PlayAsGanondorf_IsGanonOut(PlayState* play);
// Dark Link driven by Ganondorf: his input in place of Dark Link's own (false when he isn't driving him)
s32 GdfInvasion_DarkLinkInput(Actor* actor, Input* input, s16 camYaw);
s32 GdfInvasion_DarkLinkPossessed(Actor* actor);
s32 GdfInvasion_CanHurtLinks(void);
// The light ball volley (as in the tower fight): a Link swings at Ganondorf's ball and sends it back, he swings and
// sends it back again; whoever misses is hit. A Link's game decides for the Link, Ganondorf's for him.
void GdfInvasion_SendVolley(Vec3f* ballPos);
// Aero Alchemy: the air around this Link is water (he swims)
s32 GdfInvasion_AeroAlchemyActive(void);
void GdfInvasion_SendBallHit(Vec3f* ballPos);
void GdfInvasion_BallHurtLink(PlayState* play);
void GdfInvasion_BallHitGanondorf(void);
void BossGanon_Player_BallVolleyed(Vec3f* ballPos);
void BossGanon_Player_BallBurstAt(Vec3f* ballPos);
// Revival: a fallen Link is down for a while, and another Link standing by him brings him back (3 hearts)
s32 GdfInvasion_HoldDowned(PlayState* play, Player* player);
s32 GdfInvasion_ReviveHealth(s32 full);
// His Resolve, drawn as a white meter under his magic meter
s32 GdfInvasion_ShowResolveMeter(void);
f32 GdfInvasion_ResolveFraction(void);
f32 GdfInvasion_ResolveMeterLength(void);
// Kokiri Forest's peace: a young Link who has never had both sword and shield on (0: the local Link)
s32 GdfInvasion_LinkAtPeace(u32 clientId);
s32 GdfInvasion_CanHurtLink(u32 clientId);
s32 GdfInvasion_CanGrabLink(u32 clientId);

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
// The song player: a song started (notes, ending with a zero-length one) or stopped (nullptr), heard by the others
struct OcarinaNote;
void GdfInvasion_SendSong(const OcarinaNote* notes, int count);
// Another player's song is playing here (the local song player leaves the ocarina alone meanwhile)
bool GdfInvasion_HearingSong();
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
