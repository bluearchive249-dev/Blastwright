// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
#pragma once
// Play as Ganondorf: placed pieces (stairs, ramps, platforms...), see GanondorfBuild.cpp. Include after z64.h.

int GdfBuild_KindCount();
const char* GdfBuild_KindName(int kind);
int GdfBuild_CountHere();
int GdfBuild_MaxHere();
bool GdfBuild_IsPlacing();
bool GdfBuild_Focus(Vec3f* pos);
bool GdfBuild_Begin(int kind);
bool GdfBuild_PlacingFrame(Input* input);
bool GdfBuild_RemoveNearest();
int GdfBuild_ClearHere();
void GdfBuild_Cancel();
bool GdfBuild_IsSculpting();
bool GdfBuild_BeginSculpt();
bool GdfBuild_RestoreGroundHere();
bool GdfBuild_BeginTarget();
bool GdfBuild_IsTargeting();
void GdfBuild_EndTarget();
int GdfBuild_TargetFrame(Input* input, Vec3f* pos, s16* yaw);
const char* GdfBuild_PlacingTexName();
const char* GdfBuild_SculptInfo();

#ifdef __cplusplus
// ---- For the Map Editor (GanondorfMapEditor.cpp): this area's pieces by number, and the area's build as JSON ----
#include <vector>
#include <nlohmann/json.hpp>
struct GdfBuildPieceInfo {
    u32 uid = 0;
    int type = 0;
    int tex = 0;
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    s16 rotY = 0;
    f32 w = 100.0f, h = 100.0f, l = 100.0f;
};
int GdfBuild_TexCount();
const char* GdfBuild_TexName(int tex);
std::vector<GdfBuildPieceInfo> GdfBuild_ListHere();
bool GdfBuild_SetPiece(const GdfBuildPieceInfo& info); // changed in place (respawned at once)
u32 GdfBuild_AddPiece(const GdfBuildPieceInfo& info);  // 0 when the area is full
bool GdfBuild_RemovePiece(u32 uid);
// {"pieces":[{type,tex,pos,rot_y,size}], "ground":[[key,dy,refY],...]}
nlohmann::json GdfBuild_ExportHere();
// Replaces the area's pieces and sculpting. live: the area is loaded now (actors and ground redone at once); else data
// only, for an area that's about to load
void GdfBuild_ImportScene(s16 scene, const nlohmann::json& j, bool live);
#endif

#ifdef __cplusplus
// ---- Custom areas (Map Editor): a ground made to size as the area's own collision, drawn from that collision ----
#define GDF_POCKET_MAX_CELLS 64                   // a side of the grid, at most
#define GDF_POCKET_MIN_Y (-2000.0f)               // how low and high the ground can be shaped
#define GDF_POCKET_MAX_Y 4500.0f
#define GDF_POCKET_KEEP 255                       // a cell's texture / sound: the area's own
struct GdfPocketGround {
    f32 w = 2000.0f, l = 2000.0f; // its size (world units), centred on the area's middle
    f32 wallH = 0.0f;              // walls round the edge (0: none, and falling off the edge voids out)
    int floorTex = 0, wallTex = 0; // Terraform's textures
    int sound = 2;                 // the game's footstep material (0 dirt, 1 sand, 2 stone, ... 13 carpet)
    f32 cell = 100.0f;             // the grid's spacing (it shapes in steps this size)
    // Its shape (sized by GdfBuild_FitPocketGrid): a height at each grid point, and per cell a texture, a footstep
    // sound (GDF_POCKET_KEEP: the area's own) and whether it's a hole
    int nx = 0, nz = 0;
    std::vector<f32> h;         // (nx + 1) * (nz + 1), row by row from -z
    std::vector<u8> tex, snd;   // nx * nz
    std::vector<u8> hole;       // nx * nz (1: a hole)
};
const char* GdfBuild_SoundName(int sound); // 0..13
void GdfBuild_FitPocketGrid(GdfPocketGround& g); // the grid made to fit w, l and cell (the shape kept, stretched)
void GdfBuild_PocketPoint(const GdfPocketGround& g, int i, int j, f32* x, f32* z); // a grid point's place
void GdfBuild_MakePocketGround(CollisionHeader* header, const GdfPocketGround& g); // replaces the area's collision
// The ground made again from g while the area is loaded (collision and its lookup redone in place): false when it
// can't be (then reload the area)
bool GdfBuild_UpdatePocketGround(PlayState* play, const GdfPocketGround& g);
void GdfBuild_SpawnPocketGround(PlayState* play); // the actor that draws it
#endif

#ifdef __cplusplus
// Terraform's glowing ground ring (the brush's), for the Map Editor's trigger zones: call from an actor's draw
void GdfBuild_DrawRing(PlayState* play, Vec3f center, f32 radius, s16 facing, bool pointer);
#endif
