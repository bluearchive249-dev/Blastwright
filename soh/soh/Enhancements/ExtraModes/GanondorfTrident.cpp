// Play as Ganondorf: his tridents, a weapon choice for his sword stance (D-Pad Left) in the menu.
//
// The models are not part of this repository. They're read at runtime from "gdf_tridents.bin" next to the game (made
// from the player's own model files, see the format below). Without the file the sword is used.
//
// gdf_tridents.bin (little endian):
//   char[4] "GDFT", u16 version (2), u16 tridentCount
//   trident: char name[32], s16 headBaseZ, s16 tipZ, u16 texWidth, u16 texHeight,
//            texWidth * texHeight * RGBA5551 (big endian, like the game's own textures),
//            u16 batchCount
//   batch:   u8 vtxCount (<= 32), u8 triCount,
//            vtxCount * { s16 x, y, z; s16 s, t (texels, 10.5 fixed); s8 nx, ny, nz; u8 alpha }, triCount * { u8 a, b, c }
// Coordinates are in the sword's frame: z along the shaft (prongs at +z), the grip at z = -500.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
}

#include "GanondorfTrident.h"

namespace {

struct Trident {
    std::vector<u8> texture;
    std::vector<Vtx> vertices;
    std::vector<Gfx> dl;
    u16 texWidth = 0;
    u16 texHeight = 0;
    f32 baseZ = 0.0f;
    f32 tipZ = 0.0f;
};

Trident sTridents[GDF_TRIDENT_COUNT];
bool sLoaded = false;
bool sTried = false;

template <typename F> void Emit(std::vector<Gfx>& dl, F build) {
    Gfx cmd[16];
    Gfx* p = cmd;

    build(p);
    for (Gfx* q = cmd; q < p; q++) {
        dl.push_back(*q);
    }
}

s32 Log2(u16 v) {
    s32 n = 0;
    while ((1 << n) < v) {
        n++;
    }
    return n;
}

struct PendingBatch {
    size_t firstVertex;
    u8 vertexCount;
    std::vector<u8> tris;
};

bool ReadFile() {
    const std::string candidates[] = {
        Ship::Context::GetPathRelativeToAppDirectory("gdf_tridents.bin"),
        Ship::Context::GetPathRelativeToAppDirectory("mods/gdf_tridents.bin"),
        "gdf_tridents.bin",
    };
    FILE* file = nullptr;
    for (const std::string& path : candidates) {
        file = fopen(path.c_str(), "rb");
        if (file != nullptr) {
            break;
        }
    }
    if (file == nullptr) {
        return false;
    }
    std::vector<u8> data;
    u8 buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), file)) > 0) {
        data.insert(data.end(), buf, buf + n);
    }
    fclose(file);

    size_t pos = 0;
    auto need = [&](size_t count) { return pos + count <= data.size(); };
    auto u8At = [&]() { return data[pos++]; };
    auto u16At = [&]() {
        u16 v = (u16)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return v;
    };

    if (!need(8) || memcmp(data.data(), "GDFT", 4) != 0) {
        return false;
    }
    pos = 4;
    if (u16At() != 2) {
        return false;
    }
    u16 count = u16At();

    for (u16 k = 0; (k < count) && (k < GDF_TRIDENT_COUNT); k++) {
        Trident& t = sTridents[k];
        std::vector<PendingBatch> batches;

        if (!need(32 + 8)) {
            return false;
        }
        pos += 32; // name
        t.baseZ = (s16)u16At();
        t.tipZ = (s16)u16At();
        t.texWidth = u16At();
        t.texHeight = u16At();
        size_t texBytes = (size_t)t.texWidth * t.texHeight * 2;
        if ((t.texWidth == 0) || (t.texHeight == 0) || (t.texWidth > 256) || (t.texHeight > 256) || !need(texBytes + 2)) {
            return false;
        }
        t.texture.assign(data.begin() + pos, data.begin() + pos + texBytes);
        pos += texBytes;
        u16 batchCount = u16At();

        for (u16 b = 0; b < batchCount; b++) {
            if (!need(2)) {
                return false;
            }
            PendingBatch batch;
            batch.vertexCount = u8At();
            u8 triCount = u8At();
            if ((batch.vertexCount == 0) || (batch.vertexCount > 32) || !need(batch.vertexCount * 14 + triCount * 3)) {
                return false;
            }
            batch.firstVertex = t.vertices.size();
            for (u8 i = 0; i < batch.vertexCount; i++) {
                Vtx v = {};
                v.n.ob[0] = (s16)u16At();
                v.n.ob[1] = (s16)u16At();
                v.n.ob[2] = (s16)u16At();
                v.n.tc[0] = (s16)u16At();
                v.n.tc[1] = (s16)u16At();
                f32 nx = (s8)u8At() / 127.0f;
                f32 ny = (s8)u8At() / 127.0f;
                f32 nz = (s8)u8At() / 127.0f;
                u8 alpha = u8At();
                // Its own light, baked in: the scene's lights (dim and coloured at dusk and indoors) tinted the
                // pale silver and lavender of some tridents sand-brown. A key light from above and one side.
                f32 len = sqrtf(nx * nx + ny * ny + nz * nz);
                f32 lit = 0.0f;
                if (len > 0.0f) {
                    lit = (nx * 0.35f + ny * 0.85f + nz * 0.4f) / len;
                }
                u8 shade = (u8)(255.0f * CLAMP(0.62f + 0.38f * fabsf(lit), 0.0f, 1.0f));
                v.v.cn[0] = v.v.cn[1] = v.v.cn[2] = shade;
                v.v.cn[3] = alpha;
                t.vertices.push_back(v);
            }
            for (u8 i = 0; i < triCount * 3; i++) {
                u8 index = u8At();
                if (index >= batch.vertexCount) {
                    return false;
                }
                batch.tris.push_back(index);
            }
            batches.push_back(std::move(batch));
        }

        // The buffers must not move once the display list points into them
        t.vertices.shrink_to_fit();
        t.texture.shrink_to_fit();

        u8* tex = t.texture.data();
        u16 w = t.texWidth;
        u16 h = t.texHeight;
        s32 maskS = Log2(w);
        s32 maskT = Log2(h);

        Emit(t.dl, [&](Gfx*& p) { gDPPipeSync(p++); });
        // One cycle, no palette: he's drawn in two-cycle mode, where the second cycle's TEXEL0 is really the second
        // tile, which still held whatever texture the scene loaded last (the moss and grass the tridents showed)
        Emit(t.dl, [&](Gfx*& p) { gDPSetCycleType(p++, G_CYC_1CYCLE); });
        Emit(t.dl, [&](Gfx*& p) { gDPSetTextureLUT(p++, G_TT_NONE); });
        Emit(t.dl, [&](Gfx*& p) { gDPSetTextureLOD(p++, G_TL_TILE); });
        // Textured and shaded (its own baked light); texels with little alpha are cut out (the feathers)
        Emit(t.dl, [&](Gfx*& p) {
            gDPSetCombineLERP(p++, TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED);
        });
        Emit(t.dl, [&](Gfx*& p) { gDPSetPrimColor(p++, 0, 0, 255, 255, 255, 255); });
        Emit(t.dl, [&](Gfx*& p) { gDPSetRenderMode(p++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2); });
        Emit(t.dl, [&](Gfx*& p) { gSPTexture(p++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON); });
        Emit(t.dl, [&](Gfx*& p) {
            gSPClearGeometryMode(p++, G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH | G_FOG);
        });
        Emit(t.dl, [&](Gfx*& p) { gSPClearGeometryMode(p++, G_LIGHTING); });
        Emit(t.dl, [&](Gfx*& p) { gSPSetGeometryMode(p++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH); });
        // A tile load: a block load stops at 4096 texels (the N64's texture memory), far less than the texture
        Emit(t.dl, [&](Gfx*& p) {
            gDPLoadTextureTile(p++, tex, G_IM_FMT_RGBA, G_IM_SIZ_16b, w, h, 0, 0, w - 1, h - 1, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, maskS, maskT, G_TX_NOLOD,
                               G_TX_NOLOD);
        });

        for (auto& batch : batches) {
            Emit(t.dl, [&](Gfx*& p) {
                gSPVertex(p++, (uintptr_t)&t.vertices[batch.firstVertex], batch.vertexCount, 0);
            });
            size_t triCount = batch.tris.size() / 3;
            size_t i = 0;
            for (; i + 1 < triCount; i += 2) {
                const u8* a = &batch.tris[i * 3];
                Emit(t.dl, [&](Gfx*& p) { gSP2Triangles(p++, a[0], a[1], a[2], 0, a[3], a[4], a[5], 0); });
            }
            if (i < triCount) {
                const u8* a = &batch.tris[i * 3];
                Emit(t.dl, [&](Gfx*& p) { gSP1Triangle(p++, a[0], a[1], a[2], 0); });
            }
        }
        Emit(t.dl, [&](Gfx*& p) { gDPPipeSync(p++); });
        Emit(t.dl, [&](Gfx*& p) { gSPTexture(p++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF); });
        // Back to two cycles, as the actor setups that come after expect
        Emit(t.dl, [&](Gfx*& p) { gDPSetCycleType(p++, G_CYC_2CYCLE); });
        Emit(t.dl, [&](Gfx*& p) { gSPEndDisplayList(p++); });
        t.dl.shrink_to_fit();
    }
    return count >= GDF_TRIDENT_COUNT;
}

} // namespace

extern "C" s32 GanondorfTrident_GetSelected(void) {
    s32 weapon = CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Weapon"), 0);

    if ((weapon < 1) || (weapon > GDF_TRIDENT_COUNT)) {
        return 0;
    }
    if (!sTried) {
        sTried = true;
        sLoaded = ReadFile();
    }
    return sLoaded ? weapon : 0;
}

extern "C" Gfx* GanondorfTrident_GetDL(s32 index, f32* baseZ, f32* tipZ) {
    if (!sTried) {
        sTried = true;
        sLoaded = ReadFile();
    }
    if (!sLoaded || (index < 0) || (index >= GDF_TRIDENT_COUNT)) {
        return nullptr;
    }
    *baseZ = sTridents[index].baseZ;
    *tipZ = sTridents[index].tipZ;
    return sTridents[index].dl.data();
}
