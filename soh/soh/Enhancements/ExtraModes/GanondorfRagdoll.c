#include "GanondorfRagdoll.h"
#include "functions.h"
#include "macros.h"
#include <math.h>
#include <string.h>

// SoH: "Play as Ganondorf", ragdolls for the ones he throws. See GanondorfRagdoll.h.
//
// The skeleton is read when the thrown actor is next drawn: each limb's joint (jointPos, in model units), its parent,
// and the pose it was drawn in. Every limb's joint becomes a point (in world space), every bone a link of its length;
// a limb with several children (a torso: neck and both shoulders, a pelvis: both hips) is held rigid by links between
// all of them. Zero-length limbs (the many joints that sit on their parent's) share their parent's point. A soft
// limit keeps a joint from folding its two bones flat onto each other. Each frame: Verlet integration with gravity,
// the links relaxed, then the floor and walls (the points can't sink into the floor or pass through a wall).
// As the actor is drawn, its model matrix is inverted to bring the points into model space, and each limb's rotation
// is worked out from where its children's points are (one child: the bone turned onto it, keeping its twist; two or
// more: the whole frame from two of them); leaves keep the rotation they were thrown with.

#define RD_MAX_LIMBS 64
#define RD_MAX_LINKS 320
#define RD_GRAVITY 2.0f
#define RD_DAMPING 0.985f
#define RD_ITERATIONS 10
#define RD_RADIUS 4.0f
#define RD_FLOOR_FRICTION 0.55f
#define RD_BLEND_FRAMES 12

typedef struct {
    u8 a, b;
    u8 minOnly; // a joint's bend limit: only pushes apart
    f32 len;
} RdLink;

typedef struct {
    f32 m[3][3];
} RdMat;

static struct {
    Actor* actor;
    void** skeleton;
    s32 state;
    s32 pendingFrames;
    s32 count;
    u8 parent[RD_MAX_LIMBS];
    u8 order[RD_MAX_LIMBS]; // parents before children
    u8 shared[RD_MAX_LIMBS]; // a zero-length limb: on its parent's point
    Vec3f jp[RD_MAX_LIMBS];  // joint positions (model units)
    Vec3s local0[RD_MAX_LIMBS];
    RdMat W[RD_MAX_LIMBS]; // last model-space rotations
    Vec3f p[RD_MAX_LIMBS], q[RD_MAX_LIMBS];
    RdLink links[RD_MAX_LINKS];
    s32 linkCount;
    Vec3f vel;
    u8 landed;
    s32 blend;
    Vec3s pose[RD_MAX_LIMBS + 1];
    Vec3s out[RD_MAX_LIMBS + 1];
} sRd;

// ---- small 3x3 rotation helpers (v' = M v; the engine's joints rotate Z, then Y, then X: R = Rz Ry Rx) ----

static void Rd_Identity(RdMat* r) {
    memset(r, 0, sizeof(*r));
    r->m[0][0] = r->m[1][1] = r->m[2][2] = 1.0f;
}

static void Rd_Mul(RdMat* a, RdMat* b, RdMat* out) {
    RdMat t;
    for (s32 i = 0; i < 3; i++) {
        for (s32 j = 0; j < 3; j++) {
            t.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j] + a->m[i][2] * b->m[2][j];
        }
    }
    *out = t;
}

static void Rd_Transpose(RdMat* a, RdMat* out) {
    RdMat t;
    for (s32 i = 0; i < 3; i++) {
        for (s32 j = 0; j < 3; j++) {
            t.m[i][j] = a->m[j][i];
        }
    }
    *out = t;
}

static void Rd_Apply(RdMat* a, Vec3f* v, Vec3f* out) {
    Vec3f t;
    t.x = a->m[0][0] * v->x + a->m[0][1] * v->y + a->m[0][2] * v->z;
    t.y = a->m[1][0] * v->x + a->m[1][1] * v->y + a->m[1][2] * v->z;
    t.z = a->m[2][0] * v->x + a->m[2][1] * v->y + a->m[2][2] * v->z;
    *out = t;
}

