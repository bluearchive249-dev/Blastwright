#pragma once

// SoH: "Play as Ganondorf", ragdolls for the ones he throws (D-Pad Down's throat grab). Built on the spot from any
// character's own skeleton (its limbs, their joints and lengths): a point at each joint, rigid links for the bones,
// gravity, the floor and walls, and the limbs' rotations worked back out of the points as the character is drawn.

#include <libultraship/libultra.h>
#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Thrown: the ragdoll is built the next time the actor is drawn (with its velocity and a tumble)
void GdfRagdoll_Begin(Actor* actor, Vec3f* vel);
// 0: none, 1: waiting to be built, 2: flying or lying, 3: getting up (blending back to its own animation)
s32 GdfRagdoll_State(Actor* actor);
// One frame of physics (from Ganondorf's update); returns true on the first hard landing (for the slam)
s32 GdfRagdoll_Step(PlayState* play);
// Where the actor stands meanwhile (under its root joint, on the floor) and how fast its joints still move
void GdfRagdoll_Anchor(PlayState* play, Vec3f* pos);
f32 GdfRagdoll_MaxSpeed(void);
// Up again: its own animation takes over over a few frames
void GdfRagdoll_GetUp(void);
void GdfRagdoll_Cancel(void);

// Several at once: slot 0 is the one above (the throw); the fists' launches use slots 1 and up
#define GDF_RAGDOLL_SLOTS 5
void GdfRagdoll_BeginSlot(s32 slot, Actor* actor, Vec3f* vel);
s32 GdfRagdoll_StateSlot(s32 slot, Actor* actor);
s32 GdfRagdoll_StepSlot(PlayState* play, s32 slot);
void GdfRagdoll_AnchorSlot(PlayState* play, s32 slot, Vec3f* pos);
f32 GdfRagdoll_MaxSpeedSlot(s32 slot);
void GdfRagdoll_GetUpSlot(s32 slot);
void GdfRagdoll_CancelSlot(s32 slot);

// From the skeleton draws (z_skelanime.c): the joint table to draw this skeleton with
Vec3s* GdfRagdoll_Pose(PlayState* play, void** skeleton, Vec3s* jointTable, void* arg);

#ifdef __cplusplus
}
#endif
