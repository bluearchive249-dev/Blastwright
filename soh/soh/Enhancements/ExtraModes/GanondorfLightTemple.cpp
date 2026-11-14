// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// SoH: "Play as Ganondorf", the Temple of Light: fifty rooms in one scene (SCENE_SYOTES), past the Sages' mirror.
//
// The rooms, their passages (with the game's own room-change planes, En_Holl, between them), the sunbeams and the
// flames are the scene's own (tools/gdf_castle/lighttemple.py, mods/gdf_sacred.o2r). Everything that moves or answers
// him is placed here, room by room as each room becomes the one he's in, from GanondorfLightTempleData.h:
//  - doors: key doors (kicked open with a small key), the boss door (the boss key), sun and moon seals (open in the
//    temple's Light or Shadow aspect), bars (open once the room's puzzle is solved or its guards beaten)
//  - chests (kicked open; some appear only when their room is solved), the guards
//  - the master mirrors: struck, the whole temple turns from Light to Shadow or back (a saved flag); the Atrium's own
//    stands on its great platform, which rides between the Halls (Light) and the Waterways (Shadow)
//  - mirrors joining the floors, some working in one aspect only; bridges of light (Light) and of shadow (Shadow)
//  - the puzzles: Triforce plates and their mural, braziers, crystal switches against the clock, the Sages' pillars,
//    turning mirrors for a sunbeam, a kick block and a heavy plate an iron guard can hold down, a timed bridge
// Small keys held are counted from the saved flags: key chests opened, less key doors opened. Falls (into the Halls'
// broken floors, the Waterways' water, the Sanctums' sky) cost half a heart and put him back where he came in.

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/frame_interpolation.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <cstring>
#include <string>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
extern char** sequenceMap;
extern size_t sequenceMapSize;
void BossGanon_Player_HoldUp(s16 frames);
s32 BossGanon_Player_GetHeldUpPos(Vec3f* pos);
Actor* GdfSacred_SpawnGuard(PlayState* play, f32 x, f32 y, f32 z, s16 yaw, s16 params);
s32 GdfSacred_GuardId(void);
void GdfSacred_Say(PlayState* play, const char* text);
}

#include "GanondorfLightTempleData.h"

#define TEMPLE_SCENE SCENE_SYOTES
#define RAURU_ENTRANCE ENTR_BESITU_0

enum { T_DOOR, T_CHEST, T_BRIDGE, T_MIRROR, T_PLATE, T_CRYSTAL, T_BRAZIER, T_PILLAR, T_BEAMMIRROR, T_EMITTER, T_TARGET,
       T_BLOCK, T_SOCKET, T_HEAVY, T_GUARD, T_MURAL, T_LIFT, T_SWITCH, T_LIGHTBRIDGE, T_EXIT, T_INSCRIPTION,
       T_LINKMIRROR = 1000 }; // (linked mirrors: 1000 + their index; everything else: its spawn's index)
enum { D_KEY, D_BOSS, D_SUN, D_MOON, D_BARS };
enum { IT_KEY, IT_BOSS, IT_RUPEE20, IT_RUPEE50, IT_RUPEE200, IT_HEART, IT_HEARTPIECE, IT_MAGIC };
enum { K_PLATES, K_HUB, K_COMBAT, K_BRAZIERS, K_TREASURE, K_CRYSTALS, K_MIRROR, K_QUIET, K_BEAM, K_BLOCK, K_CHASM,
       K_TIMED, K_WATER, K_MINIBOSS, K_DARK, K_SECRET, K_SIMON, K_BRIDGES, K_ANTE, K_ATRIUM };

#define DL(name) "__OTR__objects/gdf_sacred/" name "DL"
#define COL(name) "__OTR__objects/gdf_sacred/gGdfTCol_" name

static const Color_RGBA8 sSage[6] = { { 70, 230, 80, 255 },  { 255, 80, 40, 255 },  { 60, 130, 255, 255 },
                                      { 175, 70, 235, 255 }, { 255, 160, 40, 255 }, { 255, 235, 110, 255 } };

// ---- Shared ----

static bool Available() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.SacredRealm"), 1) &&
           ResourceMgr_FileExists("objects/gdf_sacred/gGdfTDoorKeyDL");
}

static bool InTemple(PlayState* play) {
    return (play != NULL) && (play->sceneNum == TEMPLE_SCENE) && Available();
}

static bool Shadow(PlayState* play) {
    return Flags_GetSwitch(play, GDF_T_FLAG_ASPECT);
}

static s32 KeysHeld(PlayState* play) {
    s32 n = 0;
    for (u32 i = 0; i < ARRAY_COUNT(sGdfTKeyChests); i++) {
        n += Flags_GetTreasure(play, sGdfTKeyChests[i]) ? 1 : 0;
    }
    for (s32 i = 0; i < GDF_T_KEYDOORS; i++) {
        n -= Flags_GetSwitch(play, GDF_T_FLAG_KEYDOOR0 + i) ? 1 : 0;
    }
    return n;
}

static bool HasBossKey(PlayState* play) {
    for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
        if ((sGdfTSpawns[i].type == T_CHEST) && (sGdfTSpawns[i].p1 == IT_BOSS)) {
            return Flags_GetTreasure(play, sGdfTSpawns[i].p0);
        }
    }
    return false;
}

