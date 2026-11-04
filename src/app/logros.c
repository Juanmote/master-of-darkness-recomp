#include "logros.h"
#include "../game/mod_enhance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Shown in this order, grouped by kind. The profile stores the ids, so the order can change freely.
enum { L_ROUND1, L_ROUND2, L_ROUND3, L_ROUND4, L_FINAL,                       // story
       L_NOCONT, L_NODEATH,                                                   // a whole game
       L_STAGE_CLEAN, L_STAGE_NOSUB, L_ROUND_CLEAN, L_ROUND_NOSUB,            // stages and rounds
       L_BOSS_CLEAN, L_BOSS_KNIFE, L_BOSS_AXE,                                // bosses
       L_SECRET, L_LIFE, L_WEAPONS, L_SUBS, L_MASK50, L_KILL100, L_KILL1000,  // collected over any games
       L_SV_ROUND1, L_SV_ROUND2, L_SV_ROUND3, L_SV_ROUND4,                    // Survivor mode
       L_SV_EASY, L_SV_NORMAL, L_SV_HARD, L_SV_NOSTATE,
       L_BR_BOSS1, L_BR_BOSS2, L_BR_BOSS3, L_BR_BOSS4,                        // Boss Rush
       L_BR_EASY, L_BR_NORMAL, L_BR_HARD, L_COUNT };

static const logro_t list[L_COUNT] = {
  {"round1", {"Goodbye, Jack", "Adiós, Jack"}, {"Clear round 1", "Supera la ronda 1"}},
  {"round2", {"Museum closed", "Museo cerrado"}, {"Clear round 2", "Supera la ronda 2"}},
  {"round3", {"Rest in peace", "Descanse en paz"}, {"Clear round 3", "Supera la ronda 3"}},
  {"round4", {"Out of the castle", "Fuera del castillo"}, {"Clear round 4", "Supera la ronda 4"}},
  {"final", {"End of the curse", "Fin de la maldición"}, {"Finish the game", "Termina el juego"}},
  {"nocont", {"No second chances", "Sin segundas oportunidades"}, {"Finish the game without continuing", "Termina el juego sin continuar"}},
  {"nodeath", {"Immortal", "Inmortal"}, {"Finish the game without losing a life", "Termina el juego sin perder una vida"}},
  {"stageclean", {"A quiet walk", "Paseo tranquilo"}, {"Clear a stage without taking damage", "Supera una fase sin recibir daño"}},
  {"stagenosub", {"Saving ammo", "Ahorrando munición"}, {"Clear a stage without using a subweapon", "Supera una fase sin gastar subarma"}},
  {"roundclean", {"Not a scratch", "Ni un rasguño"}, {"Clear a whole round without taking damage", "Supera una ronda entera sin recibir daño"}},
  {"roundnosub", {"Bare hands", "A cuerpo limpio"}, {"Clear a whole round without using a subweapon", "Supera una ronda entera sin gastar subarma"}},
  {"bossclean", {"Untouchable", "Intocable"}, {"Beat a boss without taking damage", "Vence a un jefe sin recibir daño"}},
  {"bossknife", {"They got you", "Te la colaron"}, {"Beat a boss with the knife", "Vence a un jefe con el cuchillo"}},
  {"bossaxe", {"Lumberjack", "Leñador"}, {"Beat a boss with the axe", "Vence a un jefe con el hacha"}},
  {"secret", {"Sounds hollow", "Suena a hueco"}, {"Find an object hidden in a wall", "Descubre un objeto escondido en una pared"}},
  {"extralife", {"Mini-me", "Miniyo"}, {"Pick up an extra life", "Consigue una vida extra"}},
  {"weapons", {"Full arsenal", "Arsenal completo"}, {"Attack with the 4 main weapons (over any games)", "Ataca con las 4 armas principales (en cualquier partida)"}},
  {"subs", {"Bag of tricks", "Caja de sorpresas"}, {"Throw the 4 subweapons (over any games)", "Lanza las 4 subarmas (en cualquier partida)"}},
  {"mask50", {"Carnival is over", "Se acabó el carnaval"}, {"Destroy 50 masks in total", "Destruye 50 máscaras en total"}},
  {"kill100", {"Hunter", "Cazador"}, {"Destroy 100 enemies in total", "Destruye 100 enemigos en total"}},
  {"kill1000", {"Exterminator", "Exterminador"}, {"Destroy 1000 enemies in total", "Destruye 1000 enemigos en total"}},
  {"sv_round1", {"First night", "Primera noche"}, {"Clear round 1 in Survivor mode", "Supera la ronda 1 en modo Survivor"}},
  {"sv_round2", {"Still standing", "Sigo en pie"}, {"Clear round 2 in Survivor mode", "Supera la ronda 2 en modo Survivor"}},
  {"sv_round3", {"Among the graves", "Entre tumbas"}, {"Clear round 3 in Survivor mode", "Supera la ronda 3 en modo Survivor"}},
  {"sv_round4", {"At the gates", "A las puertas"}, {"Clear round 4 in Survivor mode", "Supera la ronda 4 en modo Survivor"}},
  {"sv_easy", {"Survivor", "Superviviente"}, {"Finish Survivor mode on Easy (or harder)", "Termina el modo Survivor en Fácil (o más difícil)"}},
  {"sv_normal", {"Hard to kill", "Duro de matar"}, {"Finish Survivor mode on Normal (or harder)", "Termina el modo Survivor en Normal (o más difícil)"}},
  {"sv_hard", {"Legend of London", "Leyenda de Londres"}, {"Finish Survivor mode on Hard", "Termina el modo Survivor en Difícil"}},
  {"sv_nostate", {"No safety net", "Sin red"}, {"Finish Survivor mode without retrying a stage", "Termina el modo Survivor sin repetir ninguna fase"}},
  {"br_boss1", {"Warm-up", "Calentamiento"}, {"Beat the first boss in Boss Rush", "Vence al primer jefe en Boss Rush"}},
  {"br_boss2", {"Second round", "Segundo asalto"}, {"Beat the second boss in Boss Rush", "Vence al segundo jefe en Boss Rush"}},
  {"br_boss3", {"Halfway there", "A mitad de camino"}, {"Beat the third boss in Boss Rush", "Vence al tercer jefe en Boss Rush"}},
  {"br_boss4", {"Only one left", "Solo queda uno"}, {"Beat the fourth boss in Boss Rush", "Vence al cuarto jefe en Boss Rush"}},
  {"br_easy", {"Hunting party", "Partida de caza"}, {"Finish Boss Rush on Easy (or harder)", "Termina el Boss Rush en Fácil (o más difícil)"}},
  {"br_normal", {"Monster slayer", "Matamonstruos"}, {"Finish Boss Rush on Normal (or harder)", "Termina el Boss Rush en Normal (o más difícil)"}},
  {"br_hard", {"Nightmare of the night", "Pesadilla de la noche"}, {"Finish Boss Rush on Hard", "Termina el Boss Rush en Difícil"}},
};