static void Rd_FromEuler(Vec3s* r, RdMat* out) {
    f32 sx = Math_SinS(r->x), cx = Math_CosS(r->x);
    f32 sy = Math_SinS(r->y), cy = Math_CosS(r->y);
    f32 sz = Math_SinS(r->z), cz = Math_CosS(r->z);
    out->m[0][0] = cz * cy;
    out->m[0][1] = cz * sy * sx - sz * cx;
    out->m[0][2] = cz * sy * cx + sz * sx;
    out->m[1][0] = sz * cy;
    out->m[1][1] = sz * sy * sx + cz * cx;
    out->m[1][2] = sz * sy * cx - cz * sx;
    out->m[2][0] = -sy;
    out->m[2][1] = cy * sx;
    out->m[2][2] = cy * cx;
}

static void Rd_ToEuler(RdMat* r, Vec3s* out) {
    f32 s = CLAMP(-r->m[2][0], -1.0f, 1.0f);
    f32 y = asinf(s);
    f32 x, z;
    if (fabsf(s) < 0.9999f) {
        x = atan2f(r->m[2][1], r->m[2][2]);
        z = atan2f(r->m[1][0], r->m[0][0]);
    } else { // (straight up or down: the twist goes on X)
        x = atan2f(-r->m[1][2], r->m[1][1]);
        z = 0.0f;
    }
    out->x = (s16)(x * (32768.0f / M_PI));
    out->y = (s16)(y * (32768.0f / M_PI));
    out->z = (s16)(z * (32768.0f / M_PI));
}

