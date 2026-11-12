// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// Play as Ganondorf: terraforming, step 1: placed pieces. From the Dark Summoning menu's "Build" page he sets down
// stairs, ramps, platforms, walls, pillars and bridges: each its own actor with real (dynamic) collision and a stone
// look made here in code. What he builds is kept per area in the save file and stands again whenever he returns.
// Step 2, water: a "Water pool" piece is a block of real water (a water box added to the area's list: he wades, swims
// and dives in it), drawn see-through blue. Step 3, sculpting: a brush raises and lowers the area's own ground: a field
// of height offsets on a grid, applied to the area's collision points and to the vertices of its room meshes alike
// (from their untouched values, which are put back when the area is left; the game's files are never changed).
// Placing (the world still, the camera overhead on the piece, shown see-through until it's set down):
//   Stick: move   C-Left/C-Right: turn   C-Up/C-Down: raise/lower (with R held: taller/shorter)
//   D-pad up/down: longer/shorter   D-pad left/right: wider/narrower   Z: down onto the floor   A: set down   B: cancel

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Vertex.h>
#include <fast/resource/type/Texture.h>
#include <fast/lus_gbi.h>
#include "soh/resource/type/Array.h"

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include "soh/frame_interpolation.h" // gives the OPEN_DISPS macros C linkage for FrameInterpolation_*

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "GanondorfBuild.h"

namespace {

enum PieceType {
    PIECE_STAIRS,
    PIECE_RAMP,
    PIECE_PLATFORM,
    PIECE_WALL,
    PIECE_PILLAR,
    PIECE_BRIDGE,
    PIECE_WATER,
    PIECE_COUNT,
    PIECE_BRUSH = PIECE_COUNT // (not a piece: the sculpting brush's marker on the ground)
};

struct PieceKind {
    const char* name;
    bool wedge;       // rises along its length (stairs, ramp); else a box
    f32 w, h, l;      // its size as it first appears
};
const PieceKind sKinds[PIECE_COUNT + 1] = {
    { "Stairs", true, 120.0f, 100.0f, 160.0f },  { "Ramp", true, 120.0f, 80.0f, 220.0f },
    { "Platform", false, 200.0f, 20.0f, 200.0f }, { "Wall", false, 240.0f, 160.0f, 24.0f },
    { "Pillar", false, 50.0f, 240.0f, 50.0f },    { "Bridge", false, 100.0f, 12.0f, 400.0f },
    { "Water pool", false, 300.0f, 60.0f, 300.0f }, { "Brush", false, 200.0f, 3.0f, 200.0f },
};

#define MAX_PIECES_PER_SCENE 16 // (the game's dynamic collision has room for only so many polygons an area)
#define UNIT 1000.0f // the shapes are 1000 units a side in model space, scaled to size by the actor

struct PieceData {
    u32 uid;
    u8 type;
    u8 tex = 0; // its texture (sTexList)
    Vec3f pos;
    s16 rotY;
    f32 w, h, l;
};

struct GdfPiece {
    DynaPolyActor dyna;
    u32 uid;
    u8 type;
    u8 ghost; // being placed: see-through, no collision yet
    u8 tex;
    f32 w, h, l;
};

std::map<s16, std::vector<PieceData>> sPieces; // by scene
u32 sNextUid = 1;
s32 sActorId = -1;
PieceData sSpawnSpec;
bool sSpawnGhost = false;
GdfPiece* sPlacing = nullptr;
GdfPiece* sBrush = nullptr; // the sculpting brush's marker (sculpting while it exists)
GdfPiece* sTarget = nullptr; // where a summoning will appear (choosing while it exists)
f32 sBrushRadius = 100.0f;
int sHeld[16] = {};

// ---- The two shapes: a box and a wedge (collision and display list from the same triangles) ----

struct Shape {
    Vec3s vtx[8];
    CollisionPoly polys[12];
    SurfaceType surface;
    CollisionHeader col;
    Vtx drawVtx[36];
    Gfx dl[24];
    int numTris;
    CamData cam; // (a floor's camera setting: the area's normal one)
};
Shape sBox, sWedge;
bool sShapesBuilt = false;

void BuildShape(Shape& s, const Vec3s* verts, int numVerts, const int (*tris)[3], int numTris, Vec3f inside) {
    for (int i = 0; i < numVerts; i++) {
        s.vtx[i] = verts[i];
    }
    Gfx* g = s.dl;
    for (int t = 0; t < numTris; t++) {
        int a = tris[t][0], b = tris[t][1], c = tris[t][2];
        Vec3f va = { (f32)verts[a].x, (f32)verts[a].y, (f32)verts[a].z };
        Vec3f ab = { verts[b].x - va.x, verts[b].y - va.y, verts[b].z - va.z };
        Vec3f ac = { verts[c].x - va.x, verts[c].y - va.y, verts[c].z - va.z };
        Vec3f n = { ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x };
        // Facing outward (away from a point inside the shape)
        if (n.x * (va.x - inside.x) + n.y * (va.y - inside.y) + n.z * (va.z - inside.z) < 0.0f) {
            std::swap(b, c);
            n.x = -n.x, n.y = -n.y, n.z = -n.z;
        }
        f32 len = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));
        n.x /= len, n.y /= len, n.z /= len;

        CollisionPoly* p = &s.polys[t];
        p->type = 0;
        p->flags_vIA = (u16)a;
        p->flags_vIB = (u16)b;
        p->vIC = (u16)c;
        p->normal.x = (s16)(n.x * 32767.0f);
        p->normal.y = (s16)(n.y * 32767.0f);
        p->normal.z = (s16)(n.z * 32767.0f);
        p->dist = (s16)-(n.x * va.x + n.y * va.y + n.z * va.z);

        const int idx[3] = { a, b, c };
        for (int k = 0; k < 3; k++) {
            Vtx* v = &s.drawVtx[t * 3 + k];
            v->n.ob[0] = verts[idx[k]].x;
            v->n.ob[1] = verts[idx[k]].y;
            v->n.ob[2] = verts[idx[k]].z;
            v->n.flag = 0;
            v->n.tc[0] = v->n.tc[1] = 0;
            v->n.n[0] = (s8)(n.x * 127.0f);
            v->n.n[1] = (s8)(n.y * 127.0f);
            v->n.n[2] = (s8)(n.z * 127.0f);
            v->n.a = 255;
        }
    }
    // (up to 30 vertices a load: 10 triangles)
    for (int t = 0; t < numTris; t += 10) {
        int count = std::min(numTris - t, 10);
        gSPVertex(g++, (uintptr_t)&s.drawVtx[t * 3], count * 3, 0);
        for (int k = 0; k < count; k++) {
            gSP1Triangle(g++, k * 3, k * 3 + 1, k * 3 + 2, 0);
        }
    }
    gSPEndDisplayList(g++);

    s.surface = {};
    s.col = {};
    s.col.minBounds = { -500, 0, -500 };
    s.col.maxBounds = { 500, 1000, 500 };
    s.col.numVertices = (u16)numVerts;
    s.col.vtxList = s.vtx;
    s.col.numPolygons = (u16)numTris;
    s.col.polyList = s.polys;
    s.col.surfaceTypeList = &s.surface;
    // (the game reads a floor's camera setting from this list when he stands on it: none crashed the game)
    s.cam.cameraSType = CAM_SET_NORMAL0;
    s.cam.numCameras = 0;
    s.cam.camPosData = nullptr;
    s.col.cameraDataList = &s.cam;
    s.numTris = numTris;
    s.col.numWaterBoxes = 0;
    s.col.waterBoxes = nullptr;
    s.col.cameraDataListLen = 0;
}

