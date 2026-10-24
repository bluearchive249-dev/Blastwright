#pragma once

// "Play as Ganondorf": killing NPCs (an option). A killed NPC is remembered in the save file and never appears again.
#ifdef __cplusplus
extern "C" {
#endif

struct Actor;

// The option is on
int GanondorfKills_IsEnabled(void);

// Remembers the NPC as dead (in the current save, once the game is saved)
void GanondorfKills_Record(struct Actor* actor);

// How many NPCs are dead in this save
int GanondorfKills_Count(void);

#ifdef __cplusplus
}
#endif