static void Chime(u16 sfx) {
    Audio_PlaySfxGeneral(sfx, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

static void Sparkle(PlayState* play, Vec3f* pos, f32 spread, s32 count, Color_RGBA8 c) {
    static Vec3f sNoAccel = { 0.0f, 0.0f, 0.0f };
    for (s32 i = 0; i < count; i++) {
        Color_RGBA8 prim = { (u8)MIN(255, c.r + 80), (u8)MIN(255, c.g + 80), (u8)MIN(255, c.b + 80), 255 };
        Color_RGBA8 env = { c.r, c.g, c.b, 0 };
        Vec3f p = { pos->x + Rand_CenteredFloat(spread), pos->y + Rand_CenteredFloat(spread),
                    pos->z + Rand_CenteredFloat(spread) };
        Vec3f v = { Rand_CenteredFloat(0.8f), 0.8f + Rand_ZeroFloat(0.8f), Rand_CenteredFloat(0.8f) };
        EffectSsKiraKira_SpawnDispersed(play, &p, &v, &sNoAccel, &prim, &env, (s16)(Rand_ZeroFloat(900.0f) + 1100.0f),
                                        20);
    }
}

static void Say(PlayState* play, const char* text) {
    GdfSacred_Say(play, text);
}

static void Draw(PlayState* play, Gfx** disp, const char* dl) {
    gSPMatrix((*disp)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*disp)++, (Gfx*)dl);
}

// ---- The temple's state, as he goes ----

static u64 sSpawned = 0;             // rooms whose things are out
static s16 sLastCur = -1;
static Vec3f sEntryPos;              // where he came into the room he's in (a fall puts him back here)
static s16 sEntryYaw = 0;
static s16 sFlash = 0;               // the white flash of the temple turning
static s16 sMirrorCooldown = 0;
// a mirror's journey: 1 fading out, 2 the far room loading, 3 placed
static s16 sTeleport = 0, sTeleportTimer = 0, sTeleportTo = -1;
// puzzles in progress (the room he's in)
static s16 sPlateStep[GDF_T_ROOMS];
static s16 sSimonStep[GDF_T_ROOMS];
static s16 sCrystalTimer[GDF_T_ROOMS];
static s16 sBridgeTimer[GDF_T_ROOMS];
static u8 sLit[GDF_T_SPAWNS];        // braziers lit, crystals struck, pillars lit (this visit)
static s8 sMirrorState[GDF_T_SPAWNS];
static bool sBeamOn[GDF_T_ROOMS];
static bool sArrivedFromRauru = false;
static s16 sArrivalStage = 0;

static s32 sThingId = -1;

typedef struct {
    DynaPolyActor dyna;
    ColliderCylinder col;
    s16 idx;           // the spawn (sGdfTSpawns) or, for linked mirrors, T_LINKMIRROR + the mirror's index
    s16 timer;
    f32 open;          // doors: how far down into the floor; chests: the lid's angle; the lift: its height
    u8 hasCol;
    u8 hasDyna;
    u8 state;
    s16 slide;         // the block: frames left of sliding
    Vec3f slideDir;
} GdfThing;

static ColliderCylinderInit sHitInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 34, 130, 0, { 0, 0, 0 } },
};

static const GdfTSpawn* Spawn(GdfThing* t) {
    return &sGdfTSpawns[t->idx];
}

static s32 Type(GdfThing* t) {
    return (t->idx >= T_LINKMIRROR) ? T_LINKMIRROR : sGdfTSpawns[t->idx].type;
}

// ---- Doors ----

static bool DoorOpen(PlayState* play, const GdfTSpawn* s) {
    switch (s->p0) {
        case D_KEY:
        case D_BOSS:
        case D_BARS:
            return Flags_GetSwitch(play, s->p1);
        case D_SUN:
            return !Shadow(play);
        case D_MOON:
            return Shadow(play);
    }
    return true;
}

static const char* DoorDL(s32 kind) {
    switch (kind) {
        case D_KEY:
            return DL("gGdfTDoorKey");
        case D_BOSS:
            return DL("gGdfTDoorBoss");
        case D_SUN:
            return DL("gGdfTDoorSun");
        case D_MOON:
            return DL("gGdfTDoorMoon");
    }
    return DL("gGdfTDoorBars");
}

// ---- Bridges: of light (Light), of shadow (Shadow), the timed one (its switch) ----

static bool BridgeSolid(PlayState* play, const GdfTSpawn* s) {
    if (s->p0 == 2) {
        return sBridgeTimer[s->room] > 0;
    }
    return (s->p0 == 1) == Shadow(play);
}

// ---- The beam: from its window, along the floor, off the turning mirrors, to the sun emblem ----

#define MAX_BEAM 6
typedef struct {
    Vec3f a, b;
} Seg;

static s32 TraceBeam(s32 emitter, Seg* segs, bool* hit) {
    const GdfTSpawn* e = &sGdfTSpawns[emitter];
    Vec3f p = { (f32)e->x, (f32)e->y, (f32)e->z };
    f32 dx = Math_SinS(e->yaw), dz = Math_CosS(e->yaw);
    s32 n = 0;
    s32 last = -1;

    *hit = false;
    for (s32 step = 0; step < MAX_BEAM; step++) {
        f32 best = 2600.0f;
        s32 bestIdx = -1;
        for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
            const GdfTSpawn* s = &sGdfTSpawns[i];
            if ((s->room != e->room) || (i == last) || ((s->type != T_BEAMMIRROR) && (s->type != T_TARGET))) {
                continue;
            }
            f32 rx = s->x - p.x, rz = s->z - p.z;
            f32 along = rx * dx + rz * dz;
            f32 across = fabsf(rx * dz - rz * dx);
            if ((along > 10.0f) && (across < 36.0f) && (along < best)) {
                best = along;
                bestIdx = i;
            }
        }
        Vec3f q = { p.x + dx * best, p.y, p.z + dz * best };
        if (bestIdx >= 0) {
            q.x = sGdfTSpawns[bestIdx].x;
            q.z = sGdfTSpawns[bestIdx].z;
        }
        segs[n].a = p;
        segs[n].b = q;
        n++;
        if (bestIdx < 0) {
            break;
        }
        if (sGdfTSpawns[bestIdx].type == T_TARGET) {
            *hit = true;
            break;
        }
        // a mirror: even states '/', odd '\' (struck, it turns a quarter)
        f32 ndx, ndz;
        if ((sMirrorState[bestIdx] & 1) == 0) {
            ndx = -dz;
            ndz = -dx;
        } else {
            ndx = dz;
            ndz = dx;
        }
        dx = ndx;
        dz = ndz;
        p = q;
        last = bestIdx;
    }
    return n;
}

// ---- Teleporting through a mirror ----

static void StartTeleport(PlayState* play, s32 mirror) {
    const GdfTMirror* m = &sGdfTMirrors[mirror];
    if ((sTeleport != 0) || (m->pair < 0)) {
        return;
    }
    sTeleport = 1;
    sTeleportTimer = 0;
    sTeleportTo = (s16)m->pair;
    Audio_PlayActorSound2(&GET_PLAYER(play)->actor, NA_SE_EV_TRIFORCE_FLASH);
}

// ---- The things ----

