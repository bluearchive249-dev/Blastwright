// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
#pragma once
// SoH: "Play as Ganondorf", the final battle with the roles swapped: at the top of Ganon's Tower the Hero waits in the
// tower Ganondorf's place. He's Dark Link's actor (ovl_En_Torch2: a second Player driven by a made-up controller) in
// Link's own look, at full strength, with his whole kit. See GanondorfHeroLink.cpp.

#define GDF_HERO_PARAMS 0x4C48 // ACTOR_EN_TORCH2 with these params: the Hero

#ifdef __cplusplus
extern "C" {
#endif

// (include after z64.h)
s32 GdfHero_Is(Actor* actor);
// ovl_En_Torch2's calls, for the Hero only
void GdfHero_Init(Player* hero, PlayState* play);
s32 GdfHero_Engages(Player* hero, PlayState* play); // he's seen Ganondorf: the fight starts
void GdfHero_Brain(Player* hero, PlayState* play, Input* input, u8 attacking); // before his update
s32 GdfHero_TakeHit(Player* hero, PlayState* play); // before his damage: false when it's ignored
s32 GdfHero_Fallen(Player* hero, PlayState* play);  // beaten: true while he lies there (no fading away)
void GdfHero_Draw(Player* hero, PlayState* play);
u16 GdfHero_Music(void);
// Din's Fire and his elemental arrows: the spell is his, not the player's (it follows him, hurts Ganondorf, and
// leaves the player's magic meter alone)
s32 GdfHero_OwnsMagic(Actor* magicParent);
u8 GdfHero_DinDamage(void);

#ifdef __cplusplus
}
#endif
