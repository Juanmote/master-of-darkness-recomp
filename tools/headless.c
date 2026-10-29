// Headless runner for testing: scripted input, wide/4:3 screenshots (PPM), states, audio dump.
// usage: headless rom.sms -f FRAMES [-i input.txt] [-s f1,f2] [-x ext] [-o dir] [-l state] [-S frame:state] [-a audio.raw]
#include "../src/core/sms.h"
#include "../src/game/mod_wide.h"
#include "../src/game/mod_enhance.h"
#ifdef MOD_HAVE_RECOMP
#include "../src/recomp/recomp_rt.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int mod_debug_compare(const sms_t* s, uint8_t* mask);

static sms_t S;
static mod_enhance_t E;
static sms_t T;  // scratch machine for verification
static long verify_blocks, verify_fail;
static int cpu_equal(const z80* a, const z80* b) {
#define F(x) if (a->x != b->x) { fprintf(stderr, "  %s: rec=%lX ref=%lX\n", #x, (unsigned long)a->x, (unsigned long)b->x); ok = 0; }
  int ok = 1;
  F(pc) F(sp) F(ix) F(iy) F(mem_ptr) F(a) F(b) F(c) F(d) F(e) F(h) F(l)
  F(a_) F(b_) F(c_) F(d_) F(e_) F(h_) F(l_) F(f_) F(i) F(r) F(cyc)
  F(sf) F(zf) F(yf) F(hf) F(xf) F(pf) F(nf) F(cf) F(iff_delay) F(interrupt_mode) F(iff1) F(iff2) F(halted)
#undef F
  return ok;
}
static void verify_cb(sms_t* s, const sms_state_t* before, int psg_n) {
  verify_blocks++;
  T.st = *before;
  T.rom = s->rom; T.rom_size = s->rom_size; T.input = s->input;
  T.st.cpu.userdata = &T;
  T.st.cpu.int_pending = 0; T.st.cpu.nmi_pending = 0;
  T.psg_log_n = psg_n;
  memcpy(T.psg_log, s->psg_log, sizeof(T.psg_log[0]) * psg_n);
  T.frame_start_cyc = s->frame_start_cyc;
  int n = 0;
  while (T.st.cpu.cyc < s->st.cpu.cyc && n < 100000) { z80_exec_one(&T.st.cpu); n++; }
  int ok = cpu_equal(&s->st.cpu, &T.st.cpu);
  if (memcmp(s->st.ram, T.st.ram, sizeof T.st.ram)) { fprintf(stderr, "  RAM differs\n"); ok = 0; }
  if (memcmp(s->st.bank, T.st.bank, 3) || s->st.mapper_ctrl != T.st.mapper_ctrl) { fprintf(stderr, "  banks differ\n"); ok = 0; }
  if (memcmp(&s->st.vdp, &T.st.vdp, sizeof T.st.vdp)) { fprintf(stderr, "  VDP differs\n"); ok = 0; }
  if (s->psg_log_n != T.psg_log_n || memcmp(s->psg_log, T.psg_log, sizeof(T.psg_log[0]) * T.psg_log_n)) { fprintf(stderr, "  PSG differs\n"); ok = 0; }
  if (!ok) {
    verify_fail++;
    fprintf(stderr, "MISMATCH frame %u block start pc=%04X bank2=%02X (ref ran %d instr)\n", s->st.frame, before->cpu.pc, before->bank[2], n);
    if (verify_fail > 5) exit(2);
    // resync so we can keep looking for other problems
    s->st = T.st; s->st.cpu.userdata = s;
    s->st.cpu.read_byte = T.st.cpu.read_byte;
  }
}
static uint8_t cov[1 << 20];
static uint32_t ram_exec;
static int watch_addr = -1;
static int satlines;
static void h_vramw(sms_t* s, uint16_t a, uint8_t v) {
  if (satlines && a >= 0x3F00 && a < 0x3F40) { static int lastf = -1; if ((int)s->st.frame != lastf) { lastf = s->st.frame; printf("SATW frame %u line %d\n", s->st.frame, s->st.vdp.line); } }
}
static int objlog;
static void h_ramw(sms_t* s, uint16_t a, uint8_t v) {
  if (objlog && a >= 0xCA40 && a < 0xCF20 && (a & 0x1F) == 0) {
    uint8_t* sl = &s->st.ram[(a & 0x1FFF)];
    int x = (int16_t)(sl[4] | (sl[5] << 8));
    int y = (int16_t)(sl[2] | (sl[3] << 8));
    static uint8_t prev[0x2000];
    int was = prev[a & 0x1FFF]; prev[a & 0x1FFF] = v;
    if ((v == 0) != (was == 0))
      printf("O %s slot %04X frame %u pc %04X bank %02X x=%d y=%d cam=%d\n", v ? "NEW " : "FREE", a, s->st.frame, s->st.cpu.pc, s->st.bank[2], x, y, s->st.ram[0x6A0] | (s->st.ram[0x6A1] << 8));
  }
  if (a == watch_addr) printf("W %04X=%02X frame %u pc %04X line %d cyc %lu\n", a, v, s->st.frame, s->st.cpu.pc, s->st.vdp.line, (unsigned long)s->st.cpu.cyc);
}
static unsigned long tr_from, tr_to;
static void h_exec(sms_t* s, uint16_t pc, int off) {
  if (off >= 0) cov[off] |= 1; else ram_exec++;
  if (tr_to && s->st.cpu.cyc >= tr_from && s->st.cpu.cyc <= tr_to) printf("X pc=%04X cyc=%lu iff1=%d pend=%d\n", pc, (unsigned long)s->st.cpu.cyc, s->st.cpu.iff1, s->st.cpu.int_pending);
}
typedef struct { int from, to; uint8_t btn; } span_t;
static span_t spans[4096]; static int nspans;
static struct { int from, to; int addr; int val; } pokes[64]; static int npokes;

static uint8_t parse_btn(const char* t) {
  uint8_t b = 0;
  for (; *t; t++) switch (*t) {
    case 'U': b |= BTN_UP; break; case 'D': b |= BTN_DOWN; break;
    case 'L': b |= BTN_LEFT; break; case 'R': b |= BTN_RIGHT; break;
    case '1': b |= BTN_1; break; case '2': b |= BTN_2; break; case 'P': b |= BTN_PAUSE; break;
  }
  return b;
}

int main(int argc, char** argv) {
  int frames = 600, ext = MOD_WIDE_EXT;
  int enhance = 0, oc = 1; const char *hashpath = NULL; const char *covpath = NULL; int seed = 0, use_recomp = 0, do_verify = 0;
  const char *input = NULL, *shots = "", *outdir = "out", *load = NULL, *save = NULL, *audio = NULL;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "-f")) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-i")) input = argv[++i];
    else if (!strcmp(argv[i], "-s")) shots = argv[++i];
    else if (!strcmp(argv[i], "-x")) ext = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-o")) outdir = argv[++i];
    else if (!strcmp(argv[i], "-l")) load = argv[++i];
    else if (!strcmp(argv[i], "-S")) save = argv[++i];
    else if (!strcmp(argv[i], "-a")) audio = argv[++i];
    else if (!strcmp(argv[i], "-c")) covpath = argv[++i];
    else if (!strcmp(argv[i], "-R")) use_recomp = 1;
    else if (!strcmp(argv[i], "-E")) enhance = 1;
    else if (!strcmp(argv[i], "-O")) oc = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-H")) hashpath = argv[++i];
    else if (!strcmp(argv[i], "-V")) { use_recomp = 1; do_verify = 1; }
    else if (!strcmp(argv[i], "-r")) seed = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-p")) { sscanf(argv[++i], "%d-%d:%x=%x", &pokes[npokes].from, &pokes[npokes].to, &pokes[npokes].addr, &pokes[npokes].val); npokes++; }
  }
  static uint8_t rom[1 << 20];
  FILE* f = fopen(argv[1], "rb"); int n = (int)fread(rom, 1, sizeof rom, f); fclose(f);
  sms_init(&S, rom, n);
  size_t ssz = sms_state_size(); uint8_t* sbuf = malloc(ssz);
  if (load) {
    f = fopen(load, "rb"); size_t r = fread(sbuf, 1, ssz, f); fclose(f);
    if (!sms_load_state(&S, sbuf, r)) { fprintf(stderr, "bad state\n"); return 1; }
  }
  if (enhance) { mod_enhance_attach(&S, &E); mod_enhance_set_wide(&E, ext > 0, ext); }
#ifdef MOD_HAVE_RECOMP
  if (use_recomp) S.recomp_run = recomp_run;
#else
  if (use_recomp) { fprintf(stderr, "built without recompiled code (run tools/recomp/recomp.py first)\n"); return 1; }
#endif
  if (getenv("DJ")) E.double_jump = true;
  if (getenv("DIFF")) E.difficulty = atoi(getenv("DIFF")); else E.difficulty = 1;
  if (getenv("RESPAWN")) E.respawn = atoi(getenv("RESPAWN"));
  if (getenv("HERO")) { static uint8_t hm[16]; memset(hm, 0xFF, 16); const char* h = getenv("HERO"); int i, c, n; while (sscanf(h, "%x:%x%n", &i, &c, &n) == 2) { hm[i & 15] = c; h += n; if (*h == ',') h++; else break; } mod_set_hero_colors(hm); }
  if (getenv("TRACE")) { sscanf(getenv("TRACE"), "%lu-%lu", &tr_from, &tr_to); S.hooks.on_exec = h_exec; }
  if (getenv("SATLINES")) { satlines = 1; S.hooks.on_vram_write = h_vramw; }
  if (getenv("OBJLOG")) { objlog = 1; S.hooks.on_ram_write = h_ramw; }
  if (getenv("WATCH")) { watch_addr = (int)strtol(getenv("WATCH"), NULL, 16); S.hooks.on_ram_write = h_ramw; }
#ifdef MOD_HAVE_RECOMP
  if (do_verify) { sms_init(&T, rom, n); S.recomp_verify = verify_cb; rt_single = 1; }
#endif
  if (covpath) { FILE* cf = fopen(covpath, "rb"); if (cf) { if (fread(cov, 1, sizeof cov, cf)) {} fclose(cf); } S.hooks.on_exec = h_exec; }
  if (input) {
    FILE* in = fopen(input, "r"); char line[256];
    while (fgets(line, sizeof line, in)) {
      int a, b; char t[64] = "";
      if (line[0] == '#') continue;
      if (sscanf(line, "%d %d %63s", &a, &b, t) >= 2) spans[nspans++] = (span_t){a, b, parse_btn(t)};
    }
    fclose(in);
  }
  char cmd[512]; snprintf(cmd, sizeof cmd, "mkdir -p %s", outdir); if (system(cmd)) {}
  int save_frame = -1; char save_path[256] = "";
  if (save) sscanf(save, "%d:%255s", &save_frame, save_path);
  FILE* af = audio ? fopen(audio, "wb") : NULL;
  int W = SMS_W + 2 * ext;
  uint32_t* pix = malloc(sizeof(uint32_t) * W * SMS_H);
  int valid_frames = 0;
  FILE* hf = hashpath ? fopen(hashpath, "w") : NULL;
  for (int i = 0; i < frames; i++) {
    uint32_t fr = S.st.frame;
    S.input = 0;
    for (int k = 0; k < nspans; k++) if ((int)fr >= spans[k].from && (int)fr <= spans[k].to) S.input |= spans[k].btn;
    if (seed) {  // random bot after frame 1300
      static uint32_t rs; static uint8_t held; static int hold_left;
      if (!rs) rs = seed * 2654435761u;
      if (fr > 1300) {
        if (hold_left-- <= 0) {
          rs = rs * 1103515245u + 12345u; uint32_t r = rs >> 8;
          held = 0;
          int d = r % 10;
          if (d < 5) held |= BTN_RIGHT; else if (d < 6) held |= BTN_LEFT; else if (d < 7) held |= BTN_UP | BTN_RIGHT; else if (d < 8) held |= BTN_DOWN;
          if ((r >> 4) % 3 == 0) held |= BTN_2;
          if ((r >> 6) % 3 == 0) held |= BTN_1;
          if ((r >> 8) % 7 == 0) held |= BTN_UP | BTN_1;
          hold_left = 4 + (r >> 12) % 30;
        }
        S.input |= held;
        if (fr % 600 == 0) S.input |= BTN_1;  // continue screens etc.
      }
    }
    for (int k = 0; k < npokes; k++) if ((int)fr >= pokes[k].from && (int)fr <= pokes[k].to) S.st.ram[pokes[k].addr & 0x1FFF] = pokes[k].val;
    S.cpu_mult = (oc > 1 && mod_is_gameplay(&S)) ? oc : 1;
    if (getenv("WPN") && (int)fr == atoi(getenv("WPN"))) { S.st.ram[0x1FB7] = 2; S.st.ram[0x1FB8] = 3; S.st.ram[0x1FB9] = 99; S.st.ram[0x1FBA] = 9; mod_request_hud(&E, 7); }
    { static uint32_t lp; uint32_t p0 = E.passes; sms_run_frame(&S); if (getenv("PASSLOG")) printf("PASS %u %u\n", fr, E.passes - p0); if (getenv("LAGLOG") && E.passes == p0 && lp) printf("LAG %u cam %d\n", fr, S.st.ram[0x6A0] | (S.st.ram[0x6A1] << 8)); lp = E.passes; }
    mod_enhance_frame(&S, &E);
    mod_wide_info_t info;
    mod_render(&S, pix, W, ext, &info, E.wide ? E.shown : NULL, E.wide ? E.n_shown : 0);
    if (info.map_valid) valid_frames++;
    if (getenv("PLTILES")) {
      int n = 0, miny = 999;
      for (int j = 0; j < 64; j++) { if (S.sat_mid[j] == 0xD0) break; int t = S.sat_mid[0x81 + j*2]; if (t >= 1 && t <= 9 && S.sat_mid[j] < 0xC0) { n++; if (S.sat_mid[j] < miny) miny = S.sat_mid[j]; } }
      int py = (int16_t)(S.st.ram[0xA22] | (S.st.ram[0xA23] << 8));
      printf("PL %u n=%d miny=%d py=%d\n", fr, n, miny, py);
    }
    if (getenv("HUDANY")) {
      int n = 0; for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 256; xx++) n += S.fb_spr[yy][xx];
      int pl = (int16_t)(S.st.ram[0xA22] | (S.st.ram[0xA23] << 8));
      printf("HUDANY frame %u hudpix %d playerY %d\n", fr, n, pl);
    }
    if (getenv("HUDSPR") && info.map_valid) {
      int n = 0; for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 256; xx++) n += S.fb_spr[yy][xx];
      int cxv = info.cam - (info.cam - ext < info.left_limit ? info.left_limit : (info.cam - ext + W > info.right_limit && info.right_limit - info.left_limit >= W ? info.right_limit - W : info.cam - ext));
      (void)cxv; if (n && info.cx != ext) printf("HUDSPR frame %u pixels %d cx %d\n", fr, n, info.cx);
    }
    if (getenv("SATCHECK") && E.wide) {
      // every in_sat piece shown must be in the VDP sprite table with the same position
      uint16_t sat = (S.st.vdp.reg[5] & 0x7E) << 7;
      int miss = 0, tot = 0;
      const mod_piece_t* L = E.shown; int LN = E.n_shown;
      for (int k = 0; k < LN; k++) {
        if (!L[k].in_sat) continue;
        tot++;
        int found = 0;
        for (int j = 0; j < 64; j++) {
          if (S.sat_mid[j] == (uint8_t)L[k].y && S.sat_mid[0x80 + j * 2] == (uint8_t)L[k].x) { found = 1; break; }
        }
        if (!found) miss++;
      }
      if (tot) printf("SAT frame %u pieces %d missing %d\n", fr, tot, miss);
    }
    if (getenv("VRAMDUMP") && fr % 8 == 0) { static FILE* vd; if (!vd) vd = fopen(getenv("VRAMDUMP"), "wb"); fwrite(S.st.vdp.vram, 1, 0x4000, vd); fwrite(S.st.vdp.cram, 1, 32, vd); fwrite(&S.st.vdp.reg[6], 1, 1, vd); }
    if (getenv("RAMDUMP")) { static FILE* rd; if (!rd) rd = fopen(getenv("RAMDUMP"), "wb"); fwrite(S.st.ram, 1, 0x2000, rd); }
    if (hf) {
      uint32_t h = 2166136261u;
      const uint8_t* p1 = (const uint8_t*)S.fb_idx; for (size_t k = 0; k < sizeof S.fb_idx; k++) h = (h ^ p1[k]) * 16777619u;
      for (size_t k = 0; k < sizeof S.st.ram; k++) h = (h ^ S.st.ram[k]) * 16777619u;
      const uint8_t* p2 = (const uint8_t*)S.audio; for (size_t k = 0; k < sizeof S.audio; k++) h = (h ^ p2[k]) * 16777619u;
      uint32_t hf_ = 2166136261u; for (size_t k = 0; k < sizeof S.fb_idx; k++) hf_ = (hf_ ^ p1[k]) * 16777619u;
      uint32_t hr_ = 2166136261u; for (size_t k = 0; k < sizeof S.st.ram; k++) hr_ = (hr_ ^ S.st.ram[k]) * 16777619u;
      uint32_t ha_ = 2166136261u; for (size_t k = 0; k < sizeof S.audio; k++) ha_ = (ha_ ^ p2[k]) * 16777619u;
      fprintf(hf, "%u %08X %lu fb=%08X ram=%08X au=%08X\n", fr, h, (unsigned long)S.st.cpu.cyc, hf_, hr_, ha_);
    }
    if (getenv("MOD_CMP")) { static uint8_t mk[SMS_W*SMS_H]; int d = mod_debug_compare(&S, mk); if (d > 0) { printf("F%u cam=%d mismatch=%d\n", fr, info.cam, d); char mp[512]; snprintf(mp, sizeof mp, "%s/m%06u.pgm", outdir, fr); FILE* o = fopen(mp, "wb"); fprintf(o, "P5\n256 192\n1\n"); fwrite(mk, 1, sizeof mk, o); fclose(o); } }
    if (af) fwrite(S.audio, 2, SMS_SAMPLES_PER_FRAME, af);
    char key[32], shl[8192];
    snprintf(key, sizeof key, ",%u,", fr); snprintf(shl, sizeof shl, ",%s,", shots);
    if (strstr(shl, key)) {
      char p[512]; snprintf(p, sizeof p, "%s/f%06u.ppm", outdir, fr);
      FILE* o = fopen(p, "wb"); fprintf(o, "P6\n%d %d\n255\n", W, SMS_H);
      for (int k = 0; k < W * SMS_H; k++) { uint8_t c[3] = {pix[k] >> 16, pix[k] >> 8, pix[k]}; fwrite(c, 1, 3, o); }
      fclose(o);
      if (getenv("SATDUMP")) { printf("SAT %u:", fr); for (int j = 0; j < 64; j++) printf(" %02X/%02X/%02X", S.sat_mid[j], S.sat_mid[0x80 + j*2], S.sat_mid[0x81 + j*2]); printf("\n"); }
      printf("shot %u valid=%d cam=%d limits=%d..%d\n", fr, info.map_valid, info.cam, info.left_limit, info.right_limit);
    }
    if ((int)fr == save_frame) { sms_save_state(&S, sbuf); f = fopen(save_path, "wb"); fwrite(sbuf, 1, ssz, f); fclose(f); }
  }
  if (af) fclose(af);
  if (hf) fclose(hf);
  if (enhance) printf("sprite passes %u in %d frames\n", E.passes, frames);
  if (covpath) { FILE* cf = fopen(covpath, "wb"); fwrite(cov, 1, n, cf); fclose(cf); printf("ram exec %u\n", ram_exec); }
  { char p[512]; FILE* o;
    snprintf(p, sizeof p, "%s/ram.bin", outdir); o = fopen(p, "wb"); fwrite(S.st.ram, 1, 0x2000, o); fclose(o);
    snprintf(p, sizeof p, "%s/vram.bin", outdir); o = fopen(p, "wb"); fwrite(S.st.vdp.vram, 1, 0x4000, o); fclose(o);
    snprintf(p, sizeof p, "%s/cram.bin", outdir); o = fopen(p, "wb"); fwrite(S.st.vdp.cram, 1, 32, o); fclose(o); }
  printf("frames %d, playfield detected in %d\n", frames, valid_frames);
  if (use_recomp) printf("interpreter fallback steps (last frame) %u, last pc %04X\n", S.interp_steps, S.last_interp_pc);
  if (do_verify) printf("verified blocks %ld, mismatches %ld\n", verify_blocks, verify_fail);
  return 0;
}
