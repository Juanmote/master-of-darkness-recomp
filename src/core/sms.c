#include "sms.h"
#include <string.h>

#define CYC_PER_LINE 228
#define LINES_PER_FRAME 262
#define CYC_PER_FRAME (CYC_PER_LINE * LINES_PER_FRAME)

static inline int bank_count(const sms_t* s) { return s->rom_size / 0x4000; }

int sms_rom_offset(const sms_t* s, uint16_t a) {
  const sms_state_t* st = &s->st;
  if (a < 0x400) return a;
  if (a < 0x4000) return (st->bank[0] % bank_count(s)) * 0x4000 + a;
  if (a < 0x8000) return (st->bank[1] % bank_count(s)) * 0x4000 + (a - 0x4000);
  if (a < 0xC000) return (st->bank[2] % bank_count(s)) * 0x4000 + (a - 0x8000);
  return -1;
}

uint8_t sms_peek(const sms_t* s, uint16_t a) {
  if (a >= 0xC000) return s->st.ram[a & 0x1FFF];
  int o = sms_rom_offset(s, a);
  return o >= 0 ? s->rom[o] : 0xFF;
}

static uint8_t mem_read(void* u, uint16_t a) { return sms_peek((sms_t*)u, a); }

static void mem_write(void* u, uint16_t a, uint8_t v) {
  sms_t* s = u;
  if (a < 0xC000) return;
  s->st.ram[a & 0x1FFF] = v;
  if (s->hooks.on_ram_write) s->hooks.on_ram_write(s, a, v);
  if (a >= 0xFFFC) {
    if (a == 0xFFFC) s->st.mapper_ctrl = v;
    else s->st.bank[a - 0xFFFD] = v;
  }
}

static const uint32_t* color_lut;   // optional replacement palette (64 ARGB colours)

void sms_set_palette(const uint32_t* lut64) { color_lut = lut64; }

uint32_t sms_color(uint8_t c) {
  if (color_lut) return color_lut[c & 63];
  static const uint8_t lv[4] = {0, 85, 170, 255};
  return 0xFF000000u | ((uint32_t)lv[c & 3] << 16) | ((uint32_t)lv[(c >> 2) & 3] << 8) | lv[(c >> 4) & 3];
}

// ---------------- PSG (SN76489) ----------------
static void psg_write(sms_t* s, uint8_t v) {
  if (s->psg_log_n < (int)(sizeof s->psg_log / sizeof s->psg_log[0])) {
    s->psg_log[s->psg_log_n].cyc = (uint32_t)(s->st.cpu.cyc - s->frame_start_cyc);
    s->psg_log[s->psg_log_n].val = v;
    s->psg_log_n++;
  }
}

static void psg_apply(sms_psg_t* p, uint8_t v) {
  if (v & 0x80) {
    p->latch = (v >> 4) & 7;
    int ch = p->latch >> 1;
    if (p->latch & 1) p->vol[ch] = v & 15;
    else if (ch < 3) p->tone[ch] = (p->tone[ch] & 0x3F0) | (v & 15);
    else { p->noise = v & 7; p->lfsr = 0x8000; }
  } else {
    int ch = p->latch >> 1;
    if (p->latch & 1) p->vol[ch] = v & 15;
    else if (ch < 3) p->tone[ch] = (p->tone[ch] & 15) | ((v & 0x3F) << 4);
    else { p->noise = v & 7; p->lfsr = 0x8000; }
  }
}

static const int16_t psg_vol[16] = {
  8191, 6507, 5168, 4105, 3261, 2590, 2057, 1634, 1298, 1031, 819, 650, 516, 410, 326, 0
};

static inline void psg_tick(sms_psg_t* p) {
  for (int c = 0; c < 3; c++) {
    if (--p->counter[c] <= 0) {
      p->counter[c] = p->tone[c] ? p->tone[c] : 0x400;
      p->output[c] ^= 1;
    }
  }
  if (--p->counter[3] <= 0) {
    int r = p->noise & 3;
    p->counter[3] = r == 3 ? (p->tone[2] ? p->tone[2] : 0x400) : (0x10 << r);
    p->output[3] ^= 1;
    if (p->output[3]) {
      int fb = (p->noise & 4) ? ((p->lfsr & 1) ^ ((p->lfsr >> 3) & 1)) : (p->lfsr & 1);
      p->lfsr = (p->lfsr >> 1) | (fb << 15);
    }
  }
}

