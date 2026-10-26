// Widescreen gameplay for Master of Darkness.
//
// The game keeps enemies with a 16-bit screen X (slot+4/+5) but:
//  - spawns them when the camera reaches an exact position, at a screen X inside 0..255;
//  - frees them in its sprite pass when X < -17 or X > 255;
//  - only sends sprite pieces with X in 0..255 to the VDP.
// In 16:9 we spawn them D pixels earlier (and D pixels further right, so the world
// position is the same), keep them alive D pixels further on each side, and capture
// the sprite pieces the game discards so the renderer can draw them in the side areas.
// Nothing changes in 4:3 mode.
#include "mod_enhance.h"
#include <string.h>

static mod_enhance_t* g;
static uint8_t hook_map[0x8000];

static inline uint8_t ram8(sms_t* s, uint16_t a) { return s->st.ram[a & 0x1FFF]; }
static inline void ram_w(sms_t* s, uint16_t a, uint8_t v) { s->st.ram[a & 0x1FFF] = v; }

// ---------------------------------------------------------------- sprite capture
// Same piece walk as the game's sprite builder (0x0DD1), without the X limits.
static void capture(sms_t* s, uint16_t obj) {
  if (!g->wide) return;
  int y16 = (int16_t)(ram8(s, obj + 2) | (ram8(s, obj + 3) << 8));
  int x16 = (int16_t)(ram8(s, obj + 4) | (ram8(s, obj + 5) << 8));
  uint16_t fp = ram8(s, obj + 6) | (ram8(s, obj + 7) << 8);
  int count = sms_peek(s, fp);
  for (int i = 0; i < count && g->n_building < MOD_MAX_PIECES; i++) {
    uint16_t p = fp + 1 + i * 3;
    int py = (uint16_t)(y16 + (int8_t)sms_peek(s, p));
    int px = x16 + (int8_t)sms_peek(s, p + 1);
    if (py >= 0xD1) continue;  // same vertical rule as the game (16-bit compare)
    mod_piece_t* pc = &g->building[g->n_building++];
    pc->x = (int16_t)px;
    pc->y = (int16_t)(py - 0x11);
    pc->tile = sms_peek(s, p + 2);
    pc->in_sat = px >= 0 && px <= 255;
  }
}

static bool in_wide_range(int x16) { return x16 >= -17 - g->ext - 8 && x16 <= 255 + g->ext; }

// ---------------------------------------------------------------- spawn lists
typedef struct {
  uint16_t a_hi, a_hi_next;   // LD A,(C6A1)
  uint16_t a_lo, a_lo_next;   // LD A,(C6A0) in the scan loop
  uint16_t a_lo2, a_lo2_next; // LD A,(C6A0) after an entry
  uint16_t x_hi, x_hi_next;   // LD (IX+5),0 when an object is created
  uint16_t end;               // POP AF after the loop
} spawn_addr_t;

static const spawn_addr_t spawn_addrs[2] = {
  {0x3CA6, 0x3CA9, 0x3CB3, 0x3CB6, 0x3CD6, 0x3CD9, 0x3CEE, 0x3CF2, 0x3CBE},
  {0x52BD, 0x52C0, 0x52CA, 0x52CD, 0x52ED, 0x52F0, 0x5305, 0x5309, 0x52D5},
};

static int cam16(sms_t* s) { return ram8(s, 0xC6A0) | (ram8(s, 0xC6A1) << 8); }

// stop flag of a screen (byte 3 of its descriptor), see mod_wide.c
static int screen_stop(sms_t* s, int screen) {
  int bank = ram8(s, 0xC6A2);
  uint16_t ds = ram8(s, 0xC6A8) | (ram8(s, 0xC6A9) << 8);
  if (bank >= s->rom_size / 0x4000 || ds < 0x8000 || ds >= 0xBFC0) return 1;
  return s->rom[bank * 0x4000 + (ds - 0x8000) + screen * 4 + 3];
}

static bool spawn_value_valid(sms_t* s, int v, int cam) {
  int sv = v >> 8, sc = cam >> 8;
  if (sv >= 12) return false;
  if (sv == sc) return true;
  return sv == sc + 1 && screen_stop(s, sc) == 0;
}

