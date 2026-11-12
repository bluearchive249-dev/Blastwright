// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// Play as Ganondorf: the Dark Summoning menu. Once he holds the Triforce (or with the developer tools on, for testing),
// hold L and press R to open it: the world stops, and he can call up enemies, bosses, objects and items, or anything
// in the game from the full list, in front of him.
//   Stick or D-pad up/down: choose   Stick or D-pad left/right: change page   A: summon   B (or L+R again): close
// The mouse works too: click a page, click an entry to summon it.
// While it's open the camera rises to look down on him from overhead (sliding back down when it closes).

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/GuiWindow.h>
#include <imgui.h>

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Notification/Notification.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
s32 PlayAsGanondorf_OwnsLButton(void);
s32 GdfSacred_HasTriforce(void);
extern PlayState* gPlayState;
}

#include "GanondorfBuild.h"

namespace {

struct Summon {
    const char* name;
    s16 id;
    s16 params;
};

// The curated pages (the full list is the last page)
const Summon sEnemies[] = {
    { "Stalfos", ACTOR_EN_TEST, 0 },
    { "Redead", ACTOR_EN_RD, 1 },
    { "Gibdo", ACTOR_EN_RD, (s16)0xFE },
    { "Keese", ACTOR_EN_FIREFLY, 2 },
    { "Fire Keese", ACTOR_EN_FIREFLY, 0 },
    { "Ice Keese", ACTOR_EN_FIREFLY, 4 },
    { "Deku Baba", ACTOR_EN_DEKUBABA, 0 },
    { "Big Deku Baba", ACTOR_EN_DEKUBABA, 1 },
    { "Like Like", ACTOR_EN_RR, 0 },
    { "Wolfos", ACTOR_EN_WF, 0 },
    { "White Wolfos", ACTOR_EN_WF, 1 },
    { "Lizalfos", ACTOR_EN_ZF, -1 },
    { "Iron Knuckle", ACTOR_EN_IK, 1 },
    { "Moblin", ACTOR_EN_MB, 0 },
    { "Peahat", ACTOR_EN_PEEHAT, -1 },
    { "Red Tektite", ACTOR_EN_TITE, -1 },
    { "Blue Tektite", ACTOR_EN_TITE, -2 },
    { "Skulltula", ACTOR_EN_ST, 0 },
    { "Poe", ACTOR_EN_POH, 0 },
    { "Dodongo", ACTOR_EN_DODONGO, 0 },
    { "Beamos", ACTOR_EN_VM, 0 },
    { "Armos", ACTOR_EN_AM, 1 },
    { "Floormaster", ACTOR_EN_FLOORMAS, 0 },
    { "Wallmaster", ACTOR_EN_WALLMAS, 0 },
    { "Freezard", ACTOR_EN_FZ, 0 },
    { "Stalchild", ACTOR_EN_SKB, 0 },
    { "Guay", ACTOR_EN_CROW, 0 },
    { "Flare Dancer", ACTOR_EN_FD, 0 },
    { "Dead Hand", ACTOR_EN_DH, 0 },
    { "Dark Link", ACTOR_EN_TORCH2, 0 },
};

// Bosses outside their own rooms: some won't behave, and some may crash the game
const Summon sBosses[] = {
    // The mini-bosses fight anywhere. Of the dungeon bosses only these three are tried: the others are scripted to
    // their own rooms (Volvagia's holes, Morpha's pool, Bongo Bongo's drum, Phantom Ganon's paintings, Twinrova's
    // platforms) and do nothing or crash the game elsewhere.
    { "Dark Link", ACTOR_EN_TORCH2, 0 },
    { "Dead Hand", ACTOR_EN_DH, 0 },
    { "Flare Dancer", ACTOR_EN_FD, 0 },
    { "Iron Knuckle", ACTOR_EN_IK, 1 },
    { "Gerudo fighter", ACTOR_EN_GELDB, 0 },
    { "Stalfos", ACTOR_EN_TEST, 0 },
    { "Queen Gohma (experimental)", ACTOR_BOSS_GOMA, 0 },
    { "King Dodongo (experimental)", ACTOR_BOSS_DODONGO, 0 },
    { "Barinade (experimental)", ACTOR_BOSS_VA, 0 },
};

const Summon sObjects[] = {
    { "Pot", ACTOR_OBJ_TSUBO, 0 },
    { "Wooden crate", ACTOR_OBJ_KIBAKO2, 0 },
    { "Cucco", ACTOR_EN_NIW, 0 },
    { "Bomb", ACTOR_EN_BOM, 0 },
    { "Fairy", ACTOR_EN_ELF, 2 },
    { "Recovery heart", ACTOR_EN_ITEM00, ITEM00_HEART },
    { "Green rupee", ACTOR_EN_ITEM00, ITEM00_RUPEE_GREEN },
    { "Blue rupee", ACTOR_EN_ITEM00, ITEM00_RUPEE_BLUE },
    { "Red rupee", ACTOR_EN_ITEM00, ITEM00_RUPEE_RED },
    { "Large magic jar", ACTOR_EN_ITEM00, ITEM00_MAGIC_LARGE },
    { "Arrows", ACTOR_EN_ITEM00, ITEM00_ARROWS_LARGE },
};

enum Page { PAGE_ENEMIES, PAGE_BOSSES, PAGE_OBJECTS, PAGE_BUILD, PAGE_ALL, PAGE_COUNT };
const char* sPageNames[PAGE_COUNT] = { "Enemies", "Bosses", "Objects & items", "Build", "Everything" };

bool sOpen = false;
int sPage = PAGE_ENEMIES;
int sSel[PAGE_COUNT] = {};
bool sScrollToSel = false;
int sStickHeld = 0; // frames the stick has been held in one direction (for repeat)
int sStickDir = 0;  // 1 up, 2 down, 3 left, 4 right
std::vector<Summon> sAll;
std::vector<std::string> sAllNames;
char sFilter[64] = "";
int sAllParams = 0;
std::string sLast;
Summon sPending;           // what's being placed with the marker
std::string sPendingName;
f32 sCamBlend = 0.0f; // 0 his own camera .. 1 overhead
Vec3f sCamEye;        // where his own camera stood as the menu opened
Vec3f sCamAt;

void BuildAll() {
    sAll.clear();
    sAllNames.clear();
    std::string filter = sFilter;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
    for (int i = 0; i < ActorDB::Instance->GetEntryCount(); i++) {
        ActorDB::Entry& e = ActorDB::Instance->RetrieveEntry(i);
        if (!e.entry.valid || (i == ACTOR_PLAYER)) {
            continue;
        }
        std::string label = (e.desc.empty() ? e.name : e.desc) + "  (" + e.name + ")";
        std::string low = label;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        if (!filter.empty() && (low.find(filter) == std::string::npos)) {
            continue;
        }
        sAll.push_back({ nullptr, (s16)i, 0 });
        sAllNames.push_back(label);
    }
}

#define BUILD_ID (-1000) // (the Build page's entries: BUILD_ID - kind, then remove nearest, then clear the area)

int PageCount(int page) {
    switch (page) {
        case PAGE_BUILD:
            return GdfBuild_KindCount() + 4;
        case PAGE_ENEMIES:
            return ARRAY_COUNT(sEnemies);
        case PAGE_BOSSES:
            return ARRAY_COUNT(sBosses);
        case PAGE_OBJECTS:
            return ARRAY_COUNT(sObjects);
        default:
            return (int)sAll.size();
    }
}

Summon PageEntry(int page, int i) {
    switch (page) {
        case PAGE_BUILD:
            if (i < GdfBuild_KindCount()) {
                return { GdfBuild_KindName(i), (s16)(BUILD_ID - i), 0 };
            }
            {
                static const char* sExtra[4] = { "Sculpt the ground (A raise, Z lower)", "Remove the nearest piece",
                                                 "Clear everything built in this area",
                                                 "Put this area's ground back as it was" };
                return { sExtra[i - GdfBuild_KindCount()], (s16)(BUILD_ID - i), 0 };
            }
        case PAGE_ENEMIES:
            return sEnemies[i];
        case PAGE_BOSSES:
            return sBosses[i];
        case PAGE_OBJECTS:
            return sObjects[i];
        default: {
            Summon s = sAll[i];
            s.name = sAllNames[i].c_str();
            s.params = (s16)sAllParams;
            return s;
        }
    }
}

bool Allowed() {
    PlayState* play = gPlayState;
    if ((play == nullptr) || !PlayAsGanondorf_OwnsLButton()) {
        return false;
    }
    return GdfSacred_HasTriforce() || CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0);
}