static f32 Rd_Len(Vec3f* v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static void Rd_Norm(Vec3f* v) {
    f32 l = Rd_Len(v);
    if (l > 1e-6f) {
        v->x /= l;
        v->y /= l;
        v->z /= l;
    }
}

static void Rd_Cross(Vec3f* a, Vec3f* b, Vec3f* out) {
    Vec3f t = { a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x };
    *out = t;
}

static f32 Rd_Dot(Vec3f* a, Vec3f* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

// The rotation turning direction a onto direction b (both unit)
static void Rd_Between(Vec3f* a, Vec3f* b, RdMat* out) {
    Vec3f ax;
    f32 c = Rd_Dot(a, b);
    f32 s, t;

    Rd_Cross(a, b, &ax);
    s = Rd_Len(&ax);
    if (s < 1e-5f) {
        Rd_Identity(out);
        if (c < 0.0f) { // opposite: half a turn about any perpendicular
            Vec3f p = { 1.0f, 0.0f, 0.0f };
            if (fabsf(a->x) > 0.9f) {
                p.x = 0.0f;
                p.y = 1.0f;
            }
            Rd_Cross(a, &p, &ax);
            Rd_Norm(&ax);
            for (s32 i = 0; i < 3; i++) {
                for (s32 j = 0; j < 3; j++) {
                    f32 axv[3] = { ax.x, ax.y, ax.z };
                    out->m[i][j] = 2.0f * axv[i] * axv[j] - ((i == j) ? 1.0f : 0.0f);
                }
            }
        }
        return;
    }
    ax.x /= s;
    ax.y /= s;
    ax.z /= s;
    t = 1.0f - c;
    out->m[0][0] = c + ax.x * ax.x * t;
    out->m[0][1] = ax.x * ax.y * t - ax.z * s;
    out->m[0][2] = ax.x * ax.z * t + ax.y * s;
    out->m[1][0] = ax.y * ax.x * t + ax.z * s;
    out->m[1][1] = c + ax.y * ax.y * t;
    out->m[1][2] = ax.y * ax.z * t - ax.x * s;
    out->m[2][0] = ax.z * ax.x * t - ax.y * s;
    out->m[2][1] = ax.z * ax.y * t + ax.x * s;
    out->m[2][2] = c + ax.z * ax.z * t;
}

// An orthonormal frame from two directions (columns: e1 along a, e2 toward b, e3 = e1 x e2)
static s32 Rd_Frame(Vec3f* a, Vec3f* b, RdMat* out) {
    Vec3f e1 = *a, e2, e3;
    f32 d;

    Rd_Norm(&e1);
    d = Rd_Dot(b, &e1);
    e2.x = b->x - e1.x * d;
    e2.y = b->y - e1.y * d;
    e2.z = b->z - e1.z * d;
    if (Rd_Len(&e2) < 1e-4f) {
        return false;
    }
    Rd_Norm(&e2);
    Rd_Cross(&e1, &e2, &e3);
    out->m[0][0] = e1.x;
    out->m[1][0] = e1.y;
    out->m[2][0] = e1.z;
    out->m[0][1] = e2.x;
    out->m[1][1] = e2.y;
    out->m[2][1] = e2.z;
    out->m[0][2] = e3.x;
    out->m[1][2] = e3.y;
    out->m[2][2] = e3.z;
    return true;
}

static void Rd_MtxApply(MtxF* mf, Vec3f* v, Vec3f* out) {
    Vec3f t;
    t.x = mf->xx * v->x + mf->xy * v->y + mf->xz * v->z + mf->xw;
    t.y = mf->yx * v->x + mf->yy * v->y + mf->yz * v->z + mf->yw;
    t.z = mf->zx * v->x + mf->zy * v->y + mf->zz * v->z + mf->zw;
    *out = t;
}

// ---- building it ----

static void Rd_AddLink(s32 a, s32 b, f32 len, s32 minOnly) {
    if ((sRd.linkCount < RD_MAX_LINKS) && (a != b)) {
        RdLink* l = &sRd.links[sRd.linkCount++];
        l->a = a;
        l->b = b;
        l->len = len;
        l->minOnly = minOnly;
    }
}

static f32 Rd_Dist(Vec3f* a, Vec3f* b) {
    Vec3f d = { b->x - a->x, b->y - a->y, b->z - a->z };
    return Rd_Len(&d);
}

// The point a limb is on (zero-length limbs share their parent's)
static s32 Rd_Point(s32 i) {
    while ((i > 0) && sRd.shared[i]) {
        i = sRd.parent[i];
    }
    return i;
}

static s32 Rd_Build(PlayState* play, void** skeleton, Vec3s* jointTable) {
    s32 stack[RD_MAX_LIMBS];
    s32 sp = 0;
    s32 n = 0;
    MtxF model;
    RdMat Wm[RD_MAX_LIMBS];
    Vec3f O[RD_MAX_LIMBS]; // model-space origins

    // The hierarchy (child/sibling), parents first
    memset(sRd.parent, 0, sizeof(sRd.parent));
    stack[sp++] = 0;
    sRd.count = 0;
    while (sp > 0) {
        s32 i = stack[--sp];
        StandardLimb* limb;
        if ((i < 0) || (i >= RD_MAX_LIMBS)) {
            return false;
        }
        limb = SEGMENTED_TO_VIRTUAL(skeleton[i]);
        if (limb == NULL) {
            return false;
        }
        sRd.order[n++] = i;
        sRd.count = MAX(sRd.count, i + 1);
        sRd.jp[i].x = limb->jointPos.x;
        sRd.jp[i].y = limb->jointPos.y;
        sRd.jp[i].z = limb->jointPos.z;
        sRd.local0[i] = jointTable[i + 1];
        if (limb->sibling != LIMB_DONE) {
            if (sp >= RD_MAX_LIMBS) {
                return false;
            }
            sRd.parent[limb->sibling] = sRd.parent[i];
            stack[sp++] = limb->sibling;
        }
        if (limb->child != LIMB_DONE) {
            if (sp >= RD_MAX_LIMBS) {
                return false;
            }
            sRd.parent[limb->child] = i;
            stack[sp++] = limb->child;
        }
    }
    if ((n != sRd.count) || (n < 2)) {
        return false;
    }

    // Forward kinematics of the pose it was in (model space), then into the world with the model matrix
    Matrix_Get(&model);
    for (s32 k = 0; k < n; k++) {
        s32 i = sRd.order[k];
        RdMat L;
        Rd_FromEuler(&sRd.local0[i], &L);
        if (i == 0) {
            O[0].x = jointTable[0].x;
            O[0].y = jointTable[0].y;
            O[0].z = jointTable[0].z;
            Wm[0] = L;
        } else {
            s32 pa = sRd.parent[i];
            Vec3f off;
            Rd_Apply(&Wm[pa], &sRd.jp[i], &off);
            O[i].x = O[pa].x + off.x;
            O[i].y = O[pa].y + off.y;
            O[i].z = O[pa].z + off.z;
            Rd_Mul(&Wm[pa], &L, &Wm[i]);
        }
        sRd.W[i] = Wm[i];
        sRd.shared[i] = (i != 0) && (Rd_Len(&sRd.jp[i]) < 1.0f);
        Rd_MtxApply(&model, &O[i], &sRd.p[i]);
    }

    // Links: each bone; every limb's children held rigid with each other and with it; a bend limit over each joint
    sRd.linkCount = 0;
    for (s32 i = 1; i < n; i++) {
        s32 a = Rd_Point(sRd.parent[i]), b = Rd_Point(i);
        if (a != b) {
            Rd_AddLink(a, b, Rd_Dist(&sRd.p[a], &sRd.p[b]), false);
        }
    }
    for (s32 pa = 0; pa < n; pa++) {
        s32 kids[RD_MAX_LIMBS];
        s32 kc = 0;
        s32 a = Rd_Point(pa);
        if ((pa != 0) && sRd.shared[pa]) {
            continue; // (its point's owner gathers everything on it)
        }
        // the points hanging off this limb (through zero-length children too)
        for (s32 i = 1; i < n; i++) {
            s32 up = sRd.parent[i];
            if ((Rd_Point(up) == a) && (Rd_Point(i) != a)) {
                kids[kc++] = Rd_Point(i);
            }
        }
        for (s32 u = 0; u < kc; u++) {
            for (s32 v = u + 1; v < kc; v++) {
                if (kids[u] != kids[v]) {
                    Rd_AddLink(kids[u], kids[v], Rd_Dist(&sRd.p[kids[u]], &sRd.p[kids[v]]), false);
                }
            }
        }
        // bend limit: the grandparent's point and each child's point stay at least ~40% of the way apart
        if (pa != 0) {
            s32 g = Rd_Point(sRd.parent[pa]);
            if (g != a) {
                for (s32 u = 0; u < kc; u++) {
                    f32 l1 = Rd_Dist(&sRd.p[g], &sRd.p[a]), l2 = Rd_Dist(&sRd.p[a], &sRd.p[kids[u]]);
                    Rd_AddLink(g, kids[u], 0.4f * (l1 + l2), true);
                }
            }
        }
    }

    // Thrown: everything moving with the throw, and tumbling forward end over end
    {
        Vec3f c = sRd.p[0];
        Vec3f dir = { sRd.vel.x, 0.0f, sRd.vel.z };
        Vec3f up = { 0.0f, 1.0f, 0.0f };
        Vec3f axis;
        Rd_Norm(&dir);
        Rd_Cross(&up, &dir, &axis);
        for (s32 i = 0; i < n; i++) {
            Vec3f r = { sRd.p[i].x - c.x, sRd.p[i].y - c.y, sRd.p[i].z - c.z };
            Vec3f spin;
            Rd_Cross(&axis, &r, &spin);
            sRd.q[i].x = sRd.p[i].x - sRd.vel.x - spin.x * 0.12f;
            sRd.q[i].y = sRd.p[i].y - sRd.vel.y - spin.y * 0.12f;
            sRd.q[i].z = sRd.p[i].z - sRd.vel.z - spin.z * 0.12f;
        }
    }
    return true;
}

// ---- the physics ----

s32 GdfRagdoll_Step(PlayState* play) {
    s32 impact = false;

    if (sRd.state == 1) {
        if (++sRd.pendingFrames > 3) { // never drawn with a skeleton we can read: the old throw takes over
            GdfRagdoll_Cancel();
        }
        return false;
    }
    if (sRd.state != 2) {
        return false;
    }
    for (s32 i = 0; i < sRd.count; i++) {
        Vec3f v;
        if (sRd.shared[i]) {
            continue;
        }
        v.x = (sRd.p[i].x - sRd.q[i].x) * RD_DAMPING;
        v.y = (sRd.p[i].y - sRd.q[i].y) * RD_DAMPING;
        v.z = (sRd.p[i].z - sRd.q[i].z) * RD_DAMPING;
        sRd.q[i] = sRd.p[i];
        sRd.p[i].x += v.x;
        sRd.p[i].y += v.y - RD_GRAVITY;
        sRd.p[i].z += v.z;
    }
    for (s32 it = 0; it < RD_ITERATIONS; it++) {
        for (s32 k = 0; k < sRd.linkCount; k++) {
            RdLink* l = &sRd.links[k];
            Vec3f* a = &sRd.p[l->a];
            Vec3f* b = &sRd.p[l->b];
            Vec3f d = { b->x - a->x, b->y - a->y, b->z - a->z };
            f32 dist = Rd_Len(&d);
            f32 f;
            if ((dist < 1e-4f) || (l->minOnly && (dist >= l->len))) {
                continue;
            }
            f = (dist - l->len) / dist * 0.5f;
            a->x += d.x * f;
            a->y += d.y * f;
            a->z += d.z * f;
            b->x -= d.x * f;
            b->y -= d.y * f;
            b->z -= d.z * f;
        }
    }
    // The floor and walls
    for (s32 i = 0; i < sRd.count; i++) {
        CollisionPoly* poly;
        Vec3f probe, hit;
        s32 bgId;
        f32 floorY;
        if (sRd.shared[i]) {
            continue;
        }
        if (BgCheck_EntityLineTest1(&play->colCtx, &sRd.q[i], &sRd.p[i], &hit, &poly, true, false, true, true,
                                    &bgId)) {
            f32 nx = COLPOLY_GET_NORMAL(poly->normal.x), nz = COLPOLY_GET_NORMAL(poly->normal.z);
            sRd.p[i].x = hit.x + nx * RD_RADIUS;
            sRd.p[i].z = hit.z + nz * RD_RADIUS;
            sRd.q[i].x = sRd.p[i].x;
            sRd.q[i].z = sRd.p[i].z;
        }
        probe = sRd.p[i];
        probe.y += 40.0f;
        floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
        if ((floorY > BGCHECK_Y_MIN) && (sRd.p[i].y < floorY + RD_RADIUS)) {
            f32 vy = sRd.p[i].y - sRd.q[i].y;
            if ((vy < -7.0f) && !sRd.landed) {
                impact = true;
                sRd.landed = true;
            }
            sRd.p[i].y = floorY + RD_RADIUS;
            // a little bounce, and friction along the floor
            sRd.q[i].y = sRd.p[i].y + vy * 0.25f;
            sRd.q[i].x = sRd.p[i].x - (sRd.p[i].x - sRd.q[i].x) * RD_FLOOR_FRICTION;
            sRd.q[i].z = sRd.p[i].z - (sRd.p[i].z - sRd.q[i].z) * RD_FLOOR_FRICTION;
        }
    }
    for (s32 i = 1; i < sRd.count; i++) {
        if (sRd.shared[i]) {
            sRd.p[i] = sRd.p[Rd_Point(i)];
            sRd.q[i] = sRd.p[i];
        }
    }
    return impact;
}

f32 GdfRagdoll_MaxSpeed(void) {
    f32 m = 0.0f;
    if (sRd.state != 2) {
        return 0.0f;
    }
    for (s32 i = 0; i < sRd.count; i++) {
        Vec3f d = { sRd.p[i].x - sRd.q[i].x, sRd.p[i].y - sRd.q[i].y, sRd.p[i].z - sRd.q[i].z };
        m = MAX(m, Rd_Len(&d));
    }
    return m;
}

void GdfRagdoll_Anchor(PlayState* play, Vec3f* pos) {
    CollisionPoly* poly;
    Vec3f probe;
    f32 floorY;

    *pos = sRd.p[0];
    probe = *pos;
    probe.y += 40.0f;
    floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
    if ((floorY > BGCHECK_Y_MIN) && (floorY > pos->y - 200.0f)) {
        pos->y = floorY;
    }
}

void GdfRagdoll_Begin(Actor* actor, Vec3f* vel) {
    memset(&sRd, 0, sizeof(sRd));
    sRd.actor = actor;
    sRd.vel = *vel;
    sRd.state = 1;
}

s32 GdfRagdoll_State(Actor* actor) {
    return (actor != NULL && actor == sRd.actor) ? sRd.state : 0;
}

void GdfRagdoll_GetUp(void) {
    if (sRd.state == 2) {
        sRd.state = 3;
        sRd.blend = RD_BLEND_FRAMES;
    }
}

void GdfRagdoll_Cancel(void) {
    sRd.state = 0;
    sRd.actor = NULL;
}

// ---- drawing it ----

static void Rd_Solve(PlayState* play) {
    MtxF model, inv;
    Vec3f P[RD_MAX_LIMBS];
    RdMat Wpar[RD_MAX_LIMBS];

    Matrix_Get(&model);
    if (!SkinMatrix_Invert(&model, &inv)) {
        memcpy(sRd.out, sRd.pose, sizeof(sRd.out));
        return;
    }
    for (s32 i = 0; i < sRd.count; i++) {
        Rd_MtxApply(&inv, &sRd.p[i], &P[i]);
    }
    for (s32 k = 0; k < sRd.count; k++) {
        s32 i = sRd.order[k];
        RdMat parentW, W, L, T;
        Vec3f t[2], r[2];
        s32 kc = 0;

        if (i == 0) {
            Rd_Identity(&parentW);
        } else {
            parentW = Wpar[sRd.parent[i]];
        }
        // this limb's children that have a point of their own (two are enough)
        for (s32 c = 1; (c < sRd.count) && (kc < 2); c++) {
            if ((sRd.parent[c] == i) && !sRd.shared[c]) {
                Vec3f d = { P[c].x - P[Rd_Point(i)].x, P[c].y - P[Rd_Point(i)].y, P[c].z - P[Rd_Point(i)].z };
                if (Rd_Len(&d) > 1e-3f) {
                    t[kc] = d;
                    r[kc] = sRd.jp[c];
                    kc++;
                }
            }
        }
        if (kc == 0) {
            // a leaf (or only zero-length children): as it was thrown, on its parent
            Rd_FromEuler(&sRd.local0[i], &L);
            Rd_Mul(&parentW, &L, &W);
        } else if (kc == 1) {
            // its bone turned onto the child's point, keeping its twist
            Vec3f d, tt = t[0];
            RdMat Q;
            Rd_Apply(&sRd.W[i], &r[0], &d);
            Rd_Norm(&d);
            Rd_Norm(&tt);
            Rd_Between(&d, &tt, &Q);
            Rd_Mul(&Q, &sRd.W[i], &W);
        } else {
            RdMat Fr, Ft, FrT;
            if (Rd_Frame(&r[0], &r[1], &Fr) && Rd_Frame(&t[0], &t[1], &Ft)) {
                Rd_Transpose(&Fr, &FrT);
                Rd_Mul(&Ft, &FrT, &W);
            } else {
                W = sRd.W[i];
            }
        }
        sRd.W[i] = W;
        Wpar[i] = W;
        Rd_Transpose(&parentW, &T);
        Rd_Mul(&T, &W, &L);
        Rd_ToEuler(&L, &sRd.out[i + 1]);
    }
    sRd.out[0].x = (s16)CLAMP(P[0].x, -32000.0f, 32000.0f);
    sRd.out[0].y = (s16)CLAMP(P[0].y, -32000.0f, 32000.0f);
    sRd.out[0].z = (s16)CLAMP(P[0].z, -32000.0f, 32000.0f);
}

Vec3s* GdfRagdoll_Pose(PlayState* play, void** skeleton, Vec3s* jointTable, void* arg) {
    if ((sRd.state == 0) || (arg == NULL) || (arg != (void*)sRd.actor) || (skeleton == NULL) || (jointTable == NULL)) {
        return jointTable;
    }
    if (sRd.state == 1) {
        if (!Rd_Build(play, skeleton, jointTable)) {
            GdfRagdoll_Cancel();
            return jointTable;
        }
        sRd.skeleton = skeleton;
        sRd.state = 2;
    }
    if (skeleton != sRd.skeleton) { // (another skeleton the same actor draws: a second part, left alone)
        return jointTable;
    }
    if (sRd.state == 2) {
        Rd_Solve(play);
        memcpy(sRd.pose, sRd.out, sizeof(Vec3s) * (sRd.count + 1));
        return sRd.out;
    }
    // Getting up: from the pose it lay in back to its own animation
    {
        f32 w = (f32)sRd.blend / RD_BLEND_FRAMES;
        for (s32 i = 0; i <= sRd.count; i++) {
            Vec3s* a = &sRd.pose[i];
            Vec3s* b = &jointTable[i];
            if (i == 0) {
                sRd.out[0] = *b; // (its root's own place)
                continue;
            }
            sRd.out[i].x = b->x + (s16)((s16)(a->x - b->x) * w);
            sRd.out[i].y = b->y + (s16)((s16)(a->y - b->y) * w);
            sRd.out[i].z = b->z + (s16)((s16)(a->z - b->z) * w);
        }
        if (--sRd.blend <= 0) {
            GdfRagdoll_Cancel();
        }
        return sRd.out;
    }
}
