// Play as Ganondorf: his hood (the "Hood" option). Built every frame around his head, the way Link's hood is
// (soh/Enhancements/Graphics/LinkGanonCape.cpp): a 12x12 grid drawn with his cape's own display list, so it has the
// cape's cloth, colour and lighting. The top 9 rows are a rigid shell on his head; the last 3 drape down to the cape's
// collar along his shoulders, so hood and cape meet.
//
// The shell (sGdfHoodShell) was shrink-wrapped around his head and the hair on it, with a margin, in his face's limb
// (the jewel's: x forward, y down, z across), by soh/tools/gdf_hud/hood_bake.py. Its brim stands out over his brow, and
// the face is left open from the brow down; z_boss_ganon.c darkens the face under it.

#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"
// z_boss_ganon.c: his face limb's matrix (last draw) and the cape's collar (12 points along his shoulders)
s32 BossGanon_Player_GetHoodFrame(EnGanonMant* cape, MtxF* head, Vec3f* collar);
}

#define GDF_HOOD_COLS GANON_MANT_NUM_STRANDS
#define GDF_HOOD_ROWS GANON_MANT_NUM_JOINTS
#define GDF_HOOD_SHELL_ROWS 9

// clang-format off
// [row][column] = { x, y, z }: row 0 the brim, row 8 the shell's bottom at the nape; column 0 on the -z side
static const s16 sGdfHoodShell[GDF_HOOD_SHELL_ROWS][GDF_HOOD_COLS][3] = {
    { { 645, -328, -1051 }, { 601, -609, -869 }, { 714, -928, -896 }, { 762, -1068, -608 }, { 869, -1155, -357 }, { 1049, -1201, -119 }, { 1049, -1200, 119 }, { 869, -1154, 357 }, { 762, -1068, 608 }, { 714, -928, 896 }, { 601, -609, 869 }, { 645, -328, 1051 } },
    { { 352, -343, -882 }, { 360, -614, -890 }, { 397, -868, -790 }, { 468, -1108, -645 }, { 564, -1277, -416 }, { 656, -1303, -134 }, { 656, -1303, 134 }, { 564, -1277, 416 }, { 468, -1108, 645 }, { 397, -868, 790 }, { 360, -614, 890 }, { 352, -343, 882 } },
    { { 129, -346, -843 }, { 100, -635, -990 }, { 131, -906, -858 }, { 183, -1090, -629 }, { 235, -1272, -414 }, { 300, -1357, -142 }, { 300, -1357, 142 }, { 235, -1272, 414 }, { 183, -1090, 629 }, { 131, -906, 858 }, { 100, -635, 990 }, { 129, -346, 843 } },
    { { -84, -352, -776 }, { -147, -614, -890 }, { -120, -853, -763 }, { -107, -1069, -609 }, { -115, -1287, -421 }, { -83, -1366, -144 }, { -83, -1366, 144 }, { -115, -1287, 421 }, { -107, -1069, 609 }, { -120, -853, 763 }, { -147, -614, 890 }, { -84, -352, 776 } },
    { { -344, -355, -737 }, { -347, -578, -726 }, { -352, -785, -644 }, { -410, -996, -540 }, { -437, -1139, -349 }, { -433, -1182, -116 }, { -433, -1182, 116 }, { -437, -1139, 349 }, { -410, -996, 540 }, { -352, -785, 644 }, { -347, -578, 726 }, { -344, -355, 737 } },
    { { -845, -352, -775 }, { -539, -540, -553 }, { -538, -686, -470 }, { -567, -795, -352 }, { -630, -863, -215 }, { -705, -877, -69 }, { -705, -877, 69 }, { -630, -863, 215 }, { -567, -795, 352 }, { -538, -686, 470 }, { -539, -540, 553 }, { -845, -352, 775 } },
    { { -994, -380, -455 }, { -833, -503, -382 }, { -760, -579, -281 }, { -815, -613, -181 }, { -872, -578, -77 }, { -1125, -492, -11 }, { -1125, -492, 11 }, { -872, -578, 77 }, { -815, -613, 181 }, { -760, -579, 281 }, { -833, -503, 382 }, { -994, -380, 455 } },
    { { -1138, -410, -116 }, { -1133, -442, -101 }, { -1141, -434, -25 }, { -1154, -346, 70 }, { -1148, -170, 122 }, { -1095, 51, 71 }, { -1095, 51, -71 }, { -1148, -170, -122 }, { -1154, -346, -70 }, { -1141, -434, 25 }, { -1133, -442, 101 }, { -1138, -410, 116 } },
    { { -1107, -442, 251 }, { -1109, -363, 263 }, { -1094, -240, 318 }, { -1038, -38, 358 }, { -784, 149, 277 }, { -582, 328, 114 }, { -582, 328, -114 }, { -784, 149, -277 }, { -1038, -38, -358 }, { -1094, -240, -318 }, { -1109, -363, -263 }, { -1107, -442, -251 } },
};
// clang-format on

// How far the drape rows bow outwards between the shell and the collar (world units)
#define GDF_HOOD_DRAPE_BULGE 1.6f

// The cape's texture coordinates (from gMant1Vtx), carried on up into the hood as Link's hood does
#define GDF_HOOD_TC_V(col) (-482 + 241 * (col))
#define GDF_HOOD_TC_U(row) (186 * ((row) - (GDF_HOOD_ROWS - 2)))

static const Vec3f sGdfHoodCenter = { 300.0f, -420.0f, 0.0f };

static Vec3f Gh_Normalize(Vec3f v) {
    f32 len = sqrtf(SQ(v.x) + SQ(v.y) + SQ(v.z));
    if (len < 0.00001f) {
        return v;
    }
    return { v.x / len, v.y / len, v.z / len };
}