#define LAST_ROUND 4          // rounds are 0..4 in the game's RAM
#define MUS_STORY 0x8D        // story pages, and the ending
#define MUS_CONTINUE 0x82
#define MUS_STAGE_CLEAR 0x81
#define MUS_ROUND_CLEAR 0x86  // the boss of a round's last stage has fallen
#define LAST_STAGE 2          // of rounds 0..3 (B9CB in the game's stage table)
#define MUS_INTRO 0x87        // a new game: the title music (83) also sounds between rounds

static char path[1200];
static bool got[L_COUNT];
static unsigned kills, masks;
static unsigned weapons_used, subs_used;   // one bit per weapon, kept between games
static unsigned br_best[3];                // Boss Rush best time per difficulty, in frames (0: none yet)
static bool dirty;
static int survivor = -1;     // Survivor difficulty of the games being played, -1 none
static int pending[L_COUNT], n_pending;

static struct {
  bool started, valid;        // valid: this game began at the first stage
  int round, stage, prev_ammo, prev_sub;
  bool round_done;            // its achievements were already given
  int deaths, continues;
  bool round_hurt, round_sub, stage_hurt, stage_sub;
  bool boss_on, boss_hurt, last_boss_down;
  bool state_loaded;          // for achievements earned without loading a state (Survivor)
  uint8_t sv_on, sv_diff;     // a Survivor game, and its difficulty (0 easy .. 2 hard)
  uint8_t spare[30];          // room for new fields: saved states keep their size
} run;

// ---------------------------------------------------------------- profile file
static uint32_t fnv(const char* s, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
  return h;
}

// Reads a profile. Any damaged or edited file is refused as a whole.
typedef struct { unsigned kills, masks, weapons, subs, br[3]; } counts_t;