static bool spawn_hook(sms_t* s, int k, uint16_t pc) {
  const spawn_addr_t* a = &spawn_addrs[k];
  z80* z = &s->st.cpu;
  typeof(g->spawn[0])* st = &g->spawn[k];
  if (!g->wide) { st->active = false; st->last_cam = -1000; return false; }
  int cam = cam16(s);
  if (pc == a->a_hi) {
    if (!st->active) {
      if (cam == st->last_cam + 1) { st->next = st->last = cam + g->ext; }
      else { st->next = cam; st->last = cam + g->ext; }
      st->last_cam = cam;
      st->active = true;
      st->list_base = (z->h << 8) | z->l;  // table base used by RST 28 below
    }
    while (st->next <= st->last && !spawn_value_valid(s, st->next, cam)) st->next++;
    if (st->next > st->last) {  // nothing (valid) to scan: finish through the normal exit
      st->active = false;
      z->pc = a->end;
      z->cyc += 13;
      return true;
    }
    z->a = st->next >> 8;
    z->pc = a->a_hi_next;
    z->cyc += 13;
    return true;
  }
  if (!st->active) return false;
  if (pc == a->a_lo || pc == a->a_lo2) {
    z->a = st->next & 0xFF;
    z->pc = pc == a->a_lo ? a->a_lo_next : a->a_lo2_next;
    z->cyc += 13;
    return true;
  }
  if (pc == a->x_hi) {
    uint16_t obj = z->ix;
    int x = ram8(s, obj + 4) + (st->next - cam);
    ram_w(s, obj + 4, x & 0xFF);
    ram_w(s, obj + 5, (x >> 8) & 0xFF);
    z->pc = a->x_hi_next;
    z->cyc += 19;
    return true;
  }
  if (pc == a->end) {
    st->next++;
    if (st->next <= st->last) {  // scan the next camera value
      z->h = st->list_base >> 8;
      z->l = st->list_base & 0xFF;
      z->pc = a->a_hi;
      return true;
    }
    st->active = false;
    return false;  // run POP AF / RET normally
  }
  return false;
}

// ---------------------------------------------------------------- hook entry
static bool on_hook(sms_t* s, uint16_t pc) {
  z80* z = &s->st.cpu;
  switch (pc) {
  case 0x51B6:  // start of the sprite pass
    g->n_building = 0;
    return false;
  case 0x5228: {  // end of the sprite pass: store the completed list
    int h = (g->pass_head + 1) & 3;
    memcpy(g->pass[h], g->building, sizeof(mod_piece_t) * g->n_building);
    g->n_pass[h] = g->n_building;
    g->pass_head = h;
    return false;
  }
  case 0x0DD1:  // sprite builder: observe
    capture(s, z->ix);
    return false;
  case 0x520E: {  // second object group, off the original screen: observe
    int x16 = (int16_t)(ram8(s, z->ix + 4) | (ram8(s, z->ix + 5) << 8));
    if (in_wide_range(x16)) capture(s, z->ix);
    return false;
  }
  case 0x51D5: {  // first object group: keep-alive / draw decision
    if (!g->wide) return false;
    int x16 = (int16_t)(ram8(s, z->ix + 4) | (ram8(s, z->ix + 5) << 8));
    if (x16 >= 0 && x16 <= 255) { z->a = 0; z->pc = 0x51E5; z->cyc += 35; }
    else if (x16 >= -17 && x16 < 0) { z->a = (0x11 + (x16 & 0xFF)) & 0xFF; z->pc = 0x51E5; z->cyc += 74; }
    else if (in_wide_range(x16)) { capture(s, z->ix); z->pc = 0x51E9; z->cyc += 35; }
    else { z->pc = 0x5238; z->cyc += 35; }
    return true;
  }
  }
  for (int k = 0; k < 2; k++) {
    const spawn_addr_t* a = &spawn_addrs[k];
    if (pc == a->a_hi || pc == a->a_lo || pc == a->a_lo2 || pc == a->x_hi || pc == a->end)
      return spawn_hook(s, k, pc);
  }
  return false;
}

