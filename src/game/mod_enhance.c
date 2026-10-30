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
#include "mod_wide.h"
#include <string.h>

static mod_enhance_t* g;
static uint8_t hook_map[0x8000];
static uint8_t ms_hook_map[0x8000];

static inline uint8_t ram8(sms_t* s, uint16_t a) { return s->st.ram[a & 0x1FFF]; }
static inline void ram_w(sms_t* s, uint16_t a, uint8_t v) { s->st.ram[a & 0x1FFF] = v; }

// ---------------------------------------------------------------- sprite capture
// Same piece walk as the game's sprite builder (0x0DD1), without the X limits.
static int slot_of(uint16_t obj) {
  if (obj < 0xCA40 || obj >= 0xCA40 + 46 * 0x20 || (obj & 0x1F)) return -1;
  return (obj - 0xCA40) >> 5;
}

static void capture(sms_t* s, uint16_t obj, bool sent) {
  if (!g->wide) return;
  int slot = slot_of(obj);
  int y16 = (int16_t)(ram8(s, obj + 2) | (ram8(s, obj + 3) << 8));
  int x16 = (int16_t)(ram8(s, obj + 4) | (ram8(s, obj + 5) << 8));
  uint16_t fp = ram8(s, obj + 6) | (ram8(s, obj + 7) << 8);
  int count = sms_peek(s, fp);
  if (slot >= 0 && x16 >= 0 && x16 <= 255) g->seen_in[slot] = 1;
  uint8_t keep = slot >= 0 ? g->seen_in[slot] : 0;
  for (int i = 0; i < count && g->n_building < MOD_MAX_PIECES; i++) {
    uint16_t p = fp + 1 + i * 3;
    int py = (uint16_t)(y16 + (int8_t)sms_peek(s, p));
    int px = x16 + (int8_t)sms_peek(s, p + 1);
    if (py >= 0xD1) continue;  // same vertical rule as the game (16-bit compare)
    mod_piece_t* pc = &g->building[g->n_building++];
    pc->x = (int16_t)px;
    pc->y = (int16_t)(py - 0x11);
    pc->tile = sms_peek(s, p + 2);
    pc->in_sat = sent && px >= 0 && px <= 255;  // only if the game really sent it to the VDP
    pc->keep = keep;
  }
}

// Objects that have been inside the original picture may leave it over the side areas even
// where nothing playable is shown there (one-screen rooms): they fly out of view instead of
// vanishing at the edge of the original picture.
static bool in_wide_range(uint16_t obj, int x16) {
  int slot = slot_of(obj);
  bool out = slot >= 0 && g->seen_in[slot];
  int ml = out ? g->ext : g->margin_l, mr = out ? g->ext : g->margin_r;
  return x16 >= -17 - ml - 8 && x16 <= 255 + mr;
}

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
  if (v < 0) return false;
  int sv = v >> 8, sc = cam >> 8;
  if (sv >= 12) return false;
  if (sv == sc) return true;
  if (sv == sc + 1) return screen_stop(s, sc) == 0;   // camera can scroll right into it
  if (sv == sc - 1) return screen_stop(s, sv) == 0;   // camera can scroll left into it
  return false;
}