static void psg_render(sms_t* s) {
  sms_psg_t* p = &s->st.psg;
  // PSG ticks per output sample in 16.16 fixed point: (3579545/16)/44100
  const uint32_t step = (uint32_t)(((uint64_t)3579545 << 16) / 16 / SMS_AUDIO_RATE);
  int li = 0;
  for (int i = 0; i < SMS_SAMPLES_PER_FRAME; i++) {
    uint32_t cyc = (uint32_t)((uint64_t)i * CYC_PER_FRAME * s->cpu_mult / SMS_SAMPLES_PER_FRAME);
    while (li < s->psg_log_n && s->psg_log[li].cyc <= cyc) psg_apply(p, s->psg_log[li++].val);
    p->frac += step;
    int ticks = p->frac >> 16;
    p->frac &= 0xFFFF;
    int32_t acc = 0;
    for (int t = 0; t < ticks; t++) {
      psg_tick(p);
      int32_t smp = 0;
      for (int c = 0; c < 3; c++) {
        int on = p->tone[c] <= 1 ? 1 : p->output[c];
        if (on && !(s->psg_mute & (1 << c))) smp += psg_vol[p->vol[c]];
      }
      if ((p->lfsr & 1) && !(s->psg_mute & 8)) smp += psg_vol[p->vol[3]];
      acc += smp;
    }
    int32_t v = ticks ? acc / ticks : 0;
    s->audio[i] = (int16_t)(v - 8191 * 2 > 32767 ? 32767 : v - 8191 * 2 < -32768 ? -32768 : v - 8191 * 2);
  }
  while (li < s->psg_log_n) psg_apply(p, s->psg_log[li++].val);
  s->psg_log_n = 0;
}

// ---------------- VDP ----------------
static uint8_t vcounter(sms_t* s) {
  int l = s->st.vdp.line;
  return l <= 0xDA ? l : l - 6;
}

static void vdp_ctrl_write(sms_t* s, uint8_t v) {
  sms_vdp_t* d = &s->st.vdp;
  if (!d->latch) {
    d->first = v;
    d->addr = (d->addr & 0x3F00) | v;
    d->latch = 1;
    return;
  }
  d->latch = 0;
  d->code = v >> 6;
  d->addr = ((v & 0x3F) << 8) | d->first;
  if (d->code == 0) {
    d->buffer = d->vram[d->addr & 0x3FFF];
    d->addr = (d->addr + 1) & 0x3FFF;
  } else if (d->code == 2) {
    d->reg[v & 0x0F] = d->first;
    if (s->hooks.on_vdp_reg) s->hooks.on_vdp_reg(s, v & 0x0F, d->first);
    s->st.cpu.int_pending = sms_irq_line(s);
  }
}

static void vdp_data_write(sms_t* s, uint8_t v) {
  sms_vdp_t* d = &s->st.vdp;
  d->latch = 0;
  if (d->code == 3) d->cram[d->addr & 31] = v;
  else {
    if (s->hooks.on_vram_write) s->hooks.on_vram_write(s, d->addr & 0x3FFF, v);
    d->vram[d->addr & 0x3FFF] = v;
  }
  d->buffer = v;
  d->addr = (d->addr + 1) & 0x3FFF;
}

static uint8_t vdp_data_read(sms_t* s) {
  sms_vdp_t* d = &s->st.vdp;
  d->latch = 0;
  uint8_t r = d->buffer;
  d->buffer = d->vram[d->addr & 0x3FFF];
  d->addr = (d->addr + 1) & 0x3FFF;
  return r;
}

static uint8_t vdp_status_read(sms_t* s) {
  sms_vdp_t* d = &s->st.vdp;
  uint8_t r = d->status | 0x1F;
  d->status = 0;
  d->line_irq = 0;
  d->latch = 0;
  s->st.cpu.int_pending = 0;
  return r;
}

bool sms_irq_line(sms_t* s) {
  sms_vdp_t* d = &s->st.vdp;
  return ((d->status & 0x80) && (d->reg[1] & 0x20)) || (d->line_irq && (d->reg[0] & 0x10));
}

static inline int tile_px(const uint8_t* t, int bit) {
  return ((t[0] >> bit) & 1) | (((t[1] >> bit) & 1) << 1) | (((t[2] >> bit) & 1) << 2) | (((t[3] >> bit) & 1) << 3);
}

