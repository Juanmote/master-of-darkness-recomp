#include "sms.h"
#include <string.h>
#include <stdio.h>

#define CYC_PER_LINE 228
#define LINES_PER_FRAME 262

static inline int bank_count(sms_t* s) { return s->rom_size / 0x4000; }

int sms_rom_offset(sms_t* s, uint16_t a) {
  if (a < 0x400) return a;
  if (a < 0x4000) return (s->bank[0] % bank_count(s)) * 0x4000 + a;
  if (a < 0x8000) return (s->bank[1] % bank_count(s)) * 0x4000 + (a - 0x4000);
  if (a < 0xC000) {
    if (s->mapper_ctrl & 0x08) return -1;
    return (s->bank[2] % bank_count(s)) * 0x4000 + (a - 0x8000);
  }
  return -1;
}

uint8_t sms_peek(sms_t* s, uint16_t a) {
  if (a >= 0xC000) return s->ram[a & 0x1FFF];
  if (a >= 0x8000 && (s->mapper_ctrl & 0x08)) return s->cart_ram[a & 0x3FFF];
  int o = sms_rom_offset(s, a);
  return o >= 0 ? s->rom[o] : 0xFF;
}

static uint8_t mem_read(void* u, uint16_t a) {
  sms_t* s = u;
  if (s->hooks.on_read && a < 0xC000) s->hooks.on_read(s, a, sms_rom_offset(s, a));
  return sms_peek(s, a);
}

static void mem_write(void* u, uint16_t a, uint8_t v) {
  sms_t* s = u;
  if (a >= 0xC000) {
    s->ram[a & 0x1FFF] = v;
    if (s->hooks.on_ram_write) s->hooks.on_ram_write(s, a, v);
    if (a >= 0xFFFC) {
      if (a == 0xFFFC) s->mapper_ctrl = v;
      else {
        s->bank[a - 0xFFFD] = v;
        if (s->hooks.on_bank) s->hooks.on_bank(s, a - 0xFFFD, v);
      }
    }
  } else if (a >= 0x8000 && (s->mapper_ctrl & 0x08)) {
    s->cart_ram[a & 0x3FFF] = v;
  }
}

// ---------------- VDP ----------------
static uint8_t vcounter(sms_t* s) {
  int l = s->vdp.line;
  return l <= 0xDA ? l : l - 6; // NTSC 192-line
}

static void vdp_ctrl_write(sms_t* s, uint8_t v) {
  vdp_t* d = &s->vdp;
  if (!d->latch) {
    d->first = v;
    d->addr = (d->addr & 0x3F00) | v;
    d->latch = true;
    return;
  }
  d->latch = false;
  d->code = v >> 6;
  d->addr = ((v & 0x3F) << 8) | d->first;
  if (d->code == 0) {
    d->buffer = d->vram[d->addr & 0x3FFF];
    d->addr = (d->addr + 1) & 0x3FFF;
  } else if (d->code == 2) {
    int r = v & 0x0F;
    d->reg[r] = d->first;
    if (s->hooks.on_vdp_reg) s->hooks.on_vdp_reg(s, r, d->first);
  }
}

static void vdp_data_write(sms_t* s, uint8_t v) {
  vdp_t* d = &s->vdp;
  d->latch = false;
  if (d->code == 3) {
    d->cram[d->addr & 31] = v;
  } else {
    if (s->hooks.on_vram_write) s->hooks.on_vram_write(s, d->addr & 0x3FFF, v);
    d->vram[d->addr & 0x3FFF] = v;
  }
  d->buffer = v;
  d->addr = (d->addr + 1) & 0x3FFF;
}

static uint8_t vdp_data_read(sms_t* s) {
  vdp_t* d = &s->vdp;
  d->latch = false;
  uint8_t r = d->buffer;
  d->buffer = d->vram[d->addr & 0x3FFF];
  d->addr = (d->addr + 1) & 0x3FFF;
  return r;
}

static uint8_t vdp_status_read(sms_t* s) {
  vdp_t* d = &s->vdp;
  uint8_t r = d->status | 0x1F;
  d->status = 0;
  d->line_irq = false;
  d->latch = false;
  return r;
}

static bool irq_line(sms_t* s) {
  vdp_t* d = &s->vdp;
  return ((d->status & 0x80) && (d->reg[1] & 0x20)) || (d->line_irq && (d->reg[0] & 0x10));
}

static uint32_t cram_to_argb(uint8_t c) {
  static const uint8_t lv[4] = {0, 85, 170, 255};
  return 0xFF000000u | (lv[c & 3] << 16) | (lv[(c >> 2) & 3] << 8) | lv[(c >> 4) & 3];
}

