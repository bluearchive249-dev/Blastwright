#pragma once

// SoH: "Play as Ganondorf", experiment: new files start at the Gerudo Fortress (GanondorfStart.cpp)
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// This file was started at the Gerudo Fortress
int32_t GanondorfStart_IsGerudoFile(void);

// Called when a new file is made (Sram_InitSave): with the option on, it starts at the fortress, without the opening
void GanondorfStart_InitNewSave(void);

// Where a file continues when it would continue at Link's house: the fortress for a Gerudo file
int16_t GanondorfStart_GetContinueEntrance(int16_t entrance);

#ifdef __cplusplus
}
#endif
