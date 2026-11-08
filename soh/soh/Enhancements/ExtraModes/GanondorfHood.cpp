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
    { { 689, -344, -866 }, { 872, -685, -1216 }, { 941, -1017, -1053 }, { 993, -1185, -718 }, { 1126, -1278, -417 }, { 1320, -1281, -131 }, { 1319, -1281, 131 }, { 1126, -1278, 417 }, { 993, -1185, 718 }, { 941, -1017, 1053 }, { 872, -685, 1216 }, { 689, -344, 866 } },
    { { 437, -341, -904 }, { 462, -637, -999 }, { 521, -963, -959 }, { 598, -1210, -742 }, { 704, -1379, -466 }, { 831, -1451, -157 }, { 830, -1450, 156 }, { 704, -1379, 466 }, { 598, -1210, 742 }, { 521, -963, 959 }, { 462, -637, 999 }, { 437, -341, 904 } },
    { { 182, -336, -962 }, { 166, -662, -1112 }, { 198, -966, -963 }, { 251, -1165, -699 }, { 311, -1388, -470 }, { 384, -1485, -162 }, { 384, -1485, 162 }, { 311, -1388, 470 }, { 251, -1165, 699 }, { 198, -966, 963 }, { 166, -662, 1112 }, { 182, -336, 962 } },
    { { -59, -344, -864 }, { -117, -635, -989 }, { -96, -908, -862 }, { -65, -1119, -656 }, { -48, -1302, -429 }, { -63, -1522, -167 }, { -63, -1522, 167 }, { -48, -1302, 429 }, { -65, -1119, 656 }, { -96, -908, 862 }, { -117, -635, 989 }, { -59, -344, 864 } },
    { { -348, -348, -825 }, { -352, -597, -814 }, { -356, -828, -720 }, { -410, -1061, -601 }, { -439, -1221, -389 }, { -436, -1271, -129 }, { -436, -1271, 129 }, { -439, -1221, 389 }, { -410, -1061, 601 }, { -356, -828, 720 }, { -352, -597, 814 }, { -348, -348, 825 } },
    { { -865, -345, -857 }, { -643, -567, -675 }, { -617, -737, -559 }, { -611, -851, -404 }, { -669, -926, -246 }, { -746, -945, -80 }, { -746, -945, 80 }, { -669, -926, 246 }, { -611, -851, 404 }, { -617, -737, 559 }, { -643, -567, 675 }, { -865, -345, 857 } },
    { { -1052, -375, -515 }, { -1025, -526, -485 }, { -901, -618, -349 }, { -883, -649, -215 }, { -934, -616, -95 }, { -1182, -533, -17 }, { -1182, -533, 17 }, { -934, -616, 95 }, { -883, -649, 215 }, { -901, -618, 349 }, { -1025, -526, 485 }, { -1052, -375, 515 } },
    { { -1205, -408, -142 }, { -1199, -447, -125 }, { -1208, -445, -43 }, { -1223, -357, 59 }, { -1220, -176, 119 }, { -1170, 54, 72 }, { -1170, 54, -72 }, { -1220, -176, -119 }, { -1223, -357, -59 }, { -1208, -445, 43 }, { -1199, -447, 125 }, { -1205, -408, 142 } },
    { { -1175, -443, 263 }, { -1177, -360, 276 }, { -1161, -231, 333 }, { -1103, -20, 376 }, { -845, 181, 292 }, { -636, 373, 120 }, { -636, 373, -120 }, { -845, 181, -292 }, { -1103, -20, -376 }, { -1161, -231, -333 }, { -1177, -360, -276 }, { -1175, -443, -263 } },
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

    if (true || !BossGanon_Player_GetHoodFrame(cape, &head, collar)) { // (set aside on 9 Nov 2026)
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
