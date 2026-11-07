// Start screen music (original, synthesized): see launcher_music.c
#pragma once
#include <stdint.h>

void lmusic_reset(void);                          // back to the beginning (with its fade-in)
void lmusic_mix(int16_t* out, int n, int volume);  // adds the next n samples (44100 Hz mono), volume 0-10
