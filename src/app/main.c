// Master of Darkness Recomp — launcher (SDL2). Runs on desktop and Android.
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/sms.h"
#include "../game/mod_wide.h"
#include "../game/mod_enhance.h"
#include "font_data.h"

#define APP_VERSION "0.3.2"
#define KNOWN_CRC 0x96FB4D4Bu
#define UI_W 640
#define UI_H 360
#define WIDE_W (SMS_W + 2 * MOD_WIDE_EXT)

// ---------------------------------------------------------------- config
enum { SCALE_FIT, SCALE_INT, SCALE_STRETCH, SCALE_COUNT };
enum { FILTER_SHARP, FILTER_SHARP_SMOOTH, FILTER_SMOOTH, FILTER_SCANLINES, FILTER_COUNT };

typedef struct {
  int widescreen;
  int crt;         // 4:3 mode with the 8:7 pixels of a CRT TV
  int scale;
  int filter;
  int volume;      // 0..10
  int swap_ab;
  int turbo;       // double CPU speed: removes the original slowdowns     // 0: south=jump east=attack, 1: east=jump south=attack
} config_t;

static config_t cfg = {1, 0, SCALE_FIT, FILTER_SHARP_SMOOTH, 8, 0, 1};
static char pref_path[1024];

static void cfg_path(char* out, size_t n, const char* name) { snprintf(out, n, "%s%s", pref_path, name); }

static void cfg_load(void) {
  char p[1100]; cfg_path(p, sizeof p, "config.ini");
  FILE* f = fopen(p, "r");
  if (!f) return;
  char line[128];
  while (fgets(line, sizeof line, f)) {
    int v;
    if (sscanf(line, "widescreen=%d", &v) == 1) cfg.widescreen = v ? 1 : 0;
    else if (sscanf(line, "crt=%d", &v) == 1) cfg.crt = v ? 1 : 0;
    else if (sscanf(line, "scale=%d", &v) == 1 && v >= 0 && v < SCALE_COUNT) cfg.scale = v;
    else if (sscanf(line, "filter=%d", &v) == 1 && v >= 0 && v < FILTER_COUNT) cfg.filter = v;
    else if (sscanf(line, "volume=%d", &v) == 1 && v >= 0 && v <= 10) cfg.volume = v;
    else if (sscanf(line, "swap_ab=%d", &v) == 1) cfg.swap_ab = v ? 1 : 0;
    else if (sscanf(line, "turbo=%d", &v) == 1) cfg.turbo = v ? 1 : 0;
  }
  fclose(f);
}

static void cfg_save(void) {
  char p[1100]; cfg_path(p, sizeof p, "config.ini");
  FILE* f = fopen(p, "w");
  if (!f) return;
  fprintf(f, "widescreen=%d\ncrt=%d\nscale=%d\nfilter=%d\nvolume=%d\nswap_ab=%d\nturbo=%d\n",
          cfg.widescreen, cfg.crt, cfg.scale, cfg.filter, cfg.volume, cfg.swap_ab, cfg.turbo);
  fclose(f);
}

// ---------------------------------------------------------------- util
static uint32_t crc32_buf(const uint8_t* d, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

static uint8_t* load_file(const char* path, size_t* size) {
  SDL_RWops* rw = SDL_RWFromFile(path, "rb");
  if (!rw) return NULL;
  Sint64 n = SDL_RWsize(rw);
  if (n <= 0 || n > (1 << 22)) { SDL_RWclose(rw); return NULL; }
  uint8_t* buf = malloc((size_t)n);
  size_t got = SDL_RWread(rw, buf, 1, (size_t)n);
  SDL_RWclose(rw);
  if (got != (size_t)n) { free(buf); return NULL; }
  *size = (size_t)n;
  return buf;
}

// ---------------------------------------------------------------- UI drawing (UI_W x UI_H ARGB)
static uint32_t ui[UI_W * UI_H];

static void ui_clear(void) { memset(ui, 0, sizeof ui); }

static void ui_rect(int x, int y, int w, int h, uint32_t argb) {
  for (int j = y; j < y + h; j++) {
    if (j < 0 || j >= UI_H) continue;
    for (int i = x; i < x + w; i++) if (i >= 0 && i < UI_W) ui[j * UI_W + i] = argb;
  }
}

static int glyph_index(uint32_t cp) {
  for (int i = 0; i < FONT_COUNT; i++) if (font_codepoints[i] == cp) return i;
  return '?' - 32;
}

static uint32_t utf8_next(const char** s) {
  const unsigned char* p = (const unsigned char*)*s;
  uint32_t cp;
  if (p[0] < 0x80) { cp = p[0]; *s += 1; }
  else if ((p[0] & 0xE0) == 0xC0) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *s += 2; }
  else if ((p[0] & 0xF0) == 0xE0) { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *s += 3; }
  else { cp = '?'; *s += 1; }
  return cp;
}

