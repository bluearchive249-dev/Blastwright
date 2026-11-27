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
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Enhancements/audio/AudioCollection.h"
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
#include "seqcmd.h"
#include "PlayAsGanondorf.h"
extern PlayState* gPlayState;
}

#include "GanondorfBuild.h" // (Terraform: build pieces, sculpting, and the custom areas' ground)

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

// A trigger zone: walking into it takes him somewhere (and can need, or set, a flag)
enum TriggerDest { DEST_AREA, DEST_ENTRANCE, DEST_SPOT, DEST_BACK, DEST_COUNT };
const char* sDestNames[DEST_COUNT] = { "A custom area", "A game entrance", "A spot in this area",
                                       "Back where he came from" };
enum FlagType { FLAG_SWITCH, FLAG_CHEST, FLAG_CLEAR, FLAG_COLLECT, FLAG_EVENT, FLAG_TYPE_COUNT };
const char* sFlagNames[FLAG_TYPE_COUNT] = { "Switch (this area)", "Chest (this area)", "Room clear (this area)",
                                            "Collectible (this area)", "Event (story)" };
const char* sFlagKeys[FLAG_TYPE_COUNT] = { "switch", "chest", "clear", "collectible", "event" };

struct Trigger {
    u32 uid = 0;
    std::string name = "Trigger";
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    f32 radius = 60.0f, height = 150.0f;
    int dest = DEST_AREA;
    std::string area;           // DEST_AREA
    s32 entrance = 0;           // DEST_ENTRANCE
    Vec3f spot = { 0.0f, 0.0f, 0.0f }; // DEST_SPOT
    s16 spotYaw = 0;
    bool gdfOnly = true;        // only Ganondorf sets it off
    int cond = 0;               // 0 always, 1 only when the flag is set, 2 only when it isn't
    int condType = FLAG_SWITCH, condIndex = 0;
    bool sets = false;          // walking in sets a flag
    int setType = FLAG_SWITCH, setIndex = 0;
};

// A custom area's own settings (only in a custom area's layout)
struct AreaSpec {
    bool has = false;
    GdfPocketGround ground;
    u8 amb[3] = { 80, 80, 80 };
    u8 light[3] = { 255, 255, 240 };
    u8 fog[3] = { 40, 40, 50 };
    s16 fogNear = 996, fogFar = 12800;
    u16 music = 0x7F; // NA_BGM_NO_MUSIC: none
    u8 nature = 0x13; // (none)
    Vec3f spawn = { 0.0f, 0.0f, 0.0f };
    s16 spawnYaw = 0;
};

