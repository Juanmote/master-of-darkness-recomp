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

#define APP_VERSION "0.5.1"
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
  int jump_btn;    // SDL controller button that jumps (the other face buttons attack)
  int sub_btn;     // button that throws the subweapon (up + attack), -1 none
  int turbo;       // double CPU speed: removes the original slowdowns
  int lang;        // 0: original (English), 1: translation patch chosen by the user
  int suit;        // hero colours (0 = original)
  int cheat_inv;   // official "hit check" cheat: no damage
  int cheat_lives; // lives never run out
  int triggers;    // L2 rewinds, R2 fast-forwards
} config_t;

static config_t cfg = {1, 0, SCALE_FIT, FILTER_SHARP_SMOOTH, 8, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_Y, 1, 0, 0, 0, 0, 1};
static char pref_path[1024];

static void cfg_path(char* out, size_t n, const char* name) { snprintf(out, n, "%s%s", pref_path, name); }

static void cfg_load(void) {
  char p[1100]; cfg_path(p, sizeof p, "config.ini");
  FILE* f = fopen(p, "r");
  if (!f) return;
  char line[128];
  int jump_seen = 0;
  while (fgets(line, sizeof line, f)) {
    int v;
    if (sscanf(line, "widescreen=%d", &v) == 1) cfg.widescreen = v ? 1 : 0;
    else if (sscanf(line, "crt=%d", &v) == 1) cfg.crt = v ? 1 : 0;
    else if (sscanf(line, "scale=%d", &v) == 1 && v >= 0 && v < SCALE_COUNT) cfg.scale = v;
    else if (sscanf(line, "filter=%d", &v) == 1 && v >= 0 && v < FILTER_COUNT) cfg.filter = v;
    else if (sscanf(line, "volume=%d", &v) == 1 && v >= 0 && v <= 10) cfg.volume = v;
    else if (sscanf(line, "swap_ab=%d", &v) == 1 && !jump_seen) cfg.jump_btn = v ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A;
    else if (sscanf(line, "jump_btn=%d", &v) == 1 && v >= SDL_CONTROLLER_BUTTON_A && v <= SDL_CONTROLLER_BUTTON_Y) { cfg.jump_btn = v; jump_seen = 1; }
    else if (sscanf(line, "sub_btn=%d", &v) == 1 && v >= -1 && v <= SDL_CONTROLLER_BUTTON_Y) cfg.sub_btn = v;
    else if (sscanf(line, "turbo=%d", &v) == 1) cfg.turbo = v ? 1 : 0;
    else if (sscanf(line, "lang=%d", &v) == 1) cfg.lang = v ? 1 : 0;
    else if (sscanf(line, "suit=%d", &v) == 1 && v >= 0 && v < 16) cfg.suit = v;
    else if (sscanf(line, "cheat_inv=%d", &v) == 1) cfg.cheat_inv = v ? 1 : 0;
    else if (sscanf(line, "cheat_lives=%d", &v) == 1) cfg.cheat_lives = v ? 1 : 0;
    else if (sscanf(line, "triggers=%d", &v) == 1) cfg.triggers = v ? 1 : 0;
  }
  fclose(f);
  if (cfg.sub_btn == cfg.jump_btn) cfg.sub_btn = cfg.jump_btn == SDL_CONTROLLER_BUTTON_Y ? SDL_CONTROLLER_BUTTON_X : SDL_CONTROLLER_BUTTON_Y;
}

