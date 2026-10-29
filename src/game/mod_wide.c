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

typedef struct {
  int cam;          // camera X of the picture on screen
  int left, right;  // world limits of the current section
  int view;         // world X at the left edge of the playfield lines
  int cx;           // where the original 256-pixel picture sits in those lines
} view_t;

static void compute_view(const mapctx_t* m, int cam, int ext, view_t* v) {
  const int W = SMS_W + 2 * ext;
  int sc = cam >> 8, s0 = sc, s1 = sc;
  while (s0 > 0 && desc_stop(m, s0 - 1) == 0) s0--;
  while (s1 < 11 && desc_stop(m, s1) == 0) s1++;
  v->cam = cam;
  v->left = s0 * 256;
  v->right = (s1 + 1) * 256;
  v->view = cam - ext;
  // keep the wide view inside the current section when it is wide enough
  if (v->right - v->left >= W) {
    if (v->view < v->left) v->view = v->left;
    if (v->view + W > v->right) v->view = v->right - W;
  }
  v->cx = cam - v->view;
}

bool mod_visible_margins(const sms_t* s, int ext, int* left_px, int* right_px) {
  mapctx_t m;
  int cam;
  if (ext <= 0 || !map_ctx(s, &m) || !detect_playfield(s, &m, &cam)) return false;
  view_t v;
  compute_view(&m, cam, ext, &v);
  const int W = SMS_W + 2 * ext;
  int l = v.cx, r = W - v.cx - SMS_W;
  if (l > cam - v.left) l = cam - v.left;                 // nothing playable beyond the limits
  if (r > v.right - (cam + SMS_W)) r = v.right - (cam + SMS_W);
  *left_px = l < 0 ? 0 : l;
  *right_px = r < 0 ? 0 : r;
  return true;
}

// Cells of the current section that differ from the RAM map (broken blocks, opened
// passages, dialog boxes...) as last seen on screen: the side areas use them too.
static struct {
  uint32_t key;
  int16_t t[MAP_ROWS * 2][MAP_W * 2];
} ovr = {0xFFFFFFFFu, {{0}}};

// Last good wide picture of the playfield, reused while the game shows something
// the map cannot explain (boss effects, dialog) without moving the camera.
#define CACHE_W (SMS_W + 2 * 64)
static struct {
  bool ok;
  uint32_t key;
  int cx, W;
  int coverL, coverR;            // edge columns painted over inside the picture
  uint8_t idx[SMS_H][CACHE_W];   // palette indices (background only), so palette effects still apply
  uint16_t hud[4 * 32];          // name table rows of the status bar
} cache;

static void read_hud(const sms_t* s, uint16_t* out) {
  uint16_t nt = (s->st.vdp.reg[2] & 0x0E) << 10;
  for (int i = 0; i < 4 * 32; i++) {
    const uint8_t* e = &s->st.vdp.vram[nt + i * 2];
    out[i] = e[0] | (e[1] << 8);
  }
}

// The status bar is still the one of the cached picture (not the ending, a story screen...)
static bool same_hud(const sms_t* s) {
  uint16_t h[4 * 32];
  read_hud(s, h);
  int ok = 0;
  for (int i = 0; i < 4 * 32; i++) ok += h[i] == cache.hud[i];
  return ok * 4 >= 4 * 32 * 3;
}

static uint32_t section_key(const sms_t* s) {
  return ((uint32_t)ram8(s, 0xDFC0) << 24) | ((uint32_t)ram8(s, 0xDFC1) << 16) |
         ((uint32_t)ram8(s, RAM_MAPBANK) << 8) | (uint8_t)(ram8(s, RAM_METATILE) ^ ram8(s, RAM_SCRDESC) ^ ram8(s, RAM_SCRDESC + 1));
}

static uint32_t scene_key(const sms_t* s) {
  return ((uint32_t)ram16(s, RAM_CAM) << 16) | ((uint32_t)ram8(s, 0xDFC0) << 8) | ram8(s, 0xDFC1);
}

