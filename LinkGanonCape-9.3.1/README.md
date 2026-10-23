# Link wears Ganondorf's cape (SoH 9.3.1 "Dewey Bravo")

This mod changes the game's code, so it can't go in the `mods` folder like a texture or model pack.
GitHub Actions builds it in your fork and produces a new `soh.exe`.

Based on Shipwright `develop` at 2827fc2a0 (the 9.3.1 release merge).

## What's in this folder

- `LinkGanonCape.patch` is a single commit. Use it with `git am`.
- `files/` has the same changes as complete files, laid out at their repo paths. Use these if
  you'd rather upload through the GitHub website.

Changed files:

| File | Change |
|---|---|
| `soh/soh/Enhancements/Graphics/LinkGanonCape.cpp` / `.h` | New. Spawns the cape on Link, attaches it to his shoulders and scales Ganondorf's physics values to Link's size. |
| `soh/src/overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.c` / `.h` | Adds a "Link mode" to Ganondorf's cape actor: shorter segments, a velocity clamp, a reset when Link teleports, and higher-precision vertices. Ganondorf's own cape behaves exactly as before. |
| `soh/src/code/z_player_lib.c` | Adds one call that records Link's shoulder positions each time he's drawn. |
| `soh/soh/SohGui/SohMenuEnhancements.cpp` | Adds menu options. |

## Building in your fork

### Option A: git (recommended)

```sh
git clone https://github.com/<you>/Shipwright.git
cd Shipwright
git fetch https://github.com/HarbourMasters/Shipwright.git develop
git checkout -b link-ganon-cape FETCH_HEAD
git am -3 path/to/LinkGanonCape.patch
git push -u origin link-ganon-cape
```

### Option B: GitHub website only

1. On your fork's page, click **Sync fork** so it matches upstream `develop`.
2. Create a branch, for example `link-ganon-cape`.
3. For each file under `files/`, open the matching folder in your fork and use
   **Add file > Upload files**. This overwrites the four existing files and adds the two new
   `LinkGanonCape` files to `soh/soh/Enhancements/Graphics/`. Commit to your branch.

### Get the build

1. In your fork, open the **Actions** tab. The first time, GitHub asks you to enable workflows
   on the fork; accept.
2. Pushing the branch starts the **generate-builds** workflow. The Windows job can take half an
   hour or more.
3. When it finishes, open the run and download the **soh-windows** artifact.

## Installing

1. Back up `C:\Spel\soh\soh.exe` and `C:\Spel\soh\soh.o2r`.
2. From the artifact, copy `soh.exe` and `soh.o2r` into `C:\Spel\soh`. Leave `oot.o2r`,
   `Save\` and `shipofharkinian.json` alone.
3. Start the game. Open the menu (F1), go to **Enhancements > Graphics > Ganondorf's Cape**, and
   tick **Link Wears Ganondorf's Cape**.

## Settings

| Option | CVar | Default |
|---|---|---|
| Link Wears Ganondorf's Cape | `gEnhancements.GanonCape.Enabled` | off |
| Cape on Child Link | `gEnhancements.GanonCape.Child` | on |
| Cape Length (per segment, Ganondorf's is 9.5) | `gEnhancements.GanonCape.Length` | 3.5 |
| Cape Width (relative to the shoulders) | `gEnhancements.GanonCape.Width` | 1.15 |
| Cape Back Offset | `gEnhancements.GanonCape.BackOffset` | 1.5 |

Child Link automatically uses 0.64x the length and offset.

## How it works

- The cape is Ganondorf's own `EnGanonMant` actor: 12 strands × 12 joints of cloth physics. Its
  graphics are in `gameplay_keep`, so it can be spawned anywhere.
- It's spawned in `ACTORCAT_MISC`, which draws after the player, so it can use Link's shoulder
  positions from the same frame.
- It isn't spawned as Link's child actor, because Link treats `actor.child` as whatever he's
  holding.
- Gravity, back push, sway and the body push-out radius use Ganondorf's values, scaled by
  `segment length / 9.5`.
- Vertices are stored relative to the shoulders at 16× precision and drawn with a
  translate + 1/16 scale, so the small cape doesn't jitter. This also lets the 60 fps
  interpolation move the cape smoothly with Link.
- The cape is hidden in first person and whenever Link isn't drawn. It snaps back into place
  after warps or voiding out.
- Only one cape can exist at a time because the texture mask and vertex buffers are shared.
  Link's cape disappears while Ganondorf is loaded, which is the Ganon's Tower fight.

## Status

The new and changed files passed a syntax check against the 9.3.1 headers. It hasn't been
played yet; your first fork build will be the first real test. The physics values most likely
to need tweaking are the five calls in `LinkGanonCape_PrepareDraw` that set `gravity`,
`backPush`, the two sway values and `minDist`.
