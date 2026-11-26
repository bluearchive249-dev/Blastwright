// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// Play as Ganondorf: the Map Editor. A window (Dev Tools > Map Editor) for changing an area while playing in it:
// select any actor, move and turn it, change its settings, hide it, or add new ones from the full list; build pieces
// (from Terraform: stairs, ramps, platforms, walls, pillars, bridges, water) set by number. Everything is kept in a
// layout file per area (mods/layouts/), saved from the window, and applied every time the area loads. The file is
// watched: edited by hand while the game runs, it's read again at once ("Reload area" shows it).
// The game's own data is never changed: a base actor is hidden or moved by the layout as the room spawns it.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/window/gui/GuiWindow.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/util.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

// Terraform's build pieces where this copy of the game has them (GanondorfBuild.cpp); without them the Terrain tab is
// gone and a layout's terrain is kept in its file as it is
#if __has_include("GanondorfBuild.h")
#include "GanondorfBuild.h"
#define GDF_HAS_BUILD 1
#else
#define GDF_HAS_BUILD 0
#endif

namespace {

#define SEL_TAG "GdfMapEditSel"

// ---- The layout: what the area has added, and what it changes of the game's own actors ----

struct AddedActor {
    u32 uid = 0;
    s16 id = 0;
    s16 params = 0;
    s16 room = -1; // -1: the whole area (it stays whichever room he's in)
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    Vec3s rot = { 0, 0, 0 };
    std::string note;
};

struct BaseEdit {
    s16 room = 0;
    s16 index = 0; // its place in the room's actor list
    s16 id = 0;    // (checked: a list that differs, as in Master Quest or another setup, is left alone)
    bool hide = false;
    bool moved = false;
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    Vec3s rot = { 0, 0, 0 };
    bool hasParams = false;
    s16 params = 0;
};

struct Layout {
    std::vector<AddedActor> added;
    std::vector<BaseEdit> edits;
    nlohmann::json terrain;
    bool hasTerrain = false;
};

Layout sLayout;
s16 sScene = -1;
int sSetup = -1;
bool sMq = false;
std::string sPath;
std::filesystem::file_time_type sFileTime{};
uintmax_t sFileSize = 0;
bool sFileExists = false;
bool sDirty = false;
bool sFileChangedOutside = false;
std::string sStatus;
u32 sNextUid = 1;
std::map<u32, Actor*> sAddedLive; // the added actors standing now
bool sSceneWideDone = false;
s16 sSpawnedRoom = -1; // the room the added actors were last put in
std::chrono::steady_clock::time_point sLastCheck{};
std::chrono::steady_clock::time_point sLastChange{};

Actor* sSel = nullptr;
bool sFreeze = false;
bool sFreezeWas = false;
bool sAutoSave = false;
bool sAutoApply = true;

const char* ActorLabel(s16 id) {
    static std::string s;
    ActorDB::Entry e = ActorDB::Instance->RetrieveEntry(id);
    if (!e.entry.valid) {
        return "???";
    }
    s = e.desc.empty() ? e.name : e.desc;
    return s.c_str();
}

std::string ActorInternalName(s16 id) {
    ActorDB::Entry e = ActorDB::Instance->RetrieveEntry(id);
    return e.entry.valid ? e.name : "";
}

f32 ToDeg(s16 r) {
    return r * (360.0f / 65536.0f);
}

s16 FromDeg(f32 d) {
    return (s16)(s32)lroundf(d * (65536.0f / 360.0f));
}

std::string Hex4(int v) {
    char b[16];
    snprintf(b, sizeof(b), "0x%04X", (u16)v);
    return b;
}

s16 ReadParams(const nlohmann::json& v) {
    if (v.is_number()) {
        return (s16)v.get<int>();
    }
    if (v.is_string()) {
        try {
            return (s16)(int)std::stoul(v.get<std::string>(), nullptr, 0);
        } catch (...) {
        }
    }
    return 0;
}

Vec3f ReadVec3f(const nlohmann::json& v) {
    if (v.is_array() && (v.size() == 3) && v[0].is_number() && v[1].is_number() && v[2].is_number()) {
        return { v[0].get<f32>(), v[1].get<f32>(), v[2].get<f32>() };
    }
    return { 0.0f, 0.0f, 0.0f };
}

Vec3s ReadRot(const nlohmann::json& v) {
    Vec3f d = ReadVec3f(v);
    return { FromDeg(d.x), FromDeg(d.y), FromDeg(d.z) };
}

nlohmann::ordered_json PosJson(const Vec3f& p) {
    return { lroundf(p.x), lroundf(p.y), lroundf(p.z) };
}

nlohmann::ordered_json RotJson(const Vec3s& r) {
    return { roundf(ToDeg(r.x) * 10.0f) / 10.0f, roundf(ToDeg(r.y) * 10.0f) / 10.0f,
             roundf(ToDeg(r.z) * 10.0f) / 10.0f };
}

std::string SafeName(const std::string& s) {
    std::string out;
    for (char c : s) {
        out += (std::isalnum((unsigned char)c) ? c : '_');
    }
    return out;
}

std::string LayoutPath(s16 scene, int setup, bool mq) {
    std::string name = std::to_string(scene) + "_" + SafeName(SohUtils::GetSceneName(scene)) + (mq ? "_mq" : "") +
                       "_setup" + std::to_string(setup) + ".json";
    return Ship::Context::GetPathRelativeToAppDirectory("mods/layouts/" + name);
}

void NoteFile() {
    sFileExists = std::filesystem::exists(sPath);
    if (sFileExists) {
        sFileTime = std::filesystem::last_write_time(sPath);
        sFileSize = std::filesystem::file_size(sPath);
    }
}

bool ReadFile(Layout& out) {
    out = Layout();
    if (!std::filesystem::exists(sPath)) {
        return true;
    }
    std::ifstream in(sPath, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    nlohmann::json j = nlohmann::json::parse(text, nullptr, false, true); // (comments allowed)
    if (!j.is_object()) {
        return false; // (half written, or a typo: kept as it was until it reads)
    }
    if (j.contains("actors") && j["actors"].is_array()) {
        for (auto& e : j["actors"]) {
            if (!e.is_object() || !e.contains("id")) {
                continue;
            }
            AddedActor a;
            a.uid = sNextUid++;
            a.id = (s16)e["id"].get<int>();
            a.params = ReadParams(e.value("params", nlohmann::json(0)));
            a.room = (s16)e.value("room", -1);
            a.pos = ReadVec3f(e.value("pos", nlohmann::json()));
            a.rot = ReadRot(e.value("rot", nlohmann::json()));
            a.note = e.value("note", std::string());
            out.added.push_back(a);
        }
    }
    if (j.contains("changed") && j["changed"].is_array()) {
        for (auto& e : j["changed"]) {
            if (!e.is_object() || !e.contains("room") || !e.contains("index") || !e.contains("id")) {
                continue;
            }
            BaseEdit b;
            b.room = (s16)e["room"].get<int>();
            b.index = (s16)e["index"].get<int>();
            b.id = (s16)e["id"].get<int>();
            b.hide = e.value("hide", false);
            if (e.contains("pos")) {
                b.moved = true;
                b.pos = ReadVec3f(e["pos"]);
                b.rot = ReadRot(e.value("rot", nlohmann::json()));
            }
            if (e.contains("params")) {
                b.hasParams = true;
                b.params = ReadParams(e["params"]);
            }
            out.edits.push_back(b);
        }
    }
    if (j.contains("terrain") && j["terrain"].is_object()) {
        out.terrain = j["terrain"];
        out.hasTerrain = true;
    }
    return true;
}

// The layout for the area he's in (read from its file when the area or its setup changes)
void EnsureLayout(PlayState* play) {
    if (play == nullptr) {
        return;
    }
    int setup = gSaveContext.sceneLayer;
    bool mq = ResourceMgr_IsSceneMasterQuest(play->sceneNum);
    if ((play->sceneNum == sScene) && (setup == sSetup) && (mq == sMq)) {
        return;
    }
    sScene = play->sceneNum;
    sSetup = setup;
    sMq = mq;
    sPath = LayoutPath(sScene, sSetup, sMq);
    sDirty = false;
    sFileChangedOutside = false;
    sSel = nullptr;
    try {
        if (!ReadFile(sLayout)) {
            sStatus = "The layout file has an error; fix it and it's read again";
        } else {
            sStatus = "";
        }
        NoteFile();
    } catch (...) {
        sLayout = Layout();
        sStatus = "The layout file couldn't be read";
    }
}

void SaveLayout() {
    PlayState* play = gPlayState;
    if ((play == nullptr) || (sScene != play->sceneNum)) {
        return;
    }
    nlohmann::ordered_json j;
    j["about"] = "Map Editor layout (Play as Ganondorf). Changed from the game's Map Editor window or by hand; the game "
                 "reads it again when it changes. rot is in degrees, params as in the game's actor list.";
    j["scene"] = sScene;
    j["scene_name"] = SohUtils::GetSceneName(sScene);
    j["setup"] = sSetup;
    j["master_quest"] = sMq;
    nlohmann::ordered_json actors = nlohmann::ordered_json::array();
    for (auto& a : sLayout.added) {
        nlohmann::ordered_json e;
        e["actor"] = ActorInternalName(a.id);
        e["id"] = a.id;
        e["params"] = Hex4(a.params);
        e["room"] = a.room;
        e["pos"] = PosJson(a.pos);
        e["rot"] = RotJson(a.rot);
        if (!a.note.empty()) {
            e["note"] = a.note;
        }
        actors.push_back(e);
    }
    j["actors"] = actors;
    nlohmann::ordered_json changed = nlohmann::ordered_json::array();
    for (auto& b : sLayout.edits) {
        nlohmann::ordered_json e;
        e["actor"] = ActorInternalName(b.id);
        e["id"] = b.id;
        e["room"] = b.room;
        e["index"] = b.index;
        if (b.hide) {
            e["hide"] = true;
        }
        if (b.moved) {
            e["pos"] = PosJson(b.pos);
            e["rot"] = RotJson(b.rot);
        }
        if (b.hasParams) {
            e["params"] = Hex4(b.params);
        }
        changed.push_back(e);
    }
    j["changed"] = changed;
#if GDF_HAS_BUILD
    nlohmann::json terrain = GdfBuild_ExportHere();
    if (sLayout.hasTerrain || !terrain["pieces"].empty() || !terrain["ground"].empty()) {
        j["terrain"] = nlohmann::ordered_json::parse(terrain.dump());
        sLayout.terrain = terrain;
        sLayout.hasTerrain = true;
    }
#else
    if (sLayout.hasTerrain) {
        j["terrain"] = nlohmann::ordered_json::parse(sLayout.terrain.dump());
    }
#endif
    try {
        std::filesystem::create_directories(std::filesystem::path(sPath).parent_path());
        std::string tmp = sPath + ".tmp";
        {
            std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
            o << j.dump(2) << "\n";
        }
        std::filesystem::rename(tmp, sPath);
        NoteFile();
        sDirty = false;
        sFileChangedOutside = false;
        sStatus = "Saved";
    } catch (...) {
        sStatus = "Couldn't write the layout file";
    }
}

void Changed() {
    sDirty = true;
    sStatus = "";
    sLastChange = std::chrono::steady_clock::now(); // (auto-save waits for a second of quiet: not every frame of a drag)
}

BaseEdit* FindEdit(s16 room, s16 index) {
    for (auto& b : sLayout.edits) {
        if ((b.room == room) && (b.index == index)) {
            return &b;
        }
    }
    return nullptr;
}

BaseEdit& EditFor(Actor* a) {
    s16 index = GetActorListIndex(a);
    BaseEdit* b = FindEdit(a->room, index);
    if (b == nullptr) {
        BaseEdit n;
        n.room = a->room;
        n.index = index;
        n.id = a->id;
        sLayout.edits.push_back(n);
        b = &sLayout.edits.back();
    }
    return *b;
}

AddedActor* AddedFor(Actor* a) {
    for (auto& [uid, live] : sAddedLive) {
        if (live == a) {
            for (auto& d : sLayout.added) {
                if (d.uid == uid) {
                    return &d;
                }
            }
        }
    }
    return nullptr;
}

bool IsLive(PlayState* play, Actor* a) {
    if ((play == nullptr) || (a == nullptr)) {
        return false;
    }
    for (int c = 0; c < ACTORCAT_MAX; c++) {
        for (Actor* x = play->actorCtx.actorLists[c].head; x != nullptr; x = x->next) {
            if (x == a) {
                return true;
            }
        }
    }
    return false;
}

void Select(Actor* a) {
    if (sSel == a) {
        return;
    }
    NameTag_RemoveAllByTag(SEL_TAG);
    sSel = a;
    if (a != nullptr) {
        NameTagOptions o = {};
        o.tag = SEL_TAG;
        o.yOffset = 0;
        o.textColor = { 255, 80, 255, 255 };
        o.noZBuffer = true;
        NameTag_RegisterForActorWithOptions(a, "[ Selected ]", o);
    }
}

Actor* SpawnAdded(PlayState* play, const AddedActor& d) {
    if (!ActorDB::Instance->RetrieveEntry(d.id).entry.valid) {
        return nullptr;
    }
    Actor* a = Actor_Spawn(&play->actorCtx, play, d.id, d.pos.x, d.pos.y, d.pos.z, d.rot.x, d.rot.y, d.rot.z,
                           d.params);
    if (a != nullptr) {
        if (d.room >= 0) {
            a->room = d.room;
        } else {
            a->room = -1;
        }
        sAddedLive[d.uid] = a;
    }
    return a;
}

// A room has spawned its actors: the layout's added actors for it (and, the first time, those for the whole area)
void OnSpawnActors() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        return;
    }
    EnsureLayout(play);
    s16 room = play->roomCtx.curRoom.num;
    for (auto& d : sLayout.added) {
        bool want = (d.room == room) || ((d.room < 0) && !sSceneWideDone);
        if (want && (sAddedLive.find(d.uid) == sAddedLive.end())) {
            SpawnAdded(play, d);
        }
    }
    sSceneWideDone = true;
    sSpawnedRoom = room;
}