// Width in pixels of the pattern repeated at a section edge, per metatile row: 32 when
// the two metatiles next to the edge repeat exactly (tombs, wide facades), otherwise
// the outermost 16-pixel column, so walls stay solid.
static inline int mt_attr(const mapctx_t* m, uint8_t mtile) { return m->s->st.ram[(0xD900 + mtile) & 0x1FFF] & 3; }

static int row_period(const mapctx_t* m, int row, int edge_col, int dir) {
  const uint8_t* map = &m->s->st.ram[(MAP_BASE + row * 0xC0) & 0x1FFF];
  int c0 = edge_col, c1 = edge_col - dir, c2 = edge_col - 2 * dir, c3 = edge_col - 3 * dir;
  if (c3 < 0 || c3 >= MAP_W) return 16;
  if (map[c0] == map[c1]) return 16;                        // uniform: plain wall/sky
  // exact 2-metatile pattern of the same kind (tombs...); a wall next to an opening
  // (collision differs) must not turn into a row of holes
  if (map[c0] == map[c2] && map[c1] == map[c3] && mt_attr(m, map[c0]) == mt_attr(m, map[c1])) return 32;
  return 16;
}

// Hand-picked edges (round, section, side, edge column): what to repeat beyond them.
// src: metatile column to repeat instead of the edge one; period: metatiles per repeat;
// flip: mirror the column (mirror image of the other side); cover: also draw it over this
// many columns inside the area (background only, sprites stay in front).
static const struct { uint8_t round, section; char side; uint8_t edge, src, period, flip, cover; } edge_rules[] = {
  {0, 2, 'R', 127, 0xFF, 7, 0, 0},  // 1-3: the row of houses goes on (not the edge pillar alone)
  {0, 2, 'L', 112, 0xFF, 7, 0, 0},
  {2, 0, 'L', 112, 127, 1, 1, 1},   // 3-1 bell tower: brick wall like the right side, mirrored,
                                    // also over the sky column at the edge of the room
  {2, 2, 'L', 48, 47, 1, 0, 0},     // 3-3 dead end: the wall behind it, not an open room
  {3, 2, 'L', 176, 191, 1, 0, 1},   // 4-3 boss: wall like the right side, one column more
};

typedef struct { int base[MAP_ROWS], per[MAP_ROWS]; bool flip; int cover; } side_src_t;

static void side_sources(const sms_t* s, const mapctx_t* m, const view_t* v, side_src_t* L, side_src_t* R) {
  int round = ram8(s, 0xDFC0), section = ram8(s, 0xDFC1);
  int el = v->left >> 4, er = (v->right >> 4) - 1;
  for (int r = 0; r < MAP_ROWS; r++) {
    L->per[r] = row_period(m, r, el, -1); L->base[r] = v->left;
    R->per[r] = row_period(m, r, er, 1);  R->base[r] = v->right - R->per[r];
  }
  L->flip = R->flip = false;
  L->cover = R->cover = 0;
  for (unsigned i = 0; i < sizeof edge_rules / sizeof edge_rules[0]; i++) {
    if (edge_rules[i].round != round || edge_rules[i].section != section) continue;
    bool left = edge_rules[i].side == 'L';
    if (edge_rules[i].edge != (left ? el : er)) continue;
    side_src_t* t = left ? L : R;
    int p = edge_rules[i].period * 16;
    t->flip = edge_rules[i].flip;
    t->cover = edge_rules[i].cover * 16;
    for (int r = 0; r < MAP_ROWS; r++) {
      t->per[r] = p;
      if (edge_rules[i].src != 0xFF) t->base[r] = edge_rules[i].src * 16;
      else t->base[r] = left ? v->left : v->right - p;
    }
  }
}

static inline int pmod(int a, int p) { return ((a % p) + p) % p; }

