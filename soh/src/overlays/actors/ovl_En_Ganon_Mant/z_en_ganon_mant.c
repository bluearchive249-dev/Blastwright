/*
 * File: z_en_ganon_mant.c
 * Overlay: ovl_En_Ganon_Mant
 * Description: Ganondorf's Cape
 */

#include "z_en_ganon_mant.h"
#include <string.h>
#include "overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include "soh/Enhancements/Graphics/LinkGanonCape.h"
#include "assets/objects/object_tw/object_tw.h" // SoH: Koume's burning hair, for the Goron Tunic's cape
#include "assets/objects/gameplay_keep/gameplay_keep.h" // SoH: ...and the burning enemies' fire
#include "soh/Enhancements/ExtraModes/GanondorfTuning.h"
#include "textures/icon_item_static/icon_item_static.h" // SoH: the tunics' icons as his capes
#include "textures/item_name_static/item_name_static.h"
#include "assets/objects/object_fd/object_fd.h"             // SoH: ...and Volvagia's fiery mane
#include "assets/objects/object_spot00_objects/object_spot00_objects.h" // SoH: ...and the castle drawbridge's chain

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void EnGanonMant_Init(Actor* thisx, PlayState* play);
void EnGanonMant_Destroy(Actor* thisx, PlayState* play);
void EnGanonMant_Update(Actor* thisx, PlayState* play);
void EnGanonMant_Draw(Actor* thisx, PlayState* play);

const ActorInit En_Ganon_Mant_InitVars = {
    ACTOR_EN_GANON_MANT,
    ACTORCAT_BOSS,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnGanonMant),
    (ActorFunc)EnGanonMant_Init,
    (ActorFunc)EnGanonMant_Destroy,
    (ActorFunc)EnGanonMant_Update,
    (ActorFunc)EnGanonMant_Draw,
    NULL,
};

static s16 sTearSizesMedium[] = {
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0,
};

static s16 sTearSizesLarge[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 2, 1, 1, 0, 0, 0, 0,
};

static s16 sTearSizesSmall[] = {
    0, 0, 0, 0, 0, 0, 0,
};

typedef struct {
    s16* tearAreaSizes;
    s16 count;
} TearShape; // size = 0x8

/**
 * The arrays pointed to by this table describe how many pixels should
 * be removed from the cloak texture in a single pass
 */
static TearShape sTearShapes[] = {
    { sTearSizesMedium, ARRAY_COUNT(sTearSizesMedium) },
    { sTearSizesMedium, ARRAY_COUNT(sTearSizesMedium) },
    { sTearSizesLarge, ARRAY_COUNT(sTearSizesLarge) },
    { sTearSizesSmall, ARRAY_COUNT(sTearSizesSmall) },
};

// How much each joint is affected by backwards/forwards swaying motion
static f32 sBackSwayCoefficients[GANON_MANT_NUM_JOINTS] = {
    0.0f, 1.0f, 0.5f, 0.25f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

static f32 D_80A24DB4[] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

// How much each joint is affected by sideways swaying motion, tends to 0
static f32 sSideSwayCoefficients[GANON_MANT_NUM_JOINTS] = {
    0.0f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.0f,
};

static f32 D_80A24E00[] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

static f32 sDistMultipliers[GANON_MANT_NUM_JOINTS] = {
    0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f,
};

static f32 D_80A24E48[] = {
    1.8f,
};

#define MAP_STRAND_TO_VTX(n)                                                                               \
    (n) + GANON_MANT_NUM_JOINTS * 0, (n) + GANON_MANT_NUM_JOINTS * 1, (n) + GANON_MANT_NUM_JOINTS * 2,     \
        (n) + GANON_MANT_NUM_JOINTS * 3, (n) + GANON_MANT_NUM_JOINTS * 4, (n) + GANON_MANT_NUM_JOINTS * 5, \
        (n) + GANON_MANT_NUM_JOINTS * 6, (n) + GANON_MANT_NUM_JOINTS * 7, (n) + GANON_MANT_NUM_JOINTS * 8, \
        (n) + GANON_MANT_NUM_JOINTS * 9, (n) + GANON_MANT_NUM_JOINTS * 10, (n) + GANON_MANT_NUM_JOINTS * 11

static u16 sVerticesMap[GANON_MANT_NUM_STRANDS * GANON_MANT_NUM_JOINTS] = {
    MAP_STRAND_TO_VTX(11), MAP_STRAND_TO_VTX(10), MAP_STRAND_TO_VTX(9), MAP_STRAND_TO_VTX(8),
    MAP_STRAND_TO_VTX(7),  MAP_STRAND_TO_VTX(6),  MAP_STRAND_TO_VTX(5), MAP_STRAND_TO_VTX(4),
    MAP_STRAND_TO_VTX(3),  MAP_STRAND_TO_VTX(2),  MAP_STRAND_TO_VTX(1), MAP_STRAND_TO_VTX(0),
};

#define MANT_TEX_WIDTH 32
#define MANT_TEX_HEIGHT 64

static u8 sMaskTex[MANT_TEX_WIDTH * MANT_TEX_HEIGHT] = { { 0 } };
static s16 sGdfCapeTears = 0; // SoH: tears worn into the player Ganondorf's cape (EnGanonMant_SetWear)

#include "overlays/ovl_En_Ganon_Mant/ovl_En_Ganon_Mant.h"

// SoH: Phantom Ganon skin for the player Ganondorf ("Play as Ganondorf"). His cape turns black and gray, with a
// ragged hem (some strands end early) and holes worn through it.
static u8 sPhantomMantTex[MANT_TEX_WIDTH * MANT_TEX_HEIGHT * 2];
static const u8 sPhantomHem[GANON_MANT_NUM_STRANDS] = { 12, 9, 11, 8, 12, 10, 7, 11, 9, 12, 8, 10 };

static s32 EnGanonMant_IsPhantom(EnGanonMant* this) {
    return this->ownVertices && CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.PhantomGanon"), 0);
}

// The cape texture in grays, with holes (alpha 0) worn into the middle of the cape and wherever it was torn
static s32 EnGanonMant_BuildPhantomTex(void) {
    u8* src = (u8*)ResourceGetDataByName(gMantTex);
    s32 i;

    if (src == NULL) {
        return false;
    }
    for (i = 0; i < MANT_TEX_WIDTH * MANT_TEX_HEIGHT; i++) {
        s32 tx = i % MANT_TEX_WIDTH;
        s32 ty = i / MANT_TEX_WIDTH;
        u16 c = (src[2 * i] << 8) | src[2 * i + 1];
        s32 r = (c >> 11) & 0x1F;
        s32 g = (c >> 6) & 0x1F;
        s32 b = (c >> 1) & 0x1F;
        s32 gray = (r * 77 + g * 150 + b * 29) >> 8;
        u32 hash = (u32)(tx * 73856093) ^ (u32)(ty * 19349663);
        u8 hole;

        gray = 2 + (gray * 15) / 31;
        // The texture repeats mirrored along the cape: tx near 0 is both the top and the hem, so the holes stay in
        // the middle of the cape
        hash ^= hash >> 13;
        hash *= 0x5BD1E995;
        hash ^= hash >> 15;
        hole = (tx >= 12) && ((hash % 100) < 9);
        if (sMaskTex[i]) {
            hole = true;
        }
        c = (gray << 11) | (gray << 6) | (gray << 1) | (hole ? 0 : 1);
        sPhantomMantTex[2 * i] = c >> 8;
        sPhantomMantTex[2 * i + 1] = c & 0xFF;
    }
    return true;
}

// SoH: double-buffered vertices for the cape that owns them (only the player-controlled Ganondorf's cape).
// Filled from the shared buffers once so texture coordinates and colors match.
#define GANON_MANT_NUM_VTX 144
// (A set each: his cape, and his cloak's two other pieces)
static Vtx sOwnVertices[6][2][GANON_MANT_NUM_VTX];
static u8 sOwnVerticesReady[6] = { false, false, false, false, false, false };

static Vtx* EnGanonMant_GetOwnVertices(EnGanonMant* this) {
    s32 set = ((this->actor.params >= PLAYER_GANONDORF_CLOAK_PARAMS) &&
               (this->actor.params <= PLAYER_GANONDORF_CAPELET3_PARAMS))
                  ? (this->actor.params - PLAYER_GANONDORF_CLOAK_PARAMS + 1)
                  : 0;

    if (!sOwnVerticesReady[set]) {
        Vtx* src1 = ResourceMgr_LoadVtxByName(SEGMENTED_TO_VIRTUAL(gMant1Vtx));
        Vtx* src2 = ResourceMgr_LoadVtxByName(SEGMENTED_TO_VIRTUAL(gMant2Vtx));

        if ((src1 == NULL) || (src2 == NULL)) {
            return NULL;
        }
        memcpy(sOwnVertices[set][0], src1, sizeof(sOwnVertices[set][0]));
        memcpy(sOwnVertices[set][1], src2, sizeof(sOwnVertices[set][1]));
        sOwnVerticesReady[set] = true;
    }

    return sOwnVertices[set][(this->frameTimer % 2 != 0) ? 0 : 1];
}

void EnGanonMant_Init(Actor* thisx, PlayState* play) {
    EnGanonMant* this = (EnGanonMant*)thisx;

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;

    if (this->actor.params == LINK_GANON_CAPE_PARAMS) {
        this->isLinkCape = true;
        this->needsReset = true;
    }

    if ((this->actor.params == PLAYER_GANONDORF_CAPE_PARAMS) ||
        ((this->actor.params >= PLAYER_GANONDORF_CLOAK_PARAMS) &&
         (this->actor.params <= PLAYER_GANONDORF_CAPELET3_PARAMS))) {
        this->ownVertices = true;
    }

    for (int i = 0; i < ARRAY_COUNT(sMaskTex); i++) {
        sMaskTex[i] = 0;
    }
    sGdfCapeTears = 0; // (worn in again from his update)

    Gfx_RegisterBlendedTexture(gMantTex, sMaskTex, NULL);
}

void EnGanonMant_Destroy(Actor* thisx, PlayState* play) {
}

/**
 * SoH: "Play as Ganondorf": his cape wears with Link's health. Each tear always lands in the same place (a fixed
 * sequence), so losing hearts adds tears one by one and healing mends the last ones first.
 */
#define GDF_CAPE_MAX_TEARS 14

static u32 EnGanonMant_GdfNext(u32* seed) {
    *seed = *seed * 1664525u + 1013904223u;
    return *seed >> 8;
}

static f32 EnGanonMant_GdfRand(u32* seed, f32 max) {
    return (EnGanonMant_GdfNext(seed) & 0xFFFF) * (max / 65536.0f);
}

static void EnGanonMant_TearMask(f32 tx, f32 ty, f32 tearAngle, s16 shapeIdx) {
    TearShape* shape = &sTearShapes[shapeIdx];
    f32 tearDirX = sinf(tearAngle);
    f32 tearDirY = cosf(tearAngle);
    s16 i;
    s16 areaX;
    s16 areaY;
    s32 texIdx;

    for (i = 0; i < shape->count; i++) {
        if ((0 <= tx && tx < MANT_TEX_WIDTH) && (0 <= ty && ty < MANT_TEX_HEIGHT)) {
            for (areaX = 0; areaX <= shape->tearAreaSizes[i]; areaX++) {
                for (areaY = 0; areaY <= shape->tearAreaSizes[i]; areaY++) {
                    texIdx = ((s16)tx + ((s16)ty * MANT_TEX_WIDTH)) + (areaX + (areaY * MANT_TEX_WIDTH));
                    if (texIdx < MANT_TEX_WIDTH * MANT_TEX_HEIGHT) {
                        sMaskTex[texIdx] = 1;
                    }
                }
            }
        }
        tx += tearDirX;
        ty += tearDirY;
    }
}

// wear: 0 (whole) to 1 (in tatters)
void EnGanonMant_SetWear(f32 wear) {
    s16 want = (s16)(CLAMP(wear, 0.0f, 1.0f) * GDF_CAPE_MAX_TEARS + 0.5f);
    u32 seed = 0x6A11F00Du;
    s16 n;
    s32 i;

    if (want == sGdfCapeTears) {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sMaskTex); i++) {
        sMaskTex[i] = 0;
    }
    for (n = 0; n < want; n++) {
        f32 tx = EnGanonMant_GdfRand(&seed, MANT_TEX_WIDTH);
        f32 ty = EnGanonMant_GdfRand(&seed, MANT_TEX_HEIGHT);
        f32 angle = EnGanonMant_GdfRand(&seed, 2 * M_PI);
        s16 shapeIdx = (s16)EnGanonMant_GdfRand(&seed, ARRAY_COUNT(sTearShapes) - 0.01f);

        EnGanonMant_TearMask(tx, ty, angle, shapeIdx);
    }
    sGdfCapeTears = want;
}

/**
 * Randomly zeros portions of the cloak texture
 */
void EnGanonMant_Tear(EnGanonMant* this) {
    s32 pad;
    s16 i;
    s16 areaX;
    s16 areaY;
    s16 texIdx;
    f32 tx = Rand_ZeroFloat(MANT_TEX_WIDTH);
    f32 ty = Rand_ZeroFloat(MANT_TEX_HEIGHT);
    f32 tearAngle = Rand_ZeroFloat(2 * M_PI);
    f32 tearDirX = sinf(tearAngle);
    f32 tearDirY = cosf(tearAngle);
    TearShape* shape = &sTearShapes[(s16)Rand_ZeroFloat(ARRAY_COUNT(sTearShapes) - 0.01f)];
    s16 count = shape->count;
    s16* tearAreaSizes = shape->tearAreaSizes;

    for (i = 0; i < count; i++) {
        if ((0 <= tx && tx < MANT_TEX_WIDTH) && (0 <= ty && ty < MANT_TEX_HEIGHT)) {
            for (areaX = 0; areaX <= tearAreaSizes[i]; areaX++) {
                texIdx = 0;
                for (areaY = 0; areaY <= tearAreaSizes[i]; areaY++) {
                    texIdx = (s16)((s16)tx + ((s16)ty * MANT_TEX_WIDTH)) + ((s16)areaX + ((s16)areaY * MANT_TEX_WIDTH));
                    if (texIdx < MANT_TEX_WIDTH * MANT_TEX_HEIGHT) {
                        sMaskTex[texIdx] = 1;
                    }
                }
            }
        }
        tx += tearDirX;
        ty += tearDirY;
    }

    for (i = 0; i < 4; i++) {
        this->strands[(s16)Rand_ZeroFloat(GANON_MANT_NUM_STRANDS - 0.1f)]
            .torn[(s16)Rand_ZeroFloat(GANON_MANT_NUM_JOINTS - 0.1f)] = true;
    }
}

/**
 * Updates the dynamic strands that control the shape and motion of the cloak
 */