void SetOpen(bool open) {
    PlayState* play = gPlayState;
    if (!open) {
        GdfBuild_Cancel(); // (a piece still being placed goes with the menu)
    }
    sOpen = open;
    if (play != nullptr) {
        // The world holds still while the menu is up (the game's own frame-advance pause)
        play->frameAdvCtx.enabled = open;
        play->frameAdvCtx.timer = 0;
    }
    Audio_PlaySfxGeneral(open ? NA_SE_SY_FSEL_DECIDE_L : NA_SE_SY_FSEL_CLOSE, &gSfxDefaultPos, 4,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    if (open) {
        if (play != nullptr) {
            sCamEye = play->view.eye;
            sCamAt = play->view.lookAt;
        }
        sCamBlend = 0.0f;
        BuildAll();
        sScrollToSel = true;
    }
}

void DoSummon(const Summon& s) {
    PlayState* play = gPlayState;

    if (s.id <= BUILD_ID) {
        int i = BUILD_ID - s.id;

        if (i < GdfBuild_KindCount()) {
            if (!GdfBuild_Begin(i)) {
                sLast = "No more pieces fit in this area (" + std::to_string(GdfBuild_MaxHere()) + ")";
            }
        } else if (i == GdfBuild_KindCount()) {
            GdfBuild_BeginSculpt();
        } else if (i == GdfBuild_KindCount() + 1) {
            sLast = GdfBuild_RemoveNearest() ? "Removed the nearest piece" : "Nothing built here";
        } else if (i == GdfBuild_KindCount() + 2) {
            sLast = "Cleared " + std::to_string(GdfBuild_ClearHere()) + " pieces";
        } else {
            sLast = GdfBuild_RestoreGroundHere() ? "The ground is as it was" : "Nothing sculpted here";
        }
        return;
    }
    // Choosing where it appears: a marker on the ground (A summons there, again and again; B goes back to the list)
    sPending = s;
    sPendingName = s.name;
    sPending.name = sPendingName.c_str();
    if (GdfBuild_BeginTarget()) {
        sLast = std::string("Where? ") + s.name;
    }
}

void SummonAt(const Summon& s, Vec3f pos, s16 yaw) {
    PlayState* play = gPlayState;
    // A dungeon boss appears only where its room isn't "cleared" (else it leaves its warp and heart and goes), and
    // plays its room's intro unless its battle has begun: so the room's clear flag is lifted while it's summoned, and
    // its battle marked as begun (which also skips that intro in its own room afterwards)
    bool boss = (ActorDB::Instance->RetrieveEntry(s.id).entry.category == ACTORCAT_BOSS);
    s32 room = play->roomCtx.curRoom.num;
    bool wasClear = boss && Flags_GetClear(play, room);

    if (boss) {
        Flags_UnsetClear(play, room);
        switch (s.id) {
            case ACTOR_BOSS_GOMA:
                Flags_SetEventChkInf(EVENTCHKINF_BEGAN_GOHMA_BATTLE);
                break;
            case ACTOR_BOSS_DODONGO:
                Flags_SetEventChkInf(EVENTCHKINF_BEGAN_KING_DODONGO_BATTLE);
                break;
            case ACTOR_BOSS_VA:
                Flags_SetEventChkInf(EVENTCHKINF_BEGAN_BARINA_BATTLE);
                break;
        }
    }
    Actor* a = Actor_Spawn(&play->actorCtx, play, s.id, pos.x, pos.y, pos.z, 0, yaw, 0, s.params);
    if (wasClear) {
        Flags_SetClear(play, room);
    }
    sLast = std::string(a != nullptr ? "Summoned: " : "It wouldn't come: ") + s.name;
    Audio_PlaySfxGeneral(a != nullptr ? NA_SE_EN_GANON_LAUGH : NA_SE_SY_ERROR, &gSfxDefaultPos, 4,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void MoveSel(int delta) {
    int n = PageCount(sPage);
    if (n <= 0) {
        return;
    }
    sSel[sPage] = (sSel[sPage] + delta + n) % n;
    sScrollToSel = true;
    Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

void MovePage(int delta) {
    sPage = (sPage + delta + PAGE_COUNT) % PAGE_COUNT;
    sScrollToSel = true;
    Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Spawn Mode's camera: high above him, looking down on him and the ground in front of him (where things appear),
// eased in from where his own camera stood. The world is still, so nothing else moves the view meanwhile; once the
// menu closes his own camera takes over again.
void OverheadCamera(PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yaw = player->actor.shape.rot.y;
    Vec3f focus = player->actor.world.pos;
    Vec3f at;
    Vec3f eye;
    Vec3f up;
    f32 t;

    // (placing a piece, the camera is on the piece)
    if (GdfBuild_Focus(&focus)) {
        at = focus;
    } else {
        at = focus;
        at.x += Math_SinS(yaw) * 70.0f;
        at.z += Math_CosS(yaw) * 70.0f;
    }
    // Above him and a little behind, so his front and what's summoned before him are in view
    eye.x = focus.x - Math_SinS(yaw) * 160.0f;
    eye.y = focus.y + 620.0f;
    eye.z = focus.z - Math_CosS(yaw) * 160.0f;

    Math_ApproachF(&sCamBlend, 1.0f, 1.0f, 0.07f);
    t = sCamBlend * sCamBlend * (3.0f - 2.0f * sCamBlend);
    eye.x = sCamEye.x + (eye.x - sCamEye.x) * t;
    eye.y = sCamEye.y + (eye.y - sCamEye.y) * t;
    eye.z = sCamEye.z + (eye.z - sCamEye.z) * t;
    at.x = sCamAt.x + (at.x - sCamAt.x) * t;
    at.y = sCamAt.y + (at.y - sCamAt.y) * t;
    at.z = sCamAt.z + (at.z - sCamAt.z) * t;
    up.x = Math_SinS(yaw);
    up.y = 0.0f;
    up.z = Math_CosS(yaw);
    {
        // Up is his facing tipped toward the sky (never along the line of sight)
        Vec3f d = { at.x - eye.x, at.y - eye.y, at.z - eye.z };
        f32 len = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
        if (len > 1.0f) {
            f32 dot = (up.x * d.x + up.z * d.z) / len;
            up.x -= d.x / len * dot;
            up.y -= d.y / len * dot;
            up.z -= d.z / len * dot;
        }
        if (SQ(up.x) + SQ(up.y) + SQ(up.z) < 0.0001f) {
            up = { 0.0f, 1.0f, 0.0f };
        }
    }
    func_800AA358(&play->view, &eye, &at, &up);
    if (play->skyboxId && (play->skyboxId != SKYBOX_UNSET_1D) && !play->envCtx.skyboxDisabled) {
        SkyboxDraw_UpdateMatrix(&play->skyboxCtx, eye.x, eye.y, eye.z);
    }
}

void OnFrame() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        sOpen = false;
        return;
    }
    Input* input = &play->state.input[0];

    if (!sOpen) {
        if (CHECK_BTN_ALL(input->cur.button, BTN_L) && CHECK_BTN_ALL(input->press.button, BTN_R) && Allowed() &&
            (play->pauseCtx.state == 0) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
            (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->csCtx.state == CS_STATE_IDLE)) {
            SetOpen(true);
        }
        return;
    }
    if (!Allowed() || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        SetOpen(false);
        return;
    }
    // Keep the world still (anything else may have let it go), and look down from overhead
    play->frameAdvCtx.enabled = true;
    if (GdfBuild_IsTargeting()) {
        // Choosing where a summoning appears
        Vec3f pos;
        s16 yaw;
        int r = GdfBuild_TargetFrame(input, &pos, &yaw);

        if (r > 0) {
            SummonAt(sPending, pos, yaw);
        } else if (r < 0) {
            sLast = "";
        }
        OverheadCamera(play);
        return;
    }
    if (GdfBuild_IsPlacing()) {
        // Setting a piece down: the controls are the piece's until it's placed or cancelled
        if (!GdfBuild_PlacingFrame(input)) {
            sLast = "Built: " + std::to_string(GdfBuild_CountHere()) + " of " + std::to_string(GdfBuild_MaxHere()) +
                    " pieces in this area";
        }
        OverheadCamera(play);
        return;
    }
    OverheadCamera(play);

    if (CHECK_BTN_ALL(input->press.button, BTN_B) ||
        (CHECK_BTN_ALL(input->cur.button, BTN_L) && CHECK_BTN_ALL(input->press.button, BTN_R))) {
        SetOpen(false);
        return;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A) && (PageCount(sPage) > 0)) {
        DoSummon(PageEntry(sPage, sSel[sPage]));
    }

    // Stick (with repeat while held) and D-pad
    int dir = 0;
    s8 sx = input->rel.stick_x;
    s8 sy = input->rel.stick_y;
    if (ABS(sy) > 40 && ABS(sy) >= ABS(sx)) {
        dir = (sy > 0) ? 1 : 2;
    } else if (ABS(sx) > 40) {
        dir = (sx < 0) ? 3 : 4;
    }
    bool step = false;
    if (dir != 0) {
        if (dir != sStickDir) {
            sStickHeld = 0;
            step = true;
        } else if ((++sStickHeld > 14) && ((sStickHeld % 3) == 0) && (dir <= 2)) {
            step = true;
        }
    }
    sStickDir = dir;
    if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        dir = 1, step = true;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        dir = 2, step = true;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        dir = 3, step = true;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        dir = 4, step = true;
    }
    if (step) {
        switch (dir) {
            case 1:
                MoveSel(-1);
                break;
            case 2:
                MoveSel(1);
                break;
            case 3:
                MovePage(-1);
                break;
            case 4:
                MovePage(1);
                break;
        }
    }
}

class GdfSpawnerWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;
};

