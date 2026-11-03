#include "logros.h"
#include "../game/mod_enhance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { L_ROUND1, L_ROUND2, L_ROUND3, L_ROUND4, L_FINAL, L_NOCONT, L_NODEATH, L_ROUND_CLEAN, L_ROUND_NOSUB,
       L_BOSS_CLEAN, L_BOSS_KNIFE, L_BOSS_AXE, L_KILL100, L_KILL1000, L_COUNT };

static const logro_t list[L_COUNT] = {
  {"round1", {"Goodbye, Jack", "Adiós, Jack"}, {"Clear round 1", "Supera la ronda 1"}},
  {"round2", {"Museum closed", "Museo cerrado"}, {"Clear round 2", "Supera la ronda 2"}},
  {"round3", {"Rest in peace", "Descanse en paz"}, {"Clear round 3", "Supera la ronda 3"}},
  {"round4", {"Out of the castle", "Fuera del castillo"}, {"Clear round 4", "Supera la ronda 4"}},
  {"final", {"End of the curse", "Fin de la maldición"}, {"Finish the game", "Termina el juego"}},
  {"nocont", {"No second chances", "Sin segundas oportunidades"}, {"Finish the game without continuing", "Termina el juego sin continuar"}},
  {"nodeath", {"Immortal", "Inmortal"}, {"Finish the game without losing a life", "Termina el juego sin perder una vida"}},
  {"roundclean", {"Not a scratch", "Ni un rasguño"}, {"Clear a round without taking damage", "Supera una ronda sin recibir daño"}},
  {"roundnosub", {"Bare hands", "A cuerpo limpio"}, {"Clear a round without using a subweapon", "Supera una ronda sin gastar subarma"}},
  {"bossclean", {"Untouchable", "Intocable"}, {"Beat a boss without taking damage", "Vence a un jefe sin recibir daño"}},
  {"bossknife", {"They got you", "Te la colaron"}, {"Beat a boss with the knife", "Vence a un jefe con el cuchillo"}},
  {"bossaxe", {"Lumberjack", "Leñador"}, {"Beat a boss with the axe", "Vence a un jefe con el hacha"}},
  {"kill100", {"Hunter", "Cazador"}, {"Destroy 100 enemies in total", "Destruye 100 enemigos en total"}},
  {"kill1000", {"Exterminator", "Exterminador"}, {"Destroy 1000 enemies in total", "Destruye 1000 enemigos en total"}},
};

#define LAST_ROUND 4          // rounds are 0..4 in the game's RAM
#define MUS_STORY 0x8D        // story pages, and the ending
#define MUS_CONTINUE 0x82
#define MUS_INTRO 0x87        // a new game: the title music (83) also sounds between rounds

static char path[1200];
static bool got[L_COUNT];
static unsigned kills;
static bool dirty;
static int pending[L_COUNT], n_pending;

static struct {
  bool started, valid;        // valid: this game began at the first stage
  int round, prev_ammo;
  int deaths, continues;
  bool round_hurt, round_sub;
  bool boss_on, boss_hurt, last_boss_down;
  bool state_loaded;          // for achievements earned without loading a state (Survivor)
} run;

// ---------------------------------------------------------------- profile file
static uint32_t fnv(const char* s, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
  return h;
}

// Reads a profile. Any damaged or edited file is refused as a whole.
static bool parse_file(const char* p, bool out[L_COUNT], unsigned* out_kills) {
  FILE* f = fopen(p, "rb");
  if (!f) return false;
  char buf[4096];
  size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  static const char head[] = "MOD-PERFIL 1\n";
  if (strncmp(buf, head, sizeof head - 1)) return false;
  char* chk = strstr(buf, "chk=");
  if (!chk || strtoul(chk + 4, NULL, 16) != fnv(buf, (size_t)(chk - buf))) return false;
  memset(out, 0, sizeof(bool) * L_COUNT);
  *out_kills = 0;
  *chk = 0;
  for (char* l = strtok(buf + sizeof head - 1, "\n"); l; l = strtok(NULL, "\n")) {
    if (!strncmp(l, "kills=", 6)) *out_kills = (unsigned)strtoul(l + 6, NULL, 10);
    else if (!strncmp(l, "logro=", 6))
      for (int i = 0; i < L_COUNT; i++) if (!strcmp(l + 6, list[i].id)) out[i] = true;
  }
  return true;
}

static void save(void) {
  if (!path[0]) return;
  char body[4096];
  int n = snprintf(body, sizeof body, "MOD-PERFIL 1\nkills=%u\n", kills);
  for (int i = 0; i < L_COUNT; i++)
    if (got[i]) n += snprintf(body + n, sizeof body - (size_t)n, "logro=%s\n", list[i].id);
  n += snprintf(body + n, sizeof body - (size_t)n, "chk=%08X\n", fnv(body, (size_t)n));
  char tmp[1300];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE* f = fopen(tmp, "wb");
  if (!f) return;
  bool ok = fwrite(body, 1, (size_t)n, f) == (size_t)n;
  ok = fclose(f) == 0 && ok;
  if (ok && rename(tmp, path) == 0) dirty = false;
  else remove(tmp);
}