// The 8 pixels of one tile row (4 bit planes) as 8 colour numbers, left to right; flipped = mirrored.
// spread[b] holds the 8 bits of b as 8 bytes (first byte = leftmost pixel = bit 7); spread_flip the other way round.
static uint64_t spread[256], spread_flip[256];
static bool spread_ready;
static void spread_init(void) {
  for (int b = 0; b < 256; b++) {
    uint8_t t[8], f[8];
    for (int i = 0; i < 8; i++) { t[i] = (b >> (7 - i)) & 1; f[i] = (b >> i) & 1; }
    memcpy(&spread[b], t, 8); memcpy(&spread_flip[b], f, 8);
  }
  spread_ready = true;
}
static inline void tile_row(const uint8_t* t, bool flipped, uint8_t out[8]) {
  const uint64_t* sp = flipped ? spread_flip : spread;
  uint64_t v = sp[t[0]] | (sp[t[1]] << 1) | (sp[t[2]] << 2) | (sp[t[3]] << 3);   // every byte is 0 or 1: the shifts stay inside it
  memcpy(out, &v, 8);
}

static void render_line(sms_t* s, int y) {
  sms_vdp_t* d = &s->st.vdp;
  uint8_t* out = s->fb_idx[y];
  uint8_t* over = s->spr_over[y];
  uint8_t prio[SMS_W];
  uint8_t backdrop = 16 + (d->reg[7] & 15);
  memset(over, 0, SMS_SPR_OVER);
  s->line_hscroll[y] = d->reg[8];
  s->line_vscroll[y] = d->vscroll_latched;
  s->line_reg0[y] = d->reg[0];
  s->line_display[y] = (d->reg[1] & 0x40) != 0;
  memcpy(s->line_cram[y], d->cram, 32);
  if (!(d->reg[1] & 0x40)) {
    memset(out, backdrop, SMS_W); memset(s->fb_bg[y], backdrop, SMS_W); memset(s->fb_spr[y], 0, SMS_W);
    return;
  }
  uint16_t nt = (d->reg[2] & 0x0E) << 10;
  int hs = (y < 16 && (d->reg[0] & 0x40)) ? 0 : d->reg[8];
  // Background, one name-table cell (up to 8 pixels) at a time. The picture is cut where the vertical scroll
  // changes (the last 8 columns can be locked) and at every cell edge.
  for (int x = 0; x < SMS_W; ) {
    int seg_end = (x < 192 && (d->reg[0] & 0x80)) ? 192 : SMS_W;
    int vs = (x >= 192 && (d->reg[0] & 0x80)) ? 0 : d->vscroll_latched;
    int sy = (y + vs) % 224;
    int row = sy >> 3, py = sy & 7;
    while (x < seg_end) {
      int sx = (x - hs) & 255;
      int col = sx >> 3, p0 = sx & 7;
      int n = 8 - p0;
      if (n > seg_end - x) n = seg_end - x;
      const uint8_t* ep = &d->vram[nt + (row * 32 + col) * 2];
      uint16_t e = ep[0] | (ep[1] << 8);
      unsigned gi = (unsigned)(e & 0x1FF) - s->glyph_first;
      const uint8_t* tp = s->glyphs && gi < s->glyph_count ? s->glyphs[gi] : &d->vram[(e & 0x1FF) * 32];
      uint8_t c8[8];
      tile_row(tp + ((e & 0x400) ? 7 - py : py) * 4, (e & 0x200) != 0, c8);
      uint8_t pal = (e & 0x800) ? 16 : 0;
      bool front = (e & 0x1000) != 0;
      // the accent of an extra letter is drawn in the empty row above it (the second half of the table)
      const uint8_t* acc = NULL; uint8_t acc_pal = 0;
      if (s->glyphs) {
        const uint8_t* bp = &d->vram[nt + (((row + 1) % 28) * 32 + col) * 2];
        uint16_t be = bp[0] | (bp[1] << 8);
        unsigned g2 = (unsigned)(be & 0x1FF) - s->glyph_first;
        if (g2 < s->glyph_count) { acc = s->glyphs[s->glyph_count + g2] + py * 4; acc_pal = (be & 0x800) ? 16 : 0; }
      }
      if (!acc) {
        for (int k = 0; k < n; k++) {
          uint8_t c = c8[p0 + k];
          out[x + k] = pal + c;
          prio[x + k] = front && c != 0;
        }
      } else {
        uint8_t a8[8];
        tile_row(acc, false, a8);
        for (int k = 0; k < n; k++) {
          uint8_t c = c8[p0 + k], pl = pal;
          if (!c) { c = a8[p0 + k]; if (c) pl = acc_pal; }   // the accent fades with its letter: the blank tile above has another palette
          out[x + k] = pl + c;
          prio[x + k] = front && c != 0;
        }
      }
      x += n;
    }
  }
  memcpy(s->fb_bg[y], out, SMS_W);
  memset(s->fb_spr[y], 0, SMS_W);
  uint16_t sat = (d->reg[5] & 0x7E) << 7;
  if (y == 96) memcpy(s->sat_mid, &d->vram[sat], 256);
  int h = (d->reg[1] & 0x02) ? 16 : 8;
  int zoom = (d->reg[1] & 0x01) ? 2 : 1;
  int count = 0;
  uint8_t drawn[SMS_W + SMS_SPR_OVER] = {0};
  for (int i = 0; i < 64; i++) {
    int sy = d->vram[sat + i];
    if (sy == 0xD0) break;
    sy += 1;
    if (sy > 240) sy -= 256;
    if (y < sy || y >= sy + h * zoom) continue;
    if (++count > 8) { d->status |= 0x40; break; }
    int sx = d->vram[sat + 0x80 + i * 2];
    int tile = d->vram[sat + 0x81 + i * 2];
    if (d->reg[0] & 0x08) sx -= 8;
    if (d->reg[6] & 0x04) tile |= 0x100;
    if (h == 16) tile &= ~1;
    int row = (y - sy) / zoom;
    const uint8_t* t = &d->vram[(tile * 32 + row * 4) & 0x3FFF];
    for (int px = 0; px < 8 * zoom; px++) {
      int x = sx + px;
      if (x < 0 || x >= SMS_W + SMS_SPR_OVER) continue;
      int c = tile_px(t, 7 - px / zoom);
      if (!c) continue;
      if (drawn[x]) { d->status |= 0x20; continue; }
      drawn[x] = 1;
      if (x >= SMS_W) over[x - SMS_W] = 16 + c;
      else if (!prio[x]) { out[x] = 16 + c; s->fb_spr[y][x] = 1; s->fb_sprtile[y][x] = d->vram[sat + 0x81 + i * 2]; }
    }
  }
  for (int x = 0; x < 8; x++) s->fb_left[y][x] = s->fb_spr[y][x] ? out[x] : 0;
  if (d->reg[0] & 0x20) { memset(out, backdrop, 8); memset(s->fb_bg[y], backdrop, 8); memset(s->fb_spr[y], 0, 8); }
}

