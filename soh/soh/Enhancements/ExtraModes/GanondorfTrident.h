#pragma once

// Play as Ganondorf: his tridents (loaded from gdf_tridents.bin, see the .cpp). Include after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

#define GDF_TRIDENT_COUNT 4

// The weapon picked in the menu: 0 the sword, 1-4 a trident
s32 GanondorfTrident_GetSelected(void);

// Display list of trident `index` (0-3) in the sword's frame (z along the shaft, prongs at +z, grip at z = -500), or
// null without the model file. Its head (for the hitbox) spans *baseZ to *tipZ along z.
Gfx* GanondorfTrident_GetDL(s32 index, f32* baseZ, f32* tipZ);

// The B button's icon for his trident stance (32x32 RGBA32), read from "gdf_bicon_trident.bin" next to the game (made
// from the player's own trident model, like gdf_tridents.bin: not part of this repository), or null without it
u8* GanondorfTrident_GetBIcon(void);

#ifdef __cplusplus
}
#endif
