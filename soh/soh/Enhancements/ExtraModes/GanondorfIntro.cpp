// SoH: "Play as Ganondorf", the Gerudo intro.
//
// A cutscene of its own for files that start in the Gerudo Fortress (Ganondorf at the height of his power, in the
// desert, before Hyrule ever hears of him), and replayable from the menu. There is no cutscene data behind it: this
// director runs it shot by shot through real scenes. It loads each scene, places Link (Ganondorf follows him, or rides
// the black horse it gives him), moves its own camera, spawns the Gerudo, horses and torches, cues Ganondorf's arm
// poses and the props he draws (gGdfIntroCs), and shows the dialogue. The script waits while a text box is open, so
// the dialogue goes at the player's pace. START skips it.
//
// Shots:
//   1 The desert at dusk, the camera coming down on the fortress gate. A door slams. Black, and his voice.
//   2 The hideout's torchlit halls, the Gerudo kneeling as the camera passes, to the doors.
//   3 The tower room: Ganondorf at the window, the scouts' report.
//   4 The war table: the map of Hyrule, his black stone, the captain, "power can conquer the world". The cold.
//   5 The window, and the vision: a golden light in the void, the three triangles, a castle in flames, a boy
//     beneath a great tree. Back at the window, "It will belong to me."
//   6 Night: the courtyard of torches, the oath, the chant, the lightning.
//   7 The messenger and the king's letter. "A king should always look his enemy in the eye."
//   8 He rides out of the fortress, two riders behind him, the camera rising over the walls.
//   9 Hyrule Field, the Deku Tree, a boy asleep in his house, and Ganondorf on a rise looking at the castle.

#include <cmath>
#include <string>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "seqcmd.h"
#include "PlayAsGanondorf.h"
#include "GanondorfIntro.h"
#include "overlays/actors/ovl_En_GeldB/z_en_geldb.h"
#include "overlays/actors/ovl_En_Horse_Ganon/z_en_horse_ganon.h"
extern PlayState* gPlayState;
extern LightningStrike gLightningStrike;
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
}

GdfIntroCs gGdfIntroCs;

#define CVAR_GDF(name) CVAR_ENHANCEMENT("PlayAsGanondorf." name)

// ---- Text ----

enum {
    TX_VO_DESERT = 0x7E10,
    TX_GUARD_SCOUTS,
    TX_G_SEEN,
    TX_GUARD_ROADS,
    TX_G_TIME,
    TX_G_KING,
    TX_G_GORONS,
    TX_G_ZORAS,
    TX_G_KOKIRI,
    TX_G_BOW,
    TX_CAPTAIN,
    TX_G_ARMY,
    TX_G_POWER,
    TX_G_WHAT,
    TX_G_BEYOND,
    TX_G_MINE,
    TX_G_TONIGHT,
    TX_G_TOMORROW,
    TX_CHANT,
    TX_G_WAIT,
    TX_G_TAKE,
    TX_SCOUT_MESSENGER,
    TX_G_SPEAK,
    TX_G_HORSE,
    TX_SCOUT_YOURSELF,
    TX_G_COURSE,
    TX_G_EYE,
    TX_VO_KING,
    TX_LAST
};

#define GDF_TEXT_POE_GREETING 0x7E01

struct IntroLine {
    u16 id;
    const char* text;
    u8 box; // 0: dialogue box at the bottom, 1: his voice over (no box), 2: no box, closes by itself
};

// Ganondorf's lines in red, the Gerudo's in yellow
static const IntroLine sLines[] = {
    { TX_VO_DESERT,
      "For generations, the desert has endured.^We have watched the green fields flourish... while our people were left "
      "to the mercy of the sands.",
      1 },
    { TX_GUARD_SCOUTS, "%yGerudo Guard%w&My king. The scouts have returned from Hyrule Field.", 0 },
    { TX_G_SEEN, "%rGanondorf%w&And what have they seen?", 0 },
    { TX_GUARD_ROADS,
      "%yGerudo Guard%w&The roads are open. The castle remains guarded, but the king has summoned the leaders of the "
      "surrounding lands.",
      0 },
    { TX_G_TIME, "%rGanondorf%w&Then the time has come.", 0 },
    { TX_G_KING,
      "%rGanondorf%w&The King of Hyrule believes peace is maintained by keeping every kingdom beneath his hand.", 0 },
    { TX_G_GORONS, "%rGanondorf%w&The Gorons cling to their mountain.", 0 },
    { TX_G_ZORAS, "%rGanondorf%w&The Zoras hide behind their waters.", 0 },
    { TX_G_KOKIRI, "%rGanondorf%w&The Kokiri remain prisoners of their woods.", 0 },
    { TX_G_BOW, "%rGanondorf%w&And the Gerudo are expected to bow.", 0 },
    { TX_CAPTAIN, "%yGerudo Captain%w&Give the word, my king. We will ride for Hyrule Castle before sunrise.", 0 },
    { TX_G_ARMY, "%rGanondorf%w&No. An army can conquer a kingdom.", 0 },
    { TX_G_POWER, "%rGanondorf%w&But power can conquer the world.", 0 },
    { TX_G_WHAT, "%rGanondorf%w&%c(whispering)%w What was that...?", 0 },
    { TX_G_BEYOND, "%rGanondorf%w&Whatever power lies beyond this world...", 0 },
    { TX_G_MINE, "%rGanondorf%w&It will belong to me.", 0 },
    { TX_G_TONIGHT, "%rGanondorf%w&Tonight, we stand in the desert.", 0 },
    { TX_G_TOMORROW, "%rGanondorf%w&Tomorrow, Hyrule will remember our name.", 0 },
    { TX_CHANT, "%yGanondorf! Ganondorf! Ganondorf!%w", 2 },
    { TX_G_WAIT,
      "%rGanondorf%w&We will not march blindly into war. We will watch. We will wait.^%rGanondorf%w&And when the "
      "moment comes...",
      0 },
    { TX_G_TAKE, "%rGanondorf%w&We will take what has been denied to us.", 0 },
    { TX_SCOUT_MESSENGER, "%yGerudo Scout%w&My king! A messenger from Hyrule has arrived.", 0 },
    { TX_G_SPEAK, "%rGanondorf%w&So the king wishes to speak with me.", 0 },
    { TX_G_HORSE, "%rGanondorf%w&Prepare my horse.", 0 },
    { TX_SCOUT_YOURSELF, "%yGerudo Scout%w&You intend to travel to Hyrule Castle yourself?", 0 },
    { TX_G_COURSE, "%rGanondorf%w&Of course.", 0 },
    { TX_G_EYE, "%rGanondorf%w&A king should always look his enemy in the eye.", 0 },
    { TX_VO_KING, "The world has already chosen its rulers.^Now... it will learn to choose its king.", 1 },
};

static void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if ((*textId < TX_VO_DESERT) || (*textId >= TX_LAST)) {
        return;
    }
    for (const IntroLine& line : sLines) {
        if (line.id != *textId) {
            continue;
        }
        CustomMessage msg = CustomMessage(line.text);
        if (line.box == 0) {
            msg.SetTextBoxType(TEXTBOX_TYPE_BLACK);
            msg.SetTextBoxPosition(TEXTBOX_POS_BOTTOM);
        } else {
            msg.SetTextBoxType(TEXTBOX_TYPE_NONE_BOTTOM);
            msg.SetTextBoxPosition((line.box == 1) ? TEXTBOX_POS_MIDDLE : TEXTBOX_POS_TOP);
        }
        msg.AutoFormat();
        if (line.box == 2) {
            // Closes by itself after 3 seconds (the fade code before the end)
            msg.Replace("\x02", std::string("\x0E\x3C\x02", 3));
        }
        *loadFromMessageTable = false;
        msg.LoadIntoFont();
        return;
    }
}

// ---- State ----

enum Trans : u8 { TR_BLACK, TR_WHITE, TR_CUT, TR_FLASH };

struct Shot {
    s16 entrance; // -1: the same scene as the shot before
    u16 dayTime;
    u8 trans;
    bool (*setup)(PlayState* play); // true once it's ready (objects loaded, actors spawned)
    bool (*update)(PlayState* play, s32 t);
};

enum Phase : u8 { PH_IDLE, PH_FADE_OUT, PH_LOADING, PH_SETUP, PH_RUN, PH_ENDING, PH_END_LOADING };

