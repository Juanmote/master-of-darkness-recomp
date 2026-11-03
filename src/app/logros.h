// Achievements of the "achievements mode": what the game reports frame by frame becomes unlocks,
// kept in a small text file (the profile) that can be exported and imported.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  const char* id;        // name in the profile file: never change it
  const char* name[2];   // 0 English, 1 Spanish
  const char* desc[2];
} logro_t;

typedef struct {
  uint32_t events;   // MOD_EV_* of this frame
  bool gameplay;     // a level is running
  uint8_t round, stage, weapon, ammo;
} logros_in_t;

void logros_init(const char* profile_path);
int logros_count(void);
int logros_unlocked_count(void);
const logro_t* logros_get(int i);
bool logros_has(int i);
unsigned logros_kills(void);

void logros_reset_run(void);              // a new game starts
void logros_music(int id);                // the game asks for this track (0x81-0x8F)
void logros_frame(const logros_in_t* in);
int logros_pop_new(void);                 // next achievement just unlocked, or -1
void logros_flush(void);                  // writes the profile if anything is pending

// Adds a profile file to the current one (nothing is ever lost). Returns the achievements gained,
// or -1 if the file is not a valid profile.
int logros_merge_file(const char* path);
