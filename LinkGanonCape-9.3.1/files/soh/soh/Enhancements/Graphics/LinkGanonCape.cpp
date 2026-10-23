// Ganondorf's Cape for Link
//
// Spawns Ganondorf's cloth-physics cape (EnGanonMant, the one he wears in the Ganon's Tower fight)
// on Link. The cape's graphics live in gameplay_keep, so it can be spawned in any scene.
//
// How it works:
//  - Each frame Link is drawn, LinkGanonCape_RecordLimb stores his shoulder positions.
//  - The cape is spawned in ACTORCAT_MISC, which draws after the player, so those positions are
//    from the same frame when LinkGanonCape_PrepareDraw turns them into the cape's attach points.
//  - Ganondorf's own cape values are scaled to Link's size by (jointLength / 9.5).
//  - The cape uses static textures and vertex buffers, so only one can exist. Link's cape is
//    removed while Ganondorf (ACTOR_BOSS_GANON) is loaded.
//
// Hood (optional, no physics):
//  - A 12x12 grid built every frame around Link's head and drawn with the cape's own gMantDL, so it
//    has the exact same material, texture and lighting. The grid's columns line up with the cape's
//    12 strands and its bottom row *is* the cape's top row (the strand roots), so there is no gap.
//  - The top 9 rows are a rigid shell in head space, the last 3 rows drape from the bottom of the
//    shell down to the cape's collar.
//  - The shell is a baked table (sHoodShell). It was shrink-wrapped around the vanilla adult and
//    child head meshes with a margin, so the cap dome and ears sit inside it and the bangs stick out
//    under the brim. A ray test found no head vertex outside it.
//  - The cap's tail (PLAYER_LIMB_HAT) is hidden while the hood is on, and the cap dome, which is
//    part of the head mesh and tinted by the tunic color, is tinted with the hair color instead.

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "LinkGanonCape.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"
extern PlayState* gPlayState;
}

#define CVAR_CAPE(var) CVAR_ENHANCEMENT("GanonCape." var)
#define CVAR_CAPE_ENABLED CVAR_CAPE("Enabled")

// Child Link is about 0.64x adult Link's size (see Player_OverrideLimbDrawGameplayCommon)
#define CHILD_SCALE 0.64f
// Ganondorf's strand segment length, all his cape values are tuned for this
#define GANONDORF_JOINT_LENGTH 9.5f

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sLeftShoulder;
static Vec3f sRightShoulder;
static MtxF sHeadMtx;
static u8 sRecordedLimbs = 0;    // bit 0 = left shoulder, bit 1 = right shoulder, bit 2 = head
static bool sHeadValid = false;  // head matrix is from the frame currently being drawn
static bool sCapeActive = false; // Link's cape exists this frame (used to hide his cap's tail)

static bool ShouldWearCape() {
    if (!CVarGetInteger(CVAR_CAPE_ENABLED, 0)) {
        return false;
    }
    if (!LINK_IS_ADULT && !CVarGetInteger(CVAR_CAPE("Child"), 1)) {
        return false;
    }
    return true;
}

extern "C" void LinkGanonCape_RecordLimb(PlayState* play, Player* player, int limbIndex) {
    // Dark Link also draws with Player_PostLimbDrawGameplay
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }

    if (limbIndex == PLAYER_LIMB_L_SHOULDER) {
        Matrix_MultVec3f(&sZeroVec, &sLeftShoulder);
        sRecordedLimbs |= 1;
    } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
        Matrix_MultVec3f(&sZeroVec, &sRightShoulder);
        sRecordedLimbs |= 2;
    } else if (limbIndex == PLAYER_LIMB_HEAD) {
        Matrix_Get(&sHeadMtx);
        sRecordedLimbs |= 4;
    }
}

static bool HoodEnabled() {
    return CVarGetInteger(CVAR_CAPE("Hood"), 1) != 0;
}

static Color_RGB8 sTunicColor;
static bool sTunicColorValid = false;
static bool sHeadTinted = false;