// Put the area together again from its layout, where he stands (a fast fade, as after falling into a pit)
void ReloadArea() {
    PlayState* play = gPlayState;
    if ((play == nullptr) || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return;
    }
    if (sFreeze) {
        play->frameAdvCtx.enabled = false;
    }
    sScene = -1; // (read the file again on the way in)
    Play_SetupRespawnPoint(play, RESPAWN_MODE_DOWN, 0xDFF);
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwchFlags = play->actorCtx.flags.tempSwch;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = play->actorCtx.flags.tempCollect;
    gSaveContext.respawnFlag = 1;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->nextEntranceIndex = gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex;
    play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
}

// Remake an actor with new settings in the same place (a change of params only takes when it's made again)
Actor* Respawn(PlayState* play, Actor* a, s16 params) {
    AddedActor* added = AddedFor(a);
    s16 index = GetActorListIndex(a);
    s8 room = a->room;
    Vec3f pos = a->world.pos;
    Vec3s rot = a->shape.rot;
    s16 id = a->id;
    if (added != nullptr) {
        sAddedLive.erase(added->uid);
    }
    a->draw = nullptr;
    Actor_Kill(a);
    Actor* n = Actor_Spawn(&play->actorCtx, play, id, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, params);
    if (n != nullptr) {
        n->room = room;
        if (added != nullptr) {
            sAddedLive[added->uid] = n;
        } else {
            SetActorListIndex(n, index);
        }
    }
    return n;
}

