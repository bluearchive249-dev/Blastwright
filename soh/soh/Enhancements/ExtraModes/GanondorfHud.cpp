// "Play as Ganondorf": purple diamond "hearts" and the flaming magic meter. Everything is generated here, nothing is
// read from the game's assets.
#include "GanondorfHud.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

// ---- Diamonds ----
// IA8 texels, as in the heart textures: 0x01 the faint outer edge, 0x0F the dark border (environment colour), 0xFF the
// fill (primitive colour); the empty part keeps a see-through border (0x0B) and a faint inside (0x04).

alignas(8) static unsigned char sDiamonds[GDF_DIAMOND_MAX][16 * 16];
static bool sDiamondsBuilt = false;

static void BuildDiamonds() {
    for (int kind = 0; kind < GDF_DIAMOND_MAX; kind++) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                float d = std::fabs(x - 7.5f) + std::fabs(y - 7.5f);
                bool left = x < 8;
                bool top = y < 8;
                bool filled;

                switch (kind) {
                    case GDF_DIAMOND_FULL:
                        filled = true;
                        break;
                    case GDF_DIAMOND_QUARTER:
                        filled = left && top;
                        break;
                    case GDF_DIAMOND_HALF:
                        filled = left;
                        break;
                    case GDF_DIAMOND_THREE_QUARTER:
                        filled = left || !top;
                        break;
                    default:
                        filled = false;
                        break;
                }

                unsigned char t;
                if (d > 8.0f) {
                    t = 0x00;
                } else if (d > 7.0f) {
                    t = 0x01;
                } else if (d > 5.0f) {
                    t = filled ? 0x0F : 0x0B;
                } else if (!filled) {
                    t = 0x04;
                } else if (d < 1.5f && x < 8 && y < 8) {
                    t = 0xFF; // the facets meet in a bright spark
                } else if ((x < 8) == (y < 8)) {
                    // A cut gem: the top-left and bottom-right facets catch the light, the other two are a shade darker
                    t = (x + y < 15) ? 0xFF : 0xCF;
                } else {
                    t = (x + y < 15) ? 0xDF : 0xAF;
                }
                sDiamonds[kind][y * 16 + x] = t;
            }
        }
    }
    sDiamondsBuilt = true;
}

extern "C" const unsigned char* GanondorfHud_GetDiamondTex(int kind) {
    if (!sDiamondsBuilt) {
        BuildDiamonds();
    }
    if (kind < 0 || kind >= GDF_DIAMOND_MAX) {
        kind = GDF_DIAMOND_FULL;
    }
    return sDiamonds[kind];
}

extern "C" const unsigned char* GanondorfHud_GetDiamondForFraction(int fraction) {
    // Matches sHeartTextures: 0 full, 1-5 quarter, 6-10 half, 11-15 three quarters
    if (fraction <= 0) {
        return GanondorfHud_GetDiamondTex(GDF_DIAMOND_FULL);
    } else if (fraction <= 5) {
        return GanondorfHud_GetDiamondTex(GDF_DIAMOND_QUARTER);
    } else if (fraction <= 10) {
        return GanondorfHud_GetDiamondTex(GDF_DIAMOND_HALF);
    }
    return GanondorfHud_GetDiamondTex(GDF_DIAMOND_THREE_QUARTER);
}

// ---- Magic meter flames ----
// One mask per bar length (single and double magic). Each stays at its own address, as the renderer caches textures
// by address.

struct BarMask {
    int capacity = -1;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> texels;
};

static BarMask sBarMasks[2];

static void BuildBarMask(BarMask& mask, int capacity) {
    const int barWidth = capacity + 16;
    mask.capacity = capacity;
    mask.width = barWidth + 2 * GDF_BAR_FLAME_PAD_X;
    mask.height = 16 + GDF_BAR_FLAME_PAD_TOP + GDF_BAR_FLAME_PAD_BOTTOM;
    mask.texels.assign((size_t)mask.width * mask.height, 0);

    // The bar's visible outline: rows 2-13 of its 16, its rounded caps two pixels in from each end
    const float left = GDF_BAR_FLAME_PAD_X + 2.0f;
    const float right = GDF_BAR_FLAME_PAD_X + barWidth - 2.0f;
    const float top = GDF_BAR_FLAME_PAD_TOP + 2.0f;
    const float bottom = GDF_BAR_FLAME_PAD_TOP + 13.0f;

    for (int y = 0; y < mask.height; y++) {
        for (int x = 0; x < mask.width; x++) {
            float px = x + 0.5f;
            float py = y + 0.5f;
            float dx = std::max({ left - px, 0.0f, px - right });
            float dy;
            if (py < top) {
                dy = (top - py) * 0.55f; // flames lick upward, so they reach farthest above the bar
            } else if (py > bottom) {
                dy = (py - bottom) * 1.6f;
            } else {
                dy = 0.0f;
            }
            dx *= 1.15f;
            float dist = std::sqrt(dx * dx + dy * dy);
            // Solid at the bar, fading out about eight pixels away: the scrolling flame texture eats into the fade
            float v = 1.0f - dist / 8.0f;
            v = std::clamp(v * 1.3f, 0.0f, 1.0f);
            mask.texels[(size_t)y * mask.width + x] = (unsigned char)(v * 255.0f);
        }
    }
}

extern "C" const unsigned char* GanondorfHud_GetBarFlameMask(int capacity, int* width, int* height) {
    if (capacity <= 0) {
        return nullptr;
    }
    capacity = std::min(capacity, 0x60);
    if (capacity != 0x30 && capacity != 0x60) {
        return nullptr; // the meter is still growing in (only when magic is first gained): no flames until it settles
    }
    BarMask& mask = sBarMasks[(capacity == 0x30) ? 0 : 1];
    if (mask.capacity != capacity) {
        BuildBarMask(mask, capacity);
    }
    *width = mask.width;
    *height = mask.height;
    return mask.texels.data();
}