extern "C" void LinkGanonCape_SetTunicColor(int valid, unsigned char r, unsigned char g, unsigned char b) {
    sTunicColorValid = valid;
    sTunicColor.r = r;
    sTunicColor.g = g;
    sTunicColor.b = b;
}

extern "C" void LinkGanonCape_OverrideLimb(PlayState* play, Player* player, int limbIndex, void** dList) {
    if (play == NULL || player != GET_PLAYER(play) || !sCapeActive || !HoodEnabled() || !ShouldWearCape()) {
        sHeadTinted = false;
        return;
    }

    if (limbIndex == PLAYER_LIMB_HEAD) {
        // The cap dome is part of the head mesh and takes its color from the environment color (the tunic
        // color). Use the hair color while the head is drawn so only "hair" shows under the hood.
        if (sTunicColorValid) {
            Color_RGB8 hair = { 255, 173, 27 }; // SoH's default hair cosmetic color
            if (CVarGetInteger(CVAR_COSMETIC("Link.Hair.Changed"), 0)) {
                hair = CVarGetColor24(CVAR_COSMETIC("Link.Hair.Value"), hair);
            }

            OPEN_DISPS(play->state.gfxCtx);
            gDPSetEnvColor(POLY_OPA_DISP++, hair.r, hair.g, hair.b, 0);
            CLOSE_DISPS(play->state.gfxCtx);
            sHeadTinted = true;
        }
    } else if (limbIndex == PLAYER_LIMB_HAT) {
        // The tail of the cap. The hat is drawn right after the head, so this is also where the tunic color
        // goes back for the rest of Link's body.
        *dList = NULL;

        if (sHeadTinted) {
            OPEN_DISPS(play->state.gfxCtx);
            gDPSetEnvColor(POLY_OPA_DISP++, sTunicColor.r, sTunicColor.g, sTunicColor.b, 0);
            CLOSE_DISPS(play->state.gfxCtx);
            sHeadTinted = false;
        }
    }
}

extern "C" int LinkGanonCape_PrepareDraw(EnGanonMant* cape, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 recorded = sRecordedLimbs;

    // Consume this frame's shoulder positions
    sRecordedLimbs = 0;
    sHeadValid = (recorded & 4) != 0;

    if (!ShouldWearCape()) {
        Actor_Kill(&cape->actor);
        return false;
    }

    // Hide when Link wasn't drawn this frame, or the camera is inside his head (first person)
    if ((recorded & 3) != 3 || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) || player->unk_6AD == 1 ||
        player->unk_6AD == 2) {
        cape->needsReset = true;
        return false;
    }

    f32 sizeScale = LINK_IS_ADULT ? 1.0f : CHILD_SCALE;
    f32 jointLength = CVarGetFloat(CVAR_CAPE("Length"), 3.5f) * sizeScale;
    f32 width = CVarGetFloat(CVAR_CAPE("Width"), 1.15f);
    f32 backOffset = CVarGetFloat(CVAR_CAPE("BackOffset"), 1.5f) * sizeScale;
    f32 s = jointLength / GANONDORF_JOINT_LENGTH;

    Vec3f right = sRightShoulder;
    Vec3f left = sLeftShoulder;

    // The cape's "forward" is perpendicular to the right->left shoulder line. Make sure that matches
    // the way Link is facing, in case a custom model mirrors the shoulder limbs.
    f32 dx = left.x - right.x;
    f32 dz = left.z - right.z;
    f32 faceX = Math_SinS(player->actor.shape.rot.y);
    f32 faceZ = Math_CosS(player->actor.shape.rot.y);
    if ((-dz * faceX) + (dx * faceZ) < 0.0f) {
        Vec3f tmp = right;
        right = left;
        left = tmp;
        dx = -dx;
        dz = -dz;
    }

    Vec3f mid;
    mid.x = (right.x + left.x) * 0.5f;
    mid.y = (right.y + left.y) * 0.5f;
    mid.z = (right.z + left.z) * 0.5f;

    // Widen the attach points so the cape drapes over the shoulders instead of cutting into them
    right.x = mid.x + (right.x - mid.x) * width;
    right.y = mid.y + (right.y - mid.y) * width;
    right.z = mid.z + (right.z - mid.z) * width;
    left.x = mid.x + (left.x - mid.x) * width;
    left.y = mid.y + (left.y - mid.y) * width;
    left.z = mid.z + (left.z - mid.z) * width;

    // Push them behind Link's back so the cape doesn't start inside his torso
    f32 len = sqrtf(SQ(dx) + SQ(dz));
    if (len > 0.001f) {
        f32 backX = dz / len;
        f32 backZ = -dx / len;
        right.x += backX * backOffset;
        right.z += backZ * backOffset;
        left.x += backX * backOffset;
        left.z += backZ * backOffset;
    }

    // Snap the cape into place if Link teleported (warps, voiding out, respawning, ...)
    if (!cape->needsReset) {
        f32 jumpX = mid.x - cape->anchor.x;
        f32 jumpY = mid.y - cape->anchor.y;
        f32 jumpZ = mid.z - cape->anchor.z;
        if (SQ(jumpX) + SQ(jumpY) + SQ(jumpZ) > SQ(150.0f)) {
            cape->needsReset = true;
        }
    }

    cape->rightShoulderPos = right;
    cape->leftShoulderPos = left;

    // Ganondorf's values while floating/walking around in his fight, scaled down to Link
    cape->jointLength = jointLength;
    cape->gravity = -3.0f * s;
    cape->backPush = -3.0f * s;
    cape->backSwayMagnitude = 0.25f * s;
    cape->sideSwayMagnitude = -3.0f * s;
    cape->minDist = 20.0f * s;
    cape->velocityClamp = 5.0f * s;

    // Lie on the floor instead of sinking through it
    if (player->actor.floorHeight > BGCHECK_Y_MIN + 1.0f) {
        cape->minY = player->actor.floorHeight + 0.5f;
    } else {
        cape->minY = BGCHECK_Y_MIN;
    }

    // minDist pushes the cape away from this point
    cape->actor.world.pos = player->actor.world.pos;

    return true;
}