void BuildShapes() {
    if (sShapesBuilt) {
        return;
    }
    static const Vec3s boxV[8] = { { -500, 0, -500 },    { 500, 0, -500 },    { 500, 0, 500 },    { -500, 0, 500 },
                                   { -500, 1000, -500 }, { 500, 1000, -500 }, { 500, 1000, 500 }, { -500, 1000, 500 } };
    static const int boxT[12][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 },
                                     { 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 } };
    // The wedge rises from nothing at its near end (-z) to full height at its far end (+z)
    static const Vec3s wedgeV[6] = { { -500, 0, -500 }, { 500, 0, -500 },   { 500, 0, 500 },
                                     { -500, 0, 500 },  { -500, 1000, 500 }, { 500, 1000, 500 } };
    static const int wedgeT[8][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 1, 5 }, { 0, 5, 4 },
                                      { 3, 2, 5 }, { 3, 5, 4 }, { 0, 3, 4 }, { 1, 2, 5 } };

    BuildShape(sBox, boxV, 8, boxT, 12, { 0.0f, 500.0f, 0.0f });
    BuildShape(sWedge, wedgeV, 6, wedgeT, 8, { 0.0f, 250.0f, 250.0f });
    sShapesBuilt = true;
}

// ---- The piece actor ----

void Piece_ApplySize(GdfPiece* self) {
    self->dyna.actor.scale.x = self->w / UNIT;
    self->dyna.actor.scale.y = self->h / UNIT;
    self->dyna.actor.scale.z = self->l / UNIT;
}

void Piece_Solidify(GdfPiece* self, PlayState* play) {
    self->ghost = false;
    if (self->type == PIECE_WATER) {
        return; // (water has no walls: it's in the area's water list, see RebuildWater)
    }
    self->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &self->dyna.actor,
                                          sKinds[self->type].wedge ? &sWedge.col : &sBox.col);
}

void Piece_Init(Actor* thisx, PlayState* play) {
    GdfPiece* self = (GdfPiece*)thisx;

    BuildShapes();
    self->uid = sSpawnSpec.uid;
    self->type = sSpawnSpec.type;
    self->tex = sSpawnSpec.tex;
    self->w = sSpawnSpec.w;
    self->h = sSpawnSpec.h;
    self->l = sSpawnSpec.l;
    self->ghost = sSpawnGhost;
    thisx->room = -1; // it stands whichever room he's in
    Piece_ApplySize(self);
    DynaPolyActor_Init(&self->dyna, 0);
    self->dyna.bgId = BGACTOR_NEG_ONE;
    if (!self->ghost) {
        Piece_Solidify(self, play);
    }
}

void Piece_Destroy(Actor* thisx, PlayState* play) {
    GdfPiece* self = (GdfPiece*)thisx;

    if (!self->ghost && (self->dyna.bgId != BGACTOR_NEG_ONE)) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, self->dyna.bgId);
    }
    if (sPlacing == self) {
        sPlacing = nullptr;
    }
    if (sBrush == self) {
        sBrush = nullptr;
    }
    if (sTarget == self) {
        sTarget = nullptr;
    }
}

void Piece_Update(Actor* thisx, PlayState* play) {
}

} // namespace

// (outside the unnamed namespace: the draw macros declare the game's frame interpolation functions in place, and inside it
// they would name new functions of their own, which don't exist)
// The game's own textures: stone from its push blocks, water from Lake Hylia (read from the player's game data)
#define STONE_TEX "__OTR__objects/gameplay_dangeon_keep/gPushBlockGrayTex"
// The textures a piece can have (cycled with L while placing). Any the game keeps in indexed colour show as plain
// stone colour instead.
struct TexChoice {
    const char* name;
    const char* path;
};
static const TexChoice sTexList[] = {
    { "Dungeon stone", STONE_TEX },
    { "Forest brick", "__OTR__objects/object_mori_tex/gForestTwistedHallBrickTex" },
    { "Forest stone wall", "__OTR__objects/object_mori_tex/gMoriStoneWallTex" },
    { "Kakariko brick", "__OTR__objects/object_spot01_matoyab/gKakarikoConstructionSiteBrickTex" },
    { "Brick with grass", "__OTR__objects/object_spot01_matoyab/gKakarikoConstructionSiteBrickWithGrassTex" },
    { "Kakariko wood", "__OTR__objects/object_spot01_matoyab/gKakarikoConstructionSiteWoodTex" },
    { "Drawbridge planks", "__OTR__objects/object_spot00_objects/gHyruleFieldCastleDrawbridgeWoodTex" },
    { "Well stone", "__OTR__objects/object_spot01_matoya/gKakarikoBOTWStoneTex" },
    { "Stone wall", "__OTR__objects/object_bowl/gBowlingStoneWallTex" },
    { "Wood panels", "__OTR__objects/object_bowl/gBowlingWoodPanelTex" },
    { "Crater rock", "__OTR__objects/object_spot17_obj/gCraterRockTex" },
    { "Cavern rock", "__OTR__objects/object_spot16_obj/gDodongosCavernRockTex" },
    { "Silver rock", "__OTR__objects/gameplay_field_keep/gFieldSilverRockTex" },
    { "Colossus wall", "__OTR__objects/object_spot11_obj/gDesertColossusBombableWallTex" },
    { "Ice", "__OTR__objects/object_spot06_objects/gLakeHyliaZoraShortcutIceblockTex" },
    { "Red ice", "__OTR__objects/object_ice_objects/gRedIcePlatformTex" },
    { "Metal", "__OTR__objects/object_mori_tex/gMoriKaitenkabeMetalWallTex" },
    { "Grass", "__OTR__objects/object_haka/gGravestoneGrassTex" },
};
#define TEX_COUNT ((int)ARRAY_COUNT(sTexList))
#define WATER_TEX "__OTR__objects/object_spot06_objects/gLakeHyliaWaterTex"
#define TEX_REPEAT 60.0f // world units per repeat of the texture

struct TexInfo {
    bool ok;
    int type;
    u16 w, h;
};

static TexInfo LoadTexInfo(const char* path) {
    TexInfo t = { false, 0, 0, 0 };
    auto res = std::dynamic_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path + 7));

    if (res != nullptr) {
        t.type = (int)res->Type;
        t.w = res->Width;
        t.h = res->Height;
        t.ok = (t.w > 0) && (t.h > 0);
    }
    return t;
}

static int Log2(int v) {
    int n = 0;
    while ((1 << (n + 1)) <= v) {
        n++;
    }
    return n;
}

// Loads the texture (in its own format) into tile 0, repeating; false if it can't be used
static bool SetupTex(Gfx*& disp, const char* path, const TexInfo& t) {
    if (!t.ok) {
        return false;
    }
    int ms = Log2(t.w), mt = Log2(t.h);
    switch ((Fast::TextureType)t.type) {
        case Fast::TextureType::RGBA16bpp:
            gDPLoadTextureBlock(disp++, path, G_IM_FMT_RGBA, G_IM_SIZ_16b, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt,
                                G_TX_NOLOD, G_TX_NOLOD);
            return true;
        case Fast::TextureType::RGBA32bpp:
            gDPLoadTextureBlock(disp++, path, G_IM_FMT_RGBA, G_IM_SIZ_32b, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt,
                                G_TX_NOLOD, G_TX_NOLOD);
            return true;
        case Fast::TextureType::Grayscale8bpp:
            gDPLoadTextureBlock(disp++, path, G_IM_FMT_I, G_IM_SIZ_8b, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt,
                                G_TX_NOLOD, G_TX_NOLOD);
            return true;
        case Fast::TextureType::Grayscale4bpp:
            gDPLoadTextureBlock_4b(disp++, path, G_IM_FMT_I, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt, G_TX_NOLOD,
                                   G_TX_NOLOD);
            return true;
        case Fast::TextureType::GrayscaleAlpha8bpp:
            gDPLoadTextureBlock(disp++, path, G_IM_FMT_IA, G_IM_SIZ_8b, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt,
                                G_TX_NOLOD, G_TX_NOLOD);
            return true;
        case Fast::TextureType::GrayscaleAlpha16bpp:
            gDPLoadTextureBlock(disp++, path, G_IM_FMT_IA, G_IM_SIZ_16b, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt,
                                G_TX_NOLOD, G_TX_NOLOD);
            return true;
        case Fast::TextureType::GrayscaleAlpha4bpp:
            gDPLoadTextureBlock_4b(disp++, path, G_IM_FMT_IA, t.w, t.h, 0, G_TX_WRAP, G_TX_WRAP, ms, mt, G_TX_NOLOD,
                                   G_TX_NOLOD);
            return true;
        default:
            return false; // (paletted: not used here)
    }
}