static void Thing_Init(Actor* thisx, PlayState* play) {
    GdfThing* t = (GdfThing*)thisx;
    const char* col = NULL;
    s32 type;

    t->idx = thisx->params;
    type = Type(t);
    Actor_SetScale(thisx, 1.0f);
    t->hasCol = false;
    t->hasDyna = false;
    t->open = 0.0f;
    t->state = 0;
    t->slide = 0;
    switch (type) {
        case T_DOOR:
            col = COL("door");
            t->open = DoorOpen(play, Spawn(t)) ? 1.0f : 0.0f;
            break;
        case T_CHEST:
            col = COL("chest");
            t->open = Flags_GetTreasure(play, Spawn(t)->p0) ? 1.0f : 0.0f;
            break;
        case T_BRIDGE:
        case T_LIGHTBRIDGE:
            col = COL("bridge");
            thisx->scale.z = (f32)Spawn(t)->p1;
            break;
        case T_LIFT:
            col = COL("lift");
            t->open = Shadow(play) ? (f32)Spawn(t)->p1 : (f32)Spawn(t)->p0;
            thisx->world.pos.y = t->open;
            break;
        case T_BLOCK:
            col = COL("block");
            break;
        case T_BRAZIER:
        case T_CRYSTAL:
        case T_PILLAR:
        case T_BEAMMIRROR:
        case T_MIRROR:
            t->hasCol = true;
            break;
    }
    if (type == T_LIFT) {
        t->hasCol = true; // (its mirror, on the dais in the middle)
    }
    if (col != NULL) {
        CollisionHeader* header = NULL;
        DynaPolyActor_Init(&t->dyna, DPM_PLAYER);
        CollisionHeader_GetVirtual((void*)col, &header);
        t->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, header);
        t->hasDyna = true;
    }
    if (t->hasCol) {
        Collider_InitCylinder(play, &t->col);
        Collider_SetCylinder(play, &t->col, thisx, &sHitInit);
        if (type == T_BEAMMIRROR) {
            t->col.dim.height = 160;
            t->col.dim.radius = 40;
        }
        thisx->colChkInfo.mass = MASS_IMMOVABLE;
    }
    if (type == T_BEAMMIRROR) {
        sMirrorState[t->idx] = (s8)Spawn(t)->p0;
    }
    if ((type == T_BRAZIER) && (Spawn(t)->p2 >= 0) && Flags_GetSwitch(play, Spawn(t)->p2)) {
        sLit[t->idx] = 1;
    }
    if (sLit[t->idx] && (type == T_BRAZIER)) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, thisx->world.pos.x, thisx->world.pos.y + 116.0f,
                    thisx->world.pos.z, 0, 0, 0, 0);
    }
}

static void Thing_Destroy(Actor* thisx, PlayState* play) {
    GdfThing* t = (GdfThing*)thisx;
    if (t->hasDyna) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, t->dyna.bgId);
    }
    if (t->hasCol) {
        Collider_DestroyCylinder(play, &t->col);
    }
}

static void SetSolid(PlayState* play, GdfThing* t, bool solid) {
    if (!t->hasDyna) {
        return;
    }
    if (solid) {
        func_8003EC50(play, &play->colCtx.dyna, t->dyna.bgId);
    } else {
        func_8003EBF8(play, &play->colCtx.dyna, t->dyna.bgId);
    }
}

static void FlipAspect(PlayState* play, Vec3f* at) {
    bool toShadow = !Shadow(play);
    if (toShadow) {
        Flags_SetSwitch(play, GDF_T_FLAG_ASPECT);
    } else {
        Flags_UnsetSwitch(play, GDF_T_FLAG_ASPECT);
    }
    sFlash = 30;
    sMirrorCooldown = 90;
    Chime(NA_SE_SY_CORRECT_CHIME);
    Audio_PlaySfxGeneral(NA_SE_EV_TRIFORCE_FLASH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    for (s32 k = 0; k < 6; k++) {
        Sparkle(play, at, 160.0f, 6, sSage[k]);
    }
}

static bool PlayerNear(PlayState* play, Actor* a, f32 r, f32 h) {
    Player* p = GET_PLAYER(play);
    return (Math_Vec3f_DistXZ(&p->actor.world.pos, &a->world.pos) < r) &&
           (fabsf(p->actor.world.pos.y - a->world.pos.y) < h);
}

static bool HeavyOn(PlayState* play, Actor* plate, bool playerCounts) {
    if (playerCounts && PlayerNear(play, plate, 60.0f, 30.0f)) {
        return true;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        if ((a->id == GdfSacred_GuardId()) && (a->colChkInfo.mass == MASS_HEAVY) &&
            (Math_Vec3f_DistXZ(&a->world.pos, &plate->world.pos) < 70.0f) && (a->colChkInfo.health > 0)) {
            return true;
        }
    }
    return false;
}

static Actor* FindThing(PlayState* play, s32 room, s32 type) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a != NULL; a = a->next) {
        if ((a->id == sThingId) && (((GdfThing*)a)->idx < T_LINKMIRROR) &&
            (sGdfTSpawns[((GdfThing*)a)->idx].room == room) && (sGdfTSpawns[((GdfThing*)a)->idx].type == type)) {
            return a;
        }
    }
    return NULL;
}

static void Solve(PlayState* play, s32 room) {
    s32 f = sGdfTRooms[room].solveFlag;
    if ((f >= 0) && !Flags_GetSwitch(play, f)) {
        Flags_SetSwitch(play, f);
        Chime(NA_SE_SY_CORRECT_CHIME);
    }
}

