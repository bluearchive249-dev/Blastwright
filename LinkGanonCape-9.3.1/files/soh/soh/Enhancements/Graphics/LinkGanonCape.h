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

#ifdef __cplusplus
}
#endif

#endif