// One shape at its real size (world units, the actor's scale undone), its texture laid on at a steady size per face
static void EmitShape(Gfx*& disp, PlayState* play, const Shape& shape, f32 w, f32 h, f32 l, f32 offZ, f32 uvOff,
                      const TexInfo& tex) {
    int n = shape.numTris * 3;
    Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * n);
    f32 su = (tex.ok ? tex.w : 32) * 32.0f / TEX_REPEAT, sv = (tex.ok ? tex.h : 32) * 32.0f / TEX_REPEAT;

    for (int i = 0; i < n; i++) {
        const Vtx& src = shape.drawVtx[i];
        f32 x = src.n.ob[0] * w / UNIT, y = src.n.ob[1] * h / UNIT, z = src.n.ob[2] * l / UNIT + offZ;
        s8 nx = src.n.n[0], ny = src.n.n[1], nz = src.n.n[2];
        f32 u, t;

        v[i] = src;
        v[i].n.ob[0] = (s16)x;
        v[i].n.ob[1] = (s16)y;
        v[i].n.ob[2] = (s16)z;
        if ((ABS(ny) >= ABS(nx)) && (ABS(ny) >= ABS(nz))) {
            u = x, t = z;
        } else if (ABS(nx) >= ABS(nz)) {
            u = z, t = -y;
        } else {
            u = x, t = -y;
        }
        v[i].n.tc[0] = (s16)CLAMP(fmodf(u + uvOff, TEX_REPEAT * 16.0f) * su, -32000.0f, 32000.0f);
        v[i].n.tc[1] = (s16)CLAMP(fmodf(t, TEX_REPEAT * 16.0f) * sv, -32000.0f, 32000.0f);
    }
    for (int t = 0; t < shape.numTris; t += 10) {
        int count = std::min(shape.numTris - t, 10);

        gSPVertex(disp++, (uintptr_t)&v[t * 3], count * 3, 0);
        for (int k = 0; k < count; k++) {
            gSP1Triangle(disp++, k * 3, k * 3 + 1, k * 3 + 2, 0);
        }
    }
}