// ---------------- ports ----------------
static uint8_t port_in(z80* z, uint8_t p) {
  sms_t* s = z->userdata;
  switch (p & 0xC1) {
  case 0x40: return vcounter(s);
  case 0x41: return 0;
  case 0x80: return vdp_data_read(s);
  case 0x81: return vdp_status_read(s);
  case 0xC0: return (uint8_t)~(s->input & 0x3F);
  case 0xC1: return 0xFF;
  }
  return 0xFF;
}

static void port_out(z80* z, uint8_t p, uint8_t v) {
  sms_t* s = z->userdata;
  switch (p & 0xC1) {
  case 0x40: case 0x41: psg_write(s, v); break;
  case 0x80: vdp_data_write(s, v); break;
  case 0x81: vdp_ctrl_write(s, v); break;
  default: break;
  }
}

static void bind_cpu(sms_t* s) {
  s->st.cpu.read_byte = mem_read;
  s->st.cpu.write_byte = mem_write;
  s->st.cpu.port_in = port_in;
  s->st.cpu.port_out = port_out;
  s->st.cpu.userdata = s;
}

void sms_reset(sms_t* s) {
  memset(&s->st, 0, sizeof s->st);
  s->st.bank[0] = 0; s->st.bank[1] = 1; s->st.bank[2] = 2;
  z80_init(&s->st.cpu);
  bind_cpu(s);
  s->st.cpu.sp = 0xDFF0;
  static const uint8_t init_regs[11] = {0x36, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0x00, 0x00, 0x00, 0xFF};
  memcpy(s->st.vdp.reg, init_regs, 11);
  for (int c = 0; c < 4; c++) s->st.psg.vol[c] = 15;
  s->st.psg.lfsr = 0x8000;
  s->psg_log_n = 0;
}

