// Scripted games through the achievements logic. Build:
//   cc -o test_logros tools/test/test_logros.c src/app/logros.c && ./test_logros
#include "../../src/app/logros.h"
#include "../../src/game/mod_enhance.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;
static char got[1024];
static void drain(void) { for (int n; (n = logros_pop_new()) >= 0; ) { strcat(got, logros_get(n)->id); strcat(got, " "); } }
static int cur_sub;
static void fr(uint32_t ev, int round, int stage, int weapon, int ammo) {
  logros_in_t in = {ev, true, (uint8_t)round, (uint8_t)stage, (uint8_t)weapon, (uint8_t)cur_sub, (uint8_t)ammo};
  logros_frame(&in); drain();
}
static void expect(const char* name, const char* want) {
  if (strcmp(got, want)) { printf("FAIL %s\n  want [%s]\n  got  [%s]\n", name, want, got); fails++; }
  else printf("ok   %s\n", name);
  got[0] = 0;
}
static void fresh(const char* p) { remove(p); logros_init(p); got[0] = 0; }
static void boss(int round, int weapon, int ammo, bool hurt) {
  logros_music(round == 4 ? 0x8E : 0x88);
  fr(MOD_EV_BOSS_START, round, 2, weapon, ammo);
  if (hurt) fr(MOD_EV_HURT, round, 2, weapon, ammo);
  fr(MOD_EV_BOSS_DOWN, round, 2, weapon, ammo);
  if (round < 4) { logros_music(0x86); drain(); }   // the boss only counts when its round is cleared
}