static EnGanonMant* FindLinkCape(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_EN_GANON_MANT && actor->params == LINK_GANON_CAPE_PARAMS && actor->update != NULL) {
            return (EnGanonMant*)actor;
        }
    }
    return NULL;
}

static bool IsGanondorfLoaded(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_GANON || actor->id == ACTOR_EN_GANON_MANT) {
            return true;
        }
    }
    return false;
}

static void LinkGanonCape_OnPlayerUpdate() {
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }

    Player* player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    EnGanonMant* cape = FindLinkCape(play);
    sCapeActive = false;

    if (!ShouldWearCape() || IsGanondorfLoaded(play)) {
        if (cape != NULL) {
            Actor_Kill(&cape->actor);
        }
        return;
    }

    if (cape == NULL) {
        // Not spawned as Link's child: Player treats actor.child as the actor he's holding
        cape = (EnGanonMant*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_GANON_MANT, player->actor.world.pos.x,
                                         player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0,
                                         LINK_GANON_CAPE_PARAMS);
        if (cape == NULL) {
            return;
        }
        cape->actor.parent = &player->actor;
        // Keep it through room changes, and draw it after Link (ACTORCAT_MISC comes after ACTORCAT_PLAYER)
        cape->actor.room = -1;
        Actor_ChangeCategory(play, &play->actorCtx, &cape->actor, ACTORCAT_MISC);
    }

    cape->actor.world.pos = player->actor.world.pos;
    sCapeActive = true;
}

// ---------------------------------------------------------------------------------------------
// Hood
// ---------------------------------------------------------------------------------------------

#define HOOD_COLS GANON_MANT_NUM_STRANDS // one column per cape strand
#define HOOD_ROWS GANON_MANT_NUM_JOINTS  // same grid as the cape so gMantDL can draw it
#define HOOD_SHELL_ROWS 9                // rows 0..8 are rigid on the head, rows 9..11 drape to the collar

