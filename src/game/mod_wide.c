#include "mod_wide.h"
#include <string.h>

// RAM variables found in phase 1 (see docs/NOTAS_TECNICAS.md)
#define RAM_CAM      0xC6A0  // 16-bit camera X (world pixels)
#define RAM_MAPBANK  0xC6A2  // ROM bank holding metatiles / screen descriptors
#define RAM_SCRDESC  0xC6A8  // pointer (slot 2) to 4-byte screen descriptors
#define RAM_METATILE 0xC6AC  // pointer (slot 2) to 4-byte metatile definitions
#define MAP_BASE     0xD000  // 10 rows x 192 metatiles, row stride 0xC0
#define MAP_W        192
#define MAP_ROWS     10
#define GAME_Y0      32      // first line of the playfield (name table row 4)

static inline uint8_t ram8(const sms_t* s, uint16_t a) { return s->st.ram[a & 0x1FFF]; }
static inline uint16_t ram16(const sms_t* s, uint16_t a) { return ram8(s, a) | (ram8(s, a + 1) << 8); }

typedef struct {
  const sms_t* s;
  const uint8_t* mt;    // metatile table in ROM
  const uint8_t* desc;  // screen descriptors in ROM
} mapctx_t;

static bool map_ctx(const sms_t* s, mapctx_t* m) {
  int bank = ram8(s, RAM_MAPBANK);
  uint16_t mt = ram16(s, RAM_METATILE), ds = ram16(s, RAM_SCRDESC);
  if (bank >= s->rom_size / 0x4000) return false;
  if (mt < 0x8000 || mt >= 0xBC00 || ds < 0x8000 || ds >= 0xBFC0) return false;
  m->s = s;
  m->mt = s->rom + bank * 0x4000 + (mt - 0x8000);
  m->desc = s->rom + bank * 0x4000 + (ds - 0x8000);
  return true;
}

// tile byte (VRAM tile = 256 + value) at world pixel (wx, wy) of the playfield
static inline uint8_t map_tile(const mapctx_t* m, int wx, int wy) {
  int col = wx >> 4, row = wy >> 4;
  uint8_t mtile = m->s->st.ram[(MAP_BASE + row * 0xC0 + col) & 0x1FFF];
  return m->mt[mtile * 4 + ((wx >> 3) & 1) + ((wy >> 3) & 1) * 2];
}

static inline uint8_t desc_stop(const mapctx_t* m, int screen) { return m->desc[screen * 4 + 3]; }

static inline int tile_px(const uint8_t* vram, int tile, int x, int y) {
  const uint8_t* t = &vram[(tile * 32 + y * 4) & 0x3FFF];
  int bit = 7 - x;
  return ((t[0] >> bit) & 1) | (((t[1] >> bit) & 1) << 1) | (((t[2] >> bit) & 1) << 2) | (((t[3] >> bit) & 1) << 3);
}

static bool detect_playfield(const sms_t* s, const mapctx_t* m, int* cam_out) {
  uint8_t hs = s->line_hscroll[GAME_Y0];
  for (int y = GAME_Y0; y < SMS_H; y++) {
    if (!s->line_display[y] || s->line_hscroll[y] != hs || s->line_vscroll[y] != 0) return false;
  }
  int cam = ram16(s, RAM_CAM);
  // the picture on screen may lag the variable by a frame: rebuild from the scroll actually shown
  int8_t diff = (int8_t)((uint8_t)(-hs) - (uint8_t)cam);
  cam += diff;
  if (cam < 0 || cam + SMS_W > MAP_W * 16) return false;
  const uint8_t* vram = s->st.vdp.vram;
  uint16_t nt = (s->st.vdp.reg[2] & 0x0E) << 10;
  int ok = 0, total = 0;
  for (int sx = 40; sx < SMS_W; sx += 72) {
    int wx = cam + sx;
    for (int r = 0; r < MAP_ROWS * 2; r++) {
      const uint8_t* e = &vram[nt + ((r + 4) * 32 + ((wx >> 3) & 31)) * 2];
      total++;
      if (e[0] == map_tile(m, wx, r * 8) && (e[1] & 1)) ok++;
    }
  }
  *cam_out = cam;
  return ok * 10 >= total * 9;
}

void mod_render(const sms_t* s, uint32_t* out, int pitch, int ext, mod_wide_info_t* info) {
  mod_wide_info_t local;
  if (!info) info = &local;
  memset(info, 0, sizeof *info);
  const int W = SMS_W + 2 * ext;
  mapctx_t m;
  int cam = 0;
  bool valid = ext > 0 && map_ctx(s, &m) && detect_playfield(s, &m, &cam);
  int left = 0, right = 0;
  if (valid) {
    int sc = cam >> 8;
    int s0 = sc;
    while (s0 > 0 && desc_stop(&m, s0 - 1) == 0) s0--;
    int s1 = sc;
    while (s1 < 11 && desc_stop(&m, s1) == 0) s1++;
    left = s0 * 256;
    right = (s1 + 1) * 256;
    info->map_valid = true;
    info->cam = cam;
    info->left_limit = left;
    info->right_limit = right;
  }
  const uint8_t* vram = s->st.vdp.vram;
  for (int y = 0; y < SMS_H; y++) {
    uint32_t* row = out + y * pitch;
    uint32_t pal[32];
    for (int i = 0; i < 32; i++) pal[i] = sms_color(s->line_cram[y][i]);
    const uint8_t* src = s->fb_idx[y];
    for (int x = 0; x < SMS_W; x++) row[ext + x] = pal[src[x]];
    if (!ext) continue;
    bool fill = valid && y >= GAME_Y0;
    bool left_blank = (s->line_reg0[y] & 0x20) != 0;
    for (int x = 0; x < W; x++) {
      bool in_center = x >= ext && x < ext + SMS_W;
      if (in_center && !(left_blank && x < ext + 8 && fill)) {
        continue;
      }
      uint32_t c = 0xFF000000u;
      if (fill) {
        int wx = cam + (x - ext);
        int wy = y - GAME_Y0;
        if (wx >= left && wx < right && wx >= 0 && wx < MAP_W * 16) {
          int t = 256 + map_tile(&m, wx, wy);
          c = pal[tile_px(vram, t, wx & 7, wy & 7)];
        }
      }
      row[x] = c;
    }
    // sprite pixels that spill past the original right edge
    for (int i = 0; i < SMS_SPR_OVER && ext + SMS_W + i < W; i++) {
      if (s->spr_over[y][i] && fill) row[ext + SMS_W + i] = pal[s->spr_over[y][i]];
    }
  }
}
