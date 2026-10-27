// Master of Darkness specific video output: 4:3 or widescreen with side areas
// rebuilt from the level map that the game keeps in RAM.
#pragma once
#include "../core/sms.h"
#include "mod_enhance.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOD_WIDE_EXT 43   // extra pixels per side for 16:9 (256 + 86 = 342 ~ 192*16/9)

typedef struct {
  bool map_valid;     // game area matched the RAM map this frame
  int cam;            // camera X used for the extension
  int left_limit, right_limit;
  int cx;             // x of the original picture inside the playfield lines
} mod_wide_info_t;

// Writes SMS_H lines of (SMS_W + 2*ext) ARGB pixels into out (pitch in pixels).
// ext = 0 gives the original 4:3 picture.
// True while the level is on screen (the scrolling playfield matches the RAM map).
bool mod_is_gameplay(const sms_t* s);

// How many pixels of playable area the wide picture shows beyond the original 256
// on each side right now (depends on where the view is held at section edges).
bool mod_visible_margins(const sms_t* s, int ext, int* left_px, int* right_px);

void mod_render(const sms_t* s, uint32_t* out, int pitch, int ext, mod_wide_info_t* info,
                const mod_piece_t* pieces, int n_pieces);

#ifdef __cplusplus
}
#endif