static bool sActive = false;
static bool sFromNewFile = false;
static Phase sPhase = PH_IDLE;
static s32 sShot = 0;
static s32 sT = 0;
static s32 sLastRunT = -1;
static bool sFresh = false;
static bool sMsgFreeze = false;
static s32 sPhaseTimer = 0;
static s16 sLoadScene = -1;
static s16 sCamId = SUBCAM_NONE;
static u16 sDayTime = 0;
static s16 sReturnEntrance = 0;
static u16 sReturnDayTime = 0;
static u8 sFillR = 0, sFillG = 0, sFillB = 0;
static f32 sFillA = 0.0f;
static f32 sFillTarget = 0.0f;
static f32 sFillStep = 0.0f;
static f32 sCold = 0.0f;
static f32 sColdTarget = 0.0f;
static bool sDekuMetBefore = false;
static bool sDekuFlagSaved = false;
static s32 sGreetTimer = -1;
static ActorFunc sNaviDraw = NULL;

// Link (Ganondorf follows him)
static Vec3f sLinkPos;
static s16 sLinkYaw = 0;
static f32 sLinkSpeed = 0.0f;
static bool sLinkPinned = false;
static u8 sLinkCsAction = 1;

// Spawned for the shot
static std::vector<Actor*> sExtras;
static Actor* sHorse = NULL;
static Actor* sEscortHorse[2] = { NULL, NULL };
static Actor* sEscort[2] = { NULL, NULL };
static Actor* sScout = NULL;
static Actor* sCaptain = NULL;

static Vec3f V(f32 x, f32 y, f32 z) {
    Vec3f v = { x, y, z };
    return v;
}

static Vec3f Lerp(Vec3f a, Vec3f b, f32 k) {
    return V(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k);
}

static f32 Smooth(s32 t, s32 t0, s32 t1) {
    f32 k = (t1 > t0) ? CLAMP((f32)(t - t0) / (f32)(t1 - t0), 0.0f, 1.0f) : 1.0f;
    return k * k * (3.0f - 2.0f * k);
}

static bool At(s32 t, s32 n) {
    return sFresh && (t == n);
}

// ---- Camera ----

static void Cam(PlayState* play, Vec3f eye, Vec3f at, f32 fov = 60.0f) {
    if ((sCamId == SUBCAM_NONE) || (Play_GetCamera(play, sCamId) == NULL)) {
        sCamId = Play_CreateSubCamera(play);
        if (sCamId == SUBCAM_NONE) {
            return;
        }
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, sCamId, CAM_STAT_ACTIVE);
    }
    Play_CameraSetAtEye(play, sCamId, &at, &eye);
    Play_CameraSetFov(play, sCamId, fov);
    gGdfIntroCs.cameraEye = eye;
}

static void CamMove(PlayState* play, s32 t, s32 t0, s32 t1, Vec3f e0, Vec3f a0, Vec3f e1, Vec3f a1,
                    f32 fov = 60.0f) {
    f32 k = Smooth(t, t0, t1);
    Cam(play, Lerp(e0, e1, k), Lerp(a0, a1, k), fov);
}

static void ReleaseCam(PlayState* play) {
    if ((sCamId != SUBCAM_NONE) && (Play_GetCamera(play, sCamId) != NULL)) {
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        Play_ClearCamera(play, sCamId);
    }
    sCamId = SUBCAM_NONE;
}

// ---- Helpers ----

static void Say(PlayState* play, u16 id, bool freeze = true) {
    Message_StartTextbox(play, id, NULL);
    sMsgFreeze = freeze;
}

static void Sfx(u16 id) {
    Sfx_PlaySfxCentered(id);
}

static void Music(u16 seq, u8 fadeIn) {
    SEQCMD_PLAY_SEQUENCE(SEQ_PLAYER_BGM_MAIN, fadeIn, 0, seq);
}

static void MusicStop(u8 fadeOut) {
    SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_BGM_MAIN, fadeOut);
}

static f32 FloorAt(PlayState* play, f32 x, f32 y, f32 z) {
    CollisionPoly* poly;
    Vec3f probe = V(x, y + 60.0f, z);
    f32 floor = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);

    return (floor > BGCHECK_Y_MIN) ? floor : y;
}

static void PlaceLink(PlayState* play, Vec3f pos, s16 yaw) {
    sLinkPos = pos;
    sLinkPos.y = FloorAt(play, pos.x, pos.y, pos.z);
    sLinkYaw = yaw;
    sLinkSpeed = 0.0f;
    sLinkPinned = true;
}

// Walks Link toward `to` (xz) at `speed`: true once there
static bool WalkLink(PlayState* play, Vec3f to, f32 speed) {
    f32 dx = to.x - sLinkPos.x;
    f32 dz = to.z - sLinkPos.z;
    f32 dist = sqrtf(SQ(dx) + SQ(dz));

    if (dist <= speed) {
        sLinkPos.x = to.x;
        sLinkPos.z = to.z;
        sLinkSpeed = 0.0f;
        return true;
    }
    sLinkYaw = Math_Atan2S(dz, dx);
    sLinkPos.x += dx / dist * speed;
    sLinkPos.z += dz / dist * speed;
    sLinkPos.y = FloorAt(play, sLinkPos.x, sLinkPos.y, sLinkPos.z);
    sLinkSpeed = speed;
    return false;
}

static void TurnLink(s16 yaw, s16 step) {
    Math_ScaledStepToS(&sLinkYaw, yaw, step);
}

static void Fill(u8 r, u8 g, u8 b, f32 a) {
    sFillR = r;
    sFillG = g;
    sFillB = b;
    sFillA = sFillTarget = a;
    sFillStep = 0.0f;
}

static void FillTo(f32 a, f32 step) {
    sFillTarget = a;
    sFillStep = step;
}

static bool EnsureObject(PlayState* play, s16 objectId) {
    s32 index = Object_GetIndex(&play->objectCtx, objectId);

    if (index < 0) {
        Object_Spawn(&play->objectCtx, objectId);
        return false;
    }
    return Object_IsLoaded(&play->objectCtx, index);
}

// A Gerudo for the scene (EnGeldB as an extra): colour 0 green, 1 blue, 2 red
static Actor* SpawnGerudo(PlayState* play, Vec3f pos, s16 yaw, s32 colour) {
    f32 y = FloorAt(play, pos.x, pos.y, pos.z);
    Actor* a = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_GELDB, pos.x, y, pos.z, 0, yaw, 0,
                           EN_GELDB_PARAMS_EXTRA + (colour % 3));

    if (a != NULL) {
        a->world.rot.y = a->shape.rot.y = yaw;
        sExtras.push_back(a);
    }
    return a;
}

static Actor* SpawnHorse(PlayState* play, Vec3f pos, s16 yaw) {
    f32 y = FloorAt(play, pos.x, pos.y, pos.z);
    Actor* a = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE_GANON, pos.x, y, pos.z, 0, yaw, 0, 0);

    if (a != NULL) {
        a->world.rot.y = a->shape.rot.y = yaw;
        a->room = -1;
        EnHorseGanon_SetDriven(a);
    }
    return a;
}

static void SpawnTorch(PlayState* play, Vec3f pos) {
    f32 y = FloorAt(play, pos.x, pos.y, pos.z);

    Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_SYOKUDAI, pos.x, y, pos.z, 0, 0, 0, 0x2400);
}

// Rides a driven horse along a path of points (xz), speeding up to `speed`: the index it's heading for
static s32 RideAlong(PlayState* play, Actor* horse, const Vec3f* path, s32 count, s32* index, f32 speed, f32 accel) {
    if ((horse == NULL) || (horse->update == NULL)) {
        return count;
    }
    if (*index >= count) {
        Math_ApproachZeroF(&horse->speedXZ, 1.0f, accel * 2.0f);
        return count;
    }
    f32 dx = path[*index].x - horse->world.pos.x;
    f32 dz = path[*index].z - horse->world.pos.z;

    if (sqrtf(SQ(dx) + SQ(dz)) < 60.0f) {
        (*index)++;
        return *index;
    }
    Math_SmoothStepToS(&horse->world.rot.y, Math_Atan2S(dz, dx), 4, 0x400, 0x40);
    horse->shape.rot.y = horse->world.rot.y;
    Math_ApproachF(&horse->speedXZ, speed, 1.0f, accel);
    return *index;
}

// A Gerudo riding behind him: on her horse's saddle, crouched
static void SeatRider(Actor* rider, Actor* horse) {
    Vec3f seat;

    if ((rider == NULL) || (horse == NULL) || (rider->update == NULL) || !EnHorseGanon_GetSeat(horse, &seat)) {
        return;
    }
    rider->world.pos = seat;
    rider->world.pos.y -= 28.0f;
    rider->prevPos = rider->world.pos;
    rider->gravity = 0.0f;
    rider->velocity.y = 0.0f;
    rider->speedXZ = 0.0f;
    rider->world.rot.y = rider->shape.rot.y = horse->shape.rot.y;
}

