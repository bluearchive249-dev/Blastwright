# Link wears Ganondorf's cape and hood (SoH 9.3.1 "Dewey Bravo")

This mod changes the game's code, so it can't go in the `mods` folder like a texture or model pack.
GitHub Actions builds it in your fork and produces a new `soh.exe`.

Based on Shipwright `develop` at 2827fc2a0 (the 9.3.1 release merge).

## What's in this folder

- `LinkGanonCape.patch` has two commits: the cape, then the hood. Use it with `git am`.
- `files/` has the same changes as complete files, laid out at their repo paths. Use these if
  you'd rather upload through the GitHub website.
- `hood-preview.png` is a render of the hood on Link's real head meshes from `oot.o2r`. The
  shoulders and cape in it are rough stand-ins; in game they come from Link's body and the real
  cape.
- `tools/` has the Python scripts that measured the head meshes and baked the hood shape. You only
  need them to refit the hood, for example to a custom Link model.

Changed files:

| File | Change |
|---|---|
| `soh/soh/Enhancements/Graphics/LinkGanonCape.cpp` / `.h` | New. Spawns the cape on Link, attaches it to his shoulders and scales Ganondorf's physics values to Link's size. Also builds the hood and hides the cap. |
| `soh/src/overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.c` / `.h` | Adds a "Link mode" to Ganondorf's cape actor: shorter segments, a velocity clamp, a reset when Link teleports, and higher-precision vertices. It also draws the hood right after the cape. Ganondorf's own cape behaves exactly as before. |
| `soh/src/code/z_player_lib.c` | Adds three small calls. They record Link's shoulder and head positions each time he's drawn, hide or recolor the cap while the hood is on, and remember the tunic color so it can be restored. |
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
| Hood | `gEnhancements.GanonCape.Hood` | on |
| Hood Size | `gEnhancements.GanonCape.HoodSize` | 1.0 |

Child Link automatically uses 0.64x the length and offset, and gets his own hood shape.

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

### The hood

- The hood has no physics. It's a 12×12 grid drawn with the cape's own display list
  (`gMantDL`), so its texture, material and lighting are exactly the cape's.
- **No gap:** each hood column lines up with one cape strand. The hood's bottom row is the
  cape's 12 strand roots, which are also the cape's top row of vertices, so the two meshes share
  their edge exactly. The texture coordinates continue from the cape upward, so the fabric
  pattern flows across the seam, and the seam uses the cape's own normals so the shading matches.
- The top 9 rows are rigid on Link's head and turn with it. The last 3 rows hang from the bottom
  of the head down to the cape's collar.
- The head shape is a baked table. It was shrink-wrapped around the vanilla adult and child head
  meshes with a small margin and smoothed. A ray test from the head's center found no head
  vertex outside the hood, so the cap dome and ears stay inside. The bangs come out under the
  brim, which overhangs the forehead a little.
- **The cap:** Link's cap is two parts. The floppy tail is its own limb, and it's hidden. The
  dome is built into the head mesh and tinted with the tunic color. While the hood is on, the
  dome is tinted with the hair color instead (the Hair cosmetic color if you've set one). The
  tunic color is restored right after the head, because the cap limb is drawn next.
- The hood was fitted to the vanilla Link model. A custom Link model (ZeroMod and the like)
  will probably clip until it's refitted with the scripts in `tools/`.

## Status

The new and changed files passed a syntax check against the 9.3.1 headers. The hood shape was
checked against the real head meshes in `oot.o2r`, but none of it has been played yet; your first
fork build will be the first real test.

Things most likely to need tweaking:
- **Cape physics:** the calls in `LinkGanonCape_PrepareDraw` that set `gravity`, `backPush`, the
  two sway values and `minDist`.
- **Hood fit:** the Hood Size slider in game. The drape's outward bow is `sHoodDrapeBulge`.
- **Animation clipping:** the hood follows the head, but extreme poses (looking far up, some
  item animations) may push the neck or collar through the drape rows.