void EnGanonMant_UpdateStrand(PlayState* play, EnGanonMant* this, Vec3f* root, Vec3f* pos, Vec3f* nextPos, Vec3f* rot,
                              Vec3f* vel, s16 strandNum) {
    f32 xDiff;
    f32 zDiff;
    f32 gravity;
    s32 pad[4];
    f32 yaw;
    s16 i;
    f32 x;
    f32 y;
    f32 z;
    f32 jointLength;
    Vec3f delta;
    Vec3f posStep;
    Vec3f backSwayOffset;
    Vec3f sideSwayOffset;

    delta.y = 0;
    if (this->actor.params == 0x23) {
        // Pushes all the strands away from the actor
        delta.x = 0.0f;
        delta.z = -30.0f;
        Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_NEW);
        Matrix_MultVec3f(&delta, &posStep);
        for (i = 0; i < GANON_MANT_NUM_JOINTS; i++) {
            (pos + i)->x += posStep.x;
            (pos + i)->z += posStep.z;
        }
        // Set length
        jointLength = 6.5f;
    } else if (this->jointLength > 0.0f) {
        // SoH: Link's cape uses a shorter, configurable length
        jointLength = this->jointLength;
    } else {
        jointLength = 9.5f;
    }

    for (i = 0; i < GANON_MANT_NUM_JOINTS; i++, pos++, vel++, rot++, nextPos++) {
        if (i == 0) {
            // Constraint: first position is always root
            pos->x = root->x;
            pos->y = root->y;
            pos->z = root->z;
        } else {
            // Decelerate
            Math_ApproachZeroF(&vel->x, 1.0f, 0.1f);
            Math_ApproachZeroF(&vel->y, 1.0f, 0.1f);
            Math_ApproachZeroF(&vel->z, 1.0f, 0.1f);

            // Push the cloak away from attached actor, plus oscillations
            delta.x = 0;
            delta.z = (this->backPush + (sinf((strandNum * (2 * M_PI)) / 2.1f) * this->backSwayMagnitude)) *
                      sBackSwayCoefficients[i];
            Matrix_RotateY(this->baseYaw, MTXMODE_NEW);
            Matrix_MultVec3f(&delta, &backSwayOffset);

            // Push the cloak out to either side, in a swaying manner
            delta.x = cosf((strandNum * M_PI) / (GANON_MANT_NUM_STRANDS - 1.0f)) * this->sideSwayMagnitude *
                      sSideSwayCoefficients[i];
            delta.z = 0;
            Matrix_MultVec3f(&delta, &sideSwayOffset);

            // SoH: the player-controlled Ganondorf's cape: its hem flared wide (its edge strands pushed out to either
            // side, more the lower the joint), and its billow (a wave rolling down it from the collar, swelling out
            // behind him and fluttering its sides)
            if ((this->hemFlare != 0.0f) || (this->billow != 0.0f)) {
                f32 down = i / (GANON_MANT_NUM_JOINTS - 1.0f);
                f32 across = cosf((strandNum * M_PI) / (GANON_MANT_NUM_STRANDS - 1.0f));
                f32 wave = sinf(this->billowPhase - i * 0.55f + strandNum * 0.35f);
                Vec3f extra;

                delta.x = -across * (this->hemFlare * down * down +
                                     this->billow * 0.6f * sinf(this->billowPhase * 1.3f - i * 0.45f) * down);
                delta.z = -this->billow * (0.55f + 0.45f * wave) * down;
                Matrix_MultVec3f(&delta, &extra);
                sideSwayOffset.x += extra.x;
                sideSwayOffset.z += extra.z;
            }

            // Calculate position difference
            gravity = this->gravity;
            x = ((pos->x + vel->x) - (pos - 1)->x) + (backSwayOffset.x + sideSwayOffset.x);
            y = ((pos->y + vel->y) - (pos - 1)->y) + gravity;
            z = ((pos->z + vel->z) - (pos - 1)->z) + (backSwayOffset.z + sideSwayOffset.z);

            // Calculate rotations in the direction of the position difference
            yaw = Math_Atan2F(z, x);
            x = -Math_Atan2F(sqrtf(SQ(x) + SQ(z)), y);
            (rot - 1)->x = x;

            // Calculate real position difference of correct length in the correct direction
            delta.x = 0;
            delta.z = jointLength;
            Matrix_RotateY(yaw, MTXMODE_NEW);
            Matrix_RotateX(x, MTXMODE_APPLY);
            Matrix_MultVec3f(&delta, &posStep);

            // Save position
            x = pos->x;
            y = pos->y;
            z = pos->z;

            // Calculate next position
            pos->x = (pos - 1)->x + posStep.x;
            pos->y = (pos - 1)->y + posStep.y;
            pos->z = (pos - 1)->z + posStep.z;

            // Pushes the cloak away from the actor if it is too close
            xDiff = pos->x - this->actor.world.pos.x;
            zDiff = pos->z - this->actor.world.pos.z;
            if (sqrtf(SQ(xDiff) + SQ(zDiff)) < (sDistMultipliers[i] * this->minDist)) {
                yaw = Math_Atan2F(zDiff, xDiff);
                delta.z = this->minDist * sDistMultipliers[i];
                Matrix_RotateY(yaw, MTXMODE_NEW);
                Matrix_MultVec3f(&delta, &posStep);
                pos->x = this->actor.world.pos.x + posStep.x;
                pos->z = this->actor.world.pos.z + posStep.z;
            }

            // SoH: his body (the player-controlled Ganondorf's cloak): out of every capsule, so his torso and arms
            // carry the cloth instead of passing through it
            if (this->capsules != NULL) {
                s16 c;

                for (c = 0; c < this->capsuleCount; c++) {
                    const GanonMantCapsule* cap = &this->capsules[c];
                    f32 abx = cap->b.x - cap->a.x;
                    f32 aby = cap->b.y - cap->a.y;
                    f32 abz = cap->b.z - cap->a.z;
                    f32 len2 = SQ(abx) + SQ(aby) + SQ(abz);
                    f32 t = 0.0f;
                    f32 dx;
                    f32 dy;
                    f32 dz;
                    f32 d;

                    if (len2 > 0.0001f) {
                        t = ((pos->x - cap->a.x) * abx + (pos->y - cap->a.y) * aby + (pos->z - cap->a.z) * abz) / len2;
                        t = CLAMP(t, 0.0f, 1.0f);
                    }
                    dx = pos->x - (cap->a.x + abx * t);
                    dy = pos->y - (cap->a.y + aby * t);
                    dz = pos->z - (cap->a.z + abz * t);
                    d = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
                    if (d < cap->r) {
                        if (d < 0.001f) {
                            // (right on the bone: out the way the strand hangs from its root)
                            dx = pos->x - root->x;
                            dy = 0.0f;
                            dz = pos->z - root->z;
                            d = sqrtf(SQ(dx) + SQ(dz));
                            if (d < 0.001f) {
                                continue;
                            }
                        }
                        pos->x = (cap->a.x + abx * t) + dx / d * cap->r;
                        pos->y = (cap->a.y + aby * t) + dy / d * cap->r;
                        pos->z = (cap->a.z + abz * t) + dz / d * cap->r;
                    }
                }
            }

            // Enforces minY constraint
            if (pos->y < this->minY) {
                pos->y = this->minY;

                // SoH: "Play as Ganondorf", the player's long cape: where it lies on the ground it pools out behind him
                // like a train, each strand carrying on outward and back from his body (the middle straight back, the
                // edges fanning out to the back corners), instead of smearing sideways across the floor
                if ((this->actor.params == PLAYER_GANONDORF_CAPE_PARAMS) && (GdfTune("ground.spread", 1.0f) != 0.0f)) {
                    f32 back = GdfTune("ground.back", 1.2f);
                    f32 ry = Math_SinS(this->actor.shape.rot.y);
                    f32 rx = Math_CosS(this->actor.shape.rot.y);
                    f32 ox = (pos - 1)->x - this->actor.world.pos.x;
                    f32 oz = (pos - 1)->z - this->actor.world.pos.z;
                    f32 ol = sqrtf(SQ(ox) + SQ(oz));
                    f32 dirX;
                    f32 dirZ;
                    f32 dl;
                    f32 dy = (pos - 1)->y - pos->y;
                    f32 flat = SQ(jointLength) - SQ(dy);

                    if (ol > 0.001f) {
                        ox /= ol;
                        oz /= ol;
                    }
                    // (outward from him, plus backward: his facing is (sin, cos) of his yaw)
                    dirX = ox - ry * back;
                    dirZ = oz - rx * back;
                    dl = sqrtf(SQ(dirX) + SQ(dirZ));
                    if ((dl > 0.001f) && (flat > 0.0f)) {
                        f32 w = GdfTune("ground.strength", 0.6f);

                        flat = sqrtf(flat);
                        pos->x += (((pos - 1)->x + dirX / dl * flat) - pos->x) * w;
                        pos->z += (((pos - 1)->z + dirZ / dl * flat) - pos->z) * w;
                    }
                }
            }

            // Calculate next velocity
            vel->x = (pos->x - x) * 80.0f / 100.0f;
            vel->y = (pos->y - y) * 80.0f / 100.0f;
            vel->z = (pos->z - z) * 80.0f / 100.0f;

            if (this->actor.params != 0x23) {
                f32 velClamp = (this->velocityClamp > 0.0f) ? this->velocityClamp : 5.0f;

                // Clamp elements of vel into [-5.0, 5.0]
                if (vel->x > velClamp) {
                    vel->x = velClamp;
                } else if (vel->x < -velClamp) {
                    vel->x = -velClamp;
                }
                if (vel->y > velClamp) {
                    vel->y = velClamp;
                } else if (vel->y < -velClamp) {
                    vel->y = -velClamp;
                }
                if (vel->z > velClamp) {
                    vel->z = velClamp;
                } else if (vel->z < -velClamp) {
                    vel->z = -velClamp;
                }
            }

            // update angle
            xDiff = pos->x - nextPos->x;
            zDiff = pos->z - nextPos->z;
            (rot - 1)->y = Math_Atan2F(zDiff, xDiff);
        }
    }
    rot[11].y = rot[10].y;
    rot[11].x = rot[10].x;
}

/**
 * Update the cloak vertices using the current state of the strands
 */