static void cfg_save(void) {
  char p[1100]; cfg_path(p, sizeof p, "config.ini");
  FILE* f = fopen(p, "w");
  if (!f) return;
  fprintf(f, "widescreen=%d\ncrt=%d\nscale=%d\nfilter=%d\nvolume=%d\njump_btn=%d\nsub_btn=%d\nturbo=%d\nlang=%d\n"
             "suit=%d\ncheat_inv=%d\ncheat_lives=%d\ntriggers=%d\n",
          cfg.widescreen, cfg.crt, cfg.scale, cfg.filter, cfg.volume, cfg.jump_btn, cfg.sub_btn, cfg.turbo, cfg.lang,
          cfg.suit, cfg.cheat_inv, cfg.cheat_lives, cfg.triggers);
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
static uint8_t* rom_orig;   // the user's ROM as loaded (the patch is applied on a copy)

// IPS patch: records [offset 3][size 2][data] or [offset 3][0 0][run 2][value 1], "EOF"
static int ips_apply(uint8_t* dst, size_t size, const uint8_t* p, size_t n) {
  if (n < 8 || memcmp(p, "PATCH", 5)) return 0;
  size_t i = 5;
  while (i + 3 <= n && memcmp(p + i, "EOF", 3)) {
    if (i + 5 > n) return 0;
    size_t off = ((size_t)p[i] << 16) | (p[i + 1] << 8) | p[i + 2], len = (p[i + 3] << 8) | p[i + 4];
    i += 5;
    if (len) {
      if (i + len > n || off + len > size) return 0;
      memcpy(dst + off, p + i, len); i += len;
    } else {
      if (i + 3 > n) return 0;
      size_t run = (p[i] << 8) | p[i + 1];
      if (off + run > size) return 0;
      memset(dst + off, p[i + 2], run); i += 3;
    }
  }
  return 1;
}

// Translation patch: an IPS file chosen by the user (like the ROM), kept as patch.ips.
#define WAVE_PATCH_CRC 0x1173EA42u   // Spanish translation v1.2 (C) 2018 Traducciones Wave
static uint8_t* patch_data;
static size_t patch_size;
static uint32_t patch_crc;

static void patch_path(char* out, size_t n) {
#ifdef __ANDROID__
  snprintf(out, n, "%s/patch.ips", SDL_AndroidGetInternalStoragePath());
#else
  cfg_path(out, n, "patch.ips");
#endif
}

static int load_patch(void) {
  char p[1100]; patch_path(p, sizeof p);
  size_t n = 0;
  uint8_t* d = load_file(p, &n);
  if (!d || n < 8 || memcmp(d, "PATCH", 5)) { free(d); return 0; }
  free(patch_data);
  patch_data = d; patch_size = n; patch_crc = crc32_buf(d, n);
  return 1;
}

static void apply_language(void) {
  if (!rom || !rom_orig) return;
  memcpy(rom, rom_orig, rom_size);
  if (cfg.lang && patch_data && rom_crc == KNOWN_CRC && !ips_apply(rom, rom_size, patch_data, patch_size))
    memcpy(rom, rom_orig, rom_size);
}

static int lang_active(void) { return cfg.lang && patch_data && rom_crc == KNOWN_CRC; }

// Opens the system file picker (Android): the result is copied to patch.ips by GameActivity.
static int waiting_patch;
static void request_patch_file(void) {
#ifdef __ANDROID__
  waiting_patch = 1;
  SDL_AndroidSendMessage(0x8001, 0);
#endif
}

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

// ---------------------------------------------------------------- hero colours
// light and dark shade of the suit (sprite palette entries 10 and 11), SMS colours %00BBGGRR
static const struct { const char* name; uint8_t light, dark; } suits[] = {
  {"Original", 0xFF, 0xFF}, {"Rojo", 0x17, 0x02}, {"Verde", 0x1D, 0x08}, {"Morado", 0x37, 0x21},
  {"Marrón", 0x1B, 0x06}, {"Negro", 0x2A, 0x15}, {"Blanco", 0x3F, 0x2A}, {"Dorado", 0x0F, 0x0A},
};
#define SUIT_COUNT (int)(sizeof suits / sizeof suits[0])

static void apply_suit(void) {
  if (cfg.suit <= 0 || cfg.suit >= SUIT_COUNT) { mod_set_hero_colors(NULL); return; }
  uint8_t m[16];
  memset(m, 0xFF, sizeof m);
  m[10] = suits[cfg.suit].light;
  m[11] = suits[cfg.suit].dark;
  mod_set_hero_colors(m);
}

// ---------------------------------------------------------------- cheats (game RAM, see docs)
#define RAM_INVINCIBLE 0xDFB4   // official debug menu "HIT CHECK": non-zero = no damage
#define RAM_LIVES      0xDFBA
#define RAM_WEAPON     0xDFB7   // 0 knife, 1 pick, 2 rapier, 3 axe
#define RAM_SUBWEAPON  0xDFB8   // 0 pistol, 1 boomerang, 2 darts, 3 bombs
#define RAM_AMMO       0xDFB9   // 0 = no subweapon
#define RAM_ROUND      0xDFC0
#define RAM_STAGE      0xDFC1
static inline uint8_t gram(uint16_t a) { return S.st.ram[a & 0x1FFF]; }
static inline void gram_w(uint16_t a, uint8_t v) { S.st.ram[a & 0x1FFF] = v; }

static const char* weapon_names[4] = {"Cuchillo", "Pico", "Estoque", "Hacha"};
static const char* sub_names[5] = {"Ninguna", "Pistola", "Bumerán", "Dardos", "Bombas"};
static const uint8_t start_points[][2] = {{0,0},{0,1},{0,2},{1,0},{1,1},{1,2},{2,0},{2,1},{2,2},{3,0},{3,1},{3,2},{4,0}};
#define START_COUNT (int)(sizeof start_points / sizeof start_points[0])
static int start_sel;
static int warp_pending;   // keep the chosen round in RAM until the level is running

static int cheats_ok(void) { return rom_crc == KNOWN_CRC && mod_is_gameplay(&S); }

static void cheats_frame(void) {
  if (rom_crc != KNOWN_CRC) return;
  if (warp_pending) {
    gram_w(RAM_ROUND, start_points[start_sel][0]);
    gram_w(RAM_STAGE, start_points[start_sel][1]);
    if (mod_is_gameplay(&S)) warp_pending = 0;
  }
  if (!mod_is_gameplay(&S)) return;
  if (cfg.cheat_inv) gram_w(RAM_INVINCIBLE, 0xFF);
  if (cfg.cheat_lives && gram(RAM_LIVES) < 3) { gram_w(RAM_LIVES, 3); mod_request_hud(&E, MOD_HUD_LIVES); }
}

// ---------------------------------------------------------------- menu
enum {
  MI_RESUME, MI_SAVE, MI_LOAD, MI_CHEATS, MI_VIDEO, MI_OPTIONS, MI_RESET, MI_QUIT,
  MI_FORMAT, MI_SCALE, MI_FILTER, MI_SUIT,
  MI_VOLUME, MI_BUTTONS, MI_SUBBTN, MI_TURBO, MI_TRIGGERS, MI_LANG, MI_PATCH,
  MI_INV, MI_LIVES, MI_WEAPON, MI_SUB, MI_START, MI_WARP,
  MI_BACK
};
enum { PAGE_MAIN, PAGE_VIDEO, PAGE_OPTIONS, PAGE_CHEATS };
static const int page_items[4][10] = {
  {MI_RESUME, MI_SAVE, MI_LOAD, MI_CHEATS, MI_VIDEO, MI_OPTIONS, MI_RESET, MI_QUIT, -1},
  {MI_FORMAT, MI_SCALE, MI_FILTER, MI_SUIT, MI_BACK, -1},
  {MI_VOLUME, MI_BUTTONS, MI_SUBBTN, MI_TURBO, MI_TRIGGERS, MI_LANG, MI_PATCH, MI_BACK, -1},
  {MI_INV, MI_LIVES, MI_WEAPON, MI_SUB, MI_START, MI_WARP, MI_BACK, -1},
};
static const char* page_titles[4] = {"MASTER OF DARKNESS", "IMAGEN", "OPCIONES", "TRUCOS"};
static int menu_page;

static int page_count(int p) { int n = 0; while (n < 10 && page_items[p][n] >= 0) n++; return n; }
static int cur_item(void) { return page_items[menu_page][menu_sel]; }

static void menu_set_open(int open);
static int capture_jump;   // waiting for the player to press a button: 1 jump, 2 subweapon

static const char* button_name(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_A: return "A";
  case SDL_CONTROLLER_BUTTON_B: return "B";
  case SDL_CONTROLLER_BUTTON_X: return "X";
  case SDL_CONTROLLER_BUTTON_Y: return "Y";
  }
  return "?";
}

static const char* scale_names[SCALE_COUNT] = {"Ajustar a pantalla", "Escala entera", "Estirar"};
static const char* filter_names[FILTER_COUNT] = {"Nítido", "Nítido suavizado", "Suave", "Scanlines"};

static const char* patch_name(void) {
  if (!patch_data) return "ninguno (elegir archivo)";
  return patch_crc == WAVE_PATCH_CRC ? "Español, Traducciones Wave" : "parche propio";
}

static void menu_label(int i, char* out, size_t n) {
  switch (i) {
  case MI_RESUME: snprintf(out, n, "Continuar"); break;
  case MI_SAVE: snprintf(out, n, "Guardar estado"); break;
  case MI_LOAD: snprintf(out, n, "Cargar estado"); break;
  case MI_CHEATS: snprintf(out, n, "Trucos  ›"); break;
  case MI_VIDEO: snprintf(out, n, "Imagen  ›"); break;
  case MI_OPTIONS: snprintf(out, n, "Opciones  ›"); break;
  case MI_RESET: snprintf(out, n, "Reiniciar juego"); break;
  case MI_QUIT: snprintf(out, n, "Salir"); break;
  case MI_BACK: snprintf(out, n, "‹  Volver"); break;
  case MI_FORMAT: snprintf(out, n, "Formato:  ← %s →", cfg.widescreen ? "16:9 panorámico" : cfg.crt ? "Tele de tubo (8:7)" : "4:3 píxel cuadrado"); break;
  case MI_SCALE: snprintf(out, n, "Escalado: ← %s →", scale_names[cfg.scale]); break;
  case MI_FILTER: snprintf(out, n, "Filtro:   ← %s →", filter_names[cfg.filter]); break;
  case MI_SUIT: snprintf(out, n, "Traje del Dr. Social: ← %s →", suits[cfg.suit].name); break;
  case MI_VOLUME: snprintf(out, n, "Volumen:  ← %d%% →", cfg.volume * 10); break;
  case MI_BUTTONS:
    if (capture_jump == 1) snprintf(out, n, "Saltar:   pulsa el botón que quieras...");
    else snprintf(out, n, "Saltar:   botón %s", button_name(cfg.jump_btn));
    break;
  case MI_SUBBTN:
    if (capture_jump == 2) snprintf(out, n, "Subarma:  pulsa el botón que quieras...");
    else if (cfg.sub_btn < 0) snprintf(out, n, "Subarma:  sin botón (arriba + ataque)");
    else snprintf(out, n, "Subarma:  botón %s (o arriba + ataque)", button_name(cfg.sub_btn));
    break;
  case MI_TURBO: snprintf(out, n, "Ralentizaciones: ← %s →", cfg.turbo ? "Eliminadas" : "Originales"); break;
  case MI_TRIGGERS: snprintf(out, n, "Gatillos (L2 rebobinar, R2 acelerar): ← %s →", cfg.triggers ? "Sí" : "No"); break;
  case MI_LANG: snprintf(out, n, "Idioma:   ← %s →", lang_active() ? "Traducido (parche)" : "Inglés (original)"); break;
  case MI_PATCH: snprintf(out, n, "Parche:   %s", patch_name()); break;
  case MI_INV: snprintf(out, n, "Invencible:      ← %s →", cfg.cheat_inv ? "Sí" : "No"); break;
  case MI_LIVES: snprintf(out, n, "Vidas infinitas: ← %s →", cfg.cheat_lives ? "Sí" : "No"); break;
  case MI_WEAPON: snprintf(out, n, "Arma:     ← %s →", weapon_names[gram(RAM_WEAPON) & 3]); break;
  case MI_SUB: snprintf(out, n, "Subarma:  ← %s →", gram(RAM_AMMO) ? sub_names[1 + (gram(RAM_SUBWEAPON) & 3)] : sub_names[0]); break;
  case MI_START: snprintf(out, n, "Ronda:    ← %d-%d →", start_points[start_sel][0] + 1, start_points[start_sel][1] + 1); break;
  case MI_WARP: snprintf(out, n, "Empezar partida en la ronda %d-%d", start_points[start_sel][0] + 1, start_points[start_sel][1] + 1); break;
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

static void set_language(int lang) {
  if (lang && rom_crc != KNOWN_CRC) { show_toast("La traducción necesita la ROM original sin modificar", 3000); return; }
  if (lang && !patch_data) {
#ifdef __ANDROID__
    show_toast("Elige el archivo del parche (.ips)", 2500);
    request_patch_file();
#else
    show_toast("Copia el parche como patch.ips en la carpeta de datos", 3000);
#endif
    return;
  }
  cfg.lang = lang;
  apply_language();
  sms_reset(&S);
  menu_set_open(0);
  show_toast(lang ? "Juego traducido (reiniciado)" : "Juego en inglés (reiniciado)", 2500);
}

static void menu_change(int dir) {
  switch (cur_item()) {
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
  case MI_SUIT: cfg.suit = (cfg.suit + dir + SUIT_COUNT) % SUIT_COUNT; apply_suit(); break;
  case MI_VOLUME: cfg.volume += dir; if (cfg.volume < 0) cfg.volume = 0; if (cfg.volume > 10) cfg.volume = 10; break;
  case MI_BUTTONS: capture_jump = 1; return;
  case MI_SUBBTN: capture_jump = 2; return;
  case MI_TURBO: cfg.turbo ^= 1; break;
  case MI_TRIGGERS: cfg.triggers ^= 1; break;
  case MI_LANG: set_language(!lang_active()); break;
  case MI_PATCH:
#ifdef __ANDROID__
    request_patch_file();
#else
    show_toast("Copia el parche como patch.ips en la carpeta de datos", 3000);
#endif
    return;
  case MI_INV:
    cfg.cheat_inv ^= 1;
    if (!cfg.cheat_inv && rom_crc == KNOWN_CRC) gram_w(RAM_INVINCIBLE, 0);
    break;
  case MI_LIVES: cfg.cheat_lives ^= 1; break;
  case MI_WEAPON:
    if (!cheats_ok()) { show_toast("Solo durante la partida", 1500); return; }
    gram_w(RAM_WEAPON, (uint8_t)((gram(RAM_WEAPON) + dir + 4) & 3));
    mod_request_hud(&E, MOD_HUD_WEAPON);
    return;
  case MI_SUB: {
    if (!cheats_ok()) { show_toast("Solo durante la partida", 1500); return; }
    int k = gram(RAM_AMMO) ? 1 + (gram(RAM_SUBWEAPON) & 3) : 0;
    k = (k + dir + 5) % 5;
    if (k == 0) gram_w(RAM_AMMO, 0);
    else { gram_w(RAM_SUBWEAPON, (uint8_t)(k - 1)); gram_w(RAM_AMMO, 99); }
    mod_request_hud(&E, MOD_HUD_SUBWEAPON);
    return;
  }
  case MI_START: start_sel = (start_sel + dir + START_COUNT) % START_COUNT; return;
  default: return;
  }
  cfg_save();
}

static void menu_set_open(int open) {
  menu_open = open;
  capture_jump = 0;
  if (open) { menu_page = PAGE_MAIN; menu_sel = 0; SDL_ClearQueuedAudio(audio_dev); }
}

static void menu_goto(int page) { menu_page = page; menu_sel = 0; capture_jump = 0; }

static void menu_activate(void) {
  switch (cur_item()) {
  case MI_RESUME: menu_set_open(0); break;
  case MI_SAVE: do_save_state(); menu_set_open(0); break;
  case MI_LOAD: do_load_state(); menu_set_open(0); break;
  case MI_CHEATS:
    if (rom_crc != KNOWN_CRC) { show_toast("Los trucos necesitan la ROM original", 2000); break; }
    menu_goto(PAGE_CHEATS); break;
  case MI_VIDEO: menu_goto(PAGE_VIDEO); break;
  case MI_OPTIONS: menu_goto(PAGE_OPTIONS); break;
  case MI_BACK: menu_goto(PAGE_MAIN); break;
  case MI_RESET: sms_reset(&S); menu_set_open(0); show_toast("Juego reiniciado", 1500); break;
  case MI_WARP:
    sms_reset(&S); warp_pending = 1; menu_set_open(0);
    show_toast("Pulsa START: la partida empezará en esa ronda", 3000);
    break;
  case MI_QUIT: running = 0; break;
  default: menu_change(1); break;
  }
}

static void menu_nav(int dy, int dx, int ok, int back) {
  int n = page_count(menu_page);
  if (dy) { menu_sel = (menu_sel + dy + n) % n; capture_jump = 0; }
  if (dx) menu_change(dx);
  if (ok) menu_activate();
  if (back) { if (menu_page != PAGE_MAIN) menu_goto(PAGE_MAIN); else menu_set_open(0); }
}

static int rewinding, fast_forward;

static void draw_ui(void) {
  ui_clear();
  if (menu_open) {
    int n = page_count(menu_page);
    ui_rect(0, 0, UI_W, UI_H, 0xA0000000);
    int bw = 560, bh = 40 + n * 26 + 34;
    int bx = (UI_W - bw) / 2, by = (UI_H - bh) / 2;
    ui_rect(bx, by, bw, bh, 0xE0140C18);
    ui_rect(bx, by, bw, 2, 0xFFB0302A);
    ui_rect(bx, by + bh - 2, bw, 2, 0xFFB0302A);
    ui_text_center(by + 10, page_titles[menu_page], 0xE8C060);
    for (int i = 0; i < n; i++) {
      char l[160]; menu_label(page_items[menu_page][i], l, sizeof l);
      int y = by + 40 + i * 26;
      if (i == menu_sel) ui_rect(bx + 10, y - 3, bw - 20, 24, 0xFF5A1A1A);
      ui_text(bx + 24, y, l, i == menu_sel ? 0xFFFFFF : 0xB8B0A8);
    }
    const char* foot = "v" APP_VERSION "  ·  Recomp por Juanmote";
    if (lang_active() && patch_crc == WAVE_PATCH_CRC) foot = "v" APP_VERSION " · Recomp por Juanmote · Traducción: Wave";
    if (menu_page == PAGE_CHEATS) foot = "Trucos oficiales del juego y selector de armas";
    ui_text_center(by + bh - 26, foot, 0x807870);
  } else if (rewinding || fast_forward) {
    const char* t = rewinding ? "◄◄" : "►►";
    ui_rect(UI_W - 64, 10, 52, 26, 0xA0000000);
    ui_text(UI_W - 56, 14, t, 0xFFFFFF);
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

static int is_face(int b) { return b >= SDL_CONTROLLER_BUTTON_A && b <= SDL_CONTROLLER_BUTTON_Y; }

static uint8_t padmap(int b) {
  if (b == cfg.jump_btn) return BTN_2;
  if (b == cfg.sub_btn) return BTN_UP | BTN_1;   // the game throws the subweapon with up + attack
  if (is_face(b)) return BTN_1;   // every other face button attacks
  switch (b) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
  case SDL_CONTROLLER_BUTTON_START: return BTN_PAUSE;
  }
  return 0;
}

static uint32_t pad_held_mask;   // SDL controller buttons held (any pad)
static uint8_t pad_buttons(void) {
  uint8_t r = 0;
  for (int b = 0; b < 32; b++) if (pad_held_mask & (1u << b)) r |= padmap(b);
  return r;
}
static int key_rewind, key_ff;
static uint8_t trig_l[8], trig_r[8];
static int any_trigger(const uint8_t* t) { for (int i = 0; i < 8; i++) if (t[i]) return 1; return 0; }
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
  case SDL_APP_DIDENTERFOREGROUND:
    if (waiting_patch) {
      waiting_patch = 0;
      uint32_t old = patch_data ? patch_crc : 0;
      if (load_patch() && patch_crc != old) set_language(1);
    }
    break;
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
    if (e->key.keysym.sym == SDLK_q) { key_rewind = 1; break; }
    if (e->key.keysym.sym == SDLK_e) { key_ff = 1; break; }
    if (e->key.keysym.sym == SDLK_F5) { do_save_state(); break; }
    if (e->key.keysym.sym == SDLK_F8) { do_load_state(); break; }
    kb_input |= keymap(e->key.keysym.sym);
    break;
  case SDL_KEYUP:
    if (e->key.keysym.sym == SDLK_q) key_rewind = 0;
    if (e->key.keysym.sym == SDLK_e) key_ff = 0;
    kb_input &= ~keymap(e->key.keysym.sym);
    break;
  case SDL_CONTROLLERDEVICEADDED: open_pad(e->cdevice.which); break;
  case SDL_CONTROLLERDEVICEREMOVED: {
    int s = pad_slot(e->cdevice.which);
    if (s >= 0) { SDL_GameControllerClose(pads[s]); pads[s] = NULL; stick_x[s] = stick_y[s] = 0; trig_l[s] = trig_r[s] = 0; }
    break;
  }
  case SDL_CONTROLLERBUTTONDOWN: {
    int b = e->cbutton.button;
    if (b == SDL_CONTROLLER_BUTTON_BACK || b == SDL_CONTROLLER_BUTTON_GUIDE || b == SDL_CONTROLLER_BUTTON_MISC1) {
      menu_set_open(!menu_open); break;
    }
    if (menu_open && capture_jump) {
      if (is_face(b)) {
        if (capture_jump == 1) {
          if (b == cfg.sub_btn) cfg.sub_btn = cfg.jump_btn;   // swap them
          cfg.jump_btn = b;
          show_toast("Botón de saltar guardado", 1500);
        } else {
          if (b == cfg.jump_btn) { show_toast("Ese botón ya es el de saltar", 1500); break; }
          cfg.sub_btn = b;
          show_toast("Botón de subarma guardado", 1500);
        }
        cfg_save(); capture_jump = 0;
      }
      else if (b == SDL_CONTROLLER_BUTTON_START) capture_jump = 0;
      break;
    }
    if (menu_open) {
      int ok_btn = SDL_CONTROLLER_BUTTON_A;
      int back_btn = SDL_CONTROLLER_BUTTON_B;
      menu_nav(b == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : b == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? 1 : 0,
               b == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? -1 : b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? 1 : 0,
               b == ok_btn || b == SDL_CONTROLLER_BUTTON_START, b == back_btn);
      break;
    }
    if (b >= 0 && b < 32) pad_held_mask |= 1u << b;
    break;
  }
  case SDL_CONTROLLERBUTTONUP:
    if (e->cbutton.button < 32) pad_held_mask &= ~(1u << e->cbutton.button);
    break;
  case SDL_CONTROLLERAXISMOTION: {
    int s = pad_slot(e->caxis.which);
    if (s < 0) break;
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) stick_x[s] = e->caxis.value;
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) stick_y[s] = e->caxis.value;
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) trig_l[s] = e->caxis.value > 12000;
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) trig_r[s] = e->caxis.value > 12000;
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
  rom_orig = malloc(rom_size);
  if (rom_orig) memcpy(rom_orig, rom, rom_size);
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

