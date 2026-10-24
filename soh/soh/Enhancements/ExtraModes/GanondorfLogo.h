#pragma once

// "Play as Ganondorf": the title screen logo, read at runtime from "gdf_logo.bin" next to the game. The logo isn't part
// of this repository. gdf_logo.bin: "GLOG", u16 width, u16 height (little endian), then width * height RGBA8888 pixels.
#ifdef __cplusplus
extern "C" {
#endif

// The logo's pixels (RGBA32) and size, or NULL when the option is off or the file is missing
const unsigned char* GanondorfLogo_Get(unsigned int* width, unsigned int* height);

#ifdef __cplusplus
}
#endif