// The brush and the summoning marker: a glowing circle like a blue warp's, lying on the ground's own shape (each point
// set down on the floor beneath it), pulsing; the summoning marker's has a pointer for which way it will face
static void DrawGroundRing(PlayState* play, Vec3f center, f32 radius, s16 facing, bool pointer) {
    static const f32 sRad[] = { 0.0f, 0.45f, 0.75f, 0.9f, 1.0f, 1.08f };
    static const u8 sAlpha[] = { 25, 45, 90, 210, 230, 0 };
    const int segs = 28, bands = ARRAY_COUNT(sRad) - 1;
    f32 pulse = 0.8f + 0.2f * sinf(play->gameplayFrames * 0.15f);
    Vec3f pts[ARRAY_COUNT(sRad)][28 + 1];

    for (int r = 0; r <= bands; r++) {
        for (int k = 0; k <= segs; k++) {
            f32 a = (2.0f * (f32)M_PI * k) / segs;
            Vec3f p = { center.x + sinf(a) * radius * sRad[r], center.y + 120.0f, center.z + cosf(a) * radius * sRad[r] };
            CollisionPoly* poly;
            s32 bgId;
            f32 y = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &p);

            p.y = ((y > BGCHECK_Y_MIN) && (fabsf(y - center.y) < 250.0f)) ? y + 2.5f : center.y + 2.5f;
            pts[r][k] = p;
        }
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK | G_FOG);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    int numTris = bands * segs * 2 + (pointer ? 1 : 0);
    Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * numTris * 3);
    int n = 0;
    auto put = [&](const Vec3f& p, int ring) {
        Vtx* o = &v[n++];
        f32 t = sRad[ring];
        u8 alpha = (u8)(sAlpha[ring] * pulse);

        o->v.ob[0] = (s16)p.x;
        o->v.ob[1] = (s16)p.y;
        o->v.ob[2] = (s16)p.z;
        o->v.flag = 0;
        o->v.tc[0] = o->v.tc[1] = 0;
        // White-blue in the middle to a bright warp blue at the rim
        o->v.cn[0] = (u8)(200 - 120 * t);
        o->v.cn[1] = (u8)(235 - 45 * t);
        o->v.cn[2] = 255;
        o->v.cn[3] = alpha;
    };
    for (int r = 0; r < bands; r++) {
        for (int k = 0; k < segs; k++) {
            put(pts[r][k], r), put(pts[r + 1][k], r + 1), put(pts[r + 1][k + 1], r + 1);
            put(pts[r][k], r), put(pts[r + 1][k + 1], r + 1), put(pts[r][k + 1], r);
        }
    }
    if (pointer) {
        // A bright point at the rim, the way the summoned thing will face
        f32 a = (f32)BINANG_TO_RAD(facing);
        Vec3f tip = { center.x + sinf(a) * radius * 1.5f, center.y + 3.0f, center.z + cosf(a) * radius * 1.5f };
        Vec3f l = { center.x + sinf(a + 0.35f) * radius, center.y + 3.0f, center.z + cosf(a + 0.35f) * radius };
        Vec3f rr = { center.x + sinf(a - 0.35f) * radius, center.y + 3.0f, center.z + cosf(a - 0.35f) * radius };

        put(tip, 4), put(l, 4), put(rr, 4);
    }
    for (int t = 0; t < numTris; t += 10) {
        int count = std::min(numTris - t, 10);

        gSPVertex(POLY_XLU_DISP++, (uintptr_t)&v[t * 3], count * 3, 0);
        for (int k = 0; k < count; k++) {
            gSP1Triangle(POLY_XLU_DISP++, k * 3, k * 3 + 1, k * 3 + 2, 0);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Piece_Draw(Actor* thisx, PlayState* play) {
    GdfPiece* self = (GdfPiece*)thisx;

    if (self->type == PIECE_BRUSH) {
        bool target = (self->l != self->w); // (the summoning marker: longer than wide)

        DrawGroundRing(play, thisx->world.pos, target ? 45.0f : self->w * 0.5f, thisx->shape.rot.y, target);
        return;
    }
    bool water = (self->type == PIECE_WATER);
    bool ghost = self->ghost || water; // (water is drawn see-through too)
    static TexInfo sTexInfo[TEX_COUNT];
    static bool sTexLoaded[TEX_COUNT] = {};
    static TexInfo sWater = LoadTexInfo(WATER_TEX);
    int texIndex = (self->tex < TEX_COUNT) ? self->tex : 0;

    if (!sTexLoaded[texIndex]) {
        sTexInfo[texIndex] = LoadTexInfo(sTexList[texIndex].path);
        sTexLoaded[texIndex] = true;
    }
    const TexInfo& tex = water ? sWater : sTexInfo[texIndex];
    bool textured = (self->type != PIECE_BRUSH);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx*& disp = ghost ? POLY_XLU_DISP : POLY_OPA_DISP;

    if (ghost) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
    }
    textured = textured && SetupTex(disp, water ? WATER_TEX : sTexList[texIndex].path, tex);
    if (textured) {
        // The game's texture, lit by the scene's light on each face, tinted by the primitive colour
        gDPSetCombineLERP(disp++, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0,
                          COMBINED);
    } else {
        // (plain lit colour)
        gDPSetCombineLERP(disp++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
    }
    if (water) {
        gDPSetPrimColor(disp++, 0, 0, 160, 210, 255, self->ghost ? 110 : 170);
    } else if (ghost) {
        gDPSetPrimColor(disp++, 0, 0, 255, 225, 150, 140);
    } else if (textured) {
        gDPSetPrimColor(disp++, 0, 0, 255, 255, 255, 255);
    } else {
        gDPSetPrimColor(disp++, 0, 0, 150, 142, 132, 255); // (plain stone, where a texture can't be used)
    }
    // At its real size: the actor's scale undone, the vertices made to size (so the texture keeps its size)
    Matrix_Scale(1.0f / MAX(thisx->scale.x, 0.0001f), 1.0f / MAX(thisx->scale.y, 0.0001f),
                 1.0f / MAX(thisx->scale.z, 0.0001f), MTXMODE_APPLY);
    gSPMatrix(disp++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    // (water scrolls slowly)
    f32 uvOff = water ? fmodf(play->gameplayFrames * 0.4f, TEX_REPEAT) : 0.0f;
    if ((self->type == PIECE_STAIRS) || (self->type == PIECE_RAMP)) {
        // Steps (about 12 units tall each on stairs, fine ones on a ramp) rising along its length; walked as a slope
        f32 stepH = (self->type == PIECE_STAIRS) ? 12.0f : 4.0f;
        int n = CLAMP((int)(self->h / stepH + 0.5f), 2, 60);

        for (int i = 0; i < n; i++) {
            EmitShape(disp, play, sBox, self->w, self->h * (i + 1.0f) / n, self->l / n,
                      -self->l * 0.5f + (i + 0.5f) * self->l / n, uvOff, tex);
        }
    } else {
        EmitShape(disp, play, sBox, self->w, self->h, self->l, 0.0f, uvOff, tex);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

namespace {

void RegisterActor() {
    if (sActorId >= 0) {
        return;
    }
    ActorDBInit init;
    init.name = "Gdf_BuildPiece";
    init.desc = "Built piece: stairs, ramp, platform, wall (Play as Ganondorf)";
    init.category = ACTORCAT_BG;
    init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    init.objectId = OBJECT_GAMEPLAY_KEEP;
    init.instanceSize = sizeof(GdfPiece);
    init.init = Piece_Init;
    init.destroy = Piece_Destroy;
    init.update = Piece_Update;
    init.draw = Piece_Draw;
    sActorId = ActorDB::Instance->AddEntry(init).entry.id;
}

GdfPiece* SpawnPiece(PlayState* play, const PieceData& d, bool ghost) {
    RegisterActor();
    sSpawnSpec = d;
    sSpawnGhost = ghost;
    return (GdfPiece*)Actor_Spawn(&play->actorCtx, play, sActorId, d.pos.x, d.pos.y, d.pos.z, 0, d.rotY, 0, 0);
}

void RemoveActor(PlayState* play, u32 uid) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != nullptr; a = a->next) {
        if ((a->id == sActorId) && (((GdfPiece*)a)->uid == uid)) {
            a->draw = nullptr; // (gone at once, even with the world held still)
            Actor_Kill(a);
        }
    }
}

// A button pressed, or held long enough to repeat
bool Rep(Input* input, u16 btn, int slot) {
    if (!CHECK_BTN_ALL(input->cur.button, btn)) {
        sHeld[slot] = 0;
        return false;
    }
    sHeld[slot]++;
    return (sHeld[slot] == 1) || ((sHeld[slot] > 10) && ((sHeld[slot] % 2) == 0));
}

void Sfx(u16 id) {
    Audio_PlaySfxGeneral(id, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}


// ---- Water: the area's own water boxes plus one for each pool he set down ----

CollisionHeader* sWaterHeader = nullptr;
WaterBox* sOrigWater = nullptr;
u16 sOrigWaterCount = 0;
std::vector<WaterBox> sWaterBuf;

void RestoreWater() {
    if (sWaterHeader != nullptr) {
        sWaterHeader->waterBoxes = sOrigWater;
        sWaterHeader->numWaterBoxes = sOrigWaterCount;
        sWaterHeader = nullptr;
    }
}

void RebuildWater(CollisionHeader* header, s16 scene) {
    if (header == nullptr) {
        return;
    }
    if (sWaterHeader != header) {
        RestoreWater();
        sWaterHeader = header;
        sOrigWater = header->waterBoxes;
        sOrigWaterCount = header->numWaterBoxes;
    }
    sWaterBuf.assign(sOrigWater, sOrigWater + ((sOrigWater != nullptr) ? sOrigWaterCount : 0));
    auto it = sPieces.find(scene);
    if (it != sPieces.end()) {
        for (auto& p : it->second) {
            if (p.type != PIECE_WATER) {
                continue;
            }
            WaterBox box = {};
            box.xMin = (s16)(p.pos.x - p.w * 0.5f);
            box.zMin = (s16)(p.pos.z - p.l * 0.5f);
            box.xLength = (s16)p.w;
            box.zLength = (s16)p.l;
            box.ySurface = (s16)(p.pos.y + p.h);
            box.properties = (0x3F << 13) | (0x1F << 8); // every room, the area's own light, its default camera
            sWaterBuf.push_back(box);
        }
    }
    if (sWaterBuf.size() == sOrigWaterCount) {
        header->waterBoxes = sOrigWater;
    } else {
        header->waterBoxes = sWaterBuf.data();
    }
    header->numWaterBoxes = (u16)sWaterBuf.size();
}

// ---- Sculpting: a field of height offsets over the area, on a grid ----

#define GRID 50.0f       // the grid's spacing (world units)
#define REACH 300.0f     // a point of the grid only moves ground within this height of where it was sculpted
#define MAX_OFFSET 240.0f // (higher, and the game's ground lookup can no longer place what stands on it)

struct Node {
    f32 dy;
    f32 refY;
};
typedef std::unordered_map<uint32_t, Node> Field;
std::map<s16, Field> sFields; // by scene

uint32_t NodeKey(int gx, int gz) {
    return ((uint32_t)(uint16_t)(int16_t)gx << 16) | (uint16_t)(int16_t)gz;
}

// How far the ground at (x, z), at height y before any sculpting, has been moved
f32 Offset(const Field& field, f32 x, f32 y, f32 z) {
    f32 fx = x / GRID, fz = z / GRID;
    int gx = (int)floorf(fx), gz = (int)floorf(fz);
    f32 tx = fx - gx, tz = fz - gz;
    f32 sum = 0.0f;

    for (int i = 0; i < 4; i++) {
        auto it = field.find(NodeKey(gx + (i & 1), gz + (i >> 1)));

        if ((it != field.end()) && (fabsf(y - it->second.refY) < REACH)) {
            sum += it->second.dy * ((i & 1) ? tx : 1.0f - tx) * ((i >> 1) ? tz : 1.0f - tz);
        }
    }
    return sum;
}

// The area's collision as it was, and its room meshes' vertices as they were
CollisionHeader* sColHeader = nullptr;
std::vector<Vec3s> sColVtx;
std::vector<CollisionPoly> sColPolys;
Vec3s sColMin, sColMax;
struct MeshVtx {
    std::shared_ptr<Ship::IResource> res; // (kept loaded while it's changed)
    Vtx* vtx;
    size_t count;
    std::vector<s16> origY;
};
std::unordered_map<std::string, MeshVtx> sMeshes;
std::vector<const void*> sWalkedRooms; // room meshes already read in this area
std::vector<std::string> sRoomFiles; // rooms loaded in this area, their meshes not yet looked up
s16 sFieldScene = -1;

void RestoreGround() {
    if (sColHeader != nullptr) {
        std::copy(sColVtx.begin(), sColVtx.end(), sColHeader->vtxList);
        std::copy(sColPolys.begin(), sColPolys.end(), sColHeader->polyList);
        sColHeader->minBounds = sColMin;
        sColHeader->maxBounds = sColMax;
        sColHeader = nullptr;
    }
    for (auto& [path, m] : sMeshes) {
        for (size_t i = 0; i < m.origY.size() && i < m.count; i++) {
            m.vtx[i].v.ob[1] = m.origY[i];
        }
    }
    sMeshes.clear();
    sRoomFiles.clear();
    sWalkedRooms.clear();
}

void TrackCollision(CollisionHeader* header) {
    if ((header == nullptr) || (sColHeader == header)) {
        return;
    }
    sColHeader = header;
    sColVtx.assign(header->vtxList, header->vtxList + header->numVertices);
    sColPolys.assign(header->polyList, header->polyList + header->numPolygons);
    sColMin = header->minBounds;
    sColMax = header->maxBounds;
}

// The meshes of the rooms loaded since last time: their vertex lists, by name beside the room's file
void TrackVertexResource(const char* name) {
    if ((name == nullptr) || sMeshes.count(name)) {
        return;
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(name);
    MeshVtx m;

    m.res = res;
    m.vtx = nullptr;
    m.count = 0;
    // (a vertex list is either its own kind of resource, or, as the game's room meshes are, an array of vertices)
    if (auto v = std::dynamic_pointer_cast<Fast::Vertex>(res)) {
        m.vtx = (Vtx*)v->VertexList.data();
        m.count = v->VertexList.size();
    } else if (auto arr = std::dynamic_pointer_cast<SOH::Array>(res)) {
        m.vtx = (Vtx*)arr->Vertices.data();
        m.count = arr->Vertices.size();
    }
    if ((m.vtx == nullptr) || (m.count == 0)) {
        return;
    }
    m.origY.reserve(m.count);
    for (size_t i = 0; i < m.count; i++) {
        m.origY.push_back(m.vtx[i].v.ob[1]);
    }
    sMeshes.emplace(name, std::move(m));
}

// Every vertex list a display list draws with (following the display lists it calls), as the game's graphics
// debugger reads them
void WalkDList(const Gfx* dl, int depth, int& budget) {
    if ((dl == nullptr) || (depth > 10)) {
        return;
    }
    const Gfx* cmd = dl;
    while (budget-- > 0) {
        u8 op = (u8)(cmd->words.w0 >> 24);
        bool branch = ((cmd->words.w0 >> 16) & 1) != 0;

        switch (op) {
            case 0xDF: // G_ENDDL
                return;
            case 0xDE: // G_DL (a segment address: not followed)
                if (branch) {
                    return;
                }
                cmd++;
                break;
            case 0xE4: // G_TEXRECT, G_TEXRECTFLIP, G_IMAGERECT, G_SETTILESIZE_INTERP: 3 words
            case 0xE5:
            case G_IMAGERECT:
            case G_SETTILESIZE_INTERP:
                cmd += 3;
                break;
            case G_SETTILESIZE_LERP:
                cmd += 5;
                break;
            case G_MARKER:
            case G_SETTIMG_OTR_HASH:
            case G_MTX_OTR:
            case G_MOVEMEM_OTR:
            case G_BRANCH_Z_OTR:
                cmd += 2;
                break;
            case G_DL_OTR_HASH: {
                uint64_t hash = ((uint64_t)cmd[1].words.w0 << 32) + (uint32_t)cmd[1].words.w1;
                WalkDList((const Gfx*)ResourceGetDataByCrc(hash), depth + 1, budget);
                if (branch) {
                    return;
                }
                cmd += 2;
                break;
            }
            case G_DL_OTR_FILEPATH: {
                const char* name = (const char*)cmd->words.w1;
                if ((uintptr_t)name > 0x10000) {
                    WalkDList((const Gfx*)ResourceGetDataByName(name), depth + 1, budget);
                }
                if (branch) {
                    return;
                }
                cmd++;
                break;
            }
            case G_VTX_OTR_HASH: {
                uint64_t hash = ((uint64_t)cmd[1].words.w0 << 32) + (uint32_t)cmd[1].words.w1;
                TrackVertexResource(ResourceGetNameByCrc(hash));
                cmd += 2;
                break;
            }
            case G_VTX_OTR_FILEPATH: {
                const char* name = (const char*)cmd->words.w1;
                if ((uintptr_t)name > 0x10000) {
                    TrackVertexResource(name);
                }
                cmd += 2;
                break;
            }
            case G_TEXRECT_WIDE:
            case G_FILLWIDERECT:
            case G_LOADBLOCK_WIDE:
            case G_VTX_WIDE:
            case G_TRI1_WIDE:
                return; // (not in room meshes; their lengths aren't known here, so stop rather than misread)
            default:
                cmd++;
                break;
        }
    }
}

// The meshes of the rooms now loaded (the one he's in and the one he came from): their vertex lists
void TrackMeshes() {
    PlayState* play = gPlayState;

    sRoomFiles.clear();
    if (play == nullptr) {
        return;
    }
    Room* rooms[2] = { &play->roomCtx.curRoom, &play->roomCtx.prevRoom };
    for (Room* room : rooms) {
        MeshHeader* mesh = room->meshHeader;
        int budget = 200000;

        if ((room->num < 0) || (mesh == nullptr) ||
            (std::find(sWalkedRooms.begin(), sWalkedRooms.end(), mesh) != sWalkedRooms.end())) {
            continue;
        }
        sWalkedRooms.push_back(mesh);
        // (in this port a room mesh's display lists are given by their resource names, not as pointers)
        auto dl = [](Gfx* name) -> const Gfx* {
            return (name != nullptr) ? (const Gfx*)ResourceGetDataByName((const char*)name) : nullptr;
        };
        if (mesh->base.type == 0) {
            PolygonDlist* e = (PolygonDlist*)mesh->polygon0.start;
            for (int i = 0; (e != nullptr) && (i < mesh->polygon0.num); i++) {
                WalkDList(dl(e[i].opa), 0, budget);
                WalkDList(dl(e[i].xlu), 0, budget);
            }
        } else if (mesh->base.type == 2) {
            PolygonDlist2* e = (PolygonDlist2*)mesh->polygon2.start;
            for (int i = 0; (e != nullptr) && (i < mesh->polygon2.num); i++) {
                WalkDList(dl(e[i].opa), 0, budget);
                WalkDList(dl(e[i].xlu), 0, budget);
            }
        }
    }
}

int sMovedCol = 0, sMovedMesh = 0; // how many of the area's collision points and mesh vertices the sculpting moves

// The field onto the area: its collision points and polygons, and its meshes
void ApplyGround(s16 scene, bool growBounds) {
    auto fit = sFields.find(scene);
    static const Field sNone;
    const Field& field = (fit != sFields.end()) ? fit->second : sNone;

    TrackMeshes();
    sMovedCol = sMovedMesh = 0;
    if (sColHeader != nullptr) {
        CollisionHeader* h = sColHeader;
        std::vector<u8> moved(h->numVertices, 0);
        f32 lo = 0.0f, hi = 0.0f;

        for (int i = 0; i < h->numVertices; i++) {
            const Vec3s& o = sColVtx[i];
            f32 d = field.empty() ? 0.0f : Offset(field, o.x, o.y, o.z);
            s16 y = (s16)CLAMP(o.y + d, -32000.0f, 32000.0f);

            moved[i] = (h->vtxList[i].y != y) || (d != 0.0f);
            if (d != 0.0f) {
                sMovedCol++;
            }
            h->vtxList[i].y = y;
            lo = std::min(lo, d);
            hi = std::max(hi, d);
        }
        for (int i = 0; i < h->numPolygons; i++) {
            CollisionPoly* p = &h->polyList[i];
            int a = p->flags_vIA & 0x1FFF, b = p->flags_vIB & 0x1FFF, c = p->vIC;

            if ((a >= h->numVertices) || (b >= h->numVertices) || (c >= h->numVertices) ||
                !(moved[a] || moved[b] || moved[c])) {
                continue;
            }
            Vec3f va = { (f32)h->vtxList[a].x, (f32)h->vtxList[a].y, (f32)h->vtxList[a].z };
            Vec3f ab = { h->vtxList[b].x - va.x, h->vtxList[b].y - va.y, h->vtxList[b].z - va.z };
            Vec3f ac = { h->vtxList[c].x - va.x, h->vtxList[c].y - va.y, h->vtxList[c].z - va.z };
            Vec3f n = { ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x };
            f32 len = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));

            if (len < 0.001f) {
                continue;
            }
            n.x /= len, n.y /= len, n.z /= len;
            {
                // (facing the way it did: a floor stays a floor)
                const Vec3s& was = sColPolys[i].normal;

                if (n.x * was.x + n.y * was.y + n.z * was.z < 0.0f) {
                    n.x = -n.x, n.y = -n.y, n.z = -n.z;
                }
            }
            p->normal.x = (s16)(n.x * 32767.0f);
            p->normal.y = (s16)(n.y * 32767.0f);
            p->normal.z = (s16)(n.z * 32767.0f);
            p->dist = (s16)CLAMP(-(n.x * va.x + n.y * va.y + n.z * va.z), -32767.0f, 32767.0f);
        }
        if (growBounds) {
            // (before the game sorts the area's polygons into its lookup grid: room for what was raised and dug)
            h->minBounds.y = (s16)CLAMP(sColMin.y + lo - 50.0f, -32000.0f, 32000.0f);
            h->maxBounds.y = (s16)CLAMP(sColMax.y + hi + 50.0f, -32000.0f, 32000.0f);
        }
    }
    for (auto& [path, m] : sMeshes) {
        Vtx* list = m.vtx;

        for (size_t i = 0; i < m.origY.size() && i < m.count; i++) {
            f32 d = field.empty() ? 0.0f : Offset(field, list[i].v.ob[0], m.origY[i], list[i].v.ob[2]);

            list[i].v.ob[1] = (s16)CLAMP(m.origY[i] + d, -32000.0f, 32000.0f);
            if (d != 0.0f) {
                sMovedMesh++;
            }
        }
    }
}

// The brush: every point of the grid within its radius up or down, most at its middle
// The game's ground is made of large triangles: a brush between their corners would move nothing. So the stroke lands
// on the corner nearest the brush when none is well inside it (the brush is moved there, to show where it acts).
void BrushStroke(PlayState* play, Vec3f& at, f32 radius, f32 amount) {
    Field& field = sFields[play->sceneNum];

    TrackCollision(play->colCtx.colHeader);
    if (sColHeader != nullptr) {
        f32 best = 1.0e12f;
        int bestI = -1;

        for (int i = 0; i < (int)sColVtx.size(); i++) {
            const Vec3s& o = sColVtx[i];
            f32 d = SQ(o.x - at.x) + SQ(o.z - at.z);

            if ((d < best) && (fabsf(sColHeader->vtxList[i].y - at.y) < REACH)) {
                best = d;
                bestI = i;
            }
        }
        if ((bestI >= 0) && (best > SQ(radius * 0.4f)) && (best < SQ(2500.0f))) {
            at.x = sColVtx[bestI].x;
            at.z = sColVtx[bestI].z;
            at.y = sColHeader->vtxList[bestI].y;
        }
    }
    int r = (int)ceilf(radius / GRID);
    int cx = (int)lroundf(at.x / GRID), cz = (int)lroundf(at.z / GRID);

    for (int gx = cx - r; gx <= cx + r; gx++) {
        for (int gz = cz - r; gz <= cz + r; gz++) {
            f32 d = sqrtf(SQ(gx * GRID - at.x) + SQ(gz * GRID - at.z));

            if (d >= radius) {
                continue;
            }
            auto it = field.find(NodeKey(gx, gz));
            if (it == field.end()) {
                if (field.size() >= 6000) {
                    continue; // (enough for one area)
                }
                // (new: it moves the ground that's at the brush's height, as it was before any sculpting)
                Node node = { 0.0f, at.y - Offset(field, gx * GRID, at.y, gz * GRID) };
                it = field.emplace(NodeKey(gx, gz), node).first;
            }
            it->second.dy = CLAMP(it->second.dy + amount * 0.5f * (1.0f + cosf((f32)M_PI * d / radius)), -MAX_OFFSET,
                                  MAX_OFFSET);
        }
    }
    ApplyGround(play->sceneNum, false);
}

// ---- Saving: every piece of every area, with the file ----

void SaveBuild(const SaveContext& saveContext, int sectionID, bool fullSave) {
    std::vector<uint32_t> data;

    for (auto& [scene, list] : sPieces) {
        for (auto& p : list) {
            data.push_back((uint32_t)(int32_t)scene);
            data.push_back(p.type | ((uint32_t)p.tex << 8)); // (its texture in the second byte)
            data.push_back((uint32_t)(int32_t)lroundf(p.pos.x));
            data.push_back((uint32_t)(int32_t)lroundf(p.pos.y));
            data.push_back((uint32_t)(int32_t)lroundf(p.pos.z));
            data.push_back((uint32_t)(int32_t)p.rotY);
            data.push_back((uint32_t)lroundf(p.w));
            data.push_back((uint32_t)lroundf(p.h));
            data.push_back((uint32_t)lroundf(p.l));
        }
    }
    SaveManager::Instance->SaveData("count", (uint32_t)data.size());
    SaveManager::Instance->SaveArray("data", data.size(),
                                     [&](size_t i) { SaveManager::Instance->SaveData("", data[i]); });

    // The sculpted ground: scene, grid point, offset (in quarter units), the height it was sculpted at
    std::vector<uint32_t> ground;
    for (auto& [scene, field] : sFields) {
        for (auto& [key, node] : field) {
            if (fabsf(node.dy) < 0.25f) {
                continue;
            }
            ground.push_back((uint32_t)(int32_t)scene);
            ground.push_back(key);
            ground.push_back((uint32_t)(int32_t)lroundf(node.dy * 4.0f));
            ground.push_back((uint32_t)(int32_t)lroundf(node.refY));
        }
    }
    SaveManager::Instance->SaveData("groundCount", (uint32_t)ground.size());
    SaveManager::Instance->SaveArray("ground", ground.size(),
                                     [&](size_t i) { SaveManager::Instance->SaveData("", ground[i]); });
}

void LoadBuild() {
    uint32_t count = 0;
    std::vector<uint32_t> data;

    sPieces.clear();
    SaveManager::Instance->LoadData("count", count, (uint32_t)0);
    SaveManager::Instance->LoadArray("data", count, [&](size_t i) {
        uint32_t v = 0;

        SaveManager::Instance->LoadData("", v, (uint32_t)0);
        data.push_back(v);
    });
    for (size_t i = 0; i + 9 <= data.size(); i += 9) {
        PieceData p;

        p.uid = sNextUid++;
        p.type = (u8)std::min<uint32_t>(data[i + 1] & 0xFF, PIECE_COUNT - 1);
        p.tex = (u8)((data[i + 1] >> 8) & 0xFF);
        p.pos = { (f32)(int32_t)data[i + 2], (f32)(int32_t)data[i + 3], (f32)(int32_t)data[i + 4] };
        p.rotY = (s16)(int32_t)data[i + 5];
        p.w = (f32)data[i + 6];
        p.h = (f32)data[i + 7];
        p.l = (f32)data[i + 8];
        sPieces[(s16)(int32_t)data[i]].push_back(p);
    }

    uint32_t groundCount = 0;
    std::vector<uint32_t> ground;

    sFields.clear();
    SaveManager::Instance->LoadData("groundCount", groundCount, (uint32_t)0);
    SaveManager::Instance->LoadArray("ground", groundCount, [&](size_t i) {
        uint32_t v = 0;

        SaveManager::Instance->LoadData("", v, (uint32_t)0);
        ground.push_back(v);
    });
    for (size_t i = 0; i + 4 <= ground.size(); i += 4) {
        Node node = { (f32)(int32_t)ground[i + 2] / 4.0f, (f32)(int32_t)ground[i + 3] };

        sFields[(s16)(int32_t)ground[i]][ground[i + 1]] = node;
    }
}

void OnSceneSpawnActors() {
    PlayState* play = gPlayState;

    sPlacing = nullptr;
    sBrush = nullptr;
    if (play == nullptr) {
        return;
    }
    auto it = sPieces.find(play->sceneNum);
    if (it == sPieces.end()) {
        return;
    }
    for (auto& p : it->second) {
        SpawnPiece(play, p, false);
    }
}

void RegisterGanondorfBuild() {
    SaveManager::Instance->AddInitFunction([](bool isDebug) {
        sPieces.clear();
        sFields.clear();
    });
    // Leaving an area: its water list, collision and meshes as they were (they're the game's own, loaded once)
    // A room loaded since (walking into the next one): its mesh gets the area's sculpting too
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        PlayState* play = gPlayState;

        if ((play == nullptr) || !sFields.count(play->sceneNum) || sFields[play->sceneNum].empty()) {
            return;
        }
        const void* cur = play->roomCtx.curRoom.meshHeader;
        if ((cur != nullptr) && (std::find(sWalkedRooms.begin(), sWalkedRooms.end(), cur) == sWalkedRooms.end())) {
            ApplyGround(play->sceneNum, false);
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        RestoreWater();
        RestoreGround();
    });
    SaveManager::Instance->AddLoadFunction("ganondorfBuild", 1, LoadBuild);
    SaveManager::Instance->AddSaveFunction("ganondorfBuild", 1, SaveBuild, true, SECTION_PARENT_NONE);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(OnSceneSpawnActors);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfBuild, {});

} // namespace

// The area's collision is about to be set up: his pools into its water, his sculpting onto its ground
extern "C" void GdfBuild_BeforeCollision(PlayState* play, CollisionHeader* header) {
    RestoreWater();
    RestoreGround();
    if ((play == nullptr) || (header == nullptr)) {
        return;
    }
    if (sPieces.count(play->sceneNum)) {
        RebuildWater(header, play->sceneNum);
    }
    sFieldScene = play->sceneNum;
    auto it = sFields.find(play->sceneNum);
    if ((it != sFields.end()) && !it->second.empty()) {
        TrackCollision(header);
        ApplyGround(play->sceneNum, true);
    }
}

// A room of the area is loading: its mesh gets the sculpting too
extern "C" void GdfBuild_RoomLoading(PlayState* play, const char* roomFile) {
    if ((play == nullptr) || (roomFile == nullptr)) {
        return;
    }
    sRoomFiles.push_back(roomFile);
    auto it = sFields.find(play->sceneNum);
    if ((it != sFields.end()) && !it->second.empty()) {
        ApplyGround(play->sceneNum, false);
    }
}

int GdfBuild_KindCount() {
    return PIECE_COUNT;
}

const char* GdfBuild_KindName(int kind) {
    return sKinds[kind].name;
}

int GdfBuild_CountHere() {
    if (gPlayState == nullptr) {
        return 0;
    }
    auto it = sPieces.find(gPlayState->sceneNum);
    return (it == sPieces.end()) ? 0 : (int)it->second.size();
}

int GdfBuild_MaxHere() {
    return MAX_PIECES_PER_SCENE;
}

bool GdfBuild_IsPlacing() {
    return (sPlacing != nullptr) || (sBrush != nullptr);
}

const char* GdfBuild_PlacingTexName() {
    if ((sPlacing == nullptr) || (sPlacing->type == PIECE_WATER)) {
        return nullptr;
    }
    return sTexList[(sPlacing->tex < TEX_COUNT) ? sPlacing->tex : 0].name;
}

// What the sculpting is moving, for the help bar: the ground's corners he walks on, and the corners of what's seen
const char* GdfBuild_SculptInfo() {
    static char buf[96];

    snprintf(buf, sizeof(buf), "Moved: %d ground points, %d visible points (%d meshes found)", sMovedCol, sMovedMesh,
             (int)sMeshes.size());
    return buf;
}

bool GdfBuild_IsSculpting() {
    return sBrush != nullptr;
}

bool GdfBuild_Focus(Vec3f* pos) {
    GdfPiece* p = (sPlacing != nullptr) ? sPlacing : (sBrush != nullptr) ? sBrush : sTarget;

    if (p == nullptr) {
        return false;
    }
    *pos = p->dyna.actor.world.pos;
    return true;
}

// A see-through piece in front of him, to move about and set down
bool GdfBuild_Begin(int kind) {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (sPlacing != nullptr) || (GdfBuild_CountHere() >= MAX_PIECES_PER_SCENE)) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;
    PieceData d;

    d.uid = sNextUid++;
    d.type = (u8)kind;
    d.w = sKinds[kind].w;
    d.h = sKinds[kind].h;
    d.l = sKinds[kind].l;
    d.rotY = yaw;
    d.pos = player->actor.world.pos;
    d.pos.x += Math_SinS(yaw) * (60.0f + d.l * 0.5f);
    d.pos.z += Math_CosS(yaw) * (60.0f + d.l * 0.5f);
    sPlacing = SpawnPiece(play, d, true);
    for (int& h : sHeld) {
        h = 0;
    }
    sHeld[0] = 1; // (the A that chose it isn't the A that sets it down)
    return sPlacing != nullptr;
}

// A marker on the ground in front of him, to choose where (and facing which way) a summoning appears
bool GdfBuild_BeginTarget() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || GdfBuild_IsPlacing() || (sTarget != nullptr)) {
        return sTarget != nullptr;
    }
    Player* player = GET_PLAYER(play);
    PieceData d = {};

    d.type = PIECE_BRUSH;
    d.w = 50.0f;
    d.l = 80.0f; // (longer than wide: it shows which way the summoned thing will face)
    d.h = 3.0f;
    d.rotY = player->actor.shape.rot.y + 0x8000; // (facing him)
    d.pos = player->actor.world.pos;
    d.pos.x += Math_SinS(player->actor.shape.rot.y) * 150.0f;
    d.pos.z += Math_CosS(player->actor.shape.rot.y) * 150.0f;
    sTarget = SpawnPiece(play, d, true);
    for (int& h : sHeld) {
        h = 0;
    }
    sHeld[0] = 1; // (the A that chose it isn't the A that summons)
    return sTarget != nullptr;
}