void EnGanonMant_UpdateVertices(EnGanonMant* this) {
    s16 i;
    Vtx* vtx;
    Vtx* vertices;
    MantStrand* strand;
    s16 j;
    s16 k;
    Vec3f up;
    Vec3f normal;

    if (this->frameTimer % 2 != 0) {
        vertices = SEGMENTED_TO_VIRTUAL(gMant1Vtx);
    } else {
        vertices = SEGMENTED_TO_VIRTUAL(gMant2Vtx);
    }

    vertices = ResourceMgr_LoadVtxByName(vertices);

    // SoH: the player-controlled Ganondorf's cape writes to its own copy, so two capes can be drawn at once
    if (this->ownVertices) {
        Vtx* own = EnGanonMant_GetOwnVertices(this);

        if (own != NULL) {
            vertices = own;
        }
    }

    up.x = 0.0f;
    up.y = 30.0f;
    up.z = 0.0f;

    strand = &this->strands[0];
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++, strand++) {
        for (j = 0, k = 0; j < GANON_MANT_NUM_JOINTS; j++, k += GANON_MANT_NUM_JOINTS) {
            s16 jp = j;

            // SoH: Phantom Ganon's ragged hem, this strand ends early
            if (EnGanonMant_IsPhantom(this) && (j >= sPhantomHem[i])) {
                jp = sPhantomHem[i] - 1;
            }
            vtx = &vertices[sVerticesMap[i + k]];
            // SoH: the player-controlled Ganondorf's cape is stored relative to an anchor too, so the 60 fps
            // interpolation moves it smoothly together with him instead of in 20 fps steps
            if (this->isLinkCape || this->ownVertices) {
                // SoH: store relative to the anchor at higher precision; the small cape would
                // visibly snap to whole units otherwise
                vtx->n.ob[0] = CLAMP((strand->joints[jp].x - this->anchor.x) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
                vtx->n.ob[1] = CLAMP((strand->joints[jp].y - this->anchor.y) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
                vtx->n.ob[2] = CLAMP((strand->joints[jp].z - this->anchor.z) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            } else {
                vtx->n.ob[0] = strand->joints[jp].x;
                vtx->n.ob[1] = strand->joints[jp].y;
                vtx->n.ob[2] = strand->joints[jp].z;
            }
            Matrix_RotateY(strand->rotations[jp].y, MTXMODE_NEW);
            Matrix_RotateX(strand->rotations[jp].x, MTXMODE_APPLY);
            Matrix_MultVec3f(&up, &normal);
            vtx->n.n[0] = normal.x;
            vtx->n.n[1] = normal.y;
            vtx->n.n[2] = normal.z;
        }
    }
}

// SoH: "Play as Ganondorf", the elemental capes meeting their opposites. Under water the flame cape fizzles: bubbles
// stream off it and steam hisses up where it meets the surface. Under water the ice cape freezes the water round it:
// cold mist, frost glints and little shards of ice. In a hot room (Death Mountain's crater, the Fire Temple) the ice
// cape drips as it melts, its hem shedding drops, a haze rising off it. A few points of the cape each frame, its own
// sounds now and then. Only the player's own cape (the flame or ice look).
static void EnGanonMant_ElementFx(EnGanonMant* this, PlayState* play) {
    static u32 sSound = 0;
    // (its sounds loop until they're stopped: each one is stopped again after a short burst, "fx.sound_frames")
    static u16 sLoopSfx = 0;
    static s16 sLoopTimer = 0;
    Player* player = GET_PLAYER(play);
    s32 ice = BossGanon_Player_ZoraLook();
    s32 fire = BossGanon_Player_GoronLook();
    f32 waterY;
    s32 inWater;
    s32 hot = (play->roomCtx.curRoom.behaviorType2 == ROOM_BEHAVIOR_TYPE2_3);
    s32 k;

    if ((this->actor.params == PLAYER_GANONDORF_CAPE_PARAMS) && (sLoopTimer > 0) && (--sLoopTimer == 0)) {
        Audio_StopSfxByPosAndId(&player->actor.projectedPos, sLoopSfx);
    }
    if ((this->actor.params != PLAYER_GANONDORF_CAPE_PARAMS) || !this->ownVertices || (!ice && !fire) ||
        (GdfTune("fx.on", 1.0f) == 0.0f) || (this->actor.draw == NULL)) {
        return;
    }
    inWater = (player->actor.bgCheckFlags & BGCHECKFLAG_WATER) || (player->actor.yDistToWater > 0.0f);
    waterY = player->actor.world.pos.y + player->actor.yDistToWater;
    sSound++;

    for (k = 0; k < (s32)GdfTune("fx.amount", 3.0f); k++) {
        s32 i = Rand_ZeroOne() * GANON_MANT_NUM_STRANDS;
        s32 j = 1 + (s32)(Rand_ZeroOne() * (GANON_MANT_NUM_JOINTS - 1));
        Vec3f p;
        Vec3f zero = { 0.0f, 0.0f, 0.0f };

        i = CLAMP(i, 0, GANON_MANT_NUM_STRANDS - 1);
        j = CLAMP(j, 1, GANON_MANT_NUM_JOINTS - 1);
        p = this->strands[i].joints[j];

        if (inWater && (p.y < waterY)) {
            if (fire) {
                // (fizzling: bubbles off it, steam where the surface is)
                EffectSsBubble_Spawn(play, &p, 0.0f, 4.0f, 4.0f, 0.4f + Rand_ZeroOne() * 0.3f);
                if ((k == 0) && ((sSound % 3) == 0)) {
                    Vec3f steam = { p.x, waterY + 2.0f, p.z };
                    Vec3f up = { 0.0f, 1.2f, 0.0f };

                    EffectSsIceSmoke_Spawn(play, &steam, &up, &zero, 160);
                }
            } else {
                // (freezing: cold mist, glints, a shard now and then)
                Vec3f drift = { Rand_CenteredFloat(0.6f), 0.4f, Rand_CenteredFloat(0.6f) };
                Color_RGBA8 prim = { 220, 245, 255, 255 };
                Color_RGBA8 env = { 100, 180, 255, 255 };

                EffectSsIceSmoke_Spawn(play, &p, &drift, &zero, 120 + Rand_ZeroOne() * 80);
                EffectSsKiraKira_SpawnDispersed(play, &p, &drift, &zero, &prim, &env, 1200, 16);
                if (Rand_ZeroOne() < 0.15f) {
                    Vec3f shard = { Rand_CenteredFloat(2.0f), 1.5f, Rand_CenteredFloat(2.0f) };
                    Vec3f rise = { 0.0f, 0.1f, 0.0f };

                    EffectSsIcePiece_Spawn(play, &p, 0.2f + Rand_ZeroOne() * 0.3f, &shard, &rise, 25);
                }
            }
        } else if (ice && hot) {
            // (melting: drops off the hem, a haze off the rest)
            Vec3f hem = this->strands[i].joints[GANON_MANT_NUM_JOINTS - 1];
            Vec3f fall = { 0.0f, -1.0f, 0.0f };
            Vec3f grav = { 0.0f, -0.4f, 0.0f };
            Color_RGBA8 prim = { 200, 235, 255, 255 };
            Color_RGBA8 env = { 120, 180, 255, 255 };

            f32 dripSize = GdfTune("fx.hot_drip_size", 45.0f);
            s32 d;

            // (more drops and bigger ones, from all along the hem and a few off the cape's lower half)
            for (d = 0; d < (s32)GdfTune("fx.hot_drips", 2.0f); d++) {
                s32 di = CLAMP((s32)(Rand_ZeroOne() * GANON_MANT_NUM_STRANDS), 0, GANON_MANT_NUM_STRANDS - 1);
                s32 dj = GANON_MANT_NUM_JOINTS - 1 - ((Rand_ZeroOne() < 0.3f) ? (s32)(Rand_ZeroOne() * 4.0f) : 0);

                hem = this->strands[di].joints[dj];
                fall.x = Rand_CenteredFloat(0.3f);
                fall.z = Rand_CenteredFloat(0.3f);
                EffectSsDtBubble_SpawnCustomColor(play, &hem, &fall, &grav, &prim, &env,
                                                  dripSize + Rand_ZeroOne() * dripSize * 0.5f, 22, false);
            }
            // (steam: pale clouds rolling up off the whole cape, the game's own dust puffs in white)
            if (Rand_ZeroOne() < GdfTune("fx.steam_chance", 0.7f)) {
                Vec3f rise = { Rand_CenteredFloat(0.4f), 0.6f + Rand_ZeroOne() * 0.6f, Rand_CenteredFloat(0.4f) };
                Vec3f lift = { 0.0f, 0.05f, 0.0f };
                Color_RGBA8 sprim = { 255, 255, 255, (u8)GdfTune("fx.steam_alpha", 120.0f) };
                Color_RGBA8 senv = { 200, 220, 235, 0 };
                s16 ss = (s16)GdfTune("fx.steam_size", 120.0f);

                EffectSsDust_Spawn(play, 1, &p, &rise, &lift, &sprim, &senv, ss, (s16)GdfTune("fx.steam_grow", 8.0f), 22 + Rand_ZeroOne() * 10, 0);
            }
            if ((k == 0) && ((sSound % 4) == 0)) {
                Vec3f up = { 0.0f, 0.8f, 0.0f };

                EffectSsIceSmoke_Spawn(play, &p, &up, &zero, 100);
            }
        }
    }

    // Out of the water: the hem trails its element, more of it the faster he moves. The flame cape sheds Volvagia's
    // own trail flames (the ones off his body as he flies) and embers; the ice cape sheds the same flame in its cold
    // blue (the game's own colour for it), frost glints and a sinking wisp of cold mist. The ice cape also drips now
    // and then everywhere (faster in a hot room, above).
    if (!inWater && (GdfTune("fx.trail", 1.0f) != 0.0f)) {
        f32 speed = CLAMP(player->linearVelocity / 10.0f, 0.0f, 1.0f);
        f32 rate = GdfTune("fx.trail_idle", 0.25f) + (GdfTune("fx.trail_moving", 1.2f) - GdfTune("fx.trail_idle", 0.25f)) * speed;
        f32 size = GdfTune("fx.trail_size", 22.0f);
        s32 n = (s32)rate + ((Rand_ZeroOne() < (rate - (s32)rate)) ? 1 : 0);

        for (k = 0; k < n; k++) {
            s32 i = CLAMP((s32)(Rand_ZeroOne() * GANON_MANT_NUM_STRANDS), 0, GANON_MANT_NUM_STRANDS - 1);
            s32 j = GANON_MANT_NUM_JOINTS - 1 - (s32)(Rand_ZeroOne() * GdfTune("fx.trail_rows", 2.0f));
            Vec3f p;
            Vec3f vel = { Rand_CenteredFloat(0.4f), 0.0f, Rand_CenteredFloat(0.4f) };
            Vec3f acc = { 0.0f, 0.0f, 0.0f };

            j = CLAMP(j, 1, GANON_MANT_NUM_JOINTS - 1);
            p = this->strands[i].joints[j];

            if (fire) {
                acc.y = 0.03f;
                EffectSsKFire_Spawn(play, &p, &vel, &acc, (s16)(size + Rand_ZeroFloat(size * 0.6f)), 100);
                if (Rand_ZeroOne() < GdfTune("fx.ember_chance", 0.2f)) {
                    Vec3f up = { Rand_CenteredFloat(1.5f), 1.0f + Rand_ZeroOne(), Rand_CenteredFloat(1.5f) };
                    Vec3f g = { 0.0f, -0.05f, 0.0f };
                    Color_RGBA8 prim = { 255, 220, 120, 255 };
                    Color_RGBA8 env = { 255, 60, 0, 0 };

                    EffectSsGSpk_SpawnAccel(play, &player->actor, &p, &up, &g, &prim, &env, 60, 0);
                }
            } else {
                acc.y = -0.02f;
                if (GdfTune("fx.ice_flame", 1.0f) != 0.0f) {
                    EffectSsKFire_Spawn(play, &p, &vel, &acc, (s16)(size * 0.8f + Rand_ZeroFloat(size * 0.5f)), 0);
                }
                if (Rand_ZeroOne() < GdfTune("fx.glint_chance", 0.5f)) {
                    Color_RGBA8 prim = { 230, 250, 255, 255 };
                    Color_RGBA8 env = { 100, 190, 255, 255 };

                    EffectSsKiraKira_SpawnDispersed(play, &p, &vel, &acc, &prim, &env, 1000, 20);
                }
                if (Rand_ZeroOne() < GdfTune("fx.mist_chance", 0.1f)) {
                    Vec3f sink = { 0.0f, -0.3f, 0.0f };
                    Vec3f zero = { 0.0f, 0.0f, 0.0f };

                    EffectSsIceSmoke_Spawn(play, &p, &sink, &zero, 90);
                }
            }
        }

        if (ice && !hot && (Rand_ZeroOne() < GdfTune("fx.drip_always", 0.15f))) {
            s32 i = CLAMP((s32)(Rand_ZeroOne() * GANON_MANT_NUM_STRANDS), 0, GANON_MANT_NUM_STRANDS - 1);
            Vec3f hem = this->strands[i].joints[GANON_MANT_NUM_JOINTS - 1];
            Vec3f fall = { 0.0f, -1.0f, 0.0f };
            Vec3f grav = { 0.0f, -0.4f, 0.0f };
            Color_RGBA8 prim = { 200, 235, 255, 255 };
            Color_RGBA8 env = { 120, 180, 255, 255 };

            EffectSsDtBubble_SpawnCustomColor(play, &hem, &fall, &grav, &prim, &env, 25 + Rand_ZeroOne() * 20, 18,
                                              false);
        }
    }

    // Its sounds: the hiss of fire put out, the crackle of water freezing, ice melting (each every couple of seconds)
    if (((sSound % 40) == 0) && (sLoopTimer == 0)) {
        u16 sfx = (inWater && fire) ? NA_SE_EN_EXTINCT
                  : (inWater && ice) ? NA_SE_EV_ICE_FREEZE
                  : (ice && hot)     ? NA_SE_EV_ICE_MELT
                                     : 0;

        if (sfx != 0) {
            Audio_PlayActorSound2(&player->actor, sfx);
            sLoopSfx = sfx;
            sLoopTimer = MAX((s16)GdfTune("fx.sound_frames", 20.0f), 1);
        }
    }
}

void EnGanonMant_Update(Actor* thisx, PlayState* play) {
    EnGanonMant* this = (EnGanonMant*)thisx;
    BossGanon* ganon = (BossGanon*)this->actor.parent;

    this->updateHasRun = true;
    this->frameTimer++;

    if (this->attachLeftArmTimer == 0.0f) {
    } else {
        this->attachLeftArmTimer -= 1.0f;
    }
    if (this->attachRightArmTimer != 0.0f) {
        this->attachRightArmTimer -= 1.0f;
    }
    if (this->attachShouldersTimer != 0.0f) {
        this->attachShouldersTimer -= 1.0f;
    }

    this->actor.shape.rot.y = ganon->actor.shape.rot.y;

    if (this->tearTimer != 0) {
        this->tearTimer--;
        EnGanonMant_Tear(this);
    }
    EnGanonMant_ElementFx(this, play);
}

// SoH: "Play as Ganondorf", the Goron Tunic: his cape is pure flame. The cloth isn't drawn; its mesh is, see-through,
// in Koume's hair's fire (her flame texture, streaming down from his neck, between her red and yellow), solid down to
// a hem of live flame tongues that flicker. Tears from his wounds (EnGanonMant_SetWear) are holes burnt through.
// (The cape's texture coordinates: s runs across it, 0 to 64 texels; t runs down it, -16 at his neck to 68 at the
// hem. The mask is read at half t, so its 64 rows cover -8..34 of that, wrapping: rows 56-63 are -8..-1.)
#define FLAME_TEX_S 64
#define FLAME_TEX_T 64
static u8 sFlameMaskTex[FLAME_TEX_S * FLAME_TEX_T / 2]; // I4
static u32 sFlameMaskFrame = 0xFFFFFFFF;

static void EnGanonMant_BuildFlameMask(u32 frame, s32 ice) {
    static f32 sLastStart = -1.0f;
    static f32 sLastTongue = -1.0f;
    // (where the hem's tongues start, in half texels down the cape, the hem being at 34, and how long they reach)
    f32 hemStart = GdfTune(ice ? "ice.hem_start" : "fire.hem_start", 26.0f);
    f32 hemTongue = GdfTune(ice ? "ice.hem_tongue_length" : "fire.hem_tongue_length", 8.0f);
    s32 s;
    s32 r;

    // (ice doesn't flicker: its hem is a still edge of icicles)
    if (ice) {
        frame = 0;
    }
    if (((frame | (ice << 31)) == sFlameMaskFrame) && (hemStart == sLastStart) && (hemTongue == sLastTongue)) {
        return;
    }
    sFlameMaskFrame = frame | (ice << 31);
    sLastStart = hemStart;
    sLastTongue = hemTongue;
    for (s = 0; s < FLAME_TEX_S; s++) {
        // Where this strip of the cape's flame ends (in half texels down it, the hem is at 34): tongues along the
        // hem, each lengthening and shortening at its own pace
        f32 u = s * (2.0f * M_PI / FLAME_TEX_S);
        f32 tongue = 0.55f * fabsf(sinf(u * 2.0f + frame * 0.09f)) + 0.3f * fabsf(sinf(u * 3.5f - frame * 0.13f + 1.7f)) +
                     0.15f * sinf(u * 9.0f + frame * 0.31f);
        f32 hem = hemStart + hemTongue * tongue;

        for (r = 0; r < FLAME_TEX_T; r++) {
            f32 t = (r < 48) ? r : (r - 64); // (half texels down the cape)
            f32 a;
            s32 v;

            if (t < hem - 3.0f) {
                a = 1.0f;
            } else if (t < hem) {
                a = (hem - t) / 3.0f; // the tongue's tip thins out
            } else {
                a = 0.0f;
            }
            // (a hole wherever the cloth would be torn)
            if (sMaskTex[(((s32)(t * 2.0f)) & (MANT_TEX_HEIGHT - 1)) * MANT_TEX_WIDTH + (s & (MANT_TEX_WIDTH - 1))] !=
                0) {
                a = 0.0f;
            }
            v = CLAMP((s32)(a * 15.0f + 0.5f), 0, 15);
            if (s & 1) {
                sFlameMaskTex[(r * FLAME_TEX_S + s) / 2] = (sFlameMaskTex[(r * FLAME_TEX_S + s) / 2] & 0xF0) | v;
            } else {
                sFlameMaskTex[(r * FLAME_TEX_S + s) / 2] = (sFlameMaskTex[(r * FLAME_TEX_S + s) / 2] & 0x0F) | (v << 4);
            }
        }
    }
}

// The burning enemies' fire (gameplay_keep), each frame turned upside down, once
static u8 sDeathFlameTex[10][32 * 64];
static u8 sDeathFlameReady = false;

static s32 EnGanonMant_LoadDeathFlames(void) {
    static const char* sFrames[10] = {
        gEffEnemyDeathFlame1Tex, gEffEnemyDeathFlame2Tex, gEffEnemyDeathFlame3Tex, gEffEnemyDeathFlame4Tex,
        gEffEnemyDeathFlame5Tex, gEffEnemyDeathFlame6Tex, gEffEnemyDeathFlame7Tex, gEffEnemyDeathFlame8Tex,
        gEffEnemyDeathFlame9Tex, gEffEnemyDeathFlame10Tex,
    };
    s32 i;
    s32 r;

    if (!sDeathFlameReady) {
        for (i = 0; i < 10; i++) {
            u8* src = (u8*)ResourceMgr_LoadTexOrDListByName(sFrames[i]);

            if (src == NULL) {
                return false;
            }
            for (r = 0; r < 64; r++) {
                memcpy(&sDeathFlameTex[i][r * 32], &src[(63 - r) * 32], 32);
            }
        }
        sDeathFlameReady = true;
    }
    return true;
}

// (ice: the Zora Tunic's cape, the same in Kotake's colors: ice blue to white, drifting slowly, its hem a still edge
// of icicles, no flames over it)
static Gfx* sFireGeomOverride = NULL; // (another mesh in the cape's space to draw in the fire or ice: the collar)

// SoH: "Play as Ganondorf", the flame and ice capes, their plumes, collar and chain are translucent and so, like all
// the game's translucent things, wrote nothing to the depth buffer: the footstep dust, drawn after them, showed through
// them as if in front. Now the solid-enough parts of them (past a small alpha threshold) write their depth too, so dust
// behind the cape stays behind it; the faintest wisps at their edges still don't.
#define GDF_RM_XLU_ZUPD(clk)                                                                   \
    (AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | CLR_ON_CVG | FORCE_BL | ZMODE_XLU | \
     GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

static void EnGanonMant_XluDepth(PlayState* play) {
    if (GdfTune("fx.depth_write", 1.0f) == 0.0f) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, GDF_RM_XLU_ZUPD(2));
    gDPSetAlphaCompare(POLY_XLU_DISP++, G_AC_THRESHOLD);
    gDPSetBlendColor(POLY_XLU_DISP++, 0, 0, 0, (u8)GdfTune("fx.depth_alpha", 40.0f));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnGanonMant_DrawFire(PlayState* play, Mtx* mtx, void* vtx, s32 ice) {
    // Two layers: the body of the fire, then a brighter flicker over it. Prim color, scroll speed down the cape
    static const u8 sFireLayers[2][4] = { { 200, 30, 0, 9 }, { 255, 110, 0, 15 } };
    static const u8 sIceLayers[2][4] = { { 70, 130, 220, 2 }, { 195, 225, 235, 3 } };
    static const char* sKeys[2][2][2] = { { { "fire.layer1_color", "fire.layer1_speed" },
                                            { "fire.layer2_color", "fire.layer2_speed" } },
                                          { { "ice.layer1_color", "ice.layer1_speed" },
                                            { "ice.layer2_color", "ice.layer2_speed" } } };
    const u8(*sDefault)[4] = ice ? sIceLayers : sFireLayers;
    u8 sLayers[2][3];
    f32 speed[2];
    f32 flow = GdfTune(ice ? "ice.flow_direction" : "fire.flow_direction", 1.0f); // (1: neck to hem, -1: up)
    Gfx* mant = ResourceMgr_LoadGfxByName(gMantDL);
    Gfx* geom = NULL;
    u32 frames = play->gameplayFrames;
    s32 i;

    // gMantDL past its own material (its vertices and triangles), so the fire's material stays
    for (i = 0; (mant != NULL) && (i < 32);) {
        u8 op = mant[i].words.w0 >> 24;

        if (op == 0x01) { // G_VTX
            geom = &mant[i];
            break;
        }
        if (op == 0xDF) {
            break;
        }
        i += ((op == G_SETTIMG_OTR_HASH) || (op == G_DL_OTR_HASH) || (op == G_VTX_OTR_HASH) || (op == G_MARKER) ||
              (op == G_MTX_OTR) || (op == G_BRANCH_Z_OTR) || (op == G_MOVEMEM_OTR))
                 ? 2
                 : 1;
    }
    if (sFireGeomOverride != NULL) {
        geom = sFireGeomOverride;
    } else if ((geom == NULL) || (vtx == NULL)) {
        return;
    }
    EnGanonMant_BuildFlameMask(frames, ice);
    for (i = 0; i < 2; i++) {
        s32 c;

        for (c = 0; c < 3; c++) {
            sLayers[i][c] = CLAMP((s32)GdfTuneAt(sKeys[ice ? 1 : 0][i][0], c, sDefault[i][c]), 0, 255);
        }
        speed[i] = GdfTune(sKeys[ice ? 1 : 0][i][1], sDefault[i][3]) * flow;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPInvalidateTexCache(POLY_XLU_DISP++, sFlameMaskTex);
    gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (vtx != NULL) {
        gSPSegment(POLY_XLU_DISP++, 0x0C, vtx);
    }
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH | G_LIGHTING);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    // (her flame texture stretched over the whole cape once: half across it, a quarter down it)
    gDPLoadTextureBlock(POLY_XLU_DISP++, gTwinrovaEffectTex, G_IM_FMT_I, G_IM_SIZ_8b, 32, 32, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, 1, 2);
    gDPLoadMultiBlock_4b(POLY_XLU_DISP++, sFlameMaskTex, 0x100, 1, G_IM_FMT_I, FLAME_TEX_S, FLAME_TEX_T, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 6, 6, 0, 1);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    CLOSE_DISPS(play->state.gfxCtx);
    EnGanonMant_XluDepth(play);
    OPEN_DISPS(play->state.gfxCtx);
    if (ice) {
        gDPSetEnvColor(POLY_XLU_DISP++, (u8)GdfTuneAt("ice.glow_color", 0, 240), (u8)GdfTuneAt("ice.glow_color", 1, 250),
                       (u8)GdfTuneAt("ice.glow_color", 2, 255), 255);
    } else {
        gDPSetEnvColor(POLY_XLU_DISP++, (u8)GdfTuneAt("fire.glow_color", 0, 255), (u8)GdfTuneAt("fire.glow_color", 1, 235),
                       (u8)GdfTuneAt("fire.glow_color", 2, 40), 255);
    }
    for (i = 0; i < 2; i++) {
        gDPPipeSync(POLY_XLU_DISP++);
        if (i == 0) {
            // the flame's color between Koume's red and yellow by her texture; solid inside the flame's outline
            gDPSetCombineLERP(POLY_XLU_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL1, 0, PRIMITIVE, 0,
                              0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sLayers[i][0], sLayers[i][1], sLayers[i][2], 255);
        } else {
            // the flicker: only where her texture is bright
            gDPSetCombineLERP(POLY_XLU_DISP++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, TEXEL1, 0, 0,
                              0, 0, COMBINED, 0, 0, 0, COMBINED);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sLayers[i][0], sLayers[i][1], sLayers[i][2], 180);
        }
        // (pouring from his neck down the cape: the tile's offset grows, which moves the picture toward the hem)
        gSPDisplayList(POLY_XLU_DISP++, Gfx_TwoTexScroll(play->state.gfxCtx, 0, (i * 45) & 0x7F,
                                                         ((s32)(frames * speed[i])) & 0x7F, 32, 32, 1, 0, 0,
                                                         FLAME_TEX_S, FLAME_TEX_T));
        gSPDisplayList(POLY_XLU_DISP++, geom);
    }

    // Live flames over that: the burning enemies' fire (its ten frames, played as a flipbook), upside down so the
    // tongues lick down the cape with the rest, four across it and two and a half down. Two sets, out of step and
    // offset half a flame, so they don't all flicker together. Their color runs from red at the edges to yellow at
    // the core; the hem's outline cuts them off too.
    if (!ice && (GdfTune("fire.live_flames", 1.0f) != 0.0f) && EnGanonMant_LoadDeathFlames()) {
        for (i = 0; i < 2; i++) {
            u32 f = (frames + i * 5) % 10;

            gDPPipeSync(POLY_XLU_DISP++);
            gDPLoadTextureBlock(POLY_XLU_DISP++, sDeathFlameTex[f], G_IM_FMT_I, G_IM_SIZ_8b, 32, 64, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 6, 15, 15);
            gDPSetCombineLERP(POLY_XLU_DISP++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, TEXEL1, 0, 0, 0,
                              0, COMBINED, 0, 0, 0, COMBINED);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)GdfTuneAt("fire.live_flames_edge_color", 0, 230),
                            (u8)GdfTuneAt("fire.live_flames_edge_color", 1, 40),
                            (u8)GdfTuneAt("fire.live_flames_edge_color", 2, 0), 255);
            gDPSetEnvColor(POLY_XLU_DISP++, (u8)GdfTuneAt("fire.live_flames_core_color", 0, 255),
                           (u8)GdfTuneAt("fire.live_flames_core_color", 1, 230),
                           (u8)GdfTuneAt("fire.live_flames_core_color", 2, 90), 255);
            gSPDisplayList(POLY_XLU_DISP++, Gfx_TwoTexScroll(play->state.gfxCtx, 0, (i * 64) & 0x7F,
                                                             ((s32)(frames * GdfTune("fire.live_flames_speed", 6.0f) * flow) + i * 128) & 0xFF,
                                                             32, 64, 1, 0, 0,
                                                             FLAME_TEX_S, FLAME_TEX_T));
            gSPDisplayList(POLY_XLU_DISP++, geom);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// SoH: "Play as Ganondorf", the flame cape's live flames in 3D: Volvagia's mane. As her mane runs in chains of flame
// plumes down her back, a chain of them hangs down each of the cape's strands, joint to joint, so they sway with
// it: wide at his shoulders, longer toward the hem, the last ones trailing off it. Each pulses in size on its own beat
// and each strand's color flickers between orange and yellow, as hers do.
// (ice: the same plumes as frost, pale blue to white, still but for a slow breath)
static void EnGanonMant_DrawMane(PlayState* play, EnGanonMant* this, s32 ice) {
    u32 frames = play->gameplayFrames;
    s32 i;
    s32 j;

    if (!CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.FlameCapeMane"), 1)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gVolvagiaManeMaterialDL);
    CLOSE_DISPS(play->state.gfxCtx);
    EnGanonMant_XluDepth(play);
    OPEN_DISPS(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH);
    if (ice) {
        gDPSetEnvColor(POLY_XLU_DISP++, (u8)GdfTuneAt("frost.glow_color", 0, 60), (u8)GdfTuneAt("frost.glow_color", 1, 140),
                       (u8)GdfTuneAt("frost.glow_color", 2, 255), 0);
    }
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        MantStrand* strand = &this->strands[i];
        MantStrand* left = &this->strands[MAX(i - 1, 0)];
        MantStrand* right = &this->strands[MIN(i + 1, GANON_MANT_NUM_STRANDS - 1)];
        s32 glow = ((frames + i * 3) & 8) ? 128 : 255;

        gDPPipeSync(POLY_XLU_DISP++);
        if (ice) {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)GdfTuneAt("frost.color", 0, 200), (u8)GdfTuneAt("frost.color", 1, 235),
                            (u8)GdfTuneAt("frost.color", 2, 255), 255);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255,
                            (glow + 128 + (s32)(Math_SinS(frames * 3000 + i * 9000) * 60.0f)) / 2, 0, 255);
        }
        for (j = 1; j < GANON_MANT_NUM_JOINTS; j++) {
            Vec3f* p = &strand->joints[j - 1];
            Vec3f* q = &strand->joints[j];
            Vec3f down;
            Vec3f across;
            Vec3f out;
            f32 len;
            f32 wid;
            f32 pulse;
            f32 d;
            MtxF mf;

            // along the strand (the plume's length, its tip toward the hem)
            down.x = q->x - p->x;
            down.y = q->y - p->y;
            down.z = q->z - p->z;
            len = sqrtf(SQ(down.x) + SQ(down.y) + SQ(down.z));
            if (len < 0.01f) {
                continue;
            }
            down.x /= len;
            down.y /= len;
            down.z /= len;
            // across the cape, to its neighbors
            across.x = right->joints[j].x - left->joints[j].x;
            across.y = right->joints[j].y - left->joints[j].y;
            across.z = right->joints[j].z - left->joints[j].z;
            d = across.x * down.x + across.y * down.y + across.z * down.z;
            across.x -= d * down.x;
            across.y -= d * down.y;
            across.z -= d * down.z;
            wid = sqrtf(SQ(across.x) + SQ(across.y) + SQ(across.z));
            if (wid < 0.01f) {
                continue;
            }
            across.x /= wid;
            across.y /= wid;
            across.z /= wid;
            wid /= (i == 0 || i == GANON_MANT_NUM_STRANDS - 1) ? 1.0f : 2.0f; // (one strand's width)
            // out of the cape's face
            out.x = across.y * down.z - across.z * down.y;
            out.y = across.z * down.x - across.x * down.z;
            out.z = across.x * down.y - across.y * down.x;

            // Her plume is 3900 wide and 6750 long; each overlaps the next joint's, the lower ones longer
            pulse = 1.0f + GdfTune(ice ? "frost.pulse" : "mane.pulse", ice ? 0.06f : 0.3f) *
                               Math_SinS(frames * (ice ? 900 : 5596) + i * 7000 + j * 11000);
            {
                const char* pre = ice ? "frost" : "mane";
                char k[40];
                f32 sx;
                f32 sy;
                f32 sz;

                snprintf(k, sizeof(k), "%s.width", pre);
                sx = (wid * GdfTune(k, 1.6f) / 3900.0f) * pulse;
                snprintf(k, sizeof(k), "%s.length", pre);
                sy = len * GdfTune(k, 1.5f);
                snprintf(k, sizeof(k), "%s.length_grow", pre);
                sy = ((sy + len * j * GdfTune(k, 0.06f)) / 6750.0f) * pulse;
                snprintf(k, sizeof(k), "%s.depth", pre);
                sz = sx * GdfTune(k, 0.6f);

                mf.xx = across.x * sx; mf.yx = across.y * sx; mf.zx = across.z * sx; mf.wx = 0.0f;
                mf.xy = down.x * sy;   mf.yy = down.y * sy;   mf.zy = down.z * sy;   mf.wy = 0.0f;
                mf.xz = out.x * sz;    mf.yz = out.y * sz;    mf.zz = out.z * sz;    mf.wz = 0.0f;
                mf.xw = p->x;          mf.yw = p->y;          mf.zw = p->z;          mf.ww = 1.0f;
            }
            Matrix_Put(&mf);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gVolvagiaManeModelDL);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// SoH: "Play as Ganondorf", the Zora Tunic's mage robe: the cape's own cloth in a dark blue (its weave's light and
// shade kept), for the robe's skirt and the hood
static u8 sRobeMantTex[MANT_TEX_WIDTH * MANT_TEX_HEIGHT * 2];

static s32 EnGanonMant_IsRobe(EnGanonMant* this) {
    return this->ownVertices && (this->actor.params >= PLAYER_GANONDORF_CAPELET_PARAMS) &&
           (this->actor.params <= PLAYER_GANONDORF_CAPELET_PARAMS + 2) && BossGanon_Player_ZoraLook();
}

static s32 EnGanonMant_BuildRobeTex(void) {
    u8* src = (u8*)ResourceGetDataByName(gMantTex);
    f32 cr = GdfTuneAt("robe.cloth_color", 0, 30.0f);
    f32 cg = GdfTuneAt("robe.cloth_color", 1, 45.0f);
    f32 cb = GdfTuneAt("robe.cloth_color", 2, 110.0f);
    s32 i;

    if (src == NULL) {
        return false;
    }
    for (i = 0; i < MANT_TEX_WIDTH * MANT_TEX_HEIGHT; i++) {
        u16 c = (src[2 * i] << 8) | src[2 * i + 1];
        f32 lum = (((c >> 11) & 0x1F) * 0.30f + ((c >> 6) & 0x1F) * 0.59f + ((c >> 1) & 0x1F) * 0.11f) / 31.0f;
        f32 k = 0.45f + 1.4f * lum; // (the red cloth is dark: lifted so its weave shows in the blue)
        s32 r = CLAMP((s32)(cr * k) >> 3, 0, 31);
        s32 g = CLAMP((s32)(cg * k) >> 3, 0, 31);
        s32 b = CLAMP((s32)(cb * k) >> 3, 0, 31);
        u16 o = (r << 11) | (g << 6) | (b << 1) | (c & 1);

        sRobeMantTex[2 * i] = o >> 8;
        sRobeMantTex[2 * i + 1] = o & 0xFF;
    }
    return true;
}

// SoH: "Play as Ganondorf", the ice cape's collar: a standing collar of ice where the cape begins, rising from its top
// edge and bending up and out behind his neck, its top a jagged edge of icicles (the hem's own), with frost plumes
// standing up along it. Three rows of the cape's twelve strands: its roots, then higher and further out.
static f32 sCollarScale = 1.0f; // (the mantle item models' collar: smaller, at their size)

static void EnGanonMant_DrawCollar(PlayState* play, EnGanonMant* this, Mtx* mtx, s32 ice) {
    Vtx* v = Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * 3 * GANON_MANT_NUM_STRANDS);
    Gfx* dl = Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 32);
    Gfx* g = dl;
    f32 height = GdfTune("collar.height", 14.0f) * sCollarScale;
    f32 flare = GdfTune("collar.flare", 7.0f) * sCollarScale;
    f32 lean = GdfTune("collar.lean_back", 3.0f) * sCollarScale;
    Vec3f mid = { 0.0f, 0.0f, 0.0f };
    Vec3f back;
    Vec3f top[GANON_MANT_NUM_STRANDS];
    f32 len;
    s32 i;
    s32 r;

    if (GdfTune("collar.on", 1.0f) == 0.0f) {
        return;
    }
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        mid.x += this->strands[i].joints[0].x / GANON_MANT_NUM_STRANDS;
        mid.z += this->strands[i].joints[0].z / GANON_MANT_NUM_STRANDS;
    }
    // (behind him: the way the cape falls from its top)
    back.x = this->strands[GANON_MANT_NUM_STRANDS / 2].joints[2].x - this->strands[GANON_MANT_NUM_STRANDS / 2].joints[0].x;
    back.z = this->strands[GANON_MANT_NUM_STRANDS / 2].joints[2].z - this->strands[GANON_MANT_NUM_STRANDS / 2].joints[0].z;
    len = sqrtf(SQ(back.x) + SQ(back.z));
    if (len > 0.01f) {
        back.x /= len;
        back.z /= len;
    } else {
        back.x = back.z = 0.0f;
    }
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        Vec3f* root = &this->strands[i].joints[0];
        f32 ox = root->x - mid.x;
        f32 oz = root->z - mid.z;
        f32 ol = sqrtf(SQ(ox) + SQ(oz));

        if (ol > 0.01f) {
            ox /= ol;
            oz /= ol;
        }
        for (r = 0; r < 3; r++) {
            f32 t = r / 2.0f;
            // (bending: straight up at first, then out, more toward the top)
            Vec3f p = { root->x + (ox * flare + back.x * lean) * t * t, root->y + height * t,
                        root->z + (oz * flare + back.z * lean) * t * t };
            Vtx* vt = &v[r * GANON_MANT_NUM_STRANDS + i];

            vt->v.ob[0] = CLAMP((p.x - this->anchor.x) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vt->v.ob[1] = CLAMP((p.y - this->anchor.y) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vt->v.ob[2] = CLAMP((p.z - this->anchor.z) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vt->v.flag = 0;
            // (s across like the cape; t from solid at its root to the icicle edge at its top)
            vt->v.tc[0] = (s16)(i * 2048 / (GANON_MANT_NUM_STRANDS - 1));
            vt->v.tc[1] = (s16)(r * 34 * 32 + 0);
            vt->v.cn[0] = vt->v.cn[1] = vt->v.cn[2] = vt->v.cn[3] = 255;
            if (r == 2) {
                top[i] = p;
            }
        }
    }
    gSPVertex(g++, v, 3 * GANON_MANT_NUM_STRANDS, 0);
    for (r = 0; r < 2; r++) {
        for (i = 0; i < GANON_MANT_NUM_STRANDS - 1; i++) {
            s32 a = r * GANON_MANT_NUM_STRANDS + i;
            s32 b = a + GANON_MANT_NUM_STRANDS;

            gSP2Triangles(g++, a, a + 1, b + 1, 0, a, b + 1, b, 0);
        }
    }
    gSPEndDisplayList(g++);

    sFireGeomOverride = dl;
    EnGanonMant_DrawFire(play, mtx, NULL, ice);
    sFireGeomOverride = NULL;

    // Frost plumes standing up along it, from its top edge
    if (CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.FlameCapeMane"), 1) &&
        (GdfTune("collar.plumes", 1.0f) != 0.0f)) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, gVolvagiaManeMaterialDL);
        CLOSE_DISPS(play->state.gfxCtx);
        EnGanonMant_XluDepth(play);
        OPEN_DISPS(play->state.gfxCtx);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH);
        if (ice) {
            gDPSetEnvColor(POLY_XLU_DISP++, (u8)GdfTuneAt("frost.glow_color", 0, 60),
                           (u8)GdfTuneAt("frost.glow_color", 1, 140), (u8)GdfTuneAt("frost.glow_color", 2, 255), 0);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)GdfTuneAt("frost.color", 0, 200),
                            (u8)GdfTuneAt("frost.color", 1, 235), (u8)GdfTuneAt("frost.color", 2, 255), 255);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 160, 0, 255);
        }
        for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
            Vtx* base = &v[GANON_MANT_NUM_STRANDS + i];
            Vec3f* p = &top[i];
            Vec3f* q = &top[MIN(i + 1, GANON_MANT_NUM_STRANDS - 1)];
            Vec3f* o = &top[MAX(i - 1, 0)];
            Vec3f up = { p->x - (base->v.ob[0] / LINK_GANON_CAPE_VTX_SCALE + this->anchor.x),
                         p->y - (base->v.ob[1] / LINK_GANON_CAPE_VTX_SCALE + this->anchor.y),
                         p->z - (base->v.ob[2] / LINK_GANON_CAPE_VTX_SCALE + this->anchor.z) };
            Vec3f across = { q->x - o->x, q->y - o->y, q->z - o->z };
            Vec3f out;
            f32 ul = sqrtf(SQ(up.x) + SQ(up.y) + SQ(up.z));
            f32 al = sqrtf(SQ(across.x) + SQ(across.y) + SQ(across.z));
            f32 pulse = 1.0f + 0.06f * Math_SinS(play->gameplayFrames * 900 + i * 7000);
            f32 sx;
            f32 sy;
            MtxF mf;

            if ((ul < 0.01f) || (al < 0.01f)) {
                continue;
            }
            up.x /= ul;
            up.y /= ul;
            up.z /= ul;
            across.x /= al;
            across.y /= al;
            across.z /= al;
            out.x = across.y * up.z - across.z * up.y;
            out.y = across.z * up.x - across.x * up.z;
            out.z = across.x * up.y - across.y * up.x;
            sx = (al / ((i == 0 || i == GANON_MANT_NUM_STRANDS - 1) ? 1.0f : 2.0f)) * GdfTune("collar.plume_width", 1.4f) /
                 3900.0f * pulse;
            sy = height * GdfTune("collar.plume_length", 0.9f) / 6750.0f * pulse;
            // (from the collar's middle row, tip up past its edge)
            mf.xx = across.x * sx; mf.yx = across.y * sx; mf.zx = across.z * sx; mf.wx = 0.0f;
            mf.xy = up.x * sy;     mf.yy = up.y * sy;     mf.zy = up.z * sy;     mf.wy = 0.0f;
            mf.xz = out.x * sx * 0.6f; mf.yz = out.y * sx * 0.6f; mf.zz = out.z * sx * 0.6f; mf.wz = 0.0f;
            mf.xw = base->v.ob[0] / LINK_GANON_CAPE_VTX_SCALE + this->anchor.x;
            mf.yw = base->v.ob[1] / LINK_GANON_CAPE_VTX_SCALE + this->anchor.y;
            mf.zw = base->v.ob[2] / LINK_GANON_CAPE_VTX_SCALE + this->anchor.z;
            mf.ww = 1.0f;
            Matrix_Put(&mf);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gVolvagiaManeModelDL);
        }
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