// world x of the map cell shown at wx (beyond the area limits: the repeated pattern)
static inline int side_wx(const view_t* v, const side_src_t* L, const side_src_t* R, int wx, int wy) {
  int r = wy >> 4;
  if (wx < v->left + L->cover) {
    int o = pmod(wx - v->left, L->per[r]);
    return L->base[r] + (L->flip ? L->per[r] - 1 - o : o);
  }
  if (wx >= v->right - R->cover) {
    int o = pmod(wx - v->right, R->per[r]);
    return R->base[r] + (R->flip ? R->per[r] - 1 - o : o);
  }
  return wx;
}

static bool hero_on;
static uint8_t hero_map[16];

void mod_set_hero_colors(const uint8_t* map16) {
  hero_on = map16 != NULL;
  if (map16) memcpy(hero_map, map16, 16);
}

static inline bool hero_tile(int t) { return t >= 0x01 && t <= 0x1F; }

// colour of a picture pixel (CRAM index ci); hero sprite pixels may be recoloured
static inline uint32_t px_color(const sms_t* s, const uint32_t* pal, int y, int x, int ci) {
  if (hero_on && ci >= 16 && s->fb_spr[y][x] && hero_tile(s->fb_sprtile[y][x]) && hero_map[ci - 16] != 0xFF)
    return sms_color(hero_map[ci - 16]);
  return pal[ci];
}

