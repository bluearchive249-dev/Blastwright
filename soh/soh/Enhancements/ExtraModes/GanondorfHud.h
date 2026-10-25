#pragma once

// "Play as Ganondorf": his own HUD. Purple diamonds in place of the hearts and a magic meter that burns with the title
// screen's flames. All of the art here is drawn by code (no game assets).
#ifdef __cplusplus
extern "C" {
#endif

enum GanondorfDiamond {
    GDF_DIAMOND_FULL,
    GDF_DIAMOND_QUARTER,
    GDF_DIAMOND_HALF,
    GDF_DIAMOND_THREE_QUARTER,
    GDF_DIAMOND_EMPTY,
    GDF_DIAMOND_MAX
};

// A 16x16 IA8 diamond, encoded like the heart textures (dark border, bright fill, faint empty part)
const unsigned char* GanondorfHud_GetDiamondTex(int kind);

// Picks the diamond that stands in for a heart texture (16 health units to a heart)
const unsigned char* GanondorfHud_GetDiamondForFraction(int fraction);

// The magic meter flames' mask (I8): the bar's outline, grown and softened, reaching higher above the bar than below.
// The bar (both end caps included) is capacity + 16 pixels wide and 16 high; the mask starts GDF_BAR_FLAME_PAD_X to its
// left and GDF_BAR_FLAME_PAD_TOP above it.
#define GDF_BAR_FLAME_PAD_X 8
#define GDF_BAR_FLAME_PAD_TOP 14
#define GDF_BAR_FLAME_PAD_BOTTOM 4
const unsigned char* GanondorfHud_GetBarFlameMask(int capacity, int* width, int* height);

#ifdef __cplusplus
}
#endif
