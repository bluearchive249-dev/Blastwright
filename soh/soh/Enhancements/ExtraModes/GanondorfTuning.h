#pragma once

// Copyright © 2026 Grimsfrost. All rights reserved.
// Play as Ganondorf: look tuning read at runtime from ganondorf_tuning.json in the game's folder (or mods/), so sizes,
// colors and speeds can be changed without rebuilding. Reloaded when the file changes. A key that isn't in the file
// keeps its built-in default. Every key asked for is listed, with the value in use, in ganondorf_tuning_keys.json.

#ifdef __cplusplus
extern "C" {
#endif

// A number: "key": 1.5
float GdfTune(const char* key, float def);
// One entry of a list: "key": [255, 70, 0] (a color, an offset)
float GdfTuneAt(const char* key, int index, float def);

#ifdef __cplusplus
}
#endif