static bool parse_file(const char* p, bool out[L_COUNT], counts_t* c) {
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
  memset(c, 0, sizeof *c);
  *chk = 0;
  for (char* l = strtok(buf + sizeof head - 1, "\n"); l; l = strtok(NULL, "\n")) {
    if (!strncmp(l, "kills=", 6)) c->kills = (unsigned)strtoul(l + 6, NULL, 10);
    else if (!strncmp(l, "masks=", 6)) c->masks = (unsigned)strtoul(l + 6, NULL, 10);
    else if (!strncmp(l, "weapons=", 8)) c->weapons = (unsigned)strtoul(l + 8, NULL, 10) & 15;
    else if (!strncmp(l, "subs=", 5)) c->subs = (unsigned)strtoul(l + 5, NULL, 10) & 15;
    else if (!strncmp(l, "bossrush", 8) && l[8] >= '0' && l[8] <= '2' && l[9] == '=') c->br[l[8] - '0'] = (unsigned)strtoul(l + 10, NULL, 10);
    else if (!strncmp(l, "logro=", 6))
      for (int i = 0; i < L_COUNT; i++) if (!strcmp(l + 6, list[i].id)) out[i] = true;
  }
  return true;
}

static void save(void) {
  if (!path[0]) return;
  char body[4096];
  int n = snprintf(body, sizeof body, "MOD-PERFIL 1\nkills=%u\nmasks=%u\nweapons=%u\nsubs=%u\n", kills, masks, weapons_used, subs_used);
  for (int d = 0; d < 3; d++) if (br_best[d]) n += snprintf(body + n, sizeof body - (size_t)n, "bossrush%d=%u\n", d, br_best[d]);
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
  kills = masks = weapons_used = subs_used = 0;
  memset(br_best, 0, sizeof br_best);
  dirty = false;
  n_pending = 0;
  counts_t c;
  if (parse_file(path, got, &c)) { kills = c.kills; masks = c.masks; weapons_used = c.weapons; subs_used = c.subs; memcpy(br_best, c.br, sizeof br_best); }
  else memset(got, 0, sizeof got);
  logros_reset_run();
}

void logros_switch_profile(const char* profile_path) {
  logros_flush();
  snprintf(path, sizeof path, "%s", profile_path);
  memset(got, 0, sizeof got);
  kills = masks = weapons_used = subs_used = 0;
  memset(br_best, 0, sizeof br_best);
  dirty = false;
  n_pending = 0;
  counts_t c;
  if (parse_file(path, got, &c)) { kills = c.kills; masks = c.masks; weapons_used = c.weapons; subs_used = c.subs; memcpy(br_best, c.br, sizeof br_best); }
  else memset(got, 0, sizeof got);
}

