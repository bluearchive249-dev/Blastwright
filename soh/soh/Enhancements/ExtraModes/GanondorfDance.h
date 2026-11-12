#pragma once

// Play as Ganondorf: the dance emote and its song. The song is a .wav or .ogg the player puts in the ganondorf_songs folder
// (it isn't shipped); it's mixed into the game's sound output while he dances.

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

// The menu asked for the dance (taken once, by him, when he's free to start it)
void GanondorfDance_Request(void);
int32_t GanondorfDance_TakeRequest(void);
// The song from the top (looping), with the area music faded down under it; and stopped, the area music back
void GanondorfDance_Start(void);
void GanondorfDance_Stop(void);
// Seconds into the song as it's heard, or -1 without one (no song file, or still loading)
float GanondorfDance_Time(void);
// He struts this frame (the "Strut Walk" at walking pace): strut.wav (or .ogg) plays on from where it last stopped, and fades out
// once the calls stop
void GanondorfStrut_Strutting(void);
// The audio thread: the song added into the game's mix (32 kHz stereo)
void GanondorfDance_Mix(int16_t* samples, uint32_t frames);

#ifdef __cplusplus
}
#endif