// SoH: "Play as Ganondorf", the flame and ice capes set into his knight's armour: a mount of the armour's own metal
// clipped to the back of it, filling the space between his back and the cape's top edge, from one front corner round
// the back of his neck to the other. Per strand of the cape, its cross-section is a wedge: its top running level from
// his back plate out to the cape's top (a little above it), its outer face down over the cape's top edge (in the
// armour's trim colour, as its plates' edges are), and its underside slanting from under the cape's edge down and in
// to his back, lower there, so it reads as a solid piece. Its two ends closed. Keys "plate.*": up, down (at the cape),
// drop (at his back), out (past the cape), back_depth and back_width (his back's shape), on.
#define GDF_PLATE_ROWS 4 // in top, out top, out bottom, in bottom

static void EnGanonMant_PlateNormal(Vec3f* a, Vec3f* b, Vec3f* t, Vec3f* ref, Vtx* va, Vtx* vb) {
    Vec3f e = { b->x - a->x, b->y - a->y, b->z - a->z };
    Vec3f n = { t->y * e.z - t->z * e.y, t->z * e.x - t->x * e.z, t->x * e.y - t->y * e.x };
    f32 l = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));

    if (l < 0.001f) {
        n = *ref;
        l = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));
        if (l < 0.001f) {
            return;
        }
    }
    if ((n.x * ref->x + n.y * ref->y + n.z * ref->z) < 0.0f) {
        l = -l;
    }
    va->n.n[0] = vb->n.n[0] = (s8)(n.x / l * 120.0f);
    va->n.n[1] = vb->n.n[1] = (s8)(n.y / l * 120.0f);
    va->n.n[2] = vb->n.n[2] = (s8)(n.z / l * 120.0f);
}

