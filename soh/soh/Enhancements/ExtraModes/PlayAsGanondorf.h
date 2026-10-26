#pragma once

// Play as Ganondorf: replaces Link with the Ganon's Tower boss, controlled by the player.
// The boss itself lives in ovl_Boss_Ganon (params GDF_PARAMS_PLAYER). This file is the glue with Link.
// Include after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

// Returned by PlayAsGanondorf_GetVoiceSfx for a voice clip that should not play at all
#define PLAY_AS_GANONDORF_MUTE 0

// Called from Player_Update with the input Link is about to act on. Removes Link's own moves
// (sword, shield, rolls, Z-targeting, items) while Ganondorf is out.
void PlayAsGanondorf_FilterInput(PlayState* play, Player* player, Input* input);

// True while Ganondorf's cape is up. It counts as any shield, including the Mirror Shield, so every projectile
// and puzzle that needs a shield accepts it.
s32 PlayAsGanondorf_IsCapeShielding(void);

// Ganondorf doesn't need air underwater (no drowning timer).
s32 PlayAsGanondorf_BreathesUnderwater(PlayState* play, Player* player);

// Called from Player_Draw. Link stays invisible while Ganondorf is shown.
s32 PlayAsGanondorf_ShouldHideLink(PlayState* play, Player* player);
s32 PlayAsGanondorf_IsCrawlFlame(PlayState* play, Player* player);
s32 PlayAsGanondorf_UseGanonHorse(void);
s32 PlayAsGanondorf_IsSprinting(void);
s32 PlayAsGanondorf_GetItemHoldPos(PlayState* play, Player* player, Vec3f* pos);

// Used by the camera instead of Player_GetHeight, so it frames Ganondorf instead of Link.
f32 PlayAsGanondorf_GetCameraHeight(Player* player);

// First-person look (C-Up): fills in Ganondorf's eye position and returns true, so the camera looks from there
s32 PlayAsGanondorf_GetFirstPersonEye(Player* player, Vec3f* eye);

// Link's ocarina is Ganondorf's organ: he plays it, and the songs use an organ sound
s32 PlayAsGanondorf_UseOrganOcarina(void);

// L/Z levitation is active (Ganondorf floats; otherwise he stands and walks on the ground)
s32 PlayAsGanondorf_IsLevitating(void);

// Ganon (the D-Pad Right transformation) is out instead of Ganondorf
s32 PlayAsGanondorf_IsGanonOut(PlayState* play);

// How much faster than Link Ganondorf runs (1.0 when he isn't out)
f32 PlayAsGanondorf_GetRunSpeedFactor(PlayState* play, Player* player);

// Called from Player_PlaySfx for Link's voice clips: Ganondorf's voice instead, or PLAY_AS_GANONDORF_MUTE.
u16 PlayAsGanondorf_GetVoiceSfx(Player* player, u16 sfxId);

// Title screen option: the game opens in Ganondorf's chamber, with him playing the organ, instead of Hyrule Field.
// Checked by the opening when it picks where the title screen takes place.
s32 PlayAsGanondorf_UseTitleScreen(void);

// True while that title screen is running
s32 PlayAsGanondorf_IsTitleScreen(PlayState* play);

// Where NPCs look when they look at him (his head)
s32 PlayAsGanondorf_GetLookAtPos(Player* player, Vec3f* pos);

// Navi is a Poe minion: drawn as a purple-red Poe, with a Poe's laugh and her own lines
s32 PlayAsGanondorf_UsePoeNavi(void);

// He kicks doors off their hinges instead of opening them
s32 PlayAsGanondorf_ShouldKickDoors(void);

// His own footsteps replace Link's while he walks on the ground
s32 PlayAsGanondorf_OwnsFootsteps(Player* player);
u16 PlayAsGanondorf_GetNaviSfx(u16 sfxId);

// His own HUD: purple diamonds for hearts and a magic meter wreathed in flames
s32 PlayAsGanondorf_UseGanondorfHud(void);

// L is his levitation, so it doesn't toggle the minimap (always shown)
s32 PlayAsGanondorf_OwnsLButton(void);

// A makes him sprint where Link would attack, jump or put away (the A button then reads "Run")
s32 PlayAsGanondorf_ARuns(void);

#ifdef __cplusplus
}
#endif