static int text_width(const char* s) { int n = 0; while (*s) { utf8_next(&s); n++; } return n * FONT_CW; }

static void ui_text(int x, int y, const char* s, uint32_t rgb) {
  while (*s) {
    int g = glyph_index(utf8_next(&s));
    for (int j = 0; j < FONT_CH; j++) for (int i = 0; i < FONT_CW; i++) {
      int a = font_alpha[g][j * FONT_CW + i];
      int px = x + i, py = y + j;
      if (!a || px < 0 || px >= UI_W || py < 0 || py >= UI_H) continue;
      uint32_t* d = &ui[py * UI_W + px];
      uint32_t da = *d >> 24;
      // blend text over current UI pixel
      uint32_t r = (((rgb >> 16) & 255) * a + ((*d >> 16) & 255) * (255 - a)) / 255;
      uint32_t gg = (((rgb >> 8) & 255) * a + ((*d >> 8) & 255) * (255 - a)) / 255;
      uint32_t b = ((rgb & 255) * a + (*d & 255) * (255 - a)) / 255;
      uint32_t oa = a + da * (255 - a) / 255;
      *d = (oa << 24) | (r << 16) | (gg << 8) | b;
    }
    x += FONT_CW;
  }
}

static void ui_text_center(int y, const char* s, uint32_t rgb) { ui_text((UI_W - text_width(s)) / 2, y, s, rgb); }

// ---------------------------------------------------------------- app state
static SDL_Window* win;
static SDL_Renderer* ren;
static SDL_Texture *tex_game, *tex_ui, *tex_scan, *tex_sharp;
static int tex_game_w, sharp_k;
static SDL_AudioDeviceID audio_dev;
static SDL_GameController* pads[8];

static sms_t S;
static mod_enhance_t E;
static uint8_t* rom;
static size_t rom_size;
static uint32_t rom_crc;
static uint32_t* pix;

static int menu_open, menu_sel;
static char toast[128];
static Uint32 toast_until;
static int running = 1;
static uint8_t kb_input, pad_input;
static int16_t stick_x[8], stick_y[8];

static void show_toast(const char* msg, Uint32 ms) {
  snprintf(toast, sizeof toast, "%s", msg);
  toast_until = SDL_GetTicks() + ms;
}

enum { MI_RESUME, MI_FORMAT, MI_SCALE, MI_FILTER, MI_VOLUME, MI_BUTTONS, MI_TURBO, MI_SAVE, MI_LOAD, MI_RESET, MI_QUIT, MI_COUNT };

static const char* scale_names[SCALE_COUNT] = {"Ajustar a pantalla", "Escala entera", "Estirar"};
static const char* filter_names[FILTER_COUNT] = {"Nítido", "Nítido suavizado", "Suave", "Scanlines"};

static void menu_label(int i, char* out, size_t n) {
  switch (i) {
  case MI_RESUME: snprintf(out, n, "Continuar"); break;
  case MI_FORMAT: snprintf(out, n, "Formato:  ← %s →", cfg.widescreen ? "16:9 panorámico" : cfg.crt ? "Tele de tubo (8:7)" : "4:3 píxel cuadrado"); break;
  case MI_SCALE: snprintf(out, n, "Escalado: ← %s →", scale_names[cfg.scale]); break;
  case MI_FILTER: snprintf(out, n, "Filtro:   ← %s →", filter_names[cfg.filter]); break;
  case MI_VOLUME: snprintf(out, n, "Volumen:  ← %d%% →", cfg.volume * 10); break;
  case MI_BUTTONS: snprintf(out, n, "Botones:  ← %s →", cfg.swap_ab ? "Saltar con el derecho" : "Saltar con el inferior"); break;
  case MI_TURBO: snprintf(out, n, "Ralentizaciones: ← %s →", cfg.turbo ? "Eliminadas" : "Originales"); break;
  case MI_SAVE: snprintf(out, n, "Guardar estado"); break;
  case MI_LOAD: snprintf(out, n, "Cargar estado"); break;
  case MI_RESET: snprintf(out, n, "Reiniciar juego"); break;
  case MI_QUIT: snprintf(out, n, "Salir"); break;
  }
}

