// Achievements of the "achievements mode": what the game reports frame by frame becomes unlocks,
// kept in a small text file (the profile) that can be exported and imported.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char* id;        // name in the profile file: never change it
  const char* name[2];   // 0 English, 1 Spanish
  const char* desc[2];
} logro_t;

typedef struct {
  uint32_t events;   // MOD_EV_* of this frame
  bool gameplay;     // a level is running
  uint8_t round, stage, weapon, sub, ammo;
} logros_in_t;

void logros_init(const char* profile_path);
void logros_switch_profile(const char* profile_path);   // another profile file; the game being played goes on
int logros_count(void);
int logros_unlocked_count(void);
const logro_t* logros_get(int i);
bool logros_has(int i);
unsigned logros_kills(void);
unsigned logros_masks(void);

void logros_reset_run(void);              // a new game starts
void logros_set_survivor(int diff);       // games from now on are Survivor (0 easy, 1 normal, 2 hard), -1 not
void logros_music(int id);                // the game asks for this track (0x81-0x8F)
void logros_frame(const logros_in_t* in);
int logros_pop_new(void);                 // next achievement just unlocked, or -1
void logros_flush(void);                  // writes the profile if anything is pending

// A saved state carries how its game was going, so loading one takes the deaths and hits back too.
size_t logros_run_size(void);
void logros_run_save(void* out);
void logros_run_load(const void* in);     // and notes that this game has used a saved state

// Adds a profile file to the current one (nothing is ever lost). Returns the achievements gained,
// or -1 if the file is not a valid profile.
int logros_merge_file(const char* path);