void Frame() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        sSel = nullptr;
        return;
    }
    bool open = CVarGetInteger(CVAR_WINDOW("GdfMapEditor"), 0);
    // Hold the world still while editing, if asked (the game's own frame-advance pause)
    bool wantFreeze = open && sFreeze && (play->transitionTrigger == TRANS_TRIGGER_OFF);
    if (wantFreeze) {
        play->frameAdvCtx.enabled = true;
    } else if (sFreezeWas) {
        play->frameAdvCtx.enabled = false;
    }
    sFreezeWas = wantFreeze;
    if ((sSel != nullptr) && !IsLive(play, sSel)) {
        sSel = nullptr;
    }
    // (a room with no actors of its own never says it spawned them: its added ones are put in here)
    if ((sScene == play->sceneNum) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
        ((play->roomCtx.curRoom.num != sSpawnedRoom) || !sSceneWideDone) && (play->numSetupActors == 0) &&
        !sLayout.added.empty()) {
        OnSpawnActors();
    }
    auto now = std::chrono::steady_clock::now();
    if (sAutoSave && sDirty && (sScene == play->sceneNum) && (now - sLastChange > std::chrono::seconds(1))) {
        SaveLayout();
    }
    // The file, changed outside the game: read again
    if ((sScene == play->sceneNum) && (now - sLastCheck > std::chrono::milliseconds(500))) {
        sLastCheck = now;
        try {
            bool exists = std::filesystem::exists(sPath);
            bool changed = (exists != sFileExists);
            if (exists && !changed) {
                changed = (std::filesystem::last_write_time(sPath) != sFileTime) ||
                          (std::filesystem::file_size(sPath) != sFileSize);
            }
            if (changed) {
                Layout l;
                if (ReadFile(l)) {
                    sLayout = l;
                    NoteFile();
                    sDirty = false;
                    if (sAutoApply && open) {
                        sStatus = "The file changed: area reloaded";
                        ReloadArea();
                    } else {
                        sFileChangedOutside = true;
                        sStatus = "The file changed: reload the area to see it";
                    }
                } else {
                    NoteFile();
                    sStatus = "The layout file has an error; fix it and it's read again";
                }
            }
        } catch (...) {
        }
    }
}