bool GdfBuild_IsTargeting() {
    return sTarget != nullptr;
}

void GdfBuild_EndTarget() {
    if (sTarget != nullptr) {
        sTarget->dyna.actor.draw = nullptr;
        Actor_Kill(&sTarget->dyna.actor);
        sTarget = nullptr;
    }
}

// One frame of choosing: 1 with the place and facing when A is pressed (the marker stays, for more), -1 on B, else 0
int GdfBuild_TargetFrame(Input* input, Vec3f* pos, s16* yaw) {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (sTarget == nullptr)) {
        return -1;
    }
    Actor* a = &sTarget->dyna.actor;
    s16 camYaw = GET_PLAYER(play)->actor.shape.rot.y; // (the overhead camera's "up")
    f32 fx = Math_SinS(camYaw), fz = Math_CosS(camYaw);
    f32 sx = (ABS(input->rel.stick_x) > 8) ? input->rel.stick_x / 60.0f : 0.0f;
    f32 sy = (ABS(input->rel.stick_y) > 8) ? input->rel.stick_y / 60.0f : 0.0f;
    CollisionPoly* poly;
    s32 bgId;

    a->world.pos.x += (fx * sy - fz * sx) * 8.0f;
    a->world.pos.z += (fz * sy + fx * sx) * 8.0f;
    if (Rep(input, BTN_CLEFT, 1)) {
        a->shape.rot.y += 0x800;
    }
    if (Rep(input, BTN_CRIGHT, 2)) {
        a->shape.rot.y -= 0x800;
    }
    Vec3f from = { a->world.pos.x, a->world.pos.y + 150.0f, a->world.pos.z };
    f32 floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &from);
    if (floorY > BGCHECK_Y_MIN) {
        a->world.pos.y = floorY;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        GdfBuild_EndTarget();
        return -1;
    }
    if (Rep(input, BTN_A, 0) && (sHeld[0] == 1)) {
        *pos = a->world.pos;
        *yaw = a->shape.rot.y;
        return 1;
    }
    return 0;
}

