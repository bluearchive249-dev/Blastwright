// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// Play as Ganondorf: terraforming, step 1: placed pieces. From the Dark Summoning menu's "Build" page he sets down
// stairs, ramps, platforms, walls, pillars and bridges: each its own actor with real (dynamic) collision and a stone
// look made here in code. What he builds is kept per area in the save file and stands again whenever he returns.
// Placing (the world still, the camera overhead on the piece, shown see-through until it's set down):
//   Stick: move   C-Left/C-Right: turn   C-Up/C-Down: raise/lower (with R held: taller/shorter)
//   D-pad up/down: longer/shorter   D-pad left/right: wider/narrower   Z: down onto the floor   A: set down   B: cancel

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "GanondorfBuild.h"

namespace {

enum PieceType { PIECE_STAIRS, PIECE_RAMP, PIECE_PLATFORM, PIECE_WALL, PIECE_PILLAR, PIECE_BRIDGE, PIECE_COUNT };

struct PieceKind {
    const char* name;
    bool wedge;       // rises along its length (stairs, ramp); else a box
    f32 w, h, l;      // its size as it first appears
};
const PieceKind sKinds[PIECE_COUNT] = {
    { "Stairs", true, 120.0f, 100.0f, 160.0f },  { "Ramp", true, 120.0f, 80.0f, 220.0f },
    { "Platform", false, 200.0f, 20.0f, 200.0f }, { "Wall", false, 240.0f, 160.0f, 24.0f },
    { "Pillar", false, 50.0f, 240.0f, 50.0f },    { "Bridge", false, 100.0f, 12.0f, 400.0f },
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

    if (!self->ghost) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, self->dyna.bgId);
    }
    if (sPlacing == self) {
        sPlacing = nullptr;
    }
}

void Piece_Update(Actor* thisx, PlayState* play) {
}

void Piece_Draw(Actor* thisx, PlayState* play) {
    GdfPiece* self = (GdfPiece*)thisx;
    bool ghost = self->ghost;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx*& disp = ghost ? POLY_XLU_DISP : POLY_OPA_DISP;

    if (ghost) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
    }
    // Plain lit stone (no texture): its colour times the scene's light on each face
    gDPSetCombineLERP(disp++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
    if (ghost) {
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
}

void OnSceneSpawnActors() {
    PlayState* play = gPlayState;

    sPlacing = nullptr;
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
    SaveManager::Instance->AddInitFunction([](bool isDebug) { sPieces.clear(); });
    SaveManager::Instance->AddLoadFunction("ganondorfBuild", 1, LoadBuild);
    SaveManager::Instance->AddSaveFunction("ganondorfBuild", 1, SaveBuild, true, SECTION_PARENT_NONE);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(OnSceneSpawnActors);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfBuild, {});

} // namespace

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
    return sPlacing != nullptr;
}

bool GdfBuild_Focus(Vec3f* pos) {
    if (sPlacing == nullptr) {
        return false;
    }
    *pos = sPlacing->dyna.actor.world.pos;
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
    if (sPlacing != nullptr) {
        sPlacing->dyna.actor.draw = nullptr;
        Actor_Kill(&sPlacing->dyna.actor);
        sPlacing = nullptr;
    }
}

// One frame of placing (the world is still: this moves the piece itself). False once it's set down or cancelled.
bool GdfBuild_PlacingFrame(Input* input) {
    PlayState* play = gPlayState;
    GdfPiece* self = sPlacing;

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
    if (n > 0) {
        Sfx(NA_SE_EV_WALL_BROKEN);
    }
    return n;
}