// ---- The window ----

char sFilter[64] = "";
char sAddFilter[64] = "";
s16 sAddId = -1;
u16 sAddParams = 0;
bool sAddWholeArea = false;
int sPieceType = 2; // (a platform)
int sPieceTex = 0;

void ParamsInput(const char* label, u16* v) {
    const u16 step = 1, fast = 0x10;
    ImGui::InputScalar(label, ImGuiDataType_U16, v, &step, &fast, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
}

Vec3f InFront(PlayState* play, f32 dist) {
    Player* p = GET_PLAYER(play);
    Vec3f pos = p->actor.world.pos;
    pos.x += Math_SinS(p->actor.shape.rot.y) * dist;
    pos.z += Math_CosS(p->actor.shape.rot.y) * dist;
    return pos;
}

void DrawSelected(PlayState* play) {
    Actor* a = sSel;
    if (a == nullptr) {
        ImGui::TextDisabled("Click an actor in the list to select it.");
        return;
    }
    AddedActor* added = AddedFor(a);
    s16 index = GetActorListIndex(a);
    bool base = (added == nullptr) && (index >= 0) && (a->room >= 0);
    ImGui::Text("%s", ActorLabel(a->id));
    ImGui::TextDisabled("%s  id 0x%04X  params 0x%04X  room %d  %s", ActorInternalName(a->id).c_str(), (u16)a->id,
                        (u16)a->params, a->room,
                        added ? "(added by the layout)" : (base ? ("#" + std::to_string(index)).c_str() : "(spawned in play)"));
    if (!added && !base) {
        ImGui::TextWrapped("Spawned while playing (by the game or another actor), so the layout can't keep it. "
                           "Moving it works until the area reloads.");
    }

    f32 pos[3] = { a->world.pos.x, a->world.pos.y, a->world.pos.z };
    f32 rot[3] = { ToDeg(a->shape.rot.x), ToDeg(a->shape.rot.y), ToDeg(a->shape.rot.z) };
    bool moved = false;
    ImGui::SetNextItemWidth(-90);
    moved |= ImGui::DragFloat3("Position", pos, 1.0f, -32000.0f, 32000.0f, "%.0f");
    ImGui::SetNextItemWidth(-90);
    moved |= ImGui::DragFloat3("Rotation", rot, 0.5f, -360.0f, 360.0f, "%.1f deg");
    if (ImGui::Button("To Ganondorf")) {
        Vec3f p = InFront(play, 0.0f);
        pos[0] = p.x, pos[1] = p.y, pos[2] = p.z;
        moved = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("In front of him")) {
        Vec3f p = InFront(play, 120.0f);
        pos[0] = p.x, pos[1] = p.y, pos[2] = p.z;
        moved = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Take him there")) {
        Player* p = GET_PLAYER(play);
        p->actor.world.pos = a->world.pos;
        p->actor.world.pos.y += 60.0f;
        p->actor.home.pos = p->actor.world.pos;
    }
    if (moved) {
        a->world.pos = { pos[0], pos[1], pos[2] };
        a->home.pos = a->world.pos;
        a->prevPos = a->world.pos;
        a->shape.rot = { FromDeg(rot[0]), FromDeg(rot[1]), FromDeg(rot[2]) };
        a->world.rot = a->shape.rot;
        a->home.rot = a->shape.rot;
        if (added != nullptr) {
            added->pos = a->world.pos;
            added->rot = a->shape.rot;
            Changed();
        } else if (base) {
            BaseEdit& b = EditFor(a);
            b.moved = true;
            b.pos = a->world.pos;
            b.rot = a->shape.rot;
            Changed();
        }
    }

    static u16 params = 0;
    static Actor* paramsFor = nullptr;
    if (paramsFor != a) {
        params = (u16)a->params;
        paramsFor = a;
    }
    ImGui::SetNextItemWidth(120);
    ParamsInput("Params", &params);
    ImGui::SameLine();
    ImGui::TextDisabled("(hex)");
    ImGui::SameLine();
    if (ImGui::Button("Apply params")) {
        s16 p = (s16)params;
        Actor* n = Respawn(play, a, p);
        if (added != nullptr) {
            added->params = p;
            Changed();
        } else if (base) {
            BaseEdit& b = EditFor(a);
            b.hasParams = true;
            b.params = p;
            b.moved = true;
            b.pos = a->world.pos;
            b.rot = a->shape.rot;
            Changed();
        }
        paramsFor = nullptr;
        Select(n);
        return;
    }

    if (added != nullptr) {
        char note[128];
        snprintf(note, sizeof(note), "%s", added->note.c_str());
        ImGui::SetNextItemWidth(-90);
        if (ImGui::InputText("Note", note, sizeof(note))) {
            added->note = note;
            Changed();
        }
        bool whole = (added->room < 0);
        if (ImGui::Checkbox("Whole area (stays in every room)", &whole)) {
            added->room = whole ? -1 : play->roomCtx.curRoom.num;
            a->room = added->room;
            Changed();
        }
    }

    if (ImGui::Button("Respawn")) {
        Select(Respawn(play, a, a->params));
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        AddedActor d;
        d.uid = sNextUid++;
        d.id = a->id;
        d.params = a->params;
        d.room = (added != nullptr) ? added->room : (s16)play->roomCtx.curRoom.num;
        d.pos = a->world.pos;
        d.pos.x += 60.0f;
        d.rot = a->shape.rot;
        sLayout.added.push_back(d);
        Select(SpawnAdded(play, d));
        Changed();
        return;
    }
    ImGui::SameLine();
    if (added != nullptr) {
        if (ImGui::Button("Delete")) {
            u32 uid = added->uid;
            sAddedLive.erase(uid);
            sLayout.added.erase(std::remove_if(sLayout.added.begin(), sLayout.added.end(),
                                               [&](const AddedActor& d) { return d.uid == uid; }),
                                sLayout.added.end());
            a->draw = nullptr;
            Actor_Kill(a);
            Select(nullptr);
            Changed();
            return;
        }
    } else if (base) {
        if (ImGui::Button("Hide")) {
            EditFor(a).hide = true;
            a->draw = nullptr;
            Actor_Kill(a);
            Select(nullptr);
            Changed();
            return;
        }
        BaseEdit* b = FindEdit(a->room, index);
        if ((b != nullptr) && (b->moved || b->hasParams)) {
            ImGui::SameLine();
            if (ImGui::Button("Back to the game's own")) {
                sLayout.edits.erase(sLayout.edits.begin() + (b - sLayout.edits.data()));
                Changed();
                sStatus = "Reload the area to see it where the game puts it";
            }
        }
    } else {
        if (ImGui::Button("Remove now")) {
            a->draw = nullptr;
            Actor_Kill(a);
            Select(nullptr);
            return;
        }
    }
}

void DrawActorsTab(PlayState* play) {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Filter by name...", sFilter, sizeof(sFilter));
    std::string f = sFilter;
    std::transform(f.begin(), f.end(), f.begin(), ::tolower);
    Player* player = GET_PLAYER(play);

    ImGui::BeginChild("##actors", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.45f), true);
    for (int c = 0; c < ACTORCAT_MAX; c++) {
        for (Actor* a = play->actorCtx.actorLists[c].head; a != nullptr; a = a->next) {
            if (a == &player->actor) {
                continue;
            }
            std::string label = ActorLabel(a->id);
            std::string lower = label + " " + ActorInternalName(a->id);
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!f.empty() && (lower.find(f) == std::string::npos)) {
                continue;
            }
            AddedActor* added = AddedFor(a);
            s16 index = GetActorListIndex(a);
            BaseEdit* b = (added == nullptr) ? FindEdit(a->room, index) : nullptr;
            f32 dist = Math_Vec3f_DistXYZ(&a->world.pos, &player->actor.world.pos);
            char line[256];
            snprintf(line, sizeof(line), "%s%s  [room %d]  %.0f away##%p", added ? "+ " : (b ? "* " : ""),
                     label.c_str(), a->room, dist, (void*)a);
            if (ImGui::Selectable(line, sSel == a)) {
                Select(a);
            }
        }
    }
    ImGui::EndChild();
    ImGui::TextDisabled("+ added by the layout   * changed by the layout");
    ImGui::Separator();
    DrawSelected(play);
}