static void Thing_Update(Actor* thisx, PlayState* play) {
    GdfThing* t = (GdfThing*)thisx;
    Player* player = GET_PLAYER(play);
    s32 type = Type(t);
    const GdfTSpawn* s = (type == T_LINKMIRROR) ? NULL : Spawn(t);
    bool hit = false;

    t->timer++;
    if (t->hasCol) {
        hit = (t->col.base.acFlags & AC_HIT) != 0;
        t->col.base.acFlags &= ~AC_HIT;
    }
    switch (type) {
        case T_DOOR: {
            bool open = DoorOpen(play, s);
            f32 goal = open ? 1.0f : 0.0f;
            if (t->open != goal) {
                if ((t->timer % 6) == 0) {
                    Actor_PlaySfx_Flagged(thisx, NA_SE_EV_STONE_STATUE_OPEN - SFX_FLAG);
                }
                Math_StepToF(&t->open, goal, 0.04f);
                if (t->open == goal) {
                    Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
                }
            }
            thisx->world.pos.y = thisx->home.pos.y - t->open * 300.0f;
            SetSolid(play, t, t->open < 0.6f);
            break;
        }
        case T_CHEST:
            if ((s->p2 >= 0) && !Flags_GetSwitch(play, s->p2) && !Flags_GetTreasure(play, s->p0)) {
                SetSolid(play, t, false);
                t->state = 0; // hidden till its room is solved
            } else {
                if (t->state == 0) {
                    t->state = 1;
                    if ((s->p2 >= 0) && !Flags_GetTreasure(play, s->p0) && (t->timer > 2)) {
                        Chime(NA_SE_SY_TRE_BOX_APPEAR);
                        Sparkle(play, &thisx->world.pos, 60.0f, 20, sSage[5]);
                    }
                }
                SetSolid(play, t, true);
                if (Flags_GetTreasure(play, s->p0)) {
                    Math_StepToF(&t->open, 1.0f, 0.05f);
                }
            }
            break;
        case T_BRIDGE:
        case T_LIGHTBRIDGE:
            SetSolid(play, t, BridgeSolid(play, s));
            break;
        case T_SWITCH:
            if (PlayerNear(play, thisx, 46.0f, 30.0f)) {
                if (sBridgeTimer[s->room] <= 0) {
                    Chime(NA_SE_SY_CORRECT_CHIME);
                }
                sBridgeTimer[s->room] = (s16)(s->p0 * 20);
            } else if (sBridgeTimer[s->room] > 0) {
                sBridgeTimer[s->room]--;
                if ((sBridgeTimer[s->room] % 20) == 0) {
                    Chime(NA_SE_SY_WARNING_COUNT_N);
                }
            }
            break;
        case T_LIFT: {
            f32 goal = Shadow(play) ? (f32)s->p1 : (f32)s->p0;
            // its mirror, on the platform: struck, the temple turns and the platform rides to its other floor
            if (sMirrorCooldown == 0) {
                Vec3f m = thisx->world.pos;
                m.y += 14.0f;
                if (hit && PlayerNear(play, thisx, 220.0f, 80.0f) && (fabsf(t->open - goal) < 1.0f)) {
                    FlipAspect(play, &m);
                }
            }
            goal = Shadow(play) ? (f32)s->p1 : (f32)s->p0;
            if (t->open != goal) {
                Math_StepToF(&t->open, goal, 10.0f); // (slow enough to carry him)
                Actor_PlaySfx_Flagged(thisx, NA_SE_EV_ELEVATOR_MOVE - SFX_FLAG);
                if (t->open == goal) {
                    Audio_PlayActorSound2(thisx, NA_SE_EV_ELEVATOR_STOP);
                }
            }
            thisx->world.pos.y = t->open;
            break;
        }
        case T_MIRROR:
            if (hit && (sMirrorCooldown == 0)) {
                Vec3f m = thisx->world.pos;
                m.y += 100.0f;
                FlipAspect(play, &m);
            }
            break;
        case T_LINKMIRROR: {
            const GdfTMirror* m = &sGdfTMirrors[t->idx - T_LINKMIRROR];
            bool works = (m->kind == 0) && (m->pair >= 0) && ((m->aspect < 0) || ((m->aspect == 1) == Shadow(play)));
            // stepping into its glass (from in front): through to its pair
            Vec3f rel;
            Actor_WorldToActorCoords(thisx, &rel, &player->actor.world.pos);
            if (works && (sTeleport == 0) && (rel.z > -10.0f) && (rel.z < 30.0f) && (fabsf(rel.x) < 60.0f) &&
                (fabsf(rel.y) < 120.0f)) {
                StartTeleport(play, t->idx - T_LINKMIRROR);
            }
            break;
        }
        case T_PLATE: {
            bool on = PlayerNear(play, thisx, 55.0f, 25.0f);
            s32 room = s->room;
            if (!Flags_GetSwitch(play, s->p2) && on && (t->state == 0)) {
                if (s->p0 == sPlateStep[room]) {
                    sPlateStep[room]++;
                    if (sPlateStep[room] >= s->p1) {
                        Solve(play, room);
                    } else {
                        Chime(NA_SE_SY_TRE_BOX_APPEAR);
                    }
                } else if (sPlateStep[room] > 0) {
                    Chime(NA_SE_SY_ERROR);
                    sPlateStep[room] = (s16)((s->p0 == 0) ? 1 : 0);
                }
            }
            t->state = on ? 1 : 0;
            break;
        }
        case T_BRAZIER:
            if (hit && !sLit[t->idx]) {
                sLit[t->idx] = 1;
                Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, thisx->world.pos.x, thisx->world.pos.y + 116.0f,
                            thisx->world.pos.z, 0, 0, 0, 0);
                Audio_PlayActorSound2(thisx, NA_SE_EV_FLAME_IGNITION);
                bool all = true;
                for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
                    if ((sGdfTSpawns[i].room == s->room) && (sGdfTSpawns[i].type == T_BRAZIER) && !sLit[i]) {
                        all = false;
                    }
                }
                if (all) {
                    Solve(play, s->room);
                }
            }
            break;
        case T_CRYSTAL: {
            s32 room = s->room;
            if (Flags_GetSwitch(play, sGdfTRooms[room].solveFlag)) {
                sLit[t->idx] = 1;
                break;
            }
            if (hit && !sLit[t->idx]) {
                sLit[t->idx] = 1;
                Audio_PlayActorSound2(thisx, NA_SE_EV_FOOT_SWITCH);
                if (sCrystalTimer[room] <= 0) {
                    sCrystalTimer[room] = (s16)(s->p2 * 20);
                }
                bool all = true;
                for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
                    if ((sGdfTSpawns[i].room == room) && (sGdfTSpawns[i].type == T_CRYSTAL) && !sLit[i]) {
                        all = false;
                    }
                }
                if (all) {
                    sCrystalTimer[room] = 0;
                    Solve(play, room);
                }
            }
            // (the clock is kept by the first crystal of the room)
            if ((s->p0 == 0) && (sCrystalTimer[room] > 0)) {
                sCrystalTimer[room]--;
                if ((sCrystalTimer[room] % 20) == 0) {
                    Chime(NA_SE_SY_WARNING_COUNT_N);
                }
                if (sCrystalTimer[room] == 0) {
                    Chime(NA_SE_SY_ERROR);
                    for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
                        if ((sGdfTSpawns[i].room == room) && (sGdfTSpawns[i].type == T_CRYSTAL)) {
                            sLit[i] = 0;
                        }
                    }
                }
            }
            break;
        }
        case T_PILLAR: {
            s32 room = s->room;
            if (Flags_GetSwitch(play, s->p2)) {
                sLit[t->idx] = 1;
                break;
            }
            if (hit && !sLit[t->idx]) {
                if (s->p1 == sSimonStep[room]) {
                    sLit[t->idx] = 1;
                    sSimonStep[room]++;
                    Vec3f top = thisx->world.pos;
                    top.y += 240.0f;
                    Sparkle(play, &top, 40.0f, 12, sSage[s->p0]);
                    if (sSimonStep[room] >= 6) {
                        Solve(play, room);
                    } else {
                        Chime(NA_SE_SY_TRE_BOX_APPEAR);
                    }
                } else {
                    Chime(NA_SE_SY_ERROR);
                    sSimonStep[room] = 0;
                    for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
                        if ((sGdfTSpawns[i].room == room) && (sGdfTSpawns[i].type == T_PILLAR)) {
                            sLit[i] = 0;
                        }
                    }
                }
            }
            break;
        }
        case T_BEAMMIRROR:
            if (hit && (t->timer > 10)) {
                t->timer = 0;
                sMirrorState[t->idx] = (s8)((sMirrorState[t->idx] + 1) & 3);
                Audio_PlayActorSound2(thisx, NA_SE_EV_STONE_STATUE_OPEN);
            }
            thisx->shape.rot.y = (s16)(sMirrorState[t->idx] * 0x4000 + 0x2000);
            break;
        case T_EMITTER: {
            Seg segs[MAX_BEAM];
            bool lit;
            TraceBeam(t->idx, segs, &lit);
            if (lit && !sBeamOn[s->room]) {
                Actor* target = FindThing(play, s->room, T_TARGET);
                if (target != NULL) {
                    Sparkle(play, &target->world.pos, 50.0f, 20, sSage[5]);
                }
                Solve(play, s->room);
            }
            sBeamOn[s->room] = lit;
            break;
        }
        case T_SOCKET:
        case T_HEAVY: {
            // both together: the block on its socket, something heavy on the plate
            Actor* sock = FindThing(play, s->room, T_SOCKET);
            Actor* heavy = FindThing(play, s->room, T_HEAVY);
            Actor* block = FindThing(play, s->room, T_BLOCK);
            if ((type == T_SOCKET) && (sock != NULL) && (heavy != NULL) && (block != NULL)) {
                bool a = Math_Vec3f_DistXZ(&block->world.pos, &sock->world.pos) < 30.0f;
                bool b = HeavyOn(play, heavy, false) || (a && HeavyOn(play, heavy, true));
                // (he can't stand on the plate and push the block at once: the iron guard must be lured onto it,
                // unless the block is already home, when his own weight will do)
                t->state = a ? 1 : 0;
                ((GdfThing*)heavy)->state = HeavyOn(play, heavy, true) ? 1 : 0;
                if (a && b) {
                    Solve(play, s->room);
                }
            }
            break;
        }
        case T_BLOCK:
            if (t->slide > 0) {
                Vec3f next = thisx->world.pos;
                next.x += t->slideDir.x * 10.0f;
                next.z += t->slideDir.z * 10.0f;
                // stop at the room's tiles' edge (300 from the middle) or a wall
                const GdfTRoom* r = &sGdfTRooms[s->room];
                if ((fabsf(next.x - r->cx) > 200.5f) || (fabsf(next.z - r->cz) > 200.5f)) { // (the tiles' edge)
                    t->slide = 0;
                    Audio_PlayActorSound2(thisx, NA_SE_EV_BLOCK_BOUND);
                } else {
                    thisx->world.pos = next;
                    t->slide--;
                    Actor_PlaySfx_Flagged(thisx, NA_SE_EV_ROCK_SLIDE - SFX_FLAG);
                }
            }
            break;
        case T_EXIT:
            break;
    }
    if (t->hasCol) {
        Collider_UpdateCylinder(thisx, &t->col);
        CollisionCheck_SetAC(play, &play->colChkCtx, &t->col.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &t->col.base);
    }
}