static Vec3f Gh_Cross(Vec3f a, Vec3f b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

static f32 Gh_Dot(Vec3f a, Vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static f32 Gh_Dist2(Vec3f a, Vec3f b) {
    return SQ(a.x - b.x) + SQ(a.y - b.y) + SQ(a.z - b.z);
}

extern "C" void* GanondorfHood_Build(EnGanonMant* cape, PlayState* play) {
    static Vec3f sPos[GDF_HOOD_ROWS][GDF_HOOD_COLS];
    static Vec3f sNormal[GDF_HOOD_ROWS][GDF_HOOD_COLS];
    MtxF head;
    Vec3f collar[GDF_HOOD_COLS];

    if (!CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Hood"), 1) || !BossGanon_Player_GetHoodFrame(cape, &head, collar)) {
        return NULL;
    }

    // Which side of his head is the collar's first point on? Mirror the hood to match.
    Vec3f side0;
    Vec3f local0 = { (f32)sGdfHoodShell[GDF_HOOD_SHELL_ROWS - 1][0][0], (f32)sGdfHoodShell[GDF_HOOD_SHELL_ROWS - 1][0][1],
                     (f32)sGdfHoodShell[GDF_HOOD_SHELL_ROWS - 1][0][2] };
    SkinMatrix_Vec3fMtxFMultXYZ(&head, &local0, &side0);
    bool mirror = Gh_Dist2(side0, collar[0]) > Gh_Dist2(side0, collar[GDF_HOOD_COLS - 1]);

    Vec3f center;
    SkinMatrix_Vec3fMtxFMultXYZ(&head, (Vec3f*)&sGdfHoodCenter, &center);

    for (s32 col = 0; col < GDF_HOOD_COLS; col++) {
        s32 shellCol = mirror ? (GDF_HOOD_COLS - 1 - col) : col;

        for (s32 row = 0; row < GDF_HOOD_SHELL_ROWS; row++) {
            Vec3f local = { (f32)sGdfHoodShell[row][shellCol][0], (f32)sGdfHoodShell[row][shellCol][1],
                            (f32)sGdfHoodShell[row][shellCol][2] };
            SkinMatrix_Vec3fMtxFMultXYZ(&head, &local, &sPos[row][col]);
        }
        // Down from the shell's bottom to the collar, bowing out a little
        Vec3f top = sPos[GDF_HOOD_SHELL_ROWS - 1][col];
        for (s32 row = GDF_HOOD_SHELL_ROWS; row < GDF_HOOD_ROWS; row++) {
            f32 t = (f32)(row - (GDF_HOOD_SHELL_ROWS - 1)) / (GDF_HOOD_ROWS - GDF_HOOD_SHELL_ROWS);
            Vec3f p = { top.x + (collar[col].x - top.x) * t, top.y + (collar[col].y - top.y) * t,
                        top.z + (collar[col].z - top.z) * t };

            if (row != GDF_HOOD_ROWS - 1) {
                Vec3f out = Gh_Normalize({ p.x - center.x, 0.0f, p.z - center.z });
                f32 bulge = GDF_HOOD_DRAPE_BULGE * sinf(t * (f32)M_PI);
                p.x += out.x * bulge;
                p.z += out.z * bulge;
            }
            sPos[row][col] = p;
        }
    }

    // Normals from the grid, pointing away from his head
    for (s32 row = 0; row < GDF_HOOD_ROWS; row++) {
        for (s32 col = 0; col < GDF_HOOD_COLS; col++) {
            Vec3f* rp = &sPos[MAX(row - 1, 0)][col];
            Vec3f* rn = &sPos[MIN(row + 1, GDF_HOOD_ROWS - 1)][col];
            Vec3f* cp = &sPos[row][MAX(col - 1, 0)];
            Vec3f* cn = &sPos[row][MIN(col + 1, GDF_HOOD_COLS - 1)];
            Vec3f n = Gh_Normalize(Gh_Cross({ rn->x - rp->x, rn->y - rp->y, rn->z - rp->z },
                                            { cn->x - cp->x, cn->y - cp->y, cn->z - cp->z }));
            Vec3f away = { sPos[row][col].x - center.x, sPos[row][col].y - center.y, sPos[row][col].z - center.z };

            if (Gh_Dot(n, away) < 0.0f) {
                n = { -n.x, -n.y, -n.z };
            }
            sNormal[row][col] = n;
        }
    }

    Vtx* vertices = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * GDF_HOOD_ROWS * GDF_HOOD_COLS);

    for (s32 row = 0; row < GDF_HOOD_ROWS; row++) {
        for (s32 col = 0; col < GDF_HOOD_COLS; col++) {
            // The cape's vertex layout (sVerticesMap in z_en_ganon_mant.c)
            Vtx* vtx = &vertices[(GDF_HOOD_COLS - 1 - col) + GDF_HOOD_COLS * row];
            Vec3f* p = &sPos[row][col];

            vtx->n.ob[0] = (s16)CLAMP((p->x - cape->anchor.x) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vtx->n.ob[1] = (s16)CLAMP((p->y - cape->anchor.y) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vtx->n.ob[2] = (s16)CLAMP((p->z - cape->anchor.z) * LINK_GANON_CAPE_VTX_SCALE, -32000, 32000);
            vtx->n.flag = 0;
            vtx->n.tc[0] = GDF_HOOD_TC_U(row);
            vtx->n.tc[1] = GDF_HOOD_TC_V(col);
            vtx->n.n[0] = (s8)(sNormal[row][col].x * 30.0f);
            vtx->n.n[1] = (s8)(sNormal[row][col].y * 30.0f);
            vtx->n.n[2] = (s8)(sNormal[row][col].z * 30.0f);
            vtx->n.a = 255;
        }
    }
    return vertices;
}