// Where a texel of the map (216x128) is on the war table
static Vec3f MapPoint(f32 u, f32 v) {
    GdfIntroCs* cs = &gGdfIntroCs;
    f32 lx = -GDF_TABLE_W / 2.0f + (u / 216.0f) * GDF_TABLE_W;
    f32 lz = -GDF_TABLE_D / 2.0f + (v / 128.0f) * GDF_TABLE_D;
    s16 rot = cs->tableYaw + 0x8000;
    f32 s = Math_SinS(rot);
    f32 c = Math_CosS(rot);

    return V(cs->tablePos.x + lx * c + lz * s, cs->tablePos.y + 1.0f, cs->tablePos.z - lx * s + lz * c);
}

static void ResetProps() {
    GdfIntroCs* cs = &gGdfIntroCs;

    cs->pose = GDF_CSPOSE_NONE;
    cs->openHand = false;
    cs->hideAll = false;
    cs->showLink = false;
    cs->headYaw = cs->headPitch = 0.0f;
    cs->handGlow = 0.0f;
    cs->horse = NULL;
    cs->voidAlpha = 0;
    cs->goldLight = 0.0f;
    cs->triforceScale = 0.0f;
    cs->triforceAlpha = 0;
    cs->tableOn = false;
    cs->stoneOn = false;
    cs->letterOn = false;
    cs->ocarinaOn = false;
}

// A new shot in the same scene, or a new scene: what the last shot spawned goes
static void ClearShotActors() {
    for (Actor* a : sExtras) {
        if (a != NULL) {
            Actor_Kill(a);
        }
    }
    sExtras.clear();
    for (Actor** h : { &sHorse, &sEscortHorse[0], &sEscortHorse[1] }) {
        if (*h != NULL) {
            Actor_Kill(*h);
        }
        *h = NULL;
    }
    sEscort[0] = sEscort[1] = sScout = sCaptain = NULL;
    gEnGeldBExtraCue = GELDB_EXTRA_STAND;
    gEnGeldBExtraLookOn = false;
}

// Anything that would get in the way: enemies (and their spawners), the owl, the scene's own Gerudo and carpenters
static void ClearScene(PlayState* play) {
    static const s16 sAlwaysKill[] = { ACTOR_EN_OWL, ACTOR_EN_ENCOUNT1, ACTOR_EN_GE2, ACTOR_EN_DAIKU, ACTOR_EN_KO,
                                       ACTOR_EN_MD, ACTOR_EN_SA, ACTOR_EN_ITEM00 };

    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        if (!((a->id == ACTOR_EN_GELDB) && EN_GELDB_IS_FRIEND(a->params))) {
            Actor_Kill(a);
        }
    }
    for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != NULL; a = a->next) {
            for (s16 id : sAlwaysKill) {
                if (a->id == id) {
                    Actor_Kill(a);
                }
            }
            if ((a->id == ACTOR_EN_GELDB) && !EN_GELDB_IS_FRIEND(a->params)) {
                Actor_Kill(a);
            }
        }
    }
}

// ---- Shot 1: the desert at dusk ----

static bool Setup1(PlayState* play) {
    if (!EnsureObject(play, OBJECT_GELDB)) {
        return false;
    }
    gGdfIntroCs.hideAll = true;
    PlaceLink(play, V(0.0f, 333.0f, -2000.0f), 0);
    SpawnGerudo(play, V(-560.0f, 333.0f, -1500.0f), -0x4000, 2);
    SpawnGerudo(play, V(-560.0f, 333.0f, -1950.0f), -0x4000, 1);
    SpawnGerudo(play, V(-150.0f, 333.0f, -2450.0f), -0x6000, 0);
    MusicStop(0);
    return true;
}

static bool Update1(PlayState* play, s32 t) {
    CamMove(play, t, 0, 210, V(-900.0f, 2200.0f, 2600.0f), V(-900.0f, 300.0f, -1500.0f),
            V(-1250.0f, 450.0f, -1900.0f), V(-1150.0f, 150.0f, -3150.0f));
    if (At(t, 210)) {
        Sfx(NA_SE_EV_STONEDOOR_STOP);
        Actor_RequestQuake(play, 2, 6);
    }
    if (At(t, 212)) {
        Fill(0, 0, 0, 255.0f); // cut to black
    }
    if (At(t, 220)) {
        Music(NA_BGM_TITLE, 90);
    }
    if (At(t, 240)) {
        Say(play, TX_VO_DESERT);
    }
    return t >= 250;
}

// ---- Shot 2: the hideout ----

static const Vec3f sHallPath[] = {
    { 1060.0f, 75.0f, -1990.0f }, { 1060.0f, 75.0f, -2290.0f }, { 1300.0f, 75.0f, -2325.0f },
    { 1365.0f, 75.0f, -2420.0f }, { 1365.0f, 75.0f, -2760.0f }, { 1500.0f, 75.0f, -2790.0f },
    { 1780.0f, 75.0f, -2880.0f },
};

static Vec3f PathAt(const Vec3f* path, s32 count, f32 dist) {
    for (s32 i = 0; i < count - 1; i++) {
        Vec3f a = path[i];
        Vec3f b = path[i + 1];
        f32 len = Math_Vec3f_DistXYZ(&a, &b);

        if (dist <= len) {
            return Lerp(path[i], path[i + 1], dist / len);
        }
        dist -= len;
    }
    return path[count - 1];
}

static bool Setup2(PlayState* play) {
    if (!EnsureObject(play, OBJECT_GELDB)) {
        return false;
    }
    gGdfIntroCs.hideAll = true;
    PlaceLink(play, V(1060.0f, 0.0f, -1960.0f), 0);
    // Kneeling along the walls as the camera goes by
    SpawnGerudo(play, V(1018.0f, 0.0f, -2100.0f), 0x4000, 0);
    SpawnGerudo(play, V(1102.0f, 0.0f, -2170.0f), -0x4000, 1);
    SpawnGerudo(play, V(1180.0f, 0.0f, -2265.0f), -0x8000, 2);
    SpawnGerudo(play, V(1250.0f, 0.0f, -2385.0f), 0, 0);
    SpawnGerudo(play, V(1312.0f, 0.0f, -2500.0f), 0x4000, 1);
    SpawnGerudo(play, V(1418.0f, 0.0f, -2600.0f), -0x4000, 2);
    SpawnGerudo(play, V(1312.0f, 0.0f, -2700.0f), 0x4000, 0);
    SpawnGerudo(play, V(1700.0f, 0.0f, -2760.0f), -0x6000, 1);
    SpawnGerudo(play, V(1760.0f, 0.0f, -3040.0f), 0x7000, 2);
    gEnGeldBExtraCue = GELDB_EXTRA_KNEEL;
    return true;
}

static bool Update2(PlayState* play, s32 t) {
    const s32 count = ARRAY_COUNT(sHallPath);
    f32 dist = 5.5f * MIN(t, 262);
    Vec3f eye = PathAt(sHallPath, count, dist);
    Vec3f at = PathAt(sHallPath, count, dist + 140.0f);
    f32 k = Smooth(t, 220, 262);

    at.y -= 8.0f;
    at = Lerp(at, V(2136.0f, 70.0f, -2980.0f), k);
    eye.y += Math_SinS(t * 0x0C00) * 1.5f; // a walker's bob
    Cam(play, eye, at, 65.0f);
    if (((t % 14) == 0) && (t < 262) && sFresh) {
        Sfx(NA_SE_PL_WALK_HEAVYBOOTS);
    }
    if (At(t, 275)) {
        Sfx(NA_SE_OC_DOOR_OPEN);
    }
    return t >= 282;
}

// ---- Shot 3: the window ----

static const Vec3f sWindowSpot = { 90.0f, 1113.0f, -2690.0f };

static bool Setup3(PlayState* play) {
    if (!EnsureObject(play, OBJECT_GELDB)) {
        return false;
    }
    PlaceLink(play, sWindowSpot, -0x4000);
    gGdfIntroCs.pose = GDF_CSPOSE_WINDOW;
    Actor* guard = SpawnGerudo(play, V(330.0f, 1113.0f, -2760.0f), -0x3466, 2);
    (void)guard;
    gEnGeldBExtraLook = sWindowSpot;
    gEnGeldBExtraLookOn = true;
    return true;
}

