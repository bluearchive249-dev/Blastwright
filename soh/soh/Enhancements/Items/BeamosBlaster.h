#ifndef BEAMOS_BLASTER_H
#define BEAMOS_BLASTER_H

#include <libultraship/libultra.h>
#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Beamos Blaster
 *
 * When enabled, the three sword item actions (Kokiri / Master / Biggoron) are re-skinned as three Beamos-style
 * guns (Mk.I weak, Mk.II strong, Mk.III strongest). Link holds them with the bow stance, so they can be aimed in
 * first person exactly like a bow (hold B, release to fire) or hip-fired with a quick tap of B.
 */

// Master toggle (CVAR gEnhancements.BeamosBlaster).
s32 BeamosBlaster_IsEnabled(void);

// True when `itemAction` is one of the sword item actions AND the mod is enabled, i.e. Link is holding a gun.
s32 BeamosBlaster_IsGunAction(s32 itemAction);

// True while the player has held B long enough to count as "aiming" (instead of a quick hip-fire tap).
s32 BeamosBlaster_WantsAim(void);

// True once after a quick tap of B was detected. The player's idle upper action consumes it and hip-fires.
s32 BeamosBlaster_ConsumeHipFire(void);

// Spawns one beam from Link's gun. Uses the first-person aim angles when aiming, else his Z-target / facing.
void BeamosBlaster_Fire(PlayState* play, Player* player);

// Draws the gun model in Link's hand. Called from both hand limbs' post-draw; `rightHand` says which limb is being
// drawn. The gun sits in the left hand (where swords live) normally and moves to the aiming hand while posed.
void BeamosBlaster_DrawInHand(PlayState* play, Player* player, s32 rightHand);

// Standing "gripping the gun" stance after a hip-fire tap. Start it when firing; it runs out by itself.
void BeamosBlaster_StartStance(void);
s32 BeamosBlaster_StanceActive(void);

// Called for every limb while drawing Link: poses the arms (two-handed grip, recoil) while a gun is held ready.
void BeamosBlaster_AdjustLimb(PlayState* play, Player* player, s32 limbIndex, Vec3s* rot);

// True when the first-person arms should hold the gun (so the bow / slingshot arm is not drawn).
s32 BeamosBlaster_UseGunArms(Player* player);

#ifdef __cplusplus
}
#endif

#endif // BEAMOS_BLASTER_H
