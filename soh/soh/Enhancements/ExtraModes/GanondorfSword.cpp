// Play as Ganondorf: the sword and sheath models for his sword stance (D-Pad Left).
//
// The models are not part of this repository. They're read at runtime from "gdf_sword.bin" next to the game (made
// from the player's own model file, see the format below). Without the file a plain stand-in blade is drawn and
// there's no sheath.
//
// It's looked for next to soh.exe, then in its mods folder. Copy it into every new build folder.
//
// gdf_sword.bin (little endian):
//   char[4] "GDFS", u16 version (1), u16 partCount
//   part:  u8 kind, u8 r, u8 g, u8 b, u16 batchCount
//   batch: u8 vtxCount (<= 32), u8 triCount,
//          vtxCount * { s16 x, y, z; s8 nx, ny, nz; u8 pad }, triCount * { u8 a, b, c }
//   kind: 0 blade, 1 hilt, 2 sheath body, 3 sheath trim, 4 sheath gem
// Coordinates are in the sword's own frame: z along the blade (point at +z), x across the cutting edges, y through
// the thickness. The grip is at z = -500, the guard at z = -125.

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ship/Context.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
}

#include "GanondorfSword.h"

namespace {

struct Model {
    std::vector<Vtx> vertices;
    std::vector<Gfx> sword;
    std::vector<Gfx> sheath;
    bool loaded = false;
    bool tried = false;
};

Model sModel;

void BeginMaterial(std::vector<Gfx>& dl) {
    Gfx g[1];
    Gfx* p;

    p = g;
    gDPPipeSync(p++);
    dl.push_back(g[0]);
    p = g;
    gDPSetCombineLERP(p++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE);
    dl.push_back(g[0]);
    p = g;
    gDPSetRenderMode(p++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    dl.push_back(g[0]);
    p = g;
    gSPTexture(p++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    dl.push_back(g[0]);
    p = g;
    gSPClearGeometryMode(p++, G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH | G_FOG);
    dl.push_back(g[0]);
    p = g;
    gSPSetGeometryMode(p++, G_ZBUFFER | G_SHADE | G_LIGHTING | G_SHADING_SMOOTH);
    dl.push_back(g[0]);
}

void PrimColor(std::vector<Gfx>& dl, u8 r, u8 g, u8 b) {
    Gfx cmd[1];
    Gfx* p = cmd;

    gDPSetPrimColor(p++, 0, 0, r, g, b, 255);
    dl.push_back(cmd[0]);
}

void EndList(std::vector<Gfx>& dl) {
    Gfx cmd[1];
    Gfx* p = cmd;

    gDPPipeSync(p++);
    dl.push_back(cmd[0]);
    p = cmd;
    gSPEndDisplayList(p++);
    dl.push_back(cmd[0]);
}

struct PendingBatch {
    size_t firstVertex;
    u8 vertexCount;
    std::vector<u8> tris;
};

struct PendingPart {
    u8 kind;
    u8 rgb[3];
    std::vector<PendingBatch> batches;
};

// A plain long blade, if the model file is missing
void BuildStandIn(Model& m, std::vector<PendingPart>& parts) {
    static const s16 kPoints[][3] = {
        { -200, 0, -125 }, { 200, 0, -125 }, { 0, 30, -125 },  { 0, -30, -125 }, // blade base
        { -180, 0, 4300 }, { 180, 0, 4300 }, { 0, 25, 4300 },  { 0, -25, 4300 }, // near the point
        { 0, 0, 4530 },                                                          // point
        { -60, -60, -1640 }, { 60, -60, -1640 }, { 60, 60, -1640 }, { -60, 60, -1640 }, // grip
        { -60, -60, -125 },  { 60, -60, -125 },  { 60, 60, -125 },  { -60, 60, -125 },
    };
    static const u8 kBlade[] = { 0, 4, 2, 2, 4, 6, 2, 6, 1, 1, 6, 5, 1, 5, 3, 3, 5, 7, 3, 7, 0, 0, 7, 4,
                                 4, 8, 6, 6, 8, 5, 5, 8, 7, 7, 8, 4 };
    static const u8 kGrip[] = { 0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7 };

    PendingPart blade = { 0, { 206, 209, 214 }, {} };
    PendingBatch b = { m.vertices.size(), 9, std::vector<u8>(kBlade, kBlade + sizeof(kBlade)) };
    for (int i = 0; i < 9; i++) {
        Vtx v = {};
        v.n.ob[0] = kPoints[i][0];
        v.n.ob[1] = kPoints[i][1];
        v.n.ob[2] = kPoints[i][2];
        v.n.n[0] = (s8)(kPoints[i][0] == 0 ? 0 : (kPoints[i][0] > 0 ? 120 : -120));
        v.n.n[1] = (s8)(kPoints[i][1] == 0 ? 0 : (kPoints[i][1] > 0 ? 120 : -120));
        v.n.n[2] = 30;
        v.n.a = 255;
        m.vertices.push_back(v);
    }
    blade.batches.push_back(b);
    parts.push_back(blade);

    PendingPart grip = { 1, { 226, 223, 206 }, {} };
    PendingBatch g = { m.vertices.size(), 8, std::vector<u8>(kGrip, kGrip + sizeof(kGrip)) };
    for (int i = 9; i < 17; i++) {
        Vtx v = {};
        v.n.ob[0] = kPoints[i][0];
        v.n.ob[1] = kPoints[i][1];
        v.n.ob[2] = kPoints[i][2];
        v.n.n[0] = (s8)(kPoints[i][0] > 0 ? 90 : -90);
        v.n.n[1] = (s8)(kPoints[i][1] > 0 ? 90 : -90);
        v.n.a = 255;
        m.vertices.push_back(v);
    }
    grip.batches.push_back(g);
    parts.push_back(grip);
}

bool ReadFile(Model& m, std::vector<PendingPart>& parts) {
    // Next to the game, in its mods folder, or in the working directory
    const std::string candidates[] = {
        Ship::Context::GetPathRelativeToAppDirectory("gdf_sword.bin"),
        Ship::Context::GetPathRelativeToAppDirectory("mods/gdf_sword.bin"),
        "gdf_sword.bin",
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

    if (!need(8) || memcmp(data.data(), "GDFS", 4) != 0) {
        return false;
    }
    pos = 4;
    if (u16At() != 1) {
        return false;
    }
    u16 partCount = u16At();

    for (u16 p = 0; p < partCount; p++) {
        if (!need(6)) {
            return false;
        }
        PendingPart part;
        part.kind = u8At();
        part.rgb[0] = u8At();
        part.rgb[1] = u8At();
        part.rgb[2] = u8At();
        u16 batchCount = u16At();

        for (u16 b = 0; b < batchCount; b++) {
            if (!need(2)) {
                return false;
            }
            PendingBatch batch;
            batch.vertexCount = u8At();
            u8 triCount = u8At();
            if ((batch.vertexCount == 0) || (batch.vertexCount > 32) || !need(batch.vertexCount * 10 + triCount * 3)) {
                return false;
            }
            batch.firstVertex = m.vertices.size();
            for (u8 i = 0; i < batch.vertexCount; i++) {
                Vtx v = {};
                v.n.ob[0] = (s16)u16At();
                v.n.ob[1] = (s16)u16At();
                v.n.ob[2] = (s16)u16At();
                v.n.n[0] = (s8)u8At();
                v.n.n[1] = (s8)u8At();
                v.n.n[2] = (s8)u8At();
                u8At();
                v.n.a = 255;
                m.vertices.push_back(v);
            }
            for (u8 i = 0; i < triCount * 3; i++) {
                u8 index = u8At();
                if (index >= batch.vertexCount) {
                    return false;
                }
                batch.tris.push_back(index);
            }
            part.batches.push_back(std::move(batch));
        }
        parts.push_back(std::move(part));
    }
    return true;
}

void Load() {
    Model& m = sModel;
    std::vector<PendingPart> parts;

    m.tried = true;
    if (!ReadFile(m, parts)) {
        m.vertices.clear();
        parts.clear();
        BuildStandIn(m, parts);
    }

    // The vertex buffer must not move once the display lists point into it
    m.vertices.shrink_to_fit();
    BeginMaterial(m.sword);
    BeginMaterial(m.sheath);

    for (auto& part : parts) {
        std::vector<Gfx>& dl = (part.kind <= 1) ? m.sword : m.sheath;
        PrimColor(dl, part.rgb[0], part.rgb[1], part.rgb[2]);

        for (auto& batch : part.batches) {
            Gfx cmd[1];
            Gfx* p = cmd;

            gSPVertex(p++, (uintptr_t)&m.vertices[batch.firstVertex], batch.vertexCount, 0);
            dl.push_back(cmd[0]);

            size_t triCount = batch.tris.size() / 3;
            size_t t = 0;
            for (; t + 1 < triCount; t += 2) {
                const u8* a = &batch.tris[t * 3];
                p = cmd;
                gSP2Triangles(p++, a[0], a[1], a[2], 0, a[3], a[4], a[5], 0);
                dl.push_back(cmd[0]);
            }
            if (t < triCount) {
                const u8* a = &batch.tris[t * 3];
                p = cmd;
                gSP1Triangle(p++, a[0], a[1], a[2], 0);
                dl.push_back(cmd[0]);
            }
        }
    }

    EndList(m.sword);
    EndList(m.sheath);
    m.loaded = true;
}

} // namespace

extern "C" Gfx* GanondorfSword_GetSwordDL(void) {
    if (!sModel.tried) {
        Load();
    }
    return sModel.loaded ? sModel.sword.data() : nullptr;
}

// Only with the model file (the stand-in has no sheath)
extern "C" Gfx* GanondorfSword_GetSheathDL(void) {
    if (!sModel.tried) {
        Load();
    }
    return (sModel.loaded && (sModel.sheath.size() > 8)) ? sModel.sheath.data() : nullptr;
}
