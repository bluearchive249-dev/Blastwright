#pragma once

// Play as Ganondorf: replaces Link with the Ganon's Tower boss, controlled by the player.
// The boss itself lives in ovl_Boss_Ganon (params GDF_PARAMS_PLAYER). This file is the glue with Link.
// Include after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

// Called from Player_Update with the input Link is about to act on. Removes Link's own moves
// (sword, shield, rolls, items) while Ganondorf is out.
void PlayAsGanondorf_FilterInput(PlayState* play, Player* player, Input* input);

// Called from Player_Draw. Link stays invisible while Ganondorf is shown.
s32 PlayAsGanondorf_ShouldHideLink(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif
