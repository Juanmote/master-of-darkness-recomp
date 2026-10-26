// Analysis driver: runs the ROM with a scripted input, writes screenshots,
// code/data coverage and logs of VDP activity.
//
// usage: trace rom.sms -f FRAMES [-i input.txt] [-s f1,f2,...] [-o outdir]
//        [-l state.bin to load] [-S frame:state.bin to save] [-v vdp log from frame]
// input.txt lines: "from to BUTTONS" with BUTTONS from U D L R 1 2 P (or -)
#include "sms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sms_t S;
static uint8_t cov[ROM_MAX];   // bit0 exec start, bit1 read
static FILE* vlog;
static uint32_t vlog_from = 0xFFFFFFFF;
static uint32_t cur_frame;

typedef struct { int from, to; uint8_t btn; } span_t;
static span_t spans[4096];
static int nspans;

static void h_exec(sms_t* s, uint16_t pc, int off) { if (off >= 0) cov[off] |= 1; }
static void h_read(sms_t* s, uint16_t a, int off) { if (off >= 0) cov[off] |= 2; }
static void h_reg(sms_t* s, int r, uint8_t v) {
  if (vlog && s->frame >= vlog_from)
    fprintf(vlog, "F%u L%d REG%d=%02X pc=%04X\n", s->frame, s->vdp.line, r, v, s->cpu.pc);
}
static void h_vram(sms_t* s, uint16_t a, uint8_t v) {
  if (vlog && s->frame >= vlog_from)
    fprintf(vlog, "F%u L%d VRAM %04X=%02X pc=%04X\n", s->frame, s->vdp.line, a, v, s->cpu.pc);
}
static int wlo=-1, whi=-1;
static void h_ramw(sms_t* s, uint16_t a, uint8_t v) {
  if (vlog && s->frame >= vlog_from && a >= wlo && a <= whi)
    fprintf(vlog, "F%u RAMW %04X=%02X pc=%04X b=%02X,%02X\n", s->frame, a, v, s->cpu.pc, s->bank[1], s->bank[2]);
}
static void h_bank(sms_t* s, int slot, uint8_t b) {
  if (vlog && s->frame >= vlog_from)
    fprintf(vlog, "F%u BANK%d=%02X pc=%04X\n", s->frame, slot, b, s->cpu.pc);
}

static uint8_t parse_btn(const char* t) {
  uint8_t b = 0;
  for (; *t; t++) switch (*t) {
    case 'U': b |= BTN_UP; break; case 'D': b |= BTN_DOWN; break;
    case 'L': b |= BTN_LEFT; break; case 'R': b |= BTN_RIGHT; break;
    case '1': b |= BTN_1; break; case '2': b |= BTN_2; break;
    case 'P': b |= BTN_PAUSE; break;
  }
  return b;
}

static void set_hooks(void) {
  S.hooks.on_exec = h_exec; S.hooks.on_read = h_read;
  S.hooks.on_vdp_reg = h_reg; S.hooks.on_vram_write = h_vram; S.hooks.on_bank = h_bank; S.hooks.on_ram_write = h_ramw;
}

int main(int argc, char** argv) {
  const char* rom = argv[1];
  int frames = 600; const char* input = NULL; const char* shots = "";
  const char* outdir = "out"; const char* load = NULL; const char* save = NULL;
  int shot_every = 0;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "-f")) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-i")) input = argv[++i];
    else if (!strcmp(argv[i], "-s")) shots = argv[++i];
    else if (!strcmp(argv[i], "-e")) shot_every = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-o")) outdir = argv[++i];
    else if (!strcmp(argv[i], "-l")) load = argv[++i];
    else if (!strcmp(argv[i], "-S")) save = argv[++i];
    else if (!strcmp(argv[i], "-w")) sscanf(argv[++i], "%x-%x", &wlo, &whi);
    else if (!strcmp(argv[i], "-v")) vlog_from = atoi(argv[++i]);
  }
  FILE* f = fopen(rom, "rb");
  static uint8_t buf[ROM_MAX];
  int n = fread(buf, 1, ROM_MAX, f); fclose(f);
  sms_init(&S, buf, n);
  if (load) {
    FILE* lf = fopen(load, "rb"); fread(&S, sizeof S, 1, lf); fclose(lf);
    S.cpu.userdata = &S;
    sms_t tmp; sms_init(&tmp, buf, 16384);
    S.cpu.read_byte = tmp.cpu.read_byte; S.cpu.write_byte = tmp.cpu.write_byte;
    S.cpu.port_in = tmp.cpu.port_in; S.cpu.port_out = tmp.cpu.port_out;
    FILE* cf = fopen("cov.bin", "rb"); if (cf) { fread(cov, 1, ROM_MAX, cf); fclose(cf); }
  }
  set_hooks();
  if (input) {
    FILE* in = fopen(input, "r"); char line[256];
    while (fgets(line, sizeof line, in)) {
      int a, b; char t[64] = "";
      if (line[0] == '#') continue;
      if (sscanf(line, "%d %d %63s", &a, &b, t) >= 2) { spans[nspans++] = (span_t){a, b, parse_btn(t)}; }
    }
    fclose(in);
  }
  char cmd[256]; snprintf(cmd, sizeof cmd, "mkdir -p %s", outdir); system(cmd);
  if (vlog_from != 0xFFFFFFFF) { char p[256]; snprintf(p, sizeof p, "%s/vdp.log", outdir); vlog = fopen(p, "w"); }
  int save_frame = -1; char save_path[256] = "";
  if (save) sscanf(save, "%d:%255s", &save_frame, save_path);
  uint32_t start = S.frame;
  for (int i = 0; i < frames; i++) {
    uint32_t fr = S.frame;
    S.input = 0;
    for (int k = 0; k < nspans; k++) if ((int)fr >= spans[k].from && (int)fr <= spans[k].to) S.input |= spans[k].btn;
    sms_run_frame(&S);
    char key[32]; snprintf(key, sizeof key, ",%u,", fr);
    char shl[4096]; snprintf(shl, sizeof shl, ",%s,", shots);
    if (strstr(shl, key) || (shot_every && fr % shot_every == 0)) {
      char p[256]; snprintf(p, sizeof p, "%s/f%06u.ppm", outdir, fr); sms_save_png(&S, p);
    }
    if ((int)fr == save_frame) {
      FILE* sf = fopen(save_path, "wb"); fwrite(&S, sizeof S, 1, sf); fclose(sf);
    }
  }
  (void)start;
  char p[256];
  snprintf(p, sizeof p, "%s/cov.bin", outdir); f = fopen(p, "wb"); fwrite(cov, 1, n, f); fclose(f);
  snprintf(p, sizeof p, "%s/ram.bin", outdir); f = fopen(p, "wb"); fwrite(S.ram, 1, 0x2000, f); fclose(f);
  snprintf(p, sizeof p, "%s/vram.bin", outdir); f = fopen(p, "wb"); fwrite(S.vdp.vram, 1, 0x4000, f); fclose(f);
  snprintf(p, sizeof p, "%s/cram.bin", outdir); f = fopen(p, "wb"); fwrite(S.vdp.cram, 1, 32, f); fclose(f);
  snprintf(p, sizeof p, "%s/regs.txt", outdir); f = fopen(p, "w");
  for (int r = 0; r < 11; r++) fprintf(f, "R%d=%02X\n", r, S.vdp.reg[r]);
  fprintf(f, "banks %02X %02X %02X frame %u pc %04X\n", S.bank[0], S.bank[1], S.bank[2], S.frame, S.cpu.pc);
  fclose(f);
  if (vlog) fclose(vlog);
  return 0;
}