static bool Update3(PlayState* play, s32 t) {
    if (t < 45) {
        CamMove(play, t, 0, 45, V(360.0f, 1185.0f, -2790.0f), V(70.0f, 1170.0f, -2690.0f),
                V(260.0f, 1180.0f, -2750.0f), V(60.0f, 1175.0f, -2690.0f));
    } else {
        // His face, as he turns his head toward her
        CamMove(play, t, 45, 90, V(30.0f, 1212.0f, -2612.0f), V(95.0f, 1197.0f, -2695.0f),
                V(42.0f, 1208.0f, -2628.0f), V(95.0f, 1197.0f, -2695.0f), 55.0f);
    }
    if (At(t, 20)) {
        Say(play, TX_GUARD_SCOUTS);
    }
    if (At(t, 26)) {
        Say(play, TX_G_SEEN);
    }
    if (At(t, 32)) {
        Say(play, TX_GUARD_ROADS);
    }
    if (At(t, 50)) {
        gGdfIntroCs.headYaw = -55.0f;
    }
    if (At(t, 80)) {
        Say(play, TX_G_TIME);
    }
    return t >= 95;
}

// ---- Shot 4: the war table ----

static const Vec3f sTablePos = { 560.0f, 640.0f, -2560.0f };
static const Vec3f sTableSpot = { 640.0f, 640.0f, -2560.0f };

static bool Setup4(PlayState* play) {
    GdfIntroCs* cs = &gGdfIntroCs;

    if (!EnsureObject(play, OBJECT_GELDB)) {
        return false;
    }
    PlaceLink(play, sTableSpot, -0x4000);
    cs->tableOn = true;
    cs->tablePos = sTablePos;
    cs->tablePos.y = FloorAt(play, sTablePos.x, sTablePos.y, sTablePos.z) + GDF_TABLE_H;
    cs->tableYaw = -0x4000;
    cs->stoneU = 120.0f;
    cs->stoneV = 30.0f;
    sCaptain = SpawnGerudo(play, V(472.0f, 640.0f, -2560.0f), 0x4000, 2);
    SpawnGerudo(play, V(560.0f, 640.0f, -2640.0f), 0, 1);
    SpawnGerudo(play, V(560.0f, 640.0f, -2480.0f), -0x8000, 0);
    gEnGeldBExtraLook = sTableSpot;
    gEnGeldBExtraLookOn = true;
    return true;
}

static void MapCam(PlayState* play, Vec3f p) {
    // Looking down at the map from his side of the table
    Cam(play, V(p.x + 75.0f, p.y + 95.0f, p.z + 10.0f), p, 55.0f);
}

static bool Update4(PlayState* play, s32 t) {
    GdfIntroCs* cs = &gGdfIntroCs;
    Vec3f castle = MapPoint(112.0f, 28.0f);
    Vec3f mountain = MapPoint(132.0f, 12.0f);
    Vec3f zoras = MapPoint(197.0f, 33.0f);
    Vec3f forest = MapPoint(172.0f, 68.0f);
    Vec3f head = V(sTableSpot.x, sLinkPos.y + 108.0f, sTableSpot.z);

    // The map, from the castle across the land
    if (t < 30) {
        MapCam(play, castle);
    } else if (t < 55) {
        MapCam(play, Lerp(castle, mountain, Smooth(t, 30, 50)));
    } else if (t < 80) {
        MapCam(play, Lerp(mountain, zoras, Smooth(t, 55, 75)));
    } else if (t < 105) {
        MapCam(play, Lerp(zoras, forest, Smooth(t, 80, 100)));
    } else if (t < 118) {
        // Up to his commanders
        Cam(play, V(415.0f, head.y - 5.0f, -2595.0f), V(640.0f, head.y, -2560.0f));
    } else if (t < 132) {
        // The captain, over his shoulder
        Cam(play, V(705.0f, head.y + 10.0f, -2600.0f), V(480.0f, head.y - 40.0f, -2560.0f));
    } else if (t < 165) {
        // His raised hand, from below
        Cam(play, V(485.0f, head.y - 45.0f, -2532.0f), V(640.0f, head.y + 10.0f, -2560.0f));
    } else if (t < 185) {
        Vec3f stone = MapPoint(cs->stoneU, cs->stoneV);

        Cam(play, V(stone.x + 55.0f, stone.y + 55.0f, stone.z + 30.0f), stone, 55.0f);
    } else {
        // The slow zoom to his face
        CamMove(play, t, 185, 270, V(495.0f, head.y, -2560.0f), V(640.0f, head.y + 2.0f, -2560.0f),
                V(585.0f, head.y + 2.0f, -2560.0f), V(640.0f, head.y + 2.0f, -2560.0f), 50.0f);
    }

    if (At(t, 2)) {
        cs->pose = GDF_CSPOSE_TABLE;
    }
    if (At(t, 6)) {
        cs->stoneOn = true;
        cs->stoneDrop = 30.0f;
    }
    if ((t >= 6) && (t <= 16)) {
        cs->stoneDrop = 30.0f * (1.0f - Smooth(t, 6, 16));
    }
    if (At(t, 16)) {
        Sfx(NA_SE_EV_STONE_BOUND);
    }
    if (At(t, 25)) {
        Say(play, TX_G_KING);
    }
    if (At(t, 50)) {
        Say(play, TX_G_GORONS);
    }
    if (At(t, 75)) {
        Say(play, TX_G_ZORAS);
    }
    if (At(t, 100)) {
        Say(play, TX_G_KOKIRI);
    }
    if (At(t, 106)) {
        cs->pose = GDF_CSPOSE_NONE;
    }
    if (At(t, 110)) {
        Say(play, TX_G_BOW);
    }
    // The captain steps forward
    if ((t >= 118) && (t < 124) && (sCaptain != NULL) && (sCaptain->update != NULL)) {
        sCaptain->speedXZ = 3.0f;
        sCaptain->world.rot.y = 0x4000;
    } else if ((sCaptain != NULL) && (sCaptain->update != NULL)) {
        sCaptain->speedXZ = 0.0f;
    }
    if (At(t, 126)) {
        Say(play, TX_CAPTAIN);
    }
    if (At(t, 132)) {
        cs->pose = GDF_CSPOSE_RAISE;
        cs->openHand = true;
    }
    if (At(t, 160)) {
        Say(play, TX_G_ARMY);
    }
    if (At(t, 165)) {
        cs->pose = GDF_CSPOSE_TABLE;
    }
    if (At(t, 180)) {
        // His fist closes over the stone
        cs->openHand = false;
        cs->stoneOn = false;
        Sfx(NA_SE_EV_ROCK_BROKEN);
    }
    if (At(t, 187)) {
        cs->pose = GDF_CSPOSE_NONE;
        Say(play, TX_G_POWER);
    }
    if (At(t, 192)) {
        MusicStop(60);
    }
    // The fortress shakes, the torches gutter and the room goes cold
    if (At(t, 215)) {
        Sfx(NA_SE_EV_EARTHQUAKE);
        Actor_RequestQuake(play, 3, 30);
        Rumble_Request(0.0f, 180, 20, 100);
        sColdTarget = 1.0f;
        gEnGeldBExtraLookOn = false;
    }
    // He looks down at his hand: a faint green glow, gone again
    if (At(t, 240)) {
        cs->pose = GDF_CSPOSE_HAND;
        cs->headPitch = 28.0f;
    }
    if (At(t, 250)) {
        cs->handGlow = 2.5f;
        Sfx(NA_SE_EV_TRIFORCE_MARK);
    }
    if (At(t, 266)) {
        cs->handGlow = 0.0f;
    }
    if (At(t, 272)) {
        cs->headPitch = 0.0f;
        cs->headYaw = 40.0f;
    }
    return t >= 285;
}

// ---- Shot 5: the vision ----

static Vec3f CamForward(Vec3f eye, Vec3f at, f32 dist) {
    f32 dx = at.x - eye.x;
    f32 dy = at.y - eye.y;
    f32 dz = at.z - eye.z;
    f32 len = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));

    return V(eye.x + dx / len * dist, eye.y + dy / len * dist, eye.z + dz / len * dist);
}

static bool Setup5(PlayState* play) {
    PlaceLink(play, sWindowSpot, -0x4000);
    return true;
}