struct Layout {
    std::vector<AddedActor> added;
    std::vector<BaseEdit> edits;
    std::vector<Trigger> triggers;
    AreaSpec area;
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

int FlagTypeOf(const std::string& k) {
    for (int i = 0; i < FLAG_TYPE_COUNT; i++) {
        if (k == sFlagKeys[i]) {
            return i;
        }
    }
    return FLAG_SWITCH;
}

int TexIndexOf(const std::string& name) {
    for (int i = 0; i < GdfBuild_TexCount(); i++) {
        if (name == GdfBuild_TexName(i)) {
            return i;
        }
    }
    return 0;
}

int SoundIndexOf(const std::string& name) {
    for (int i = 0; i < 14; i++) {
        if (name == GdfBuild_SoundName(i)) {
            return i;
        }
    }
    return 2;
}

void ReadColor(const nlohmann::json& a, const char* key, u8* out) {
    if (a.contains(key) && a[key].is_array() && (a[key].size() == 3)) {
        for (int i = 0; i < 3; i++) {
            if (a[key][i].is_number()) {
                out[i] = (u8)CLAMP(a[key][i].get<int>(), 0, 255);
            }
        }
    }
}

std::string SafeName(const std::string& s) {
    std::string out;
    for (char c : s) {
        out += (std::isalnum((unsigned char)c) ? c : '_');
    }
    return out;
}

// ---- Custom areas: made in the unused SCENE_SASATEST slot (gdf_pocket.o2r), one layout file each ----
#define POCKET_SCENE SCENE_SASATEST
#define POCKET_ENTRANCE ENTR_SASATEST_0
#define CVAR_POCKET "gGdfMapEditor.Area"

bool InPocket(PlayState* play) {
    return (play != nullptr) && (play->sceneNum == POCKET_SCENE);
}

std::string PocketName() {
    const char* n = CVarGetString(CVAR_POCKET, "");
    return ((n != nullptr) && (n[0] != 0)) ? n : "Untitled";
}

std::string PocketPath(const std::string& name) {
    return Ship::Context::GetPathRelativeToAppDirectory("mods/layouts/area_" + SafeName(name) + ".json");
}

std::string LayoutPath(s16 scene, int setup, bool mq) {
    if (scene == POCKET_SCENE) {
        return PocketPath(PocketName());
    }
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
    if (j.contains("triggers") && j["triggers"].is_array()) {
        for (auto& e : j["triggers"]) {
            if (!e.is_object()) {
                continue;
            }
            Trigger t;
            t.uid = sNextUid++;
            t.name = e.value("name", std::string("Trigger"));
            t.pos = ReadVec3f(e.value("pos", nlohmann::json()));
            t.radius = e.value("radius", 60.0f);
            t.height = e.value("height", 150.0f);
            std::string to = e.value("to", std::string("area"));
            t.dest = (to == "entrance") ? DEST_ENTRANCE : (to == "spot") ? DEST_SPOT : (to == "back") ? DEST_BACK : DEST_AREA;
            t.area = e.value("area", std::string());
            t.entrance = ReadParams(e.value("entrance", nlohmann::json(0))) & 0xFFFF;
            t.spot = ReadVec3f(e.value("spot", nlohmann::json()));
            t.spotYaw = FromDeg(e.value("spot_yaw", 0.0f));
            t.gdfOnly = e.value("ganondorf_only", true);
            if (e.contains("needs") && e["needs"].is_object()) {
                auto& n = e["needs"];
                t.cond = n.value("set", true) ? 1 : 2;
                t.condType = FlagTypeOf(n.value("flag", std::string("switch")));
                t.condIndex = n.value("index", 0);
            }
            if (e.contains("sets") && e["sets"].is_object()) {
                t.sets = true;
                t.setType = FlagTypeOf(e["sets"].value("flag", std::string("switch")));
                t.setIndex = e["sets"].value("index", 0);
            }
            out.triggers.push_back(t);
        }
    }
    if (j.contains("area") && j["area"].is_object()) {
        auto& a = j["area"];
        AreaSpec& A = out.area;
        A.has = true;
        Vec3f size = ReadVec3f(nlohmann::json::array({ a.value("width", 2000.0f), a.value("walls", 0.0f),
                                                       a.value("length", 2000.0f) }));
        A.ground.w = size.x;
        A.ground.wallH = size.y;
        A.ground.l = size.z;
        A.ground.floorTex = TexIndexOf(a.value("floor_texture", std::string()));
        A.ground.wallTex = TexIndexOf(a.value("wall_texture", std::string()));
        A.ground.sound = SoundIndexOf(a.value("footsteps", std::string("Stone")));
        ReadColor(a, "ambient", A.amb);
        ReadColor(a, "light", A.light);
        ReadColor(a, "fog", A.fog);
        A.fogNear = (s16)CLAMP(a.value("fog_near", 996), 0, 1000);
        A.fogFar = (s16)CLAMP(a.value("fog_far", 12800), 100, 32000);
        A.music = (u16)(ReadParams(a.value("music", nlohmann::json(0x7F))) & 0xFFFF);
        A.nature = (u8)CLAMP(a.value("nature_sound", 0x13), 0, 0x13);
        A.spawn = ReadVec3f(a.value("spawn", nlohmann::json()));
        A.spawnYaw = FromDeg(a.value("spawn_yaw", 0.0f));
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
    if ((play->sceneNum == sScene) && (setup == sSetup) && (mq == sMq) &&
        (LayoutPath(play->sceneNum, setup, mq) == sPath)) {
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
        if (play->sceneNum == POCKET_SCENE) {
            sLayout.area.has = true; // (a custom area always has its settings, the defaults until changed)
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
    if (!sLayout.triggers.empty()) {
        nlohmann::ordered_json trig = nlohmann::ordered_json::array();
        for (auto& t : sLayout.triggers) {
            nlohmann::ordered_json e;
            static const char* sTo[DEST_COUNT] = { "area", "entrance", "spot", "back" };
            e["name"] = t.name;
            e["pos"] = PosJson(t.pos);
            e["radius"] = lroundf(t.radius);
            e["height"] = lroundf(t.height);
            e["to"] = sTo[CLAMP(t.dest, 0, DEST_COUNT - 1)];
            if (t.dest == DEST_AREA) {
                e["area"] = t.area;
            } else if (t.dest == DEST_ENTRANCE) {
                e["entrance"] = Hex4(t.entrance);
            } else if (t.dest == DEST_SPOT) {
                e["spot"] = PosJson(t.spot);
                e["spot_yaw"] = roundf(ToDeg(t.spotYaw));
            }
            e["ganondorf_only"] = t.gdfOnly;
            if (t.cond != 0) {
                e["needs"] = { { "flag", sFlagKeys[t.condType] }, { "index", t.condIndex }, { "set", t.cond == 1 } };
            }
            if (t.sets) {
                e["sets"] = { { "flag", sFlagKeys[t.setType] }, { "index", t.setIndex } };
            }
            trig.push_back(e);
        }
        j["triggers"] = trig;
    }
    if (sLayout.area.has) {
        const AreaSpec& A = sLayout.area;
        nlohmann::ordered_json a;
        a["width"] = lroundf(A.ground.w);
        a["length"] = lroundf(A.ground.l);
        a["walls"] = lroundf(A.ground.wallH);
        a["floor_texture"] = GdfBuild_TexName(A.ground.floorTex);
        a["wall_texture"] = GdfBuild_TexName(A.ground.wallTex);
        a["footsteps"] = GdfBuild_SoundName(A.ground.sound);
        a["ambient"] = { A.amb[0], A.amb[1], A.amb[2] };
        a["light"] = { A.light[0], A.light[1], A.light[2] };
        a["fog"] = { A.fog[0], A.fog[1], A.fog[2] };
        a["fog_near"] = A.fogNear;
        a["fog_far"] = A.fogFar;
        a["music"] = Hex4(A.music);
        a["nature_sound"] = A.nature;
        a["spawn"] = PosJson(A.spawn);
        a["spawn_yaw"] = roundf(ToDeg(A.spawnYaw));
        j["area"] = a;
    }
    nlohmann::json terrain = GdfBuild_ExportHere();
    if (sLayout.hasTerrain || !terrain["pieces"].empty() || !terrain["ground"].empty()) {
        j["terrain"] = nlohmann::ordered_json::parse(terrain.dump());
        sLayout.terrain = terrain;
        sLayout.hasTerrain = true;
    }
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
        n.room = (s16)a->room;
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
        a->room = (s8)((d.room >= 0) ? d.room : -1);
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

// ---- Going to a custom area and back, and the game's own entrances ----

RespawnData sReturn = {};
bool sHasReturn = false;
int sMusicDelay = -1; // frames until a custom area's music starts (the game starts its own first)

void StartTransition(PlayState* play, s16 entrance, bool respawn) {
    if (sFreeze) {
        play->frameAdvCtx.enabled = false;
    }
    gSaveContext.respawnFlag = respawn ? 1 : 0;
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
}

void NoteReturn(PlayState* play) {
    if (InPocket(play) && sHasReturn) {
        return; // (from one custom area to the next, the way back stays the way out of the first)
    }
    Player* p = GET_PLAYER(play);
    sReturn = {};
    sReturn.pos = p->actor.world.pos;
    sReturn.yaw = p->actor.shape.rot.y;
    sReturn.playerParams = 0x0DFF;
    sReturn.entranceIndex = (s16)gSaveContext.entranceIndex;
    sReturn.roomIndex = (u8)play->roomCtx.curRoom.num;
    sReturn.tempSwchFlags = play->actorCtx.flags.tempSwch;
    sReturn.tempCollectFlags = play->actorCtx.flags.tempCollect;
    sHasReturn = true;
}

void GoToPocket(PlayState* play, const std::string& name) {
    if ((play == nullptr) || (play->transitionTrigger != TRANS_TRIGGER_OFF) || name.empty()) {
        return;
    }
    NoteReturn(play);
    CVarSetString(CVAR_POCKET, name.c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    StartTransition(play, POCKET_ENTRANCE, false);
}

void GoBack(PlayState* play) {
    if ((play == nullptr) || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return;
    }
    if (!sHasReturn) {
        sStatus = "Nowhere to go back to (he didn't come in through a trigger or the editor)";
        return;
    }
    gSaveContext.respawn[RESPAWN_MODE_DOWN] = sReturn;
    sHasReturn = false;
    StartTransition(play, sReturn.entranceIndex, true);
}

void GoToEntrance(PlayState* play, s32 entrance) {
    if ((play == nullptr) || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return;
    }
    NoteReturn(play);
    StartTransition(play, (s16)entrance, false);
}

// ---- Flags ----

int FlagCount(int type) {
    switch (type) {
        case FLAG_SWITCH:
            return 64;
        case FLAG_EVENT:
            return 0xE0;
        default:
            return 32;
    }
}

bool FlagGet(PlayState* play, int type, int i) {
    i = CLAMP(i, 0, FlagCount(type) - 1);
    switch (type) {
        case FLAG_SWITCH:
            return Flags_GetSwitch(play, i) != 0;
        case FLAG_CHEST:
            return Flags_GetTreasure(play, i) != 0;
        case FLAG_CLEAR:
            return Flags_GetClear(play, i) != 0;
        case FLAG_COLLECT:
            return Flags_GetCollectible(play, i) != 0;
        default:
            return Flags_GetEventChkInf(i) != 0;
    }
}

void FlagSet(PlayState* play, int type, int i, bool on) {
    i = CLAMP(i, 0, FlagCount(type) - 1);
    switch (type) {
        case FLAG_SWITCH:
            on ? Flags_SetSwitch(play, i) : Flags_UnsetSwitch(play, i);
            break;
        case FLAG_CHEST:
            if (on) {
                Flags_SetTreasure(play, i);
            } else {
                play->actorCtx.flags.chest &= ~(1u << i);
            }
            break;
        case FLAG_CLEAR:
            on ? Flags_SetClear(play, i) : Flags_UnsetClear(play, i);
            break;
        case FLAG_COLLECT:
            if (on) {
                Flags_SetCollectible(play, i);
            } else {
                play->actorCtx.flags.collect &= ~(1u << i);
            }
            break;
        default:
            on ? Flags_SetEventChkInf(i) : Flags_UnsetEventChkInf(i);
            break;
    }
}

// ---- Trigger zones ----

std::map<u32, int> sTrigInside; // 1 inside, 0 outside; a trigger only goes off when he walks in, not when he starts in it
bool sTriggersLive = true;
u32 sSelTrig = 0;

bool InsideTrigger(const Trigger& t, const Vec3f& p) {
    f32 dx = p.x - t.pos.x, dz = p.z - t.pos.z;
    return (dx * dx + dz * dz <= t.radius * t.radius) && (p.y >= t.pos.y - 30.0f) && (p.y <= t.pos.y + t.height);
}

void FireTrigger(PlayState* play, const Trigger& t) {
    if (t.sets) {
        FlagSet(play, t.setType, t.setIndex, true);
    }
    switch (t.dest) {
        case DEST_AREA:
            GoToPocket(play, t.area);
            break;
        case DEST_ENTRANCE:
            GoToEntrance(play, t.entrance);
            break;
        case DEST_SPOT: {
            Player* p = GET_PLAYER(play);
            p->actor.world.pos = t.spot;
            p->actor.home.pos = t.spot;
            p->actor.prevPos = t.spot;
            p->actor.shape.rot.y = p->actor.world.rot.y = t.spotYaw;
            p->yaw = t.spotYaw;
            sTrigInside.clear(); // (wherever he lands, he has to walk into a zone to set it off)
            break;
        }
        case DEST_BACK:
            GoBack(play);
            break;
    }
}

void TriggersFrame(PlayState* play) {
    if (!sTriggersLive || (sScene != play->sceneNum) || (play->transitionTrigger != TRANS_TRIGGER_OFF) ||
        (play->msgCtx.msgMode != MSGMODE_NONE) || (play->csCtx.state != CS_STATE_IDLE) || play->frameAdvCtx.enabled) {
        return;
    }
    Player* p = GET_PLAYER(play);
    bool gdf = PlayAsGanondorf_IsDorfOut(play) || PlayAsGanondorf_IsGanonOut(play);
    for (auto& t : sLayout.triggers) {
        int now = InsideTrigger(t, p->actor.world.pos) ? 1 : 0;
        auto it = sTrigInside.find(t.uid);
        int before = (it == sTrigInside.end()) ? now : it->second;
        sTrigInside[t.uid] = now;
        if (!now || before) {
            continue;
        }
        if (t.gdfOnly && !gdf) {
            continue;
        }
        if ((t.cond != 0) && (FlagGet(play, t.condType, t.condIndex) != (t.cond == 1))) {
            continue;
        }
        FireTrigger(play, t);
        return; // (one at a time)
    }
}

// ---- A custom area's light, fog and music ----

// The blank area has POCKET_LIGHTS light settings (gdf_pocket.o2r). The game doesn't set its count of them for areas
// loaded from files, so the count left from the area before is set right here (it was written past the list's end)
#define POCKET_LIGHTS 4

void ApplyAreaLook(PlayState* play) {
    if (!InPocket(play) || (play->envCtx.lightSettingsList == nullptr)) {
        return;
    }
    play->envCtx.numLightSettings = POCKET_LIGHTS;
    const AreaSpec& A = sLayout.area;
    for (int k = 0; k < POCKET_LIGHTS; k++) {
        EnvLightSettings* l = &play->envCtx.lightSettingsList[k];
        for (int i = 0; i < 3; i++) {
            l->ambientColor[i] = A.amb[i];
            l->light1Color[i] = A.light[i];
            l->light2Color[i] = (u8)(A.light[i] / 3);
            l->fogColor[i] = A.fog[i];
        }
        l->fogNear = (s16)((l->fogNear & 0xFC00) | CLAMP(A.fogNear, 0, 1000));
        l->fogFar = A.fogFar;
    }
}

void StartAreaMusic(PlayState* play) {
    const AreaSpec& A = sLayout.area;
    if (A.music == 0x7F) {
        SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0);
    } else {
        SEQCMD_PLAY_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0, 0, A.music);
    }
    if (A.nature < 0x13) {
        Audio_PlayNatureAmbienceSequence(A.nature);
    }
}

// ---- The editor's marks in the world: the trigger zones (shown while the editor is open) ----

s32 sOverlayId = -1;

void Overlay_Draw(Actor* thisx, PlayState* play) {
    if (!CVarGetInteger(CVAR_WINDOW("GdfMapEditor"), 0) || (sScene != play->sceneNum)) {
        return;
    }
    for (auto& t : sLayout.triggers) {
        GdfBuild_DrawRing(play, t.pos, t.radius, 0, t.uid == sSelTrig);
    }
}

void Overlay_Nothing(Actor* thisx, PlayState* play) {
}

void SpawnOverlay(PlayState* play) {
    if (sOverlayId < 0) {
        ActorDBInit init;
        init.name = "Gdf_MapEditorMarks";
        init.desc = "Map Editor: trigger zones";
        init.category = ACTORCAT_BG;
        init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = sizeof(Actor);
        init.init = Overlay_Nothing;
        init.destroy = Overlay_Nothing;
        init.update = Overlay_Nothing;
        init.draw = Overlay_Draw;
        sOverlayId = ActorDB::Instance->AddEntry(init).entry.id;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != nullptr; a = a->next) {
        if (a->id == sOverlayId) {
            return;
        }
    }
    Actor* a = Actor_Spawn(&play->actorCtx, play, (s16)sOverlayId, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
    if (a != nullptr) {
        a->room = -1;
    }
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
    if ((sScene == play->sceneNum) && (play->transitionTrigger == TRANS_TRIGGER_OFF)) {
        SpawnOverlay(play);
        if (InPocket(play)) {
            GdfBuild_SpawnPocketGround(play);
            if (sMusicDelay > 0) {
                sMusicDelay--;
            } else if (sMusicDelay == 0) {
                sMusicDelay = -1;
                StartAreaMusic(play);
            }
        }
    }
    TriggersFrame(play);
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
            added->room = (s16)(whole ? -1 : play->roomCtx.curRoom.num);
            a->room = (s8)added->room;
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
        d.room = (s16)(sAddWholeArea ? -1 : play->roomCtx.curRoom.num);
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

// Keep an actor's place (as it stands now) in the layout
void RecordActorPlace(Actor* a) {
    AddedActor* added = AddedFor(a);
    s16 index = GetActorListIndex(a);
    if (added != nullptr) {
        added->pos = a->world.pos;
        added->rot = a->shape.rot;
        Changed();
    } else if ((index >= 0) && (a->room >= 0)) {
        BaseEdit& b = EditFor(a);
        b.moved = true;
        b.pos = a->world.pos;
        b.rot = a->shape.rot;
        Changed();
    }
}

void PlaceActor(Actor* a, Vec3f pos) {
    a->world.pos = pos;
    a->home.pos = pos;
    a->prevPos = pos;
}

// The floor under a point (from high above), or his height where there's none
f32 FloorAt(PlayState* play, f32 x, f32 z, f32 fromY) {
    CollisionPoly* poly = nullptr;
    Vec3f p = { x, fromY + 400.0f, z };
    f32 y = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &p);
    return (y > BGCHECK_Y_MIN + 1.0f) ? y : GET_PLAYER(play)->actor.world.pos.y;
}

Trigger* FindTrigger(u32 uid) {
    for (auto& t : sLayout.triggers) {
        if (t.uid == uid) {
            return &t;
        }
    }
    return nullptr;
}

Trigger* AddTrigger(PlayState* play, Vec3f pos) {
    Trigger t;
    t.uid = sNextUid++;
    t.name = "Trigger " + std::to_string(sLayout.triggers.size() + 1);
    t.pos = pos;
    t.dest = InPocket(play) ? DEST_BACK : DEST_AREA;
    t.spot = pos;
    t.entrance = gSaveContext.entranceIndex;
    sLayout.triggers.push_back(t);
    sTrigInside[t.uid] = InsideTrigger(t, GET_PLAYER(play)->actor.world.pos) ? 1 : 0; // (not set off by placing it on him)
    sSelTrig = t.uid;
    Changed();
    return &sLayout.triggers.back();
}

// ---- The Map tab: the area from above ----

struct MapView {
    f32 cx = 0.0f, cz = 0.0f;
    f32 scale = 0.12f; // screen pixels per world unit
    bool follow = true;
    f32 band = 500.0f;  // how far above and below his height the ground is drawn
    bool showActors = true, showPieces = true, showTriggers = true;
};
MapView sMap;
enum DragKind { DRAG_NONE, DRAG_PAN, DRAG_ACTOR, DRAG_PIECE, DRAG_TRIGGER };
DragKind sDrag = DRAG_NONE;
u32 sSelPiece = 0;
Vec3f sMenuAt = { 0.0f, 0.0f, 0.0f };

void DrawMapTab(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f pp = player->actor.world.pos;
    if (sMap.follow && (sDrag == DRAG_NONE)) {
        sMap.cx = pp.x;
        sMap.cz = pp.z;
    }
    ImGui::Checkbox("Follow him", &sMap.follow);
    ImGui::SameLine();
    ImGui::Checkbox("Actors", &sMap.showActors);
    ImGui::SameLine();
    ImGui::Checkbox("Pieces", &sMap.showPieces);
    ImGui::SameLine();
    ImGui::Checkbox("Triggers", &sMap.showTriggers);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::DragFloat("Height band", &sMap.band, 10.0f, 100.0f, 5000.0f, "%.0f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Ground this far above or below him is drawn (for areas with floors over floors)");
    }

    // The selection's own controls under the map: its height (the map moves it across, not up and down)
    f32 below = 64.0f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = MAX(size.y - below, 200.0f);
    ImGui::InvisibleButton("##map", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
    dl->AddRectFilled(p0, p1, IM_COL32(14, 10, 20, 255));
    dl->PushClipRect(p0, p1, true);
    ImVec2 mid = ImVec2(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
    auto toScreen = [&](f32 x, f32 z) { return ImVec2(mid.x + (x - sMap.cx) * sMap.scale, mid.y + (z - sMap.cz) * sMap.scale); };
    auto toWorldX = [&](f32 sx) { return sMap.cx + (sx - mid.x) / sMap.scale; };
    auto toWorldZ = [&](f32 sy) { return sMap.cz + (sy - mid.y) / sMap.scale; };

    // Zoom about the mouse
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && (io.MouseWheel != 0.0f)) {
        f32 wx = toWorldX(io.MousePos.x), wz = toWorldZ(io.MousePos.y);
        sMap.scale = CLAMP(sMap.scale * powf(1.2f, io.MouseWheel), 0.004f, 3.0f);
        sMap.cx = wx - (io.MousePos.x - mid.x) / sMap.scale;
        sMap.cz = wz - (io.MousePos.y - mid.y) / sMap.scale;
        sMap.follow = false;
    }

    // The ground: floors filled (lighter higher up), walls as lines, water in blue
    CollisionHeader* h = play->colCtx.colHeader;
    if ((h != nullptr) && (h->polyList != nullptr) && (h->vtxList != nullptr)) {
        for (int i = 0; i < h->numPolygons; i++) {
            const CollisionPoly& poly = h->polyList[i];
            const Vec3s& a = h->vtxList[COLPOLY_VTX_INDEX(poly.flags_vIA)];
            const Vec3s& b = h->vtxList[COLPOLY_VTX_INDEX(poly.flags_vIB)];
            const Vec3s& c = h->vtxList[poly.vIC];
            f32 y = ((f32)a.y + (f32)b.y + (f32)c.y) / 3.0f;
            if (fabsf(y - pp.y) > sMap.band) {
                continue;
            }
            ImVec2 A = toScreen(a.x, a.z), B = toScreen(b.x, b.z), C = toScreen(c.x, c.z);
            f32 minx = MIN(A.x, MIN(B.x, C.x)), maxx = MAX(A.x, MAX(B.x, C.x));
            f32 miny = MIN(A.y, MIN(B.y, C.y)), maxy = MAX(A.y, MAX(B.y, C.y));
            if ((maxx < p0.x) || (minx > p1.x) || (maxy < p0.y) || (miny > p1.y)) {
                continue;
            }
            f32 ny = poly.normal.y / 32767.0f;
            f32 t = CLAMP(0.5f + (y - pp.y) / (2.0f * sMap.band), 0.0f, 1.0f);
            if (ny > 0.5f) {
                int g = (int)(40 + 120 * t);
                dl->AddTriangleFilled(A, B, C, IM_COL32(g, g - 10 > 0 ? g - 10 : 0, g + 20, 255));
            } else if (ny > -0.5f) {
                dl->AddTriangle(A, B, C, IM_COL32(150, 110, 80, 200), 1.0f);
            }
        }
        for (int i = 0; i < h->numWaterBoxes; i++) {
            const WaterBox& w = h->waterBoxes[i];
            if (fabsf(w.ySurface - pp.y) > sMap.band) {
                continue;
            }
            dl->AddRectFilled(toScreen((f32)w.xMin, (f32)w.zMin),
                              toScreen((f32)(w.xMin + w.xLength), (f32)(w.zMin + w.zLength)),
                              IM_COL32(40, 90, 200, 90));
        }
    }

    // What's on it
    struct Hit {
        DragKind kind;
        Actor* actor;
        u32 uid;
        f32 d2;
    };
    Hit best = { DRAG_NONE, nullptr, 0, 1e30f };
    ImVec2 m = io.MousePos;
    auto consider = [&](DragKind k, Actor* a, u32 uid, ImVec2 at, f32 reach) {
        f32 d2 = (at.x - m.x) * (at.x - m.x) + (at.y - m.y) * (at.y - m.y);
        if ((d2 <= reach * reach) && (d2 < best.d2)) {
            best = { k, a, uid, d2 };
        }
    };
    if (sMap.showPieces) {
        for (auto& pc : GdfBuild_ListHere()) {
            f32 cs = Math_CosS(pc.rotY), sn = Math_SinS(pc.rotY);
            ImVec2 q[4];
            const f32 cx[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, cz[4] = { -0.5f, -0.5f, 0.5f, 0.5f };
            for (int k = 0; k < 4; k++) {
                f32 lx = cx[k] * pc.w, lz = cz[k] * pc.l;
                q[k] = toScreen(pc.pos.x + lx * cs + lz * sn, pc.pos.z - lx * sn + lz * cs);
            }
            ImU32 col = (pc.uid == sSelPiece) ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 160, 60, 230);
            dl->AddQuad(q[0], q[1], q[2], q[3], col, (pc.uid == sSelPiece) ? 2.5f : 1.5f);
            consider(DRAG_PIECE, nullptr, pc.uid, toScreen(pc.pos.x, pc.pos.z), MAX(10.0f, 0.4f * MIN(pc.w, pc.l) * sMap.scale));
        }
    }
    if (sMap.showTriggers) {
        for (auto& t : sLayout.triggers) {
            ImVec2 c = toScreen(t.pos.x, t.pos.z);
            bool sel = (t.uid == sSelTrig);
            dl->AddCircle(c, MAX(t.radius * sMap.scale, 4.0f), sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(80, 220, 255, 230), 32,
                          sel ? 2.5f : 1.5f);
            dl->AddText(ImVec2(c.x + 6, c.y - 6), IM_COL32(150, 235, 255, 255), t.name.c_str());
            consider(DRAG_TRIGGER, nullptr, t.uid, c, MAX(10.0f, t.radius * sMap.scale));
        }
    }
    if (sMap.showActors) {
        for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
            for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
                if ((a == &player->actor) || (a->id == sOverlayId) || (fabsf(a->world.pos.y - pp.y) > sMap.band)) {
                    continue;
                }
                ImVec2 c = toScreen(a->world.pos.x, a->world.pos.z);
                bool added = (AddedFor(a) != nullptr);
                bool changed = !added && (FindEdit(a->room, GetActorListIndex(a)) != nullptr);
                ImU32 col = added ? IM_COL32(255, 90, 255, 255)
                            : changed ? IM_COL32(255, 230, 80, 255)
                            : (cat == ACTORCAT_ENEMY || cat == ACTORCAT_BOSS) ? IM_COL32(255, 80, 80, 255)
                            : IM_COL32(200, 200, 210, 255);
                dl->AddCircleFilled(c, 3.5f, col);
                if (a == sSel) {
                    dl->AddCircle(c, 8.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
                }
                consider(DRAG_ACTOR, a, 0, c, 8.0f);
            }
        }
    }
    // Him: an arrow the way he faces
    {
        ImVec2 c = toScreen(pp.x, pp.z);
        f32 sy = Math_SinS(player->actor.shape.rot.y), cy = Math_CosS(player->actor.shape.rot.y);
        ImVec2 tip = ImVec2(c.x + sy * 11.0f, c.y + cy * 11.0f);
        ImVec2 l = ImVec2(c.x - sy * 5.0f + cy * 6.0f, c.y - cy * 5.0f - sy * 6.0f);
        ImVec2 r = ImVec2(c.x - sy * 5.0f - cy * 6.0f, c.y - cy * 5.0f + sy * 6.0f);
        dl->AddTriangleFilled(tip, l, r, IM_COL32(90, 255, 120, 255));
    }
    dl->AddText(ImVec2(p0.x + 6, p0.y + 4), IM_COL32(180, 170, 200, 255), "North is up. Wheel: zoom. Drag: move / pan. Right-click: add.");
    dl->PopClipRect();

    // Clicking and dragging
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        sDrag = (best.kind == DRAG_NONE) ? DRAG_PAN : best.kind;
        if (best.kind == DRAG_ACTOR) {
            Select(best.actor);
            sSelPiece = sSelTrig = 0;
        } else if (best.kind == DRAG_PIECE) {
            sSelPiece = best.uid;
            sSelTrig = 0;
            Select(nullptr);
        } else if (best.kind == DRAG_TRIGGER) {
            sSelTrig = best.uid;
            sSelPiece = 0;
            Select(nullptr);
        }
    }
    if ((sDrag != DRAG_NONE) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        f32 wx = toWorldX(m.x), wz = toWorldZ(m.y);
        if (sDrag == DRAG_PAN) {
            sMap.cx -= io.MouseDelta.x / sMap.scale;
            sMap.cz -= io.MouseDelta.y / sMap.scale;
            sMap.follow = false;
        } else if ((sDrag == DRAG_ACTOR) && (sSel != nullptr)) {
            PlaceActor(sSel, { wx, sSel->world.pos.y, wz });
        } else if (sDrag == DRAG_TRIGGER) {
            if (Trigger* t = FindTrigger(sSelTrig)) {
                t->pos.x = wx;
                t->pos.z = wz;
            }
        } else if (sDrag == DRAG_PIECE) {
            for (auto pc : GdfBuild_ListHere()) {
                if (pc.uid == sSelPiece) {
                    pc.pos.x = wx;
                    pc.pos.z = wz;
                    GdfBuild_SetPiece(pc);
                }
            }
        }
    }
    if ((sDrag != DRAG_NONE) && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if ((sDrag == DRAG_ACTOR) && (sSel != nullptr) && (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x != 0.0f ||
                                                          ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y != 0.0f)) {
            RecordActorPlace(sSel);
        } else if ((sDrag == DRAG_TRIGGER) || (sDrag == DRAG_PIECE)) {
            Changed();
        }
        sDrag = DRAG_NONE;
    }

    // Right-click: add something here
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        f32 wx = toWorldX(m.x), wz = toWorldZ(m.y);
        sMenuAt = { wx, FloorAt(play, wx, wz, pp.y), wz };
        ImGui::OpenPopup("##mapmenu");
    }
    if (ImGui::BeginPopup("##mapmenu")) {
        ImGui::TextDisabled("At %.0f, %.0f, %.0f", sMenuAt.x, sMenuAt.y, sMenuAt.z);
        if ((sAddId >= 0) && ImGui::MenuItem((std::string("Add ") + ActorLabel(sAddId) + " here").c_str())) {
            AddedActor d;
            d.uid = sNextUid++;
            d.id = sAddId;
            d.params = (s16)sAddParams;
            d.room = (s16)(sAddWholeArea ? -1 : play->roomCtx.curRoom.num);
            d.pos = sMenuAt;
            sLayout.added.push_back(d);
            Select(SpawnAdded(play, d));
            Changed();
        }
        if (sAddId < 0) {
            ImGui::TextDisabled("(pick an actor on the Add tab to add it here)");
        }
        if (ImGui::MenuItem((std::string("Add a ") + GdfBuild_KindName(sPieceType) + " here").c_str())) {
            static const f32 sizes[][3] = { { 120, 100, 160 }, { 120, 80, 220 }, { 200, 20, 200 }, { 240, 160, 24 },
                                            { 50, 240, 50 },   { 100, 12, 400 }, { 300, 60, 300 } };
            int k = CLAMP(sPieceType, 0, 6);
            GdfBuildPieceInfo i;
            i.type = sPieceType;
            i.tex = sPieceTex;
            i.pos = sMenuAt;
            i.w = sizes[k][0], i.h = sizes[k][1], i.l = sizes[k][2];
            u32 uid = GdfBuild_AddPiece(i);
            if (uid == 0) {
                sStatus = "No more pieces fit in this area";
            } else {
                sSelPiece = uid;
                Changed();
            }
        }
        if (ImGui::MenuItem("Add a trigger here")) {
            AddTrigger(play, sMenuAt);
        }
        if (ImGui::MenuItem("Take him here")) {
            player->actor.world.pos = sMenuAt;
            player->actor.world.pos.y += 20.0f;
            player->actor.home.pos = player->actor.world.pos;
        }
        if (InPocket(play) && ImGui::MenuItem("Make this the area's start point")) {
            sLayout.area.spawn = sMenuAt;
            Changed();
        }
        ImGui::EndPopup();
    }

    // The selection's height
    if (sSel != nullptr) {
        f32 y = sSel->world.pos.y;
        ImGui::SetNextItemWidth(160);
        if (ImGui::DragFloat("Height##selactor", &y, 1.0f, -32000.0f, 32000.0f, "%.0f")) {
            PlaceActor(sSel, { sSel->world.pos.x, y, sSel->world.pos.z });
            RecordActorPlace(sSel);
        }
        ImGui::SameLine();
        ImGui::Text("%s (more on the Actors tab)", ActorLabel(sSel->id));
    } else if (Trigger* t = FindTrigger(sSelTrig)) {
        ImGui::SetNextItemWidth(160);
        if (ImGui::DragFloat("Height##seltrig", &t->pos.y, 1.0f, -32000.0f, 32000.0f, "%.0f")) {
            Changed();
        }
        ImGui::SameLine();
        ImGui::Text("%s (more on the Triggers tab)", t->name.c_str());
    } else if (sSelPiece != 0) {
        for (auto pc : GdfBuild_ListHere()) {
            if (pc.uid == sSelPiece) {
                ImGui::SetNextItemWidth(160);
                if (ImGui::DragFloat("Height##selpiece", &pc.pos.y, 1.0f, -32000.0f, 32000.0f, "%.0f")) {
                    GdfBuild_SetPiece(pc);
                    Changed();
                }
                ImGui::SameLine();
                ImGui::Text("%s (more on the Terrain tab)", GdfBuild_KindName(pc.type));
            }
        }
    } else {
        ImGui::TextDisabled("Click something on the map to select it.");
    }
}

// ---- The Triggers tab ----

void FlagPicker(const char* id, int* type, int* index) {
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(190);
    if (ImGui::BeginCombo("##type", sFlagNames[CLAMP(*type, 0, FLAG_TYPE_COUNT - 1)])) {
        for (int i = 0; i < FLAG_TYPE_COUNT; i++) {
            if (ImGui::Selectable(sFlagNames[i], *type == i)) {
                *type = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    const s32 one = 1, eight = 8;
    ImGui::InputScalar("##index", ImGuiDataType_S32, index, &one, &eight, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    *index = CLAMP(*index, 0, FlagCount(*type) - 1);
    ImGui::PopID();
}

std::vector<std::string> ListPockets() {
    std::vector<std::string> out;
    try {
        std::string dir = Ship::Context::GetPathRelativeToAppDirectory("mods/layouts");
        if (std::filesystem::exists(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                std::string n = e.path().filename().string();
                if ((n.rfind("area_", 0) == 0) && (n.size() > 10) && (n.substr(n.size() - 5) == ".json")) {
                    out.push_back(n.substr(5, n.size() - 10));
                }
            }
        }
    } catch (...) {
    }
    std::sort(out.begin(), out.end());
    return out;
}

void DrawTriggersTab(PlayState* play) {
    ImGui::Checkbox("Triggers work (off: walk through them while building)", &sTriggersLive);
    if (ImGui::Button("Add a trigger where he stands")) {
        AddTrigger(play, GET_PLAYER(play)->actor.world.pos);
    }
    ImGui::TextDisabled("Zones show as glowing rings while this window is open. A zone goes off when he walks into it.");
    std::vector<std::string> areas = ListPockets();
    ImGui::BeginChild("##trigs", ImVec2(0, 0), true);
    for (size_t n = 0; n < sLayout.triggers.size(); n++) {
        Trigger& t = sLayout.triggers[n];
        ImGui::PushID((int)t.uid);
        bool open = ImGui::TreeNodeEx("##t", (t.uid == sSelTrig) ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s  -> %s",
                                      t.name.c_str(), sDestNames[t.dest]);
        if (ImGui::IsItemClicked()) {
            sSelTrig = t.uid;
        }
        if (open) {
            bool ch = false;
            char name[64];
            snprintf(name, sizeof(name), "%s", t.name.c_str());
            ImGui::SetNextItemWidth(220);
            if (ImGui::InputText("Name", name, sizeof(name))) {
                t.name = name;
                ch = true;
            }
            f32 pos[3] = { t.pos.x, t.pos.y, t.pos.z };
            ImGui::SetNextItemWidth(-110);
            if (ImGui::DragFloat3("Position", pos, 1.0f, -32000.0f, 32000.0f, "%.0f")) {
                t.pos = { pos[0], pos[1], pos[2] };
                ch = true;
            }
            ImGui::SetNextItemWidth(120);
            ch |= ImGui::DragFloat("Radius", &t.radius, 1.0f, 10.0f, 3000.0f, "%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ch |= ImGui::DragFloat("Height", &t.height, 1.0f, 10.0f, 3000.0f, "%.0f");
            if (ImGui::Button("To him")) {
                t.pos = GET_PLAYER(play)->actor.world.pos;
                sTrigInside[t.uid] = 1;
                ch = true;
            }
            ImGui::SetNextItemWidth(220);
            if (ImGui::BeginCombo("Leads to", sDestNames[t.dest])) {
                for (int i = 0; i < DEST_COUNT; i++) {
                    if (ImGui::Selectable(sDestNames[i], t.dest == i)) {
                        t.dest = i;
                        ch = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (t.dest == DEST_AREA) {
                ImGui::SetNextItemWidth(220);
                if (ImGui::BeginCombo("Area", t.area.empty() ? "(choose)" : t.area.c_str())) {
                    for (auto& a : areas) {
                        if (ImGui::Selectable(a.c_str(), t.area == a)) {
                            t.area = a;
                            ch = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(make new ones on the Areas tab)");
            } else if (t.dest == DEST_ENTRANCE) {
                u16 e = (u16)t.entrance;
                ImGui::SetNextItemWidth(120);
                ParamsInput("Entrance", &e);
                if (e != (u16)t.entrance) {
                    t.entrance = e;
                    ch = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("The one he came in by")) {
                    t.entrance = gSaveContext.entranceIndex;
                    ch = true;
                }
            } else if (t.dest == DEST_SPOT) {
                f32 sp[3] = { t.spot.x, t.spot.y, t.spot.z };
                ImGui::SetNextItemWidth(-110);
                if (ImGui::DragFloat3("Spot", sp, 1.0f, -32000.0f, 32000.0f, "%.0f")) {
                    t.spot = { sp[0], sp[1], sp[2] };
                    ch = true;
                }
                if (ImGui::SmallButton("Where he stands")) {
                    t.spot = GET_PLAYER(play)->actor.world.pos;
                    t.spotYaw = GET_PLAYER(play)->actor.shape.rot.y;
                    ch = true;
                }
            }
            ch |= ImGui::Checkbox("Only Ganondorf sets it off", &t.gdfOnly);
            static const char* sConds[3] = { "Always", "Only when this flag is set", "Only when this flag isn't set" };
            ImGui::SetNextItemWidth(220);
            if (ImGui::BeginCombo("When", sConds[CLAMP(t.cond, 0, 2)])) {
                for (int i = 0; i < 3; i++) {
                    if (ImGui::Selectable(sConds[i], t.cond == i)) {
                        t.cond = i;
                        ch = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (t.cond != 0) {
                int ty = t.condType, ix = t.condIndex;
                FlagPicker("cond", &ty, &ix);
                if ((ty != t.condType) || (ix != t.condIndex)) {
                    t.condType = ty, t.condIndex = ix;
                    ch = true;
                }
            }
            ch |= ImGui::Checkbox("Walking in sets a flag", &t.sets);
            if (t.sets) {
                int ty = t.setType, ix = t.setIndex;
                FlagPicker("set", &ty, &ix);
                if ((ty != t.setType) || (ix != t.setIndex)) {
                    t.setType = ty, t.setIndex = ix;
                    ch = true;
                }
            }
            if (ImGui::Button("Try it now")) {
                FireTrigger(play, t);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                if (sSelTrig == t.uid) {
                    sSelTrig = 0;
                }
                sLayout.triggers.erase(sLayout.triggers.begin() + n);
                Changed();
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            if (ch) {
                sSelTrig = t.uid;
                Changed();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ---- The Areas tab: custom areas, and this one's ground, light and sound ----

char sNewArea[48] = "";

void DrawAreasTab(PlayState* play) {
    ImGui::SeparatorText("Custom areas");
    std::vector<std::string> areas = ListPockets();
    if (areas.empty()) {
        ImGui::TextDisabled("None yet.");
    }
    for (auto& a : areas) {
        ImGui::PushID(a.c_str());
        bool here = InPocket(play) && (PocketName() == a);
        ImGui::Text("%s%s", a.c_str(), here ? "  (here)" : "");
        ImGui::SameLine();
        if (!here && ImGui::SmallButton("Go there")) {
            if (sDirty) {
                SaveLayout();
            }
            GoToPocket(play, a);
        }
        ImGui::PopID();
    }
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##newarea", "New area's name", sNewArea, sizeof(sNewArea));
    ImGui::SameLine();
    if (ImGui::Button("Make it and go") && (sNewArea[0] != 0)) {
        std::string name = sNewArea;
        std::string path = PocketPath(name);
        try {
            if (!std::filesystem::exists(path)) {
                std::filesystem::create_directories(std::filesystem::path(path).parent_path());
                std::ofstream o(path, std::ios::binary | std::ios::trunc);
                o << "{ \"about\": \"A custom area (Map Editor)\", \"area\": { \"width\": 2000, \"length\": 2000 } }\n";
            }
        } catch (...) {
        }
        if (sDirty) {
            SaveLayout();
        }
        sNewArea[0] = 0;
        GoToPocket(play, name);
    }
    if (sHasReturn && ImGui::Button("Back where he came from")) {
        if (sDirty) {
            SaveLayout();
        }
        GoBack(play);
    }

    if (!InPocket(play)) {
        ImGui::TextDisabled("Go into a custom area to shape it here.");
        return;
    }
    AreaSpec& A = sLayout.area;
    ImGui::SeparatorText(("This area: " + PocketName()).c_str());
    bool ground = false;
    ImGui::SetNextItemWidth(120);
    ground |= ImGui::DragFloat("Width", &A.ground.w, 10.0f, 200.0f, 30000.0f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ground |= ImGui::DragFloat("Length", &A.ground.l, 10.0f, 200.0f, 30000.0f, "%.0f");
    ImGui::SetNextItemWidth(120);
    ground |= ImGui::DragFloat("Walls (0: none)", &A.ground.wallH, 5.0f, 0.0f, 4000.0f, "%.0f");
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Floor texture", GdfBuild_TexName(A.ground.floorTex))) {
        for (int i = 0; i < GdfBuild_TexCount(); i++) {
            if (ImGui::Selectable(GdfBuild_TexName(i), A.ground.floorTex == i)) {
                A.ground.floorTex = i;
                ground = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Wall texture", GdfBuild_TexName(A.ground.wallTex))) {
        for (int i = 0; i < GdfBuild_TexCount(); i++) {
            if (ImGui::Selectable(GdfBuild_TexName(i), A.ground.wallTex == i)) {
                A.ground.wallTex = i;
                ground = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Footsteps", GdfBuild_SoundName(A.ground.sound))) {
        for (int i = 0; i < 14; i++) {
            if (ImGui::Selectable(GdfBuild_SoundName(i), A.ground.sound == i)) {
                A.ground.sound = i;
                ground = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ground) {
        Changed();
    }
    if (ImGui::Button("Rebuild the ground (reloads the area)")) {
        SaveLayout();
        ReloadArea();
    }
    ImGui::TextDisabled("Size, walls, textures and footsteps take effect when the area reloads.");

    ImGui::SeparatorText("Light and fog (live)");
    bool look = false;
    f32 amb[3] = { A.amb[0] / 255.0f, A.amb[1] / 255.0f, A.amb[2] / 255.0f };
    f32 light[3] = { A.light[0] / 255.0f, A.light[1] / 255.0f, A.light[2] / 255.0f };
    f32 fog[3] = { A.fog[0] / 255.0f, A.fog[1] / 255.0f, A.fog[2] / 255.0f };
    if (ImGui::ColorEdit3("Ambient", amb)) {
        for (int i = 0; i < 3; i++) {
            A.amb[i] = (u8)(amb[i] * 255.0f + 0.5f);
        }
        look = true;
    }
    if (ImGui::ColorEdit3("Light", light)) {
        for (int i = 0; i < 3; i++) {
            A.light[i] = (u8)(light[i] * 255.0f + 0.5f);
        }
        look = true;
    }
    if (ImGui::ColorEdit3("Fog and sky", fog)) {
        for (int i = 0; i < 3; i++) {
            A.fog[i] = (u8)(fog[i] * 255.0f + 0.5f);
        }
        look = true;
    }
    int fn = A.fogNear, ff = A.fogFar;
    ImGui::SetNextItemWidth(120);
    if (ImGui::DragInt("Fog starts", &fn, 1.0f, 0, 1000)) {
        A.fogNear = (s16)fn;
        look = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::DragInt("Fog ends", &ff, 20.0f, 100, 32000)) {
        A.fogFar = (s16)ff;
        look = true;
    }
    if (look) {
        ApplyAreaLook(play);
        Changed();
    }

    ImGui::SeparatorText("Sound (live)");
    std::string cur = (A.music == 0x7F) ? std::string("None") : Hex4(A.music);
    std::map<uint16_t, SequenceInfo> seqs = AudioCollection::Instance->GetAllSequences();
    auto found = seqs.find(A.music);
    if (found != seqs.end()) {
        cur = found->second.label;
    }
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Music", cur.c_str())) {
        if (ImGui::Selectable("None", A.music == 0x7F)) {
            A.music = 0x7F;
            StartAreaMusic(play);
            Changed();
        }
        for (auto& [id, info] : seqs) {
            if (!(info.category & SEQ_BGM_CUSTOM)) {
                continue;
            }
            if (ImGui::Selectable(info.label.c_str(), A.music == id)) {
                A.music = id;
                StartAreaMusic(play);
                Changed();
            }
        }
        ImGui::EndCombo();
    }
    int nat = A.nature;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderInt("Nature sound (19: none)", &nat, 0, 0x13)) {
        A.nature = (u8)nat;
        StartAreaMusic(play);
        Changed();
    }

    ImGui::SeparatorText("Start point");
    ImGui::Text("%.0f, %.0f, %.0f", A.spawn.x, A.spawn.y, A.spawn.z);
    ImGui::SameLine();
    if (ImGui::SmallButton("Where he stands")) {
        A.spawn = GET_PLAYER(play)->actor.world.pos;
        A.spawnYaw = GET_PLAYER(play)->actor.shape.rot.y;
        Changed();
    }
}

void DrawFlags(PlayState* play) {
    ImGui::SeparatorText("Flags");
    static int type = FLAG_SWITCH;
    static int index = 0;
    FlagPicker("flags", &type, &index);
    bool on = FlagGet(play, type, index);
    ImGui::SameLine();
    if (ImGui::Checkbox("Set##flag", &on)) {
        FlagSet(play, type, index, on);
    }
    if (type != FLAG_EVENT) {
        // This area's whole set at a glance
        int n = FlagCount(type);
        for (int i = 0; i < n; i++) {
            bool v = FlagGet(play, type, i);
            ImGui::PushID(i);
            if ((i % 16) != 0) {
                ImGui::SameLine(0.0f, 2.0f);
            }
            ImGui::PushStyleColor(ImGuiCol_Button, v ? ImVec4(0.6f, 0.2f, 0.7f, 1.0f) : ImVec4(0.15f, 0.12f, 0.2f, 1.0f));
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%02X", i);
            if (ImGui::Button(lbl, ImVec2(26.0f, 0.0f))) {
                FlagSet(play, type, i, !v);
                index = i;
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
}

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
            sLayout.hasTerrain = true; // (so the saved file also clears what was built)
            GdfBuild_ImportScene(play->sceneNum, nlohmann::json::object(), true);
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
    DrawFlags(play);
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
        if (ImGui::BeginTabItem("Map")) {
            DrawMapTab(play);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Actors")) {
            DrawActorsTab(play);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Add")) {
            DrawAddTab(play);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Terrain")) {
            DrawTerrainTab(play);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Triggers")) {
            DrawTriggersTab(play);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Areas")) {
            DrawAreasTab(play);
            ImGui::EndTabItem();
        }
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
    PlayState* play = gPlayState;
    sScene = -1;
    EnsureLayout(play);
    sAddedLive.clear();
    sSceneWideDone = false;
    sSpawnedRoom = -1;
    sTrigInside.clear();
    sMusicDelay = -1;
    if (InPocket(play)) {
        ApplyAreaLook(play);
        // (should the area come up without a start point for him, he's given one rather than the game falling over)
        static ActorEntry sFallbackStart = { ACTOR_PLAYER, { 0, 0, 0 }, { 0, 0, 0 }, 0x0FFF };
        if (play->linkActorEntry == nullptr) {
            play->linkActorEntry = &sFallbackStart;
        }
        // Walked in (not put back after a fall or a reload): he arrives at the area's start point
        if ((gSaveContext.respawnFlag == 0) && (play->linkActorEntry != nullptr)) {
            const AreaSpec& A = sLayout.area;
            play->linkActorEntry->pos = { (s16)A.spawn.x, (s16)A.spawn.y, (s16)A.spawn.z };
            play->linkActorEntry->rot = { 0, A.spawnYaw, 0 };
        }
        sMusicDelay = 20;
    }
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

// An area is loading (before its collision is set up): its layout's build handed to Terraform
void GdfMapEdit_AreaLoading(PlayState* play, CollisionHeader* header) {
    if (play == nullptr) {
        return;
    }
    sScene = -1;
    EnsureLayout(play);
    if (InPocket(play)) {
        GdfBuild_MakePocketGround(header, sLayout.area.ground);
    }
    if (sLayout.hasTerrain) {
        try {
            GdfBuild_ImportScene(play->sceneNum, sLayout.terrain, false);
        } catch (...) {
            sStatus = "The layout's terrain has an error; the save's build is used";
        }
    }
}

std::shared_ptr<Ship::GuiWindow> GdfMapEditor_CreateWindow() {
    return std::make_shared<GdfMapEditorWindow>(CVAR_WINDOW("GdfMapEditor"), "Map Editor", ImVec2(560, 720));
}
