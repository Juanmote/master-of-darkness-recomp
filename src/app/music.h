// Optional music pack: a .zip with Ogg Vorbis files that replace the game's tracks.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { int id; const char* file; const char* name; bool loops; } music_track_t;
extern const music_track_t music_tracks[];
extern const int music_track_count;

bool music_load_pack(const char* zip_path);   // false: no pack or no usable files
void music_unload(void);
bool music_pack_loaded(void);
int music_pack_tracks(void);                   // number of tracks the pack replaces
bool music_has(int id);
void music_play(int id);
void music_stop(void);
void music_fade(void);                         // fade out over about one second
void music_set_paused(bool paused);
// Adds the music to `out` (n mono samples at 44100 Hz), scaled by volume (0..10).
void music_mix(int16_t* out, int n, int volume);