void logros_flush(void) { if (dirty) save(); }

void logros_init(const char* profile_path) {
  snprintf(path, sizeof path, "%s", profile_path);
  memset(got, 0, sizeof got);
  kills = 0;
  dirty = false;
  n_pending = 0;
  if (!parse_file(path, got, &kills)) { memset(got, 0, sizeof got); kills = 0; }
  logros_reset_run();
}

int logros_merge_file(const char* p) {
  bool other[L_COUNT];
  unsigned other_kills;
  if (!parse_file(p, other, &other_kills)) return -1;
  int gained = 0;
  for (int i = 0; i < L_COUNT; i++) if (other[i] && !got[i]) { got[i] = true; gained++; }
  if (other_kills > kills) kills = other_kills;
  dirty = true;
  save();
  return gained;
}

// ---------------------------------------------------------------- queries
int logros_count(void) { return L_COUNT; }
int logros_unlocked_count(void) { int n = 0; for (int i = 0; i < L_COUNT; i++) n += got[i]; return n; }
const logro_t* logros_get(int i) { return &list[i]; }
bool logros_has(int i) { return got[i]; }
unsigned logros_kills(void) { return kills; }

int logros_pop_new(void) {
  if (!n_pending) return -1;
  int v = pending[0];
  memmove(pending, pending + 1, sizeof(int) * (size_t)--n_pending);
  return v;
}

static void unlock(int i) {
  if (got[i]) return;
  got[i] = true;
  if (n_pending < L_COUNT) pending[n_pending++] = i;
  dirty = true;
  save();
}

// ---------------------------------------------------------------- what happens in the game
void logros_reset_run(void) { memset(&run, 0, sizeof run); }

size_t logros_run_size(void) { return sizeof run; }
void logros_run_save(void* out) { memcpy(out, &run, sizeof run); }
void logros_run_load(const void* in) { memcpy(&run, in, sizeof run); run.state_loaded = true; }

static void finish_round(int r) {
  if (r < LAST_ROUND) unlock(L_ROUND1 + r);
  if (!run.round_hurt) unlock(L_ROUND_CLEAN);
  if (!run.round_sub) unlock(L_ROUND_NOSUB);
}

void logros_music(int id) {
  if (id == MUS_INTRO) logros_reset_run();
  else if (id == MUS_CONTINUE) run.continues++;
  else if (id == MUS_STORY && run.valid && run.round == LAST_ROUND && run.last_boss_down) {
    // there are no story pages after the last round: this is the ending
    finish_round(LAST_ROUND);
    unlock(L_FINAL);
    if (!run.continues) unlock(L_NOCONT);
    if (!run.deaths) unlock(L_NODEATH);
    run.valid = false;
  }
}

void logros_frame(const logros_in_t* in) {
  if (in->gameplay) {
    if (!run.started) {
      run.started = true;
      run.valid = in->round == 0 && in->stage == 0;
      run.round = in->round;
      run.prev_ammo = in->ammo;
    } else if (in->round != run.round) {
      if (run.valid && in->round == run.round + 1) finish_round(run.round);
      else run.valid = false;
      run.round = in->round;
      run.round_hurt = run.round_sub = false;
      run.boss_on = run.last_boss_down = false;
    }
    if (in->ammo < run.prev_ammo) run.round_sub = true;
    run.prev_ammo = in->ammo;
  }
  uint32_t ev = in->events;
  if (ev & MOD_EV_KILL) {
    kills++;
    dirty = true;
    if (kills % 10 == 0) save();
    if (kills >= 100) unlock(L_KILL100);
    if (kills >= 1000) unlock(L_KILL1000);
  }
  if (ev & MOD_EV_HURT) run.round_hurt = run.boss_hurt = true;
  if (ev & MOD_EV_DEATH) { run.deaths++; run.round_hurt = run.boss_hurt = true; }
  if (ev & MOD_EV_BOSS_START) { run.boss_on = true; run.boss_hurt = false; }
  if ((ev & MOD_EV_BOSS_DOWN) && run.boss_on) {
    run.boss_on = false;
    run.last_boss_down = run.round == LAST_ROUND;
    if (run.valid) {
      if (!run.boss_hurt) unlock(L_BOSS_CLEAN);
      if (in->weapon == 0) unlock(L_BOSS_KNIFE);
      if (in->weapon == 3) unlock(L_BOSS_AXE);
    }
  }
}