void DrawAddTab(PlayState* play) {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##addfilter", "Search the game's actors (chest, stalfos, torch...)", sAddFilter,
                             sizeof(sAddFilter));
    std::string f = sAddFilter;
    std::transform(f.begin(), f.end(), f.begin(), ::tolower);
    ImGui::BeginChild("##addlist", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.6f), true);
    for (int i = 0; i < ActorDB::Instance->GetEntryCount(); i++) {
        ActorDB::Entry e = ActorDB::Instance->RetrieveEntry(i);
        if (!e.entry.valid || (i == ACTOR_PLAYER)) {
            continue;
        }
        std::string text = e.desc + " " + e.name;
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (!f.empty() && (lower.find(f) == std::string::npos)) {
            continue;
        }
        char line[256];
        snprintf(line, sizeof(line), "%s  (%s)##add%d", e.desc.c_str(), e.name.c_str(), i);
        if (ImGui::Selectable(line, sAddId == i)) {
            sAddId = (s16)i;
        }
    }
    ImGui::EndChild();
    if (sAddId < 0) {
        ImGui::TextDisabled("Pick an actor above.");
        return;
    }
    ImGui::Text("%s", ActorLabel(sAddId));
    ImGui::SetNextItemWidth(120);
    ParamsInput("Params", &sAddParams);
    ImGui::SameLine();
    ImGui::TextDisabled("(hex)");
    ImGui::Checkbox("Whole area (stays in every room)", &sAddWholeArea);
    if (ImGui::Button("Add in front of Ganondorf")) {
        AddedActor d;
        d.uid = sNextUid++;
        d.id = sAddId;
        d.params = (s16)sAddParams;
        d.room = sAddWholeArea ? -1 : (s16)play->roomCtx.curRoom.num;
        d.pos = InFront(play, 120.0f);
        d.rot = { 0, (s16)(GET_PLAYER(play)->actor.shape.rot.y + 0x8000), 0 };
        sLayout.added.push_back(d);
        Actor* a = SpawnAdded(play, d);
        Changed();
        if (a != nullptr) {
            Select(a);
            sStatus = "Added (select it on the Actors tab to move it)";
        } else {
            sStatus = "It's in the layout, but wouldn't appear here (it may need its own area or settings)";
        }
    }
}