void mod_render(const sms_t* s, uint32_t* out, int pitch, int ext, mod_wide_info_t* info,
                const mod_piece_t* pieces, int n_pieces) {
  mod_wide_info_t local;
  if (!info) info = &local;
  memset(info, 0, sizeof *info);
  const int W = SMS_W + 2 * ext;
  const uint8_t* vram = s->st.vdp.vram;
  mapctx_t m;
  int cam = 0;
  bool valid = ext > 0 && W <= CACHE_W && map_ctx(s, &m) && detect_playfield(s, &m, &cam);
  view_t v = {0, 0, 0, -ext, ext};
  int16_t remap[256];
  for (int i = 0; i < 256; i++) remap[i] = -1;
  bool from_cache = false;
  if (valid) {
    compute_view(&m, cam, ext, &v);
    info->map_valid = true;
    info->cam = cam;
    info->left_limit = v.left;
    info->right_limit = v.right;
    // Tiles the game animates straight in the name table (water...) differ from the map:
    // learn "map tile -> tile on screen" from the visible area and apply it to the sides.
    uint16_t nt = (s->st.vdp.reg[2] & 0x0E) << 10;
    for (int r = 0; r < MAP_ROWS * 2; r++)
      for (int wx = (cam + 8 + 7) & ~7; wx + 8 <= cam + SMS_W; wx += 8) {
        uint8_t e = map_tile(&m, wx, r * 8);
        const uint8_t* ne = &vram[nt + ((r + 4) * 32 + ((wx >> 3) & 31)) * 2];
        if (!(ne[1] & 1)) continue;
        if (remap[e] == -1) remap[e] = ne[0];
        else if (remap[e] != ne[0]) remap[e] = -2;   // not a global swap
      }
    // remember cells that differ from the map in this section
    uint32_t key = section_key(s);
    if (key != ovr.key) {
      ovr.key = key;
      for (int r = 0; r < MAP_ROWS * 2; r++) for (int c = 0; c < MAP_W * 2; c++) ovr.t[r][c] = -1;
    }
    for (int r = 0; r < MAP_ROWS * 2; r++)
      for (int wx = (cam + 8 + 7) & ~7; wx + 8 <= cam + SMS_W; wx += 8) {
        uint8_t e = map_tile(&m, wx, r * 8);
        const uint8_t* ne = &vram[nt + ((r + 4) * 32 + ((wx >> 3) & 31)) * 2];
        int shown = remap[e] >= 0 ? remap[e] : e;
        ovr.t[r][wx >> 3] = ((ne[1] & 1) && ne[0] != shown && remap[e] != -2) ? ne[0] : -1;
      }
  } else if (ext > 0 && cache.ok && cache.W == W && cache.key == scene_key(s) && s->line_display[100] &&
             same_hud(s)) {
    from_cache = true;
    v.cx = cache.cx;
  }
  info->cx = v.cx;
  const int cx = v.cx;
  side_src_t srcL, srcR;
  if (valid) side_sources(s, &m, &v, &srcL, &srcR);
  const sms_vdp_t* d = &s->st.vdp;
  const int sh = (d->reg[1] & 0x02) ? 16 : 8;
  for (int y = 0; y < SMS_H; y++) {
    uint32_t* row = out + y * pitch;
    uint32_t pal[32];
    for (int i = 0; i < 32; i++) pal[i] = sms_color(s->line_cram[y][i]);
    const uint8_t* src = s->fb_idx[y];
    bool scene = valid || from_cache;
    bool fill = scene && y >= GAME_Y0;
    bool hud = scene && y < GAME_Y0;
    bool left_blank = (s->line_reg0[y] & 0x20) != 0;
    int off = fill ? cx : ext;
    if (ext) for (int x = 0; x < W; x++) row[x] = 0xFF000000u;
    if (hud && cx != ext) {
      // status bar stays centred, sprites crossing into it follow the playfield
      for (int x = 0; x < SMS_W; x++) row[ext + x] = pal[s->fb_bg[y][x]];
      for (int x = 0; x < SMS_W; x++)
        if (s->fb_spr[y][x]) row[cx + x] = px_color(s, pal, y, x, src[x]);
    } else if (fill && from_cache) {
      // The map does not explain the picture (boss effects, dialog boxes): per side, keep
      // the last good side area while the picture next to it is unchanged, otherwise
      // continue what is at the edge of the picture now (a darkened room, a new floor).
      const uint8_t* bg = s->fb_bg[y];
      const uint8_t* ci = cache.idx[y];
      bool keepL = true, keepR = true;
      int l0 = cache.coverL > 8 ? cache.coverL : 8, r1 = SMS_W - cache.coverR;
      for (int x = l0; x < l0 + 16; x++) if (bg[x] != ci[cx + x]) { keepL = false; break; }
      for (int x = r1 - 16; x < r1; x++) if (bg[x] != ci[cx + x]) { keepR = false; break; }
      // continue only what repeats every 16 pixels in this band of 8 lines (floor, wall,
      // plain colour); anything else (a big boss drawn with background tiles) is not
      // copied: the side shows the backdrop colour there
      int b0 = GAME_Y0 + ((y - GAME_Y0) & ~7);
      bool perL = true, perR = true;
      for (int yy = b0; yy < b0 + 8 && yy < SMS_H; yy++) {
        const uint8_t* bb = s->fb_bg[yy];
        for (int x = 8; x < 24 && perL; x++) if (bb[x] != bb[x + 16]) perL = false;
        for (int x = SMS_W - 16; x < SMS_W && perR; x++) if (bb[x] != bb[x - 16]) perR = false;
      }
      uint32_t back = pal[16 + (d->reg[7] & 15)];
      for (int X = 0; X < cx; X++)
        row[X] = keepL ? pal[ci[X]] : perL ? pal[bg[8 + pmod(X - cx - 8, 16)]] : back;
      for (int X = cx + SMS_W; X < W; X++)
        row[X] = keepR ? pal[ci[X]] : perR ? pal[bg[SMS_W - 16 + pmod(X - cx - SMS_W, 16)]] : back;
      for (int x = left_blank ? 8 : 0; x < SMS_W; x++) row[cx + x] = px_color(s, pal, y, x, src[x]);
      if (left_blank) for (int x = 0; x < 8; x++) row[cx + x] = keepL ? pal[ci[cx + x]] : perL ? pal[bg[8 + pmod(x - 8, 16)]] : back;
      if (keepL) for (int x = 0; x < cache.coverL; x++) if (!s->fb_spr[y][x]) row[cx + x] = pal[ci[cx + x]];
      if (keepR) for (int x = SMS_W - cache.coverR; x < SMS_W; x++) if (!s->fb_spr[y][x]) row[cx + x] = pal[ci[cx + x]];
    } else {
      for (int x = 0; x < SMS_W; x++) row[off + x] = px_color(s, pal, y, x, src[x]);
    }
    if (!ext || !valid) continue;
    if (hud) off = cx;
    int wy = y - GAME_Y0;
    uint8_t* cidx = cache.idx[y];
    // side areas from the map
    for (int x = 0; fill && x < W; x++) {
      bool in_center = x >= off && x < off + SMS_W;
      int vx = v.view + x;
      bool covered = (vx >= v.left && vx < v.left + srcL.cover) || (vx < v.right && vx >= v.right - srcR.cover);
      if (in_center && covered && s->fb_spr[y][x - off]) { cidx[x] = s->fb_bg[y][x - off]; continue; }
      if (in_center && !covered && !(left_blank && x < off + 8)) { cidx[x] = s->fb_bg[y][x - off]; continue; }
      // outside the section (one-screen rooms, section ends): keep repeating the
      // pattern found at the edge, so walls, tombs or pillars simply continue
      int wx = side_wx(&v, &srcL, &srcR, v.view + x, wy);
      int o = ovr.t[wy >> 3][wx >> 3];
      int e = map_tile(&m, wx, wy);
      int t = 256 + (o >= 0 ? o : remap[e] >= 0 ? remap[e] : e);
      int c = tile_px(vram, t, wx & 7, wy & 7);
      row[x] = pal[c];
      cidx[x] = (uint8_t)c;
    }
    // sprites the game keeps outside its 256-pixel picture (captured by mod_enhance)
    if (n_pieces) {
      uint8_t drawn[CACHE_W];
      memset(drawn, 0, (size_t)W);
      for (int i = 0; i < n_pieces; i++) {
        const mod_piece_t* pc = &pieces[i];
        int top = pc->y + 1;
        if (pc->y >= 0xD0 || y < top || y >= top + sh) continue;
        int tile = pc->tile | ((d->reg[6] & 0x04) ? 0x100 : 0);
        if (sh == 16) tile &= ~1;
        int r = y - top;
        for (int px = 0; px < 8; px++) {
          int sx = pc->x + px;           // position in the original picture
          int X = off + sx;              // position in the wide picture
          if (X < 0 || X >= W || drawn[X]) continue;
          int wx = v.view + X;
          // beyond the area limits: only objects that came from inside (leaving the room),
          // not objects of the next area
          if ((wx < v.left || wx >= v.right) && !pc->keep) continue;
          bool center = sx >= 0 && sx < SMS_W;
          bool hidden_column = left_blank && sx >= 0 && sx < 8;
          if (center) {
            bool missing = !pc->in_sat && !s->fb_spr[y][sx];
            if (!hidden_column && !missing) continue;
            if (hud && !hidden_column && pc->in_sat) continue;
          }
          int c = tile_px(vram, tile, px, r);
          if (!c) continue;
          drawn[X] = 1;
          // background tiles with priority stay in front (side areas only)
          if (fill && (!center || hidden_column)) {
            int sw = side_wx(&v, &srcL, &srcR, wx, wy);
            int o = ovr.t[wy >> 3][sw >> 3];
            int e = map_tile(&m, sw, wy);
            int t = o >= 0 ? o : remap[e] >= 0 ? remap[e] : e;
            if (t >= ram8(s, 0xC6A6) && tile_px(vram, 256 + t, sw & 7, wy & 7)) continue;
          }
          row[X] = (hero_on && hero_tile(pc->tile) && hero_map[c] != 0xFF) ? sms_color(hero_map[c]) : pal[16 + c];
        }
      }
    } else if (fill) {
      // sprite pixels that spill past the original right edge
      for (int i = 0; i < SMS_SPR_OVER && off + SMS_W + i < W; i++) {
        int wx = v.view + off + SMS_W + i;
        if (s->spr_over[y][i] && wx < v.right) row[off + SMS_W + i] = pal[s->spr_over[y][i]];
      }
    }
  }
  if (valid) {
    cache.ok = true;
    cache.key = scene_key(s);
    cache.cx = cx;
    cache.W = W;
    cache.coverL = srcL.cover;
    cache.coverR = srcR.cover;
    read_hud(s, cache.hud);
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
