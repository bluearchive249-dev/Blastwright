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
struct GdfPocketGround {
    f32 w = 2000.0f, l = 2000.0f; // its size (world units), centred on the area's middle
    f32 wallH = 0.0f;              // walls round the edge (0: none, and falling off the edge voids out)
    int floorTex = 0, wallTex = 0; // Terraform's textures
    int sound = 2;                 // the game's footstep material (0 dirt, 1 sand, 2 stone, ... 13 carpet)
};
const char* GdfBuild_SoundName(int sound); // 0..13
void GdfBuild_MakePocketGround(CollisionHeader* header, const GdfPocketGround& g); // replaces the area's collision
void GdfBuild_SpawnPocketGround(PlayState* play);                                // the actor that draws it
#endif

#ifdef __cplusplus
// Terraform's glowing ground ring (the brush's), for the Map Editor's trigger zones: call from an actor's draw
void GdfBuild_DrawRing(PlayState* play, Vec3f center, f32 radius, s16 facing, bool pointer);
#endif