void GdfBuild_Cancel() {
    GdfBuild_EndTarget();
    if (sBrush != nullptr) {
        sBrush->dyna.actor.draw = nullptr;
        Actor_Kill(&sBrush->dyna.actor);
        sBrush = nullptr;
    }
    if (sPlacing != nullptr) {
        sPlacing->dyna.actor.draw = nullptr;
        Actor_Kill(&sPlacing->dyna.actor);
        sPlacing = nullptr;
    }
}

// The brush out on the ground in front of him
bool GdfBuild_BeginSculpt() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || GdfBuild_IsPlacing()) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    PieceData d = {};

    d.uid = 0;
    d.type = PIECE_BRUSH;
    d.w = d.l = sBrushRadius * 2.0f;
    d.h = 3.0f;
    d.pos = player->actor.world.pos;
    d.pos.x += Math_SinS(player->actor.shape.rot.y) * 120.0f;
    d.pos.z += Math_CosS(player->actor.shape.rot.y) * 120.0f;
    sBrush = SpawnPiece(play, d, true);
    for (int& h : sHeld) {
        h = 0;
    }
    return sBrush != nullptr;
}

// Everything sculpted in this area, back as it was
bool GdfBuild_RestoreGroundHere() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || !sFields.count(play->sceneNum)) {
        return false;
    }
    sFields.erase(play->sceneNum);
    TrackCollision(play->colCtx.colHeader);
    ApplyGround(play->sceneNum, false);
    Sfx(NA_SE_EV_WALL_BROKEN);
    return true;
}