static bool Update5(PlayState* play, s32 t) {
    GdfIntroCs* cs = &gGdfIntroCs;
    Vec3f eye = V(300.0f, 1225.0f, -2700.0f);
    Vec3f at = V(-1500.0f, 1100.0f, -3000.0f);

    if (t < 34) {
        // Out over the desert; the horizon trembles
        Cam(play, Lerp(eye, V(220.0f, 1215.0f, -2700.0f), Smooth(t, 0, 34)),
            V(at.x, at.y + Math_SinS(t * 0x2000) * 4.0f * Smooth(t, 15, 34), at.z), 60.0f + 6.0f * Smooth(t, 15, 34));
        if (At(t, 18)) {
            Sfx(NA_SE_EV_FANTOM_WARP_S);
        }
        return false;
    }
    eye = V(220.0f, 1215.0f, -2700.0f);
    Cam(play, eye, at, 60.0f);
    if (At(t, 34)) {
        Fill(255, 255, 255, 255.0f);
        FillTo(0.0f, 20.0f);
        cs->hideAll = true;
        cs->voidAlpha = 255;
        cs->goldPos = CamForward(eye, at, 420.0f);
        Sfx(NA_SE_EV_LIGHT_GATHER);
    }
    // A vast golden light, suspended over the void
    if ((t >= 34) && (t < 84)) {
        cs->goldLight = 4.0f + 30.0f * Smooth(t, 34, 80);
    }
    // Three triangles, turning slowly in the dark
    if (At(t, 84)) {
        Fill(255, 255, 255, 255.0f);
        FillTo(0.0f, 25.0f);
        cs->goldLight = 0.0f;
        cs->triforcePos = CamForward(eye, at, 300.0f);
        cs->triforceAlpha = 255;
        cs->triforceYaw = 0x4000; // face on to the camera (it looks west) to begin with
        Sfx(NA_SE_EV_TRIFORCE);
    }
    if (t >= 84) {
        cs->triforceScale = 3.0f + 3.0f * Smooth(t, 84, 140);
        cs->triforceYaw += 0x100;
    }
    return t >= 140;
}

// The castle in flames (outside Ganon's Castle, its lava)
static bool Setup5b(PlayState* play) {
    gGdfIntroCs.hideAll = true;
    PlaceLink(play, V(-453.0f, 1085.0f, 3400.0f), -0x8000);
    Fill(255, 70, 0, 70.0f);
    return true;
}

static bool Update5b(PlayState* play, s32 t) {
    CamMove(play, t, 0, 60, V(-453.0f, 1250.0f, 3600.0f), V(-250.0f, 1700.0f, 500.0f), V(-430.0f, 1260.0f, 3300.0f),
            V(-250.0f, 1720.0f, 500.0f), 55.0f);
    if (At(t, 2)) {
        Actor_RequestQuake(play, 2, 50);
    }
    if (sFresh && ((t % 12) == 2)) {
        Sfx(NA_SE_EV_FIRE_PILLAR);
    }
    return t >= 60;
}

// A boy beneath a great tree, holding something that glows blue
static bool Setup5c(PlayState* play) {
    gGdfIntroCs.showLink = true;
    PlaceLink(play, V(3650.0f, -178.0f, -900.0f), -0x1D9C);
    sLinkCsAction = 28; // holding it up
    Fill(255, 255, 255, 55.0f);
    return true;
}

static bool Update5c(PlayState* play, s32 t) {
    Player* player = GET_PLAYER(play);
    GdfIntroCs* cs = &gGdfIntroCs;

    CamMove(play, t, 0, 60, V(3250.0f, -90.0f, -450.0f), V(3780.0f, -40.0f, -1120.0f), V(3330.0f, -100.0f, -540.0f),
            V(3760.0f, -50.0f, -1100.0f), 58.0f);
    cs->ocarinaOn = (t >= 4);
    cs->ocarinaPos = Lerp(player->bodyPartsPos[PLAYER_BODYPART_L_HAND], player->bodyPartsPos[PLAYER_BODYPART_R_HAND],
                          0.5f);
    cs->ocarinaPos.y += 6.0f;
    return t >= 60;
}

// Back at the window
static bool Setup6(PlayState* play) {
    PlaceLink(play, sWindowSpot, -0x4000);
    gGdfIntroCs.pose = GDF_CSPOSE_WINDOW;
    sColdTarget = 1.0f;
    return true;
}

static bool Update6(PlayState* play, s32 t) {
    GdfIntroCs* cs = &gGdfIntroCs;

    if (t < 50) {
        Cam(play, V(35.0f, 1205.0f, -2640.0f), V(95.0f, 1192.0f, -2692.0f), 55.0f);
    } else {
        // He turns from the window, toward Hyrule
        CamMove(play, t, 50, 85, V(120.0f, 1215.0f, -2610.0f), V(90.0f, 1195.0f, -2690.0f),
                V(265.0f, 1205.0f, -2700.0f), V(90.0f, 1200.0f, -2690.0f));
    }
    if (At(t, 8)) {
        // He grips the sill hard enough to crack it
        Sfx(NA_SE_EV_WALL_BROKEN);
        Actor_RequestQuake(play, 1, 8);
    }
    if (At(t, 20)) {
        Say(play, TX_G_WHAT);
    }
    if (At(t, 30)) {
        sColdTarget = 0.0f;
    }
    if (At(t, 40)) {
        Say(play, TX_G_BEYOND);
    }
    if (At(t, 50)) {
        cs->pose = GDF_CSPOSE_NONE;
    }
    if (t >= 50) {
        TurnLink(0x4000, 0x500);
    }
    if (At(t, 88)) {
        Say(play, TX_G_MINE);
    }
    if (At(t, 92)) {
        cs->laugh = true;
    }
    return t >= 135;
}

// ---- Shot 6: the oath ----

static const Vec3f sBalcony = { 335.0f, 640.0f, -2520.0f };

static bool Setup7(PlayState* play) {
    if (!EnsureObject(play, OBJECT_GELDB) || !EnsureObject(play, OBJECT_SYOKUDAI) ||
        !EnsureObject(play, OBJECT_HORSE_GANON)) {
        return false;
    }
    PlaceLink(play, V(470.0f, 640.0f, -2520.0f), -0x4000);
    // The courtyard, lit by torches
    static const Vec3f sTorches[] = {
        { -250.0f, 333.0f, -1300.0f }, { 180.0f, 333.0f, -1300.0f }, { -250.0f, 333.0f, -1850.0f },
        { 200.0f, 333.0f, -2100.0f },  { 0.0f, 333.0f, -2450.0f },   { 330.0f, 640.0f, -2650.0f },
        { 330.0f, 640.0f, -2410.0f },  { -150.0f, 333.0f, -1550.0f },
    };
    for (const Vec3f& p : sTorches) {
        SpawnTorch(play, p);
    }
    // The warriors, in ranks before his balcony
    static const Vec3f sRanks[] = {
        { 150.0f, 333.0f, -2350.0f }, { 150.0f, 333.0f, -2230.0f }, { 150.0f, 333.0f, -2110.0f },
        { 150.0f, 333.0f, -1990.0f }, { 40.0f, 333.0f, -2390.0f },  { 40.0f, 333.0f, -2270.0f },
        { 40.0f, 333.0f, -2150.0f },  { 40.0f, 333.0f, -2030.0f },  { -70.0f, 333.0f, -2200.0f },
        { -70.0f, 333.0f, -2080.0f }, { -70.0f, 333.0f, -1960.0f }, { -70.0f, 333.0f, -1840.0f },
        { -180.0f, 333.0f, -2050.0f }, { -180.0f, 333.0f, -1930.0f }, { -180.0f, 333.0f, -1810.0f },
        { -180.0f, 333.0f, -1690.0f },
    };
    s32 i = 0;
    for (const Vec3f& p : sRanks) {
        SpawnGerudo(play, p, Math_Atan2S(sBalcony.z - p.z, sBalcony.x - p.x), i++);
    }
    gEnGeldBExtraLook = sBalcony;
    gEnGeldBExtraLookOn = true;
    // Their horses, stamping in the sand
    sEscortHorse[0] = SpawnHorse(play, V(-260.0f, 333.0f, -1450.0f), 0x6000);
    sEscortHorse[1] = SpawnHorse(play, V(-120.0f, 333.0f, -1380.0f), 0x5000);
    MusicStop(0);
    return true;
}