int logros_merge_file(const char* p) {
  bool other[L_COUNT];
  counts_t c;
  if (!parse_file(p, other, &c)) return -1;
  int gained = 0;
  for (int i = 0; i < L_COUNT; i++) if (other[i] && !got[i]) { got[i] = true; gained++; }
  if (c.kills > kills) kills = c.kills;
  if (c.masks > masks) masks = c.masks;
  weapons_used |= c.weapons;
  subs_used |= c.subs;
  for (int d = 0; d < 3; d++) if (c.br[d] && (!br_best[d] || c.br[d] < br_best[d])) br_best[d] = c.br[d];
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
unsigned logros_masks(void) { return masks; }

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
void logros_reset_run(void) {
  memset(&run, 0, sizeof run);
  run.sv_on = survivor >= 0;
  run.sv_diff = (uint8_t)(survivor < 0 ? 0 : survivor);
}

void logros_set_survivor(int diff) { survivor = diff; run.sv_on = diff >= 0; run.sv_diff = (uint8_t)(diff < 0 ? 0 : diff); }

size_t logros_run_size(void) { return sizeof run; }
void logros_run_save(void* out) { memcpy(out, &run, sizeof run); }
void logros_run_load(const void* in) { memcpy(&run, in, sizeof run); run.state_loaded = true; }

static void finish_stage(void) {
  if (!run.stage_hurt) unlock(L_STAGE_CLEAN);
  if (!run.stage_sub) unlock(L_STAGE_NOSUB);
}

static void finish_round(int r) {
  if (r < LAST_ROUND) unlock(L_ROUND1 + r);
  if (r < LAST_ROUND && run.sv_on) unlock(L_SV_ROUND1 + r);
  if (!run.round_hurt) unlock(L_ROUND_CLEAN);
  if (!run.round_sub) unlock(L_ROUND_NOSUB);
}

void logros_music(int id) {
  if (id == MUS_INTRO) { logros_reset_run(); return; }
  if ((id == MUS_STAGE_CLEAR || id == MUS_ROUND_CLEAR) && run.valid) finish_stage();
  if (id == MUS_ROUND_CLEAR && run.valid && run.round < LAST_ROUND && run.stage == LAST_STAGE && !run.round_done) {
    finish_round(run.round);
    run.round_done = true;
  }
  else if (id == MUS_CONTINUE) run.continues++;
  else if (id == MUS_STORY && run.valid && run.round == LAST_ROUND && run.last_boss_down) {
    // there are no story pages after the last round: this is the ending
    finish_stage();
    finish_round(LAST_ROUND);
    unlock(L_FINAL);
    if (!run.continues) unlock(L_NOCONT);
    if (!run.deaths) unlock(L_NODEATH);
    if (run.sv_on && !run.deaths) {
      unlock(L_SV_EASY);
      if (run.sv_diff >= 1) unlock(L_SV_NORMAL);
      if (run.sv_diff >= 2) unlock(L_SV_HARD);
      if (!run.state_loaded) unlock(L_SV_NOSTATE);
    }
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
      run.prev_sub = in->sub;
    } else if (in->round != run.round) {
      if (run.valid && in->round == run.round + 1) { if (!run.round_done) finish_round(run.round); }
      else run.valid = false;
      run.round = in->round;
      run.round_done = false;
      run.round_hurt = run.round_sub = false;
      run.stage_hurt = run.stage_sub = false;
      run.boss_on = run.last_boss_down = false;
    }
    if (in->stage != run.stage) run.stage_hurt = run.stage_sub = false;
    run.stage = in->stage;
    // one less of the same subweapon: picking up another one can also lower the counter
    if (in->sub == run.prev_sub && in->ammo + 1 == run.prev_ammo) {
      run.round_sub = run.stage_sub = true;
      if (!(subs_used >> (in->sub & 3) & 1)) {
        subs_used |= 1u << (in->sub & 3);
        dirty = true;
        save();
        if (subs_used == 15) unlock(L_SUBS);
      }
    }
    run.prev_ammo = in->ammo;
    run.prev_sub = in->sub;
  }
  uint32_t ev = in->events;
  if ((ev & MOD_EV_MELEE) && !(weapons_used >> (in->weapon & 3) & 1)) {
    weapons_used |= 1u << (in->weapon & 3);
    dirty = true;
    save();
    if (weapons_used == 15) unlock(L_WEAPONS);
  }
  if (ev & MOD_EV_SECRET) unlock(L_SECRET);
  if (ev & MOD_EV_LIFE) unlock(L_LIFE);
  if (ev & MOD_EV_MASK) {
    masks++;
    dirty = true;
    if (masks % 10 == 0) save();
    if (masks >= 50) unlock(L_MASK50);
  }
  if (ev & MOD_EV_KILL) {
    kills++;
    dirty = true;
    if (kills % 10 == 0) save();
    if (kills >= 100) unlock(L_KILL100);
    if (kills >= 1000) unlock(L_KILL1000);
  }
  if (ev & MOD_EV_HURT) run.round_hurt = run.stage_hurt = run.boss_hurt = true;
  if (ev & MOD_EV_DEATH) { run.deaths++; run.round_hurt = run.stage_hurt = run.boss_hurt = true; }
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

// ---------------------------------------------------------------- Boss Rush
void logros_br_boss(int k) { if (k >= 0 && k < 4) unlock(L_BR_BOSS1 + k); }

unsigned logros_br_best(int diff) { return diff >= 0 && diff < 3 ? br_best[diff] : 0; }

bool logros_br_finish(int diff, unsigned frames) {
  if (diff < 0 || diff > 2) return false;
  unlock(L_BR_EASY);
  if (diff >= 1) unlock(L_BR_NORMAL);
  if (diff >= 2) unlock(L_BR_HARD);
  bool rec = !br_best[diff] || frames < br_best[diff];
  if (rec) { br_best[diff] = frames; dirty = true; save(); }
  return rec;
}