static bool SculptFrame(Input* input) {
    PlayState* play = gPlayState;
    Actor* a = &sBrush->dyna.actor;
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;
    f32 fx = Math_SinS(yaw), fz = Math_CosS(yaw);
    f32 sx = (ABS(input->rel.stick_x) > 8) ? input->rel.stick_x / 60.0f : 0.0f;
    f32 sy = (ABS(input->rel.stick_y) > 8) ? input->rel.stick_y / 60.0f : 0.0f;
    CollisionPoly* poly;
    s32 bgId;

    a->world.pos.x += (fx * sy - fz * sx) * 8.0f;
    a->world.pos.z += (fz * sy + fx * sx) * 8.0f;
    if (Rep(input, BTN_DUP, 5) || Rep(input, BTN_DRIGHT, 7)) {
        sBrushRadius += 10.0f;
    }
    if (Rep(input, BTN_DDOWN, 6) || Rep(input, BTN_DLEFT, 8)) {
        sBrushRadius -= 10.0f;
    }
    sBrushRadius = CLAMP(sBrushRadius, 50.0f, 400.0f);
    sBrush->w = sBrush->l = sBrushRadius * 2.0f;
    Piece_ApplySize(sBrush);
    // The marker lies on the ground under it
    Vec3f from = { a->world.pos.x, a->world.pos.y + 150.0f, a->world.pos.z };
    f32 floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &from);
    if (floorY > BGCHECK_Y_MIN) {
        a->world.pos.y = floorY;
    }
    if (CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        BrushStroke(play, a->world.pos, sBrushRadius, 4.0f);
        a->world.pos.y += 4.0f;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_Z)) {
        BrushStroke(play, a->world.pos, sBrushRadius, -4.0f);
        a->world.pos.y -= 4.0f;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        GdfBuild_Cancel();
        Sfx(NA_SE_SY_CANCEL);
        return false;
    }
    return true;
}