// Each list scan normally checks the single camera value the game has just reached.
// Moving right (lists C905/C90A) we check the value D pixels ahead and create the object
// D pixels further right; moving left (lists C907/C90C) D pixels behind and further left.
// After a jump of the camera (new section, change of direction) the whole range between
// the real camera and the shifted one is checked once.
static bool spawn_hook(sms_t* s, int k, uint16_t pc) {
  const spawn_addr_t* a = &spawn_addrs[k];
  z80* z = &s->st.cpu;
  typeof(g->spawn[0])* st = &g->spawn[k];
  if (!g->wide) { st->active = false; st->edge_ok_r = st->edge_ok_l = false; return false; }
  int cam = cam16(s);
  if (pc == a->a_hi) {
    if (!st->active) {
      uint16_t base = (z->h << 8) | z->l;  // table base used by RST 28 below
      uint16_t right = k == 0 ? 0xC905 : 0xC90A, left = k == 0 ? 0xC907 : 0xC90C;
      int dir;
      if (base == (ram8(s, right) | (ram8(s, right + 1) << 8))) dir = 1;
      else if (base == (ram8(s, left) | (ram8(s, left + 1) << 8))) dir = -1;
      else dir = 0;
      // Values to check: only those that have just come into view. Values that were
      // already visible are not checked again (an enemy killed in view, by a gem for
      // example, comes back only after its spot leaves the view and enters it again,
      // as in the original game where the check happens at the edge of the screen).
      bool near = cam - st->edge_cam >= -8 && cam - st->edge_cam <= 8;
      st->step = dir < 0 ? -1 : 1;
      if (dir > 0) {
        int to = cam + g->margin_r;
        int from = (st->edge_ok_r && near) ? st->edge_r + 1 : cam;
        st->next = from; st->last = to;
        if (!(st->edge_ok_r && near) || to > st->edge_r) st->edge_r = to;
        st->edge_ok_r = true;
      } else if (dir < 0) {
        int to = cam - g->margin_l;
        int from = (st->edge_ok_l && near) ? st->edge_l - 1 : cam;
        st->next = from; st->last = to;
        if (!(st->edge_ok_l && near) || to < st->edge_l) st->edge_l = to;
        st->edge_ok_l = true;
      } else {
        st->next = st->last = cam;
      }
      st->edge_cam = cam;
      st->list_base = base;
      st->active = true;
    }
    while ((st->next - st->last) * st->step <= 0 && !spawn_value_valid(s, st->next, cam)) st->next += st->step;
    if ((st->next - st->last) * st->step > 0) {  // nothing (valid) left: leave through the normal exit
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
    uint16_t x = (uint16_t)(ram8(s, obj + 4) + (st->next - cam));
    int slot = slot_of(obj);
    if (slot >= 0) g->seen_in[slot] = 0;
    ram_w(s, obj + 4, x & 0xFF);
    ram_w(s, obj + 5, x >> 8);
    z->pc = a->x_hi_next;
    z->cyc += 19;
    return true;
  }
  if (pc == a->end) {
    st->next += st->step;
    if ((st->next - st->last) * st->step <= 0) {  // scan the next camera value
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

// ---------------------------------------------------------------- respawn option
// Each spawn-list entry names the object slot it uses: the game creates the enemy when the
// camera reaches the entry and that slot is free. An enemy killed (freed while visible, not by
// the off-screen clean-up) is remembered so its entry does not fill the slot again:
// "off" until the section changes or the player dies, "mixed" until its spot has been left behind.
static uint32_t kill_section_key(sms_t* s) {
  return ((uint32_t)ram8(s, 0xDFC0) << 16) | ((uint32_t)ram8(s, 0xDFC1) << 8) | ram8(s, 0xC6A2);
}

static bool entry_blocked(uint32_t id) {
  if (g->respawn == MOD_RESPAWN_ON) return false;
  for (int i = 0; i < g->n_killed; i++) if (g->killed[i].id == id) return true;
  return false;
}

static void note_spawn(sms_t* s, int k, uint16_t slot_addr, uint16_t entry) {
  int slot = slot_of(slot_addr);
  if (slot < 0) return;
  g->slot_entry[slot] = ((uint32_t)s->st.bank[2] << 16) | entry | 0x80000000u;
  g->slot_trigger[slot] = g->spawn[k].active ? g->spawn[k].next : cam16(s);
  g->slot_culled[slot] = 0;
}

// called at the start of every sprite pass: which objects disappeared since the last one
static void scan_slots(sms_t* s) {
  uint32_t key = kill_section_key(s);
  int lives = ram8(s, 0xDFBA);
  if (key != g->kill_key || lives < g->last_lives) {
    g->kill_key = key;
    // the list of killed enemies starts again; the objects already created for the new
    // section keep their entry (the game creates them before its first sprite pass), and
    // the slots emptied by the change itself do not count as kills
    g->n_killed = 0;
    memset(g->slot_active, 0, sizeof g->slot_active);
  }
  g->last_lives = lives;
  for (int i = 0; i < 46; i++) {
    uint16_t a = 0xCA40 + i * 0x20;
    bool act = ram8(s, a) != 0;
    if (g->slot_active[i] && !act && g->slot_entry[i]) {
      int x = g->slot_x[i];
      bool visible = x >= -16 - g->margin_l && x <= 255 + g->margin_r;
      if (!g->slot_culled[i] && visible && g->n_killed < 64) {
        g->killed[g->n_killed].id = g->slot_entry[i];
        g->killed[g->n_killed].trigger = g->slot_trigger[i];
        g->n_killed++;
      }
      g->slot_entry[i] = 0;
    }
    g->slot_active[i] = act;
    if (act) g->slot_x[i] = (int16_t)(ram8(s, a + 4) | (ram8(s, a + 5) << 8));
    g->slot_culled[i] = 0;
  }
  // mixed: an enemy can come back once its spot has been left well behind
  if (g->respawn == MOD_RESPAWN_MIXED) {
    int cam = cam16(s);
    for (int i = 0; i < g->n_killed; ) {
      int d = g->killed[i].trigger - cam;
      if (d < -(256 + 2 * MOD_WIDE_EXT) || d > 256 + 2 * MOD_WIDE_EXT) g->killed[i] = g->killed[--g->n_killed];
      else i++;
    }
  }
}

// ---------------------------------------------------------------- difficulty
static int scale_damage(int d, int* frac, bool taken) {
  int mode = g->difficulty;
  bool half = (taken && mode == MOD_DIFF_EASY) || (!taken && mode == MOD_DIFF_HARD);
  bool twice = (taken && mode == MOD_DIFF_HARD) || (!taken && mode == MOD_DIFF_EASY);
  if (twice) return d * 2 > 255 ? 255 : d * 2;
  if (half) { int t = d + *frac; *frac = t & 1; return t >> 1; }
  return d;
}


// ---------------------------------------------------------------- more subweapons on screen
// The hero's projectiles live in two fixed slots (CA40, CA60) and a throw is refused while both
// are busy. Extra slots of the same layout are added in RAM the game never uses; every place that
// walks the projectile slots is extended to walk them too, by sending the CPU through the game's
// own loop body once more (so their movement, drawing, hits and walls are the original code):
//   throw check (147E/1521/1577/15D8), slot choice (22B9), update loop (2354, ends 2379),
//   hitbox list C858.. (built at 43C7, checked by 43DC, walls at 4448), hit flags (3D7C, 43EF),
//   sprite pass (51B9, ends 51EC), camera shifts (5179, 5195), pushes (50D8), clears (50CA, 61D2).
#define MS_SLOTS 6
#define MS_BASE 0xDC00            // never used by the game (checked in every round); MS_SLOTS * 0x20 bytes of slots, then MS_SLOTS * 8 of hitboxes
#define MS_HIT (MS_BASE + MS_SLOTS * 0x20)

static inline uint16_t ms_slot(int k) { return (uint16_t)(MS_BASE + k * 0x20); }
static inline uint16_t ms_hit(int k) { return (uint16_t)(MS_HIT + k * 8); }

static bool ms_any(sms_t* s) {
  for (int k = 0; k < MS_SLOTS; k++) if (ram8(s, ms_slot(k))) return true;
  return false;
}

static void ms_clear(sms_t* s) {
  for (int k = 0; k < MS_SLOTS; k++) { ram_w(s, ms_slot(k), 0); ram_w(s, ms_hit(k), 0); }
}

static void push_ret(sms_t* s, uint16_t ret) {
  z80* z = &s->st.cpu;
  z->sp -= 2;
  ram_w(s, z->sp, (uint8_t)ret);
  ram_w(s, z->sp + 1, (uint8_t)(ret >> 8));
}

static int ms_next_hit(sms_t* s, int from) {
  for (int k = from; k < MS_SLOTS; k++) if (ram8(s, ms_hit(k))) return k;
  return -1;
}

static bool ms_hook(sms_t* s, uint16_t pc) {
  z80* z = &s->st.cpu;
  switch (pc) {
  case 0x147E: case 0x1521: case 0x1577: case 0x15D8: {   // can a subweapon be thrown?
    if (!g->multi_shot) return false;
    bool free_slot = !ram8(s, 0xCA40) || !ram8(s, 0xCA60);
    for (int k = 0; k < MS_SLOTS && !free_slot; k++) free_slot = !ram8(s, ms_slot(k));
    static const uint16_t yes[4] = {0x148B, 0x152D, 0x1584, 0x15E5}, no[4] = {0x14C6, 0x154A, 0x15A6, 0x1607};
    int i = pc == 0x147E ? 0 : pc == 0x1521 ? 1 : pc == 0x1577 ? 2 : 3;
    z->pc = free_slot ? yes[i] : no[i];
    z->cyc += 37;
    return true;
  }
  case 0x22B9: {   // the projectile is created: first free slot
    uint16_t slot = 0;
    if (!ram8(s, 0xCA40)) slot = 0xCA40;
    else if (!ram8(s, 0xCA60) || !g->multi_shot) slot = 0xCA60;
    else for (int k = 0; k < MS_SLOTS && !slot; k++) if (!ram8(s, ms_slot(k))) slot = ms_slot(k);
    if (!slot) slot = 0xCA60;   // (the throw check makes this impossible)
    z->b = z->a;
    z->h = (uint8_t)(slot >> 8); z->l = (uint8_t)slot;
    z->pc = 0x22C5;
    z->cyc += 21;
    return true;
  }
  case 0x2379:   // end of the projectile update loop
    if (g->ms_upd) { g->ms_upd = 0; return false; }
    if (z->ix != 0xCA80 || !ms_any(s)) return false;
    g->ms_upd = 1;
    z->ix = MS_BASE; z->b = MS_SLOTS; z->d = 0; z->e = 0x20;
    z->pc = 0x235D;
    return true;
  case 0x43D5: {   // hitboxes of the hero's weapons were built (HL = after the last one)
    if (g->ms_draw == 0) {
      if (!ms_any(s)) { ms_clear(s); return false; }
      g->ms_hl = (uint16_t)((z->h << 8) | z->l);
    }
    if (g->ms_draw < MS_SLOTS) {
      int k = g->ms_draw++;
      z->ix = ms_slot(k);
      z->h = (uint8_t)(ms_hit(k) >> 8); z->l = (uint8_t)ms_hit(k);
      push_ret(s, 0x43D5);
      z->pc = 0x0F1A;
      z->cyc += 17;
      return true;
    }
    g->ms_draw = 0;
    z->h = (uint8_t)(g->ms_hl >> 8); z->l = (uint8_t)g->ms_hl;
    return false;
  }
  case 0x43E8: {   // end of "does this object touch a weapon?": also the extra projectiles
    if (z->cf) { g->ms_coll = 0; return false; }
    int k = ms_next_hit(s, g->ms_coll);
    if (k < 0) { g->ms_coll = 0; return false; }
    g->ms_coll = k + 1;
    z->h = (uint8_t)(ms_hit(k) >> 8); z->l = (uint8_t)ms_hit(k);
    push_ret(s, 0x43E8);
    z->pc = 0x0F5B;
    z->cyc += 17;
    return true;
  }
  case 0x446D: {   // walls: run the check written for C860 on each extra hitbox (swapped in)
    if (g->ms_wall) {
      int k = g->ms_wall - 1;
      for (int i = 0; i < 8; i++) ram_w(s, ms_hit(k) + i, ram8(s, 0xC860 + i));
      for (int i = 0; i < 8; i++) ram_w(s, 0xC860 + i, g->ms_saved[i]);
    }
    int k = ms_next_hit(s, g->ms_wall);
    if (k < 0) { g->ms_wall = 0; return false; }
    g->ms_wall = k + 1;
    for (int i = 0; i < 8; i++) g->ms_saved[i] = ram8(s, 0xC860 + i);
    for (int i = 0; i < 8; i++) ram_w(s, 0xC860 + i, ram8(s, ms_hit(k) + i));
    z->pc = 0x4452;
    return true;
  }
  case 0x3D7C: case 0x43EF:   // hit flags go back to the projectiles
    for (int k = 0; k < MS_SLOTS; k++) {
      uint8_t f = ram8(s, ms_hit(k) + 7);
      if (f && ram8(s, ms_hit(k))) ram_w(s, ms_slot(k) + 0x1A, f);
    }
    return false;
  case 0x51EC:   // end of the first group of the sprite pass
    if (g->ms_spr) { g->ms_spr = 0; return false; }
    if (!ms_any(s)) return false;
    g->ms_spr = 1;
    z->h = (uint8_t)(MS_BASE >> 8); z->l = (uint8_t)MS_BASE; z->d = 0; z->e = 0x20; z->b = MS_SLOTS;
    z->pc = 0x51C1;
    return true;
  case 0x50EE:   // objects pushed by their speed (17 slots from CA40)
    if (g->ms_vel) { g->ms_vel = 0; return false; }
    if (!ms_any(s)) return false;
    g->ms_vel = 1;
    z->h = (uint8_t)(MS_BASE >> 8); z->l = (uint8_t)MS_BASE; z->d = 0; z->e = 0x20; z->b = MS_SLOTS;
    z->pc = 0x50E0;
    return true;
  case 0x5194: case 0x51B5:   // camera moved: every object shifts one pixel
    for (int k = 0; k < MS_SLOTS; k++) {
      uint16_t o = ms_slot(k);
      if (!ram8(s, o)) continue;
      uint16_t x = (uint16_t)(ram8(s, o + 4) | (ram8(s, o + 5) << 8));
      x = pc == 0x5194 ? x + 1 : x - 1;
      ram_w(s, o + 4, (uint8_t)x); ram_w(s, o + 5, (uint8_t)(x >> 8));
    }
    return false;
  case 0x50D7: case 0x61D2:   // all objects / the hero's projectiles are removed
    ms_clear(s);
    return false;
  }
  return false;
}

// ---------------------------------------------------------------- hook entry
static bool on_hook(sms_t* s, uint16_t pc) {
  z80* z = &s->st.cpu;
  if (ms_hook_map[pc]) return ms_hook(s, pc);
  switch (pc) {
  case 0x3CD0: case 0x52E7: {  // spawn-list entry: is its slot free? (A = slot byte, HL = entry + 3)
    if (z->a == 0 && entry_blocked(((uint32_t)s->st.bank[2] << 16) | (uint16_t)(((z->h << 8) | z->l) - 3) | 0x80000000u))
      z->a = 1;   // treat the slot as busy: the enemy is not created
    return false;
  }
  case 0x3CDD: case 0x52F4:  // object created from a spawn-list entry (DE = slot, HL = entry + 3)
    note_spawn(s, pc == 0x3CDD ? 0 : 1, (z->d << 8) | z->e, (uint16_t)(((z->h << 8) | z->l) - 3));
    return false;
  case 0x5238: case 0x523D: {  // the game removes an object that went off screen (HL = slot)
    int slot = slot_of((z->h << 8) | z->l);
    if (slot >= 0) g->slot_culled[slot] = 1;
    return false;
  }
  case 0x46F1:  // damage to the player (B)
    if (g->difficulty != MOD_DIFF_NORMAL) z->b = (uint8_t)scale_damage(z->b, &g->frac_taken, true);
    return false;
  case 0x6198:  // damage to a boss (B)
    if (g->difficulty != MOD_DIFF_NORMAL) z->b = (uint8_t)scale_damage(z->b, &g->frac_dealt, false);
    return false;
  case 0x3DD6:  // damage to an enemy: SUB (IY+1), adjusted for this instruction only
    if (g->difficulty != MOD_DIFF_NORMAL) {
      uint16_t a = z->iy + 1;
      g->saved_dmg_addr = a;
      g->saved_dmg = ram8(s, a);
      ram_w(s, a, (uint8_t)scale_damage(g->saved_dmg, &g->frac_dealt, false));
    }
    return false;
  case 0x3DD9:
    if (g->saved_dmg_addr) { ram_w(s, (uint16_t)g->saved_dmg_addr, g->saved_dmg); g->saved_dmg_addr = 0; }
    return false;
  case 0x811C:  // sound driver (bank 9): A = (C103) is the sound that starts now
    if (s->st.bank[2] == 9) {
      int id = ram8(s, 0xC103);
      int ev = 0;
      if (id >= 0x81 && id <= 0x8F) {
        if (g->music_has && g->music_has(id)) {
          // no new sound starts (0x80 = idle) and the music channels go quiet: the driver's own
          // "stop all" (C1) cannot be used here, it breaks the game's frame wait
          ram_w(s, 0xC103, 0x80);
          for (int c = 0; c < 4; c++) ram_w(s, 0xC140 + c * 0x30, 0);
          for (int c = 0; c < 4; c++) s->st.psg.vol[c] = 0x0F;
          ev = MOD_MUSIC_PLAY | id;
        } else ev = MOD_MUSIC_STOP | id;
      } else if (id == 0xC0) ev = MOD_MUSIC_FADE;
      else if (id == 0xC1) ev = MOD_MUSIC_STOP;
      if (ev && g->n_music_events < 8) g->music_events[g->n_music_events++] = ev;
    }
    return false;
  case 0x51B6:  // start of the sprite pass
    // status bar redraw requested by a cheat: call the game's own routine (the one used
    // when an item is picked up) and come back here
    if (g->hud_pending && mod_is_gameplay(s)) {
      uint16_t target = 0;
      if (g->hud_pending & MOD_HUD_WEAPON) { g->hud_pending &= ~MOD_HUD_WEAPON; target = 0x4A07; }
      else if (g->hud_pending & MOD_HUD_SUBWEAPON) { g->hud_pending &= ~MOD_HUD_SUBWEAPON; target = 0x4A10; }
      else { g->hud_pending &= ~MOD_HUD_LIVES; target = 0x4A2C; }
      z->sp -= 2;
      ram_w(s, z->sp, 0xB6);
      ram_w(s, z->sp + 1, 0x51);
      z->pc = target;
      z->cyc += 17;
      return true;
    }
    scan_slots(s);
    g->n_building = 0;
    g->passes++;
    for (int i = 0; i < 46; i++) if (!ram8(s, 0xCA40 + i * 0x20)) g->seen_in[i] = 0;
    return false;
  case 0x5228: {  // end of the sprite pass: store the completed list
    int h = (g->pass_head + 1) & 3;
    memcpy(g->pass[h], g->building, sizeof(mod_piece_t) * g->n_building);
    g->n_pass[h] = g->n_building;
    g->pass_head = h;
    return false;
  }
  case 0x0DD1:  // sprite builder: observe
    capture(s, z->ix, true);
    return false;
  case 0x520E: {  // second object group, off the original screen: observe
    int x16 = (int16_t)(ram8(s, z->ix + 4) | (ram8(s, z->ix + 5) << 8));
    if (in_wide_range(z->ix, x16)) capture(s, z->ix, false);
    return false;
  }
  case 0x51D5: {  // first object group: keep-alive / draw decision
    if (!g->wide) return false;
    int x16 = (int16_t)(ram8(s, z->ix + 4) | (ram8(s, z->ix + 5) << 8));
    if (x16 >= 0 && x16 <= 255) { z->a = 0; z->pc = 0x51E5; z->cyc += 35; }
    else if (x16 >= -17 && x16 < 0) { z->a = (0x11 + (x16 & 0xFF)) & 0xFF; z->pc = 0x51E5; z->cyc += 74; }
    else if (in_wide_range(z->ix, x16)) { capture(s, z->ix, false); z->pc = 0x51E9; z->cyc += 35; }
    else { int sl = slot_of(z->ix); if (sl >= 0) g->slot_culled[sl] = 1; z->pc = 0x5238; z->cyc += 35; }
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
  {0x3CCF, {0x1A, 0xB7, 0xCC, 0xDD}}, {0x52E6, {0x1A, 0xB7, 0xCC, 0xF4}}, {0x3CDD, {0xD5, 0xDD, 0xE1, 0x7E}},
  {0x52F4, {0xD5, 0xDD, 0xE1, 0x7E}}, {0x523D, {0x36, 0x00, 0xC3, 0x25}}, {0x46F1, {0x3A, 0x3A, 0xCA, 0x90}},
  {0x6198, {0x3A, 0x3A, 0xCB, 0x90}}, {0x3DD3, {0xDD, 0x7E, 0x1A, 0xFD}}, {0x3DD9, {0xDD, 0x77, 0x1A, 0xDD}},
  {0x4A07, {0xCD, 0x4D, 0x4B, 0x3A}}, {0x4A10, {0xCD, 0x7E, 0x4B, 0x3A}}, {0x4A2C, {0x3A, 0xBA, 0xDF, 0x3D}},
  // extra projectiles
  {0x147E, {0x3E, 0xFF, 0x21, 0x40}}, {0x1521, {0x3E, 0xFF, 0x21, 0x40}}, {0x1577, {0x3E, 0xFF, 0x21, 0x40}},
  {0x15D8, {0x3E, 0xFF, 0x21, 0x40}}, {0x22B9, {0x47, 0x21, 0x40, 0xCA}}, {0x2375, {0xDD, 0x19, 0x10, 0xE5}},
  {0x43C7, {0xDD, 0x21, 0x40, 0xCA}}, {0x43D5, {0xDD, 0x21, 0x20, 0xCA}}, {0x43DC, {0x21, 0x58, 0xC8, 0xCD}},
  {0x43E8, {0xC9, 0xCD, 0x01, 0x44}}, {0x444B, {0x3A, 0x60, 0xC8, 0xB7}}, {0x446D, {0x3A, 0x68, 0xC8, 0xB7}},
  {0x3D7C, {0x3A, 0x67, 0xC8, 0xB7}}, {0x43EF, {0x3A, 0x67, 0xC8, 0xB7}}, {0x51E9, {0x19, 0x10, 0xD5, 0x21}},
  {0x50E1, {0xB6, 0x28, 0x07, 0xAF}}, {0x50EC, {0x10, 0xF3, 0xC9, 0x21}}, {0x5192, {0x10, 0xEE, 0xC9, 0x21}},
  {0x51B3, {0x10, 0xE9, 0xC9, 0xCD}}, {0x50D5, {0x10, 0xFC, 0xC9, 0x21}}, {0x61D2, {0x21, 0x40, 0xCA, 0x36}},
  {0x0F1A, {0xDD, 0xCB, 0x00, 0x46}}, {0x0F5B, {0x7E, 0xB7, 0x28, 0x42}},
};

void mod_request_hud(mod_enhance_t* e, int parts) { e->hud_pending |= (uint8_t)parts; }

int mod_music_poll(mod_enhance_t* e) {
  if (!e->n_music_events) return 0;
  int ev = e->music_events[0];
  for (int i = 1; i < e->n_music_events; i++) e->music_events[i - 1] = e->music_events[i];
  e->n_music_events--;
  return ev;
}

bool mod_music_paused(const sms_t* s) { return s->st.ram[0x0108] != 0; }

void mod_music_request(sms_t* s, int id) { s->st.ram[0x0106] = (uint8_t)id; }

bool mod_enhance_compatible(const sms_t* s) {
  if (s->rom_size < 0x28000) return false;
  static const uint8_t drv[4] = {0x3A, 0x03, 0xC1, 0xCB};   // 9:811C LD A,(C103) ; BIT 7,A
  if (memcmp(s->rom + 9 * 0x4000 + 0x011C, drv, 4)) return false;
  for (unsigned i = 0; i < sizeof signature / sizeof signature[0]; i++)
    if (memcmp(s->rom + signature[i].addr, signature[i].b, 4)) return false;
  return true;
}

void mod_enhance_attach(sms_t* s, mod_enhance_t* e) {
  g = e;
  if (!mod_enhance_compatible(s)) { s->hook_map = NULL; s->hook2_pc = 0; return; }
  memset(hook_map, 0, sizeof hook_map);
  static const uint16_t addrs[] = {0x51B6, 0x5228, 0x0DD1, 0x520E, 0x51D5, 0x3CD0, 0x52E7, 0x3CDD, 0x52F4,
                                   0x5238, 0x523D, 0x46F1, 0x6198, 0x3DD6, 0x3DD9};
  for (unsigned i = 0; i < sizeof addrs / sizeof addrs[0]; i++) hook_map[addrs[i]] = 1;
  memset(ms_hook_map, 0, sizeof ms_hook_map);
  static const uint16_t ms_addrs[] = {0x147E, 0x1521, 0x1577, 0x15D8, 0x22B9, 0x2379, 0x43D5, 0x43E8, 0x446D,
                                      0x3D7C, 0x43EF, 0x51EC, 0x50EE, 0x5194, 0x51B5, 0x50D7, 0x61D2};
  for (unsigned i = 0; i < sizeof ms_addrs / sizeof ms_addrs[0]; i++) hook_map[ms_addrs[i]] = ms_hook_map[ms_addrs[i]] = 1;
  e->ms_draw = e->ms_coll = e->ms_wall = e->ms_upd = e->ms_spr = e->ms_vel = 0;
  for (int k = 0; k < 2; k++) {
    const spawn_addr_t* a = &spawn_addrs[k];
    hook_map[a->a_hi] = hook_map[a->a_lo] = hook_map[a->a_lo2] = hook_map[a->x_hi] = hook_map[a->end] = 1;
    e->spawn[k].active = false;
  }
  for (int k = 0; k < 2; k++) e->spawn[k].edge_ok_r = e->spawn[k].edge_ok_l = false;
  e->n_killed = 0; e->kill_key = 0xFFFFFFFFu; e->saved_dmg_addr = 0;
  e->difficulty = MOD_DIFF_NORMAL; e->respawn = MOD_RESPAWN_ON;
  memset(e->slot_entry, 0, sizeof e->slot_entry);
  memset(e->slot_active, 0, sizeof e->slot_active);
  memset(e->seen_in, 0, sizeof e->seen_in);
  s->hook_map = hook_map;
  s->pc_hook = on_hook;
  s->hook2_pc = 0x811C;   // sound driver, bank 9
  s->hook2_bank = 9;
}

void mod_enhance_set_wide(mod_enhance_t* e, bool wide, int ext) {
  e->wide = wide;
  e->ext = ext;
  e->margin_l = e->margin_r = wide ? ext : 0;
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

// Double jump: pressing jump again in the air restarts the jump arc (state 3 = rising,
// 2 = falling, 0 = on the ground; CA2B = frame of the arc; C701 = buttons pressed this frame).
static void double_jump(sms_t* s, mod_enhance_t* e) {
  if (!s->hook_map) return;   // other ROM: enhancements off
  int st = s->st.ram[0x0A31];
  if (st == 0) e->air_jump_used = false;
  bool in_air = st == 2 || st == 3;
  bool was_in_air = e->prev_state == 2 || e->prev_state == 3;
  if (e->double_jump && in_air && was_in_air && !e->air_jump_used && (s->st.ram[0x0701] & 0x20) &&
      mod_is_gameplay(s)) {
    s->st.ram[0x0A31] = 3;
    s->st.ram[0x0A2B] = 0;
    // the arc moves in steps of 8 and the landing check needs the same alignment as a jump
    // from the ground: start the new arc on a multiple of 8
    s->st.ram[0x0A22] &= 0xF8;
    s->st.ram[0x0A32] = s->st.ram[0x0A33] = 0;   // no leftover falling speed
    e->air_jump_used = true;
  }
  e->prev_state = st;
}

void mod_enhance_frame(const sms_t* s, mod_enhance_t* e) {
  double_jump((sms_t*)s, e);
  // margins for the next frame's game logic
  if (!e->wide || !mod_visible_margins(s, e->ext, &e->margin_l, &e->margin_r)) {
    e->margin_l = e->margin_r = e->wide ? e->ext : 0;
  }
  // what stays covered after this frame: the window shrinks when the view moves back or
  // the visible margin gets smaller, so those values are checked again when they reappear
  if (e->wide && mod_is_gameplay(s)) {
    int cam = s->st.ram[0x06A0] | (s->st.ram[0x06A1] << 8);
    for (int k = 0; k < 2; k++) {
      if (e->spawn[k].edge_r > cam + e->margin_r) e->spawn[k].edge_r = cam + e->margin_r;
      if (e->spawn[k].edge_l < cam - e->margin_l) e->spawn[k].edge_l = cam - e->margin_l;
      e->spawn[k].edge_cam = cam;
    }
  } else {
    for (int k = 0; k < 2; k++) e->spawn[k].edge_ok_r = e->spawn[k].edge_ok_l = false;
  }
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