// ---------------------------------------------------------------- rewind
// A snapshot every 2 frames, the last 20 seconds. The enhancement state that decides
// spawning goes with it, so rewinding does not create or lose enemies.
#define RW_SLOTS 600
typedef struct { typeof(E.spawn) spawn; uint8_t seen_in[46]; } rw_extra_t;
static uint8_t* rw_buf;
static rw_extra_t rw_extra[RW_SLOTS];
static int rw_head, rw_count, rw_tick;
static size_t rw_size;

static void rw_push(void) {
  if (!rw_buf) { rw_size = sms_state_size(); rw_buf = malloc(rw_size * RW_SLOTS); if (!rw_buf) return; }
  rw_head = (rw_head + 1) % RW_SLOTS;
  sms_save_state(&S, rw_buf + rw_size * rw_head);
  memcpy(rw_extra[rw_head].spawn, E.spawn, sizeof E.spawn);
  memcpy(rw_extra[rw_head].seen_in, E.seen_in, sizeof E.seen_in);
  if (rw_count < RW_SLOTS) rw_count++;
}

static int rw_pop(void) {
  if (!rw_buf || rw_count == 0) return 0;
  sms_load_state(&S, rw_buf + rw_size * rw_head, rw_size);
  memcpy(E.spawn, rw_extra[rw_head].spawn, sizeof E.spawn);
  memcpy(E.seen_in, rw_extra[rw_head].seen_in, sizeof E.seen_in);
  rw_head = (rw_head - 1 + RW_SLOTS) % RW_SLOTS;
  rw_count--;
  return 1;
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

  load_patch();
  apply_language();
  apply_suit();
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
    if (getenv("MOD_MENU")) { menu_open = 1; menu_sel = atoi(getenv("MOD_MENU")); if (getenv("MOD_PAGE")) menu_page = atoi(getenv("MOD_PAGE")); }
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
    rewinding = !menu_open && cfg.triggers && (any_trigger(trig_l) || key_rewind);
    fast_forward = !menu_open && !rewinding && cfg.triggers && (any_trigger(trig_r) || key_ff);
    if (!menu_open) {
      // keep the audio queue around 3 frames by nudging the emulation speed slightly
      double queued = audio_dev ? SDL_GetQueuedAudioSize(audio_dev) / 2.0 / SMS_SAMPLES_PER_FRAME : 3.0;
      double speed = 1.0 + 0.01 * (3.0 - queued);
      if (speed < 0.98) speed = 0.98;
      if (speed > 1.02) speed = 1.02;
      if (rewinding || fast_forward) speed = 1.0;
      acc += dt * speed;
      int frames = 0;
      while (acc >= frame_time && frames < 4) {
        acc -= frame_time;
        frames++;
        S.input = kb_input | pad_buttons() | stick_dirs();
        // faster CPU only while playing: menus and story screens time themselves with CPU loops
        S.cpu_mult = (cfg.turbo && mod_is_gameplay(&S)) ? 2 : 1;
        if (rewinding) {
          // one snapshot back (2 frames) per frame shown, then one frame to get the picture
          if (rw_pop()) { S.input = 0; sms_run_frame(&S); mod_enhance_frame(&S, &E); }
          continue;
        }
        int steps = fast_forward ? 4 : 1;
        for (int k = 0; k < steps; k++) {
          cheats_frame();
          sms_run_frame(&S);
          mod_enhance_frame(&S, &E);
          if (cfg.triggers && (++rw_tick & 1) == 0) rw_push();
        }
        if (audio_dev && queued < 8 && !fast_forward) {
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