static void PrimGlow(PlayState* play, Gfx** disp, u8 r, u8 g, u8 b, u8 a) {
    gDPSetPrimColor((*disp)++, 0, 0, r, g, b, a);
}

static void Thing_Draw(Actor* thisx, PlayState* play) {
    GdfThing* t = (GdfThing*)thisx;
    s32 type = Type(t);
    const GdfTSpawn* s = (type == T_LINKMIRROR) ? NULL : Spawn(t);
    f32 pulse = 0.75f + 0.25f * Math_SinS(t->timer * 0x500);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    switch (type) {
        case T_DOOR:
            if (t->open < 0.99f) {
                Draw(play, &POLY_OPA_DISP, DoorDL(s->p0));
            }
            break;
        case T_CHEST:
            if ((t->state != 0) || Flags_GetTreasure(play, s->p0)) {
                Draw(play, &POLY_OPA_DISP, DL("gGdfTChest"));
                Matrix_Translate(0.0f, 50.0f, -30.0f, MTXMODE_APPLY);
                Matrix_RotateX(-t->open * 1.9f, MTXMODE_APPLY);
                Draw(play, &POLY_OPA_DISP, DL("gGdfTChestLid"));
            }
            break;
        case T_BRIDGE:
        case T_LIGHTBRIDGE: {
            bool solid = BridgeSolid(play, s);
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            if (s->p0 == 1) {
                PrimGlow(play, &POLY_XLU_DISP, 120, 70, 200, solid ? (u8)(170 * pulse) : 30);
            } else {
                PrimGlow(play, &POLY_XLU_DISP, 255, 235, 150, solid ? (u8)(170 * pulse) : 30);
            }
            Draw(play, &POLY_XLU_DISP, DL("gGdfTBridge"));
            break;
        }
        case T_LIFT: {
            Draw(play, &POLY_OPA_DISP, DL("gGdfTLift"));
            // its mirror, on the dais in the middle
            Matrix_Translate(0.0f, 14.0f, 0.0f, MTXMODE_APPLY);
            Draw(play, &POLY_OPA_DISP, "__OTR__objects/gdf_sacred/gGdfMirrorFrameDL");
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            if (Shadow(play)) {
                PrimGlow(play, &POLY_XLU_DISP, 170, 110, 255, 200);
            } else {
                PrimGlow(play, &POLY_XLU_DISP, 255, 230, 140, 200);
            }
            Draw(play, &POLY_XLU_DISP, "__OTR__objects/gdf_sacred/gGdfMirrorGlassDL");
            break;
        }
        case T_MIRROR:
        case T_LINKMIRROR: {
            Draw(play, &POLY_OPA_DISP, "__OTR__objects/gdf_sacred/gGdfMirrorFrameDL");
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            u8 r = 255, g = 255, b = 255, a = 190;
            if (type == T_MIRROR) {
                Color_RGBA8 c = sSage[(t->timer / 40) % 6];
                r = c.r;
                g = c.g;
                b = c.b;
            } else {
                const GdfTMirror* m = &sGdfTMirrors[t->idx - T_LINKMIRROR];
                bool works = (m->kind == 0) && (m->pair >= 0) && ((m->aspect < 0) || ((m->aspect == 1) == Shadow(play)));
                if (!works) {
                    r = g = b = 90;
                    a = 120;
                } else if (m->aspect == 0) {
                    r = 255; g = 220; b = 120;
                } else if (m->aspect == 1) {
                    r = 160; g = 100; b = 255;
                } else {
                    r = 200; g = 240; b = 255;
                }
            }
            PrimGlow(play, &POLY_XLU_DISP, r, g, b, (u8)(a * pulse));
            Draw(play, &POLY_XLU_DISP, "__OTR__objects/gdf_sacred/gGdfMirrorGlassDL");
            break;
        }
        case T_PLATE: {
            Draw(play, &POLY_OPA_DISP, DL("gGdfTPlate"));
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            bool lit = Flags_GetSwitch(play, s->p2) || (s->p0 < sPlateStep[s->room]);
            PrimGlow(play, &POLY_XLU_DISP, 255, 220, 120, lit ? 230 : 60);
            Draw(play, &POLY_XLU_DISP, DL("gGdfTPlateGlow"));
            break;
        }
        case T_MURAL: {
            Draw(play, &POLY_OPA_DISP, DL("gGdfTMural"));
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            s32 n = s->p0;
            bool done = Flags_GetSwitch(play, s->p1);
            s32 beat = (t->timer / 30) % (n + 2);   // lights them one by one in the plates' order, then a pause
            static const char* sTri[5] = { DL("gGdfTMuralTri0"), DL("gGdfTMuralTri1"), DL("gGdfTMuralTri2"),
                                           DL("gGdfTMuralTri3"), DL("gGdfTMuralTri4") };
            // (each triangle's model stands in its own place along the mural; n of the five, centred)
            for (s32 i = 0; i < n; i++) {
                s32 tri = (s->p2 >> (3 * i)) & 7;
                bool lit = done || (i < beat);
                PrimGlow(play, &POLY_XLU_DISP, 255, 220, 120, lit ? 230 : 40);
                Draw(play, &POLY_XLU_DISP, sTri[CLAMP(tri + (5 - n) / 2, 0, 4)]);
            }
            break;
        }
        case T_BRAZIER:
            Draw(play, &POLY_OPA_DISP, "__OTR__objects/gdf_sacred/gGdfBrazierDL");
            break;
        case T_CRYSTAL:
            Draw(play, &POLY_OPA_DISP, DL("gGdfTCrystalBase"));
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            if (sLit[t->idx]) {
                PrimGlow(play, &POLY_XLU_DISP, 90, 140, 255, 220);
            } else {
                PrimGlow(play, &POLY_XLU_DISP, 255, 70, 60, (u8)(220 * pulse));
            }
            Matrix_RotateY(t->timer * 0.03f, MTXMODE_APPLY);
            Draw(play, &POLY_XLU_DISP, DL("gGdfTCrystal"));
            break;
        case T_PILLAR: {
            Draw(play, &POLY_OPA_DISP, DL("gGdfTPillar"));
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            Color_RGBA8 c = sSage[s->p0];
            PrimGlow(play, &POLY_XLU_DISP, c.r, c.g, c.b, sLit[t->idx] ? 240 : 70);
            Draw(play, &POLY_XLU_DISP, DL("gGdfTPillarGlow"));
            break;
        }
        case T_BEAMMIRROR:
            Draw(play, &POLY_OPA_DISP, DL("gGdfTBeamMirror"));
            break;
        case T_TARGET: {
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            bool lit = sBeamOn[s->room] || Flags_GetSwitch(play, s->p0);
            PrimGlow(play, &POLY_XLU_DISP, 255, 220, 100, lit ? 250 : 90);
            Draw(play, &POLY_XLU_DISP, DL("gGdfTSunEmblem"));
            break;
        }
        case T_EMITTER: {
            // the beam itself: each stretch a glowing bar from one point to the next
            Seg segs[MAX_BEAM];
            bool lit;
            s32 n = TraceBeam(t->idx, segs, &lit);
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            for (s32 i = 0; i < n; i++) {
                f32 dx = segs[i].b.x - segs[i].a.x, dz = segs[i].b.z - segs[i].a.z;
                f32 len = sqrtf(SQ(dx) + SQ(dz));
                if (len < 1.0f) {
                    continue;
                }
                Matrix_Translate(segs[i].a.x, segs[i].a.y, segs[i].a.z, MTXMODE_NEW);
                Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
                Matrix_Scale(1.0f, 1.0f, len, MTXMODE_APPLY);
                PrimGlow(play, &POLY_XLU_DISP, 255, 245, 200, (u8)(150 + 60 * pulse));
                Draw(play, &POLY_XLU_DISP, DL("gGdfTBeam"));
            }
            break;
        }
        case T_BLOCK:
            Draw(play, &POLY_OPA_DISP, DL("gGdfTBlock"));
            break;
        case T_SOCKET:
            Draw(play, &POLY_OPA_DISP, DL("gGdfTSocket"));
            break;
        case T_HEAVY:
            Matrix_Translate(0.0f, t->state ? -6.0f : 0.0f, 0.0f, MTXMODE_APPLY);
            Draw(play, &POLY_OPA_DISP, DL("gGdfTHeavy"));
            break;
        case T_SWITCH:
            Matrix_Translate(0.0f, (sBridgeTimer[s->room] > 0) ? -7.0f : 0.0f, 0.0f, MTXMODE_APPLY);
            Draw(play, &POLY_OPA_DISP, DL("gGdfTSwitch"));
            break;
        case T_INSCRIPTION:
            Draw(play, &POLY_OPA_DISP, DL("gGdfTTablet"));
            break;
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterThing() {
    if (sThingId >= 0) {
        return;
    }
    ActorDBInit i;
    i.name = "Gdf_TempleThing";
    i.desc = "The Temple of Light's doors, chests, mirrors and puzzles (Play as Ganondorf)";
    i.category = ACTORCAT_PROP;
    i.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    i.objectId = OBJECT_GAMEPLAY_KEEP;
    i.instanceSize = sizeof(GdfThing);
    i.init = Thing_Init;
    i.destroy = Thing_Destroy;
    i.update = Thing_Update;
    i.draw = Thing_Draw;
    sThingId = ActorDB::Instance->AddEntry(i).entry.id;
}

// ---- His kick (z_boss_ganon.c, through GanondorfSacred.cpp): key doors, the boss door, chests, the block ----

extern "C" s32 GdfTemple_IsKickable(Actor* a) {
    return (sThingId >= 0) && (a->id == sThingId) && (((GdfThing*)a)->idx < T_LINKMIRROR) &&
           ((sGdfTSpawns[((GdfThing*)a)->idx].type == T_CHEST) || (sGdfTSpawns[((GdfThing*)a)->idx].type == T_BLOCK) ||
            ((sGdfTSpawns[((GdfThing*)a)->idx].type == T_DOOR) && (sGdfTSpawns[((GdfThing*)a)->idx].p0 <= D_BOSS)));
}

extern "C" s32 GdfTemple_IsShut(Actor* a) {
    PlayState* play = gPlayState;
    GdfThing* t = (GdfThing*)a;
    const GdfTSpawn* s;

    if ((play == NULL) || !GdfTemple_IsKickable(a)) {
        return false;
    }
    s = Spawn(t);
    switch (s->type) {
        case T_DOOR:
            return !Flags_GetSwitch(play, s->p1);
        case T_CHEST:
            return (t->state != 0) && !Flags_GetTreasure(play, s->p0);
        case T_BLOCK:
            return t->slide == 0;
    }
    return false;
}

static const char* ItemText(s32 item, s32 keys) {
    static std::string sText;
    switch (item) {
        case IT_KEY:
            sText = "%rGanondorf:%w A small key. The Sages locked their own temple and left the keys lying about.";
            if (keys > 1) {
                sText += " That makes " + std::to_string(keys) + " of them.";
            }
            return sText.c_str();
        case IT_BOSS:
            return "%rGanondorf:%w The great key. Rauru waits behind the door it opens. Good. I would hate to keep an "
                   "old man waiting.";
        case IT_RUPEE20:
            return "%rGanondorf:%w Twenty rupees. Even the Sages could not resist hoarding.";
        case IT_RUPEE50:
            return "%rGanondorf:%w Fifty rupees. Tribute, long overdue.";
        case IT_RUPEE200:
            return "%rGanondorf:%w Two hundred rupees, hidden under the water. The Sages were misers.";
        case IT_HEART:
            return "%rGanondorf:%w A heart's worth of strength. It will not be needed. Still.";
        case IT_HEARTPIECE:
            return "%rGanondorf:%w A piece of a heart. Hidden where only someone who looked twice would find it.";
        case IT_MAGIC:
            return "%rGanondorf:%w Magic, bottled by the Sages for their own use. Mine now.";
    }
    return "";
}

extern "C" void GdfTemple_Kick(Actor* a, PlayState* play) {
    GdfThing* t = (GdfThing*)a;
    const GdfTSpawn* s = Spawn(t);
    Player* player = GET_PLAYER(play);

    switch (s->type) {
        case T_DOOR:
            if (s->p0 == D_KEY) {
                if (KeysHeld(play) > 0) {
                    Flags_SetSwitch(play, s->p1);
                    Audio_PlayActorSound2(a, NA_SE_EV_CHAIN_KEY_UNLOCK);
                } else {
                    Say(play, "%rGanondorf:%w Locked, and sealed besides. A small key would break the seal. Kicking it "
                              "harder will not.");
                }
            } else if (s->p0 == D_BOSS) {
                if (HasBossKey(play)) {
                    Flags_SetSwitch(play, s->p1);
                    Audio_PlayActorSound2(a, NA_SE_EV_CHAIN_KEY_UNLOCK_B);
                } else {
                    Say(play, "%rGanondorf:%w The great door. Its lock wants the great key, somewhere in this "
                              "temple.");
                }
            }
            break;
        case T_CHEST: {
            Flags_SetTreasure(play, s->p0);
            Audio_PlayActorSound2(a, NA_SE_EV_TBOX_OPEN);
            switch (s->p1) {
                case IT_RUPEE20:
                    Rupees_ChangeBy(20);
                    break;
                case IT_RUPEE50:
                    Rupees_ChangeBy(50);
                    break;
                case IT_RUPEE200:
                    Rupees_ChangeBy(200);
                    break;
                case IT_HEART:
                    Health_ChangeBy(play, 0x30);
                    break;
                case IT_HEARTPIECE:
                    Item_Give(play, ITEM_HEART_PIECE_2);
                    break;
                case IT_MAGIC:
                    if (gSaveContext.magicLevel > 0) {
                        gSaveContext.magic = (s8)gSaveContext.magicCapacity;
                    }
                    break;
            }
            BossGanon_Player_HoldUp(70);
            Audio_PlayFanfare(((s->p1 == IT_KEY) || (s->p1 >= IT_RUPEE20)) ? NA_BGM_SMALL_ITEM_GET : NA_BGM_ITEM_GET);
            Say(play, ItemText(s->p1, KeysHeld(play)));
            break;
        }
        case T_BLOCK: {
            s16 yaw = (s16)((player->actor.shape.rot.y + 0x2000) & 0xC000);  // (to the nearest of the four ways)
            t->slideDir = { Math_SinS(yaw), 0.0f, Math_CosS(yaw) };
            t->slide = 120;
            break;
        }
    }
}

// ---- Placing a room's things ----

static void SpawnRoom(PlayState* play, s32 room) {
    if ((sSpawned >> room) & 1) {
        return;
    }
    sSpawned |= (1ull << room);
    for (s32 i = 0; i < GDF_T_SPAWNS; i++) {
        const GdfTSpawn* s = &sGdfTSpawns[i];
        if (s->room != room) {
            continue;
        }
        if (s->type == T_GUARD) {
            // guards: only while the room's guards (or its puzzle) aren't beaten
            s32 f = sGdfTRooms[room].solveFlag;
            bool beaten = (f >= 0) && Flags_GetSwitch(play, f);
            if (!beaten || (s->p2 == 0)) {
                s16 params = s->p0;
                if (s->p0 == 4) {
                    params |= 0x10 | 0x20; // the Captain: waits till he's seen, then the mini-boss music
                }
                GdfSacred_SpawnGuard(play, (f32)s->x, (f32)s->y, (f32)s->z, s->yaw, params);
            }
            continue;
        }
        Actor_Spawn(&play->actorCtx, play, (s16)sThingId, (f32)s->x, (f32)s->y, (f32)s->z, 0, s->yaw, 0, (s16)i);
    }
    for (s32 i = 0; i < GDF_T_MIRRORS; i++) {
        const GdfTMirror* m = &sGdfTMirrors[i];
        if (m->room == room) {
            Actor_Spawn(&play->actorCtx, play, (s16)sThingId, (f32)m->x, (f32)m->y, (f32)m->z, 0, m->yaw, 0,
                        (s16)(T_LINKMIRROR + i));
        }
    }
    // the room's sunbeams (the Temple of Time's light plane): their size kept in their x rotation
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != NULL; a = a->next) {
        if ((a->id == ACTOR_DEMO_KANKYO) && (a->params == 0x0E) && (a->home.rot.x > 0)) {
            Actor_SetScale(a, a->home.rot.x / 10000.0f);
            a->world.rot.x = a->shape.rot.x = 0;
        }
    }
}

// Combat rooms: beaten when none of their guards still stand
static void CheckGuards(PlayState* play, s32 room) {
    s32 f = sGdfTRooms[room].solveFlag;
    s32 kind = sGdfTRooms[room].kind;
    if ((f < 0) || ((kind != K_COMBAT) && (kind != K_MINIBOSS)) || Flags_GetSwitch(play, f)) {
        return;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        if ((a->id == GdfSacred_GuardId()) && (a->room == room) && (a->colChkInfo.health > 0) && (a->update != NULL)) {
            return;
        }
    }
    Solve(play, room);
    if (kind == K_MINIBOSS) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS_CLEAR);
    }
}

