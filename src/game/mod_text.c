#include "mod_text.h"
#include "mod_wide.h"
#include <string.h>

static uint8_t glyphs[2 * MOD_GLYPH_COUNT][32];   // letters, then their accents (drawn in the row above)

typedef uint8_t px8[8][8];

static void unpack(const uint8_t* t, px8 p) {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      int b = 7 - x;
      p[y][x] = (uint8_t)(((t[y * 4] >> b) & 1) | (((t[y * 4 + 1] >> b) & 1) << 1) | (((t[y * 4 + 2] >> b) & 1) << 2) |
                          (((t[y * 4 + 3] >> b) & 1) << 3));
    }
}

static void pack(px8 p, uint8_t* t) {
  memset(t, 0, 32);
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      for (int k = 0; k < 4; k++)
        if (p[y][x] & (1 << k)) t[y * 4 + k] |= (uint8_t)(0x80 >> x);
}

static int ink(px8 p) {   // colour of the letter
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (p[y][x]) return p[y][x];
  return 15;
}

static void mark(px8 p, int y, const char* row, int c) { for (int x = 0; x < 8; x++) if (row[x] == '#') p[y][x] = (uint8_t)c; }

static void upside_down(px8 src, px8 dst) {
  // turned half round, then moved back to the top-left like the rest of the font (7x7 in 8x8)
  for (int y = 0; y < 7; y++) for (int x = 0; x < 7; x++) dst[y][x] = src[6 - y][6 - x];
  for (int x = 0; x < 8; x++) dst[7][x] = 0;
  for (int y = 0; y < 8; y++) dst[y][7] = 0;
}

static bool build(const sms_t* s) {
  const uint8_t* v = s->st.vdp.vram;
  px8 a, g;
  unpack(&v[(0x100 + 'A') * 32], a);
  if (!ink(a) || !memcmp(&v[(0x100 + 'A') * 32], &v[(0x100 + 'B') * 32], 32)) return false;   // no font loaded
  // the letter itself is the game's own; the accent goes in the bottom of the empty row above
  static const char base[MOD_GLYPH_COUNT] = {'A', 'E', 'I', 'O', 'U', 'N', '?', '>', 'U'};   // '>' is the font's '!'
  for (int i = 0; i < MOD_GLYPH_COUNT; i++) {
    unpack(&v[(0x100 + base[i]) * 32], a);
    int c = ink(a);
    if (i == 6 || i == 7) upside_down(a, g); else memcpy(g, a, sizeof g);
    pack(g, glyphs[i]);
    memset(g, 0, sizeof g);
    if (i < 5) { mark(g, 5, ".....##.", c); mark(g, 6, "....##..", c); }   // acute
    else if (i == 5) { mark(g, 5, "..##..#.", c); mark(g, 6, ".#..##..", c); }   // tilde
    else if (i == 8) mark(g, 6, ".##.##..", c);   // dieresis
    pack(g, glyphs[MOD_GLYPH_COUNT + i]);
  }
  return true;
}

// The score label of the status bar (ROM 10024, 5 letters) in the top rows: a level is running, even when
// mod_is_gameplay says no (boss dialogs cover the playfield). Then tiles 1B0-1B8 belong to the level.
static bool hud_visible(const sms_t* s) {
  if (s->rom_size < 0x10029) return false;
  const uint8_t* lab = s->rom + 0x10024;
  const uint8_t* v = s->st.vdp.vram;
  uint16_t nt = (s->st.vdp.reg[2] & 0x0E) << 10;
  for (int row = 0; row < 4; row++)
    for (int c = 0; c + 5 <= 32; c++) {
      int k = 0;
      while (k < 5) {
        const uint8_t* e = &v[(nt + (row * 32 + c + k) * 2) & 0x3FFF];
        if (e[0] != lab[k] || !(e[1] & 1)) break;
        k++;
      }
      if (k == 5) return true;
    }
  return false;
}

// Text screens leave the extra letters' tiles empty; anything drawn there is someone else's.
static bool glyph_tiles_free(const sms_t* s) {
  const uint8_t* v = s->st.vdp.vram;
  for (int i = 0; i < MOD_GLYPH_COUNT * 32; i++) if (v[(MOD_GLYPH_FIRST * 32 + i) & 0x3FFF]) return false;
  return true;
}

void mod_text_frame(sms_t* s, bool on) {
  s->glyphs = NULL;
  if (!on || mod_is_gameplay(s) || hud_visible(s) || !glyph_tiles_free(s) || !build(s)) return;
  s->glyphs = (const uint8_t(*)[32])glyphs;
  s->glyph_first = MOD_GLYPH_FIRST;
  s->glyph_count = MOD_GLYPH_COUNT;
}
