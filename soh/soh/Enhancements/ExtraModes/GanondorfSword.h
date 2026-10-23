#pragma once

// Play as Ganondorf: sword and sheath models for his sword stance (loaded from gdf_sword.bin, see the .cpp).
// Include after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

// Display lists in the sword's own frame (z along the blade, grip at z = -500). Null until loaded.
Gfx* GanondorfSword_GetSwordDL(void);
Gfx* GanondorfSword_GetSheathDL(void);

#ifdef __cplusplus
}
#endif