void GdfSpawnerWindow::Draw() {
    if (!sOpen || (gPlayState == nullptr)) {
        return;
    }
    if (GdfBuild_IsPlacing() || GdfBuild_IsTargeting()) {
        // Setting a piece down: the menu out of the way, its controls along the bottom
        ImGuiViewport* v = ImGui::GetMainViewport();
        ImDrawList* list = ImGui::GetForegroundDrawList(v);
        std::string where = sLast + "    Stick: move the marker    C-Left/Right: turn    A: summon here    "
                                    "B: back to the list";
        const char* texName = GdfBuild_PlacingTexName();
        std::string piece = std::string(texName != nullptr ? std::string("Texture: ") + texName + "  (L: next)    " : "") +
                            "Stick: move    C-Left/Right: turn    C-Up/Down: raise/lower (R held: taller/shorter)    "
                            "D-pad: longer/shorter, wider/narrower    Z: onto the floor    A: set down    B: cancel";
        std::string sculpt = std::string(GdfBuild_SculptInfo()) +
                             "    Stick: move the brush    A (hold): raise    Z (hold): lower    D-pad: brush size    B: done";
        const char* help = GdfBuild_IsTargeting() ? where.c_str()
                           : GdfBuild_IsSculpting()
                               ? sculpt.c_str()
                               : piece.c_str();
        float fontSize = ImGui::GetFontSize() * std::max(v->Size.y / 720.0f, 1.0f);
        ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fontSize, v->Size.x * 0.9f, v->Size.x * 0.9f, help);
        ImVec2 p(v->Pos.x + (v->Size.x - ts.x) * 0.5f, v->Pos.y + v->Size.y - ts.y - 24.0f);

        list->AddRectFilled(ImVec2(p.x - 10.0f, p.y - 6.0f), ImVec2(p.x + ts.x + 10.0f, p.y + ts.y + 6.0f),
                            IM_COL32(18, 5, 26, 215), 6.0f);
        list->AddText(ImGui::GetFont(), fontSize, p, IM_COL32(255, 215, 110, 255), help, nullptr, v->Size.x * 0.9f);
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float scale = std::max(vp->Size.y / 720.0f, 1.0f);
    ImVec2 size(420.0f * scale, 470.0f * scale);

    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.02f, 0.10f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.65f, 0.15f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.55f, 0.12f, 0.35f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.45f, 0.10f, 0.30f, 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::Begin("Dark Summoning##GdfSpawner", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    ImGui::SetWindowFontScale(scale);

    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.3f, 1.0f), "Dark Summoning");
    ImGui::SameLine();
    ImGui::TextDisabled("  A summon   B close   stick: choose / page");

    // Pages
    for (int p = 0; p < PAGE_COUNT; p++) {
        if (p > 0) {
            ImGui::SameLine();
        }
        bool cur = (p == sPage);
        if (cur) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.4f, 1.0f));
        }
        if (ImGui::Button(sPageNames[p])) {
            sPage = p;
            sScrollToSel = true;
        }
        if (cur) {
            ImGui::PopStyleColor();
        }
    }
    if (sPage == PAGE_BUILD) {
        ImGui::TextColored(ImVec4(0.8f, 0.75f, 0.6f, 1.0f), "Built here: %d of %d. Kept with your save.",
                           GdfBuild_CountHere(), GdfBuild_MaxHere());
    } else if (sPage == PAGE_BOSSES) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "Mini-bosses fight anywhere. The three dungeon bosses are experimental outside their rooms.");
    } else if (sPage == PAGE_ALL) {
        ImGui::SetNextItemWidth(170.0f * scale);
        if (ImGui::InputTextWithHint("##filter", "Search", sFilter, sizeof(sFilter))) {
            BuildAll();
            sSel[PAGE_ALL] = 0;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f * scale);
        ImGui::InputInt("Params", &sAllParams, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    }

    float bottom = ImGui::GetTextLineHeightWithSpacing() * 1.4f;
    ImGui::BeginChild("##list", ImVec2(0, -bottom), true);
    int n = PageCount(sPage);
    if (sSel[sPage] >= n) {
        sSel[sPage] = std::max(n - 1, 0);
    }
    for (int i = 0; i < n; i++) {
        Summon s = PageEntry(sPage, i);
        bool sel = (i == sSel[sPage]);
        ImGui::PushID(i);
        if (ImGui::Selectable(s.name, sel)) {
            sSel[sPage] = i;
            DoSummon(s);
        }
        if (sel && sScrollToSel) {
            ImGui::SetScrollHereY(0.5f);
        }
        ImGui::PopID();
    }
    sScrollToSel = false;
    ImGui::EndChild();
    ImGui::TextDisabled("%s", sLast.c_str());

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void RegisterGanondorfSpawner() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnFrame);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfSpawner, {});

} // namespace

std::shared_ptr<Ship::GuiWindow> GdfSpawner_CreateWindow() {
    return std::make_shared<GdfSpawnerWindow>(CVAR_WINDOW("GdfSpawner"), "Dark Summoning");
}
