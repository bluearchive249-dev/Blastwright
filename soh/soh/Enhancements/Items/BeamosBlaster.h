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

// Draws the gun model (and aiming laser sight) in Link's hand (left hand limb, where swords live). Called from the limb post-draw.
void BeamosBlaster_DrawInHand(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif

#endif // BEAMOS_BLASTER_H