static void EnGanonMant_PlateVtx(EnGanonMant* this, Vec3f* p, Vtx* vt) {
    vt->n.ob[0] = CLAMP((p->x - this->anchor.x) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
    vt->n.ob[1] = CLAMP((p->y - this->anchor.y) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
    vt->n.ob[2] = CLAMP((p->z - this->anchor.z) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
    vt->n.flag = 0;
    vt->n.tc[0] = vt->n.tc[1] = 0;
    vt->n.a = 255;
}

static void EnGanonMant_DrawPlate(PlayState* play, EnGanonMant* this, Mtx* mtx) {
    f32 up = GdfTune("plate.up", 3.0f);
    f32 down = GdfTune("plate.down", 4.0f);
    f32 drop = GdfTune("plate.drop", 14.0f);
    f32 out = GdfTune("plate.out", 1.5f);
    // (the three faces, each its own pair of rows for its own normal, and the two end caps)
    Vtx* v = Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * (6 * GANON_MANT_NUM_STRANDS + 8));
    Gfx* dl = Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 80);
    Gfx* dlTrim = Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 32);
    Gfx* g;
    Vec3f prof[GANON_MANT_NUM_STRANDS][GDF_PLATE_ROWS];
    static const u8 sFaceRows[3][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 } }; // the top, the outer face, the underside
    s32 i;
    s32 f;

    if ((GdfTune("plate.on", 1.0f) == 0.0f) || !BossGanon_Player_KnightLook()) {
        return;
    }
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        Vec3f* root = &this->strands[i].joints[0];
        Vec3f back;
        f32 ox;
        f32 oz;
        f32 ol;

        if (!BossGanon_Player_ArmorBackPoint(root, &back)) {
            return;
        }
        ox = root->x - back.x;
        oz = root->z - back.z;
        ol = sqrtf(SQ(ox) + SQ(oz));
        if (ol > 0.01f) {
            ox /= ol;
            oz /= ol;
        }
        prof[i][0] = back;
        prof[i][0].y += up;
        prof[i][1].x = root->x + ox * out;
        prof[i][1].y = root->y + up;
        prof[i][1].z = root->z + oz * out;
        prof[i][2] = prof[i][1];
        prof[i][2].y = root->y - down;
        prof[i][3] = back;
        prof[i][3].y -= drop;
    }
    // (its own turn, stretch and shift: "plate.rot", "plate.scale_xyz", "plate.offset", along his torso)
    BossGanon_Player_PieceAdjustPoints("plate", &prof[0][0], GANON_MANT_NUM_STRANDS * GDF_PLATE_ROWS);
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        Vec3f* pa = &prof[MAX(i - 1, 0)][1];
        Vec3f* pb = &prof[MIN(i + 1, GANON_MANT_NUM_STRANDS - 1)][1];
        Vec3f t = { pb->x - pa->x, pb->y - pa->y, pb->z - pa->z };
        Vec3f outward = { prof[i][1].x - prof[i][0].x, 0.0f, prof[i][1].z - prof[i][0].z };
        Vec3f refs[3];

        refs[0].x = 0.0f;
        refs[0].y = 1.0f;
        refs[0].z = 0.0f;
        refs[1] = outward;
        refs[2].x = outward.x * 0.3f;
        refs[2].y = -1.0f;
        refs[2].z = outward.z * 0.3f;
        for (f = 0; f < 3; f++) {
            Vtx* va = &v[(f * 2 + 0) * GANON_MANT_NUM_STRANDS + i];
            Vtx* vb = &v[(f * 2 + 1) * GANON_MANT_NUM_STRANDS + i];

            EnGanonMant_PlateVtx(this, &prof[i][sFaceRows[f][0]], va);
            EnGanonMant_PlateVtx(this, &prof[i][sFaceRows[f][1]], vb);
            EnGanonMant_PlateNormal(&prof[i][sFaceRows[f][0]], &prof[i][sFaceRows[f][1]], &t, &refs[f], va, vb);
        }
    }
    // (the end caps: the whole cross-section at each front corner, facing along the plate)
    for (f = 0; f < 2; f++) {
        s32 end = (f == 0) ? 0 : (GANON_MANT_NUM_STRANDS - 1);
        s32 next = (f == 0) ? 1 : (GANON_MANT_NUM_STRANDS - 2);
        Vec3f n = { prof[end][1].x - prof[next][1].x, prof[end][1].y - prof[next][1].y,
                    prof[end][1].z - prof[next][1].z };
        f32 l = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));
        s32 r;

        for (r = 0; r < GDF_PLATE_ROWS; r++) {
            Vtx* vt = &v[6 * GANON_MANT_NUM_STRANDS + f * 4 + r];

            EnGanonMant_PlateVtx(this, &prof[end][r], vt);
            if (l > 0.001f) {
                vt->n.n[0] = (s8)(n.x / l * 120.0f);
                vt->n.n[1] = (s8)(n.y / l * 120.0f);
                vt->n.n[2] = (s8)(n.z / l * 120.0f);
            }
        }
    }

    // In the metal: the top, the underside and the caps
    g = dl;
    for (f = 0; f < 3; f += 2) {
        gSPVertex(g++, &v[f * 2 * GANON_MANT_NUM_STRANDS], 2 * GANON_MANT_NUM_STRANDS, 0);
        for (i = 0; i < GANON_MANT_NUM_STRANDS - 1; i++) {
            s32 a = i;
            s32 b = i + GANON_MANT_NUM_STRANDS;

            gSP2Triangles(g++, a, a + 1, b + 1, 0, a, b + 1, b, 0);
        }
    }
    gSPVertex(g++, &v[6 * GANON_MANT_NUM_STRANDS], 8, 0);
    gSP2Triangles(g++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSP2Triangles(g++, 4, 5, 6, 0, 4, 6, 7, 0);
    gSPEndDisplayList(g++);
    // In the trim: the outer face
    g = dlTrim;
    gSPVertex(g++, &v[2 * GANON_MANT_NUM_STRANDS], 2 * GANON_MANT_NUM_STRANDS, 0);
    for (i = 0; i < GANON_MANT_NUM_STRANDS - 1; i++) {
        s32 a = i;
        s32 b = i + GANON_MANT_NUM_STRANDS;

        gSP2Triangles(g++, a, a + 1, b + 1, 0, a, b + 1, b, 0);
    }
    gSPEndDisplayList(g++);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    g = POLY_OPA_DISP;
    BossGanon_Player_PlateMaterialExt(&g, false);
    POLY_OPA_DISP = g;
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, dl);
    g = POLY_OPA_DISP;
    BossGanon_Player_PlateMaterialExt(&g, true);
    POLY_OPA_DISP = g;
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    gSPDisplayList(POLY_OPA_DISP++, dlTrim);
    CLOSE_DISPS(play->state.gfxCtx);
}