// One frame of placing (the world is still: this moves the piece itself). False once it's set down or cancelled.
bool GdfBuild_PlacingFrame(Input* input) {
    PlayState* play = gPlayState;
    GdfPiece* self = sPlacing;

    if ((play != nullptr) && (sBrush != nullptr)) {
        return SculptFrame(input);
    }
    if ((play == nullptr) || (self == nullptr)) {
        return false;
    }
    Actor* a = &self->dyna.actor;
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y; // the overhead camera's "up"
    f32 fx = Math_SinS(yaw), fz = Math_CosS(yaw);
    f32 sx = (ABS(input->rel.stick_x) > 8) ? input->rel.stick_x / 60.0f : 0.0f;
    f32 sy = (ABS(input->rel.stick_y) > 8) ? input->rel.stick_y / 60.0f : 0.0f;
    bool tall = CHECK_BTN_ALL(input->cur.button, BTN_R);

    a->world.pos.x += (fx * sy - fz * sx) * 6.0f;
    a->world.pos.z += (fz * sy + fx * sx) * 6.0f;
    if (Rep(input, BTN_CLEFT, 1)) {
        a->shape.rot.y += 0x400;
    }
    if (Rep(input, BTN_CRIGHT, 2)) {
        a->shape.rot.y -= 0x400;
    }
    if (Rep(input, BTN_CUP, 3)) {
        if (tall) {
            self->h += 4.0f;
        } else {
            a->world.pos.y += 4.0f;
        }
    }
    if (Rep(input, BTN_CDOWN, 4)) {
        if (tall) {
            self->h -= 4.0f;
        } else {
            a->world.pos.y -= 4.0f;
        }
    }
    if (Rep(input, BTN_DUP, 5)) {
        self->l += 10.0f;
    }
    if (Rep(input, BTN_DDOWN, 6)) {
        self->l -= 10.0f;
    }
    if (Rep(input, BTN_DRIGHT, 7)) {
        self->w += 10.0f;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_L) && (self->type != PIECE_WATER)) {
        self->tex = (self->tex + 1) % TEX_COUNT;
        Sfx(NA_SE_SY_CURSOR);
    }
    if (Rep(input, BTN_DLEFT, 8)) {
        self->w -= 10.0f;
    }
    self->w = CLAMP(self->w, 20.0f, 800.0f);
    self->l = CLAMP(self->l, 20.0f, 800.0f);
    self->h = CLAMP(self->h, 8.0f, 600.0f);
    if (CHECK_BTN_ALL(input->press.button, BTN_Z)) {
        // Down onto the floor beneath it
        CollisionPoly* poly;
        s32 bgId;
        Vec3f from = { a->world.pos.x, a->world.pos.y + 60.0f, a->world.pos.z };
        f32 floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &from);

        if (floorY > BGCHECK_Y_MIN) {
            a->world.pos.y = floorY;
        }
    }
    if (self->type == PIECE_WATER) {
        a->shape.rot.y = 0; // (the game's water lies square to the world)
    }
    a->world.rot.y = a->shape.rot.y;
    Piece_ApplySize(self);

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        a->draw = nullptr;
        Actor_Kill(a);
        sPlacing = nullptr;
        Sfx(NA_SE_SY_CANCEL);
        return false;
    }
    if (Rep(input, BTN_A, 0) && (sHeld[0] == 1)) {
        PieceData d;

        d.uid = self->uid;
        d.type = self->type;
        d.pos = a->world.pos;
        d.rotY = a->shape.rot.y;
        d.w = self->w;
        d.h = self->h;
        d.l = self->l;
        d.tex = self->tex;
        a->home.pos = a->world.pos;
        sPieces[play->sceneNum].push_back(d);
        Piece_Solidify(self, play);
        if (d.type == PIECE_WATER) {
            RebuildWater(play->colCtx.colHeader, play->sceneNum);
        }
        sPlacing = nullptr;
        Sfx(NA_SE_EV_STONE_BOUND);
        return false;
    }
    return true;
}

// The piece nearest him, gone
bool GdfBuild_RemoveNearest() {
    PlayState* play = gPlayState;

    if (play == nullptr) {
        return false;
    }
    auto it = sPieces.find(play->sceneNum);
    if ((it == sPieces.end()) || it->second.empty()) {
        return false;
    }
    Vec3f p = GET_PLAYER(play)->actor.world.pos;
    auto best = std::min_element(it->second.begin(), it->second.end(), [&](const PieceData& a, const PieceData& b) {
        return Math_Vec3f_DistXYZ(&p, (Vec3f*)&a.pos) < Math_Vec3f_DistXYZ(&p, (Vec3f*)&b.pos);
    });
    RemoveActor(play, best->uid);
    it->second.erase(best);
    RebuildWater(play->colCtx.colHeader, play->sceneNum);
    Sfx(NA_SE_EV_WALL_BROKEN);
    return true;
}

// Everything he built in this area, gone
int GdfBuild_ClearHere() {
    PlayState* play = gPlayState;

    if (play == nullptr) {
        return 0;
    }
    auto it = sPieces.find(play->sceneNum);
    if (it == sPieces.end()) {
        return 0;
    }
    int n = (int)it->second.size();
    for (auto& p : it->second) {
        RemoveActor(play, p.uid);
    }
    sPieces.erase(it);
    RebuildWater(play->colCtx.colHeader, play->sceneNum);
    if (n > 0) {
        Sfx(NA_SE_EV_WALL_BROKEN);
    }
    return n;
}