// The light: each floor's own setting, darker in Shadow (the scene's settings 0..5)
static void SetLight(PlayState* play) {
    s32 room = play->roomCtx.curRoom.num;
    if ((room < 0) || (room >= GDF_T_ROOMS)) {
        return;
    }
    s32 floor = sGdfTRooms[room].floor;
    if (floor == 3) { // the atrium: by the level he's on
        f32 y = GET_PLAYER(play)->actor.world.pos.y;
        floor = (y < -1200.0f) ? 1 : (y > 1200.0f) ? 2 : 0;
    }
    play->envCtx.unk_BF = (u8)(floor * 2 + (Shadow(play) ? 1 : 0));
}

// The temple's music: the converted MIDI (custom/music/...), found by its name among the game's sequences
static s32 FindTempleSeq() {
    for (size_t i = 0; i < sequenceMapSize; i++) {
        if ((sequenceMap[i] != NULL) && (strstr(sequenceMap[i], "Temple of Light") != NULL)) {
            return (s32)i;
        }
    }
    return -1;
}

static void OnSceneInit(int16_t sceneNum) {
    PlayState* play = gPlayState;
    sSpawned = 0;
    sLastCur = -1;
    sTeleport = 0;
    sFlash = 0;
    if ((play == NULL) || (sceneNum != TEMPLE_SCENE) || !Available()) {
        return;
    }
    RegisterThing();
    memset(sPlateStep, 0, sizeof(sPlateStep));
    memset(sSimonStep, 0, sizeof(sSimonStep));
    memset(sCrystalTimer, 0, sizeof(sCrystalTimer));
    memset(sBridgeTimer, 0, sizeof(sBridgeTimer));
    memset(sLit, 0, sizeof(sLit));
    memset(sBeamOn, 0, sizeof(sBeamOn));
    s32 seq = FindTempleSeq();
    if (seq >= 0) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | (seq & 0xFFFF));
    }
}

