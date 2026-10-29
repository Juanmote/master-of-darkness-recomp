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
  uint8_t keep;   // object was inside the original picture: draw it even beyond the area limits
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
  // per spawn list; edge_*: spawn values already checked (the visible window edges)
  struct { int next, last, step; bool active; uint16_t list_base;
           int edge_r, edge_l, edge_cam; bool edge_ok_r, edge_ok_l; } spawn[2];
  uint8_t seen_in[46];       // object slot has been inside the original picture
  uint8_t hud_pending;       // MOD_HUD_* parts of the status bar to redraw (done by the game's own code)
  // enemy respawn option and the enemies killed that must not come back yet
  int respawn;               // MOD_RESPAWN_*
  uint32_t slot_entry[46];   // spawn-list entry that created the object in each slot (0 = none)
  int slot_trigger[46];      // camera value of that entry
  uint8_t slot_active[46], slot_culled[46];
  int16_t slot_x[46];
  struct { uint32_t id; int trigger; } killed[64];
  int n_killed;
  uint32_t kill_key;         // section the list belongs to
  int last_lives;
  // difficulty
  int difficulty;            // MOD_DIFF_*
  int frac_taken, frac_dealt;
  int saved_dmg_addr; uint8_t saved_dmg;
  // music pack: the game's sound driver asks before starting a track
  bool (*music_has)(int id);   // set by the app: a replacement exists for this track
  int music_events[8];         // MOD_MUSIC_* | id, read by the app with mod_music_poll
  int n_music_events;
  // double jump
  bool double_jump;
  bool air_jump_used;
  int prev_state;
  int margin_l, margin_r;    // playable pixels visible beyond the original picture (per side)
} mod_enhance_t;

#define MOD_HUD_WEAPON 1
#define MOD_HUD_SUBWEAPON 2
#define MOD_HUD_LIVES 4

enum { MOD_RESPAWN_ON, MOD_RESPAWN_OFF, MOD_RESPAWN_MIXED };
enum { MOD_DIFF_EASY, MOD_DIFF_NORMAL, MOD_DIFF_HARD };
// music events: MOD_MUSIC_PLAY | id (replacement takes over), MOD_MUSIC_STOP [| id of an original track],
// MOD_MUSIC_FADE (the game fades the music out)
#define MOD_MUSIC_PLAY 0x100
#define MOD_MUSIC_STOP 0x200
#define MOD_MUSIC_FADE 0x300
int mod_music_poll(mod_enhance_t* e);           // next event or 0
bool mod_music_paused(const sms_t* s);          // the game has paused its sound (pause screen)
void mod_music_request(sms_t* s, int id);       // ask the game to play a track (music test)

bool mod_enhance_compatible(const sms_t* s);
// Asks the game to redraw parts of the status bar (after a cheat changed weapons or lives).
void mod_request_hud(mod_enhance_t* e, int parts);
void mod_enhance_attach(sms_t* s, mod_enhance_t* e);
void mod_enhance_set_wide(mod_enhance_t* e, bool wide, int ext);
// Call once per frame after sms_run_frame: selects e->shown / e->n_shown.
void mod_enhance_frame(const sms_t* s, mod_enhance_t* e);

#ifdef __cplusplus
}
#endif