// Bytes expected at the patched addresses: if the ROM differs (another version or a
// hack that moved code), the enhancements stay off and the game runs untouched.
static const struct { uint16_t addr; uint8_t b[4]; } signature[] = {
  {0x51B6, {0xCD, 0x42, 0x52, 0x21}}, {0x0DD1, {0xE5, 0xD5, 0xC5, 0xDD}}, {0x520E, {0xD9, 0xDD, 0x6E, 0x04}},
  {0x51D5, {0xDD, 0x7E, 0x05, 0xB7}}, {0x3CA6, {0x3A, 0xA1, 0xC6, 0xEF}}, {0x3CB3, {0x3A, 0xA0, 0xC6, 0xBE}},
  {0x3CD6, {0x3A, 0xA0, 0xC6, 0x11}}, {0x3CEE, {0xDD, 0x36, 0x05, 0x00}}, {0x3CBE, {0xF1, 0x32, 0xFF, 0xFF}},
  {0x52BD, {0x3A, 0xA1, 0xC6, 0xEF}}, {0x52CA, {0x3A, 0xA0, 0xC6, 0xBE}}, {0x52ED, {0x3A, 0xA0, 0xC6, 0x11}},
  {0x5305, {0xDD, 0x36, 0x05, 0x00}}, {0x52D5, {0xF1, 0x32, 0xFF, 0xFF}}, {0x51E5, {0x79, 0xCD, 0xD1, 0x0D}},
  {0x51E9, {0x19, 0x10, 0xD5, 0x21}}, {0x5238, {0x36, 0x00, 0xC3, 0xE9}},
  {0x5228, {0x26, 0xC3, 0x69, 0x3E}},
};

bool mod_enhance_compatible(const sms_t* s) {
  if (s->rom_size < 0x8000) return false;
  for (unsigned i = 0; i < sizeof signature / sizeof signature[0]; i++)
    if (memcmp(s->rom + signature[i].addr, signature[i].b, 4)) return false;
  return true;
}

void mod_enhance_attach(sms_t* s, mod_enhance_t* e) {
  g = e;
  if (!mod_enhance_compatible(s)) { s->hook_map = NULL; return; }
  memset(hook_map, 0, sizeof hook_map);
  static const uint16_t addrs[] = {0x51B6, 0x5228, 0x0DD1, 0x520E, 0x51D5};
  for (unsigned i = 0; i < sizeof addrs / sizeof addrs[0]; i++) hook_map[addrs[i]] = 1;
  for (int k = 0; k < 2; k++) {
    const spawn_addr_t* a = &spawn_addrs[k];
    hook_map[a->a_hi] = hook_map[a->a_lo] = hook_map[a->a_lo2] = hook_map[a->x_hi] = hook_map[a->end] = 1;
    e->spawn[k].last_cam = -1000;
    e->spawn[k].active = false;
  }
  s->hook_map = hook_map;
  s->pc_hook = on_hook;
}

void mod_enhance_set_wide(mod_enhance_t* e, bool wide, int ext) {
  e->wide = wide;
  e->ext = ext;
  if (!wide) {
    e->n_building = e->n_shown = 0;
    for (int i = 0; i < 4; i++) e->n_pass[i] = 0;
  }
}

// How many pieces of a pass are present in the sprite table the VDP displayed.
static int pass_matches(const sms_t* s, const mod_piece_t* p, int n, int* total) {
  int ok = 0;
  *total = 0;
  for (int k = 0; k < n; k++) {
    if (!p[k].in_sat) continue;
    (*total)++;
    for (int j = 0; j < 64; j++) {
      if (s->sat_mid[j] == (uint8_t)p[k].y && s->sat_mid[0x80 + j * 2] == (uint8_t)p[k].x &&
          s->sat_mid[0x81 + j * 2] == p[k].tile) { ok++; break; }
    }
  }
  return ok;
}

void mod_enhance_frame(const sms_t* s, mod_enhance_t* e) {
  e->shown = NULL;
  e->n_shown = 0;
  if (!e->wide) return;
  int best = -1, best_score = -1;
  for (int age = 0; age < 4; age++) {
    int i = (e->pass_head - age) & 3, total;
    int ok = pass_matches(s, e->pass[i], e->n_pass[i], &total);
    if (total == 0) continue;
    int score = ok * 1000 / total * 4 - age;  // fraction matched, newest wins ties
    if (score > best_score) { best_score = score; best = age; }
  }
  if (best < 0) best = e->last_age;
  e->last_age = best;
  int i = (e->pass_head - best) & 3;
  e->shown = e->pass[i];
  e->n_shown = e->n_pass[i];
}
