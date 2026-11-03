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
static void fr(uint32_t ev, int round, int stage, int weapon, int ammo) {
  logros_in_t in = {ev, true, (uint8_t)round, (uint8_t)stage, (uint8_t)weapon, (uint8_t)ammo};
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
}

int main(void) {
  const char* P = "test_perfil.txt", *Q = "test_perfil2.txt";
  fresh(P);
  logros_music(0x83);
  fr(0, 0, 0, 0, 0); fr(MOD_EV_HURT, 0, 0, 0, 0);
  boss(0, 1, 0, false);
  expect("boss without damage (the round was hurt)", "bossclean ");
  fr(0, 1, 0, 1, 0);
  expect("round 1 with a hit: no clean round, no subweapon used", "round1 roundnosub ");
  boss(1, 0, 16, true);
  expect("knife, hurt", "bossknife ");
  fr(0, 2, 0, 0, 15);
  expect("round 2: already known ones are not repeated", "round2 ");
  fr(MOD_EV_DEATH, 2, 1, 3, 15);
  boss(2, 3, 15, true);
  expect("axe", "bossaxe ");
  fr(0, 3, 0, 3, 15); expect("round 3", "round3 ");
  logros_music(0x8D); expect("story page mid game is not the ending", "");
  fr(0, 4, 0, 3, 15); expect("round 4 (round 3 had no hits)", "round4 roundclean ");
  logros_music(0x8D); expect("story before the last boss is not the ending", "");
  boss(4, 3, 15, true);
  logros_music(0x8D); drain();
  expect("ending after a death: no immortal, no continue is fine", "final nocont ");

  fresh(P);
  logros_music(0x87);
  fr(0, 0, 0, 3, 0);
  for (int r = 1; r <= 4; r++) { fr(0, r, 0, 3, 0); }
  got[0] = 0;
  boss(4, 3, 0, false); got[0] = 0;
  logros_music(0x8D); drain();
  expect("flawless ending", "final nocont nodeath ");

  fresh(P);
  logros_music(0x83);
  fr(0, 2, 0, 3, 0);   // a game started from the hidden menu
  boss(2, 3, 0, false);
  fr(0, 3, 0, 3, 0);
  expect("a game not started at the first stage earns nothing", "");

  fresh(P);
  logros_music(0x83);
  fr(0, 0, 0, 3, 0);
  logros_music(0x82);
  fr(0, 3, 0, 3, 0);
  expect("skipping rounds earns nothing", "");

  fresh(P);
  logros_music(0x83);
  fr(0, 0, 0, 1, 0);
  fr(0, 1, 0, 1, 0); got[0] = 0;
  logros_music(0x83);
  fr(0, 1, 0, 1, 0); fr(0, 2, 0, 1, 0);
  expect("back to the title forgets the game", "");

  // a saved state brings back how the game was going
  fresh(P);
  logros_music(0x83); fr(0, 0, 0, 1, 0);
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
  logros_music(0x83); fr(0, 0, 0, 1, 0); boss(0, 1, 0, true); fr(0, 1, 0, 1, 0); got[0] = 0;
  int g = logros_merge_file(Q);
  if (g != 2 || logros_kills() != 1000 || logros_unlocked_count() != 4) { printf("FAIL merge %d %u %d\n", g, logros_kills(), logros_unlocked_count()); fails++; } else printf("ok   merge\n");
  logros_init(P);
  if (logros_unlocked_count() != 4) { printf("FAIL merge saved\n"); fails++; } else printf("ok   merge saved\n");

  // damaged and foreign files are refused
  FILE* f = fopen(Q, "rb+"); fseek(f, 20, SEEK_SET); fputc('9', f); fclose(f);
  if (logros_merge_file(Q) != -1) { printf("FAIL edited accepted\n"); fails++; } else printf("ok   edited refused\n");
  f = fopen(Q, "wb"); fputs("hello\n", f); fclose(f);
  if (logros_merge_file(Q) != -1) { printf("FAIL foreign accepted\n"); fails++; } else printf("ok   foreign refused\n");
  remove(P); remove(Q);
  printf("%s\n", fails ? "FAILED" : "ALL OK");
  return fails != 0;
}
