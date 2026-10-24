#pragma once

#ifdef __cplusplus
// Menu: the song list and the rescan button
void GanondorfSongs_DrawMenu();
extern "C" {
#endif

// The ocarina is out (and the song player is on): the D-pad controls the song player
s32 GanondorfSongs_IsOcarinaOut(void);

#ifdef __cplusplus
}
#endif