// Back from Rauru's arena: into the antechamber
extern "C" void GdfTemple_ArriveFromRauru(void) {
    sArrivedFromRauru = true;
    sArrivalStage = 0;
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;
    if (!InTemple(play)) {
        return;
    }
    Player* player = GET_PLAYER(play);
    RoomContext* rc = &play->roomCtx;
    s32 cur = rc->curRoom.num;

    if (sMirrorCooldown > 0) {
        sMirrorCooldown--;
    }
    // the rooms that are out: their things placed; rooms gone from memory, forgotten (their actors went with them)
    for (s32 r = 0; r < GDF_T_ROOMS; r++) {
        if (((sSpawned >> r) & 1) && (r != cur) && (r != rc->prevRoom.num)) {
            sSpawned &= ~(1ull << r);
        }
    }
    if ((cur >= 0) && (cur < GDF_T_ROOMS) && (rc->status == 0)) {
        SpawnRoom(play, cur);
        if (cur != sLastCur) {
            sLastCur = (s16)cur;
            sEntryPos = player->actor.world.pos;
            sEntryYaw = player->actor.shape.rot.y;
            // (arriving on the far side of a broken floor, or mid-air: the room's own safe spot)
            if (player->actor.world.pos.y < sGdfTRooms[cur].y - 50.0f) {
                sEntryPos = { (f32)sGdfTRooms[cur].rx, (f32)sGdfTRooms[cur].ry, (f32)sGdfTRooms[cur].rz };
            }
        }
        CheckGuards(play, cur);
    }
    SetLight(play);
    // the flash of the temple turning
    if (sFlash > 0) {
        sFlash--;
        play->envCtx.fillScreen = true;
        play->envCtx.screenFillColor[0] = 255;
        play->envCtx.screenFillColor[1] = 255;
        play->envCtx.screenFillColor[2] = 255;
        play->envCtx.screenFillColor[3] = (u8)(sFlash * 6);
        if (sFlash == 0) {
            play->envCtx.fillScreen = false;
        }
    }
    // back from Rauru: the antechamber
    if (sArrivedFromRauru) {
        if (sArrivalStage == 0) {
            if (Room_RequestNewRoom(play, rc, GDF_T_ANTE)) {
                sArrivalStage = 1;
            }
        } else if (func_800973FC(play, rc)) {
            const GdfTRoom* a = &sGdfTRooms[GDF_T_ANTE];
            Room_FinishRoomChange(play, rc);
            player->actor.world.pos = { (f32)a->cx, (f32)a->y, (f32)a->cz - 380.0f };
            player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = -0x8000;
            sArrivedFromRauru = false;
        }
        return;
    }
    // a mirror's journey
    if (sTeleport != 0) {
        const GdfTMirror* to = &sGdfTMirrors[sTeleportTo];
        sTeleportTimer++;
        if (sTeleport == 1) {
            play->envCtx.fillScreen = true;
            play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] = 255;
            play->envCtx.screenFillColor[3] = (u8)MIN(255, sTeleportTimer * 20);
            player->actor.velocity.y = 0.0f;
            player->linearVelocity = 0.0f;
            if ((sTeleportTimer >= 14) && Room_RequestNewRoom(play, rc, to->room)) {
                sTeleport = 2;
            }
        } else if (sTeleport == 2) {
            if (func_800973FC(play, rc)) {
                Room_FinishRoomChange(play, rc);
                f32 fx = Math_SinS(to->yaw) * 90.0f, fz = Math_CosS(to->yaw) * 90.0f;
                player->actor.world.pos = { (f32)to->x + fx, (f32)to->y, (f32)to->z + fz };
                player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
                player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = to->yaw;
                sTeleport = 3;
                sTeleportTimer = 0;
            }
        } else {
            play->envCtx.screenFillColor[3] = (u8)MAX(0, 255 - sTeleportTimer * 20);
            if (sTeleportTimer > 14) {
                play->envCtx.fillScreen = false;
                sTeleport = 0;
            }
        }
        return;
    }
    // falls: half a heart, and back where he came into the room
    if ((cur >= 0) && (cur < GDF_T_ROOMS) && (player->actor.world.pos.y < sGdfTRooms[cur].y - 450.0f) &&
        !(cur == GDF_T_ATRIUM && player->actor.world.pos.y > -2900.0f)) {
        Health_ChangeBy(play, -8);
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
        player->actor.world.pos = sEntryPos;
        player->actor.prevPos = player->actor.home.pos = sEntryPos;
        player->actor.velocity.y = 0.0f;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = sEntryYaw;
    }
    // the antechamber's far end: on to Rauru
    if ((cur == GDF_T_ANTE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        (player->actor.world.pos.z < sGdfTRooms[GDF_T_ANTE].cz - 760.0f)) {
        play->nextEntranceIndex = RAURU_ENTRANCE;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        gSaveContext.respawnFlag = 0;
        gSaveContext.cutsceneIndex = 0;
    }
}

static void RegisterLightTemple() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterLightTemple, {});
