// Master of Darkness gameplay enhancements for the widescreen mode.
// Our own code, attached to the original game at a few addresses (see docs).
#pragma once
#include "../core/sms.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOD_MAX_PIECES 256
#define MOD_MAX_KILLED 256   // enemies remembered as killed in the current round

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
  int slot_world[46];        // where the enemy is placed in the map (camera value + its screen X)
  uint8_t slot_y[46], slot_list[46];
  // An enemy is in two spawn lists (one used walking right, one walking left) with different entries:
  // it is known by section, list group, slot, Y and map position, so it stays dead from both sides.
  struct { int trigger, world; uint8_t sect, list, slot, y; } killed[MOD_MAX_KILLED];
  int n_killed;
  uint32_t kill_key;         // round the list belongs to
  // difficulty
  int difficulty;            // MOD_DIFF_*
  int frac_taken, frac_dealt;
  int saved_dmg_addr; uint8_t saved_dmg;
  // music pack: the game's sound driver asks before starting a track
  bool (*music_has)(int id);   // set by the app: a replacement exists for this track
  int music_events[8];         // MOD_MUSIC_* | id, read by the app with mod_music_poll
  int n_music_events;
  // Survivor: when energy_lock is on, potions heal nothing and a new stage starts with energy_keep
  // (the energy the hero had) instead of a full bar. The app keeps energy_keep up to date.
  bool energy_lock;
  uint8_t energy_keep;
  // double jump
  bool double_jump;
  bool air_jump_used;
  int prev_state;
  int margin_l, margin_r;    // playable pixels visible beyond the original picture (per side)
  // more subweapons on screen: extra projectile slots in free RAM, run by the game's own code
  bool multi_shot;
  int ms_draw, ms_coll, ms_wall, ms_upd, ms_spr, ms_vel;   // re-entry state of each extended loop
  uint16_t ms_hl;
  uint8_t ms_saved[8];
  // moments for the vibration (MOD_EV_*), collected until the app reads them
  uint32_t events;
  uint8_t prev_buttons;
  bool boss_music;
} mod_enhance_t;

// Moments of the game the app can react to (vibration). Read with mod_events_take.
#define MOD_EV_SWING      0x001   // the hero attacks (attack button pressed while playing)
#define MOD_EV_HIT        0x002   // a weapon hits an enemy
#define MOD_EV_KILL       0x004   // an enemy is destroyed
#define MOD_EV_HURT       0x008   // the hero takes damage
#define MOD_EV_DEATH      0x010   // a life is lost
#define MOD_EV_LAND       0x020   // the hero lands after a jump or fall
#define MOD_EV_BOSS_START 0x040   // the boss music starts
#define MOD_EV_BOSS_HIT   0x080   // a boss is hit
#define MOD_EV_BOSS_DOWN  0x100   // a boss runs out of energy
#define MOD_EV_MASK       0x200   // a floating mask is broken (the object it hides comes out)
#define MOD_EV_SECRET     0x400   // a hit on a wall uncovers a hidden object
#define MOD_EV_MELEE      0x800   // attack with the main weapon (attack pressed without holding up)
#define MOD_EV_PICKUP     0x1000  // an object is picked up
#define MOD_EV_LIFE       0x2000  // an extra life is picked up
uint32_t mod_events_take(mod_enhance_t* e);

#define MOD_HUD_WEAPON 1
#define MOD_HUD_SUBWEAPON 2
#define MOD_HUD_LIVES 4
#define MOD_HUD_ENERGY 8

enum { MOD_RESPAWN_ON, MOD_RESPAWN_OFF, MOD_RESPAWN_MIXED };
enum { MOD_DIFF_EASY, MOD_DIFF_NORMAL, MOD_DIFF_HARD };
// music events: MOD_MUSIC_PLAY | id (replacement takes over), MOD_MUSIC_STOP [| id of an original track],
// MOD_MUSIC_FADE (the game fades the music out)
#define MOD_MUSIC_PLAY 0x100
#define MOD_MUSIC_STOP 0x200
#define MOD_MUSIC_FADE 0x300
int mod_music_poll(mod_enhance_t* e);           // next event or 0
bool mod_music_paused(const sms_t* s);          // the game has paused its sound (pause screen)
void mod_respawn_clear(mod_enhance_t* e);       // forget the enemies killed so far
void mod_respawn_forget_slots(mod_enhance_t* e); // after loading a state: the objects on screen are not known
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