static void render_line(sms_t* s, int y) {
  vdp_t* d = &s->vdp;
  uint8_t* out = &s->fb_idx[y * SMS_W];
  uint8_t prio[SMS_W];
  uint8_t backdrop = 16 + (d->reg[7] & 15);
  s->line_hscroll[y] = d->reg[8];
  s->line_vscroll[y] = d->vscroll_latched;
  if (!(d->reg[1] & 0x40)) {
    memset(out, backdrop, SMS_W);
    goto finish;
  }
  uint16_t nt = (d->reg[2] & 0x0E) << 10;
  int hs = (y < 16 && (d->reg[0] & 0x40)) ? 0 : d->reg[8];
  for (int x = 0; x < SMS_W; x++) {
    int vs = (x >= 192 && (d->reg[0] & 0x80)) ? 0 : d->vscroll_latched;
    int sx = (x - hs) & 255;
    int sy = (y + vs) % 224;
    int col = sx >> 3, row = sy >> 3;
    uint16_t e = d->vram[nt + (row * 32 + col) * 2] | (d->vram[nt + (row * 32 + col) * 2 + 1] << 8);
    int tile = e & 0x1FF;
    int px = sx & 7, py = sy & 7;
    if (e & 0x200) px = 7 - px;
    if (e & 0x400) py = 7 - py;
    int pal = (e & 0x800) ? 16 : 0;
    uint8_t* t = &d->vram[tile * 32 + py * 4];
    int bit = 7 - px;
    int c = ((t[0] >> bit) & 1) | (((t[1] >> bit) & 1) << 1) | (((t[2] >> bit) & 1) << 2) | (((t[3] >> bit) & 1) << 3);
    out[x] = pal + c;
    prio[x] = (e & 0x1000) && c != 0;
  }
  // sprites
  {
    uint16_t sat = (d->reg[5] & 0x7E) << 7;
    int h = (d->reg[1] & 0x02) ? 16 : 8;
    int zoom = (d->reg[1] & 0x01) ? 2 : 1;
    int count = 0;
    uint8_t drawn[SMS_W] = {0};
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
      uint8_t* t = &d->vram[(tile * 32 + row * 4) & 0x3FFF];
      for (int px = 0; px < 8 * zoom; px++) {
        int x = sx + px;
        if (x < 0 || x >= SMS_W) continue;
        int bit = 7 - px / zoom;
        int c = ((t[0] >> bit) & 1) | (((t[1] >> bit) & 1) << 1) | (((t[2] >> bit) & 1) << 2) | (((t[3] >> bit) & 1) << 3);
        if (!c) continue;
        if (drawn[x]) { d->status |= 0x20; continue; }
        drawn[x] = 1;
        if (!prio[x]) out[x] = 16 + c;
      }
    }
  }
  if (d->reg[0] & 0x20) memset(out, backdrop, 8);
finish:
  for (int x = 0; x < SMS_W; x++) s->fb[y * SMS_W + x] = cram_to_argb(d->cram[out[x]]);
}

// ---------------- ports ----------------
static uint8_t port_in(z80* z, uint8_t p) {
  sms_t* s = z->userdata;
  switch (p & 0xC1) {
  case 0x40: return vcounter(s);
  case 0x41: return 0;
  case 0x80: return vdp_data_read(s);
  case 0x81: return vdp_status_read(s);
  case 0xC0: return ~(s->input & 0x3F);
  case 0xC1: return 0xFF;
  }
  return 0xFF;
}

static void port_out(z80* z, uint8_t p, uint8_t v) {
  sms_t* s = z->userdata;
  switch (p & 0xC1) {
  case 0x80: vdp_data_write(s, v); break;
  case 0x81: vdp_ctrl_write(s, v); break;
  default: break; // PSG / memory control / IO control ignored
  }
}

void sms_init(sms_t* s, const uint8_t* rom, int size) {
  memset(s, 0, sizeof *s);
  memcpy(s->rom, rom, size);
  s->rom_size = size;
  s->bank[0] = 0; s->bank[1] = 1; s->bank[2] = 2;
  z80_init(&s->cpu);
  s->cpu.read_byte = mem_read;
  s->cpu.write_byte = mem_write;
  s->cpu.port_in = port_in;
  s->cpu.port_out = port_out;
  s->cpu.userdata = s;
  s->cpu.sp = 0xDFF0;
  static const uint8_t init_regs[11] = {0x36, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0x00, 0x00, 0x00, 0xFF};
  memcpy(s->vdp.reg, init_regs, 11);
}

void sms_run_frame(sms_t* s) {
  vdp_t* d = &s->vdp;
  if ((s->input & BTN_PAUSE) && !(s->prev_input & BTN_PAUSE)) z80_gen_nmi(&s->cpu);
  s->prev_input = s->input;
  for (int line = 0; line < LINES_PER_FRAME; line++) {
    d->line = line;
    if (line == 0) d->vscroll_latched = d->reg[9];
    if (line < SMS_H) render_line(s, line);
    // line counter
    if (line <= SMS_H) {
      if (d->line_counter-- == 0) {
        d->line_counter = d->reg[10];
        d->line_irq = true;
      }
    } else {
      d->line_counter = d->reg[10];
    }
    if (line == SMS_H + 1) d->status |= 0x80;
    unsigned long target = s->cpu.cyc + CYC_PER_LINE;
    while (s->cpu.cyc < target) {
      if (irq_line(s)) { s->cpu.int_pending = 1; s->cpu.int_data = 0xFF; }
      else s->cpu.int_pending = 0;
      if (s->hooks.on_exec) s->hooks.on_exec(s, s->cpu.pc, sms_rom_offset(s, s->cpu.pc));
      z80_step(&s->cpu);
    }
  }
  s->frame++;
}

bool sms_save_png(sms_t* s, const char* path) {
  // writes binary PPM (convert externally)
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  fprintf(f, "P6\n%d %d\n255\n", SMS_W, SMS_H);
  for (int i = 0; i < SMS_W * SMS_H; i++) {
    uint32_t c = s->fb[i];
    uint8_t rgb[3] = {(c >> 16) & 255, (c >> 8) & 255, c & 255};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  return true;
}