int main(void) {
  const char* P = "test_perfil.txt", *Q = "test_perfil2.txt";
  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 0, 0); fr(MOD_EV_HURT, 0, 0, 0, 0);
  boss(0, 1, 0, false);
  expect("boss without damage (the round was hurt), then its round clear", "bossclean stageclean stagenosub round1 roundnosub ");
  fr(0, 1, 0, 1, 0);
  expect("round 1 already given at its round clear", "");
  boss(1, 0, 16, true);
  expect("knife, hurt", "bossknife round2 ");
  fr(0, 2, 0, 0, 15);
  expect("round 2: already known ones are not repeated", "");
  fr(MOD_EV_DEATH, 2, 1, 3, 15);
  boss(2, 3, 15, true);
  expect("axe, after the first death", "firstdeath bossaxe round3 ");
  fr(0, 3, 0, 3, 15); expect("round 3 was given with its boss", "");
  logros_music(0x8D); expect("story page mid game is not the ending", "");
  fr(0, 4, 0, 3, 15); expect("round 4 (round 3 had no hits)", "round4 roundclean ");
  logros_music(0x8D); expect("story before the last boss is not the ending", "");
  boss(4, 3, 15, true);
  logros_music(0x8D); drain();
  expect("ending after a death: no immortal, no continue is fine", "final nocont ");

  // a boss with two forms: the bar empties, fills again, and only the round clear counts
  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 0, 0); fr(0, 1, 0, 0, 0); got[0] = 0;
  logros_music(0x88); fr(MOD_EV_BOSS_START, 1, 2, 0, 0);
  fr(MOD_EV_BOSS_DOWN, 1, 2, 0, 0);
  expect("first form down: nothing yet", "");
  fr(MOD_EV_HURT, 1, 2, 1, 0); fr(MOD_EV_BOSS_DOWN, 1, 2, 1, 0);
  logros_music(0x86); drain();
  expect("second form down with the sword, hurt: round clear only", "stagenosub round2 ");

  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 3, 0);
  for (int r = 1; r <= 4; r++) { fr(0, r, 0, 3, 0); }
  got[0] = 0;
  boss(4, 3, 0, false); got[0] = 0;
  logros_music(0x8D); drain();
  expect("flawless ending", "stageclean stagenosub final nocont nodeath ");

  // Survivor on normal: its round achievements, then the ending gives easy and normal (and no state loaded)
  fresh(P);
  logros_set_survivor(1);
  logros_music(0x87);
  fr(0, 0, 0, 3, 0);
  for (int r = 1; r <= 4; r++) { fr(0, r, 0, 3, 0); }
  expect("survivor rounds", "round1 sv_round1 roundclean roundnosub round2 sv_round2 round3 sv_round3 round4 sv_round4 ");
  boss(4, 3, 0, false); got[0] = 0;
  logros_music(0x8D); drain();
  expect("survivor ending on normal", "stageclean stagenosub final nocont nodeath sv_easy sv_normal sv_nostate ");

  // Survivor on hard with a state loaded on the way
  fresh(P);
  logros_set_survivor(2);
  logros_music(0x87);
  fr(0, 0, 0, 3, 0);
  { static unsigned char st[256]; logros_run_save(st); logros_run_load(st); }
  for (int r = 1; r <= 4; r++) { fr(0, r, 0, 3, 0); }
  boss(4, 3, 0, false); got[0] = 0;
  logros_music(0x8D); drain();
  expect("survivor on hard, state loaded", "stageclean stagenosub final nocont nodeath sv_easy sv_normal sv_hard ");
  logros_set_survivor(-1);

  fresh(P);
  logros_music(0x87);
  fr(0, 2, 0, 3, 0);   // a game started from the hidden menu
  boss(2, 3, 0, false);
  fr(0, 3, 0, 3, 0);
  expect("a game not started at the first stage earns nothing", "");

  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 3, 0);
  logros_music(0x82);
  fr(0, 3, 0, 3, 0);
  expect("skipping rounds earns nothing", "");

  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 1, 0);
  fr(0, 1, 0, 1, 0); got[0] = 0;
  logros_music(0x87);
  fr(0, 1, 0, 1, 0); fr(0, 2, 0, 1, 0);
  expect("back to the intro forgets the game", "");

  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 1, 0);
  logros_music(0x81); drain(); expect("a stage clear is not a round clear", "stageclean stagenosub ");
  fr(MOD_EV_HURT, 0, 1, 1, 5); fr(0, 0, 1, 1, 4);
  logros_music(0x81); drain(); expect("nothing new with a hit in the stage", "");
  fr(0, 0, 2, 1, 4);
  logros_music(0x86); drain();
  expect("round clear gives them at once, not the hurt ones", "round1 ");
  logros_music(0x83); fr(0, 1, 0, 1, 0);
  expect("and not again on the next round", "");

  fresh(P);
  logros_music(0x87); logros_music(0x83);
  fr(0, 0, 0, 1, 0);
  logros_music(0x83);                    // the title music sounds between rounds too
  fr(0, 1, 0, 1, 0);
  expect("title music between rounds keeps the game", "round1 roundclean roundnosub ");

  // picking up another subweapon with a lower counter is not using one
  fresh(P);
  logros_music(0x87);
  cur_sub = 0; fr(0, 0, 0, 1, 16);
  cur_sub = 3; fr(0, 0, 0, 1, 8);
  logros_music(0x81); drain();
  expect("swapping subweapon does not count as using it", "stageclean stagenosub ");
  cur_sub = 0;
  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 1, 16); fr(0, 0, 0, 1, 15);
  logros_music(0x81); drain();
  expect("throwing one does", "stageclean ");

  // a saved state brings back how the game was going
  fresh(P);
  logros_music(0x87); fr(0, 0, 0, 1, 0);
  { char st[512]; if (logros_run_size() > sizeof st) return 1;
    logros_run_save(st);
    fr(MOD_EV_HURT, 0, 0, 1, 0);          // hit after saving...
    logros_run_load(st);                  // ...undone by loading
    fr(0, 1, 0, 1, 0);
    expect("loading a state takes the hit back", "round1 roundclean roundnosub "); }

  fresh(P);
  for (int i = 0; i < 1000; i++) fr(MOD_EV_KILL, 0, 0, 1, 0);
  expect("kills", "kill100 kill1000 ");
  logros_flush();
  logros_init(P);
  if (logros_kills() != 1000 || logros_unlocked_count() != 2) { printf("FAIL reload\n"); fails++; } else printf("ok   reload\n");

  // a second profile merges: nothing is lost
  rename(P, Q);
  fresh(P);
  logros_music(0x87); fr(0, 0, 0, 1, 0); boss(0, 1, 0, true); fr(0, 1, 0, 1, 0); got[0] = 0;
  int g = logros_merge_file(Q);
  if (g != 2 || logros_kills() != 1000 || logros_unlocked_count() != 5) { printf("FAIL merge %d %u %d\n", g, logros_kills(), logros_unlocked_count()); fails++; } else printf("ok   merge\n");
  logros_init(P);
  if (logros_unlocked_count() != 5) { printf("FAIL merge saved\n"); fails++; } else printf("ok   merge saved\n");

  // damaged and foreign files are refused
  FILE* f = fopen(Q, "rb+"); fseek(f, 20, SEEK_SET); fputc('9', f); fclose(f);
  if (logros_merge_file(Q) != -1) { printf("FAIL edited accepted\n"); fails++; } else printf("ok   edited refused\n");
  f = fopen(Q, "wb"); fputs("hello\n", f); fclose(f);
  if (logros_merge_file(Q) != -1) { printf("FAIL foreign accepted\n"); fails++; } else printf("ok   foreign refused\n");

  // cumulative achievements, kept between games and across a profile merge
  fresh(P);
  fr(MOD_EV_SECRET, 0, 0, 0, 0); expect("secret wall", "secret ");
  for (int w = 0; w < 3; w++) fr(MOD_EV_MELEE, 0, 0, w, 0);
  fr(MOD_EV_SWING, 0, 0, 3, 0);                     // throwing is not an attack with the weapon
  expect("3 weapons are not enough", "");
  logros_init(P); logros_reset_run();
  fr(0, 0, 0, 3, 0); fr(MOD_EV_MELEE, 0, 0, 3, 0); expect("the fourth one in another game", "weapons ");
  fresh(P);
  for (int k = 0; k < 4; k++) {
    cur_sub = k; fr(0, 0, 0, 0, 9); fr(0, 0, 0, 0, 8);
    if (k == 2) { logros_flush(); logros_init(P); logros_reset_run(); }
  }
  cur_sub = 0;
  expect("the 4 subweapons", "subs ");
  fresh(P);
  for (int k = 0; k < 49; k++) fr(MOD_EV_MASK, 0, 0, 0, 0);
  expect("49 masks", "");
  logros_flush(); logros_init(P); logros_reset_run();
  if (logros_masks() != 49) { printf("FAIL masks saved %u\n", logros_masks()); fails++; } else printf("ok   masks saved\n");
  fr(MOD_EV_MASK, 0, 0, 0, 0); expect("50 masks", "mask50 ");
  fr(MOD_EV_PICKUP, 0, 0, 0, 0);
  if (logros_kills() != 0) { printf("FAIL pickup counted as a kill\n"); fails++; } else printf("ok   pickup is not a kill\n");
  rename(P, Q);
  fresh(P);
  fr(MOD_EV_MASK, 0, 0, 0, 0); logros_flush();
  if (logros_merge_file(Q) != 1 || logros_masks() != 50) { printf("FAIL merge masks %u\n", logros_masks()); fails++; } else printf("ok   merge masks\n");
  remove(P); remove(Q);
  printf("%s\n", fails ? "FAILED" : "ALL OK");
  return fails != 0;
}