// clang-format off
// Generated by tools/hoodbake.py from gLinkAdultHeadNearDL / gLinkChildHeadNearDL in oot.o2r (SoH 9.3.1).
// Head-limb space, model units: x = forward, y = down, z = side. [age][row][column] = { x, y, z }
// Row 0 is the brim around the face, row 8 the bottom of the shell. Column 0 is on the -z side.
static const s16 sHoodShell[2][HOOD_SHELL_ROWS][HOOD_COLS][3] = {
    {
        // adult
        { { 760, -476, -595 }, { 901, -594, -575 }, { 1009, -723, -518 }, { 1150, -852, -443 }, { 1244, -955, -295 }, { 1281, -1007, -102 }, { 1272, -1004, 101 }, { 1222, -947, 287 }, { 1075, -832, 399 }, { 1000, -722, 510 }, { 890, -596, 564 }, { 747, -482, 578 } },
        { { 723, -449, -592 }, { 766, -634, -623 }, { 799, -854, -600 }, { 842, -1058, -507 }, { 886, -1210, -334 }, { 911, -1284, -112 }, { 897, -1269, 109 }, { 845, -1170, 309 }, { 823, -1043, 488 }, { 802, -855, 603 }, { 762, -635, 618 }, { 716, -453, 583 } },
        { { 689, -422, -592 }, { 629, -667, -627 }, { 582, -940, -605 }, { 552, -1180, -507 }, { 549, -1352, -329 }, { 556, -1433, -107 }, { 550, -1417, 105 }, { 541, -1331, 319 }, { 548, -1173, 500 }, { 591, -948, 625 }, { 646, -665, 658 }, { 685, -425, 586 } },
        { { 653, -397, -588 }, { 513, -692, -616 }, { 396, -998, -582 }, { 289, -1232, -458 }, { 230, -1396, -287 }, { 212, -1476, -90 }, { 213, -1474, 89 }, { 230, -1394, 286 }, { 289, -1247, 471 }, { 405, -1023, 631 }, { 541, -691, 690 }, { 658, -393, 595 } },
        { { 574, -416, -507 }, { 470, -615, -620 }, { 303, -919, -619 }, { 97, -1189, -494 }, { -38, -1321, -267 }, { -126, -1436, -80 }, { -126, -1436, 80 }, { -40, -1324, 268 }, { 91, -1204, 509 }, { 306, -944, 689 }, { 497, -603, 708 }, { 628, -364, 601 } },
        { { 556, -382, -527 }, { 429, -533, -633 }, { 239, -780, -768 }, { -38, -1013, -645 }, { -281, -1160, -412 }, { -384, -1204, -125 }, { -384, -1204, 125 }, { -281, -1159, 412 }, { -37, -1013, 643 }, { 239, -780, 769 }, { 451, -509, 724 }, { 599, -333, 607 } },
        { { 533, -351, -539 }, { 388, -441, -656 }, { 169, -593, -786 }, { -127, -758, -689 }, { -375, -849, -445 }, { -396, -857, -125 }, { -396, -857, 125 }, { -374, -849, 444 }, { -130, -758, 695 }, { 169, -592, 788 }, { 401, -410, 736 }, { 573, -297, 623 } },
        { { 511, -315, -556 }, { 336, -355, -646 }, { 112, -416, -709 }, { -159, -488, -636 }, { -384, -531, -422 }, { -379, -569, -120 }, { -375, -570, 119 }, { -384, -531, 422 }, { -161, -487, 638 }, { 105, -404, 739 }, { 342, -315, 720 }, { 544, -260, 636 } },
        { { 482, -286, -562 }, { 280, -279, -609 }, { 67, -264, -592 }, { -132, -251, -497 }, { -260, -264, -315 }, { -343, -257, -111 }, { -343, -257, 111 }, { -249, -272, 309 }, { -138, -244, 504 }, { 53, -234, 632 }, { 280, -230, 679 }, { 510, -230, 639 } },
    },
    {
        // child
        { { 661, -362, -572 }, { 798, -501, -554 }, { 944, -657, -535 }, { 1020, -792, -420 }, { 1075, -892, -268 }, { 1105, -945, -93 }, { 1089, -936, 91 }, { 1042, -875, 257 }, { 993, -782, 405 }, { 883, -643, 487 }, { 800, -501, 557 }, { 675, -357, 588 } },
        { { 627, -325, -573 }, { 664, -526, -612 }, { 675, -764, -571 }, { 688, -962, -460 }, { 702, -1094, -290 }, { 715, -1158, -96 }, { 714, -1157, 96 }, { 702, -1095, 291 }, { 682, -957, 455 }, { 671, -762, 567 }, { 666, -526, 616 }, { 646, -317, 596 } },
        { { 593, -289, -573 }, { 527, -545, -633 }, { 448, -838, -581 }, { 387, -1051, -448 }, { 367, -1201, -281 }, { 367, -1272, -91 }, { 367, -1275, 91 }, { 368, -1203, 282 }, { 387, -1051, 447 }, { 444, -833, 571 }, { 528, -545, 636 }, { 615, -277, 602 } },
        { { 560, -254, -574 }, { 414, -544, -643 }, { 246, -873, -543 }, { 124, -1092, -409 }, { 56, -1233, -248 }, { 32, -1300, -76 }, { 32, -1300, 77 }, { 56, -1233, 247 }, { 125, -1088, 406 }, { 244, -867, 533 }, { 408, -544, 626 }, { 580, -239, 603 } },
        { { 524, -221, -569 }, { 359, -456, -618 }, { 168, -760, -591 }, { -62, -1037, -492 }, { -202, -1143, -262 }, { -270, -1202, -76 }, { -270, -1202, 76 }, { -202, -1143, 262 }, { -61, -1036, 491 }, { 168, -757, 584 }, { 361, -455, 623 }, { 543, -205, 599 } },
        { { 486, -192, -561 }, { 311, -372, -606 }, { 107, -607, -655 }, { -176, -827, -584 }, { -361, -917, -337 }, { -332, -871, -83 }, { -332, -871, 83 }, { -360, -917, 336 }, { -175, -827, 583 }, { 107, -607, 657 }, { 317, -365, 630 }, { 504, -174, 590 } },
        { { 413, -210, -484 }, { 261, -293, -584 }, { 45, -436, -663 }, { -203, -575, -535 }, { -320, -630, -289 }, { -331, -635, -81 }, { -331, -635, 81 }, { -320, -630, 289 }, { -203, -575, 535 }, { 45, -436, 664 }, { 265, -282, 608 }, { 462, -146, 577 } },
        { { 375, -195, -463 }, { 209, -229, -535 }, { 0, -282, -582 }, { -205, -362, -465 }, { -335, -409, -279 }, { -319, -440, -78 }, { -319, -440, 78 }, { -335, -409, 279 }, { -206, -362, 465 }, { -1, -281, 583 }, { 211, -218, 553 }, { 386, -178, 487 } },
        { { 344, -174, -454 }, { 160, -170, -488 }, { -22, -177, -450 }, { -163, -196, -347 }, { -231, -231, -203 }, { -321, -206, -76 }, { -322, -206, 76 }, { -232, -231, 204 }, { -164, -196, 348 }, { -23, -174, 453 }, { 160, -165, 494 }, { 347, -167, 462 } },
    },
};
// Point the hood is scaled around by the Hood Size option (head-limb space)
static const Vec3f sHoodCenter[2] = { { 280.0f, -700.0f, 0.0f }, { 160.0f, -540.0f, 0.0f } };
// clang-format on