static bool Update7(PlayState* play, s32 t) {
    GdfIntroCs* cs = &gGdfIntroCs;
    Vec3f head = V(sBalcony.x, sLinkPos.y + 105.0f, sBalcony.z);

    if (t < 48) {
        CamMove(play, t, 0, 48, V(-650.0f, 560.0f, -1500.0f), V(330.0f, 700.0f, -2520.0f), V(-560.0f, 540.0f, -1580.0f),
                V(330.0f, 710.0f, -2520.0f));
    } else if (t < 60) {
        Cam(play, V(60.0f, 410.0f, -2300.0f), V(head.x, head.y, head.z));
    } else if (t < 150) {
        // Round the courtyard as the chant grows
        f32 a = (200.0f + 120.0f * Smooth(t, 60, 150)) * (M_PI / 180.0f);
        Vec3f c = V(-30.0f, 393.0f, -2050.0f);

        Cam(play, V(c.x + cosf(a) * 650.0f, c.y + 300.0f, c.z + sinf(a) * 650.0f), c, 65.0f);
    } else if (t < 183) {
        Cam(play, V(180.0f, head.y - 175.0f, -2480.0f), head);
    } else {
        Cam(play, V(-300.0f, 420.0f, -1700.0f), V(head.x, head.y + 60.0f, head.z), 65.0f);
    }

    // He steps out onto the balcony
    if (t < 40) {
        WalkLink(play, sBalcony, 3.6f);
    } else {
        sLinkSpeed = 0.0f;
        TurnLink(-0x4000, 0x800);
    }
    if (At(t, 50)) {
        Say(play, TX_G_TONIGHT);
    }
    if (At(t, 55)) {
        Say(play, TX_G_TOMORROW);
    }
    if (At(t, 60)) {
        gEnGeldBExtraCue = GELDB_EXTRA_CHEER;
        Music(NA_BGM_OPENING_GANON, 10);
        Say(play, TX_CHANT, false);
    }
    if ((t >= 60) && (t < 150) && sFresh && ((t % 5) == 0)) {
        Sfx(((t / 5) % 2) ? NA_SE_EN_GERUDOFT_ATTACK : NA_SE_EN_GERUDOFT_BREATH);
    }
    // His raised hand, and silence
    if (At(t, 150)) {
        cs->pose = GDF_CSPOSE_RAISE;
        cs->openHand = true;
        gEnGeldBExtraCue = GELDB_EXTRA_STAND;
    }
    if (At(t, 170)) {
        Say(play, TX_G_WAIT);
    }
    if (At(t, 175)) {
        cs->pose = GDF_CSPOSE_NONE;
        cs->openHand = false;
        cs->headYaw = 35.0f;
        cs->headPitch = -8.0f;
    }
    if (At(t, 180)) {
        Say(play, TX_G_TAKE);
    }
    // Lightning across the desert sky, without rain
    if (At(t, 186) || At(t, 210)) {
        play->envCtx.lightningMode = LIGHTNING_MODE_ON;
        Environment_AddLightningBolts(play, 3);
        gLightningStrike.state = LIGHTNING_STRIKE_START;
        gLightningStrike.flashAlphaTarget = 200;
        Sfx(NA_SE_EV_LIGHTNING);
        Actor_RequestQuake(play, 1, 6);
    }
    if (At(t, 230)) {
        play->envCtx.lightningMode = LIGHTNING_MODE_OFF;
        cs->headYaw = cs->headPitch = 0.0f;
    }
    return t >= 236;
}

// ---- Shot 7: the messenger ----

static bool Setup8(PlayState* play) {
    if (!EnsureObject(play, OBJECT_GELDB)) {
        return false;
    }
    PlaceLink(play, sBalcony, 0x4000);
    sScout = SpawnGerudo(play, V(430.0f, 640.0f, -2525.0f), -0x4000, 1);
    gEnGeldBExtraCue = GELDB_EXTRA_KNEEL;
    gEnGeldBExtraLookOn = false;
    return true;
}

static bool Update8(PlayState* play, s32 t) {
    GdfIntroCs* cs = &gGdfIntroCs;
    f32 headY = sLinkPos.y + 105.0f;

    if (t < 15) {
        Cam(play, V(560.0f, headY - 10.0f, -2640.0f), V(385.0f, headY - 40.0f, -2522.0f));
    } else if (t < 52) {
        // Over his shoulder: the royal seal, the letter's words hidden from us
        Cam(play, V(285.0f, headY + 40.0f, -2500.0f), V(372.0f, headY - 25.0f, -2522.0f), 50.0f);
    } else if (t < 60) {
        Cam(play, V(545.0f, headY - 23.0f, -2625.0f), V(425.0f, headY - 55.0f, -2525.0f));
    } else {
        // Following him toward the gates
        Vec3f behind = V(sLinkPos.x - Math_SinS(sLinkYaw) * 190.0f, headY + 30.0f, sLinkPos.z - Math_CosS(sLinkYaw) * 190.0f);
        Vec3f ahead = V(sLinkPos.x + Math_SinS(sLinkYaw) * 120.0f, headY - 20.0f, sLinkPos.z + Math_CosS(sLinkYaw) * 120.0f);

        Cam(play, behind, ahead);
    }

    if (At(t, 8)) {
        Say(play, TX_SCOUT_MESSENGER);
    }
    if (At(t, 15)) {
        cs->pose = GDF_CSPOSE_LETTER;
        cs->letterOn = true;
    }
    if (At(t, 28)) {
        cs->sealBroken = true;
        Sfx(NA_SE_PL_PULL_UP_PLANT);
    }
    if (At(t, 45)) {
        Say(play, TX_G_SPEAK);
    }
    if (At(t, 50)) {
        cs->letterOn = false;
        cs->pose = GDF_CSPOSE_NONE;
        Say(play, TX_G_HORSE);
    }
    if (At(t, 56)) {
        Say(play, TX_SCOUT_YOURSELF);
    }
    if (At(t, 60)) {
        Say(play, TX_G_COURSE);
    }
    // He walks past her
    if ((t > 60) && (t < 150)) {
        if (sLinkPos.x < 425.0f) {
            WalkLink(play, V(430.0f, 640.0f, -2455.0f), 3.2f);
        } else {
            WalkLink(play, V(680.0f, 640.0f, -2470.0f), 3.2f);
        }
    } else {
        sLinkSpeed = 0.0f;
    }
    if (At(t, 150)) {
        Say(play, TX_G_EYE);
    }
    return t >= 158;
}

// ---- Shot 8: the ride out ----

// Down the path to the valley, stopping short of the way out (crossing it would leave the scene)
static const Vec3f sRidePath[] = {
    { -850.0f, 0.0f, -700.0f },
    { -800.0f, 0.0f, 150.0f },
    { -640.0f, 0.0f, 420.0f },
};
static s32 sRideIndex[3] = { 0, 0, 0 };

static bool SetupRiders(PlayState* play, Vec3f start, s16 yaw, bool escorts) {
    if (!EnsureObject(play, OBJECT_HORSE_GANON) || !EnsureObject(play, OBJECT_GELDB)) {
        return false;
    }
    sHorse = SpawnHorse(play, start, yaw);
    gGdfIntroCs.horse = sHorse;
    if (escorts) {
        for (s32 i = 0; i < 2; i++) {
            f32 side = (i == 0) ? -90.0f : 90.0f;
            Vec3f p = V(start.x + Math_CosS(yaw) * side - Math_SinS(yaw) * 140.0f, start.y,
                        start.z - Math_SinS(yaw) * side - Math_CosS(yaw) * 140.0f);

            sEscortHorse[i] = SpawnHorse(play, p, yaw);
            sEscort[i] = SpawnGerudo(play, p, yaw, 2 - i);
        }
        gEnGeldBExtraCue = GELDB_EXTRA_KNEEL; // crouched in the saddle
    }
    sRideIndex[0] = sRideIndex[1] = sRideIndex[2] = 0;
    PlaceLink(play, start, yaw);
    return true;
}

// Link keeps up with the horse he's on (Ganondorf sits in its saddle)
static void FollowHorse() {
    if ((sHorse != NULL) && (sHorse->update != NULL)) {
        // Clear above the horse, so Link (hidden) doesn't shove it about
        sLinkPos = sHorse->world.pos;
        sLinkPos.y += 300.0f;
        sLinkYaw = sHorse->shape.rot.y;
        sLinkSpeed = 0.0f;
    }
}

static bool Setup9(PlayState* play) {
    return SetupRiders(play, V(-850.0f, 90.0f, -2050.0f), 0, true);
}