// SoH: "Play as Ganondorf", the clasp as a chain: the castle drawbridge's chain links, made small, one after another
// down the V (each lying flat on him, the next on its edge, as a chain's links hang), red hot and flickering on the
// flame cape, frosted on the ice. Keys "clasp.chain_links", "clasp.chain_width", "clasp.style" (0: the plumes).
static Vtx sChainLinkVtx[4] = {
    VTX(-50, 0, 0, 0, 1024, 255, 255, 255, 255),
    VTX(50, 0, 0, 512, 1024, 255, 255, 255, 255),
    VTX(50, 100, 0, 512, 0, 255, 255, 255, 255),
    VTX(-50, 100, 0, 0, 0, 255, 255, 255, 255),
};
static Gfx sChainLinkDL[] = {
    gsSPVertex(sChainLinkVtx, 4, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSPEndDisplayList(),
};

static void EnGanonMant_ClaspPoint(f32 t, Vec3f* a, Vec3f* b, Vec3f* neck, f32 sag, Vec3f* p) {
    f32 shape = GdfTune("clasp.shape", 2.0f);

    if (shape == 2.0f) {
        // (the U: each half leaves its corner heading down, its sides bowing outward a little, then curves in along
        // the bottom into the V's point: "clasp.u_down" how far down it goes before it turns in (0-1, of the way down
        // to the point), "clasp.u_pull" how far in (+) or out (-) its sides lean, "clasp.u_forward" how far forward,
        // toward the point, and "clasp.u_drop" how much lower still)
        f32 down = GdfTune("clasp.u_down", 0.55f);
        f32 pull = GdfTune("clasp.u_pull", -0.1f);
        f32 fwd = GdfTune("clasp.u_forward", 0.9f);
        f32 drop = GdfTune("clasp.u_drop", 0.0f);
        Vec3f mid = { (a->x + b->x) * 0.5f, (a->y + b->y) * 0.5f, (a->z + b->z) * 0.5f };
        Vec3f* corner = (t < 0.5f) ? a : b;
        f32 h = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f); // (0 at the corner, 1 at the point)
        f32 u = 1.0f - h;
        Vec3f c;

        c.x = corner->x + (mid.x - corner->x) * pull + (neck->x - mid.x) * fwd;
        c.y = corner->y + (neck->y - corner->y) * down - drop;
        c.z = corner->z + (mid.z - corner->z) * pull + (neck->z - mid.z) * fwd;
        p->x = u * u * corner->x + 2.0f * u * h * c.x + h * h * neck->x;
        p->y = u * u * corner->y + 2.0f * u * h * c.y + h * h * neck->y;
        p->z = u * u * corner->z + 2.0f * u * h * c.z + h * h * neck->z;
    } else if (shape == 1.0f) {
        // (the V: straight from each corner down to the point on the breastplate's emblem)
        f32 h = (t < 0.5f) ? (t * 2.0f) : ((t - 0.5f) * 2.0f);
        Vec3f* from = (t < 0.5f) ? a : neck;
        Vec3f* to = (t < 0.5f) ? neck : b;

        p->x = from->x + (to->x - from->x) * h;
        p->y = from->y + (to->y - from->y) * h;
        p->z = from->z + (to->z - from->z) * h;
    } else {
        f32 u = 1.0f - t;
        Vec3f c = { 2.0f * neck->x - 0.5f * (a->x + b->x), 2.0f * neck->y - 0.5f * (a->y + b->y) - sag,
                    2.0f * neck->z - 0.5f * (a->z + b->z) };

        p->x = u * u * a->x + 2 * u * t * c.x + t * t * b->x;
        p->y = u * u * a->y + 2 * u * t * c.y + t * t * b->y;
        p->z = u * u * a->z + 2 * u * t * c.z + t * t * b->z;
    }
}

// The chain's physics: its two ends (the cape's corners, moving with the cape) and its clasp (the middle, on the
// emblem) are fixed; the links between swing, lag behind as he moves and settle again, each pulled gently back toward
// its place in the chain's shape so it never sags through the pauldrons, and kept their own length apart. Keys
// "clasp.physics" (0 off), "clasp.stiffness" (the pull back to its shape), "clasp.damping", "clasp.gravity".
static Vec3f sChainPos[64];
static Vec3f sChainPrev[64];
static s32 sChainCount = 0;
static u32 sChainFrame = 0xFFFFFFFF;

static void EnGanonMant_ChainPhysics(PlayState* play, Vec3f* pts, s32 links) {
    f32 stiff = GdfTune("clasp.stiffness", 0.15f);
    f32 damp = GdfTune("clasp.damping", 0.75f);
    f32 grav = GdfTune("clasp.gravity", 0.4f);
    s32 k;
    s32 it;

    if (GdfTune("clasp.physics", 1.0f) == 0.0f) {
        sChainCount = 0;
        return;
    }
    // (a new chain, or one far from where it should be (a teleport, a cut): put it where it should be)
    if (sChainCount != links + 1) {
        sChainCount = 0;
    }
    for (k = 0; (sChainCount != 0) && (k <= links); k++) {
        if (Math_Vec3f_DistXYZ(&sChainPos[k], &pts[k]) > 60.0f) {
            sChainCount = 0;
        }
    }
    if (sChainCount == 0) {
        for (k = 0; k <= links; k++) {
            sChainPos[k] = sChainPrev[k] = pts[k];
        }
        sChainCount = links + 1;
        sChainFrame = play->gameplayFrames;
        return;
    }
    if (sChainFrame != play->gameplayFrames) {
        sChainFrame = play->gameplayFrames;
        for (k = 0; k <= links; k++) {
            Vec3f v;

            if ((k == 0) || (k == links) || (k == links / 2)) {
                sChainPrev[k] = sChainPos[k];
                sChainPos[k] = pts[k];
                continue;
            }
            v.x = (sChainPos[k].x - sChainPrev[k].x) * damp;
            v.y = (sChainPos[k].y - sChainPrev[k].y) * damp;
            v.z = (sChainPos[k].z - sChainPrev[k].z) * damp;
            sChainPrev[k] = sChainPos[k];
            sChainPos[k].x += v.x + (pts[k].x - sChainPos[k].x) * stiff;
            sChainPos[k].y += v.y - grav + (pts[k].y - sChainPos[k].y) * stiff;
            sChainPos[k].z += v.z + (pts[k].z - sChainPos[k].z) * stiff;
        }
        // (each link kept its own length from the next, the fixed points left where they are)
        for (it = 0; it < 3; it++) {
            for (k = 0; k < links; k++) {
                Vec3f* p = &sChainPos[k];
                Vec3f* q = &sChainPos[k + 1];
                s32 fixP = (k == 0) || (k == links / 2);
                s32 fixQ = (k + 1 == links) || (k + 1 == links / 2);
                f32 rest = Math_Vec3f_DistXYZ(&pts[k], &pts[k + 1]);
                Vec3f d = { q->x - p->x, q->y - p->y, q->z - p->z };
                f32 len = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
                f32 corr;
                f32 wp;
                f32 wq;

                if ((len < 0.001f) || (fixP && fixQ)) {
                    continue;
                }
                corr = (len - rest) / len;
                wp = fixP ? 0.0f : (fixQ ? 1.0f : 0.5f);
                wq = fixQ ? 0.0f : (fixP ? 1.0f : 0.5f);
                p->x += d.x * corr * wp;
                p->y += d.y * corr * wp;
                p->z += d.z * corr * wp;
                q->x -= d.x * corr * wq;
                q->y -= d.y * corr * wq;
                q->z -= d.z * corr * wq;
            }
        }
    }
    for (k = 0; k <= links; k++) {
        pts[k] = sChainPos[k];
    }
}

static void EnGanonMant_DrawChain(PlayState* play, Vec3f* a, Vec3f* b, Vec3f* neck, f32 sag, s32 ice) {
    s32 links = MAX((s32)GdfTune("clasp.chain_links", 22.0f), 2);
    Vec3f pts[64];
    f32 width = GdfTune("clasp.chain_width", 3.0f);
    u32 frames = play->gameplayFrames;
    Vec3f p0;
    s32 k;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_OPA_DISP++, gHyruleFieldCastleDrawbridgeChainsTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 32, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 5, G_TX_NOLOD, G_TX_NOLOD);
    // (the iron's own light and shade, tinted, plus a glow: red hot, or frost)
    gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL0, 0, PRIMITIVE, ENVIRONMENT, 0, 0, 0, TEXEL0, COMBINED, 0, SHADE, 0, 0, 0,
                      0, COMBINED);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2);
    if (ice) {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, (u8)GdfTuneAt("clasp.ice_color", 0, 200),
                        (u8)GdfTuneAt("clasp.ice_color", 1, 235), (u8)GdfTuneAt("clasp.ice_color", 2, 255), 255);
    } else {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, (u8)GdfTuneAt("clasp.fire_color", 0, 200),
                        (u8)GdfTuneAt("clasp.fire_color", 1, 200), (u8)GdfTuneAt("clasp.fire_color", 2, 200), 255);
    }

    // (the chain's points first, so the whole chain can be turned, stretched and shifted: "clasp.rot",
    // "clasp.scale_xyz", "clasp.offset", along his torso)
    links = MIN(links, 62) & ~1; // (even: its middle point is the clasp)
    for (k = 0; k <= links; k++) {
        EnGanonMant_ClaspPoint((f32)k / links, a, b, neck, sag, &pts[k]);
    }
    BossGanon_Player_PieceAdjustPoints("clasp", pts, links + 1);
    EnGanonMant_ChainPhysics(play, pts, links);
    p0 = pts[0];
    for (k = 0; k < links; k++) {
        Vec3f p1;
        Vec3f d;
        Vec3f side;
        Vec3f out;
        Vec3f* across;
        Vec3f* face;
        f32 len;
        f32 sl;
        f32 L;
        MtxF mf;

        p1 = pts[k + 1];
        d.x = p1.x - p0.x;
        d.y = p1.y - p0.y;
        d.z = p1.z - p0.z;
        len = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
        if (len < 0.01f) {
            p0 = p1;
            continue;
        }
        d.x /= len;
        d.y /= len;
        d.z /= len;
        side.x = d.z;
        side.y = 0.0f;
        side.z = -d.x;
        sl = sqrtf(SQ(side.x) + SQ(side.z));
        if (sl < 0.01f) {
            side.x = 1.0f;
            side.z = 0.0f;
            sl = 1.0f;
        }
        side.x /= sl;
        side.z /= sl;
        out.x = side.y * d.z - side.z * d.y;
        out.y = side.z * d.x - side.x * d.z;
        out.z = side.x * d.y - side.y * d.x;
        // (flat on him, then on its edge, by turns)
        across = (k & 1) ? &out : &side;
        face = (k & 1) ? &side : &out;
        // (each link a little longer than its step, so it reaches into the next)
        L = len * 1.45f;

        if (ice) {
            gDPSetEnvColor(POLY_OPA_DISP++, 30, 60, 100, 255);
        } else {
            s32 glow = 70 + (s32)(40.0f * Math_SinS(frames * 4000 + k * 9000));

            gDPSetEnvColor(POLY_OPA_DISP++, glow, glow / 4, 0, 255);
        }
        mf.xx = across->x * width / 100.0f; mf.yx = across->y * width / 100.0f; mf.zx = across->z * width / 100.0f;
        mf.wx = 0.0f;
        mf.xy = d.x * L / 100.0f;           mf.yy = d.y * L / 100.0f;           mf.zy = d.z * L / 100.0f;
        mf.wy = 0.0f;
        mf.xz = face->x;                    mf.yz = face->y;                    mf.zz = face->z;
        mf.wz = 0.0f;
        mf.xw = (p0.x + p1.x) * 0.5f - d.x * L * 0.5f;
        mf.yw = (p0.y + p1.y) * 0.5f - d.y * L * 0.5f;
        mf.zw = (p0.z + p1.z) * 0.5f - d.z * L * 0.5f;
        mf.ww = 1.0f;
        Matrix_Put(&mf);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sChainLinkDL);
        p0 = p1;
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// SoH: "Play as Ganondorf", the cape fastened round his neck: from the cape's two top corners a chain of plumes (fire
// or frost, like the cape's) runs forward over his shoulders and across the front of his throat, so the cape hangs
// from it instead of in the air beside him. Each link a small plume laid along the chain, out of step with the next.
static void EnGanonMant_DrawClasp(PlayState* play, EnGanonMant* this, s32 ice) {
    Vec3f neck;
    Vec3f a;
    Vec3f b;
    s32 links = (s32)GdfTune("clasp.links", 14.0f);
    f32 sag = GdfTune("clasp.sag", 3.0f);
    f32 thick = GdfTune("clasp.thickness", 1.0f);
    Vec3f prev;
    s32 k;

    if ((GdfTune("clasp.on", 1.0f) == 0.0f) || !BossGanon_Player_GetNeckFront(&neck) || (links < 2)) {
        return;
    }
    a = this->strands[0].joints[0];
    b = this->strands[GANON_MANT_NUM_STRANDS - 1].joints[0];

    // (as a chain: the drawbridge's links)
    if (GdfTune("clasp.style", 1.0f) != 0.0f) {
        EnGanonMant_DrawChain(play, &a, &b, &neck, sag, ice);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gVolvagiaManeMaterialDL);
    CLOSE_DISPS(play->state.gfxCtx);
    EnGanonMant_XluDepth(play);
    OPEN_DISPS(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH);
    if (ice) {
        gDPSetEnvColor(POLY_XLU_DISP++, (u8)GdfTuneAt("frost.glow_color", 0, 60), (u8)GdfTuneAt("frost.glow_color", 1, 140),
                       (u8)GdfTuneAt("frost.glow_color", 2, 255), 0);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)GdfTuneAt("frost.color", 0, 200), (u8)GdfTuneAt("frost.color", 1, 235),
                        (u8)GdfTuneAt("frost.color", 2, 255), 255);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 170, 0, 255);
    }
    // A curve through the neck point: corner, throat, corner (quadratic, its middle pulled to pass through the throat)
    for (k = 0; k <= links; k++) {
        f32 t = (f32)k / links;
        f32 u = 1.0f - t;
        Vec3f c = { 2.0f * neck.x - 0.5f * (a.x + b.x), 2.0f * neck.y - 0.5f * (a.y + b.y) - sag,
                    2.0f * neck.z - 0.5f * (a.z + b.z) };
        Vec3f p = { u * u * a.x + 2 * u * t * c.x + t * t * b.x, u * u * a.y + 2 * u * t * c.y + t * t * b.y,
                    u * u * a.z + 2 * u * t * c.z + t * t * b.z };

        // (the V: straight from each corner down to the point, which sits on the breastplate's emblem, the two
        // halves clasped together there)
        if (GdfTune("clasp.shape", 1.0f) == 1.0f) {
            f32 h = (t < 0.5f) ? (t * 2.0f) : ((t - 0.5f) * 2.0f);
            Vec3f* from = (t < 0.5f) ? &a : &neck;
            Vec3f* to = (t < 0.5f) ? &neck : &b;

            p.x = from->x + (to->x - from->x) * h;
            p.y = from->y + (to->y - from->y) * h;
            p.z = from->z + (to->z - from->z) * h;
        }

        if (k > 0) {
            Vec3f d = { p.x - prev.x, p.y - prev.y, p.z - prev.z };
            f32 len = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
            Vec3f side;
            Vec3f out;
            f32 sl;
            f32 pulse = 1.0f + (ice ? 0.06f : 0.25f) * Math_SinS(play->gameplayFrames * (ice ? 900 : 5000) + k * 9000);
            f32 sy;
            f32 sx;
            MtxF mf;

            if (len < 0.01f) {
                prev = p;
                continue;
            }
            d.x /= len;
            d.y /= len;
            d.z /= len;
            // (lying flat on him: its width across the chain, sideways to world up)
            side.x = d.z;
            side.y = 0.0f;
            side.z = -d.x;
            sl = sqrtf(SQ(side.x) + SQ(side.z));
            if (sl < 0.01f) {
                side.x = 1.0f;
                side.z = 0.0f;
                sl = 1.0f;
            }
            side.x /= sl;
            side.z /= sl;
            out.x = side.y * d.z - side.z * d.y;
            out.y = side.z * d.x - side.x * d.z;
            out.z = side.x * d.y - side.y * d.x;
            sy = len * 1.7f / 6750.0f * pulse;
            sx = 3.0f * thick / 3900.0f * pulse;
            mf.xx = side.x * sx; mf.yx = side.y * sx; mf.zx = side.z * sx; mf.wx = 0.0f;
            mf.xy = d.x * sy;    mf.yy = d.y * sy;    mf.zy = d.z * sy;    mf.wy = 0.0f;
            mf.xz = out.x * sx;  mf.yz = out.y * sx;  mf.zz = out.z * sx;  mf.wz = 0.0f;
            mf.xw = prev.x;      mf.yw = prev.y;      mf.zw = prev.z;      mf.ww = 1.0f;
            Matrix_Put(&mf);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gVolvagiaManeModelDL);
        }
        prev = p;
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// (for the mage's hood, drawn by Ganondorf: the same cloth)
u8* EnGanonMant_GetRobeTex(void) {
    return EnGanonMant_BuildRobeTex() ? sRobeMantTex : NULL;
}