void sms_init(sms_t* s, const uint8_t* rom, int size) {
  memset(s, 0, sizeof *s);
  s->rom = rom;
  s->rom_size = size;
  s->cpu_mult = 1;
  sms_reset(s);
}

void sms_run_frame(sms_t* s) {
  sms_vdp_t* d = &s->st.vdp;
  z80* cpu = &s->st.cpu;
  if (!spread_ready) spread_init();
  if ((s->input & BTN_PAUSE) && !(s->st.prev_input & BTN_PAUSE)) z80_gen_nmi(cpu);
  s->st.prev_input = s->input;
  s->frame_start_cyc = cpu->cyc;
  for (int line = 0; line < LINES_PER_FRAME; line++) {
    d->line = line;
    if (line == 0) d->vscroll_latched = d->reg[9];
    if (line < SMS_H) render_line(s, line);
    if (line <= SMS_H) {
      if (d->line_counter-- == 0) { d->line_counter = d->reg[10]; d->line_irq = 1; }
    } else {
      d->line_counter = d->reg[10];
    }
    if (line == SMS_H + 1) d->status |= 0x80;
    unsigned long target = (unsigned long)(s->frame_start_cyc + (uint64_t)(line + 1) * CYC_PER_LINE * s->cpu_mult);
    while (cpu->cyc < target) {
      if (sms_irq_line(s)) { cpu->int_pending = 1; cpu->int_data = 0xFF; }
      else cpu->int_pending = 0;
      // take pending interrupts at this instruction boundary (same for both CPU paths)
      if (!cpu->iff_delay && (cpu->nmi_pending || (cpu->int_pending && cpu->iff1 && !s->ei_shadow))) {
        z80_process_interrupts(cpu);
        continue;
      }
      if (s->recomp_run && !cpu->halted) {
        static sms_state_t before;
        int psg_n = s->psg_log_n;
        if (s->recomp_verify) before = s->st;
        s->ei_shadow = 0;
        if (s->recomp_run(s, target)) {
          if (s->recomp_verify) s->recomp_verify(s, &before, psg_n);
          continue;
        }
        s->interp_steps++;
        s->last_interp_pc = cpu->pc;
      }
      if (s->hook_map && cpu->pc < 0x8000 && s->hook_map[cpu->pc] && !cpu->halted &&
          s->st.bank[0] == 0 && s->st.bank[1] == 1 && s->pc_hook(s, cpu->pc)) {
        s->ei_shadow = 0;
        continue;
      }
      if (s->hook2_pc && cpu->pc == s->hook2_pc && s->st.bank[2] == s->hook2_bank && !cpu->halted &&
          s->pc_hook(s, cpu->pc)) {
        s->ei_shadow = 0;
        continue;
      }
      if (s->hooks.on_exec) s->hooks.on_exec(s, cpu->pc, sms_rom_offset(s, cpu->pc));
      s->ei_shadow = !cpu->halted && sms_peek(s, cpu->pc) == 0xFB;
      z80_step(cpu);
    }
  }
  psg_render(s);
  s->st.frame++;
}

// ---------------- save states ----------------
#define STATE_MAGIC 0x4D4F4431u  // "MOD1"
size_t sms_state_size(void) { return 8 + sizeof(sms_state_t); }

void sms_save_state(const sms_t* s, void* out) {
  uint32_t hdr[2] = {STATE_MAGIC, (uint32_t)sizeof(sms_state_t)};
  memcpy(out, hdr, 8);
  memcpy((uint8_t*)out + 8, &s->st, sizeof s->st);
}

bool sms_load_state(sms_t* s, const void* in, size_t size) {
  uint32_t hdr[2];
  if (size < 8) return false;
  memcpy(hdr, in, 8);
  if (hdr[0] != STATE_MAGIC || hdr[1] != sizeof(sms_state_t) || size != sms_state_size()) return false;
  memcpy(&s->st, (const uint8_t*)in + 8, sizeof s->st);
  bind_cpu(s);
  // a damaged file must not make the sound chip read or write outside its tables
  s->st.psg.latch &= 7;
  s->st.psg.noise &= 7;
  for (int c = 0; c < 4; c++) s->st.psg.vol[c] &= 15;
  s->psg_log_n = 0;
  return true;
}