static void state_file(char* out, size_t n) { cfg_path(out, n, "estado1.sav"); }

static void do_save_state(void) {
  char p[1100]; state_file(p, sizeof p);
  size_t n = sms_state_size();
  void* buf = malloc(n);
  sms_save_state(&S, buf);
  SDL_RWops* rw = SDL_RWFromFile(p, "wb");
  if (rw) { SDL_RWwrite(rw, buf, 1, n); SDL_RWclose(rw); show_toast("Estado guardado", 1500); }
  else show_toast("No se pudo guardar el estado", 2000);
  free(buf);
}

static void do_load_state(void) {
  char p[1100]; state_file(p, sizeof p);
  size_t n = 0;
  uint8_t* buf = load_file(p, &n);
  if (buf && sms_load_state(&S, buf, n)) show_toast("Estado cargado", 1500);
  else show_toast("No hay ningún estado guardado", 2000);
  free(buf);
}

static void menu_change(int dir) {
  switch (menu_sel) {
  case MI_FORMAT: {
    // cycle 16:9 -> 4:3 -> CRT 8:7
    int f = cfg.widescreen ? 0 : cfg.crt ? 2 : 1;
    f = (f + dir + 3) % 3;
    cfg.widescreen = f == 0;
    cfg.crt = f == 2;
    mod_enhance_set_wide(&E, cfg.widescreen, MOD_WIDE_EXT);
    break;
  }
  case MI_SCALE: cfg.scale = (cfg.scale + dir + SCALE_COUNT) % SCALE_COUNT; break;
  case MI_FILTER: cfg.filter = (cfg.filter + dir + FILTER_COUNT) % FILTER_COUNT; break;
  case MI_VOLUME: cfg.volume += dir; if (cfg.volume < 0) cfg.volume = 0; if (cfg.volume > 10) cfg.volume = 10; break;
  case MI_BUTTONS: cfg.swap_ab ^= 1; break;
  case MI_TURBO: cfg.turbo ^= 1; break;
  default: return;
  }
  cfg_save();
}

static void menu_set_open(int open) {
  menu_open = open;
  if (open) { menu_sel = 0; SDL_ClearQueuedAudio(audio_dev); }
}

static void menu_activate(void) {
  switch (menu_sel) {
  case MI_RESUME: menu_set_open(0); break;
  case MI_SAVE: do_save_state(); menu_set_open(0); break;
  case MI_LOAD: do_load_state(); menu_set_open(0); break;
  case MI_RESET: sms_reset(&S); menu_set_open(0); show_toast("Juego reiniciado", 1500); break;
  case MI_QUIT: running = 0; break;
  default: menu_change(1); break;
  }
}

static void menu_nav(int dy, int dx, int ok, int back) {
  if (dy) menu_sel = (menu_sel + dy + MI_COUNT) % MI_COUNT;
  if (dx) menu_change(dx);
  if (ok) menu_activate();
  if (back) menu_set_open(0);
}

static void draw_ui(void) {
  ui_clear();
  if (menu_open) {
    ui_rect(0, 0, UI_W, UI_H, 0xA0000000);
    int bw = 480, bh = 40 + MI_COUNT * 26 + 30;
    int bx = (UI_W - bw) / 2, by = (UI_H - bh) / 2;
    ui_rect(bx, by, bw, bh, 0xE0140C18);
    ui_rect(bx, by, bw, 2, 0xFFB0302A);
    ui_rect(bx, by + bh - 2, bw, 2, 0xFFB0302A);
    ui_text_center(by + 10, "MASTER OF DARKNESS", 0xE8C060);
    for (int i = 0; i < MI_COUNT; i++) {
      char l[128]; menu_label(i, l, sizeof l);
      int y = by + 40 + i * 26;
      if (i == menu_sel) ui_rect(bx + 10, y - 3, bw - 20, 24, 0xFF5A1A1A);
      ui_text(bx + 24, y, l, i == menu_sel ? 0xFFFFFF : 0xB8B0A8);
    }
    ui_text_center(by + bh - 26, "v" APP_VERSION "  ·  Recomp por Juanmote", 0x807870);
  }
  if (toast[0] && SDL_GetTicks() < toast_until) {
    int w = text_width(toast) + 24;
    ui_rect((UI_W - w) / 2, UI_H - 44, w, 28, 0xD0000000);
    ui_text_center(UI_H - 40, toast, 0xFFFFFF);
  }
}