void EnGanonMant_DrawCloak(PlayState* play, EnGanonMant* this) {
    s32 pad;
    Mtx* mtx;

    OPEN_DISPS(play->state.gfxCtx);

    // Invalidate cape texture mask as it may have been torn
    gSPInvalidateTexCache(POLY_OPA_DISP++, sMaskTex);

    if (this->isLinkCape || this->ownVertices) {
        Matrix_Translate(this->anchor.x, this->anchor.y, this->anchor.z, MTXMODE_NEW);
        Matrix_Scale(1.0f / LINK_GANON_CAPE_VTX_SCALE, 1.0f / LINK_GANON_CAPE_VTX_SCALE,
                     1.0f / LINK_GANON_CAPE_VTX_SCALE, MTXMODE_APPLY);
    } else {
        Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    }

    mtx = MATRIX_NEWMTX(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // set texture
    if (EnGanonMant_IsPhantom(this) && EnGanonMant_BuildPhantomTex()) {
        gSPInvalidateTexCache(POLY_OPA_DISP++, sPhantomMantTex);
        gDPPipeSync(POLY_OPA_DISP++);
        gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPLoadTextureBlock(POLY_OPA_DISP++, sPhantomMantTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, MANT_TEX_WIDTH,
                            MANT_TEX_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, 5, 6, G_TX_NOLOD,
                            G_TX_NOLOD);
    } else if (EnGanonMant_IsRobe(this) && EnGanonMant_BuildRobeTex()) {
        gSPDisplayList(POLY_OPA_DISP++, gMantMaterialDL);
        gSPInvalidateTexCache(POLY_OPA_DISP++, sRobeMantTex);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPLoadTextureBlock(POLY_OPA_DISP++, sRobeMantTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, MANT_TEX_WIDTH, MANT_TEX_HEIGHT,
                            0, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, 5, 6, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gSPDisplayList(POLY_OPA_DISP++, gMantMaterialDL);
    }

    // set vertices, vertices are double buffered to prevent
    // modification of vertices as they are being drawn
    if (this->ownVertices && (EnGanonMant_GetOwnVertices(this) != NULL)) {
        gSPSegment(POLY_OPA_DISP++, 0x0C, EnGanonMant_GetOwnVertices(this));
    } else if (this->frameTimer % 2 != 0) {
        gSPSegmentLoadRes(POLY_OPA_DISP++, 0x0C, gMant1Vtx);
    } else {
        gSPSegmentLoadRes(POLY_OPA_DISP++, 0x0C, gMant2Vtx);
    }

    // draw cloak (SoH: with the Goron Tunic, flame in its place)
    if (this->ownVertices && !EnGanonMant_IsRobe(this) && (BossGanon_Player_GoronLook() || BossGanon_Player_ZoraLook())) {
        s32 ice = BossGanon_Player_ZoraLook();

        EnGanonMant_DrawFire(play, mtx, EnGanonMant_GetOwnVertices(this), ice);
        if (this->actor.params == PLAYER_GANONDORF_CAPE_PARAMS) {
            EnGanonMant_DrawMane(play, this, ice);
            EnGanonMant_DrawCollar(play, this, mtx, ice); // (of ice, or of flame on the flame cape)
            EnGanonMant_DrawClasp(play, this, ice);
            EnGanonMant_DrawPlate(play, this, mtx); // (set into his armour)
        }
    } else {
        gSPDisplayList(POLY_OPA_DISP++, gMantDL);
    }

    if (this->actor.params == PLAYER_GANONDORF_CAPE_PARAMS) {
        // SoH: the player-controlled Ganondorf's hood, the same way as Link's
        void* hoodVtx = GanondorfHood_Build(this, play);

        if (hoodVtx != NULL) {
            if (BossGanon_Player_ZoraLook() && EnGanonMant_BuildRobeTex()) {
                // (the mage's hood: the robe's dark blue cloth)
                gSPDisplayList(POLY_OPA_DISP++, gMantMaterialDL);
                gSPInvalidateTexCache(POLY_OPA_DISP++, sRobeMantTex);
                gDPPipeSync(POLY_OPA_DISP++);
                gDPLoadTextureBlock(POLY_OPA_DISP++, sRobeMantTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, MANT_TEX_WIDTH,
                                    MANT_TEX_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_CLAMP, 5, 6,
                                    G_TX_NOLOD, G_TX_NOLOD);
                gSPSegment(POLY_OPA_DISP++, 0x0C, hoodVtx);
                gSPDisplayList(POLY_OPA_DISP++, gMantDL);
            } else if (BossGanon_Player_GoronLook()) {
                EnGanonMant_DrawFire(play, mtx, hoodVtx, false);
            } else {
                gSPSegment(POLY_OPA_DISP++, 0x0C, hoodVtx);
                gSPDisplayList(POLY_OPA_DISP++, gMantDL);
            }
        }
    }

    if (this->isLinkCape) {
        // SoH: Link's hood, same material and grid as the cloak, sharing its top edge
        void* hoodVtx = LinkGanonCape_BuildHood(this, play);

        if (hoodVtx != NULL) {
            gSPSegment(POLY_OPA_DISP++, 0x0C, hoodVtx);
            gSPDisplayList(POLY_OPA_DISP++, gMantDL);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// SoH: a cloth sewn on (the player-controlled Ganondorf's cloak): its roots and push from outside, and its first strand
// the one before's last, so the two are one piece of fabric
static void EnGanonMant_SimLinked(PlayState* play, EnGanonMant* this, EnGanonMant* prev, s32 sewn) {
    s16 strandIdx;

    this->frameTimer = prev->frameTimer; // (the same buffer as the one it's drawn with)
    this->baseYaw = this->overrideBaseYaw;
    this->anchor = this->overrideRoots[GANON_MANT_NUM_STRANDS / 2];
    for (strandIdx = 0; strandIdx < GANON_MANT_NUM_STRANDS; strandIdx++) {
        s16 nextStrandIdx = (strandIdx + 1 < GANON_MANT_NUM_STRANDS) ? (strandIdx + 1) : (strandIdx - 1);

        this->strands[strandIdx].root = this->overrideRoots[strandIdx];
        if ((strandIdx == 0) && sewn) {
            MantStrand* edge = &prev->strands[GANON_MANT_NUM_STRANDS - 1];

            memcpy(this->strands[0].joints, edge->joints, sizeof(edge->joints));
            memcpy(this->strands[0].rotations, edge->rotations, sizeof(edge->rotations));
            memcpy(this->strands[0].velocities, edge->velocities, sizeof(edge->velocities));
            this->strands[0].root = edge->root;
            continue;
        }
        Matrix_Push();
        EnGanonMant_UpdateStrand(play, this, &this->strands[strandIdx].root, this->strands[strandIdx].joints,
                                 this->strands[nextStrandIdx].joints, this->strands[strandIdx].rotations,
                                 this->strands[strandIdx].velocities, strandIdx);
        Matrix_Pop();
    }
    EnGanonMant_UpdateVertices(this);
}

void EnGanonMant_Draw(Actor* thisx, PlayState* play) {
    EnGanonMant* this = (EnGanonMant*)thisx;
    f32 xDiff;
    f32 pitch;
    Vec3f strandOffset;
    Vec3f strandDivPos;
    f32 zDiff;
    f32 diffHalfDist;
    f32 yDiff;
    f32 yaw;
    Vec3f* rightPos;
    Vec3f* leftPos;
    s16 strandIdx;
    Vec3f midpoint;
    s16 nextStrandIdx;

    if (this->isLinked) {
        return; // (sewn onto another cloth, which simulates and draws it)
    }

    if (this->isLinkCape && !LinkGanonCape_PrepareDraw(this, play)) {
        // SoH: Link isn't visible this frame (first person, hidden in a cutscene, ...)
        return;
    }

    if (this->updateHasRun) {
        // Only run this if update has run since last draw

        // Choose endpoints
        if (this->isLinkCape) {
            // SoH: Link's cape always hangs from his shoulders; gravity etc. are set in PrepareDraw
            rightPos = &this->rightShoulderPos;
            leftPos = &this->leftShoulderPos;
        } else if (this->attachRightArmTimer != 0.0f) {
            rightPos = &this->rightForearmPos;
            leftPos = &this->leftShoulderPos;
            this->gravity = -13.0f;
        } else if (this->attachLeftArmTimer != 0.0f) {
            rightPos = &this->rightShoulderPos;
            leftPos = &this->leftForearmPos;
            this->gravity = -13.0f;
        } else if (this->attachShouldersTimer != 0.0f) {
            rightPos = &this->rightShoulderPos;
            leftPos = &this->leftShoulderPos;
            this->gravity = -3.0f;
        } else {
            rightPos = &this->rightForearmPos;
            leftPos = &this->leftForearmPos;
        }

        xDiff = leftPos->x - rightPos->x;
        yDiff = leftPos->y - rightPos->y;
        zDiff = leftPos->z - rightPos->z;

        midpoint.x = rightPos->x + xDiff * 0.5f;
        midpoint.y = rightPos->y + yDiff * 0.5f;
        midpoint.z = rightPos->z + zDiff * 0.5f;

        // Calculte base orientation for chosen endpoints
        yaw = Math_Atan2F(zDiff, xDiff);
        pitch = -Math_Atan2F(sqrtf(SQ(xDiff) + SQ(zDiff)), yDiff);
        diffHalfDist = sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) * 0.5f;
        if (this->widthScale > 0.0f) {
            diffHalfDist *= this->widthScale; // SoH: spread wider than the shoulders it hangs from
        }

        Matrix_RotateY(yaw, MTXMODE_NEW);
        Matrix_RotateX(pitch, MTXMODE_APPLY);
        this->baseYaw = yaw - M_PI / 2.0f;
        // SoH: the player-controlled Ganondorf's cloak: its push from outside
        if (this->rootOverride) {
            this->baseYaw = this->overrideBaseYaw;
        }

        if (this->ownVertices) {
            this->anchor = midpoint;
        }

        if (this->isLinkCape) {
            this->anchor = midpoint;

            if (this->needsReset) {
                // SoH: first frame or Link teleported, let every strand hang straight down from its root
                for (strandIdx = 0; strandIdx < GANON_MANT_NUM_STRANDS; strandIdx++) {
                    s16 j;

                    Matrix_Push();
                    strandOffset.x = sinf((strandIdx * M_PI) / (GANON_MANT_NUM_STRANDS - 1)) * diffHalfDist;
                    strandOffset.y = 0;
                    strandOffset.z = -cosf((strandIdx * M_PI) / (GANON_MANT_NUM_STRANDS - 1)) * diffHalfDist;
                    Matrix_MultVec3f(&strandOffset, &strandDivPos);
                    Matrix_Pop();

                    for (j = 0; j < GANON_MANT_NUM_JOINTS; j++) {
                        this->strands[strandIdx].joints[j].x = midpoint.x + strandDivPos.x;
                        this->strands[strandIdx].joints[j].y = midpoint.y + strandDivPos.y - j * this->jointLength;
                        this->strands[strandIdx].joints[j].z = midpoint.z + strandDivPos.z;
                        this->strands[strandIdx].velocities[j].x = 0.0f;
                        this->strands[strandIdx].velocities[j].y = 0.0f;
                        this->strands[strandIdx].velocities[j].z = 0.0f;
                    }
                }
                this->needsReset = false;
            }
        }

        for (strandIdx = 0; strandIdx < GANON_MANT_NUM_STRANDS; strandIdx++) {
            Matrix_Push();

            // Calculate root positions for chosen endpoints
            strandOffset.x = sinf((strandIdx * M_PI) / (GANON_MANT_NUM_STRANDS - 1)) * diffHalfDist;
            strandOffset.y = 0;
            strandOffset.z = -cosf((strandIdx * M_PI) / (GANON_MANT_NUM_STRANDS - 1)) * diffHalfDist;
            Matrix_MultVec3f(&strandOffset, &strandDivPos);
            this->strands[strandIdx].root.x = midpoint.x + strandDivPos.x;
            this->strands[strandIdx].root.y = midpoint.y + strandDivPos.y;
            this->strands[strandIdx].root.z = midpoint.z + strandDivPos.z;
            // SoH: the player-controlled Ganondorf's cloak: its roots from outside (along his shoulders, or over his
            // left shoulder and down his front)
            if (this->rootOverride) {
                this->strands[strandIdx].root = this->overrideRoots[strandIdx];
            }

            nextStrandIdx = strandIdx + 1;
            if (nextStrandIdx >= GANON_MANT_NUM_STRANDS) {
                nextStrandIdx = strandIdx - 1;
            }

            // Update the strand joints
            EnGanonMant_UpdateStrand(play, this, &this->strands[strandIdx].root, this->strands[strandIdx].joints,
                                     this->strands[nextStrandIdx].joints, this->strands[strandIdx].rotations,
                                     this->strands[strandIdx].velocities, strandIdx);
            Matrix_Pop();
        }
        EnGanonMant_UpdateVertices(this);

        // SoH: the cloths sewn on, in order (each onto the one before; the first onto this one, while its roots are
        // its own and not following an arm)
        for (strandIdx = 0; strandIdx < this->linkedCount; strandIdx++) {
            EnGanonMant* prev = (strandIdx == 0) ? this : this->linked[strandIdx - 1];

            EnGanonMant_SimLinked(play, this->linked[strandIdx], prev, (strandIdx > 0) || this->rootOverride);
        }
        this->updateHasRun = false;
    }

    EnGanonMant_DrawCloak(play, this);
    for (strandIdx = 0; strandIdx < this->linkedCount; strandIdx++) {
        EnGanonMant_DrawCloak(play, this->linked[strandIdx]);
    }
}

// SoH: "Play as Ganondorf", single player: the Goron and Zora Tunics' item models (in shops, on pickup, held up when
// he gets one) as the mantles they are for him: the flame or ice cape itself, still, hanging from its collar, drawn by
// the same code as the one he wears (its fire or ice, Volvagia's plumes down it, the collar standing up round the
// neck), only at the item's size and in its place. A curved collar at the top, the cloth widening as it falls, folds
// down it and a wavy hem; the fire still burns and the plumes still pulse, the cloth itself doesn't move.
void EnGanonMant_DrawItemMantle(PlayState* play, s32 ice) {
    EnGanonMant_DrawHangingCape(play, ice ? 2 : 1);
}

// The same still cape for anywhere it's needed (the item models; the equipment screen, behind him): kind 0 his own red
// cloth, 1 the flame cape, 2 the ice cape. On the current matrix, in the item's units.
void EnGanonMant_DrawHangingCape(PlayState* play, s32 kind) {
    static EnGanonMant sItem; // (only its strands and anchor are used)
    s32 ice = (kind == 2);
    Vtx* src = ResourceMgr_LoadVtxByName(SEGMENTED_TO_VIRTUAL(gMant1Vtx));
    f32 size = GdfTune("item.size", 1.0f);
    Vtx* vtx;
    Mtx* mtx;
    MtxF base;
    f32 unit;
    s32 i;
    s32 j;

    if (src == NULL) {
        return;
    }
    vtx = Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * GANON_MANT_NUM_STRANDS * GANON_MANT_NUM_JOINTS);
    memcpy(vtx, src, sizeof(Vtx) * GANON_MANT_NUM_STRANDS * GANON_MANT_NUM_JOINTS);

    Matrix_Push();
    Matrix_Get(&base);
    unit = sqrtf(SQ(base.xx) + SQ(base.yx) + SQ(base.zx)); // (one of the item's units in the world)

    // Its shape in the item's own units (the tunic it stands in for is about 68 wide and 62 tall): the strands from
    // one side round the back of the collar to the other, the joints down each
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        f32 th = (i * M_PI) / (GANON_MANT_NUM_STRANDS - 1);

        for (j = 0; j < GANON_MANT_NUM_JOINTS; j++) {
            f32 s = (f32)j / (GANON_MANT_NUM_JOINTS - 1);
            f32 halfW = GdfTune("item.top_width", 13.0f) + GdfTune("item.flare", 18.0f) * s;
            f32 depth = 7.0f + 7.0f * s;
            Vec3f p;

            p.x = -cosf(th) * halfW;
            p.z = -sinf(th) * depth + GdfTune("item.folds", 3.0f) * s * sinf(i * 1.9f + j * 0.45f);
            p.y = 28.0f - j * GdfTune("item.length", 5.4f) - 2.0f * s * s * sinf(i * 2.3f + 0.6f);
            p.x *= size;
            p.y *= size;
            p.z *= size;
            Matrix_MultVec3f(&p, &sItem.strands[i].joints[j]);
        }
        sItem.strands[i].root = sItem.strands[i].joints[0];
    }
    sItem.anchor = sItem.strands[GANON_MANT_NUM_STRANDS / 2].joints[0];

    // The cloth's vertices, relative to the collar's middle at higher precision (as his own cape's are)
    for (i = 0; i < GANON_MANT_NUM_STRANDS; i++) {
        Vec3f* left = sItem.strands[MAX(i - 1, 0)].joints;
        Vec3f* right = sItem.strands[MIN(i + 1, GANON_MANT_NUM_STRANDS - 1)].joints;

        for (j = 0; j < GANON_MANT_NUM_JOINTS; j++) {
            Vec3f* p = &sItem.strands[i].joints[j];
            Vec3f* up = &sItem.strands[i].joints[MAX(j - 1, 0)];
            Vec3f* dn = &sItem.strands[i].joints[MIN(j + 1, GANON_MANT_NUM_JOINTS - 1)];
            Vec3f a = { right[j].x - left[j].x, right[j].y - left[j].y, right[j].z - left[j].z };
            Vec3f d = { dn->x - up->x, dn->y - up->y, dn->z - up->z };
            Vec3f n = { a.y * d.z - a.z * d.y, a.z * d.x - a.x * d.z, a.x * d.y - a.y * d.x };
            f32 nl = sqrtf(SQ(n.x) + SQ(n.y) + SQ(n.z));
            Vtx* vt = &vtx[sVerticesMap[i + j * GANON_MANT_NUM_JOINTS]];

            vt->n.ob[0] = CLAMP((p->x - sItem.anchor.x) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vt->n.ob[1] = CLAMP((p->y - sItem.anchor.y) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vt->n.ob[2] = CLAMP((p->z - sItem.anchor.z) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            if (nl > 0.001f) {
                vt->n.n[0] = (s8)(n.x / nl * 120.0f);
                vt->n.n[1] = (s8)(n.y / nl * 120.0f);
                vt->n.n[2] = (s8)(n.z / nl * 120.0f);
            }
        }
    }

    Matrix_Translate(sItem.anchor.x, sItem.anchor.y, sItem.anchor.z, MTXMODE_NEW);
    Matrix_Scale(1.0f / LINK_GANON_CAPE_VTX_SCALE, 1.0f / LINK_GANON_CAPE_VTX_SCALE, 1.0f / LINK_GANON_CAPE_VTX_SCALE,
                 MTXMODE_APPLY);
    mtx = MATRIX_NEWMTX(play->state.gfxCtx);

    if (kind == 0) {
        // (his own cloth: the cape's material and mesh, lit)
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gMantMaterialDL);
        gSPSegment(POLY_OPA_DISP++, 0x0C, vtx);
        gSPDisplayList(POLY_OPA_DISP++, gMantDL);
        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        EnGanonMant_DrawFire(play, mtx, vtx, ice);
        EnGanonMant_DrawMane(play, &sItem, ice);
        sCollarScale = unit * size * GdfTune("item.collar", 0.6f);
        EnGanonMant_DrawCollar(play, &sItem, mtx, ice);
        sCollarScale = 1.0f;
    }

    Matrix_Pop();
}

// SoH: "Play as Ganondorf", the tunics' icons on the equipment screen as the capes they give him: his own red cloak
// (drawn from the cape's own texture) for the Kokiri Tunic, the flame cape for the Goron Tunic and the ice cape for the
// Zora Tunic. Drawn here once, 32x32 like the icons they stand in for: a wavy cloak from the front, turned a little to
// the right (flame tongues on the fire, icicles on the ice), an outline and a gold clasp at the collar.
static u8 sCapeIcon[3][32 * 32 * 4];
static u8 sCapeIconMask[32 * 32];
static s8 sCapeIconsOn = false;
static s8 sCapeIconsMade = false;

// The cloak from the front, turned a little to its right (its right side recedes), leaning and swaying right as if
// in the wind, wavy folds drifting down it and a wavy hem
static s32 EnGanonMant_IconRow(s32 y, f32* v, f32* h, f32* cx) {
    if (y < 4) {
        return false;
    }
    *v = (y - 4) / 24.0f;
    *h = (y < 8) ? 5.5f : (5.5f + (y - 8) * 0.45f);
    *cx = 15.5f + 3.0f * SQ(*v) + 1.2f * sinf(*v * M_PI * 2.2f);
    return true;
}

static f32 EnGanonMant_IconHem(s32 x, s32 kind) {
    f32 base = 27.5f + 1.6f * sinf(x * 0.55f + 0.8f);
    f32 f;

    switch (kind) {
        case 1: // (flame tongues)
            f = x * 0.37f - floorf(x * 0.37f);
            return base - 4.5f + 4.5f * (1.0f - fabsf(f * 2.0f - 1.0f));
        case 2: // (icicles)
            f = x * 0.5f - floorf(x * 0.5f);
            return base - 3.0f + 3.0f * (1.0f - fabsf(f * 2.0f - 1.0f));
        default:
            return base;
    }
}

static void EnGanonMant_MakeCapeIcon(s32 kind, u8* out) {
    u8* mant = (u8*)ResourceGetDataByName(gMantTex);
    u8 in[32][32];
    f32 v, h, cx;
    s32 x, y;

    memset(out, 0, 32 * 32 * 4);
    memset(in, 0, sizeof(in));
    for (y = 0; y < 32; y++) {
        f32 hl, hr;

        if (!EnGanonMant_IconRow(y, &v, &h, &cx)) {
            continue;
        }
        hl = h * 1.12f;
        hr = h * 0.82f;
        for (x = 0; x < 32; x++) {
            f32 dx = x + 0.5f - cx;
            f32 u, k, q;
            s32 r, g, b;
            u8* o = &out[(y * 32 + x) * 4];

            if ((dx < -hl) || (dx > hr) || (y + 0.5f > EnGanonMant_IconHem(x, kind))) {
                continue;
            }
            in[y][x] = true;
            u = (dx + hl) / (hl + hr);
            // (its folds, and lit from its left, its far side darker)
            k = (0.7f + 0.3f * cosf(u * M_PI * 5.0f - v * 3.0f)) * (1.08f - 0.38f * u);
            if (y < 8) {
                k *= 1.15f;
            }
            if (kind == 0) {
                s32 tu = CLAMP((s32)(u * 32.0f), 0, 31);
                s32 tv = CLAMP((s32)(v * 64.0f), 0, 63);
                u16 px = (mant != NULL) ? ((mant[(tv * 32 + tu) * 2] << 8) | mant[(tv * 32 + tu) * 2 + 1]) : 0x9001;

                r = ((px >> 11) & 0x1F) * 255 / 31;
                g = ((px >> 6) & 0x1F) * 255 / 31;
                b = ((px >> 1) & 0x1F) * 255 / 31;
            } else if (kind == 1) {
                q = v + 0.12f * sinf(u * 19.0f + v * 7.0f);
                r = 255;
                g = (s32)(240.0f - 200.0f * q);
                b = (s32)(150.0f - 150.0f * CLAMP_MAX(q * 1.6f, 1.0f));
                if (q > 0.75f) {
                    r = 230;
                    g = 40;
                    b = 10;
                }
            } else {
                q = v + 0.1f * sinf(u * 17.0f + v * 9.0f);
                r = (s32)(225.0f - 170.0f * q);
                g = (s32)(245.0f - 120.0f * q);
                b = 255;
            }
            o[0] = CLAMP((s32)(r * k), 0, 255);
            o[1] = CLAMP((s32)(g * k), 0, 255);
            o[2] = CLAMP((s32)(b * k), 0, 255);
            o[3] = 255;
        }
    }
    // (its outline)
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            if (!in[y][x] && (((y > 0) && in[y - 1][x]) || ((y < 31) && in[y + 1][x]) || ((x > 0) && in[y][x - 1]) ||
                              ((x < 31) && in[y][x + 1]))) {
                u8* o = &out[(y * 32 + x) * 4];

                o[0] = 20;
                o[1] = 10;
                o[2] = 10;
                o[3] = 255;
            }
        }
    }
    // (the gold clasp, at the collar)
    EnGanonMant_IconRow(5, &v, &h, &cx);
    for (y = 5; y <= 6; y++) {
        for (x = (s32)cx - 1; x <= (s32)cx; x++) {
            u8* o = &out[(y * 32 + x) * 4];

            o[0] = 250;
            o[1] = 210;
            o[2] = 80;
            o[3] = 255;
        }
    }
}

// ...and their names on that screen: "Royal Cape", "Fire Mantle" and "Ice Mantle" (a mantle being the longer of the
// two), written in the game's own message font the way the names are drawn (128x16, light letters outlined in dark,
// centered)
extern f32 sFontWidths[];
static u8 sCapeName[3][128 * 16 / 2];
static u8 sCapeNameMask[128 * 16];

static void EnGanonMant_MakeCapeName(const char* text, u8* out) {
    static u8 val[16][128];
    f32 width = 0.0f;
    f32 x;
    s32 i, gx, gy;

    memset(val, 0, sizeof(val));
    for (i = 0; text[i] != '\0'; i++) {
        width += sFontWidths[text[i] - ' '];
    }
    x = (s32)((128.0f - width) * 0.5f);
    for (i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        char name[96];
        u8* glyph;

        if ((c >= 'A') && (c <= 'Z')) {
            snprintf(name, sizeof(name), "__OTR__textures/nes_font_static/gMsgChar%02XLatinCapitalLetter%cTex", c, c);
        } else if ((c >= 'a') && (c <= 'z')) {
            snprintf(name, sizeof(name), "__OTR__textures/nes_font_static/gMsgChar%02XLatinSmallLetter%cTex", c,
                     c - 'a' + 'A');
        } else {
            name[0] = '\0';
        }
        glyph = (name[0] != '\0') ? (u8*)ResourceGetDataByName(name) : NULL;
        if (glyph != NULL) {
            for (gy = 0; gy < 16; gy++) {
                for (gx = 0; gx < 16; gx++) {
                    s32 X = (s32)x + gx;
                    u8 v = (glyph[(gy * 16 + gx) / 2] >> (((gx & 1) == 0) ? 4 : 0)) & 0xF;

                    if ((X >= 0) && (X < 128) && (v > val[gy][X])) {
                        val[gy][X] = v;
                    }
                }
            }
        }
        x += sFontWidths[c - ' '];
    }
    // IA4: 3 bits of light, 1 of cover; the letters, then a dark outline around them
    memset(out, 0, 128 * 16 / 2);
    for (gy = 0; gy < 16; gy++) {
        for (gx = 0; gx < 128; gx++) {
            u8 px = 0;

            if (val[gy][gx] >= 6) {
                px = (MIN(7, val[gy][gx] >> 1) << 1) | 1;
            } else {
                s32 dy, dx;

                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        if ((gy + dy >= 0) && (gy + dy < 16) && (gx + dx >= 0) && (gx + dx < 128) &&
                            (val[gy + dy][gx + dx] >= 6)) {
                            px = 1;
                        }
                    }
                }
            }
            out[(gy * 128 + gx) / 2] |= ((gx & 1) == 0) ? (px << 4) : px;
        }
    }
}