// How far the drape rows bow outwards between the shell and the collar, world units (adult, child)
static const f32 sHoodDrapeBulge[2] = { 1.0f, 0.64f };

// The cape's texture coordinates (from gMant1Vtx): v goes across the strands, u down the cape and the
// top row is u = 186. Continue upwards from there so the texture flows from hood into cape.
#define HOOD_TC_V(col) (-482 + 241 * (col))
#define HOOD_TC_U(row) (186 * ((row) - (HOOD_ROWS - 2)))

static Vec3f Vec_Normalize(Vec3f v) {
    f32 len = sqrtf(SQ(v.x) + SQ(v.y) + SQ(v.z));
    if (len < 0.00001f) {
        return v;
    }
    v.x /= len;
    v.y /= len;
    v.z /= len;
    return v;
}

static f32 Vec_Dot(Vec3f a, Vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3f Vec_Cross(Vec3f a, Vec3f b) {
    Vec3f out = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    return out;
}

static Vec3f Vec_Lerp(Vec3f a, Vec3f b, f32 t) {
    Vec3f out = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    return out;
}

// Baked shell point -> world, scaled around the hood's center by the Hood Size option
static Vec3f Hood_ShellToWorld(s32 age, s32 row, s32 col, f32 size) {
    const s16* p = sHoodShell[age][row][col];
    const Vec3f* center = &sHoodCenter[age];
    Vec3f local;
    Vec3f world;

    local.x = center->x + (p[0] - center->x) * size;
    local.y = center->y + (p[1] - center->y) * size;
    local.z = center->z + (p[2] - center->z) * size;
    SkinMatrix_Vec3fMtxFMultXYZ(&sHeadMtx, &local, &world);
    return world;
}

static Vec3f Hood_CapeNormal(EnGanonMant* cape, s32 strand) {
    // Same formula as EnGanonMant_UpdateVertices, for joint 0
    Vec3f up = { 0.0f, 30.0f, 0.0f };
    Vec3f normal;

    Matrix_Push();
    Matrix_RotateY(cape->strands[strand].rotations[0].y, MTXMODE_NEW);
    Matrix_RotateX(cape->strands[strand].rotations[0].x, MTXMODE_APPLY);
    Matrix_MultVec3f(&up, &normal);
    Matrix_Pop();
    return normal;
}

extern "C" void* LinkGanonCape_BuildHood(EnGanonMant* cape, PlayState* play) {
    static Vec3f sPos[HOOD_ROWS][HOOD_COLS];

    if (!HoodEnabled() || !sHeadValid) {
        return NULL;
    }

    s32 age = LINK_IS_ADULT ? 0 : 1;
    f32 size = CVarGetFloat(CVAR_CAPE("HoodSize"), 1.0f);

    // Which side of the head is the cape's strand 0 on? Mirror the hood to match.
    Vec3f side0 = Hood_ShellToWorld(age, HOOD_SHELL_ROWS - 1, 0, size);
    Vec3f* root0 = &cape->strands[0].root;
    Vec3f* root11 = &cape->strands[HOOD_COLS - 1].root;
    bool mirror = (SQ(side0.x - root0->x) + SQ(side0.y - root0->y) + SQ(side0.z - root0->z)) >
                  (SQ(side0.x - root11->x) + SQ(side0.y - root11->y) + SQ(side0.z - root11->z));

    Vec3f headCenter;
    SkinMatrix_Vec3fMtxFMultXYZ(&sHeadMtx, (Vec3f*)&sHoodCenter[age], &headCenter);

    for (s32 col = 0; col < HOOD_COLS; col++) {
        s32 shellCol = mirror ? (HOOD_COLS - 1 - col) : col;

        // Rigid shell on the head
        for (s32 row = 0; row < HOOD_SHELL_ROWS; row++) {
            sPos[row][col] = Hood_ShellToWorld(age, row, shellCol, size);
        }

        // Drape down to the cape's collar. The last row is exactly the strand root, which is also the
        // cape's first vertex row, so hood and cape share their edge.
        Vec3f top = sPos[HOOD_SHELL_ROWS - 1][col];
        Vec3f root = cape->strands[col].root;
        for (s32 row = HOOD_SHELL_ROWS; row < HOOD_ROWS; row++) {
            f32 t = (f32)(row - (HOOD_SHELL_ROWS - 1)) / (HOOD_ROWS - HOOD_SHELL_ROWS);
            Vec3f p = Vec_Lerp(top, root, t);

            if (row != HOOD_ROWS - 1) {
                Vec3f out = { p.x - cape->anchor.x, 0.0f, p.z - cape->anchor.z };
                out = Vec_Normalize(out);
                f32 bulge = sHoodDrapeBulge[age] * sinf(t * M_PI);
                p.x += out.x * bulge;
                p.z += out.z * bulge;
            }
            sPos[row][col] = p;
        }
    }

    // Normals from the grid, pointing away from the head
    static Vec3f sNormal[HOOD_ROWS][HOOD_COLS];
    f32 agreement = 0.0f;

    for (s32 row = 0; row < HOOD_ROWS; row++) {
        for (s32 col = 0; col < HOOD_COLS; col++) {
            Vec3f* rowPrev = &sPos[MAX(row - 1, 0)][col];
            Vec3f* rowNext = &sPos[MIN(row + 1, HOOD_ROWS - 1)][col];
            Vec3f* colPrev = &sPos[row][MAX(col - 1, 0)];
            Vec3f* colNext = &sPos[row][MIN(col + 1, HOOD_COLS - 1)];
            Vec3f dRow = { rowNext->x - rowPrev->x, rowNext->y - rowPrev->y, rowNext->z - rowPrev->z };
            Vec3f dCol = { colNext->x - colPrev->x, colNext->y - colPrev->y, colNext->z - colPrev->z };
            Vec3f n = Vec_Normalize(Vec_Cross(dRow, dCol));
            Vec3f away = { sPos[row][col].x - headCenter.x, sPos[row][col].y - headCenter.y,
                           sPos[row][col].z - headCenter.z };

            if (Vec_Dot(n, away) < 0.0f) {
                n.x = -n.x;
                n.y = -n.y;
                n.z = -n.z;
            }
            sNormal[row][col] = n;
        }
    }

    // Match the cape's normal convention so the shading carries straight across the seam
    Vec3f capeNormals[HOOD_COLS];
    for (s32 col = 0; col < HOOD_COLS; col++) {
        capeNormals[col] = Hood_CapeNormal(cape, col);
        agreement += Vec_Dot(capeNormals[col], sNormal[HOOD_ROWS - 1][col]);
    }
    f32 flip = (agreement < 0.0f) ? -30.0f : 30.0f;

    Vtx* vertices = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * HOOD_ROWS * HOOD_COLS);

    for (s32 row = 0; row < HOOD_ROWS; row++) {
        for (s32 col = 0; col < HOOD_COLS; col++) {
            // Same vertex layout as the cape (see sVerticesMap in z_en_ganon_mant.c)
            Vtx* vtx = &vertices[(HOOD_COLS - 1 - col) + HOOD_COLS * row];
            Vec3f* p = &sPos[row][col];

            vtx->n.ob[0] = CLAMP((p->x - cape->anchor.x) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vtx->n.ob[1] = CLAMP((p->y - cape->anchor.y) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vtx->n.ob[2] = CLAMP((p->z - cape->anchor.z) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vtx->n.flag = 0;
            vtx->n.tc[0] = HOOD_TC_U(row);
            vtx->n.tc[1] = HOOD_TC_V(col);

            if (row == HOOD_ROWS - 1) {
                vtx->n.n[0] = capeNormals[col].x;
                vtx->n.n[1] = capeNormals[col].y;
                vtx->n.n[2] = capeNormals[col].z;
            } else {
                vtx->n.n[0] = sNormal[row][col].x * flip;
                vtx->n.n[1] = sNormal[row][col].y * flip;
                vtx->n.n[2] = sNormal[row][col].z * flip;
            }
            vtx->n.a = 255;
        }
    }

    return vertices;
}

static void RegisterLinkGanonCape() {
    // Always registered so turning the setting off also removes the cape
    COND_HOOK(OnPlayerUpdate, true, LinkGanonCape_OnPlayerUpdate);

    // Ganondorf brings his own cape, which shares the same buffers. Remove Link's right away.
    COND_ID_HOOK(OnActorInit, ACTOR_BOSS_GANON, true, [](void* actorPtr) {
        if (gPlayState == NULL) {
            return;
        }
        EnGanonMant* cape = FindLinkCape(gPlayState);
        if (cape != NULL) {
            Actor_Kill(&cape->actor);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterLinkGanonCape, {});
