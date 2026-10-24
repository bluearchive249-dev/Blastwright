// SoH: "Play as Ganondorf", custom title screen logo (see GanondorfLogo.h)

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"

#include "GanondorfLogo.h"

static std::vector<unsigned char> sPixels;
static unsigned int sWidth = 0;
static unsigned int sHeight = 0;
static bool sTried = false;

static void LoadLogo() {
    const std::string candidates[] = {
        Ship::Context::GetPathRelativeToAppDirectory("gdf_logo.bin"),
        Ship::Context::GetPathRelativeToAppDirectory("mods/gdf_logo.bin"),
        "gdf_logo.bin",
    };
    FILE* file = nullptr;

    sTried = true;
    for (const std::string& path : candidates) {
        file = fopen(path.c_str(), "rb");
        if (file != nullptr) {
            break;
        }
    }
    if (file == nullptr) {
        return;
    }

    unsigned char header[8];
    if (fread(header, 1, sizeof(header), file) == sizeof(header) && header[0] == 'G' && header[1] == 'L' &&
        header[2] == 'O' && header[3] == 'G') {
        unsigned int w = header[4] | (header[5] << 8);
        unsigned int h = header[6] | (header[7] << 8);

        if ((w > 0) && (h > 0) && (w <= 2048) && (h <= 2048)) {
            sPixels.resize((size_t)w * h * 4);
            if (fread(sPixels.data(), 1, sPixels.size(), file) == sPixels.size()) {
                sWidth = w;
                sHeight = h;
            } else {
                sPixels.clear();
            }
        }
    }
    fclose(file);
}

extern "C" const unsigned char* GanondorfLogo_Get(unsigned int* width, unsigned int* height) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.TitleLogo"), 1)) {
        return nullptr;
    }
    if (!sTried) {
        LoadLogo();
    }
    if (sPixels.empty()) {
        return nullptr;
    }
    *width = sWidth;
    *height = sHeight;
    return sPixels.data();
}

// ---- Placement and the flame mask ----
#define LOGO_MAX_WIDTH 288
#define LOGO_MAX_HEIGHT 160
#define LOGO_CENTER_X 160
#define LOGO_CENTER_Y 100
#define FLAME_MARGIN 16 // how far past the logo the flames can reach
#define FLAME_GROW 5    // the silhouette grows this much before softening
#define FLAME_SOFTEN 4  // box blur radius (three passes, about a Gaussian)

static std::vector<unsigned char> sMask;
static int sMaskLeft, sMaskTop, sMaskWidth, sMaskHeight;

extern "C" void GanondorfLogo_GetRect(int* left, int* top, int* width, int* height) {
    int w = LOGO_MAX_WIDTH;
    int h = (sWidth > 0) ? (int)((LOGO_MAX_WIDTH * sHeight) / sWidth) : LOGO_MAX_HEIGHT;

    if (h > LOGO_MAX_HEIGHT) {
        h = LOGO_MAX_HEIGHT;
        w = (sHeight > 0) ? (int)((LOGO_MAX_HEIGHT * sWidth) / sHeight) : LOGO_MAX_WIDTH;
    }
    *left = LOGO_CENTER_X - w / 2;
    *top = LOGO_CENTER_Y - h / 2;
    *width = w;
    *height = h;
}

static void BoxBlur(std::vector<float>& img, int w, int h, int r) {
    std::vector<float> tmp(img.size());

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0.0f;
            int n = 0;
            for (int k = -r; k <= r; k++) {
                int xx = x + k;
                if (xx >= 0 && xx < w) {
                    sum += img[y * w + xx];
                }
                n++;
            }
            tmp[y * w + x] = sum / n;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0.0f;
            int n = 0;
            for (int k = -r; k <= r; k++) {
                int yy = y + k;
                if (yy >= 0 && yy < h) {
                    sum += tmp[yy * w + x];
                }
                n++;
            }
            img[y * w + x] = sum / n;
        }
    }
}

static void BuildMask() {
    int left, top, w, h;

    GanondorfLogo_GetRect(&left, &top, &w, &h);
    sMaskLeft = std::max(0, left - FLAME_MARGIN);
    sMaskTop = std::max(0, top - FLAME_MARGIN);
    sMaskWidth = std::min(320, left + w + FLAME_MARGIN) - sMaskLeft;
    sMaskHeight = std::min(240, top + h + FLAME_MARGIN) - sMaskTop;

    // The logo's coverage on screen pixels (area average of its alpha)
    std::vector<float> cover((size_t)sMaskWidth * sMaskHeight, 0.0f);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx0 = (int)((long long)x * sWidth / w), sx1 = std::max(sx0 + 1, (int)((long long)(x + 1) * sWidth / w));
            int sy0 = (int)((long long)y * sHeight / h), sy1 = std::max(sy0 + 1, (int)((long long)(y + 1) * sHeight / h));
            float sum = 0.0f;
            int n = 0;
            for (int sy = sy0; sy < sy1 && sy < (int)sHeight; sy++) {
                for (int sx = sx0; sx < sx1 && sx < (int)sWidth; sx++) {
                    sum += sPixels[((size_t)sy * sWidth + sx) * 4 + 3];
                    n++;
                }
            }
            int mx = left + x - sMaskLeft, my = top + y - sMaskTop;
            if (n > 0 && mx >= 0 && mx < sMaskWidth && my >= 0 && my < sMaskHeight) {
                cover[(size_t)my * sMaskWidth + mx] = sum / (n * 255.0f);
            }
        }
    }

    // Grow the silhouette (a round max filter), then soften it into a glow
    std::vector<float> grown(cover.size(), 0.0f);
    for (int y = 0; y < sMaskHeight; y++) {
        for (int x = 0; x < sMaskWidth; x++) {
            float m = 0.0f;
            for (int dy = -FLAME_GROW; dy <= FLAME_GROW; dy++) {
                for (int dx = -FLAME_GROW; dx <= FLAME_GROW; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (dx * dx + dy * dy > FLAME_GROW * FLAME_GROW || xx < 0 || yy < 0 || xx >= sMaskWidth ||
                        yy >= sMaskHeight) {
                        continue;
                    }
                    m = std::max(m, cover[(size_t)yy * sMaskWidth + xx]);
                }
            }
            grown[(size_t)y * sMaskWidth + x] = m;
        }
    }
    for (int pass = 0; pass < 3; pass++) {
        BoxBlur(grown, sMaskWidth, sMaskHeight, FLAME_SOFTEN);
    }

    sMask.resize(grown.size());
    for (size_t i = 0; i < grown.size(); i++) {
        sMask[i] = (unsigned char)std::min(255.0f, grown[i] * 1.4f * 255.0f);
    }
}

extern "C" const unsigned char* GanondorfLogo_GetFlameMask(int* left, int* top, int* width, int* height) {
    if (sPixels.empty()) {
        return nullptr;
    }
    if (sMask.empty()) {
        BuildMask();
    }
    *left = sMaskLeft;
    *top = sMaskTop;
    *width = sMaskWidth;
    *height = sMaskHeight;
    return sMask.data();
}
