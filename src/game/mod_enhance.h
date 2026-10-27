// Master of Darkness gameplay enhancements for the widescreen mode.
// Our own code, attached to the original game at a few addresses (see docs).
#pragma once
#include "../core/sms.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOD_MAX_PIECES 256

typedef struct {
  int16_t x;      // screen X relative to the original 256-pixel picture (can be <0 or >255)
  int16_t y;      // sprite Y as written to the SAT (picture line = y + 1)
  uint8_t tile;
  uint8_t in_sat; // the original game also sent this piece to the VDP
} mod_piece_t;

typedef struct {
  bool wide;                 // enhancements active (16:9 mode)
  int ext;                   // extra pixels per side
  // sprite pieces captured during the game's sprite pass
  mod_piece_t building[MOD_MAX_PIECES];
  int n_building;
  // last completed passes (ring), the renderer shows the one the VDP is displaying
  mod_piece_t pass[4][MOD_MAX_PIECES];
  int n_pass[4];
  int pass_head;           // index of the newest completed pass
  int last_age;            // age chosen last frame (used when nothing can be matched)
  uint32_t passes;          // sprite passes so far (to measure slowdown)
  const mod_piece_t* shown;
  int n_shown;
  // early spawning (one state per spawn list)
  struct { int last_cam[2]; int done_hi, done_lo; int next, last, step; bool active; uint16_t list_base; } spawn[2];
  int margin_l, margin_r;    // playable pixels visible beyond the original picture (per side)
} mod_enhance_t;

bool mod_enhance_compatible(const sms_t* s);
void mod_enhance_attach(sms_t* s, mod_enhance_t* e);
void mod_enhance_set_wide(mod_enhance_t* e, bool wide, int ext);
// Call once per frame after sms_run_frame: selects e->shown / e->n_shown.
void mod_enhance_frame(const sms_t* s, mod_enhance_t* e);

#ifdef __cplusplus
}
#endif