static bool Update9(PlayState* play, s32 t) {
    Vec3f horse = (sHorse != NULL) ? sHorse->world.pos : V(-850.0f, 0.0f, -1000.0f);

    if (At(t, 0)) {
        Music(NA_BGM_TITLE, 40);
    }
    if (At(t, 4) && (sHorse != NULL)) {
        Audio_PlayActorSound2(sHorse, NA_SE_EV_HORSE_NEIGH);
    }
    if (t >= 6) {
        RideAlong(play, sHorse, sRidePath, ARRAY_COUNT(sRidePath), &sRideIndex[0], 12.0f, 0.4f);
        for (s32 i = 0; i < 2; i++) {
            RideAlong(play, sEscortHorse[i], sRidePath, ARRAY_COUNT(sRidePath), &sRideIndex[1 + i], 12.0f, 0.38f);
        }
    }
    FollowHorse();
    for (s32 i = 0; i < 2; i++) {
        SeatRider(sEscort[i], sEscortHorse[i]);
    }

    if (t < 105) {
        // Low by the path, looking up at him as he rides out
        Cam(play, V(-700.0f, 40.0f, -620.0f), V(horse.x, horse.y + 110.0f, horse.z), 62.0f);
    } else {
        // Rising over the walls: the desert behind, Hyrule ahead under the stars
        CamMove(play, t, 105, 185, V(-700.0f, 40.0f, -620.0f), V(horse.x, horse.y + 110.0f, horse.z),
                V(-650.0f, 1500.0f, -900.0f), V(1500.0f, 300.0f, 3000.0f), 62.0f);
    }
    return t >= 190;
}

// ---- Shot 9: the legend begins ----

static const Vec3f sFieldPath[] = { { -300.0f, 0.0f, 1300.0f } };

static bool Setup10(PlayState* play) {
    return SetupRiders(play, V(-2300.0f, 0.0f, 3300.0f), 0x6000, false);
}

static bool Update10(PlayState* play, s32 t) {
    Vec3f horse = (sHorse != NULL) ? sHorse->world.pos : V(-2000.0f, 0.0f, 3000.0f);
    s16 yaw = (sHorse != NULL) ? sHorse->shape.rot.y : 0x6000;

    if (sHorse != NULL) {
        Math_ApproachF(&sHorse->speedXZ, 13.0f, 1.0f, 1.0f);
    }
    RideAlong(play, sHorse, sFieldPath, ARRAY_COUNT(sFieldPath), &sRideIndex[0], 13.0f, 1.0f);
    FollowHorse();
    // Alongside him at a gallop
    Cam(play, V(horse.x + Math_CosS(yaw) * 380.0f, horse.y + 120.0f, horse.z - Math_SinS(yaw) * 380.0f),
        V(horse.x + Math_SinS(yaw) * 60.0f, horse.y + 100.0f, horse.z + Math_CosS(yaw) * 60.0f), 55.0f);
    return t >= 75;
}

static bool Setup11(PlayState* play) {
    gGdfIntroCs.hideAll = true;
    PlaceLink(play, V(3300.0f, -110.0f, -560.0f), 0);
    return true;
}

static bool Update11(PlayState* play, s32 t) {
    CamMove(play, t, 0, 60, V(3250.0f, -90.0f, -450.0f), V(3850.0f, 0.0f, -1200.0f), V(3280.0f, -85.0f, -500.0f),
            V(3850.0f, 10.0f, -1200.0f), 60.0f);
    return t >= 60;
}

static bool Setup12(PlayState* play) {
    gGdfIntroCs.showLink = true;
    PlaceLink(play, V(1.0f, 0.0f, 95.0f), -0x8000);
    sLinkCsAction = 38; // asleep in bed
    return true;
}

static bool Update12(PlayState* play, s32 t) {
    CamMove(play, t, 0, 70, V(-70.0f, 75.0f, 30.0f), V(0.0f, 20.0f, 110.0f), V(-55.0f, 62.0f, 45.0f),
            V(0.0f, 20.0f, 110.0f), 55.0f);
    return t >= 70;
}

static bool Setup13(PlayState* play) {
    return SetupRiders(play, V(1000.0f, 370.0f, 4500.0f), -0x7718, false);
}

static bool Update13(PlayState* play, s32 t) {
    Vec3f horse = (sHorse != NULL) ? sHorse->world.pos : V(1000.0f, 370.0f, 4500.0f);

    if (sHorse != NULL) {
        sHorse->speedXZ = 0.0f;
    }
    FollowHorse();
    if (t < 40) {
        CamMove(play, t, 0, 40, V(1330.0f, horse.y + 260.0f, 5000.0f), V(0.0f, 250.0f, 0.0f),
                V(1260.0f, horse.y + 220.0f, 4900.0f), V(0.0f, 250.0f, 0.0f), 55.0f);
    } else {
        // He looks toward the horizon
        CamMove(play, t, 40, 120, V(horse.x - 150.0f, horse.y + 190.0f, horse.z - 140.0f),
                V(horse.x, horse.y + 185.0f, horse.z), V(horse.x - 125.0f, horse.y + 192.0f, horse.z - 120.0f),
                V(horse.x, horse.y + 185.0f, horse.z), 50.0f);
    }
    if (At(t, 50)) {
        Say(play, TX_VO_KING);
    }
    if (At(t, 56)) {
        FillTo(255.0f, 6.0f); // fade to black
        MusicStop(60);
    }
    return t >= 100;
}

static const Shot sShots[] = {
    { ENTR_GERUDOS_FORTRESS_EAST_EXIT, 0xBC00, TR_BLACK, Setup1, Update1 },
    { ENTR_THIEVES_HIDEOUT_12, 0xBC00, TR_BLACK, Setup2, Update2 },
    { ENTR_GERUDOS_FORTRESS_EAST_EXIT, 0xBC00, TR_WHITE, Setup3, Update3 },
    { -1, 0xBC00, TR_BLACK, Setup4, Update4 },
    { -1, 0xBC00, TR_FLASH, Setup5, Update5 },
    { ENTR_OUTSIDE_GANONS_CASTLE_0_2, 0x8000, TR_WHITE, Setup5b, Update5b },
    { ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE, 0x8000, TR_WHITE, Setup5c, Update5c },
    { ENTR_GERUDOS_FORTRESS_EAST_EXIT, 0xBC00, TR_WHITE, Setup6, Update6 },
    { ENTR_GERUDOS_FORTRESS_EAST_EXIT, 0xE000, TR_BLACK, Setup7, Update7 },
    { -1, 0xE000, TR_BLACK, Setup8, Update8 },
    { -1, 0xE000, TR_BLACK, Setup9, Update9 },
    { ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN, 0xE400, TR_BLACK, Setup10, Update10 },
    { ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE, 0xE400, TR_BLACK, Setup11, Update11 },
    { ENTR_LINKS_HOUSE_CHILD_SPAWN, 0xE400, TR_BLACK, Setup12, Update12 },
    { ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN, 0xE400, TR_BLACK, Setup13, Update13 },
};

// ---- Running it ----

static s16 EntranceScene(s16 entrance) {
    return gEntranceTable[entrance].scene;
}

static void LoadEntrance(PlayState* play, s16 entrance, u8 trans) {
    // The intro keeps its own music through the loads (no scene music, no fade-out)
    gSaveContext.forcedSeqId = NA_BGM_DISABLED;
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = (trans == TR_WHITE) ? TRANS_TYPE_FADE_WHITE_FAST : TRANS_TYPE_FADE_BLACK;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.respawnFlag = 0;
    sLoadScene = EntranceScene(entrance);
}

static void BeginShot(PlayState* play, s32 index) {
    const Shot& shot = sShots[index];

    sShot = index;
    sDayTime = shot.dayTime;
    gSaveContext.dayTime = sDayTime;
    if (shot.entrance >= 0) {
        ClearShotActors();
        ReleaseCam(play);
        LoadEntrance(play, shot.entrance, shot.trans);
        sPhase = PH_LOADING;
    } else if (shot.trans == TR_CUT) {
        sPhase = PH_SETUP;
    } else {
        // A quick fade within the scene
        if (shot.trans == TR_FLASH) {
            Fill(255, 255, 255, 0.0f);
            FillTo(255.0f, 64.0f);
        } else {
            Fill(0, 0, 0, sFillA);
            FillTo(255.0f, 32.0f);
        }
        sPhase = PH_FADE_OUT;
        sPhaseTimer = 0;
    }
}

static void RestoreLink(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player == NULL) {
        return;
    }
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
    player->actor.shape.shadowAlpha = 255;
    Player_SetCsAction(play, NULL, PLAYER_CSACTION_7);
    if ((player->naviActor != NULL) && (sNaviDraw != NULL)) {
        player->naviActor->draw = sNaviDraw;
    }
}