#if GDF_HAS_BUILD
void DrawTerrainTab(PlayState* play) {
    ImGui::TextWrapped("Pieces from Terraform, set by number. Sculpting the ground is done with the Dark Summoning "
                       "menu (L+R, Build page); both are kept in this layout when it's saved.");
    std::vector<GdfBuildPieceInfo> list = GdfBuild_ListHere();
    ImGui::Text("%d of %d pieces in this area", (int)list.size(), GdfBuild_MaxHere());

    ImGui::SetNextItemWidth(140);
    if (ImGui::BeginCombo("##ptype", GdfBuild_KindName(sPieceType))) {
        for (int i = 0; i < GdfBuild_KindCount(); i++) {
            if (ImGui::Selectable(GdfBuild_KindName(i), sPieceType == i)) {
                sPieceType = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170);
    if (ImGui::BeginCombo("##ptex", GdfBuild_TexName(sPieceTex))) {
        for (int i = 0; i < GdfBuild_TexCount(); i++) {
            if (ImGui::Selectable(GdfBuild_TexName(i), sPieceTex == i)) {
                sPieceTex = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add piece")) {
        GdfBuildPieceInfo i;
        static const f32 sizes[][3] = { { 120, 100, 160 }, { 120, 80, 220 }, { 200, 20, 200 }, { 240, 160, 24 },
                                        { 50, 240, 50 },   { 100, 12, 400 }, { 300, 60, 300 } };
        int t = CLAMP(sPieceType, 0, 6);
        i.type = sPieceType;
        i.tex = sPieceTex;
        i.pos = InFront(play, 200.0f);
        i.rotY = GET_PLAYER(play)->actor.shape.rot.y;
        i.w = sizes[t][0], i.h = sizes[t][1], i.l = sizes[t][2];
        if (GdfBuild_AddPiece(i) == 0) {
            sStatus = "No more pieces fit in this area";
        } else {
            Changed();
        }
    }

    ImGui::BeginChild("##pieces", ImVec2(0, 0), true);
    for (auto& p : list) {
        ImGui::PushID((int)p.uid);
        bool open = ImGui::TreeNodeEx("##piece", ImGuiTreeNodeFlags_DefaultOpen, "%s  (%s)", GdfBuild_KindName(p.type),
                                      GdfBuild_TexName(p.tex));
        if (open) {
            GdfBuildPieceInfo n = p;
            bool ch = false;
            f32 pos[3] = { n.pos.x, n.pos.y, n.pos.z };
            f32 size[3] = { n.w, n.h, n.l };
            f32 rotY = ToDeg(n.rotY);
            ImGui::SetNextItemWidth(-110);
            if (ImGui::DragFloat3("Position", pos, 1.0f, -32000.0f, 32000.0f, "%.0f")) {
                n.pos = { pos[0], pos[1], pos[2] };
                ch = true;
            }
            ImGui::SetNextItemWidth(-110);
            if (ImGui::DragFloat3("Width/height/length", size, 1.0f, 2.0f, 4000.0f, "%.0f")) {
                n.w = size[0], n.h = size[1], n.l = size[2];
                ch = true;
            }
            ImGui::SetNextItemWidth(-110);
            if (ImGui::DragFloat("Turn", &rotY, 0.5f, -360.0f, 360.0f, "%.1f deg")) {
                n.rotY = FromDeg(rotY);
                ch = true;
            }
            ImGui::SetNextItemWidth(170);
            if (ImGui::BeginCombo("Texture", GdfBuild_TexName(n.tex))) {
                for (int i = 0; i < GdfBuild_TexCount(); i++) {
                    if (ImGui::Selectable(GdfBuild_TexName(i), n.tex == i)) {
                        n.tex = i;
                        ch = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("To Ganondorf")) {
                n.pos = GET_PLAYER(play)->actor.world.pos;
                ch = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                GdfBuild_RemovePiece(p.uid);
                Changed();
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            if (ch) {
                GdfBuild_SetPiece(n);
                Changed();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

#endif

void DrawLayoutTab(PlayState* play) {
    ImGui::TextWrapped("File: %s", sPath.c_str());
    ImGui::Text("%d added, %d changed", (int)sLayout.added.size(), (int)sLayout.edits.size());
    if (ImGui::Button("Read the file again (undo unsaved)")) {
        sScene = -1;
        EnsureLayout(play);
        ReloadArea();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear this layout")) {
        ImGui::OpenPopup("Clear layout?");
    }
    if (ImGui::BeginPopupModal("Clear layout?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remove everything this layout adds and changes in this area?\nIt takes effect when saved.");
        if (ImGui::Button("Clear")) {
            sLayout = Layout();
#if GDF_HAS_BUILD
            sLayout.hasTerrain = true; // (so the saved file also clears what was built)
            GdfBuild_ImportScene(play->sceneNum, nlohmann::json::object(), true);
#endif
            Changed();
            ImGui::CloseCurrentPopup();
            ReloadArea();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SeparatorText("Hidden actors of the game's own");
    for (size_t i = 0; i < sLayout.edits.size(); i++) {
        BaseEdit& b = sLayout.edits[i];
        if (!b.hide) {
            continue;
        }
        ImGui::PushID((int)i);
        ImGui::Text("%s  (room %d, #%d)", ActorLabel(b.id), b.room, b.index);
        ImGui::SameLine();
        if (ImGui::SmallButton("Show again")) {
            b.hide = false;
            if (!b.moved && !b.hasParams) {
                sLayout.edits.erase(sLayout.edits.begin() + i);
            }
            Changed();
            sStatus = "Reload the area to see it";
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
}

class GdfMapEditorWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override;
};

void GdfMapEditorWindow::DrawElement() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        ImGui::TextDisabled("Load a save and walk into an area to edit it.");
        return;
    }
    EnsureLayout(play);
    ImGui::Text("%s", SohUtils::GetSceneName(play->sceneNum).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("scene %d, setup %d%s, room %d", play->sceneNum, sSetup, sMq ? ", Master Quest" : "",
                        play->roomCtx.curRoom.num);

    if (sDirty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.55f, 1.0f));
    }
    if (ImGui::Button(sDirty ? "Save*" : "Save")) {
        SaveLayout();
    }
    if (sDirty) {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload area")) {
        if (sDirty) {
            SaveLayout();
        }
        ReloadArea();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-save", &sAutoSave);
    ImGui::SameLine();
    ImGui::Checkbox("Freeze world", &sFreeze);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The world holds still while the editor is open (things added come to life when it's off)");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Follow file", &sAutoApply);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When the layout file is changed outside the game, reload the area at once");
    }
    if (!sStatus.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 1.0f, 1.0f), "%s", sStatus.c_str());
    }
    if (sFileChangedOutside) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload now")) {
            ReloadArea();
        }
    }

    if (ImGui::BeginTabBar("##mapedit")) {
        if (ImGui::BeginTabItem("Actors")) {
            DrawActorsTab(play);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Add")) {
            DrawAddTab(play);
            ImGui::EndTabItem();
        }
#if GDF_HAS_BUILD
        if (ImGui::BeginTabItem("Terrain")) {
            DrawTerrainTab(play);
            ImGui::EndTabItem();
        }
#endif
        if (ImGui::BeginTabItem("Layout")) {
            DrawLayoutTab(play);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// The game's own actors, as their room spawns them: hidden, moved or with other params, as the layout says
void OnSpawnEntry(bool* should, va_list args) {
    ActorContext* actorCtx = va_arg(args, ActorContext*);
    ActorEntry* entry = va_arg(args, ActorEntry*);
    PlayState* play = va_arg(args, PlayState*);
    Actor** out = va_arg(args, Actor**);

    if (!*should || (play == nullptr) || (play->sceneNum != sScene) || sLayout.edits.empty() ||
        (play->numSetupActors <= 0) || (entry < play->setupActorList) ||
        (entry >= play->setupActorList + play->numSetupActors)) {
        return;
    }
    BaseEdit* b = FindEdit(play->roomCtx.curRoom.num, (s16)(entry - play->setupActorList));
    if ((b == nullptr) || (b->id != entry->id)) {
        return;
    }
    *should = false;
    if (b->hide) {
        *out = nullptr;
        return;
    }
    Vec3f pos = b->moved ? b->pos : Vec3f{ (f32)entry->pos.x, (f32)entry->pos.y, (f32)entry->pos.z };
    Vec3s rot = b->moved ? b->rot : entry->rot;
    s16 params = b->hasParams ? b->params : entry->params;
    *out = Actor_Spawn(actorCtx, play, entry->id, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, params);
}

// An area has loaded (before its actors spawn): its layout read, nothing of it standing yet
void AreaStart() {
    sScene = -1;
    EnsureLayout(gPlayState);
    sAddedLive.clear();
    sSceneWideDone = false;
    sSpawnedRoom = -1;
}

void RegisterGanondorfMapEditor() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { AreaStart(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(OnSpawnActors);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Frame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>([](void* ref) {
        Actor* a = (Actor*)ref;
        for (auto it = sAddedLive.begin(); it != sAddedLive.end(); ++it) {
            if (it->second == a) {
                sAddedLive.erase(it);
                break;
            }
        }
        if (sSel == a) {
            sSel = nullptr;
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        sAddedLive.clear();
        sSel = nullptr;
    });
    // The game's own actors, as their room spawns them: hidden, moved or with other params, as the layout says
    REGISTER_VB_SHOULD(VB_SPAWN_ACTOR_ENTRY, { OnSpawnEntry(should, args); });
}

static RegisterShipInitFunc initFunc(RegisterGanondorfMapEditor, {});

} // namespace

#if GDF_HAS_BUILD
// An area is loading (before its collision is set up): its layout's build handed to Terraform
void GdfMapEdit_AreaLoading(PlayState* play) {
    if (play == nullptr) {
        return;
    }
    sScene = -1;
    EnsureLayout(play);
    if (sLayout.hasTerrain) {
        try {
            GdfBuild_ImportScene(play->sceneNum, sLayout.terrain, false);
        } catch (...) {
            sStatus = "The layout's terrain has an error; the save's build is used";
        }
    }
}
#endif

std::shared_ptr<Ship::GuiWindow> GdfMapEditor_CreateWindow() {
    return std::make_shared<GdfMapEditorWindow>(CVAR_WINDOW("GdfMapEditor"), "Map Editor", ImVec2(560, 720));
}
