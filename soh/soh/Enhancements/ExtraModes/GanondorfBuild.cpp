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

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/ResourceManagerHelpers.h"
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
    Vec3f pos;
    s16 rotY;
    f32 w, h, l;
};

struct GdfPiece {
    DynaPolyActor dyna;
    u32 uid;
    u8 type;
    u8 ghost; // being placed: see-through, no collision yet
    f32 w, h, l;
};

std::map<s16, std::vector<PieceData>> sPieces; // by scene
u32 sNextUid = 1;
s32 sActorId = -1;
PieceData sSpawnSpec;
bool sSpawnGhost = false;
GdfPiece* sPlacing = nullptr;
GdfPiece* sBrush = nullptr; // the sculpting brush's marker (sculpting while it exists)
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
    s.col.cameraDataList = nullptr;
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
}

void Piece_Update(Actor* thisx, PlayState* play) {
}

} // namespace

// (outside the unnamed namespace: the draw macros declare the game's frame interpolation functions in place, and inside it
// they would name new functions of their own, which don't exist)
static void Piece_Draw(Actor* thisx, PlayState* play) {
    GdfPiece* self = (GdfPiece*)thisx;
    bool water = (self->type == PIECE_WATER);
    bool ghost = self->ghost || water; // (water is drawn see-through too)

    OPEN_DISPS(play->state.gfxCtx);
    Gfx*& disp = ghost ? POLY_XLU_DISP : POLY_OPA_DISP;

    if (ghost) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
    }
    // Plain lit stone (no texture): its colour times the scene's light on each face
    gDPSetCombineLERP(disp++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
    if (water) {
        gDPSetPrimColor(disp++, 0, 0, 70, 140, 215, self->ghost ? 90 : 125);
    } else if (ghost) {
        gDPSetPrimColor(disp++, 0, 0, 255, 215, 120, 130);
    } else {
        gDPSetPrimColor(disp++, 0, 0, 150, 142, 132, 255);
    }
    if (self->type == PIECE_STAIRS) {
        // Steps about 12 units tall each: boxes of rising height along its length (its collision is the slope)
        int n = CLAMP((int)(self->h / 12.0f + 0.5f), 2, 40);

        for (int i = 0; i < n; i++) {
            Matrix_Push();
            Matrix_Translate(0.0f, 0.0f, -500.0f + (i + 0.5f) * UNIT / n, MTXMODE_APPLY);
            Matrix_Scale(1.0f, (i + 1.0f) / n, 1.0f / n, MTXMODE_APPLY);
            gSPMatrix(disp++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(disp++, sBox.dl);
            Matrix_Pop();
        }
    } else {
        gSPMatrix(disp++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(disp++, sKinds[self->type].wedge ? sWedge.dl : sBox.dl);
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
#define MAX_OFFSET 400.0f

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
    std::shared_ptr<Fast::Vertex> res;
    std::vector<s16> origY;
};
std::unordered_map<std::string, MeshVtx> sMeshes;
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
        for (size_t i = 0; i < m.origY.size() && i < m.res->VertexList.size(); i++) {
            m.res->VertexList[i].v.ob[1] = m.origY[i];
        }
    }
    sMeshes.clear();
    sRoomFiles.clear();
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
void TrackMeshes() {
    for (std::string path : sRoomFiles) {
        if (path.substr(0, 7) == "__OTR__") {
            path = path.substr(7);
        }
        size_t pos;
        if (ResourceMgr_IsGameMasterQuest() && ((pos = path.find("/nonmq/")) != std::string::npos)) {
            path.replace(pos, 7, "/mq/");
        }
        int count = 0;
        char** files = ResourceMgr_ListFiles((path + "Vtx_*").c_str(), &count);

        for (int i = 0; i < count; i++) {
            std::string name = files[i];

            free(files[i]);
            if (sMeshes.count(name)) {
                continue;
            }
            auto res = std::dynamic_pointer_cast<Fast::Vertex>(
                Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(name.c_str()));
            if (res == nullptr) {
                continue;
            }
            MeshVtx m;
            m.res = res;
            m.origY.reserve(res->VertexList.size());
            for (auto& v : res->VertexList) {
                m.origY.push_back(v.v.ob[1]);
            }
            sMeshes.emplace(name, std::move(m));
        }
        free(files);
    }
    sRoomFiles.clear();
}

// The field onto the area: its collision points and polygons, and its meshes
void ApplyGround(s16 scene, bool growBounds) {
    auto fit = sFields.find(scene);
    static const Field sNone;
    const Field& field = (fit != sFields.end()) ? fit->second : sNone;

    TrackMeshes();
    if (sColHeader != nullptr) {
        CollisionHeader* h = sColHeader;
        std::vector<u8> moved(h->numVertices, 0);
        f32 lo = 0.0f, hi = 0.0f;

        for (int i = 0; i < h->numVertices; i++) {
            const Vec3s& o = sColVtx[i];
            f32 d = field.empty() ? 0.0f : Offset(field, o.x, o.y, o.z);
            s16 y = (s16)CLAMP(o.y + d, -32000.0f, 32000.0f);

            moved[i] = (h->vtxList[i].y != y) || (d != 0.0f);
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
        auto& list = m.res->VertexList;

        for (size_t i = 0; i < m.origY.size() && i < list.size(); i++) {
            f32 d = field.empty() ? 0.0f : Offset(field, list[i].v.ob[0], m.origY[i], list[i].v.ob[2]);

            list[i].v.ob[1] = (s16)CLAMP(m.origY[i] + d, -32000.0f, 32000.0f);
        }
    }
}

// The brush: every point of the grid within its radius up or down, most at its middle
void BrushStroke(PlayState* play, Vec3f at, f32 radius, f32 amount) {
    Field& field = sFields[play->sceneNum];
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
    TrackCollision(play->colCtx.colHeader);
    ApplyGround(play->sceneNum, false);
}

// ---- Saving: every piece of every area, with the file ----

void SaveBuild(const SaveContext& saveContext, int sectionID, bool fullSave) {
    std::vector<uint32_t> data;

    for (auto& [scene, list] : sPieces) {
        for (auto& p : list) {
            data.push_back((uint32_t)(int32_t)scene);
            data.push_back(p.type);
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
        p.type = (u8)std::min<uint32_t>(data[i + 1], PIECE_COUNT - 1);
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

bool GdfBuild_IsSculpting() {
    return sBrush != nullptr;
}

bool GdfBuild_Focus(Vec3f* pos) {
    GdfPiece* p = (sPlacing != nullptr) ? sPlacing : sBrush;

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

void GdfBuild_Cancel() {
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
        BrushStroke(play, a->world.pos, sBrushRadius, 3.0f);
        a->world.pos.y += 3.0f;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_Z)) {
        BrushStroke(play, a->world.pos, sBrushRadius, -3.0f);
        a->world.pos.y -= 3.0f;
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
