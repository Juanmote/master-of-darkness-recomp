// Headless runner for testing: scripted input, wide/4:3 screenshots (PPM), states, audio dump.
// usage: headless rom.sms -f FRAMES [-i input.txt] [-s f1,f2] [-x ext] [-o dir] [-l state] [-S frame:state] [-a audio.raw]
#include "../src/core/sms.h"
#include "../src/game/mod_wide.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sms_t S;
typedef struct { int from, to; uint8_t btn; } span_t;
static span_t spans[4096]; static int nspans;

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
  }
  static uint8_t rom[1 << 20];
  FILE* f = fopen(argv[1], "rb"); int n = (int)fread(rom, 1, sizeof rom, f); fclose(f);
  sms_init(&S, rom, n);
  size_t ssz = sms_state_size(); uint8_t* sbuf = malloc(ssz);
  if (load) {
    f = fopen(load, "rb"); size_t r = fread(sbuf, 1, ssz, f); fclose(f);
    if (!sms_load_state(&S, sbuf, r)) { fprintf(stderr, "bad state\n"); return 1; }
  }
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
  for (int i = 0; i < frames; i++) {
    uint32_t fr = S.st.frame;
    S.input = 0;
    for (int k = 0; k < nspans; k++) if ((int)fr >= spans[k].from && (int)fr <= spans[k].to) S.input |= spans[k].btn;
    sms_run_frame(&S);
    mod_wide_info_t info;
    mod_render(&S, pix, W, ext, &info);
    if (info.map_valid) valid_frames++;
    if (af) fwrite(S.audio, 2, SMS_SAMPLES_PER_FRAME, af);
    char key[32], shl[8192];
    snprintf(key, sizeof key, ",%u,", fr); snprintf(shl, sizeof shl, ",%s,", shots);
    if (strstr(shl, key)) {
      char p[512]; snprintf(p, sizeof p, "%s/f%06u.ppm", outdir, fr);
      FILE* o = fopen(p, "wb"); fprintf(o, "P6\n%d %d\n255\n", W, SMS_H);
      for (int k = 0; k < W * SMS_H; k++) { uint8_t c[3] = {pix[k] >> 16, pix[k] >> 8, pix[k]}; fwrite(c, 1, 3, o); }
      fclose(o);
      printf("shot %u valid=%d cam=%d limits=%d..%d\n", fr, info.map_valid, info.cam, info.left_limit, info.right_limit);
    }
    if ((int)fr == save_frame) { sms_save_state(&S, sbuf); f = fopen(save_path, "wb"); fwrite(sbuf, 1, ssz, f); fclose(f); }
  }
  if (af) fclose(af);
  printf("frames %d, playfield detected in %d\n", frames, valid_frames);
  return 0;
}
