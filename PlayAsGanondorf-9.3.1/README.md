# Play as Ganondorf (SoH 9.3.1 "Dewey Bravo")

Replaces Link with Ganondorf from the Ganon's Tower fight, with his boss moveset instead of Link's.
This is the boss actor itself (`BossGanon`) with a new player-controlled mode, so the model, animations,
cloth cape, energy balls, Big Magic, lightning and shockwave are all the real ones from the fight.

This changes the game's code, so it can't go in the `mods` folder. Build it in your fork with GitHub
Actions, the same way as the cape mod.

Based on Shipwright `develop` at 2827fc2a0 (the 9.3.1 release merge).

## Controls

| Button | Move |
|---|---|
| B | Charge a light ball and throw it at your lock-on target, the nearest enemy in front, or straight ahead |
| Hold B | Keep holding through the charge to switch to **Big Magic**: five balls that fan out and home in on enemies |
| R | Cape block. Link can't be hurt or knocked back while it's up |
| R + B | Pound the floor: Ganondorf rises, slams down and sends out a shockwave |
| C-Left / C-Down / C-Right | Light ball / Pound / Big Magic |
| A | Still opens doors, talks, climbs, grabs, dives. No rolls, side hops or jump attacks |
| Z, C-Up, stick | Unchanged |

Ganondorf stays in place while he attacks, like in the fight. Getting hit plays his damage animation
with a shock flash, and he collapses when Link dies (and gets back up if a fairy saves you).

## Damage

Every attack carries **every damage type in the game** as hidden data. In game you only see his dark orbs,
Big Magic and shockwave, but the target reacts as if it was hit by whatever works on it:

- Rocks, boulders, bombable walls and floors break (explosive and hammer flags).
- Beamos, Bubbles, fake doors, the cracked walls in the Fire Temple and the stairs in Dodongo's Cavern
  react, because his attacks count as explosions for those "bombs only" checks too.
- Switches and targets for arrows, light, fire, hookshot, slingshot and so on react.
- Enemies take the strongest plain hit their damage table allows. Fire, ice and light reactions (burning,
  freezing, sparkles) are only used when that's the only way to hurt the enemy, so they stay invisible
  otherwise.

The one type left out is "blocked by a shield", because enemies treat that flag as a harmless shield
push and would ignore the hit.

Damage-table enemies use their own numbers. Anything without a table takes 4 (light ball), 2 per Big Magic
ball, or 8 (shockwave, hits twice as it spreads).

## Settings

Enhancements > Extra Modes:

| Option | CVar | Default |
|---|---|---|
| Play as Ganondorf | `gEnhancements.PlayAsGanondorf.Enabled` | off |
| Keep C-Button Items | `gEnhancements.PlayAsGanondorf.KeepItems` | off |
| Big Magic | `gEnhancements.PlayAsGanondorf.BigMagic` | on |
| Show Link in Cutscenes | `gEnhancements.PlayAsGanondorf.ShowLinkInCutscenes` | on |
| Ganondorf Hover Height | `gEnhancements.PlayAsGanondorf.HoverHeight` | 70 (150 in his fight) |

**Keep C-Button Items** gives the C buttons back to Link's items (Ocarina, Hookshot, Bombs, ...). Without it
there's no Ocarina, so turn it on if you want to finish the game. The C-button attacks are off then; use B,
hold B and R + B.

## Things to know

- Link plays the real Ganondorf and Ganon fights. The real boss uses the same cape and effect buffers,
  so Ganondorf can't be in the room twice.
- Ganondorf is hidden and Link is shown in first person, in crawlspaces, and during story cutscenes
  (the last one can be turned off).
- Link's collision is still Link-sized, so enemies have to reach Link's feet to hit you.
- Things that need a real bomb actor, rather than a hit (feeding King Dodongo, lighting bomb flowers),
  still need actual bombs. Use **Keep C-Button Items** for those.
- Link's voice clips still play when he gets hurt.
- Works together with the cape mod. Link's cape disappears while Ganondorf is out, since Ganondorf brings his own.

## What's in this folder

- `PlayAsGanondorf.patch` is a single commit. Use it with `git am`.
- `files/` has the same changes as complete files at their repo paths.

| File | Change |
|---|---|
| `soh/src/overlays/actors/ovl_Boss_Ganon/z_boss_ganon.c` / `.h` | Player-controlled Ganondorf: follows Link, reads the controller, his attacks as new action functions modelled on the vanilla ones, player-side light ball and Big Magic projectiles. Effects and shadows use the floor under Link instead of the arena floor. The vanilla fight is unchanged. |
| `soh/soh/Enhancements/ExtraModes/PlayAsGanondorf.cpp` / `.h` | New. Spawns and removes Ganondorf, hides Link and his shadow, and removes Link's moves from his input. |
| `soh/src/overlays/actors/ovl_player_actor/z_player.c` | Two calls: filter Link's input, and skip drawing Link. |
| `soh/src/code/z_collision_check.c` | Picks the best damage-table entry for Ganondorf's all-types hits. |
| `soh/src/code/z_actor.c` | His attacks count as explosions for bomb-only targets. |
| `soh/soh/SohGui/SohMenuEnhancements.cpp` | Menu options. |

## Building in your fork

### Option A: git (recommended, and needed if you also want the cape mod)

```sh
git clone https://github.com/<you>/Shipwright.git
cd Shipwright
git fetch https://github.com/HarbourMasters/Shipwright.git develop
git checkout -b play-as-ganondorf 2827fc2a0
git am -3 path/to/PlayAsGanondorf.patch
# optional, the cape mod on top (applies cleanly in either order):
git am -3 path/to/LinkGanonCape.patch
git push -u origin play-as-ganondorf
```

### Option B: GitHub website only

1. Create a branch in your fork from `develop` at 2827fc2a0 (or Sync fork, then branch).
2. Upload each file under `files/` to the same folder in your fork (**Add file > Upload files**).
3. Don't combine this way with the cape mod: both change `SohMenuEnhancements.cpp`, and uploading one
   overwrites the other. Use git for that.

### Get the build

1. Open the **Actions** tab in your fork (enable workflows the first time).
2. The push starts **generate-builds**. The Windows job can take half an hour or more.
3. Download the **soh-windows** artifact from the finished run.

## Installing

1. Back up `C:\Spel\soh\soh.exe` and `C:\Spel\soh\soh.o2r`.
2. Copy `soh.exe` and `soh.o2r` from the artifact into `C:\Spel\soh`. Leave `oot.o2r`, `Save\` and
   `shipofharkinian.json` alone.
3. Start the game, open the menu (Esc), go to **Enhancements > Extra Modes**, and tick **Play as Ganondorf**.

## Status

All changed files compile cleanly against the 9.3.1 headers (C files fully; the menu file apart from two
vanilla lines that need defines from the real build). It hasn't been played yet, so your first fork build
is the first real test. If something feels off, these are the easiest values to tweak, all in
`z_boss_ganon.c`:

- Light ball speed and lifetime: `speedXZ = 14.0f`, `timers[2] = 90` in `BossGanon_PlayerProjectile_Init`
- Big Magic charge length: `timers[0] = 60` in `BossGanon_Player_ChargeBigMagic`
- How long B must be held for Big Magic: `playerHeldTimer >= 20` in `BossGanon_Player_ChargeLightBall`
- Auto-aim range and cone: `700.0f, 0x2000` in `BossGanon_Player_UpdateAim`
- Shockwave radius: `130 : 280` in `BossGanon_Player_Pound`
