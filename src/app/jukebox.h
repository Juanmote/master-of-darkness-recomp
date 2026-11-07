// The game's own music outside the game: see jukebox.c
#pragma once
#include <stdbool.h>
#include <stdint.h>

void jukebox_play(const uint8_t* rom, int rom_size, int id);   // starts that track (sound driver id 81-8F)
void jukebox_stop(void);
bool jukebox_active(void);
void jukebox_mix(int16_t* out, int n, int volume);   // one frame (SMS_SAMPLES_PER_FRAME samples) added to out