// ---------------------------------------------------------------- video
static void ensure_textures(int game_w) {
  if (!tex_game || tex_game_w != game_w) {
    if (tex_game) SDL_DestroyTexture(tex_game);
    tex_game = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, game_w, SMS_H);
    tex_game_w = game_w;
    if (tex_sharp) { SDL_DestroyTexture(tex_sharp); tex_sharp = NULL; }
  }
  if (!tex_ui) {
    tex_ui = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, UI_W, UI_H);
    SDL_SetTextureBlendMode(tex_ui, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex_ui, SDL_ScaleModeLinear);
  }
  if (!tex_scan) {
    // 4 texture rows per game line: last one darkened
    uint32_t px[SMS_H * 4];
    for (int i = 0; i < SMS_H * 4; i++) px[i] = (i % 4 == 3) ? 0x90000000 : (i % 4 == 2 ? 0x30000000 : 0x00000000);
    tex_scan = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, SMS_H * 4);
    SDL_UpdateTexture(tex_scan, NULL, px, 4);
    SDL_SetTextureBlendMode(tex_scan, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex_scan, SDL_ScaleModeLinear);
  }
}

static SDL_Rect dest_rect(int ww, int wh, int gw) {
  SDL_Rect r;
  double par = (!cfg.widescreen && cfg.crt) ? 8.0 / 7.0 : 1.0;  // pixel aspect ratio
  double sx = (double)ww / (gw * par), sy = (double)wh / SMS_H;
  double s = sx < sy ? sx : sy;
  if (cfg.scale == SCALE_STRETCH) { r.x = 0; r.y = 0; r.w = ww; r.h = wh; return r; }
  if (cfg.scale == SCALE_INT && s >= 1.0) s = floor(s);
  r.w = (int)(gw * par * s + 0.5); r.h = (int)(SMS_H * s + 0.5);
  r.x = (ww - r.w) / 2; r.y = (wh - r.h) / 2;
  return r;
}

static void present(void) {
  int gw = cfg.widescreen ? WIDE_W : SMS_W;
  ensure_textures(gw);
  mod_render(&S, pix, gw, cfg.widescreen ? MOD_WIDE_EXT : 0, NULL, E.wide ? E.shown : NULL, E.wide ? E.n_shown : 0);
  SDL_UpdateTexture(tex_game, NULL, pix, gw * 4);
  int ww, wh;
  SDL_GetRendererOutputSize(ren, &ww, &wh);
  SDL_Rect dst = dest_rect(ww, wh, gw);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
  SDL_RenderClear(ren);
  if (cfg.filter == FILTER_SHARP_SMOOTH) {
    // integer nearest prescale, then linear down to the final size
    int k = (int)ceil((double)dst.h / SMS_H);
    if (k < 1) k = 1;
    if (k > 8) k = 8;
    if (!tex_sharp || sharp_k != k) {
      if (tex_sharp) SDL_DestroyTexture(tex_sharp);
      tex_sharp = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, gw * k, SMS_H * k);
      sharp_k = k;
    }
    if (tex_sharp) {
      SDL_SetTextureScaleMode(tex_game, SDL_ScaleModeNearest);
      SDL_SetRenderTarget(ren, tex_sharp);
      SDL_RenderCopy(ren, tex_game, NULL, NULL);
      SDL_SetRenderTarget(ren, NULL);
      SDL_SetTextureScaleMode(tex_sharp, SDL_ScaleModeLinear);
      SDL_RenderCopy(ren, tex_sharp, NULL, &dst);
    } else {
      SDL_RenderCopy(ren, tex_game, NULL, &dst);
    }
  } else {
    SDL_SetTextureScaleMode(tex_game, cfg.filter == FILTER_SMOOTH ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    SDL_RenderCopy(ren, tex_game, NULL, &dst);
    if (cfg.filter == FILTER_SCANLINES) SDL_RenderCopy(ren, tex_scan, NULL, &dst);
  }
  draw_ui();
  SDL_UpdateTexture(tex_ui, NULL, ui, UI_W * 4);
  SDL_Rect udst = {0, 0, ww, wh};
  double us = (double)ww / UI_W < (double)wh / UI_H ? (double)ww / UI_W : (double)wh / UI_H;
  udst.w = (int)(UI_W * us); udst.h = (int)(UI_H * us);
  udst.x = (ww - udst.w) / 2; udst.y = (wh - udst.h) / 2;
  SDL_RenderCopy(ren, tex_ui, NULL, &udst);
  SDL_RenderPresent(ren);
}

// ---------------------------------------------------------------- input
static uint8_t keymap(SDL_Keycode k) {
  switch (k) {
  case SDLK_UP: return BTN_UP; case SDLK_DOWN: return BTN_DOWN;
  case SDLK_LEFT: return BTN_LEFT; case SDLK_RIGHT: return BTN_RIGHT;
  case SDLK_z: case SDLK_j: return BTN_1;
  case SDLK_x: case SDLK_k: case SDLK_SPACE: return BTN_2;
  case SDLK_RETURN: return BTN_PAUSE;
  }
  return 0;
}

static uint8_t padmap(int b) {
  int jump_btn = cfg.swap_ab ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A;
  int atk_btn = cfg.swap_ab ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B;
  if (b == jump_btn) return BTN_2;
  if (b == atk_btn || b == SDL_CONTROLLER_BUTTON_X || b == SDL_CONTROLLER_BUTTON_Y) return BTN_1;
  switch (b) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
  case SDL_CONTROLLER_BUTTON_START: return BTN_PAUSE;
  }
  return 0;
}

static uint8_t pad_buttons_held;
static uint8_t stick_dirs(void) {
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    if (stick_x[i] < -16000) b |= BTN_LEFT;
    if (stick_x[i] > 16000) b |= BTN_RIGHT;
    if (stick_y[i] < -16000) b |= BTN_UP;
    if (stick_y[i] > 16000) b |= BTN_DOWN;
  }
  return b;
}

static void open_pad(int idx) {
  if (!SDL_IsGameController(idx)) return;
  SDL_GameController* c = SDL_GameControllerOpen(idx);
  if (!c) return;
  for (int i = 0; i < 8; i++) if (!pads[i]) { pads[i] = c; return; }
  SDL_GameControllerClose(c);
}

static int pad_slot(SDL_JoystickID id) {
  for (int i = 0; i < 8; i++)
    if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id) return i;
  return -1;
}

static void handle_event(SDL_Event* e) {
  switch (e->type) {
  case SDL_QUIT: running = 0; break;
  case SDL_APP_WILLENTERBACKGROUND: cfg_save(); if (!menu_open) menu_set_open(1); break;
  case SDL_KEYDOWN:
    if (e->key.repeat && !menu_open) break;
    if (e->key.keysym.sym == SDLK_ESCAPE || e->key.keysym.sym == SDLK_AC_BACK || e->key.keysym.sym == SDLK_TAB) {
      menu_set_open(!menu_open); break;
    }
    if (menu_open) {
      SDL_Keycode k = e->key.keysym.sym;
      menu_nav(k == SDLK_UP ? -1 : k == SDLK_DOWN ? 1 : 0, k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0,
               k == SDLK_RETURN || k == SDLK_z || k == SDLK_SPACE, k == SDLK_x || k == SDLK_BACKSPACE);
      break;
    }
    if (e->key.keysym.sym == SDLK_F5) { do_save_state(); break; }
    if (e->key.keysym.sym == SDLK_F8) { do_load_state(); break; }
    kb_input |= keymap(e->key.keysym.sym);
    break;
  case SDL_KEYUP: kb_input &= ~keymap(e->key.keysym.sym); break;
  case SDL_CONTROLLERDEVICEADDED: open_pad(e->cdevice.which); break;
  case SDL_CONTROLLERDEVICEREMOVED: {
    int s = pad_slot(e->cdevice.which);
    if (s >= 0) { SDL_GameControllerClose(pads[s]); pads[s] = NULL; stick_x[s] = stick_y[s] = 0; }
    break;
  }
  case SDL_CONTROLLERBUTTONDOWN: {
    int b = e->cbutton.button;
    if (b == SDL_CONTROLLER_BUTTON_BACK || b == SDL_CONTROLLER_BUTTON_GUIDE || b == SDL_CONTROLLER_BUTTON_MISC1) {
      menu_set_open(!menu_open); break;
    }
    if (menu_open) {
      int ok_btn = cfg.swap_ab ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A;
      int back_btn = cfg.swap_ab ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B;
      menu_nav(b == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : b == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? 1 : 0,
               b == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? -1 : b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? 1 : 0,
               b == ok_btn || b == SDL_CONTROLLER_BUTTON_START, b == back_btn);
      break;
    }
    pad_buttons_held |= padmap(b);
    break;
  }
  case SDL_CONTROLLERBUTTONUP: pad_buttons_held &= ~padmap(e->cbutton.button); break;
  case SDL_CONTROLLERAXISMOTION: {
    int s = pad_slot(e->caxis.which);
    if (s < 0) break;
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) stick_x[s] = e->caxis.value;
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) stick_y[s] = e->caxis.value;
    break;
  }
  }
}

// ---------------------------------------------------------------- ROM
static const char* rom_error;

static void find_and_load_rom(int argc, char** argv) {
  const char* candidates[4];
  int n = 0;
  char p1[1100], p2[1100];
  if (argc > 1) candidates[n++] = argv[1];
#ifdef __ANDROID__
  snprintf(p1, sizeof p1, "%s/rom.sms", SDL_AndroidGetInternalStoragePath());
  candidates[n++] = p1;
#else
  candidates[n++] = "mod.sms";
  cfg_path(p1, sizeof p1, "rom.sms");
  candidates[n++] = p1;
#endif
  (void)p2;
  for (int i = 0; i < n; i++) {
    rom = load_file(candidates[i], &rom_size);
    if (rom) break;
  }
  if (!rom) { rom_error = "No se encontró la ROM de Master of Darkness."; return; }
  if (rom_size < 0x8000 || rom_size > (1 << 20) || memcmp(rom + 0x7FF0, "TMR SEGA", 8) != 0) {
    rom_error = "El archivo no es una ROM válida de Master System.";
    free(rom); rom = NULL; return;
  }
  rom_crc = crc32_buf(rom, rom_size);
}

static void error_screen(void) {
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = 0;
      if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_FINGERDOWN) running = 0;
      if (e.type == SDL_CONTROLLERDEVICEADDED) open_pad(e.cdevice.which);
    }
    ui_clear();
    ui_rect(0, 0, UI_W, UI_H, 0xFF100808);
    ui_text_center(110, "MASTER OF DARKNESS", 0xE8C060);
    ui_text_center(160, rom_error, 0xFFFFFF);
#ifdef __ANDROID__
    ui_text_center(190, "Cierra y vuelve a abrir la app para elegir la ROM.", 0xB8B0A8);
#else
    ui_text_center(190, "Coloca la ROM como mod.sms junto al ejecutable.", 0xB8B0A8);
#endif
    ui_text_center(250, "Pulsa cualquier botón para salir", 0x807870);
    ensure_textures(SMS_W);
    SDL_UpdateTexture(tex_ui, NULL, ui, UI_W * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex_ui, NULL, NULL);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
  }
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
  SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  char* pp = SDL_GetPrefPath("Juanmote", "MasterOfDarkness");
  snprintf(pref_path, sizeof pref_path, "%s", pp ? pp : "./");
  SDL_free(pp);
  cfg_load();

  Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
  wflags |= SDL_WINDOW_FULLSCREEN;
#endif
  win = SDL_CreateWindow("Master of Darkness", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDE_W * 3, SMS_H * 3, wflags);
  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
  pix = malloc(sizeof(uint32_t) * WIDE_W * SMS_H);

  for (int i = 0; i < SDL_NumJoysticks(); i++) open_pad(i);

  find_and_load_rom(argc, argv);
  if (!rom) { error_screen(); SDL_Quit(); return 0; }

  SDL_AudioSpec want = {0}, have;
  want.freq = SMS_AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
  audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);

  sms_init(&S, rom, (int)rom_size);
  mod_enhance_attach(&S, &E);
  mod_enhance_set_wide(&E, cfg.widescreen, MOD_WIDE_EXT);
  if (rom_crc != KNOWN_CRC) show_toast("ROM no reconocida: algunas mejoras pueden fallar", 4000);
  else show_toast("Pulsa SELECT o ATRÁS para abrir el menú", 4000);

  const double frame_time = 1.0 / 60.0;
  double acc = 0;
  Uint64 last = SDL_GetPerformanceCounter();
  const double freq = (double)SDL_GetPerformanceFrequency();
  int16_t mixbuf[SMS_SAMPLES_PER_FRAME];

  // Automated test mode (desktop only): MOD_AUTOTEST="frames:out.bmp" MOD_INPUT=script MOD_MENU=1
  const char* autotest = getenv("MOD_AUTOTEST");
  if (autotest) {
    int total = 0; char out[512] = "";
    sscanf(autotest, "%d:%511s", &total, out);
    struct { int a, b; uint8_t btn; } spans[256]; int ns = 0;
    const char* inp = getenv("MOD_INPUT");
    FILE* f = inp ? fopen(inp, "r") : NULL;
    char line[128];
    while (f && fgets(line, sizeof line, f) && ns < 256) {
      int a, b; char t[32] = "";
      if (sscanf(line, "%d %d %31s", &a, &b, t) >= 2) {
        uint8_t m = 0;
        for (char* c = t; *c; c++) m |= *c == 'R' ? BTN_RIGHT : *c == 'L' ? BTN_LEFT : *c == 'U' ? BTN_UP : *c == 'D' ? BTN_DOWN : *c == '1' ? BTN_1 : *c == '2' ? BTN_2 : 0;
        spans[ns].a = a; spans[ns].b = b; spans[ns].btn = m; ns++;
      }
    }
    if (f) fclose(f);
    for (int fr = 0; fr < total; fr++) {
      S.input = 0;
      for (int k = 0; k < ns; k++) if (fr >= spans[k].a && fr <= spans[k].b) S.input |= spans[k].btn;
      sms_run_frame(&S);
      mod_enhance_frame(&S, &E);
    }
    if (getenv("MOD_MENU")) { menu_open = 1; menu_sel = atoi(getenv("MOD_MENU")); }
    toast_until = 0;
    present();
    int ww, wh; SDL_GetRendererOutputSize(ren, &ww, &wh);
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
    SDL_SaveBMP(sf, out);
    SDL_Quit();
    return 0;
  }

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(&e);
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (now - last) / freq;
    last = now;
    if (dt > 0.1) dt = 0.1;
    if (!menu_open) {
      // keep the audio queue around 3 frames by nudging the emulation speed slightly
      double queued = audio_dev ? SDL_GetQueuedAudioSize(audio_dev) / 2.0 / SMS_SAMPLES_PER_FRAME : 3.0;
      double speed = 1.0 + 0.01 * (3.0 - queued);
      if (speed < 0.98) speed = 0.98;
      if (speed > 1.02) speed = 1.02;
      acc += dt * speed;
      int frames = 0;
      while (acc >= frame_time && frames < 4) {
        acc -= frame_time;
        frames++;
        S.input = kb_input | pad_buttons_held | stick_dirs();
        // faster CPU only while playing: menus and story screens time themselves with CPU loops
        S.cpu_mult = (cfg.turbo && mod_is_gameplay(&S)) ? 2 : 1;
        sms_run_frame(&S);
        mod_enhance_frame(&S, &E);
        if (audio_dev && queued < 8) {
          for (int i = 0; i < SMS_SAMPLES_PER_FRAME; i++) mixbuf[i] = (int16_t)(S.audio[i] * cfg.volume / 10);
          SDL_QueueAudio(audio_dev, mixbuf, sizeof mixbuf);
        }
      }
      if (frames == 4) acc = 0;
    }
    present();
    if (menu_open) SDL_Delay(1);
  }
  cfg_save();
  SDL_Quit();
  return 0;
}
