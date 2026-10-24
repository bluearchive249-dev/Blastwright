// SoH: "Play as Ganondorf", custom title screen logo (see GanondorfLogo.h)

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
