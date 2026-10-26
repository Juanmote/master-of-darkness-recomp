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

static inline bool hidden_column_prio(bool left_blank, int sx) { return left_blank && sx >= 0 && sx < 8; }

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

void mod_render(const sms_t* s, uint32_t* out, int pitch, int ext, mod_wide_info_t* info,
                const mod_piece_t* pieces, int n_pieces) {
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
  // Tiles the game animates straight in the name table (water...) differ from the RAM map.
  // Learn "map tile -> tile on screen" from the visible area and apply it to the sides.
  int16_t remap[256];
  for (int i = 0; i < 256; i++) remap[i] = -1;
  int view = cam - ext;   // world X shown at the left edge of the playfield lines
  if (valid) {
    uint16_t nt = (s->st.vdp.reg[2] & 0x0E) << 10;
    for (int r = 0; r < MAP_ROWS * 2; r++)
      for (int wx = (cam + 8 + 7) & ~7; wx + 8 <= cam + SMS_W; wx += 8) {
        uint8_t e = map_tile(&m, wx, r * 8);
        const uint8_t* ne = &vram[nt + ((r + 4) * 32 + ((wx >> 3) & 31)) * 2];
        if (!(ne[1] & 1)) continue;
        if (remap[e] == -1) remap[e] = ne[0];
        else if (remap[e] != ne[0]) remap[e] = -2;   // not a global swap: leave it alone
      }
    // keep the wide view inside the current section instead of showing black
    if (right - left >= W) {
      if (view < left) view = left;
      if (view + W > right) view = right - W;
    }
    info->cam = cam;
  }
  int cx = valid ? cam - view : ext;  // where the original picture sits in playfield lines
  info->cx = cx;
  for (int y = 0; y < SMS_H; y++) {
    uint32_t* row = out + y * pitch;
    uint32_t pal[32];
    for (int i = 0; i < 32; i++) pal[i] = sms_color(s->line_cram[y][i]);
    const uint8_t* src = s->fb_idx[y];
    bool fill = valid && y >= GAME_Y0;
    bool hud = valid && y < GAME_Y0;
    int off = fill ? cx : ext;
    if (ext) for (int x = 0; x < W; x++) row[x] = 0xFF000000u;
    if (hud && cx != ext) {
      // status bar stays centred, sprites crossing into it follow the playfield
      for (int x = 0; x < SMS_W; x++) row[ext + x] = pal[s->fb_bg[y][x]];
      for (int x = 0; x < SMS_W; x++)
        if (s->fb_spr[y][x]) row[cx + x] = pal[src[x]];
    } else {
      for (int x = 0; x < SMS_W; x++) row[off + x] = pal[src[x]];
    }
    if (!ext || !valid) continue;
    if (hud) off = cx;
    bool left_blank = (s->line_reg0[y] & 0x20) != 0;
    int wy = y - GAME_Y0;
    for (int x = 0; fill && x < W; x++) {
      bool in_center = x >= off && x < off + SMS_W;
      if (in_center && !(left_blank && x < off + 8)) continue;
      int wx = view + x;
      // outside the current section (one-screen rooms, section ends): keep repeating the
      // outermost 16-pixel column, so walls and pillars simply continue
      if (wx < left) wx = left + (((wx - left) % 16) + 16) % 16;
      else if (wx >= right) wx = right - 16 + (wx - right) % 16;
      int e = map_tile(&m, wx, wy);
      int t = 256 + (remap[e] >= 0 ? remap[e] : e);
      row[x] = pal[tile_px(vram, t, wx & 7, wy & 7)];
    }
    // sprites the game keeps outside its 256-pixel picture (captured by mod_enhance)
    if (n_pieces) {
      const sms_vdp_t* d = &s->st.vdp;
      int h = (d->reg[1] & 0x02) ? 16 : 8;
      uint8_t drawn[1024];
      memset(drawn, 0, (size_t)W);
      for (int i = 0; i < n_pieces; i++) {
        const mod_piece_t* pc = &pieces[i];
        int top = pc->y + 1;
        if (pc->y >= 0xD0 || y < top || y >= top + h) continue;
        int tile = pc->tile | ((d->reg[6] & 0x04) ? 0x100 : 0);
        if (h == 16) tile &= ~1;
        int r = y - top;
        for (int px = 0; px < 8; px++) {
          int sx = pc->x + px;           // position in the original picture
          int X = off + sx;              // position in the wide picture
          if (X < 0 || X >= W || drawn[X]) continue;
          bool center = sx >= 0 && sx < SMS_W;
          if (center) {
            bool hidden_column = left_blank && sx < 8;
            bool missing = !pc->in_sat && !s->fb_spr[y][sx];
            if (!hidden_column && !missing) continue;
            if (hud && !hidden_column) continue;  // over the status bar: only what the game drew
          }
          int c = tile_px(vram, tile, px, r);
          if (!c) continue;
          drawn[X] = 1;
          // background tiles with priority stay in front (side areas only)
          if (fill && (!center || hidden_column_prio(left_blank, sx))) {
            int wx = view + X;
            if (wx < left) wx = left + (((wx - left) % 16) + 16) % 16;
            else if (wx >= right) wx = right - 16 + (wx - right) % 16;
            int e = map_tile(&m, wx, wy);
            int t = remap[e] >= 0 ? remap[e] : e;
            if (t >= ram8(s, 0xC6A6) && tile_px(vram, 256 + t, wx & 7, wy & 7)) continue;
          }
          row[X] = pal[16 + c];
        }
      }
    } else if (fill) {
      // sprite pixels that spill past the original right edge
      for (int i = 0; i < SMS_SPR_OVER && off + SMS_W + i < W; i++) {
        if (s->spr_over[y][i]) row[off + SMS_W + i] = pal[s->spr_over[y][i]];
      }
    }
  }
}

bool mod_is_gameplay(const sms_t* s) {
  mapctx_t m; int cam;
  return map_ctx(s, &m) && detect_playfield(s, &m, &cam);
}

// Debug: marks playfield pixels where the VDP background differs from the RAM map.
// Returns number of mismatching pixels (sprite pixels are ignored). mask: SMS_H*SMS_W bytes.
int mod_debug_compare(const sms_t* s, uint8_t* mask) {
  mapctx_t m; int cam;
  memset(mask, 0, SMS_W * SMS_H);
  if (!map_ctx(s, &m) || !detect_playfield(s, &m, &cam)) return -1;
  int n = 0;
  for (int y = GAME_Y0; y < SMS_H; y++)
    for (int x = 8; x < SMS_W; x++) {
      uint8_t v = s->fb_idx[y][x];
      if (v >= 16) continue;
      int wx = cam + x, wy = y - GAME_Y0;
      int t = 256 + map_tile(&m, wx, wy);
      int c = tile_px(s->st.vdp.vram, t, wx & 7, wy & 7);
      if (c != v) { mask[y * SMS_W + x] = 1; n++; }
    }
  return n;
}