// Where play picks up afterwards: a new file wakes at dawn outside the castle town; a replay goes back where it was
static void Finish(PlayState* play) {
    ClearShotActors();
    ReleaseCam(play);
    ResetProps();
    RestoreLink(play);
    gGdfIntroCs.active = false;
    sActive = false;
    sPhase = PH_END_LOADING;
    play->envCtx.lightningMode = LIGHTNING_MODE_OFF;
    gEnGeldBExtraCue = GELDB_EXTRA_STAND;
    gEnGeldBExtraLookOn = false;
    if (sDekuFlagSaved && !sDekuMetBefore) {
        Flags_UnsetEventChkInf(EVENTCHKINF_MET_DEKU_TREE);
    }
    sDekuFlagSaved = false;

    gSaveContext.forcedSeqId = NA_BGM_GENERAL_SFX;
    MusicStop(20);
    Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
    Letterbox_SetSizeTarget(0);
    if (sFromNewFile) {
        gSaveContext.dayTime = 0x4AAB; // seven in the morning: the drawbridge is down
        play->nextEntranceIndex = ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
        sGreetTimer = PlayAsGanondorf_UsePoeNavi() ? 40 : -1;
    } else {
        gSaveContext.dayTime = sReturnDayTime;
        play->nextEntranceIndex = sReturnEntrance;
        sGreetTimer = -1;
    }
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.respawnFlag = 0;
}

extern "C" s32 GanondorfIntro_IsActive(void) {
    return sActive;
}

extern "C" void GanondorfIntro_Start(s32 fromNewFile) {
    PlayState* play = gPlayState;

    if ((play == NULL) || sActive) {
        return;
    }
    sActive = true;
    sFromNewFile = fromNewFile;
    sReturnEntrance = gSaveContext.entranceIndex;
    sReturnDayTime = gSaveContext.dayTime;
    sNaviDraw = NULL;
    sCold = sColdTarget = 0.0f;
    sCamId = SUBCAM_NONE;
    ResetProps();
    gGdfIntroCs.active = true;
    sDekuMetBefore = Flags_GetEventChkInf(EVENTCHKINF_MET_DEKU_TREE);
    sDekuFlagSaved = true;
    // Straight to black (on a new file the fortress mustn't show first)
    Fill(0, 0, 0, fromNewFile ? 255.0f : 0.0f);
    if (fromNewFile) {
        play->envCtx.fillScreen = true;
        play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] = 0;
        play->envCtx.screenFillColor[3] = 255;
    }
    BeginShot(play, 0);
}

// Every frame of a shot: Link held in place (or walked), hidden as the shot needs; the HUD away, the letterbox in,
// the time held, anything in the way removed
static void HoldScene(PlayState* play) {
    Player* player = GET_PLAYER(play);

    gSaveContext.dayTime = sDayTime;
    Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
    Letterbox_SetSizeTarget(32);
    ClearScene(play);

    if (player->csAction != sLinkCsAction) {
        Player_SetCsAction(play, NULL, sLinkCsAction);
    }
    if (sLinkPinned) {
        player->actor.world.pos = sLinkPos;
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = sLinkYaw;
        player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
        player->linearVelocity = sLinkSpeed;
    }
    if (gGdfIntroCs.hideAll) {
        player->stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;
        player->actor.shape.shadowAlpha = 0; // no shadow left on the floor where he isn't
    } else {
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
        if (gGdfIntroCs.showLink) {
            player->actor.shape.shadowAlpha = 255;
        }
    }
    // The Poe stays out of it
    if (player->naviActor != NULL) {
        if ((player->naviActor->draw != NULL) && (sNaviDraw == NULL)) {
            sNaviDraw = player->naviActor->draw;
        }
        player->naviActor->draw = NULL;
    }

    // The screen fill (fades, the black between shots, the vision's tints)
    if (sFillStep > 0.0f) {
        Math_StepToF(&sFillA, sFillTarget, sFillStep);
    }
    if (sFillA > 0.0f) {
        play->envCtx.fillScreen = true;
        play->envCtx.screenFillColor[0] = sFillR;
        play->envCtx.screenFillColor[1] = sFillG;
        play->envCtx.screenFillColor[2] = sFillB;
        play->envCtx.screenFillColor[3] = (u8)CLAMP(sFillA, 0.0f, 255.0f);
    } else {
        play->envCtx.fillScreen = false;
    }

    // The cold: the light dims, the torches gutter, everything turns blue
    static bool sColdShown = false;

    Math_StepToF(&sCold, sColdTarget, 0.03f);
    if ((sCold <= 0.0f) && sColdShown) {
        // Warm again: the light back as it was
        sColdShown = false;
        for (s32 i = 0; i < 3; i++) {
            play->envCtx.adjAmbientColor[i] = play->envCtx.adjLight1Color[i] = play->envCtx.adjFogColor[i] = 0;
        }
    }
    if (sCold > 0.0f) {
        sColdShown = true;
        f32 flicker = (Rand_ZeroOne() - 0.5f) * 30.0f * sCold;

        play->envCtx.adjAmbientColor[0] = (s16)(-50.0f * sCold + flicker);
        play->envCtx.adjAmbientColor[1] = (s16)(-35.0f * sCold + flicker);
        play->envCtx.adjAmbientColor[2] = (s16)(15.0f * sCold + flicker);
        play->envCtx.adjLight1Color[0] = (s16)(-60.0f * sCold);
        play->envCtx.adjLight1Color[1] = (s16)(-40.0f * sCold);
        play->envCtx.adjLight1Color[2] = (s16)(20.0f * sCold);
        play->envCtx.adjFogColor[0] = (s16)(-50.0f * sCold);
        play->envCtx.adjFogColor[1] = (s16)(-30.0f * sCold);
        play->envCtx.adjFogColor[2] = (s16)(30.0f * sCold);
    }
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }

    // After the intro: the Poe greets her master
    if (!sActive) {
        if ((sGreetTimer > 0) && (play->sceneNum == SCENE_HYRULE_FIELD) &&
            (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
            (--sGreetTimer == 0)) {
            Message_StartTextbox(play, GDF_TEXT_POE_GREETING, NULL);
            sGreetTimer = -1;
        }
        return;
    }

    // START skips it
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_START) && (sPhase != PH_LOADING) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF)) {
        if (play->msgCtx.msgMode != MSGMODE_NONE) {
            Message_CloseTextbox(play);
        }
        Finish(play);
        return;
    }

    switch (sPhase) {
        case PH_LOADING:
            return; // OnSceneInit moves on

        case PH_FADE_OUT:
            HoldScene(play);
            if (sFillA >= 255.0f) {
                ClearShotActors();
                ResetProps();
                sLinkCsAction = 1;
                sPhase = PH_SETUP;
            }
            return;

        case PH_SETUP:
            HoldScene(play);
            if (play->transitionTrigger != TRANS_TRIGGER_OFF) {
                return;
            }
            if (sShots[sShot].setup(play)) {
                sPhase = PH_RUN;
                sT = 0;
                sLastRunT = -1;
                sMsgFreeze = false;
                // Fade back in from a fade within the scene
                if ((sShots[sShot].entrance < 0) && (sFillA > 0.0f)) {
                    FillTo(0.0f, (sShots[sShot].trans == TR_FLASH) ? 18.0f : 24.0f);
                }
            }
            return;

        case PH_RUN: {
            HoldScene(play);
            sFresh = (sT != sLastRunT);
            sLastRunT = sT;
            bool done = sShots[sShot].update(play, sT);

            if (sMsgFreeze && (play->msgCtx.msgMode != MSGMODE_NONE)) {
                return; // the script waits for the text
            }
            sMsgFreeze = false;
            if (done) {
                if (sShot + 1 < (s32)ARRAY_COUNT(sShots)) {
                    BeginShot(play, sShot + 1);
                } else {
                    Finish(play);
                }
                return;
            }
            sT++;
            return;
        }

        default:
            return;
    }
}

static void OnSceneInit(int16_t sceneNum) {
    PlayState* play = gPlayState;

    if (!sActive || (sPhase != PH_LOADING) || (play == NULL)) {
        return;
    }
    if (sceneNum != sLoadScene) {
        return;
    }
    sCamId = SUBCAM_NONE;
    sNaviDraw = NULL;
    ResetProps();
    sLinkCsAction = 1;
    sLinkPinned = false;
    sExtras.clear();
    sHorse = sEscortHorse[0] = sEscortHorse[1] = NULL;
    sEscort[0] = sEscort[1] = sScout = sCaptain = NULL;
    // The fill set before the load (the black after the desert) comes off; the vision's tints are set by the shot
    if (sFillA >= 255.0f) {
        Fill(0, 0, 0, 0.0f);
    }
    sPhase = PH_SETUP;
}

static void RegisterGanondorfIntro() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);

    // The Gerudo serve their King throughout, and the Deku Tree doesn't start his own scene when the boy is near
    REGISTER_VB_SHOULD(VB_GERUDOS_BE_FRIENDLY, {
        if (sActive) {
            *should = true;
        }
    });
    REGISTER_VB_SHOULD(VB_PLAY_DEKU_TREE_INTRO_CS, {
        if (sActive) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterGanondorfIntro, {});
