#ifndef LINK_GANON_CAPE_H
#define LINK_GANON_CAPE_H

// Lets Link wear Ganondorf's physics cape (EnGanonMant) from the Ganon's Tower fight.

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;
struct EnGanonMant;

// Called from Player_PostLimbDrawGameplay for every limb Link draws
void LinkGanonCape_RecordLimb(struct PlayState* play, struct Player* player, int limbIndex);

// Called at the start of EnGanonMant_Draw for Link's cape. Sets the attach points and physics
// values for this frame. Returns 0 when the cape shouldn't be drawn.
int LinkGanonCape_PrepareDraw(struct EnGanonMant* cape, struct PlayState* play);

// Builds this frame's hood vertices (same 12x12 layout as the cape's, drawn with gMantDL).
// Returns NULL when the hood is off. Call after the cape's own vertices are updated.
void* LinkGanonCape_BuildHood(struct EnGanonMant* cape, struct PlayState* play);

// Called from Player_OverrideLimbDrawGameplayCommon. While the hood is on, hides the cap's tail
// (PLAYER_LIMB_HAT) and tints the cap dome in the head mesh with the hair color. dList is a Gfx**.
void LinkGanonCape_OverrideLimb(struct PlayState* play, struct Player* player, int limbIndex, void** dList);

// Called from Player_DrawImpl with the tunic color it applied (valid = 0 if it didn't apply one)
void LinkGanonCape_SetTunicColor(int valid, unsigned char r, unsigned char g, unsigned char b);

#ifdef __cplusplus
}
#endif

#endif