static void EnGanonMant_CapeNames(s32 on) {
    static const char* sNames[3] = { gKokiriTunicItemNameENGTex, gGoronTunicItemNameENGTex, gZoraTunicItemNameENGTex };
    static const char* sText[3] = { "Royal Cape", "Fire Mantle", "Ice Mantle" };
    static s8 sMade = false;
    s32 i;

    if (on && !sMade) {
        memset(sCapeNameMask, 0xFF, sizeof(sCapeNameMask));
        for (i = 0; i < 3; i++) {
            EnGanonMant_MakeCapeName(sText[i], sCapeName[i]);
        }
        sMade = true;
    }
    for (i = 0; i < 3; i++) {
        if (on) {
            Gfx_RegisterBlendedTexture(sNames[i], sCapeNameMask, sCapeName[i]);
        } else {
            Gfx_UnregisterBlendedTexture(sNames[i]);
        }
    }
}

void EnGanonMant_CapeIcons(s32 on) {
    static const char* sIcons[3] = { gItemIconTunicKokiriTex, gItemIconTunicGoronTex, gItemIconTunicZoraTex };
    s32 i;

    on = on && (GdfTune("icons.capes", 1.0f) != 0.0f);
    if (on == sCapeIconsOn) {
        return;
    }
    if (on && !sCapeIconsMade) {
        memset(sCapeIconMask, 0xFF, sizeof(sCapeIconMask));
        for (i = 0; i < 3; i++) {
            EnGanonMant_MakeCapeIcon(i, sCapeIcon[i]);
        }
        sCapeIconsMade = true;
    }
    for (i = 0; i < 3; i++) {
        if (on) {
            Gfx_RegisterBlendedTexture(sIcons[i], sCapeIconMask, sCapeIcon[i]);
        } else {
            Gfx_UnregisterBlendedTexture(sIcons[i]);
        }
    }
    EnGanonMant_CapeNames(on);
    sCapeIconsOn = on;
}
