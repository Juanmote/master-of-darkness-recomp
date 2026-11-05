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
#include "music.h"
#include "logro_icons.h"
#include "logros.h"
#include "text_es.h"
#include "../game/mod_text.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

#include "miniz.h"
#define APP_VERSION "0.15.1"
#define KNOWN_CRC 0x96FB4D4Bu
#define UI_W 640
#define UI_H 360
#define WIDE_W (SMS_W + 2 * MOD_WIDE_EXT)

// ---------------------------------------------------------------- config
// C and Z of six-button pads (Retroid Pocket Classic, Arc): SDL has no game-controller button for them,
// they arrive as joystick buttons 17 and 18 and are handled as two more face buttons with these ids
#define PAD_C 24
#define PAD_Z 25
static int is_face(int b) { return (b >= SDL_CONTROLLER_BUTTON_A && b <= SDL_CONTROLLER_BUTTON_Y) || b == PAD_C || b == PAD_Z; }

enum { SCALE_FIT, SCALE_INT, SCALE_STRETCH, SCALE_COUNT };
enum { FILTER_SHARP, FILTER_SHARP_SMOOTH, FILTER_SMOOTH, FILTER_SCANLINES, FILTER_CRT, FILTER_CRT_CURVED, FILTER_COUNT };
enum { PAL_ORIGINAL, PAL_TV, PAL_VIVID, PAL_NIGHT, PAL_SEPIA, PAL_GREEN, PAL_COUNT };

typedef struct {
  int widescreen;
  int crt;         // 4:3 mode with the 8:7 pixels of a CRT TV
  int scale;
  int filter;
  int volume;      // 0..10
  int jump_btn;    // SDL controller button that jumps (the other face buttons attack)
  int sub_btn;     // button that throws the subweapon (up + attack), -1 none
  int turbo;       // double CPU speed: removes the original slowdowns
  int lang;        // game text: 0 English (original), 1 Spanish (our translation), -1 not chosen yet: follows the menus
  int suit;        // hero colours (0 = original)
  int cheat_inv;   // official "hit check" cheat: no damage
  int cheat_lives; // lives never run out
  int triggers;    // L2 rewinds, R2 fast-forwards
  int difficulty;  // MOD_DIFF_*
  int respawn;     // MOD_RESPAWN_*
  int dbl_jump;
  int music_pack;  // 0: original music, 1-3: the user's music pack in that slot (OST 1, 2, 3)
  int touch;       // on-screen controls: 0 no, 1 yes, 2 only without a gamepad
  int touch_size;  // 0 small, 1 normal, 2 big
  int ui_lang;     // menus and launcher: 0 English, 1 Spanish (-1: not chosen yet, follows the system)
  int ammo_inf;    // cheat: the subweapon never runs out
  int lock_weapon; // cheat: pickups do not change the weapon / subweapon
  int multi_shot;  // cheat: no limit of two subweapons on screen
  int margin;      // empty border around the picture: 0 none, 1 small, 2 medium (rounded corners, notches)
  int palette;     // PAL_*: how the console's 64 colours are shown
  int vib[6];      // vibration strength per kind of moment (VIB_*): 0 off, 1 soft, 2 medium, 3 strong
  int bands;       // empty screen around the picture: 0 black, 1 "ambient" (the picture's own colours, blurred and dark)
  int stairs;      // 1: jump + up grabs a staircase in the air, jump + down drops off it
  int gg;          // "Game Gear" format: a 160x144 window that follows the hero, without the status bar (Start shows it)
} config_t;

// A new installation starts as the "remastered" version (see apply_remaster)
static config_t cfg = {1, 0, SCALE_FIT, FILTER_SCANLINES, 8, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_Y, 1, -1, 0, 0, 0, 1, MOD_DIFF_NORMAL, MOD_RESPAWN_ON, 0, 0, 2, 1, -1, 0, 0, 1, 0, PAL_ORIGINAL,
                       {1, 1, 2, 3, 1, 3}, 0, 1, 0};
enum { VIB_TOUCH, VIB_SWING, VIB_HIT, VIB_HURT, VIB_LAND, VIB_BOSS, VIB_COUNT };
static const char* vib_keys[VIB_COUNT] = {"vib_touch", "vib_swing", "vib_hit", "vib_hurt", "vib_land", "vib_boss"};
static const int vib_recommended[VIB_COUNT] = {1, 1, 2, 3, 1, 3};   // same as the initial cfg.vib
#define MUSIC_SLOTS 3
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
    else if (sscanf(line, "jump_btn=%d", &v) == 1 && is_face(v)) { cfg.jump_btn = v; jump_seen = 1; }
    else if (sscanf(line, "sub_btn=%d", &v) == 1 && (v == -1 || is_face(v))) cfg.sub_btn = v;
    else if (sscanf(line, "turbo=%d", &v) == 1) cfg.turbo = v ? 1 : 0;
    else if (sscanf(line, "lang=%d", &v) == 1) cfg.lang = v < 0 ? -1 : v ? 1 : 0;
    else if (sscanf(line, "suit=%d", &v) == 1 && v >= 0 && v < 16) cfg.suit = v;
    else if (sscanf(line, "cheat_inv=%d", &v) == 1) cfg.cheat_inv = v ? 1 : 0;
    else if (sscanf(line, "cheat_lives=%d", &v) == 1) cfg.cheat_lives = v ? 1 : 0;
    else if (sscanf(line, "triggers=%d", &v) == 1) cfg.triggers = v ? 1 : 0;
    else if (sscanf(line, "difficulty=%d", &v) == 1 && v >= 0 && v <= 2) cfg.difficulty = v;
    else if (sscanf(line, "respawn=%d", &v) == 1 && v >= 0 && v <= 2) cfg.respawn = v;
    else if (sscanf(line, "dbl_jump=%d", &v) == 1) cfg.dbl_jump = v ? 1 : 0;
    else if (sscanf(line, "music_pack=%d", &v) == 1) cfg.music_pack = v >= 0 && v <= MUSIC_SLOTS ? v : 0;
    else if (sscanf(line, "touch=%d", &v) == 1 && v >= 0 && v <= 2) cfg.touch = v;
    else if (sscanf(line, "touch_size=%d", &v) == 1 && v >= 0 && v <= 2) cfg.touch_size = v;
    else if (sscanf(line, "ui_lang=%d", &v) == 1 && v >= 0 && v <= 1) cfg.ui_lang = v;
    else if (sscanf(line, "ammo_inf=%d", &v) == 1) cfg.ammo_inf = v ? 1 : 0;
    else if (sscanf(line, "lock_weapon=%d", &v) == 1) cfg.lock_weapon = v ? 1 : 0;
    else if (sscanf(line, "multi_shot=%d", &v) == 1) cfg.multi_shot = v ? 1 : 0;
    else if (sscanf(line, "margin=%d", &v) == 1 && v >= 0 && v <= 2) cfg.margin = v;
    else if (sscanf(line, "palette=%d", &v) == 1 && v >= 0 && v < PAL_COUNT) cfg.palette = v;
    else if (sscanf(line, "bands=%d", &v) == 1) cfg.bands = v ? 1 : 0;
    else if (sscanf(line, "stairs=%d", &v) == 1) cfg.stairs = v ? 1 : 0;
    else if (sscanf(line, "gg=%d", &v) == 1) cfg.gg = v ? 1 : 0;
    else {
      for (int k = 0; k < VIB_COUNT; k++) {
        char fmt[24]; snprintf(fmt, sizeof fmt, "%s=%%d", vib_keys[k]);
        if (sscanf(line, fmt, &v) == 1 && v >= 0 && v <= 3) cfg.vib[k] = v;
      }
    }
  }
  fclose(f);
  if (cfg.gg) cfg.widescreen = cfg.crt = 0;
  if (cfg.sub_btn == cfg.jump_btn) cfg.sub_btn = cfg.jump_btn == SDL_CONTROLLER_BUTTON_Y ? SDL_CONTROLLER_BUTTON_X : SDL_CONTROLLER_BUTTON_Y;
}

static void cfg_save(void) {
  char p[1100]; cfg_path(p, sizeof p, "config.ini");
  FILE* f = fopen(p, "w");
  if (!f) return;
  fprintf(f, "widescreen=%d\ncrt=%d\nscale=%d\nfilter=%d\nvolume=%d\njump_btn=%d\nsub_btn=%d\nturbo=%d\nlang=%d\n"
             "suit=%d\ncheat_inv=%d\ncheat_lives=%d\ntriggers=%d\ndifficulty=%d\nrespawn=%d\ndbl_jump=%d\nmusic_pack=%d\ntouch=%d\ntouch_size=%d\n"
             "ui_lang=%d\nammo_inf=%d\nlock_weapon=%d\nmulti_shot=%d\nmargin=%d\npalette=%d\nbands=%d\nstairs=%d\ngg=%d\n",
          cfg.widescreen, cfg.crt, cfg.scale, cfg.filter, cfg.volume, cfg.jump_btn, cfg.sub_btn, cfg.turbo, cfg.lang,
          cfg.suit, cfg.cheat_inv, cfg.cheat_lives, cfg.triggers, cfg.difficulty, cfg.respawn, cfg.dbl_jump, cfg.music_pack, cfg.touch, cfg.touch_size,
          cfg.ui_lang, cfg.ammo_inf, cfg.lock_weapon, cfg.multi_shot, cfg.margin, cfg.palette, cfg.bands, cfg.stairs, cfg.gg);
  for (int k = 0; k < VIB_COUNT; k++) fprintf(f, "%s=%d\n", vib_keys[k], cfg.vib[k]);
  fclose(f);
}

// ---------------------------------------------------------------- interface language
// English by default, Spanish when the system is in Spanish; the player can change it in the menu
#define ES (cfg.ui_lang == 1)
#define T(en, sp) (ES ? (sp) : (en))

static void detect_ui_lang(void) {
  if (cfg.ui_lang >= 0) return;
  cfg.ui_lang = 0;
  const char* force = getenv("MOD_LANG");
  if (force) { cfg.ui_lang = force[0] == 'e' && force[1] == 's'; return; }
  SDL_Locale* l = SDL_GetPreferredLocales();
  if (l) {
    if (l[0].language && l[0].language[0] == 'e' && l[0].language[1] == 's') cfg.ui_lang = 1;
    SDL_free(l);
  }
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
static int tex_game_w, tex_game_h, sharp_k;
static int game_h = SMS_H;   // lines of the picture sent to the screen (144 in the Game Gear format)
static SDL_Texture* tex_hud;  // Game Gear format: the status bar, shown over the picture during the pause
static SDL_AudioDeviceID audio_dev;
static SDL_GameController* pads[8];

static sms_t S;
static mod_enhance_t E;
static uint8_t* rom;
static size_t rom_size;
static uint32_t rom_crc;
static uint32_t* pix;
static uint8_t* rom_orig;   // the user's ROM as loaded (the translation is applied on a copy)

// Spanish text: our own translation (text_es.h), applied on a copy of the original ROM. Records:
// offset (3 bytes), length (2 bytes), data. The extra letters (codes 0xB0..) are drawn by mod_text.
static void apply_language(void) {
  if (!rom || !rom_orig) return;
  memcpy(rom, rom_orig, rom_size);
  if (cfg.lang != 1 || rom_crc != KNOWN_CRC) return;
  for (size_t i = 0; i + 5 <= sizeof text_es_data; ) {
    size_t off = ((size_t)text_es_data[i] << 16) | (text_es_data[i + 1] << 8) | text_es_data[i + 2];
    size_t len = (text_es_data[i + 3] << 8) | text_es_data[i + 4];
    i += 5;
    if (off + len <= rom_size && i + len <= sizeof text_es_data) memcpy(rom + off, text_es_data + i, len);
    i += len;
  }
}

static int lang_active(void) { return cfg.lang == 1 && rom_crc == KNOWN_CRC; }

// ---------------------------------------------------------------- music pack
static int music_current = -1;   // last track the game asked for
// Title, intro, story and continue screens (known by their music): there START starts or
// continues the game like button 1 did on the Master System, instead of pausing.
static int on_title_screens(void) {
  return music_current < 0 || music_current == 0x83 || music_current == 0x87 || music_current == 0x8D ||
         music_current == 0x82;
}
static int waiting_music;

// Music packs: up to three slots (OST 1-3), saved as musica.zip, musica2.zip and musica3.zip.
// Only the selected one is open; changing the selection opens the other file, nothing is copied again.
static const char* music_pack_file(int slot) { return slot == 2 ? "musica2.zip" : slot == 3 ? "musica3.zip" : "musica.zip"; }

static void music_pack_path(int slot, char* out, size_t n) {
#ifdef __ANDROID__
  snprintf(out, n, "%s/%s", SDL_AndroidGetInternalStoragePath(), music_pack_file(slot));
#else
  cfg_path(out, n, music_pack_file(slot));
#endif
}

static bool music_enabled_has(int id) { return cfg.music_pack > 0 && music_has(id); }

static void load_music_pack(void) {
  if (cfg.music_pack <= 0) { music_unload(); return; }
  char p[1100]; music_pack_path(cfg.music_pack, p, sizeof p);
  if (!music_load_pack(p)) music_unload();
}

static void request_music_file(int slot) {
#ifdef __ANDROID__
  waiting_music = slot;
  SDL_AndroidSendMessage(0x8002, slot);
#else
  (void)slot;
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

// ---------------------------------------------------------------- colour palettes
// The Master System has 64 colours (2 bits per component). A palette only decides which RGB
// colour each of them becomes: the pictures themselves are untouched.
static uint32_t palette_lut[64];

static int clamp255(double v) { return v < 0 ? 0 : v > 255 ? 255 : (int)(v + 0.5); }

static void apply_palette(void) {
  if (cfg.palette == PAL_ORIGINAL) { sms_set_palette(NULL); return; }
  for (int c = 0; c < 64; c++) {
    int ci[3] = {c & 3, (c >> 2) & 3, (c >> 4) & 3};
    double rgb[3];
    for (int k = 0; k < 3; k++) rgb[k] = ci[k] * 85.0;
    double y = 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2];
    double o[3];
    switch (cfg.palette) {
    case PAL_TV: {   // composite TV: softer contrast, slightly warm, lifted blacks, a little less saturated
      static const double lv[4] = {16, 98, 178, 240};
      for (int k = 0; k < 3; k++) o[k] = lv[ci[k]];
      double yy = 0.299 * o[0] + 0.587 * o[1] + 0.114 * o[2];
      for (int k = 0; k < 3; k++) o[k] = yy + (o[k] - yy) * 0.82;
      o[0] += 8; o[2] -= 6;
      break;
    }
    case PAL_VIVID: {   // richer colours and deeper shadows
      static const double lv[4] = {0, 72, 168, 255};
      for (int k = 0; k < 3; k++) o[k] = lv[ci[k]];
      double yy = 0.299 * o[0] + 0.587 * o[1] + 0.114 * o[2];
      for (int k = 0; k < 3; k++) o[k] = yy + (o[k] - yy) * 1.28;
      break;
    }
    case PAL_NIGHT: {   // moonlight: cold shadows, slightly muted, warm highlights stay
      static const double lv[4] = {6, 80, 160, 236};
      for (int k = 0; k < 3; k++) o[k] = lv[ci[k]];
      double yy = 0.299 * o[0] + 0.587 * o[1] + 0.114 * o[2];
      double t = 1.0 - yy / 255.0;   // how dark: darker colours turn bluer
      for (int k = 0; k < 3; k++) o[k] = yy + (o[k] - yy) * 0.9;
      o[0] -= 22 * t; o[1] -= 6 * t; o[2] += 26 * t;
      break;
    }
    case PAL_SEPIA: {   // old photograph
      double v = y / 255.0;
      o[0] = 255 * (0.10 + 0.95 * v); o[1] = 255 * (0.06 + 0.80 * v); o[2] = 255 * (0.03 + 0.58 * v);
      break;
    }
    default: {   // green monochrome screen, four shades like an old portable
      static const uint32_t g4[4] = {0x0F380F, 0x306230, 0x8BAC0F, 0x9BBC0F};
      int sh = y < 40 ? 0 : y < 110 ? 1 : y < 185 ? 2 : 3;
      o[0] = (g4[sh] >> 16) & 255; o[1] = (g4[sh] >> 8) & 255; o[2] = g4[sh] & 255;
      break;
    }
    }
    palette_lut[c] = 0xFF000000u | ((uint32_t)clamp255(o[0]) << 16) | ((uint32_t)clamp255(o[1]) << 8) | (uint32_t)clamp255(o[2]);
  }
  sms_set_palette(palette_lut);
}

// ---------------------------------------------------------------- hero colours
// light and dark shade of the suit (sprite palette entries 10 and 11), SMS colours %00BBGGRR
static const struct { const char *en, *es; uint8_t light, dark; } suits[] = {
  {"Original", "Original", 0xFF, 0xFF}, {"Red", "Rojo", 0x17, 0x02}, {"Green", "Verde", 0x1D, 0x08},
  {"Purple", "Morado", 0x37, 0x21}, {"Brown", "Marrón", 0x1B, 0x06}, {"Black", "Negro", 0x2A, 0x15},
  {"White", "Blanco", 0x3F, 0x2A}, {"Gold", "Dorado", 0x0F, 0x0A},
  {"Chameleon", "Camaleón", 0, 0},   // changes colour by itself while playing
};
#define SUIT_COUNT (int)(sizeof suits / sizeof suits[0])
#define SUIT_CHAMELEON (SUIT_COUNT - 1)

// chameleon: walks around the colour wheel (SMS colours have 2 bits per component)
static const uint8_t chameleon_light[12] = {0x17, 0x1B, 0x0F, 0x1E, 0x1D, 0x2D, 0x3C, 0x39, 0x35, 0x37, 0x27, 0x2B};
static const uint8_t chameleon_dark[12] = {0x02, 0x06, 0x0A, 0x09, 0x08, 0x18, 0x24, 0x20, 0x10, 0x21, 0x11, 0x12};
static int chameleon_step;
static Uint32 chameleon_next;

static void apply_suit(void) {
  if (cfg.suit <= 0 || cfg.suit >= SUIT_COUNT) { mod_set_hero_colors(NULL); return; }
  uint8_t m[16];
  memset(m, 0xFF, sizeof m);
  if (cfg.suit == SUIT_CHAMELEON) {
    m[10] = chameleon_light[chameleon_step % 12];
    m[11] = chameleon_dark[chameleon_step % 12];
  } else {
    m[10] = suits[cfg.suit].light;
    m[11] = suits[cfg.suit].dark;
  }
  mod_set_hero_colors(m);
}

// called every game frame: the chameleon suit changes colour every half second
static void suit_frame(void) {
  if (cfg.suit != SUIT_CHAMELEON) return;
  Uint32 now = SDL_GetTicks();
  if ((Sint32)(now - chameleon_next) < 0) return;
  chameleon_next = now + 500;
  chameleon_step = (chameleon_step + 1) % 12;
  apply_suit();
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

static const char* weapon_names[2][4] = {{"Knife", "Rapier", "Sword", "Axe"}, {"Cuchillo", "Estoque", "Espada", "Hacha"}};
static const char* sub_names[2][5] = {{"None", "Pistol", "Boomerang", "Stakes", "Bombs"},
                                      {"Ninguna", "Pistola", "Bumerán", "Estacas", "Bombas"}};
static const uint8_t start_points[][2] = {{0,0},{0,1},{0,2},{1,0},{1,1},{1,2},{2,0},{2,1},{2,2},{3,0},{3,1},{3,2},{4,0}};
#define START_COUNT (int)(sizeof start_points / sizeof start_points[0])
static int start_sel;
static int warp_pending;   // keep the chosen round in RAM until the level is running
static int locked_weapon = -1, locked_sub = -1;   // weapon lock cheat (-1: take the current ones)

// Game modes. Every mode but the normal one blocks the advantages: rewind, fast forward, cheats, double jump,
// difficulty and "no respawn". Saved states, picture, music and controls stay free.
// Survivor: the achievements mode with a single life; its difficulty only says when energy comes back
// (easy: as in the game, normal: only after each boss, hard: never). Achievements count in both.
enum { GM_NORMAL, GM_LOGROS, GM_SURVIVOR, GM_BOSSRUSH };
static int game_mode = GM_NORMAL;
static int sv_diff = 1;    // difficulty of the Survivor game
// Boss Rush: the five bosses in a row against the clock, with a single life. Energy after each boss (life) and
// weapons (arms) have their own level, 0 easy .. 2 hard; the run's difficulty comes from both.
static int br_life = 1, br_arms = 1, br_wsel, br_ssel;
static const int br_weapons[3][2] = {{1, 3}, {2, 1}, {0, 0}};   // RAM_WEAPON: rapier or axe; sword or rapier; knife
static const int br_subs[3][2] = {{3, 2}, {0, 1}, {0, 1}};      // RAM_SUBWEAPON: bombs or stakes; pistol or boomerang
static const int br_choices[3] = {2, 2, 1};
static int br_diff(void) { int t = br_life + br_arms; return t == 0 ? 0 : t <= 2 ? 1 : 2; }
// TEMPORARY, for testing the achievements (remove before the final version): invincible in the modes with
// achievements, R2 fast forwards, and everything goes to a separate profile (logros_pruebas.txt)
static int test_inv;
#define ADVANTAGES_BLOCKED (game_mode != GM_NORMAL)
#define LOGROS_ON (game_mode != GM_NORMAL)

static int cheats_ok(void) { return rom_crc == KNOWN_CRC && mod_is_gameplay(&S); }

static void cheats_frame(void) {
  if (rom_crc != KNOWN_CRC) return;
  if (ADVANTAGES_BLOCKED) { gram_w(RAM_INVINCIBLE, test_inv ? 0xFF : 0); return; }   // also the game's hidden debug menu
  if (warp_pending) {
    gram_w(RAM_ROUND, start_points[start_sel][0]);
    gram_w(RAM_STAGE, start_points[start_sel][1]);
    if (mod_is_gameplay(&S)) warp_pending = 0;
  }
  if (!mod_is_gameplay(&S)) return;
  if (cfg.cheat_inv) gram_w(RAM_INVINCIBLE, 0xFF);
  if (cfg.cheat_lives && gram(RAM_LIVES) < 3) { gram_w(RAM_LIVES, 3); mod_request_hud(&E, MOD_HUD_LIVES); }
  // weapon lock: whatever is picked up, the weapons chosen stay (changing them in the menu moves the lock)
  if (cfg.lock_weapon) {
    if (locked_weapon < 0) { locked_weapon = gram(RAM_WEAPON) & 3; locked_sub = gram(RAM_SUBWEAPON) & 3; }
    if ((gram(RAM_WEAPON) & 3) != locked_weapon) { gram_w(RAM_WEAPON, (uint8_t)locked_weapon); mod_request_hud(&E, MOD_HUD_WEAPON); }
    if ((gram(RAM_SUBWEAPON) & 3) != locked_sub) { gram_w(RAM_SUBWEAPON, (uint8_t)locked_sub); mod_request_hud(&E, MOD_HUD_SUBWEAPON); }
  }
  // infinite ammo: the counter is kept full (only when the hero carries a subweapon)
  if (cfg.ammo_inf && gram(RAM_AMMO) && gram(RAM_AMMO) < 99) { gram_w(RAM_AMMO, 99); mod_request_hud(&E, MOD_HUD_SUBWEAPON); }
}

// ---------------------------------------------------------------- vibration
// Touch controls always vibrate the phone. Game moments vibrate the gamepad when one with rumble is
// connected, otherwise the device itself (consoles like the Odin vibrate their own motor).
// Each kind of moment has its own strength (cfg.vib): off, soft, medium or strong.
static const char* vib_level_names[2][4] = {{"Off", "Soft", "Medium", "Strong"}, {"No", "Suave", "Media", "Fuerte"}};
static const char* vib_names[2][VIB_COUNT] = {
  {"Touch controls:", "Attacking:", "Hitting enemies:", "Taking hits:", "Landing:", "Bosses:"},
  {"Controles táctiles:", "Al atacar:", "Al acertar:", "Al recibir golpes:", "Al caer al suelo:", "Jefes:"}};
static Uint32 vib_until;   // end of the vibration in progress
static int vib_amp;        // and its strength (a weaker one does not cut it short)

static SDL_GameController* rumble_pad(void) {
  for (int i = 0; i < 8; i++) if (pads[i] && SDL_GameControllerHasRumble(pads[i])) return pads[i];
  return NULL;
}

// kind: VIB_*; ms and power (0..1) describe the moment at full strength
static void vibrate(int kind, int ms, float power, int on_pad) {
  int lv = cfg.vib[kind];
  if (lv <= 0) return;
  static const float gain[4] = {0, 0.40f, 0.70f, 1.0f};
  int amp = (int)(255 * power * gain[lv] + 0.5f);
  if (amp < 1) amp = 1;
  if (amp > 255) amp = 255;
  ms = ms * (2 + lv) / 5;   // softer is also shorter (phones without strength control feel the length)
  if (ms < 8) ms = 8;
  Uint32 now = SDL_GetTicks();
  if ((Sint32)(vib_until - now) > 0 && amp < vib_amp) return;
  vib_until = now + (Uint32)ms; vib_amp = amp;
  SDL_GameController* pad = on_pad ? rumble_pad() : NULL;
  if (pad) { SDL_GameControllerRumble(pad, (Uint16)(amp * 257), (Uint16)(amp * 200), (Uint32)ms); return; }
#ifdef __ANDROID__
  SDL_AndroidSendMessage(0x8004, (amp << 16) | (ms & 0xFFFF));
#else
  if (getenv("MOD_VIBLOG")) printf("VIB kind=%d amp=%d ms=%d\n", kind, amp, ms);
#endif
}

static void vibrate_touch(void) { vibrate(VIB_TOUCH, 18, 0.65f, 0); }

// the moments the game reported this frame (MOD_EV_*): only the strongest one is felt
static void vibrate_game(uint32_t ev) {
  static const struct { uint32_t bit; int kind, ms; float power; } m[] = {
    {MOD_EV_BOSS_DOWN, VIB_BOSS, 900, 1.0f}, {MOD_EV_DEATH, VIB_HURT, 380, 1.0f}, {MOD_EV_BOSS_START, VIB_BOSS, 450, 0.6f},
    {MOD_EV_HURT, VIB_HURT, 110, 1.0f}, {MOD_EV_BOSS_HIT, VIB_BOSS, 80, 0.9f}, {MOD_EV_KILL, VIB_HIT, 60, 0.9f}, {MOD_EV_PICKUP, VIB_HIT, 60, 0.9f},
    {MOD_EV_HIT, VIB_HIT, 40, 0.7f}, {MOD_EV_LAND, VIB_LAND, 35, 0.55f}, {MOD_EV_SWING, VIB_SWING, 25, 0.45f}};
  for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++)
    if ((ev & m[i].bit) && cfg.vib[m[i].kind]) { vibrate(m[i].kind, m[i].ms, m[i].power, 1); return; }
}

// a sample of each kind, felt when its strength is changed in the menu
static void vibrate_sample(int kind) {
  static const int ms[VIB_COUNT] = {18, 25, 60, 110, 35, 450};
  static const float pw[VIB_COUNT] = {0.65f, 0.45f, 0.9f, 1.0f, 0.55f, 0.8f};
  vib_until = 0;
  vibrate(kind, ms[kind], pw[kind], kind != VIB_TOUCH);
}

// ---------------------------------------------------------------- menu
enum {
  MI_RESUME, MI_SAVE, MI_LOAD, MI_GAME, MI_CHEATS, MI_VIDEO, MI_OPTIONS, MI_RESET, MI_QUIT,
  MI_CONTROLS, MI_HOME, MI_TOUCH, MI_TOUCHSIZE,
  MI_DIFF, MI_RESPAWN, MI_DJUMP, MI_STAIRS,
  MI_FORMAT, MI_SCALE, MI_FILTER, MI_PALETTE, MI_SUIT, MI_MARGIN, MI_BANDS,
  MI_VOLUME, MI_BUTTONS, MI_SUBBTN, MI_TURBO, MI_TRIGGERS, MI_LANG, MI_MUSIC, MI_MUSPACK, MI_MUSTEST,
  MI_INV, MI_LIVES, MI_WEAPON, MI_SUB, MI_AMMO, MI_MULTI, MI_LOCK, MI_START, MI_WARP, MI_UILANG, MI_ORIGINAL,
  MI_REMASTER, MI_VIBRA, MI_VIB0, MI_VIB_LAST = MI_VIB0 + VIB_COUNT - 1,
  MI_MODE_NORMAL, MI_MODE_LOGROS, MI_LOGROS_LIST, MI_PROFILE_EXPORT, MI_PROFILE_IMPORT, MI_STATE_EXPORT,
  MI_MODE_SURVIVOR, MI_SV_EASY, MI_SV_NORMAL, MI_SV_HARD, MI_TESTINV,
  MI_MODE_BOSSRUSH, MI_BR_LIFE, MI_BR_ARMS, MI_BR_WEAPON, MI_BR_SUB, MI_BR_START,
  MI_BACK
};
enum { PAGE_MAIN, PAGE_VIDEO, PAGE_OPTIONS, PAGE_CHEATS, PAGE_GAME, PAGE_CONTROLS, PAGE_HOME, PAGE_VIBRA, PAGE_MAIN_LOCKED, PAGE_MODES, PAGE_SURVIVOR, PAGE_MAIN_SURVIVOR, PAGE_BOSSRUSH, PAGE_MAIN_BR, PAGE_COUNT };
#define PAGE_MAX 12
static const int page_items[PAGE_COUNT][PAGE_MAX] = {
  {MI_RESUME, MI_SAVE, MI_LOAD, MI_GAME, MI_CHEATS, MI_VIDEO, MI_OPTIONS, MI_CONTROLS, MI_RESET, MI_HOME, MI_QUIT, -1},
  {MI_FORMAT, MI_SCALE, MI_BANDS, MI_FILTER, MI_PALETTE, MI_MARGIN, MI_SUIT, MI_BACK, -1},
  {MI_UILANG, MI_VOLUME, MI_LANG, MI_MUSIC, MI_MUSPACK, MI_MUSTEST, MI_BACK, -1},
  {MI_INV, MI_LIVES, MI_WEAPON, MI_SUB, MI_AMMO, MI_MULTI, MI_LOCK, MI_START, MI_WARP, MI_BACK, -1},
  {MI_DIFF, MI_RESPAWN, MI_DJUMP, MI_STAIRS, MI_TURBO, MI_REMASTER, MI_ORIGINAL, MI_BACK, -1},
  {MI_BUTTONS, MI_SUBBTN, MI_TRIGGERS, MI_TOUCH, MI_TOUCHSIZE, MI_VIBRA, MI_BACK, -1},
  {MI_GAME, MI_CHEATS, MI_VIDEO, MI_OPTIONS, MI_BACK, -1},
  {MI_VIB0, MI_VIB0 + 1, MI_VIB0 + 2, MI_VIB0 + 3, MI_VIB0 + 4, MI_VIB0 + 5, MI_BACK, -1},
  {MI_RESUME, MI_SAVE, MI_LOAD, MI_LOGROS_LIST, MI_VIDEO, MI_OPTIONS, MI_CONTROLS, MI_TESTINV, MI_RESET, MI_HOME, MI_QUIT, -1},
  {MI_MODE_NORMAL, MI_MODE_LOGROS, MI_MODE_SURVIVOR, MI_MODE_BOSSRUSH, MI_LOGROS_LIST, MI_PROFILE_EXPORT, MI_PROFILE_IMPORT, MI_STATE_EXPORT, MI_BACK, -1},
  {MI_SV_EASY, MI_SV_NORMAL, MI_SV_HARD, MI_BACK, -1},
  {MI_RESUME, MI_LOGROS_LIST, MI_VIDEO, MI_OPTIONS, MI_CONTROLS, MI_TESTINV, MI_RESET, MI_HOME, MI_QUIT, -1},
  {MI_BR_LIFE, MI_BR_ARMS, MI_BR_WEAPON, MI_BR_SUB, MI_BR_START, MI_BACK, -1},
  {MI_RESUME, MI_LOGROS_LIST, MI_VIDEO, MI_OPTIONS, MI_CONTROLS, MI_TESTINV, MI_RESET, MI_HOME, MI_QUIT, -1},
};
static const char* page_titles[2][PAGE_COUNT] = {
  {"MASTER OF DARKNESS", "VIDEO", "SOUND & LANGUAGE", "CHEATS", "GAME", "CONTROLS", "OPTIONS", "VIBRATION", "ACHIEVEMENTS MODE", "PLAY", "SURVIVOR MODE", "SURVIVOR", "BOSS RUSH", "BOSS RUSH"},
  {"MASTER OF DARKNESS", "IMAGEN", "SONIDO E IDIOMA", "TRUCOS", "JUEGO", "CONTROLES", "OPCIONES", "VIBRACIÓN", "MODO LOGROS", "JUGAR", "MODO SURVIVOR", "SURVIVOR", "BOSS RUSH", "BOSS RUSH"}};
static const char* touch_names[2][3] = {{"No", "Yes", "Only without a gamepad"}, {"No", "Sí", "Solo sin mando"}};
static const char* touch_size_names[2][3] = {{"Small", "Normal", "Big"}, {"Pequeños", "Normales", "Grandes"}};
static int in_launcher = 1;   // the start screen is shown (the game waits)
static int menu_root;         // page the menu was opened on (Volver goes back to it)
static const char* diff_names[2][3] = {{"Easy", "Normal", "Hard"}, {"Fácil", "Normal", "Difícil"}};
static const char* respawn_names[2][3] = {{"Yes (original)", "No", "When coming back"}, {"Sí (original)", "No", "Al volver a la zona"}};

static void apply_game_options(void) {
  E.difficulty = ADVANTAGES_BLOCKED ? MOD_DIFF_NORMAL : cfg.difficulty;
  E.respawn = ADVANTAGES_BLOCKED ? MOD_RESPAWN_ON : cfg.respawn;
  E.double_jump = !ADVANTAGES_BLOCKED && cfg.dbl_jump != 0;
  E.multi_shot = !ADVANTAGES_BLOCKED && cfg.multi_shot != 0;
  E.stairs_jump = cfg.stairs != 0;   // a way of moving, not an advantage: also in the achievement modes
}
static int menu_page;
static int confirm_original;   // "back to the original game" (1) or "remastered" (2) was chosen once: the next press confirms

static int page_count(int p) { int n = 0; while (n < PAGE_MAX && page_items[p][n] >= 0) n++; return n; }
static int cur_item(void) { return page_items[menu_page][menu_sel]; }

static void menu_set_open(int open);
static int capture_jump;   // waiting for the player to press a button: 1 jump, 2 subweapon

static const char* button_name(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_A: return "A";
  case SDL_CONTROLLER_BUTTON_B: return "B";
  case SDL_CONTROLLER_BUTTON_X: return "X";
  case SDL_CONTROLLER_BUTTON_Y: return "Y";
  case PAD_C: return "C";
  case PAD_Z: return "Z";
  }
  return "?";
}

static const char* scale_names[2][SCALE_COUNT] = {{"Fit to screen", "Integer scale", "Stretch"},
                                                  {"Ajustar a pantalla", "Escala entera", "Estirar"}};
static const char* filter_names[2][FILTER_COUNT] = {{"Sharp", "Sharp smoothed", "Smooth", "Scanlines", "CRT", "Curved CRT"},
                                                   {"Nítido", "Nítido suavizado", "Suave", "Scanlines", "CRT", "CRT curvo"}};
static const char* palette_names[2][PAL_COUNT] = {
  {"Original", "Old TV", "Vivid", "Night", "Sepia", "Green screen"},
  {"Original", "Tele antigua", "Vivos", "Nocturna", "Sepia", "Pantalla verde"}};
static const char* yes_no(int v) { return v ? T("Yes", "Sí") : "No"; }

static int music_test_sel;

static void menu_label(int i, char* out, size_t n) {
  const int L = ES;
  switch (i) {
  case MI_RESUME: snprintf(out, n, "%s", T("Continue", "Continuar")); break;
  case MI_SAVE: snprintf(out, n, "%s", T("Save state", "Guardar estado")); break;
  case MI_LOAD: snprintf(out, n, "%s", T("Load state", "Cargar estado")); break;
  case MI_CHEATS: snprintf(out, n, "%s  ›", T("Cheats", "Trucos")); break;
  case MI_GAME: snprintf(out, n, "%s  ›", T("Game", "Juego")); break;
  case MI_DIFF: snprintf(out, n, "%s  ← %s →", T("Difficulty:", "Dificultad:"), diff_names[L][cfg.difficulty]); break;
  case MI_RESPAWN: snprintf(out, n, "%s ← %s →", T("Enemies respawn:", "Reaparición de enemigos:"), respawn_names[L][cfg.respawn]); break;
  case MI_DJUMP: snprintf(out, n, "%s ← %s →", T("Double jump:", "Doble salto:"), yes_no(cfg.dbl_jump)); break;
  case MI_STAIRS: snprintf(out, n, "%s ← %s →", T("Stairs:", "Escaleras:"), cfg.stairs ? T("With jump", "Con salto") : T("Classic", "Clásicas")); break;
  case MI_VIDEO: snprintf(out, n, "%s  ›", T("Video", "Imagen")); break;
  case MI_OPTIONS: snprintf(out, n, "%s  ›", T("Sound & language", "Sonido e idioma")); break;
  case MI_CONTROLS: snprintf(out, n, "%s  ›", T("Controls", "Controles")); break;
  case MI_HOME: snprintf(out, n, "%s", T("Start screen", "Pantalla de inicio")); break;
  case MI_MODE_NORMAL: snprintf(out, n, "%s", T("Normal game", "Partida normal")); break;
  case MI_MODE_LOGROS: snprintf(out, n, "%s", T("Achievements mode", "Modo logros")); break;
  case MI_MODE_SURVIVOR: snprintf(out, n, "%s  ›", T("Survivor mode", "Modo Survivor")); break;
  case MI_SV_EASY: case MI_SV_NORMAL: case MI_SV_HARD: snprintf(out, n, "%s", diff_names[L][i - MI_SV_EASY]); break;
  case MI_MODE_BOSSRUSH: snprintf(out, n, "%s  ›", "Boss Rush"); break;
  case MI_BR_LIFE: snprintf(out, n, "%s ← %s →", T("Energy:", "Vida:"), diff_names[L][br_life]); break;
  case MI_BR_ARMS: snprintf(out, n, "%s ← %s →", T("Weapons:", "Armas:"), diff_names[L][br_arms]); break;
  case MI_BR_WEAPON:
    if (br_choices[br_arms] > 1) snprintf(out, n, "%s ← %s →", T("Weapon:", "Arma:"), weapon_names[L][br_weapons[br_arms][br_wsel]]);
    else snprintf(out, n, "%s   %s", T("Weapon:", "Arma:"), weapon_names[L][br_weapons[br_arms][0]]);
    break;
  case MI_BR_SUB: snprintf(out, n, "%s ← %s →", T("Subweapon:", "Subarma:"), sub_names[L][br_subs[br_arms][br_ssel] + 1]); break;
  case MI_BR_START: snprintf(out, n, "%s  ·  %s", T("Start", "Empezar"), diff_names[L][br_diff()]); break;
  case MI_TESTINV: snprintf(out, n, "%s ← %s →", T("Invincible (testing):", "Invencible (pruebas):"), test_inv ? T("Yes", "Sí") : "No"); break;
  case MI_LOGROS_LIST: snprintf(out, n, "%s (%d/%d)  ›", T("Achievements", "Logros"), logros_unlocked_count(), logros_count()); break;
  case MI_PROFILE_EXPORT: snprintf(out, n, "%s", T("Export profile", "Exportar perfil")); break;
  case MI_PROFILE_IMPORT: snprintf(out, n, "%s", T("Import profile", "Importar perfil")); break;
  case MI_STATE_EXPORT: snprintf(out, n, "%s", T("Export state", "Exportar estado")); break;
  case MI_TOUCH: snprintf(out, n, "%s ← %s →", T("Touch controls:", "Controles táctiles:"), touch_names[L][cfg.touch]); break;
  case MI_TOUCHSIZE: snprintf(out, n, "%s ← %s →", T("Touch size:", "Tamaño táctil:"), touch_size_names[L][cfg.touch_size]); break;
  case MI_RESET: snprintf(out, n, "%s", T("Reset game", "Reiniciar juego")); break;
  case MI_QUIT: snprintf(out, n, "%s", T("Exit", "Salir")); break;
  case MI_BACK: snprintf(out, n, "‹  %s", T("Back", "Volver")); break;
  case MI_ORIGINAL:
    snprintf(out, n, "%s", confirm_original == 1 ? T("Sure? Press again to confirm", "¿Seguro? Pulsa otra vez para confirmar")
                                                 : T("Back to the original game", "Volver al juego original"));
    break;
  case MI_UILANG: snprintf(out, n, "%s ← %s →", T("Menu language:", "Idioma de los menús:"), T("English", "Español")); break;
  case MI_REMASTER:
    snprintf(out, n, "%s", confirm_original == 2 ? T("Sure? Press again to confirm", "¿Seguro? Pulsa otra vez para confirmar")
                                                 : T("Remastered version (recommended)", "Versión remasterizada (recomendada)"));
    break;
  case MI_VIBRA: snprintf(out, n, "%s  ›", T("Vibration", "Vibración")); break;
  case MI_FORMAT:
    snprintf(out, n, "%s  ← %s →", T("Format:", "Formato:"),
             cfg.widescreen ? T("16:9 widescreen", "16:9 panorámico") : cfg.crt ? T("CRT TV (8:7)", "Tele de tubo (8:7)") :
             cfg.gg ? "Game Gear (8:7)" : T("4:3 square pixels", "4:3 píxel cuadrado"));
    break;
  case MI_SCALE: snprintf(out, n, "%s ← %s →", T("Scaling:", "Escalado:"), scale_names[L][cfg.scale]); break;
  case MI_FILTER: snprintf(out, n, "%s   ← %s →", T("Filter:", "Filtro:"), filter_names[L][cfg.filter]); break;
  case MI_PALETTE: snprintf(out, n, "%s ← %s →", T("Colours:", "Colores:"), palette_names[L][cfg.palette]); break;
  case MI_BANDS: snprintf(out, n, "%s ← %s →", T("Empty bands:", "Bandas vacías:"), cfg.bands ? T("Ambient", "Ambiente") : T("Black", "Negras")); break;
  case MI_MARGIN: {
    static const char* mn[2][3] = {{"None", "Small", "Medium"}, {"No", "Pequeño", "Mediano"}};
    snprintf(out, n, "%s ← %s →", T("Screen margin:", "Margen de pantalla:"), mn[L][cfg.margin]);
    break;
  }
  case MI_SUIT: snprintf(out, n, "%s ← %s →", T("Dr. Social's suit:", "Traje del Dr. Social:"), L ? suits[cfg.suit].es : suits[cfg.suit].en); break;
  case MI_VOLUME: snprintf(out, n, "%s  ← %d%% →", T("Volume:", "Volumen:"), cfg.volume * 10); break;
  case MI_BUTTONS:
    if (capture_jump == 1) snprintf(out, n, "%s", T("Jump:     press the button you want...", "Saltar:   pulsa el botón que quieras..."));
    else snprintf(out, n, "%s %s", T("Jump:     button", "Saltar:   botón"), button_name(cfg.jump_btn));
    break;
  case MI_SUBBTN:
    if (capture_jump == 2) snprintf(out, n, "%s", T("Subweapon: press the button you want...", "Subarma:  pulsa el botón que quieras..."));
    else if (cfg.sub_btn < 0) snprintf(out, n, "%s", T("Subweapon: no button (up + attack)", "Subarma:  sin botón (arriba + ataque)"));
    else snprintf(out, n, T("Subweapon: button %s (or up + attack)", "Subarma:  botón %s (o arriba + ataque)"), button_name(cfg.sub_btn));
    break;
  case MI_TURBO: snprintf(out, n, "%s ← %s →", T("Slowdowns:", "Ralentizaciones:"), cfg.turbo ? T("Removed", "Eliminadas") : T("Original", "Originales")); break;
  case MI_TRIGGERS: snprintf(out, n, "%s ← %s →", T("Triggers (L2 rewind, R2 fast forward):", "Gatillos (L2 rebobinar, R2 acelerar):"), yes_no(cfg.triggers)); break;
  case MI_LANG: snprintf(out, n, "%s ← %s →", T("Game text:", "Texto del juego:"), lang_active() ? T("Spanish", "Español") : T("English (original)", "Inglés (original)")); break;
  case MI_MUSIC:
    if (!cfg.music_pack) snprintf(out, n, "%s ← Original →", T("Music:", "Música:"));
    else snprintf(out, n, "%s ← OST %d%s →", T("Music:", "Música:"), cfg.music_pack,
                  music_pack_loaded() ? "" : T(" (empty)", " (vacía)"));
    break;
  case MI_MUSPACK:
    if (!cfg.music_pack) snprintf(out, n, "%s", T("Music pack: pick OST 1, 2 or 3 above", "Pack de música: elige OST 1, 2 o 3 arriba"));
    else if (music_pack_loaded()) snprintf(out, n, T("OST %d pack: %d tracks (change)", "Pack OST %d: %d pistas (cambiar)"), cfg.music_pack, music_pack_tracks());
    else snprintf(out, n, T("OST %d pack: none (choose a .zip)", "Pack OST %d: ninguno (elegir .zip)"), cfg.music_pack);
    break;
  case MI_MUSTEST: {
    const music_track_t* t = &music_tracks[music_test_sel];
    snprintf(out, n, "%s ← %s %s%s →", T("Music test:", "Probar música:"), t->file, L ? t->name : t->name_en, music_has(t->id) && cfg.music_pack ? " (pack)" : "");
    break;
  }
  case MI_INV: snprintf(out, n, "%s ← %s →", T("Invincible:      ", "Invencible:     "), yes_no(cfg.cheat_inv)); break;
  case MI_LIVES: snprintf(out, n, "%s ← %s →", T("Infinite lives:  ", "Vidas infinitas:"), yes_no(cfg.cheat_lives)); break;
  case MI_AMMO: snprintf(out, n, "%s ← %s →", T("Infinite ammo:   ", "Munición infinita:"), yes_no(cfg.ammo_inf)); break;
  case MI_MULTI: snprintf(out, n, "%s ← %s →", T("No subweapon limit:", "Subarmas sin límite:"), yes_no(cfg.multi_shot)); break;
  case MI_LOCK: snprintf(out, n, "%s ← %s →", T("Keep my weapons: ", "Bloquear armas:  "), yes_no(cfg.lock_weapon)); break;
  case MI_WEAPON: snprintf(out, n, "%s ← %s →", T("Weapon:   ", "Arma:     "), weapon_names[L][gram(RAM_WEAPON) & 3]); break;
  case MI_SUB: snprintf(out, n, "%s ← %s →", T("Subweapon:", "Subarma: "), gram(RAM_AMMO) ? sub_names[L][1 + (gram(RAM_SUBWEAPON) & 3)] : sub_names[L][0]); break;
  case MI_START: snprintf(out, n, "%s ← %d-%d →", T("Round:   ", "Ronda:   "), start_points[start_sel][0] + 1, start_points[start_sel][1] + 1); break;
  case MI_WARP: snprintf(out, n, T("Start a game at round %d-%d", "Empezar partida en la ronda %d-%d"), start_points[start_sel][0] + 1, start_points[start_sel][1] + 1); break;
  default:
    if (i >= MI_VIB0 && i <= MI_VIB_LAST) snprintf(out, n, "%-20s ← %s →", vib_names[L][i - MI_VIB0], vib_level_names[L][cfg.vib[i - MI_VIB0]]);
    break;
  }
}

// Each kind of game has its own state: one saved in a normal game (with cheats, at any round) cannot be
// loaded where achievements count. These also carry how the game was going (logros_run_*).
static void state_file(char* out, size_t n) {
  static const char* sv[3] = {"estado_survivor_0.sav", "estado_survivor_1.sav", "estado_survivor_2.sav"};
  cfg_path(out, n, game_mode == GM_LOGROS ? "estado_logros.sav" : game_mode == GM_SURVIVOR ? sv[sv_diff] : "estado1.sav");
}
static void sv_reset(void);
static void sv_after_load(void);

// After the state itself: how the achievements game was going (only in that mode) and the music.
typedef struct { int32_t id, from_pack; } state_music_t;

static void save_state_to(const char* p, int quiet);
static void do_save_state(void) { char p[1100]; state_file(p, sizeof p); save_state_to(p, 0); }
static void save_state_to(const char* p, int quiet) {
  size_t base = sms_state_size(), run = LOGROS_ON ? logros_run_size() : 0;
  size_t n = base + run + sizeof(state_music_t);
  uint8_t* buf = malloc(n);
  sms_save_state(&S, buf);
  if (run) logros_run_save(buf + base);
  state_music_t m = {music_current, cfg.music_pack > 0 && music_has(music_current)};
  memcpy(buf + base + run, &m, sizeof m);
  SDL_RWops* rw = SDL_RWFromFile(p, "wb");
  if (rw) { SDL_RWwrite(rw, buf, 1, n); SDL_RWclose(rw); if (!quiet) show_toast(T("State saved", "Estado guardado"), 1500); }
  else if (!quiet) show_toast(T("Could not save the state", "No se pudo guardar el estado"), 2000);
  free(buf);
}

// The game's own music comes back with the state; a track of a music pack has to be started again.
static void restore_state_music(const state_music_t* m) {
  music_stop();
  if (m->id <= 0) return;
  music_current = m->id;
  int pack_now = cfg.music_pack > 0 && music_has(m->id);
  if (pack_now != (m->from_pack != 0)) mod_music_request(&S, m->id);   // the source changed since it was saved
  else if (pack_now) music_play(m->id);
}

static int load_state_from(const char* p, const char* done);
static void do_load_state(void) { char p[1100]; state_file(p, sizeof p); load_state_from(p, T("State loaded", "Estado cargado")); }
static int load_state_from(const char* p, const char* done) {
  size_t n = 0;
  uint8_t* buf = load_file(p, &n);
  size_t base = sms_state_size(), run = LOGROS_ON ? logros_run_size() : 0;
  state_music_t m = {0, 0};
  // states saved by v0.10.1 and earlier have no music after them
  int has_music = n == base + run + sizeof m;
  int ok = buf && (has_music || n == base + run) && sms_load_state(&S, buf, base);
  if (ok) {
    if (run) logros_run_load(buf + base);
    if (game_mode == GM_SURVIVOR) sv_after_load();   // energy is read again from the state
    if (has_music) memcpy(&m, buf + base + run, sizeof m);
    restore_state_music(&m);
    mod_respawn_forget_slots(&E);
    show_toast(done, 1500);
  } else show_toast(T("There is no saved state", "No hay ningún estado guardado"), 2000);
  free(buf);
  return ok;
}

static void set_language(int lang) {
  if (lang && rom_crc != KNOWN_CRC) { show_toast(T("The translation needs the original, unmodified ROM", "La traducción necesita la ROM original sin modificar"), 3000); return; }
  cfg.lang = lang;
  apply_language();
  music_stop();
  sms_reset(&S);
  menu_set_open(0);
  show_toast(lang ? T("Game translated (restarted)", "Juego traducido (reiniciado)") : T("Game in English (restarted)", "Juego en inglés (reiniciado)"), 2500);
}

// TEMPORARY test switch (see test_inv)
static void set_test_inv(int on) {
  test_inv = on;
  char lp[1100]; cfg_path(lp, sizeof lp, on ? "logros_pruebas.txt" : "logros.txt");
  logros_switch_profile(lp);
  show_toast(on ? T("Testing: invincible, R2 speeds up, test profile", "Pruebas: invencible, R2 acelera, perfil de pruebas")
                : T("Testing off: your own profile again", "Pruebas apagadas: vuelve tu perfil"), 3000);
}

static void menu_change(int dir) {
  switch (cur_item()) {
  case MI_FORMAT: {
    // cycle 16:9 -> 4:3 -> CRT 8:7 -> Game Gear
    int f = cfg.widescreen ? 0 : cfg.crt ? 2 : cfg.gg ? 3 : 1;
    f = (f + dir + 4) % 4;
    cfg.widescreen = f == 0;
    cfg.crt = f == 2;
    cfg.gg = f == 3;
    // 8:7 keeps its shape (fit) and the bands left on a square screen take the picture's colours
    if (cfg.crt || cfg.gg) { cfg.scale = SCALE_FIT; cfg.bands = 1; }
    if (cfg.gg) show_toast(T("Game Gear: a smaller window that follows you. Pause shows the status bar",
                             "Game Gear: ventana más pequeña que te sigue. La pausa enseña el marcador"), 3500);
    mod_enhance_set_wide(&E, cfg.widescreen, MOD_WIDE_EXT);
    break;
  }
  case MI_SCALE: cfg.scale = (cfg.scale + dir + SCALE_COUNT) % SCALE_COUNT; break;
  case MI_FILTER: cfg.filter = (cfg.filter + dir + FILTER_COUNT) % FILTER_COUNT; break;
  case MI_SUIT: cfg.suit = (cfg.suit + dir + SUIT_COUNT) % SUIT_COUNT; apply_suit(); break;
  case MI_MARGIN: cfg.margin = (cfg.margin + dir + 3) % 3; break;
  case MI_BANDS: cfg.bands = !cfg.bands; break;
  case MI_TESTINV: set_test_inv(!test_inv); break;
  case MI_BR_LIFE: br_life = (br_life + dir + 3) % 3; break;
  case MI_BR_ARMS: br_arms = (br_arms + dir + 3) % 3; br_wsel = 0; br_ssel = 0; break;
  case MI_BR_WEAPON: br_wsel = (br_wsel + 1) % br_choices[br_arms]; break;
  case MI_BR_SUB: br_ssel = !br_ssel; break;
  case MI_PALETTE: cfg.palette = (cfg.palette + dir + PAL_COUNT) % PAL_COUNT; apply_palette(); break;

  case MI_VOLUME: cfg.volume += dir; if (cfg.volume < 0) cfg.volume = 0; if (cfg.volume > 10) cfg.volume = 10; break;
  case MI_BUTTONS: capture_jump = 1; return;
  case MI_SUBBTN: capture_jump = 2; return;
  case MI_TURBO: cfg.turbo ^= 1; break;
  case MI_TRIGGERS: cfg.triggers ^= 1; break;
  case MI_TOUCH: cfg.touch = (cfg.touch + dir + 3) % 3; break;
  case MI_TOUCHSIZE: cfg.touch_size = (cfg.touch_size + dir + 3) % 3; break;
  case MI_MUSIC:
    cfg.music_pack = (cfg.music_pack + (dir ? dir : 1) + MUSIC_SLOTS + 1) % (MUSIC_SLOTS + 1);
    music_stop();
    load_music_pack();
    if (music_current > 0 && !in_launcher) mod_music_request(&S, music_current);   // the current track again, from the new source
    break;
  case MI_MUSPACK:
    if (!cfg.music_pack) { show_toast(T("First choose OST 1, 2 or 3 in Music", "Primero elige OST 1, 2 o 3 en Música"), 2500); return; }
#ifdef __ANDROID__
    request_music_file(cfg.music_pack);
#else
    { char t[120]; snprintf(t, sizeof t, T("Copy the pack as %.12s in the data folder", "Copia el pack como %.12s en la carpeta de datos"), music_pack_file(cfg.music_pack)); show_toast(t, 3000); }
#endif
    return;
  case MI_MUSTEST:
    if (dir) music_test_sel = (music_test_sel + dir + music_track_count) % music_track_count;
    return;
  case MI_DIFF: cfg.difficulty = (cfg.difficulty + dir + 3) % 3; apply_game_options(); break;
  case MI_RESPAWN: cfg.respawn = (cfg.respawn + dir + 3) % 3; apply_game_options(); break;
  case MI_DJUMP: cfg.dbl_jump ^= 1; apply_game_options(); break;
  case MI_STAIRS: cfg.stairs ^= 1; apply_game_options(); break;
  case MI_LANG: set_language(!lang_active()); break;
  case MI_INV:
    cfg.cheat_inv ^= 1;
    if (!cfg.cheat_inv && rom_crc == KNOWN_CRC) gram_w(RAM_INVINCIBLE, 0);
    break;
  case MI_LIVES: cfg.cheat_lives ^= 1; break;
  case MI_AMMO: cfg.ammo_inf ^= 1; break;
  case MI_MULTI: cfg.multi_shot ^= 1; apply_game_options(); break;
  case MI_LOCK: cfg.lock_weapon ^= 1; locked_weapon = locked_sub = -1; break;
  case MI_UILANG: cfg.ui_lang = !cfg.ui_lang; break;
  case MI_WEAPON:
    if (in_launcher || !cheats_ok()) { show_toast(T("Only while playing", "Solo durante la partida"), 1500); return; }
    gram_w(RAM_WEAPON, (uint8_t)((gram(RAM_WEAPON) + dir + 4) & 3));
    locked_weapon = gram(RAM_WEAPON) & 3;
    mod_request_hud(&E, MOD_HUD_WEAPON);
    return;
  case MI_SUB: {
    if (in_launcher || !cheats_ok()) { show_toast(T("Only while playing", "Solo durante la partida"), 1500); return; }
    int k = gram(RAM_AMMO) ? 1 + (gram(RAM_SUBWEAPON) & 3) : 0;
    k = (k + dir + 5) % 5;
    if (k == 0) gram_w(RAM_AMMO, 0);
    else { gram_w(RAM_SUBWEAPON, (uint8_t)(k - 1)); gram_w(RAM_AMMO, 99); locked_sub = k - 1; }
    mod_request_hud(&E, MOD_HUD_SUBWEAPON);
    return;
  }
  case MI_START: start_sel = (start_sel + dir + START_COUNT) % START_COUNT; return;
  default:
    if (cur_item() >= MI_VIB0 && cur_item() <= MI_VIB_LAST) {
      int k = cur_item() - MI_VIB0;
      cfg.vib[k] = (cfg.vib[k] + dir + 4) % 4;
      vibrate_sample(k);
      break;
    }
    return;
  }
  cfg_save();
}

static void menu_open_at(int page) {
  menu_open = 1;
  capture_jump = 0;
  confirm_original = 0;
  menu_root = menu_page = page;
  menu_sel = 0;
  SDL_ClearQueuedAudio(audio_dev);
}

static void menu_set_open(int open) {
  if (open) { menu_open_at(in_launcher ? PAGE_HOME : game_mode == GM_SURVIVOR ? PAGE_MAIN_SURVIVOR : game_mode == GM_BOSSRUSH ? PAGE_MAIN_BR : ADVANTAGES_BLOCKED ? PAGE_MAIN_LOCKED : PAGE_MAIN); return; }
  menu_open = 0;
  capture_jump = 0;
}

static void menu_goto(int page) { menu_page = page; menu_sel = 0; capture_jump = 0; confirm_original = 0; }

// Everything that changes how the game plays goes back to the original: cheats, difficulty, enemies,
// double jump, slowdowns, suit and music. Picture, controls, volume and languages stay as they are.
static void reset_to_original(void) {
  cfg.cheat_inv = cfg.cheat_lives = cfg.ammo_inf = cfg.lock_weapon = cfg.multi_shot = 0;
  if (rom_crc == KNOWN_CRC) gram_w(RAM_INVINCIBLE, 0);
  locked_weapon = locked_sub = -1;
  cfg.difficulty = MOD_DIFF_NORMAL;
  cfg.respawn = MOD_RESPAWN_ON;
  cfg.dbl_jump = 0;
  cfg.stairs = 0;
  cfg.turbo = 0;
  cfg.suit = 0;
  apply_suit();
  apply_game_options();
  if (cfg.music_pack) {
    cfg.music_pack = 0;
    music_stop();
    load_music_pack();
    if (music_current > 0 && !in_launcher) mod_music_request(&S, music_current);   // the original track again
  }
  cfg_save();
  show_toast(T("Original game restored (picture and controls kept)", "Juego original restablecido (imagen y controles se mantienen)"), 3000);
}

// The version recommended by the project (also what a new installation starts with): the original rules
// without slowdowns, real 16:9 with scanlines and the original colours, suit and music, no subweapon
// limit as the only cheat, the recommended vibration, and touch controls only when no gamepad is
// connected. Buttons, volume and languages stay.
static void apply_remaster(void) {
  cfg.cheat_inv = cfg.cheat_lives = cfg.ammo_inf = cfg.lock_weapon = 0;
  cfg.multi_shot = 1;
  for (int k = 0; k < VIB_COUNT; k++) cfg.vib[k] = vib_recommended[k];
  if (rom_crc == KNOWN_CRC) gram_w(RAM_INVINCIBLE, 0);
  locked_weapon = locked_sub = -1;
  cfg.difficulty = MOD_DIFF_NORMAL;
  cfg.respawn = MOD_RESPAWN_ON;
  cfg.dbl_jump = 0;
  cfg.stairs = 1;
  cfg.turbo = 1;
  cfg.widescreen = 1; cfg.crt = 0; cfg.gg = 0;
  mod_enhance_set_wide(&E, 1, MOD_WIDE_EXT);
  cfg.scale = SCALE_FIT;
  cfg.filter = FILTER_SCANLINES;
  cfg.palette = PAL_ORIGINAL; apply_palette();
  cfg.margin = 0;
  cfg.suit = 0; apply_suit();
  cfg.touch = 2;
  apply_game_options();
  if (cfg.music_pack) {
    cfg.music_pack = 0;
    music_stop();
    load_music_pack();
    if (music_current > 0 && !in_launcher) mod_music_request(&S, music_current);
  }
  cfg_save();
  show_toast(T("Remastered version restored", "Versión remasterizada restablecida"), 3000);
}

static int rw_count_reset;   // the rewind history no longer applies (see the main loop)
static int show_logros, logros_page;
static int waiting_import;

// Plays a mode: the current game goes on if it is the same mode, otherwise a new one starts
// ---------------------------------------------------------------- Survivor mode
#define RAM_ENERGY 0xCA3A   // hero energy, 0x20 = full bar
#define MUS_SV_CONTINUE 0x82
#define MUS_SV_ROUND_CLEAR 0x86
static int sv_over;            // the game has ended: the result screen is shown and the game waits
static Uint32 sv_over_at;      // the life was lost: the screen comes after the fall
static int sv_refill_round = -1;   // normal: energy comes back once, at the start of this round
static int sv_end_round, sv_end_stage;
// No saving or loading by hand in Survivor: the game is saved by itself when each stage starts, and after
// losing the life that stage can be tried again (it costs the "No safety net" achievement).
static int sv_ck_round = -1, sv_ck_stage = -1;

static void sv_ck_file(char* out, size_t n) { char nm[40]; snprintf(nm, sizeof nm, "survivor_fase_%d.sav", sv_diff); cfg_path(out, n, nm); }

static void sv_after_load(void) {
  sv_over = 0; sv_over_at = 0; sv_refill_round = -1;
  E.energy_lock = false; E.energy_keep = 0;
}

static void sv_reset(void) {
  sv_after_load();
  sv_ck_round = sv_ck_stage = -1;
}

static void sv_game_over(void) {
  if (sv_over) return;
  sv_over = 1;
  sv_over_at = 0;
  music_stop();
  logros_flush();
}

// Every frame of a Survivor game (after the emulated frame, with its events)
static void sv_frame(uint32_t ev) {
  if (sv_over) return;
  if (sv_over_at && (Sint32)(SDL_GetTicks() - sv_over_at) >= 0) { sv_game_over(); return; }
  if (rom_crc != KNOWN_CRC) return;
  int playing = mod_is_gameplay(&S);
  if ((ev & MOD_EV_DEATH) && !sv_over_at) {
    sv_end_round = gram(RAM_ROUND); sv_end_stage = gram(RAM_STAGE);
    sv_over_at = SDL_GetTicks() + 2500;   // the fall is seen, then the game ends
  }
  if (!playing) return;
  // a new stage: saved by itself, to try it again if the life is lost
  if (!sv_over_at && (gram(RAM_ROUND) != sv_ck_round || gram(RAM_STAGE) != sv_ck_stage)) {
    if (gram(RAM_LIVES) != 1) { gram_w(RAM_LIVES, 1); mod_request_hud(&E, MOD_HUD_LIVES); }
    sv_ck_round = gram(RAM_ROUND); sv_ck_stage = gram(RAM_STAGE);
    char p[1100]; sv_ck_file(p, sizeof p);
    save_state_to(p, 1);
  }
  // a single life: the counter shows 0 lives left, also after picking up an extra life
  if (gram(RAM_LIVES) != 1) { gram_w(RAM_LIVES, 1); mod_request_hud(&E, MOD_HUD_LIVES); }
  // energy: easy as in the game; normal only at the start of the round after a boss; hard never
  int round = gram(RAM_ROUND);
  if (sv_refill_round >= 0 && round == sv_refill_round) sv_refill_round = -1;   // that start has been and gone
  E.energy_lock = sv_diff == 2 || (sv_diff == 1 && sv_refill_round < 0);
  uint8_t en = gram(RAM_ENERGY);
  if (en && en <= 0x20) E.energy_keep = en;
}

static void sv_music(int id) {
  if (id == MUS_SV_CONTINUE) { sv_end_round = gram(RAM_ROUND); sv_end_stage = gram(RAM_STAGE); sv_game_over(); }
  if (id == MUS_SV_ROUND_CLEAR && sv_diff == 1 && gram(RAM_STAGE) == 2) {
    sv_refill_round = gram(RAM_ROUND) + 1;
    E.energy_lock = false;
  }
}

static void start_mode(int mode);
static void sv_retry(void) {
  game_mode = GM_NORMAL;   // makes start_mode begin a new game
  start_mode(GM_SURVIVOR);
}

static void sv_retry_stage(void) {
  char p[1100]; sv_ck_file(p, sizeof p);
  int r = sv_ck_round, st = sv_ck_stage;
  if (r < 0 || !load_state_from(p, T("Trying the stage again", "Repites la fase"))) { sv_retry(); return; }
  sv_ck_round = r; sv_ck_stage = st;
}

static void draw_sv_over(void) {
  ui_rect(0, 0, UI_W, UI_H, 0xC8000000);
  int by = UI_H / 2 - 90;
  ui_rect(UI_W / 2 - 280, by, 560, 180, 0xF0140C18);
  ui_rect(UI_W / 2 - 280, by, 560, 2, 0xFFB0302A);
  ui_rect(UI_W / 2 - 280, by + 178, 560, 2, 0xFFB0302A);
  char t[96];
  snprintf(t, sizeof t, "SURVIVOR  ·  %s", diff_names[ES][sv_diff]);
  ui_text_center(by + 14, t, 0xE8C060);
  ui_text_center(by + 48, T("Your only life is gone", "Has perdido tu única vida"), 0xFFFFFF);
  snprintf(t, sizeof t, T("You reached round %d-%d", "Llegaste a la ronda %d-%d"), sv_end_round + 1, sv_end_stage + 1);
  ui_text_center(by + 74, t, 0xB8B0A8);
  // three choices: the stage again, the whole game again, the start screen
  const char* lab[3] = {T("A: Retry stage", "A: Repetir fase"), T("X: Start over", "X: Reiniciar"), T("B: Start screen", "B: Inicio")};
  for (int i = 0; i < 3; i++) {
    int x = UI_W / 2 - 270 + i * 182;
    ui_rect(x, by + 116, 174, 40, i == 0 ? 0xFF5A1A1A : 0xFF2A2430);
    ui_text(x + (174 - text_width(lab[i])) / 2, by + 125, lab[i], 0xFFFFFF);
  }
}

// the result screen takes every button: A the stage again, X the whole game again, B the start screen
static int sv_over_event(SDL_Event* e) {
  int ok = 0, back = 0, over = 0;
  if (e->type == SDL_KEYDOWN && !e->key.repeat) {
    SDL_Keycode k = e->key.keysym.sym;
    ok = k == SDLK_RETURN || k == SDLK_z || k == SDLK_SPACE;
    back = k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_x || k == SDLK_BACKSPACE;
    over = k == SDLK_c;
  } else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
    int b = e->cbutton.button;
    ok = b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START;
    back = b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK;
    over = b == SDL_CONTROLLER_BUTTON_X;
  } else if (e->type == SDL_FINGERDOWN) {
    ok = e->tfinger.x < 0.4f; over = !ok && e->tfinger.x < 0.6f; back = !ok && !over;
  } else return e->type == SDL_KEYUP || e->type == SDL_CONTROLLERBUTTONUP || e->type == SDL_FINGERUP || e->type == SDL_FINGERMOTION;
  if (ok) sv_retry_stage();
  else if (over) sv_retry();
  else if (back) { sv_retry(); in_launcher = 1; music_set_paused(true); SDL_ClearQueuedAudio(audio_dev); }
  return 1;
}

// ---------------------------------------------------------------- Boss Rush run
// Each boss starts from the moment the player walked into its room in one of their own games: when the game
// sets C90E = 3 (the boss is coming) in any other mode, that moment is saved as jefe_N.sav, once per boss.
// The Boss Rush loads them one after another, with its own weapons, energy and a single life. Nothing from
// the ROM goes in the APK: the pictures are the player's own.
#define RAM_STAGE_STATE 0xC90E   // 1 stage cleared, 3 boss
#define MUS_BR_ROUND_CLEAR 0x86
#define MUS_BR_ENDING 0x8D
#define MUS_BR_CONTINUE 0x82
#define BR_BOSSES 5
static int br_k;              // boss being fought (0..4)
static int br_transit;        // a boss has fallen: the next one is loaded when br_next_at comes
static Uint32 br_next_at;
static int br_fight, br_down; // its room is loaded / it has fallen (the round-clear music has sounded)
static int br_zero;           // its energy bar is empty: the clock stops, unless the boss comes back (some have two forms)
static unsigned br_frames;    // time of the run (playing frames, 60 per second)
static int br_over;           // result screen: 1 life lost, 2 finished
static Uint32 br_over_at;
static int br_record;
static int br_prev_state = -1;   // C90E in the frame before (to see the game itself call a boss)

static void br_snap_file(int k, char* out, size_t n) { char nm[32]; snprintf(nm, sizeof nm, "jefe_%d.sav", k + 1); cfg_path(out, n, nm); }
static int br_have(int k) { char p[1100]; br_snap_file(k, p, sizeof p); FILE* f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }
static int br_have_all(void) { for (int k = 0; k < BR_BOSSES; k++) if (!br_have(k)) return 0; return 1; }
static int br_have_count(void) { int n = 0; for (int k = 0; k < BR_BOSSES; k++) n += br_have(k); return n; }

// the state and its music, without the achievements game (each run is the Boss Rush's own)
static void br_snap_save(int k) {
  char p[1100]; br_snap_file(k, p, sizeof p);
  size_t base = sms_state_size();
  uint8_t* buf = malloc(base + sizeof(state_music_t));
  if (!buf) return;
  sms_save_state(&S, buf);
  state_music_t m = {music_current, cfg.music_pack > 0 && music_has(music_current)};
  memcpy(buf + base, &m, sizeof m);
  SDL_RWops* rw = SDL_RWFromFile(p, "wb");
  if (rw) { SDL_RWwrite(rw, buf, 1, base + sizeof m); SDL_RWclose(rw); }
  free(buf);
  char t[96]; snprintf(t, sizeof t, T("Boss Rush: boss %d unlocked", "Boss Rush: jefe %d desbloqueado"), k + 1);
  show_toast(t, 2500);
}

static int br_snap_load(int k) {
  char p[1100]; br_snap_file(k, p, sizeof p);
  size_t n = 0, base = sms_state_size();
  uint8_t* buf = load_file(p, &n);
  int ok = buf && n == base + sizeof(state_music_t) && sms_load_state(&S, buf, base);
  if (ok) {
    state_music_t m; memcpy(&m, buf + base, sizeof m);
    restore_state_music(&m);
    mod_respawn_forget_slots(&E);
  }
  free(buf);
  return ok;
}

// every frame of the other modes: the game has just called a boss in its own stage
static void br_watch_frame(void) {
  if (rom_crc != KNOWN_CRC) return;
  int st = gram(RAM_STAGE_STATE), r = gram(RAM_ROUND);
  if (st == 3 && br_prev_state != 3 && r < BR_BOSSES && gram(RAM_STAGE) == (r == BR_BOSSES - 1 ? 0 : 2) && !br_have(r))
    br_snap_save(r);
  br_prev_state = st;
}

static void br_reset_run(void) {
  br_k = 0; br_transit = 0; br_fight = br_down = 0; br_frames = 0;
  br_over = 0; br_over_at = 0; br_record = 0;
  E.energy_lock = true; E.energy_keep = 0x20;   // potions do not heal
}

static void br_time_text(unsigned fr, char* out, size_t n) {
  unsigned cs = fr * 100 / 60;
  snprintf(out, n, "%u:%02u.%02u", cs / 6000, cs / 100 % 60, cs % 100);
}

static void br_lost(void) { if (!br_over) { br_over = 1; br_over_at = 0; music_stop(); logros_flush(); } }

// into the room of boss br_k, with this run's weapons, a full load of ammo, its energy and one life
static void br_enter(void) {
  if (!br_snap_load(br_k)) { br_lost(); return; }
  int w = br_weapons[br_arms][br_wsel], sub = br_subs[br_arms][br_ssel];
  gram_w(RAM_WEAPON, (uint8_t)w); gram_w(RAM_SUBWEAPON, (uint8_t)sub); gram_w(RAM_AMMO, sub <= 1 ? 16 : 8);
  gram_w(RAM_LIVES, 1);
  gram_w(RAM_ENERGY, E.energy_keep ? E.energy_keep : 0x20);
  gram_w(RAM_INVINCIBLE, 0);
  mod_request_hud(&E, MOD_HUD_WEAPON | MOD_HUD_SUBWEAPON | MOD_HUD_LIVES | MOD_HUD_ENERGY);
  br_transit = 0; br_fight = 1; br_down = 0; br_zero = 0;
  char t[64]; snprintf(t, sizeof t, T("Boss %d of %d", "Jefe %d de %d"), br_k + 1, BR_BOSSES);
  show_toast(t, 2000);
}

static void br_before_frame(void) {
  if (br_transit && !br_over && (Sint32)(SDL_GetTicks() - br_next_at) >= 0) br_enter();
}

// the boss of this room has fallen: only the round-clear music says so for sure. An empty energy bar is not
// enough: the second boss (and maybe others) empties it at the end of its first form and then fills it again
static void br_boss_beaten(void) {
  if (br_down) return;
  br_down = 1;
  // energy for the next boss: easy all of it, normal half a bar more, hard what is left
  int en = gram(RAM_ENERGY);
  en = br_life == 0 ? 0x20 : br_life == 1 ? en + 0x10 : en;
  if (en > 0x20) en = 0x20;
  if (en < 1) en = 1;
  E.energy_keep = (uint8_t)en;
  if (br_k < BR_BOSSES - 1) logros_br_boss(br_k);
}

// after each emulated frame, with its events
static void br_frame(uint32_t ev) {
  if (br_over || rom_crc != KNOWN_CRC || !br_fight) return;
  if (br_over_at && (Sint32)(SDL_GetTicks() - br_over_at) >= 0) { br_lost(); return; }
  if ((ev & MOD_EV_DEATH) && !br_over_at) br_over_at = SDL_GetTicks() + 2500;
  if (ev & MOD_EV_BOSS_DOWN) br_zero = 1;
  else if (br_zero && !br_down && gram(0xCB3A)) br_zero = 0;   // a new form of the boss, with energy again
  if (!br_down && !br_zero && !br_over_at) br_frames++;   // the clock runs from entering the room until the boss falls
  if (!mod_is_gameplay(&S)) return;
  if (gram(RAM_LIVES) != 1) { gram_w(RAM_LIVES, 1); mod_request_hud(&E, MOD_HUD_LIVES); }
  // the weapons of the run stay (anything picked up in a boss room does not change them)
  int w = br_weapons[br_arms][br_wsel], sub = br_subs[br_arms][br_ssel];
  if (gram(RAM_WEAPON) != w) { gram_w(RAM_WEAPON, (uint8_t)w); mod_request_hud(&E, MOD_HUD_WEAPON); }
  if (gram(RAM_AMMO) && gram(RAM_SUBWEAPON) != sub) { gram_w(RAM_SUBWEAPON, (uint8_t)sub); mod_request_hud(&E, MOD_HUD_SUBWEAPON); }
}

static void br_music(int id) {
  if (br_over || !br_fight) return;
  if (id == MUS_BR_CONTINUE) { br_lost(); return; }
  if (!br_over_at && ((id == MUS_BR_ROUND_CLEAR && br_k < BR_BOSSES - 1) || (id == MUS_BR_ENDING && br_k == BR_BOSSES - 1)))
    br_boss_beaten();
  if (id == MUS_BR_ROUND_CLEAR && br_down && br_k < BR_BOSSES - 1 && !br_transit) {
    br_k++; br_transit = 1; br_next_at = SDL_GetTicks() + 2000;   // a moment to see it fall, then the next room
  } else if (id == MUS_BR_ENDING && br_down && br_k == BR_BOSSES - 1) {
    br_record = logros_br_finish(br_diff(), br_frames);
    br_over = 2;
    music_stop();
  }
}

static void draw_br_hud(void) {
  char t[64], tt[32]; br_time_text(br_frames, tt, sizeof tt);
  snprintf(t, sizeof t, "%d/%d  %s", br_k + 1, BR_BOSSES, tt);
  int w = text_width(t) + 16;
  ui_rect(UI_W - w - 6, UI_H - 28, w, 22, 0x90000000);
  ui_text(UI_W - w + 2, UI_H - 26, t, 0xE8C060);
}

static void draw_br_over(void) {
  ui_rect(0, 0, UI_W, UI_H, 0xC8000000);
  int by = UI_H / 2 - 90;
  ui_rect(UI_W / 2 - 280, by, 560, 180, 0xF0140C18);
  ui_rect(UI_W / 2 - 280, by, 560, 2, 0xFFB0302A);
  ui_rect(UI_W / 2 - 280, by + 178, 560, 2, 0xFFB0302A);
  char t[128], tm[32], best[32] = "-";
  snprintf(t, sizeof t, "BOSS RUSH  ·  %s", diff_names[ES][br_diff()]);
  ui_text_center(by + 14, t, 0xE8C060);
  br_time_text(br_frames, tm, sizeof tm);
  if (logros_br_best(br_diff())) br_time_text(logros_br_best(br_diff()), best, sizeof best);
  if (br_over == 2) ui_text_center(by + 48, br_record ? T("All bosses beaten. New record!", "¡Todos los jefes vencidos! ¡Nuevo récord!")
                                                      : T("All bosses beaten!", "¡Todos los jefes vencidos!"), 0xFFFFFF);
  else {
    snprintf(t, sizeof t, T("You fell against boss %d of %d", "Has caído ante el jefe %d de %d"), br_k + 1, BR_BOSSES);
    ui_text_center(by + 48, t, 0xFFFFFF);
  }
  snprintf(t, sizeof t, T("Time %s   Record %s", "Tiempo %s   Récord %s"), tm, best);
  ui_text_center(by + 74, t, 0xB8B0A8);
  ui_rect(UI_W / 2 - 250, by + 116, 240, 40, 0xFF5A1A1A);
  ui_rect(UI_W / 2 + 10, by + 116, 240, 40, 0xFF2A2430);
  const char* a = T("A: Try again", "A: Otra vez"), *b = T("B: Start screen", "B: Inicio");
  ui_text(UI_W / 2 - 130 - text_width(a) / 2, by + 125, a, 0xFFFFFF);
  ui_text(UI_W / 2 + 130 - text_width(b) / 2, by + 125, b, 0xFFFFFF);
}

static void start_mode(int mode);
static void br_new_run(void) {
  if (!br_have_all()) {
    char t[120], miss[32] = ""; int n = 0;
    for (int k = 0; k < BR_BOSSES; k++) if (!br_have(k)) n += snprintf(miss + n, sizeof miss - (size_t)n, "%s%d", n ? ", " : "", k + 1);
    snprintf(t, sizeof t, T("Bosses still locked: %s. Reach them once in any game", "Faltan jefes: %s. Llega a ellos una vez en cualquier partida"), miss);
    show_toast(t, 4000);
    return;
  }
  game_mode = GM_NORMAL;   // makes start_mode begin a new game
  start_mode(GM_BOSSRUSH);
  br_enter();
}

static int br_over_event(SDL_Event* e) {
  int ok = 0, back = 0;
  if (e->type == SDL_KEYDOWN && !e->key.repeat) {
    SDL_Keycode k = e->key.keysym.sym;
    ok = k == SDLK_RETURN || k == SDLK_z || k == SDLK_SPACE;
    back = k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_x || k == SDLK_BACKSPACE;
  } else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
    int b = e->cbutton.button;
    ok = b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START;
    back = b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK;
  } else if (e->type == SDL_FINGERDOWN) {
    ok = e->tfinger.x < 0.5f; back = !ok;
  } else return e->type == SDL_KEYUP || e->type == SDL_CONTROLLERBUTTONUP || e->type == SDL_FINGERUP || e->type == SDL_FINGERMOTION;
  if (ok) br_new_run();
  else if (back) { br_new_run(); in_launcher = 1; music_set_paused(true); SDL_ClearQueuedAudio(audio_dev); }
  return 1;
}

static void start_mode(int mode) {
  static int last_sv = -1;
  if (mode != game_mode || (mode == GM_SURVIVOR && sv_diff != last_sv)) {
    game_mode = mode;
    if (mode == GM_SURVIVOR) last_sv = sv_diff;
    logros_set_survivor(mode == GM_SURVIVOR ? sv_diff : -1);
    sv_reset();
    music_stop();
    sms_reset(&S);
    warp_pending = 0;
    locked_weapon = locked_sub = -1;
    rw_count_reset = 1;
    logros_reset_run();
    if (rom_crc == KNOWN_CRC) gram_w(RAM_INVINCIBLE, 0);
    apply_game_options();
    if (mode == GM_BOSSRUSH) br_reset_run();
  }
  menu_set_open(0);
  in_launcher = 0;
  music_set_paused(false);
  if (mode == GM_LOGROS) show_toast(T("Achievements mode: no rewind or cheats", "Modo logros: sin rebobinado ni trucos"), 3000);
  else if (mode == GM_SURVIVOR) show_toast(T("Survivor: a single life. Good luck", "Survivor: una sola vida. Suerte"), 3000);
  else if (mode == GM_BOSSRUSH) toast_until = 0;
  else show_toast(T("Press SELECT or BACK to open the menu", "Pulsa SELECT o ATRÁS para abrir el menú"), 3000);
}

static void profile_transfer(int import) {
#ifdef __ANDROID__
  logros_flush();
  if (import) waiting_import = 1;
  SDL_AndroidSendMessage(import ? 0x8006 : 0x8005, 0);
#else
  (void)import;
  show_toast(T("Only on Android", "Solo en Android"), 2000);
#endif
}

// The normal game's saved state (estado1.sav) is copied to a file the player chooses, to send it for bug reports.
static void state_export(void) {
#ifdef __ANDROID__
  char p[1100]; cfg_path(p, sizeof p, "estado1.sav");
  SDL_RWops* rw = SDL_RWFromFile(p, "rb");
  if (!rw) { show_toast(T("There is no saved state", "No hay ningún estado guardado"), 2000); return; }
  SDL_RWclose(rw);
  SDL_AndroidSendMessage(0x8008, 0);
#else
  show_toast(T("Only on Android", "Solo en Android"), 2000);
#endif
}

static void import_profile_file(void) {
  char p[1100]; cfg_path(p, sizeof p, "perfil_importado.txt");
  FILE* f = fopen(p, "rb");
  if (!f) return;   // nothing was chosen
  fclose(f);
  int gained = logros_merge_file(p);
  remove(p);
  char t[96];
  if (gained < 0) snprintf(t, sizeof t, "%s", T("That file is not a valid profile", "Ese archivo no es un perfil válido"));
  else snprintf(t, sizeof t, T("Profile imported: %d new achievements", "Perfil importado: %d logros nuevos"), gained);
  show_toast(t, 3000);
}

static void menu_activate(void) {
  switch (cur_item()) {
  case MI_MODE_NORMAL:
    if (rom_crc != KNOWN_CRC && game_mode != GM_NORMAL) break;
    start_mode(GM_NORMAL); break;
  case MI_MODE_LOGROS:
    if (rom_crc != KNOWN_CRC) { show_toast(T("Achievements need the original ROM", "Los logros necesitan la ROM original"), 2500); break; }
    start_mode(GM_LOGROS); break;
  case MI_MODE_SURVIVOR:
    if (rom_crc != KNOWN_CRC) { show_toast(T("Achievements need the original ROM", "Los logros necesitan la ROM original"), 2500); break; }
    menu_goto(PAGE_SURVIVOR); menu_sel = sv_diff; break;
  case MI_SV_EASY: case MI_SV_NORMAL: case MI_SV_HARD:
    sv_diff = cur_item() - MI_SV_EASY; start_mode(GM_SURVIVOR); break;
  case MI_MODE_BOSSRUSH:
    if (rom_crc != KNOWN_CRC) { show_toast(T("Achievements need the original ROM", "Los logros necesitan la ROM original"), 2500); break; }
    menu_goto(PAGE_BOSSRUSH); menu_sel = 4; break;
  case MI_BR_START: br_new_run(); break;
  case MI_LOGROS_LIST: show_logros = 1; logros_page = 0; break;
  case MI_PROFILE_EXPORT: profile_transfer(0); break;
  case MI_PROFILE_IMPORT: profile_transfer(1); break;
  case MI_STATE_EXPORT: state_export(); break;
  case MI_RESUME: menu_set_open(0); break;
  case MI_SAVE: do_save_state(); menu_set_open(0); break;
  case MI_LOAD: do_load_state(); menu_set_open(0); break;
  case MI_CHEATS:
    if (rom_crc != KNOWN_CRC) { show_toast(T("Cheats need the original ROM", "Los trucos necesitan la ROM original"), 2000); break; }
    menu_goto(PAGE_CHEATS); break;
  case MI_VIDEO: menu_goto(PAGE_VIDEO); break;
  case MI_GAME:
    if (rom_crc != KNOWN_CRC) { show_toast(T("These options need the original ROM", "Estas opciones necesitan la ROM original"), 2000); break; }
    menu_goto(PAGE_GAME); break;
  case MI_OPTIONS: menu_goto(PAGE_OPTIONS); break;
  case MI_BACK: if (menu_page == PAGE_VIBRA) { menu_goto(PAGE_CONTROLS); menu_sel = 5; } else if (menu_page == menu_root) menu_set_open(0); else menu_goto(menu_root); break;
  case MI_CONTROLS: menu_goto(PAGE_CONTROLS); break;
  case MI_HOME: menu_set_open(0); in_launcher = 1; music_set_paused(true); SDL_ClearQueuedAudio(audio_dev); break;
  case MI_RESET:
    if (game_mode == GM_BOSSRUSH) { br_new_run(); break; }
    music_stop(); sms_reset(&S); if (game_mode == GM_SURVIVOR) sv_reset(); menu_set_open(0); show_toast(T("Game reset", "Juego reiniciado"), 1500); break;
  case MI_MUSTEST:
    if (rom_crc != KNOWN_CRC) break;
    if (in_launcher) { show_toast(T("Try the music while playing", "Prueba la música durante la partida"), 2000); break; }
    mod_music_request(&S, music_tracks[music_test_sel].id);
    menu_set_open(0);
    show_toast(ES ? music_tracks[music_test_sel].name : music_tracks[music_test_sel].name_en, 2000);
    break;
  case MI_WARP:
    game_mode = GM_NORMAL; apply_game_options();   // chosen from the cheats: this is a normal game
    music_stop(); sms_reset(&S); warp_pending = 1; menu_set_open(0); in_launcher = 0;
    show_toast(T("Press START: the game will begin at that round", "Pulsa START: la partida empezará en esa ronda"), 3000);
    break;
  case MI_QUIT: running = 0; break;
  case MI_ORIGINAL:
    if (confirm_original != 1) { confirm_original = 1; break; }
    confirm_original = 0;
    reset_to_original();
    break;
  case MI_REMASTER:
    if (confirm_original != 2) { confirm_original = 2; break; }
    confirm_original = 0;
    apply_remaster();
    break;
  case MI_VIBRA: menu_goto(PAGE_VIBRA); break;
  default: menu_change(1); break;
  }
}

static void menu_nav(int dy, int dx, int ok, int back) {
  int n = page_count(menu_page);
  if (dy || dx || back) confirm_original = 0;
  if (dy) { menu_sel = (menu_sel + dy + n) % n; capture_jump = 0; }
  if (dx) menu_change(dx);
  if (ok) menu_activate();
  if (back) { if (menu_page == PAGE_VIBRA) { menu_goto(PAGE_CONTROLS); menu_sel = 5; } else if (menu_page != menu_root) menu_goto(menu_root); else menu_set_open(0); }
}

static int rewinding, fast_forward;
#define CREDITS_NAME_Y 96
static int show_credits;
static int credits_taps;           // presses on the author's name: three show the ticket
static Uint32 credits_tap_time;

static void draw_credits(void) {
  ui_rect(0, 0, UI_W, UI_H, 0xC8000000);
  int y = 34;
  ui_text_center(y, "RE-MASTER-ED OF DARKNESS", 0xE8C060); y += 22;
  ui_text_center(y, "A RECOMP PROJECT  ·  v" APP_VERSION, 0xB8B0A8); y += 40;
  ui_text_center(y, T("Recomp and launcher by Juanmote", "Recomp y launcher por Juanmote"), SDL_GetTicks() - credits_tap_time < 200 && credits_taps ? 0xE8C060 : 0xFFFFFF); y += 26;
  ui_text_center(y, T("Spanish translation of the game: Re-Master-Ed", "Traducción del juego al español: Re-Master-Ed"), 0xFFFFFF); y += 26;
  y += 8;
  ui_text_center(y, "Master of Darkness © 1992 Sega / SIMS", 0xB8B0A8); y += 22;
  ui_text_center(y, T("The app does not include the game: use your own ROM", "La app no incluye el juego: usa tu propia ROM"), 0xB8B0A8); y += 34;
  ui_text_center(y, T("SDL2 (zlib) · stb (public domain) · miniz (MIT)", "SDL2 (zlib) · stb (dominio público) · miniz (MIT)"), 0x807870); y += 22;
  ui_text_center(y, T("DejaVu font (free license)", "Fuente DejaVu (licencia libre)"), 0x807870);
  ui_text_center(UI_H - 34, T("Press B or tap to go back", "Pulsa B o toca para volver"), 0x807870);
}

#define LOGROS_PER_PAGE 7
static int logros_pages(void) { return (logros_count() + LOGROS_PER_PAGE - 1) / LOGROS_PER_PAGE; }

static void draw_logros(void) {
  ui_rect(0, 0, UI_W, UI_H, 0xF0100A14);
  char t[96];
  snprintf(t, sizeof t, test_inv ? T("ACHIEVEMENTS (TESTING)  %d/%d", "LOGROS (PRUEBAS)  %d/%d") : T("ACHIEVEMENTS  %d/%d", "LOGROS  %d/%d"), logros_unlocked_count(), logros_count());
  ui_text_center(8, t, 0xE8C060);
  int y = 34;
  for (int i = logros_page * LOGROS_PER_PAGE; i < logros_count() && i < (logros_page + 1) * LOGROS_PER_PAGE; i++) {
    const logro_t* l = logros_get(i);
    int on = logros_has(i);
    ui_rect(26, y + 5, 28, 28, on ? 0xFFE8C060 : 0xFF3A3440);
    ui_rect(29, y + 8, 22, 22, on ? 0xFF3A1418 : 0xFF1C1820);
    uint32_t ic[11 * 11]; int dots = 0;
    if (logro_icon(l->id, ic, &dots))   // locked: a dark silhouette of the icon
      for (int py = 0; py < 11; py++)
        for (int px = 0; px < 11; px++) {
          uint32_t c = ic[py * 11 + px];
          if (!c) continue;
          if (!on) c = 0xFF4A4450;
          ui_rect(29 + px * 2, y + 8 + py * 2, 2, 2, c);
        }
    for (int d = 0; d < dots; d++) ui_rect(40 - dots * 3 + d * 6 + 1, y + 30, 4, 3, 0xFF200810);   // on the bottom of the frame
    ui_text(68, y - 1, l->name[ES], on ? 0xFFFFFF : 0x8C8490);
    ui_text(68, y + 18, l->desc[ES], on ? 0xB8B0A8 : 0x6C6470);
    y += 41;
  }
  snprintf(t, sizeof t, T("‹ Page %d/%d ›   Enemies: %u   Masks: %u", "‹ Página %d/%d ›   Enemigos: %u   Máscaras: %u"), logros_page + 1, logros_pages(), logros_kills(), logros_masks());
  ui_text_center(UI_H - 26, t, 0x807870);
}

static void draw_viewer_ui(void);
static int viewer_active(void);

static void draw_br_hud(void);
static void draw_br_over(void);
static int br_over, br_transit;
static void draw_ui(void) {
  ui_clear();
  if (game_mode == GM_BOSSRUSH && !in_launcher && !viewer_active() && !br_over) draw_br_hud();
  if (viewer_active()) draw_viewer_ui();
  else if (show_logros) draw_logros();
  else if (show_credits) draw_credits();
  else if (sv_over && !in_launcher && !menu_open) draw_sv_over();
  else if (br_over && !in_launcher && !menu_open) draw_br_over();
  else if (menu_open) {
    int n = page_count(menu_page);
    ui_rect(0, 0, UI_W, UI_H, 0xA0000000);
    int bw = 560, bh = 40 + n * 26 + 34;
    int bx = (UI_W - bw) / 2, by = (UI_H - bh) / 2;
    ui_rect(bx, by, bw, bh, 0xE0140C18);
    ui_rect(bx, by, bw, 2, 0xFFB0302A);
    ui_rect(bx, by + bh - 2, bw, 2, 0xFFB0302A);
    if (menu_page == PAGE_MAIN_BR) {
      char tt[64]; snprintf(tt, sizeof tt, "BOSS RUSH  ·  %s", diff_names[ES][br_diff()]);
      ui_text_center(by + 10, tt, 0xE8C060);
    } else if (menu_page == PAGE_MAIN_SURVIVOR) {
      char tt[64]; snprintf(tt, sizeof tt, "SURVIVOR  ·  %s", diff_names[ES][sv_diff]);
      ui_text_center(by + 10, tt, 0xE8C060);
    } else ui_text_center(by + 10, page_titles[ES][menu_page], 0xE8C060);
    for (int i = 0; i < n; i++) {
      char l[160]; menu_label(page_items[menu_page][i], l, sizeof l);
      int y = by + 40 + i * 26;
      if (i == menu_sel) ui_rect(bx + 10, y - 3, bw - 20, 24, 0xFF5A1A1A);
      ui_text(bx + 24, y, l, i == menu_sel ? 0xFFFFFF : 0xB8B0A8);
    }
    const char* foot = "v" APP_VERSION "  ·  Recomp por Juanmote";
    if (menu_page == PAGE_CHEATS) {
      int it = cur_item();
      foot = it == MI_AMMO ? T("The subweapon never runs out", "La subarma no se gasta nunca") :
             it == MI_MULTI ? T("Throw as many as you like, not only two at a time", "Lanza todas las que quieras, no solo dos a la vez") :
             it == MI_LOCK ? T("Pickups no longer change your weapons", "Recoger objetos ya no cambia tus armas") :
             T("Official game cheats and weapon selector", "Trucos oficiales del juego y selector de armas");
    }
    if (menu_page == PAGE_CONTROLS) {
      int it = cur_item();
      foot = it == MI_TRIGGERS ? T("L2 rewinds up to 20 s, R2 fast forwards", "L2 rebobina hasta 20 s, R2 acelera") :
             it == MI_VIBRA ? T("Vibration strength for each moment of the game", "Fuerza de la vibración para cada momento del juego") :
             it == MI_TOUCH ? T("On screen: d-pad, 1 attack, 2 jump, dagger subweapon, START menu, PAUSE pause", "En pantalla: cruceta, 1 ataque, 2 salto, puñal subarma, START menú, PAUSE pausa") :
             T("SELECT or BACK opens the menu while playing", "SELECT o ATRÁS abre el menú durante la partida");
    }
    if (menu_page == PAGE_GAME) {
      int it = cur_item();
      foot = it == MI_DIFF ? T("Easy: you take half the damage and deal double", "Fácil: recibes la mitad, pegas el doble") :
             it == MI_RESPAWN ? (cfg.respawn == MOD_RESPAWN_OFF ? T("Enemies you kill stay dead for the whole round", "Los enemigos que matas no vuelven en toda la ronda") :
                                 cfg.respawn == MOD_RESPAWN_MIXED ? T("Enemies you kill come back once you have gone far away", "Los que matas vuelven cuando te has alejado") :
                                 T("Whether the enemies you kill come back", "Si los enemigos que matas vuelven a salir")) :
             it == MI_DJUMP ? T("Press jump again in the air", "Pulsa saltar otra vez en el aire") :
             it == MI_STAIRS ? T("Jump + up grabs a staircase, jump + down lets go", "Salto + arriba: te agarras. Salto + abajo: te sueltas") :
             it == MI_ORIGINAL ? T("Cheats off, original rules, suit and music", "Trucos fuera, reglas, traje y música originales") :
             it == MI_REMASTER ? T("No slowdowns, 16:9, scanlines, no subweapon limit", "Sin ralentizar, 16:9, scanlines, subarmas sin límite") : T("Game", "Juego");
    }
    if (menu_page == PAGE_VIBRA) {
      int it = cur_item();
      foot = it == MI_VIB0 ? T("When you press the on-screen buttons", "Al pulsar los botones de la pantalla") :
             it == MI_VIB0 + 1 ? T("Every swing of the weapon", "Cada golpe del arma") :
             it == MI_VIB0 + 2 ? T("Hitting and destroying enemies", "Al dar y destruir enemigos") :
             it == MI_VIB0 + 3 ? T("Damage and lives lost", "Daño recibido y vidas perdidas") :
             it == MI_VIB0 + 4 ? T("Touching the ground after a jump or a fall", "Al tocar suelo tras un salto o una caída") :
             it == MI_VIB0 + 5 ? T("Boss appears, is hit and is defeated", "Aparece el jefe, le aciertas y lo derrotas") :
             T("A gamepad with rumble vibrates instead of the phone", "Si el mando vibra, vibra el mando en vez del móvil");
    }
    if (menu_page == PAGE_MODES) {
      int it = cur_item();
      foot = it == MI_MODE_NORMAL ? T("Every option and cheat. Achievements do not count", "Con todas las opciones y trucos. Los logros no cuentan") :
             it == MI_MODE_LOGROS ? T("No rewind or cheats. Achievements count here", "Sin rebobinado ni trucos. Aquí cuentan los logros") :
             it == MI_MODE_SURVIVOR ? T("The whole game with a single life. Achievements count", "Todo el juego con una sola vida. Cuentan los logros") :
             it == MI_MODE_BOSSRUSH ? T("The five bosses in a row, against the clock", "Los cinco jefes seguidos, contra el reloj") :
             it == MI_LOGROS_LIST ? T("See the ones you have and the ones left", "Mira los que tienes y los que te faltan") :
             it == MI_PROFILE_EXPORT ? T("Saves your achievements to a file", "Guarda tus logros en un archivo") :
             it == MI_PROFILE_IMPORT ? T("Adds a saved profile to this one", "Añade un perfil guardado a este") :
             it == MI_STATE_EXPORT ? T("Copies the normal game's saved state to a file", "Copia a un archivo el estado guardado de la partida normal") :
             T("Back to the start screen", "Vuelve a la pantalla de inicio");
    }
    static char br_foot[96];
    if (menu_page == PAGE_BOSSRUSH) {
      int it = cur_item();
      if (it == MI_BR_LIFE) foot = br_life == 0 ? T("Full energy again after each boss", "Tras cada jefe recuperas toda la energía") :
                                   br_life == 1 ? T("Half a bar back after each boss", "Tras cada jefe recuperas media barra") :
                                   T("Energy never comes back", "La energía no se recupera nunca");
      else if (it == MI_BR_ARMS) foot = br_arms == 0 ? T("Rapier or axe, bombs or stakes", "Estoque o hacha, bombas o estacas") :
                                        br_arms == 1 ? T("Sword or rapier, pistol or boomerang", "Espada o estoque, pistola o bumerán") :
                                        T("Knife, pistol or boomerang", "Cuchillo, pistola o bumerán");
      else if ((it == MI_BR_START || it == MI_BR_WEAPON || it == MI_BR_SUB) && !br_have_all()) {
        snprintf(br_foot, sizeof br_foot, T("Bosses unlocked: %d/5 (reach them in any game)", "Jefes desbloqueados: %d/5 (llega a ellos en cualquier partida)"), br_have_count());
        foot = br_foot;
      } else if (it == MI_BR_START || it == MI_BR_WEAPON || it == MI_BR_SUB) {
        unsigned b = logros_br_best(br_diff());
        char tm[32]; br_time_text(b, tm, sizeof tm);
        if (b) snprintf(br_foot, sizeof br_foot, T("Record on %s: %s", "Récord en %s: %s"), diff_names[ES][br_diff()], tm);
        else snprintf(br_foot, sizeof br_foot, T("No record on %s yet", "Aún sin récord en %s"), diff_names[ES][br_diff()]);
        foot = br_foot;
      } else foot = T("Back", "Volver");
    }
    if (menu_page == PAGE_SURVIVOR) {
      int it = cur_item();
      foot = it == MI_SV_EASY ? T("Energy comes back as in the normal game", "La energía se recupera como en el juego normal") :
             it == MI_SV_NORMAL ? T("Energy only comes back after each boss", "La energía solo se recupera tras cada jefe") :
             it == MI_SV_HARD ? T("Energy never comes back", "La energía no se recupera nunca") :
             T("Back", "Volver");
    }
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
  if (!tex_game || tex_game_w != game_w || tex_game_h != game_h) {
    if (tex_game) SDL_DestroyTexture(tex_game);
    tex_game = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, game_w, game_h);
    tex_game_w = game_w; tex_game_h = game_h;
    if (tex_scan) { SDL_DestroyTexture(tex_scan); tex_scan = NULL; }
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
    for (int i = 0; i < game_h * 4; i++) px[i] = (i % 4 == 3) ? 0x90000000 : (i % 4 == 2 ? 0x30000000 : 0x00000000);
    tex_scan = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, game_h * 4);
    SDL_UpdateTexture(tex_scan, NULL, px, 4);
    SDL_SetTextureBlendMode(tex_scan, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex_scan, SDL_ScaleModeLinear);
  }
}

// ---------------------------------------------------------------- CRT filter
// Done with the GPU: the picture is drawn 3 columns x k lines per game pixel, multiplied by a
// mask (beam profile of each scanline and red/green/blue phosphor stripes), then a blurred copy
// is added on top (the glow that also gives back the brightness the mask took). The curved
// version draws the result on a bulging grid with darker corners.
static SDL_Texture *tex_crt, *tex_mask, *tex_glow;
static int crt_w, crt_h, crt_k;

static void crt_textures(int gw, int k) {
  int w = gw * 3, h = game_h * k;
  if (tex_crt && crt_w == w && crt_h == h) return;
  if (tex_crt) SDL_DestroyTexture(tex_crt);
  if (tex_mask) SDL_DestroyTexture(tex_mask);
  if (tex_glow) SDL_DestroyTexture(tex_glow);
  tex_crt = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
  tex_glow = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, gw / 2, game_h / 2);
  tex_mask = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  uint32_t* px = malloc(sizeof(uint32_t) * w * h);
  for (int y = 0; y < h; y++) {
    // beam profile across the k lines of one scanline: bright in the middle, dark between lines
    double t = ((y % k) + 0.5) / k - 0.5;
    double beam = 0.38 + 0.62 * exp(-t * t / (2 * 0.2 * 0.2));
    for (int x = 0; x < w; x++) {
      double m[3] = {0.78, 0.78, 0.78};
      m[x % 3] = 1.0;   // phosphor stripe
      uint32_t c = 0xFF000000u;
      for (int ch = 0; ch < 3; ch++) c |= (uint32_t)clamp255(255 * beam * m[ch]) << (16 - 8 * ch);
      px[y * w + x] = c;
    }
  }
  SDL_UpdateTexture(tex_mask, NULL, px, w * 4);
  free(px);
  SDL_SetTextureBlendMode(tex_mask, SDL_BLENDMODE_MOD);
  SDL_SetTextureScaleMode(tex_crt, SDL_ScaleModeLinear);
  SDL_SetTextureScaleMode(tex_glow, SDL_ScaleModeLinear);
  crt_w = w; crt_h = h; crt_k = k;
}

static void draw_crt(SDL_Rect dst, int gw, int curved) {
  int k = dst.h / game_h;
  if (k < 3) k = 3;
  if (k > 6) k = 6;
  crt_textures(gw, k);
  if (!tex_crt || !tex_mask || !tex_glow) { SDL_RenderCopy(ren, tex_game, NULL, &dst); return; }
  // glow source: the picture at half size, smoothed
  SDL_SetTextureScaleMode(tex_game, SDL_ScaleModeLinear);
  SDL_SetRenderTarget(ren, tex_glow);
  SDL_SetTextureBlendMode(tex_game, SDL_BLENDMODE_NONE);
  SDL_RenderCopy(ren, tex_game, NULL, NULL);
  // the picture with the mask, then the glow added
  SDL_SetRenderTarget(ren, tex_crt);
  SDL_SetTextureScaleMode(tex_game, SDL_ScaleModeNearest);
  SDL_RenderCopy(ren, tex_game, NULL, NULL);
  SDL_SetTextureBlendMode(tex_game, SDL_BLENDMODE_BLEND);
  SDL_RenderCopy(ren, tex_mask, NULL, NULL);
  SDL_SetTextureBlendMode(tex_glow, SDL_BLENDMODE_ADD);
  SDL_SetTextureAlphaMod(tex_glow, 72);
  SDL_RenderCopy(ren, tex_glow, NULL, NULL);
  SDL_SetTextureAlphaMod(tex_glow, 255);
  SDL_SetRenderTarget(ren, NULL);
  if (!curved) { SDL_RenderCopy(ren, tex_crt, NULL, &dst); return; }
  // bulging screen: a grid whose edges bow outwards, corners pulled in and darker
  enum { GX = 32, GY = 24 };
  static SDL_Vertex v[(GX + 1) * (GY + 1)];
  static int idx[GX * GY * 6];
  const double bend = 0.045;
  for (int j = 0; j <= GY; j++)
    for (int i = 0; i <= GX; i++) {
      double u = (double)i / GX, w = (double)j / GY;
      double x = u * 2 - 1, y = w * 2 - 1;
      double xs = x * (1 - bend * y * y), ys = y * (1 - bend * x * x);
      double r2 = x * x * y * y;   // darker towards the corners
      int lum = clamp255(255 * (1 - 0.35 * r2 - 0.06 * (x * x + y * y)));
      SDL_Vertex* p = &v[j * (GX + 1) + i];
      p->position.x = (float)(dst.x + (xs + 1) * 0.5 * dst.w);
      p->position.y = (float)(dst.y + (ys + 1) * 0.5 * dst.h);
      p->color = (SDL_Color){(Uint8)lum, (Uint8)lum, (Uint8)lum, 255};
      p->tex_coord.x = (float)u; p->tex_coord.y = (float)w;
    }
  int n = 0;
  for (int j = 0; j < GY; j++)
    for (int i = 0; i < GX; i++) {
      int a = j * (GX + 1) + i, b = a + 1, c = a + GX + 1, d = c + 1;
      idx[n++] = a; idx[n++] = b; idx[n++] = c; idx[n++] = b; idx[n++] = d; idx[n++] = c;
    }
  SDL_RenderGeometry(ren, tex_crt, v, (GX + 1) * (GY + 1), idx, n);
}

// "Ambient" bands: the picture shrunk to a few blocks of average colour, spread over the whole screen and darkened,
// so the space the game cannot fill without changing its shape takes the colours of the scene
#define AMB_W 16
#define AMB_H 12
static SDL_Texture* tex_amb;
static void draw_ambient(int gw, int ww, int wh) {
  if (!tex_amb) {
    tex_amb = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, AMB_W, AMB_H);
    if (!tex_amb) return;
    SDL_SetTextureScaleMode(tex_amb, SDL_ScaleModeLinear);
  }
  static uint32_t px[AMB_W * AMB_H];
  for (int by = 0; by < AMB_H; by++)
    for (int bx = 0; bx < AMB_W; bx++) {
      int x0 = bx * gw / AMB_W, x1 = (bx + 1) * gw / AMB_W, y0 = by * game_h / AMB_H, y1 = (by + 1) * game_h / AMB_H;
      unsigned r = 0, g = 0, b = 0, n = 0;
      for (int y = y0; y < y1; y += 2)
        for (int x = x0; x < x1; x += 2) { uint32_t c = pix[y * gw + x]; r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255; n++; }
      if (!n) n = 1;
      px[by * AMB_W + bx] = 0xFF000000u | ((r / n) << 16) | ((g / n) << 8) | (b / n);
    }
  SDL_UpdateTexture(tex_amb, NULL, px, AMB_W * 4);
  SDL_SetTextureColorMod(tex_amb, 110, 110, 110);
  SDL_Rect all = {0, 0, ww, wh};
  SDL_RenderCopy(ren, tex_amb, NULL, &all);
}


// ---------------------------------------------------------------- Game Gear format
// Like the Game Gear version: a smaller piece of the picture that follows the hero (it looks bigger, enemies come
// in suddenly) and, instead of the status bar, one line with energy, subweapon ammo and time (the pause shows the
// whole bar). The piece takes the full height of the stage (160 lines), so the floor is always in sight, and
// 182 columns, 8:7. It only moves sideways, when the hero leaves a margin in its middle: no shaking while the
// game itself scrolls. Title, story and other screens are shown whole.
#define GG_W 182
#define GG_H 160
#define HUD_H 32                 // status bar lines at the top of the picture
#define GG_MARGIN 62             // the hero is kept between these columns of the piece and GG_W - GG_MARGIN
static uint32_t gg_full[SMS_W * SMS_H];
static int gg_x = -1;
static int gg_on;                // the piece is being shown (in a stage)

static void gg_window(void) {
  int hx = (int16_t)(gram(0xCA24) | gram(0xCA25) << 8) + 8;
  if (gg_x < 0) gg_x = hx - GG_W / 2;
  if (hx < gg_x + GG_MARGIN) gg_x = hx - GG_MARGIN;
  if (hx > gg_x + GG_W - GG_MARGIN) gg_x = hx - (GG_W - GG_MARGIN);
  if (gg_x < 0) gg_x = 0;
  if (gg_x > SMS_W - GG_W) gg_x = SMS_W - GG_W;
}

// a piece of the status bar (x, y, width; 8 lines) to (dx, 1) of the piece
static void gg_hud_part(int x, int y, int w, int dx) {
  for (int r = 0; r < 8; r++) memcpy(pix + (r + 1) * GG_W + dx, gg_full + (y + r) * SMS_W + x, (size_t)w * 4);
}

static void gg_crop(void) {
  if (mod_is_gameplay(&S) || gg_x < 0) gg_window();
  for (int y = 0; y < GG_H; y++) memcpy(pix + y * GG_W, gg_full + (y + HUD_H) * SMS_W + gg_x, GG_W * 4);
  // the Game Gear line: energy (label and dots), subweapon ammo and time, on a black band
  for (int i = 0; i < 10 * GG_W; i++) pix[i] = 0xFF000000u;
  gg_hud_part(40, 16, 96, 3);                   // LIFE / VIDA and the energy dots
  gg_hud_part(176, 16, 24, 3 + 96 + 10);        // =NN, subweapon ammo
  gg_hud_part(224, 16, 24, GG_W - 24 - 3);      // time
}

// during the pause, the status bar over the top of the piece (as Start does on the Game Gear)
static void gg_draw_hud(SDL_Rect dst) {
  if (!tex_hud) tex_hud = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SMS_W, HUD_H);
  if (!tex_hud) return;
  SDL_UpdateTexture(tex_hud, NULL, gg_full, SMS_W * 4);
  SDL_SetTextureScaleMode(tex_hud, SDL_ScaleModeNearest);
  SDL_Rect r = {dst.x, dst.y, dst.w, dst.w * HUD_H / SMS_W};   // over the Game Gear line
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
  SDL_RenderFillRect(ren, &r);
  SDL_RenderCopy(ren, tex_hud, NULL, &r);
}

static SDL_Rect dest_rect(int ww, int wh, int gw) {
  SDL_Rect r;
  // margin: the picture keeps away from the screen edges (phones with rounded corners or a notch)
  int mx = 0, my = 0;
  if (cfg.margin) { my = wh * (cfg.margin == 1 ? 3 : 6) / 100; mx = ww * (cfg.margin == 1 ? 2 : 4) / 100; }
  ww -= 2 * mx; wh -= 2 * my;
  // pixel aspect ratio: CRT 8:7 pixels; Game Gear, 160x144 shown as 8:7
  double par = (!cfg.widescreen && cfg.crt) ? 8.0 / 7.0 : gw == GG_W ? (8.0 / 7.0) / ((double)GG_W / GG_H) : 1.0;
  double sx = (double)ww / (gw * par), sy = (double)wh / game_h;
  double s = sx < sy ? sx : sy;
  if (cfg.scale == SCALE_STRETCH) { r.x = mx; r.y = my; r.w = ww; r.h = wh; return r; }
  if (cfg.scale == SCALE_INT && s >= 1.0) s = floor(s);
  r.w = (int)(gw * par * s + 0.5); r.h = (int)(game_h * s + 0.5);
  r.x = mx + (ww - r.w) / 2; r.y = my + (wh - r.h) / 2;
  return r;
}

// ---------------------------------------------------------------- start screen and touch controls
enum { ART_BG, ART_PLAY, ART_OPTIONS, ART_CONTROLS, ART_CREDITS, ART_QUIT, ART_DPAD, ART_B1, ART_B2, ART_START, ART_PAUSE,
       ART_B3, ART_B3_SUB, ART_EN_PLAY = ART_B3_SUB + 4, ART_GALLERY = ART_EN_PLAY + 5, ART_MANUAL, ART_EN_GALLERY, ART_EN_MANUAL, ART_COUNT };
static const char* art_files[ART_COUNT] = {"fondo.jpg", "jugar.png", "opciones.png", "controles.png", "creditos.png",
                                           "salir.png", "dpad.png", "b1.png", "b2.png", "start.png", "pause.png", "b3.png",
                                           "b3_pistola.png", "b3_bumeran.png", "b3_estacas.png", "b3_bombas.png",
                                           "en_jugar.png", "en_opciones.png", "en_controles.png", "en_creditos.png", "en_salir.png",
                                           "galeria.png", "manual.png", "en_galeria.png", "en_manual.png"};
static SDL_Texture* art[ART_COUNT];
static int art_w[ART_COUNT], art_h[ART_COUNT];
static int art_ok;

static uint8_t* read_asset(const char* name, size_t* n) {
  char p[1200];
  const char* env = getenv("MOD_ASSETS");
#ifdef __ANDROID__
  snprintf(p, sizeof p, "launcher/%s", name);   // inside the APK (assets/launcher)
  uint8_t* d = load_file(p, n);
  if (d) return d;
#endif
  if (env) { snprintf(p, sizeof p, "%s/%s", env, name); uint8_t* d2 = load_file(p, n); if (d2) return d2; }
  char* base = SDL_GetBasePath();
  if (base) {
    snprintf(p, sizeof p, "%sassets/launcher/%s", base, name);
    SDL_free(base);
    return load_file(p, n);
  }
  return NULL;
}

static void load_art(void) {
  art_ok = 1;
  for (int i = 0; i < ART_COUNT; i++) {
    size_t n = 0;
    uint8_t* d = read_asset(art_files[i], &n);
    int w, h, c;
    uint8_t* px = d ? stbi_load_from_memory(d, (int)n, &w, &h, &c, 4) : NULL;
    free(d);
    if (!px) { if (i < ART_B3) art_ok = 0; continue; }   // the subweapon button and English art are optional
    art[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (art[i]) {
      SDL_UpdateTexture(art[i], NULL, px, w * 4);
      SDL_SetTextureBlendMode(art[i], SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(art[i], SDL_ScaleModeLinear);
    } else art_ok = 0;
    art_w[i] = w; art_h[i] = h;
    stbi_image_free(px);
  }
}

// start screen buttons, left to right
#define LAUNCH_N 7
#define LAUNCH_QUIT (LAUNCH_N - 1)
static const int launch_art[LAUNCH_N] = {ART_OPTIONS, ART_CONTROLS, ART_GALLERY, ART_PLAY, ART_MANUAL, ART_CREDITS, ART_QUIT};
static int launch_sel = 3;
static SDL_Rect launch_rect[LAUNCH_N];

// texture of a launcher button in the menu language (the English art is optional)
static int lart(int a) {
  if (!ES && a >= ART_PLAY && a <= ART_QUIT && art[ART_EN_PLAY + a - ART_PLAY]) return ART_EN_PLAY + a - ART_PLAY;
  if (!ES && (a == ART_GALLERY || a == ART_MANUAL) && art[a + ART_EN_GALLERY - ART_GALLERY]) return a + ART_EN_GALLERY - ART_GALLERY;
  return a;
}

static void layout_launcher(int ww, int wh) {
  // widths relative to a 1920-wide screen, in a row at the bottom
  static const int base_w[LAUNCH_N] = {250, 236, 236, 340, 236, 236, 250};
  double k = ww / 1920.0 < wh / 1080.0 ? ww / 1920.0 : wh / 1080.0;
  int total = 0;
  for (int i = 0; i < LAUNCH_N; i++) total += (int)(base_w[i] * k);
  int gap = (int)(14 * k), x = (ww - total - (LAUNCH_N - 1) * gap) / 2, cy = wh - (int)(110 * k);
  for (int i = 0; i < LAUNCH_N; i++) {
    int a = lart(launch_art[i]), w = (int)(base_w[i] * k), h = art_w[a] ? w * art_h[a] / art_w[a] : 0;
    launch_rect[i] = (SDL_Rect){x, cy - h / 2 - (launch_art[i] == ART_PLAY ? (int)(20 * k) : 0), w, h};
    x += w + gap;
  }
}

static void draw_art(int a, SDL_Rect r, int highlight, int dim) {
  if (!art[a]) return;
  if (highlight) {   // a little bigger, with a glow
    int dx = r.w / 20, dy = r.h / 20;
    r.x -= dx; r.y -= dy; r.w += 2 * dx; r.h += 2 * dy;
  }
  SDL_SetTextureColorMod(art[a], dim ? 170 : 255, dim ? 170 : 255, dim ? 170 : 255);
  SDL_SetTextureBlendMode(art[a], SDL_BLENDMODE_BLEND);
  SDL_RenderCopy(ren, art[a], NULL, &r);
  if (highlight) {
    SDL_SetTextureBlendMode(art[a], SDL_BLENDMODE_ADD);
    SDL_SetTextureAlphaMod(art[a], 70);
    SDL_RenderCopy(ren, art[a], NULL, &r);
    SDL_SetTextureAlphaMod(art[a], 255);
    SDL_SetTextureBlendMode(art[a], SDL_BLENDMODE_BLEND);
  }
}

static void draw_launcher(int ww, int wh) {
  // background covers the screen keeping its proportions
  SDL_Rect bg;
  double k = (double)ww / art_w[ART_BG] > (double)wh / art_h[ART_BG] ? (double)ww / art_w[ART_BG] : (double)wh / art_h[ART_BG];
  bg.w = (int)(art_w[ART_BG] * k); bg.h = (int)(art_h[ART_BG] * k);
  // on screens wider than the picture only its bottom is cut, so the logo stays whole
  bg.x = (ww - bg.w) / 2; bg.y = bg.h > wh ? 0 : (wh - bg.h) / 2;
  SDL_RenderCopy(ren, art[ART_BG], NULL, &bg);
  layout_launcher(ww, wh);
  for (int i = 0; i < LAUNCH_N; i++)
    if (i != launch_sel) draw_art(lart(launch_art[i]), launch_rect[i], 0, !menu_open && !show_credits);
  draw_art(lart(launch_art[launch_sel]), launch_rect[launch_sel], !menu_open && !show_credits, 0);
}

// ---------------------------------------------------------------- image viewer: gallery, manual, ticket
// The gallery and the manual are optional packs (.zip of .jpg/.png, like the music) chosen once with the file
// picker and copied inside the app. The name of each file, without a leading number, is its caption.
// The ticket is one picture inside the APK.
enum { PACK_GALLERY = 1, PACK_MANUAL = 2, VIEW_TICKET = 3 };
#define VIEW_MAX 256
static struct {
  int kind;            // what is open: PACK_*, VIEW_TICKET or 0
  int prompt;          // no pack yet: asks for it (PACK_*)
  int waiting;         // the file picker is open for this PACK_*
  mz_zip_archive zip; int zip_open;
  int n, entry[VIEW_MAX]; char name[VIEW_MAX][80];
  int cur;
  SDL_Texture* tex; int tw, th;
  double zoom, cx, cy; // zoom over "fits the screen"; picture point at the centre (0..1)
  Uint32 hud_until; int hud_seen;
  // touch
  int fingers; float dx, dy; float down_x, down_y; int moved; Uint32 last_tap;
  int stick_dir;
} viewer;

static int viewer_active(void) { return viewer.kind || viewer.prompt; }
static void viewer_hud(void) { viewer.hud_until = SDL_GetTicks() + 2000; }   // the buttons go away after 2 s

static void pack_path(int kind, char* out, size_t n) {
  const char* f = kind == PACK_MANUAL ? "manual.zip" : "galeria.zip";
#ifdef __ANDROID__
  snprintf(out, n, "%s/%s", SDL_AndroidGetInternalStoragePath(), f);
#else
  cfg_path(out, n, f);
#endif
}

static int ui_scale_xy(float x, float y, int ww, int wh, float* ux, float* uy);
// Where the UI layer starts on screen: centred, except over a picture of the gallery or manual, where it
// sits on the bottom edge (the viewer's buttons go at the very bottom of any screen shape).
static double ui_top(int wh, double uh) { return viewer.kind && !viewer.prompt ? wh - uh : (wh - uh) / 2; }

static void viewer_set_picture(uint8_t* data, size_t size) {
  if (viewer.tex) { SDL_DestroyTexture(viewer.tex); viewer.tex = NULL; }
  int w = 0, h = 0, c;
  uint8_t* px = data ? stbi_load_from_memory(data, (int)size, &w, &h, &c, 4) : NULL;
  if (!px) return;
  // a picture larger than the GPU allows is halved until it fits
  SDL_RendererInfo ri;
  int maxs = SDL_GetRendererInfo(ren, &ri) == 0 && ri.max_texture_width > 0 ? ri.max_texture_width : 4096;
  while (w > maxs || h > maxs) {
    int nw = w / 2, nh = h / 2;
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) memcpy(px + (y * nw + x) * 4, px + (y * 2 * w + x * 2) * 4, 4);
    w = nw; h = nh;
  }
  viewer.tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (viewer.tex) {
    SDL_UpdateTexture(viewer.tex, NULL, px, w * 4);
    SDL_SetTextureBlendMode(viewer.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(viewer.tex, SDL_ScaleModeLinear);
  }
  viewer.tw = w; viewer.th = h;
  stbi_image_free(px);
}

static void viewer_show(int i) {
  if (!viewer.n) return;
  viewer.cur = (i + viewer.n) % viewer.n;
  viewer.zoom = 1; viewer.cx = viewer.cy = 0.5;
  size_t size = 0;
  void* d = mz_zip_reader_extract_to_heap(&viewer.zip, (mz_uint)viewer.entry[viewer.cur], &size, 0);
  viewer_set_picture(d, size);
  if (d) mz_free(d);
  viewer_hud();
}

static void viewer_close_zip(void) {
  if (viewer.zip_open) mz_zip_reader_end(&viewer.zip);
  viewer.zip_open = 0;
  viewer.n = 0;
}

static void viewer_close(void) {
  viewer_close_zip();
  if (viewer.tex) SDL_DestroyTexture(viewer.tex);
  viewer.tex = NULL;
  viewer.kind = viewer.prompt = 0;
  viewer.fingers = 0;
}

static int viewer_name_cmp(const void* a, const void* b) {
  const int* x = a; const int* y = b;
  return strcmp(viewer.name[*x], viewer.name[*y]);
}

// opens a pack; false if the file is missing or holds no pictures
static int viewer_load_pack(int kind) {
  viewer_close_zip();
  char p[1100];
  pack_path(kind, p, sizeof p);
  memset(&viewer.zip, 0, sizeof viewer.zip);
  if (!mz_zip_reader_init_file(&viewer.zip, p, 0)) return 0;
  viewer.zip_open = 1;
  int total = (int)mz_zip_reader_get_num_files(&viewer.zip), order[VIEW_MAX], n = 0;
  for (int i = 0; i < total && n < VIEW_MAX; i++) {
    char nm[512];
    if (mz_zip_reader_is_file_a_directory(&viewer.zip, i)) continue;
    mz_zip_reader_get_filename(&viewer.zip, i, nm, sizeof nm);
    const char* base = strrchr(nm, '/');
    base = base ? base + 1 : nm;
    const char* dot = strrchr(base, '.');
    if (!dot || base[0] == '.') continue;
    if (SDL_strcasecmp(dot, ".jpg") && SDL_strcasecmp(dot, ".jpeg") && SDL_strcasecmp(dot, ".png")) continue;
    snprintf(viewer.name[n], sizeof viewer.name[n], "%.*s", (int)(dot - base), base);
    viewer.entry[n] = i;
    order[n] = n;
    n++;
  }
  // in the order of their names
  qsort(order, (size_t)n, sizeof order[0], viewer_name_cmp);
  static int entry2[VIEW_MAX]; static char name2[VIEW_MAX][80];
  for (int i = 0; i < n; i++) { entry2[i] = viewer.entry[order[i]]; memcpy(name2[i], viewer.name[order[i]], 80); }
  memcpy(viewer.entry, entry2, sizeof(int) * (size_t)n);
  memcpy(viewer.name, name2, 80 * (size_t)n);
  viewer.n = n;
  if (!n) viewer_close_zip();
  return n > 0;
}

static void viewer_open_pack(int kind) {
  viewer.prompt = 0;
  if (viewer_load_pack(kind)) { viewer.kind = kind; viewer_show(0); }
  else { viewer.kind = 0; viewer.prompt = kind; }
}

static void viewer_open_ticket(void) {
  viewer_close_zip();
  size_t size = 0;
  uint8_t* d = read_asset("ticket.jpg", &size);
  viewer_set_picture(d, size);
  free(d);
  if (!viewer.tex) return;
  viewer.kind = VIEW_TICKET; viewer.prompt = 0;
  viewer.zoom = 1; viewer.cx = viewer.cy = 0.5;
  viewer_hud();
}

static void credits_tap(void) {
  Uint32 now = SDL_GetTicks();
  if (now - credits_tap_time > 2000) credits_taps = 0;
  credits_tap_time = now;
  if (++credits_taps >= 3) { credits_taps = 0; viewer_open_ticket(); }
}

static void viewer_pick_file(int kind) {
#ifdef __ANDROID__
  viewer.waiting = kind;
  SDL_AndroidSendMessage(0x8007, kind);
#else
  (void)kind;
  show_toast(T("Copy the pack as galeria.zip or manual.zip into the settings folder", "Copia el pack como galeria.zip o manual.zip en la carpeta de ajustes"), 3000);
#endif
}

// the file picker has closed
static void viewer_back_from_picker(void) {
  if (!viewer.waiting) return;
  int kind = viewer.waiting, had = viewer.kind;
  viewer.waiting = 0;
  viewer.fingers = 0;
  if (viewer_load_pack(kind)) { viewer.prompt = 0; viewer.kind = kind; viewer_show(0); }
  else if (had) { viewer_open_pack(had); }   // cancelled, or not a pack: what was open goes on
  else show_toast(T("That file has no pictures", "Ese archivo no tiene imágenes"), 2500);
}

static void viewer_zoom_step(void) {
  viewer.zoom = viewer.zoom < 1.4 ? 1.6 : viewer.zoom < 2.2 ? 2.4 : 1;
  if (viewer.zoom == 1) viewer.cx = viewer.cy = 0.5;
}

// picture rectangle for this window; keeps the view inside the picture
static SDL_Rect viewer_rect(int ww, int wh) {
  SDL_Rect r = {0, 0, 0, 0};
  if (!viewer.tw || !viewer.th) return r;
  double s = (double)ww / viewer.tw < (double)wh / viewer.th ? (double)ww / viewer.tw : (double)wh / viewer.th;
  s *= viewer.zoom;
  r.w = (int)(viewer.tw * s + 0.5); r.h = (int)(viewer.th * s + 0.5);
  double hx = (double)ww / r.w / 2, hy = (double)wh / r.h / 2;
  viewer.cx = hx >= 0.5 ? 0.5 : viewer.cx < hx ? hx : viewer.cx > 1 - hx ? 1 - hx : viewer.cx;
  viewer.cy = hy >= 0.5 ? 0.5 : viewer.cy < hy ? hy : viewer.cy > 1 - hy ? 1 - hy : viewer.cy;
  r.x = (int)(ww / 2.0 - viewer.cx * r.w); r.y = (int)(wh / 2.0 - viewer.cy * r.h);
  return r;
}

static void draw_viewer(int ww, int wh) {
  SDL_SetRenderDrawColor(ren, 12, 8, 14, 255);
  SDL_RenderClear(ren);
  if (!viewer.tex) return;
  SDL_Rect r = viewer_rect(ww, wh);
  SDL_RenderCopy(ren, viewer.tex, NULL, &r);
}

static void draw_viewer_ui(void) {
  if (viewer.prompt) {
    ui_rect(0, 0, UI_W, UI_H, 0xC8000000);
    int by = UI_H / 2 - 80;
    ui_rect(UI_W / 2 - 280, by, 560, 170, 0xF0140C18);
    ui_text_center(by + 14, viewer.prompt == PACK_MANUAL ? T("MANUAL", "MANUAL") : T("GALLERY", "GALERÍA"), 0xE8C060);
    ui_text_center(by + 46, T("No pack installed. Choose its .zip file:", "No hay ningún pack instalado. Elige su archivo .zip:"), 0xFFFFFF);
    ui_text_center(by + 70, T("it is copied inside the app, only once.", "se copia dentro de la app, solo una vez."), 0xB8B0A8);
    ui_rect(UI_W / 2 - 250, by + 106, 240, 40, 0xFF5A1A1A);
    ui_rect(UI_W / 2 + 10, by + 106, 240, 40, 0xFF2A2430);
    ui_text(UI_W / 2 - 220, by + 115, T("A: Choose file", "A: Elegir archivo"), 0xFFFFFF);
    ui_text(UI_W / 2 + 70, by + 115, T("B: Back", "B: Volver"), 0xFFFFFF);
    return;
  }
  if ((Sint32)(SDL_GetTicks() - viewer.hud_until) >= 0) return;
  char t[160], cap[80] = "";
  if (viewer.kind == VIEW_TICKET) snprintf(t, sizeof t, "%s", T("Bought on 12 November 1994", "Comprado el 12 de noviembre de 1994"));
  else {
    const char* s = viewer.name[viewer.cur];
    while (*s >= '0' && *s <= '9') s++;     // the leading number only sets the order
    while (*s == ' ' || *s == '-' || *s == '_') s++;
    snprintf(cap, sizeof cap, "%s", *s ? s : viewer.name[viewer.cur]);
    snprintf(t, sizeof t, "%s   ‹ %d/%d ›", cap, viewer.cur + 1, viewer.n);
  }
  // everything on the bottom edge, so the middle of the picture stays clean
  int w = text_width(t) + 24;
  ui_rect((UI_W - w) / 2, UI_H - 30, w, 24, 0xC0000000);
  ui_text_center(UI_H - 28, t, 0xFFFFFF);
  const char* close = T("X  Close", "X  Cerrar");
  ui_rect(UI_W - text_width(close) - 24, UI_H - 30, text_width(close) + 18, 24, 0xC0000000);
  ui_text(UI_W - text_width(close) - 15, UI_H - 28, close, 0xFFFFFF);
  if (viewer.kind != VIEW_TICKET) {
    const char* ch = T("Change pack", "Cambiar pack");
    ui_rect(6, UI_H - 30, text_width(ch) + 18, 24, 0xC0000000);
    ui_text(15, UI_H - 28, ch, 0xB8B0A8);
  }
}

// held directions move a zoomed picture
static void viewer_tick(void) {
  if (!viewer.kind || viewer.zoom <= 1.01) return;
  double dx = 0, dy = 0;
  const Uint8* k = SDL_GetKeyboardState(NULL);
  dx += k[SDL_SCANCODE_RIGHT] - k[SDL_SCANCODE_LEFT];
  dy += k[SDL_SCANCODE_DOWN] - k[SDL_SCANCODE_UP];
  for (int i = 0; i < 8; i++) {
    SDL_GameController* c = pads[i];
    if (!c) continue;
    dx += SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) - SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    dy += SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN) - SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP);
    int ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (ax > 8000 || ax < -8000) dx += ax / 32768.0;
    if (ay > 8000 || ay < -8000) dy += ay / 32768.0;
  }
  viewer.cx += dx * 0.012 / viewer.zoom;
  viewer.cy += dy * 0.012 / viewer.zoom;
}

static void viewer_touch(SDL_Event* e, int ww, int wh) {
  float x = e->tfinger.x * ww, y = e->tfinger.y * wh;
  if (e->type == SDL_FINGERDOWN) {
    viewer.fingers++;
    viewer.hud_seen = (Sint32)(SDL_GetTicks() - viewer.hud_until) < 0;   // the buttons only work while shown
    viewer_hud();
    if (viewer.fingers == 1) { viewer.down_x = x; viewer.down_y = y; viewer.moved = 0; }
    else viewer.moved = 1;
    return;
  }
  if (e->type == SDL_FINGERMOTION) {
    float fx = e->tfinger.dx * ww, fy = e->tfinger.dy * wh;
    if (viewer.fingers != 1) return;
    if (fabsf(x - viewer.down_x) > ww / 40.0f || fabsf(y - viewer.down_y) > wh / 40.0f) viewer.moved = 1;
    if (viewer.zoom > 1.01) {
      SDL_Rect r = viewer_rect(ww, wh);
      if (r.w) viewer.cx -= (double)fx / r.w;
      if (r.h) viewer.cy -= (double)fy / r.h;
    }
    return;
  }
  // finger up (the finger that opened the viewer does not count)
  if (viewer.fingers == 0) return;
  viewer.fingers--;
  if (viewer.fingers) return;
  float ddx = x - viewer.down_x;
  if (viewer.moved) {
    if (viewer.zoom <= 1.01 && fabsf(ddx) > ww / 8.0f && viewer.n) viewer_show(viewer.cur + (ddx < 0 ? 1 : -1));
    return;
  }
  float ux, uy;
  ui_scale_xy(x, y, ww, wh, &ux, &uy);
  if (viewer.hud_seen && uy > UI_H - 36 && ux > UI_W - 140) { viewer_close(); return; }
  if (viewer.hud_seen && uy > UI_H - 36 && ux < 160 && viewer.kind != VIEW_TICKET) { viewer_pick_file(viewer.kind); return; }
  Uint32 now = SDL_GetTicks();
  if (now - viewer.last_tap < 350) { viewer.last_tap = 0; viewer_zoom_step(); return; }
  viewer.last_tap = now;
  if (viewer.zoom <= 1.01 && viewer.n) {
    if (x < ww * 0.2f) viewer_show(viewer.cur - 1);
    else if (x > ww * 0.8f) viewer_show(viewer.cur + 1);
  }
}

// true when the event belongs to the viewer
static int viewer_event(SDL_Event* e) {
  int ww, wh;
  SDL_GetRendererOutputSize(ren, &ww, &wh);
  int prev = 0, next = 0, ok = 0, back = 0, other = 0, zoomed = viewer.zoom > 1.01;
  switch (e->type) {
  case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION:
    if (viewer.prompt) {
      if (e->type != SDL_FINGERDOWN) return 1;
      float ux, uy;
      ui_scale_xy(e->tfinger.x * ww, e->tfinger.y * wh, ww, wh, &ux, &uy);
      if (ux < UI_W / 2) ok = 1; else back = 1;
      break;
    }
    viewer_touch(e, ww, wh);
    return 1;
  case SDL_MULTIGESTURE:
    if (viewer.kind && e->mgesture.numFingers == 2) {
      viewer.zoom *= 1.0 + e->mgesture.dDist * 2.5;
      if (viewer.zoom < 1) viewer.zoom = 1;
      if (viewer.zoom > 4) viewer.zoom = 4;
    }
    return 1;
  case SDL_KEYDOWN: {
    if (e->key.repeat) return 1;
    SDL_Keycode k = e->key.keysym.sym;
    prev = k == SDLK_LEFT && !zoomed; next = k == SDLK_RIGHT && !zoomed;
    ok = k == SDLK_RETURN || k == SDLK_z || k == SDLK_SPACE;
    back = k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_x || k == SDLK_BACKSPACE;
    other = k == SDLK_c;
    break;
  }
  case SDL_KEYUP: return 1;
  case SDL_CONTROLLERBUTTONDOWN: {
    int b = e->cbutton.button;
    prev = (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT && !zoomed) || b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    next = (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT && !zoomed) || b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    ok = b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START;
    back = b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK;
    other = b == SDL_CONTROLLER_BUTTON_X;
    break;
  }
  case SDL_CONTROLLERBUTTONUP: return 1;
  case SDL_CONTROLLERAXISMOTION:
    if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX && !zoomed) {
      int d = e->caxis.value < -20000 ? -1 : e->caxis.value > 20000 ? 1 : 0;
      if (d && !viewer.stick_dir) { prev = d < 0; next = d > 0; }
      viewer.stick_dir = d;
      break;
    }
    return 1;
  default: return 0;
  }
  if (prev || next || ok || back || other) viewer_hud();
  if (viewer.prompt) {
    if (ok) viewer_pick_file(viewer.prompt);
    else if (back) viewer.prompt = 0;
    return 1;
  }
  if (back) viewer_close();
  else if (ok && viewer.kind) viewer_zoom_step();
  else if (other && viewer.kind != VIEW_TICKET) viewer_pick_file(viewer.kind);
  else if (viewer.n && prev) viewer_show(viewer.cur - 1);
  else if (viewer.n && next) viewer_show(viewer.cur + 1);
  return 1;
}

static void launcher_activate(int i) {
  switch (launch_art[i]) {
  case ART_PLAY: menu_open_at(PAGE_MODES); menu_sel = game_mode == GM_LOGROS ? 1 : game_mode == GM_SURVIVOR ? 2 : game_mode == GM_BOSSRUSH ? 3 : 0; break;
  case ART_OPTIONS: menu_open_at(PAGE_HOME); break;
  case ART_CONTROLS: menu_open_at(PAGE_CONTROLS); break;
  case ART_GALLERY: viewer_open_pack(PACK_GALLERY); break;
  case ART_MANUAL: viewer_open_pack(PACK_MANUAL); break;
  case ART_CREDITS: show_credits = 1; credits_taps = 0; break;
  case ART_QUIT: running = 0; break;
  }
}

// --- touch controls
enum { TZ_NONE, TZ_DPAD, TZ_B1, TZ_B2, TZ_START, TZ_PAUSE, TZ_B3, TZ_COUNT };
static struct { SDL_FingerID id; int zone; float x, y; int used; } fingers[10];
static SDL_Rect tz_rect[TZ_COUNT];
static uint8_t touch_input;
static int touch_seen;   // a finger has touched the screen (for "only without a gamepad")

static int any_pad(void) { for (int i = 0; i < 8; i++) if (pads[i]) return 1; return 0; }
static int touch_visible(void) {
  if (in_launcher || menu_open) return 0;
  return cfg.touch == 1 || (cfg.touch == 2 && !any_pad());
}

static void layout_touch(int ww, int wh) {
  double sz = cfg.touch_size == 0 ? 0.8 : cfg.touch_size == 2 ? 1.2 : 1.0;
  int m = wh / 30;
  int d = (int)(wh * 0.40 * sz), b = (int)(wh * 0.21 * sz);
  tz_rect[TZ_DPAD] = (SDL_Rect){m, wh - m - d, d, d};
  tz_rect[TZ_B2] = (SDL_Rect){ww - m - b, wh - m - b - b / 3, b, b};
  tz_rect[TZ_B1] = (SDL_Rect){ww - m - 2 * b - m / 2, wh - m - b + b / 8, b, b};
  // subweapon button, a little smaller, above the attack button
  int b3 = b * 4 / 5;
  tz_rect[TZ_B3] = (SDL_Rect){tz_rect[TZ_B1].x - b / 3, tz_rect[TZ_B1].y - b3 - m / 3, b3, b3};
  int sw = (int)(wh * 0.17 * sz), sh = art_w[ART_START] ? sw * art_h[ART_START] / art_w[ART_START] : sw / 3;
  tz_rect[TZ_START] = (SDL_Rect){ww / 2 + m / 2, wh - m - sh, sw, sh};
  tz_rect[TZ_PAUSE] = (SDL_Rect){ww / 2 - m / 2 - sw, wh - m - sh, sw, sh};
}

static int in_rect(SDL_Rect r, float x, float y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

static int touch_zone(float x, float y) {
  if (in_rect(tz_rect[TZ_DPAD], x, y)) return TZ_DPAD;
  for (int z = TZ_B1; z < TZ_COUNT; z++) {
    if (z == TZ_B3 && !art[ART_B3]) continue;
    SDL_Rect r = tz_rect[z];
    r.x -= r.w / 8; r.y -= r.h / 8; r.w += r.w / 4; r.h += r.h / 4;   // a bit of margin around
    if (in_rect(r, x, y)) return z;
  }
  return TZ_NONE;
}

static uint8_t dpad_dirs(float x, float y) {
  SDL_Rect r = tz_rect[TZ_DPAD];
  float dx = (x - (r.x + r.w / 2.0f)) / (r.w / 2.0f), dy = (y - (r.y + r.h / 2.0f)) / (r.h / 2.0f);
  uint8_t b = 0;
  if (dx * dx + dy * dy < 0.04f) return 0;
  float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  // 8 directions: diagonals when both axes are close
  if (ax > ay * 0.45f) b |= dx < 0 ? BTN_LEFT : BTN_RIGHT;
  if (ay > ax * 0.45f) b |= dy < 0 ? BTN_UP : BTN_DOWN;
  return b;
}

static void update_touch_input(void) {
  touch_input = 0;
  for (int i = 0; i < 10; i++) {
    if (!fingers[i].used) continue;
    switch (fingers[i].zone) {
    case TZ_DPAD: touch_input |= dpad_dirs(fingers[i].x, fingers[i].y); break;
    case TZ_B1: touch_input |= BTN_1; break;
    case TZ_B2: touch_input |= BTN_2; break;
    case TZ_PAUSE: touch_input |= BTN_PAUSE; break;
    case TZ_B3: touch_input |= BTN_UP | BTN_1; break;   // the game throws the subweapon with up + attack
    }
  }
}

static void draw_touch(int ww, int wh) {
  layout_touch(ww, wh);
  int held[TZ_COUNT] = {0};
  for (int i = 0; i < 10; i++) if (fingers[i].used) held[fingers[i].zone] = 1;
  static const int za[TZ_COUNT] = {0, ART_DPAD, ART_B1, ART_B2, ART_START, ART_PAUSE, ART_B3};
  for (int z = TZ_DPAD; z < TZ_COUNT; z++) {
    int a = za[z];
    if (!art[a]) continue;
    SDL_Rect r = tz_rect[z];
    int dim = 0;
    if (z == TZ_B3 && rom_crc == KNOWN_CRC) {
      // the subweapon being carried (its picture is drawn larger: the weapon sticks out of the ring);
      // dimmed when there is no ammunition
      int sa = ART_B3_SUB + (gram(RAM_SUBWEAPON) & 3);
      if (art[sa]) {
        a = sa;
        int g = r.w * 45 / 100;
        r.x -= g / 2; r.y -= g / 2; r.w += g; r.h += g;
      }
      dim = gram(RAM_AMMO) == 0;
    }
    SDL_SetTextureColorMod(art[a], dim ? 110 : 255, dim ? 110 : 255, dim ? 110 : 255);
    SDL_SetTextureAlphaMod(art[a], held[z] && z != TZ_DPAD ? 230 : dim ? 100 : 140);
    SDL_RenderCopy(ren, art[a], NULL, &r);
    SDL_SetTextureAlphaMod(art[a], 255);
    SDL_SetTextureColorMod(art[a], 255, 255, 255);
  }
  // the cross lights up only the arms being pressed
  uint8_t dirs = 0;
  for (int i = 0; i < 10; i++)
    if (fingers[i].used && fingers[i].zone == TZ_DPAD) dirs |= dpad_dirs(fingers[i].x, fingers[i].y);
  if (dirs && art[ART_DPAD]) {
    static const struct { uint8_t bit; float x0, y0, x1, y1; } arm[4] = {
      {BTN_UP, 0.33f, 0.00f, 0.67f, 0.42f}, {BTN_DOWN, 0.33f, 0.58f, 0.67f, 1.00f},
      {BTN_LEFT, 0.00f, 0.33f, 0.42f, 0.67f}, {BTN_RIGHT, 0.58f, 0.33f, 1.00f, 0.67f}};
    SDL_Rect r = tz_rect[TZ_DPAD];
    int tw = art_w[ART_DPAD], th = art_h[ART_DPAD];
    for (int k = 0; k < 4; k++) {
      if (!(dirs & arm[k].bit)) continue;
      SDL_Rect src = {(int)(arm[k].x0 * tw), (int)(arm[k].y0 * th), (int)((arm[k].x1 - arm[k].x0) * tw), (int)((arm[k].y1 - arm[k].y0) * th)};
      SDL_Rect dst = {r.x + (int)(arm[k].x0 * r.w), r.y + (int)(arm[k].y0 * r.h), (int)((arm[k].x1 - arm[k].x0) * r.w), (int)((arm[k].y1 - arm[k].y0) * r.h)};
      SDL_SetTextureAlphaMod(art[ART_DPAD], 255);
      SDL_RenderCopy(ren, art[ART_DPAD], &src, &dst);
      SDL_SetTextureBlendMode(art[ART_DPAD], SDL_BLENDMODE_ADD);
      SDL_SetTextureAlphaMod(art[ART_DPAD], 110);
      SDL_RenderCopy(ren, art[ART_DPAD], &src, &dst);
      SDL_SetTextureBlendMode(art[ART_DPAD], SDL_BLENDMODE_BLEND);
    }
    SDL_SetTextureAlphaMod(art[ART_DPAD], 255);
  }
}

// menu rows on the UI layer (for taps): returns the row under (x, y) in window pixels or -1
static int ui_scale_xy(float x, float y, int ww, int wh, float* ux, float* uy) {
  double us = (double)ww / UI_W < (double)wh / UI_H ? (double)ww / UI_W : (double)wh / UI_H;
  *ux = (float)((x - (ww - UI_W * us) / 2) / us);
  *uy = (float)((y - ui_top(wh, UI_H * us)) / us);
  return 1;
}

static int menu_row_at(float x, float y, int ww, int wh) {
  float ux, uy;
  ui_scale_xy(x, y, ww, wh, &ux, &uy);
  int n = page_count(menu_page), bh = 40 + n * 26 + 34, by = (UI_H - bh) / 2, bx = (UI_W - 560) / 2;
  if (ux < bx || ux > bx + 560) return -1;
  int row = (int)floorf((uy - (by + 37)) / 26.0f);
  return row >= 0 && row < n ? row : -1;
}

static void handle_touch(SDL_Event* e) {
  int ww, wh;
  SDL_GetRendererOutputSize(ren, &ww, &wh);
  float x = e->tfinger.x * ww, y = e->tfinger.y * wh;
  if (e->type == SDL_FINGERDOWN) {
    touch_seen = 1;
    if (show_credits) {
      float ux, uy;
      ui_scale_xy(x, y, ww, wh, &ux, &uy);
      if (uy >= CREDITS_NAME_Y - 8 && uy < CREDITS_NAME_Y + 26) credits_tap();
      else show_credits = 0;
      return;
    }
    if (menu_open) {
      int row = menu_row_at(x, y, ww, wh);
      if (row < 0) return;
      float ux, uy;
      ui_scale_xy(x, y, ww, wh, &ux, &uy);
      menu_sel = row;
      vibrate_touch();
      // value rows: left half goes back, right half forward; other rows activate
      int it = cur_item();
      int value_row = it == MI_FORMAT || it == MI_SCALE || it == MI_BANDS || it == MI_TESTINV || it == MI_BR_LIFE || it == MI_BR_ARMS ||
                      it == MI_BR_WEAPON || it == MI_BR_SUB || it == MI_FILTER || it == MI_SUIT || it == MI_MARGIN || it == MI_PALETTE || it == MI_VOLUME ||
                      it == MI_TURBO || it == MI_TRIGGERS || it == MI_DIFF || it == MI_RESPAWN || it == MI_DJUMP || it == MI_STAIRS ||
                      it == MI_TOUCH || it == MI_TOUCHSIZE || it == MI_INV || it == MI_LIVES || it == MI_WEAPON ||
                      it == MI_SUB || it == MI_START || it == MI_MUSTEST || it == MI_MUSIC || it == MI_AMMO || it == MI_LOCK || it == MI_UILANG || it == MI_MULTI ||
                      (it >= MI_VIB0 && it <= MI_VIB_LAST);
      if (value_row) menu_change(ux < UI_W / 2 ? -1 : 1);
      else menu_activate();
      return;
    }
    if (in_launcher) {
      layout_launcher(ww, wh);
      for (int i = 0; i < LAUNCH_N; i++) if (in_rect(launch_rect[i], x, y)) { launch_sel = i; vibrate_touch(); launcher_activate(i); }
      return;
    }
    if (!touch_visible()) return;
    layout_touch(ww, wh);
    int z = touch_zone(x, y);
    if (z != TZ_NONE) vibrate_touch();
    if (z == TZ_START && !on_title_screens()) { menu_set_open(1); return; }
    if (z == TZ_START) z = TZ_B1;   // on the title screens START starts the game
    for (int i = 0; i < 10; i++)
      if (!fingers[i].used) { fingers[i].used = 1; fingers[i].id = e->tfinger.fingerId; fingers[i].zone = z; fingers[i].x = x; fingers[i].y = y; break; }
  } else {
    for (int i = 0; i < 10; i++) {
      if (!fingers[i].used || fingers[i].id != e->tfinger.fingerId) continue;
      if (e->type == SDL_FINGERUP) fingers[i].used = 0;
      else {
        uint8_t before = fingers[i].zone == TZ_DPAD ? dpad_dirs(fingers[i].x, fingers[i].y) : 0;
        int zone_before = fingers[i].zone;
        fingers[i].x = x; fingers[i].y = y;
        // sliding between the two buttons or out of the cross keeps working
        int z = touch_zone(x, y);
        if (z == TZ_B1 || z == TZ_B2 || z == TZ_B3 || z == TZ_DPAD) fingers[i].zone = z;
        // a tap is felt when the finger slides onto another button or another direction of the cross
        if (fingers[i].zone != zone_before) vibrate_touch();
        else if (z == TZ_DPAD) { uint8_t now = dpad_dirs(x, y); if (now && now != before) vibrate_touch(); }
      }
    }
  }
  update_touch_input();
}

static void present(void) {
  if (in_launcher && art_ok) {
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    ensure_textures(SMS_W);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (viewer.kind) draw_viewer(ww, wh); else draw_launcher(ww, wh);
    draw_ui();
    SDL_UpdateTexture(tex_ui, NULL, ui, UI_W * 4);
    double us = (double)ww / UI_W < (double)wh / UI_H ? (double)ww / UI_W : (double)wh / UI_H;
    SDL_Rect udst = {(int)((ww - UI_W * us) / 2), (int)ui_top(wh, UI_H * us), (int)(UI_W * us), (int)(UI_H * us)};
    SDL_RenderCopy(ren, tex_ui, NULL, &udst);
    SDL_RenderPresent(ren);
    return;
  }
  // Game Gear: only while playing. Title, story and continue screens, and the bosses' dialogues (their boxes
  // are wider than the piece), are shown whole; a few frames without the stage on screen are not enough
  static int gg_off_frames;
  if (mod_is_gameplay(&S)) gg_off_frames = 0; else if (gg_off_frames < 1000) gg_off_frames++;
  if (!cfg.gg || on_title_screens() || gg_off_frames > 20) gg_on = 0;
  else if (!gg_off_frames) gg_on = 1;
  if (!gg_on) gg_x = -1;   // the next stage starts centred on the hero
  int gw = cfg.widescreen ? WIDE_W : gg_on ? GG_W : SMS_W;
  game_h = gg_on ? GG_H : SMS_H;
  ensure_textures(gw);
  if (cfg.widescreen) mod_render(&S, pix, gw, MOD_WIDE_EXT, NULL, E.wide ? E.shown : NULL, E.wide ? E.n_shown : 0);
  else if (gg_on) { mod_render_full43(&S, gg_full, SMS_W); gg_crop(); }
  else mod_render_full43(&S, pix, gw);   // also fills the column the game blanks on the left
  SDL_UpdateTexture(tex_game, NULL, pix, gw * 4);
  int ww, wh;
  SDL_GetRendererOutputSize(ren, &ww, &wh);
  SDL_Rect dst = dest_rect(ww, wh, gw);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
  SDL_RenderClear(ren);
  if (cfg.bands && (dst.w < ww - 2 || dst.h < wh - 2)) draw_ambient(gw, ww, wh);
  if (cfg.filter == FILTER_SHARP_SMOOTH) {
    // integer nearest prescale, then linear down to the final size
    int k = (int)ceil((double)dst.h / game_h);
    if (k < 1) k = 1;
    if (k > 8) k = 8;
    static int sharp_w, sharp_h;
    if (!tex_sharp || sharp_k != k || sharp_w != gw || sharp_h != game_h) {
      sharp_w = gw; sharp_h = game_h;
      if (tex_sharp) SDL_DestroyTexture(tex_sharp);
      tex_sharp = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, gw * k, game_h * k);
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
  } else if (cfg.filter == FILTER_CRT || cfg.filter == FILTER_CRT_CURVED) {
    draw_crt(dst, gw, cfg.filter == FILTER_CRT_CURVED);
  } else {
    SDL_SetTextureScaleMode(tex_game, cfg.filter == FILTER_SMOOTH ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    SDL_RenderCopy(ren, tex_game, NULL, &dst);
    if (cfg.filter == FILTER_SCANLINES) SDL_RenderCopy(ren, tex_scan, NULL, &dst);
  }
  if (gg_on && mod_music_paused(&S)) gg_draw_hud(dst);
  if (touch_visible()) draw_touch(ww, wh);
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
  case SDLK_RETURN: return on_title_screens() ? BTN_1 : BTN_PAUSE;
  }
  return 0;
}


static uint8_t padmap(int b) {
  if (b == cfg.jump_btn) return BTN_2;
  if (b == cfg.sub_btn) return BTN_UP | BTN_1;   // the game throws the subweapon with up + attack
  if (is_face(b)) return BTN_1;   // every other face button attacks
  switch (b) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
  case SDL_CONTROLLER_BUTTON_START: return on_title_screens() ? BTN_1 : BTN_PAUSE;
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
  if (viewer_active() && viewer_event(e)) return;
  if (sv_over && !in_launcher && sv_over_event(e)) return;
  if (br_over && !in_launcher && br_over_event(e)) return;
  // list of achievements: left and right change the page, anything else closes it
  if (show_logros && (e->type == SDL_KEYDOWN || e->type == SDL_CONTROLLERBUTTONDOWN || e->type == SDL_FINGERDOWN)) {
    int d = 0;
    if (e->type == SDL_KEYDOWN) d = e->key.keysym.sym == SDLK_LEFT || e->key.keysym.sym == SDLK_UP ? -1 : e->key.keysym.sym == SDLK_RIGHT || e->key.keysym.sym == SDLK_DOWN ? 1 : 0;
    else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
      int b = e->cbutton.button;
      d = b == SDL_CONTROLLER_BUTTON_DPAD_LEFT || b == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || b == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? 1 : 0;
    } else d = e->tfinger.x < 0.3f ? -1 : e->tfinger.x > 0.7f ? 1 : 0;
    if (e->type == SDL_KEYDOWN && e->key.repeat) return;
    if (d) logros_page = (logros_page + d + logros_pages()) % logros_pages();
    else show_logros = 0;
    return;
  }
  // start screen: any button closes the credits, directions move between buttons
  if (in_launcher && !menu_open && art_ok) {
    int act = 0, dx = 0, quit = 0;
    if (e->type == SDL_KEYDOWN && !e->key.repeat) {
      SDL_Keycode k = e->key.keysym.sym;
      dx = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0;
      act = k == SDLK_RETURN || k == SDLK_z || k == SDLK_SPACE;
      quit = k == SDLK_ESCAPE || k == SDLK_AC_BACK;
      if (show_credits) {
        if (k == SDLK_RETURN || k == SDLK_z) credits_tap(); else show_credits = 0;
        return;
      }
    } else if (e->type == SDL_CONTROLLERBUTTONDOWN) {
      int b = e->cbutton.button;
      if (show_credits) {
        if (b == SDL_CONTROLLER_BUTTON_A) credits_tap(); else show_credits = 0;
        return;
      }
      dx = b == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? -1 : b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? 1 : 0;
      act = b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START;
    } else if (e->type == SDL_CONTROLLERAXISMOTION && e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
      static int held;
      int d = e->caxis.value < -20000 ? -1 : e->caxis.value > 20000 ? 1 : 0;
      if (d && !held) dx = d;
      held = d;
    }
    if (dx) { launch_sel = (launch_sel + dx + LAUNCH_N) % LAUNCH_N; return; }
    if (act) { launcher_activate(launch_sel); return; }
    if (quit) { launch_sel = LAUNCH_QUIT; return; }
  }
  switch (e->type) {
  case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION: handle_touch(e); break;
  case SDL_QUIT: running = 0; break;
  case SDL_APP_WILLENTERBACKGROUND: cfg_save(); logros_flush(); if (!menu_open && !viewer_active()) menu_set_open(1); break;
  case SDL_APP_DIDENTERFOREGROUND:
    viewer_back_from_picker();
    if (waiting_import) { waiting_import = 0; import_profile_file(); }
    if (waiting_music) {
      cfg.music_pack = waiting_music;
      waiting_music = 0;
      music_stop();
      load_music_pack();
      if (music_pack_loaded()) {
        cfg_save();
        char t[80]; snprintf(t, sizeof t, T("Music pack loaded: %d tracks", "Pack de música cargado: %d pistas"), music_pack_tracks());
        show_toast(t, 2500);
        if (music_current > 0) mod_music_request(&S, music_current);
      } else show_toast(T("That file has no valid music tracks", "Ese archivo no tiene pistas de música válidas"), 3000);
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
    if (e->key.keysym.sym == SDLK_RETURN) kb_input &= ~(BTN_1 | BTN_PAUSE);   // what it meant may have changed
    break;
  case SDL_CONTROLLERDEVICEADDED: open_pad(e->cdevice.which); break;
#ifdef __ANDROID__
  case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP: {
    // C and Z (Android KEYCODE_BUTTON_C/Z) become two more face buttons: they attack, and can be
    // chosen as the jump or subweapon button like A, B, X and Y
    int b = e->jbutton.button == 17 ? PAD_C : e->jbutton.button == 18 ? PAD_Z : -1;
    if (b < 0) break;
    SDL_Event c;
    memset(&c, 0, sizeof c);
    c.type = e->type == SDL_JOYBUTTONDOWN ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    c.cbutton.which = e->jbutton.which;
    c.cbutton.button = (Uint8)b;
    c.cbutton.state = e->jbutton.state;
    handle_event(&c);
    break;
  }
#endif
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
          show_toast(T("Jump button saved", "Botón de saltar guardado"), 1500);
        } else {
          if (b == cfg.jump_btn) { show_toast(T("That button already jumps", "Ese botón ya es el de saltar"), 1500); break; }
          cfg.sub_btn = b;
          show_toast(T("Subweapon button saved", "Botón de subarma guardado"), 1500);
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
  if (!rom) { rom_error = T("The Master of Darkness ROM was not found.", "No se encontró la ROM de Master of Darkness."); return; }
  if (rom_size < 0x8000 || rom_size > (1 << 20) || memcmp(rom + 0x7FF0, "TMR SEGA", 8) != 0) {
    rom_error = T("The file is not a valid Master System ROM.", "El archivo no es una ROM válida de Master System.");
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
    ui_text_center(190, T("Close and open the app again to choose the ROM.", "Cierra y vuelve a abrir la app para elegir la ROM."), 0xB8B0A8);
#else
    ui_text_center(190, T("Put the ROM as mod.sms next to the program.", "Coloca la ROM como mod.sms junto al ejecutable."), 0xB8B0A8);
#endif
    ui_text_center(250, T("Press any button to exit", "Pulsa cualquier botón para salir"), 0x807870);
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
typedef struct { typeof(E.spawn) spawn; uint8_t seen_in[46]; uint32_t slot_entry[46]; int slot_trigger[46], slot_world[46];
                 uint8_t slot_y[46], slot_list[46]; } rw_extra_t;
#define RW_SLOT_FIELDS(X) X(slot_entry) X(slot_trigger) X(slot_world) X(slot_y) X(slot_list)
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
#define RW_SAVE(f) memcpy(rw_extra[rw_head].f, E.f, sizeof E.f);
  RW_SLOT_FIELDS(RW_SAVE)
  if (rw_count < RW_SLOTS) rw_count++;
}

static int rw_pop(void) {
  if (!rw_buf || rw_count == 0) return 0;
  sms_load_state(&S, rw_buf + rw_size * rw_head, rw_size);
  memcpy(E.spawn, rw_extra[rw_head].spawn, sizeof E.spawn);
  memcpy(E.seen_in, rw_extra[rw_head].seen_in, sizeof E.seen_in);
#define RW_LOAD(f) memcpy(E.f, rw_extra[rw_head].f, sizeof E.f);
  RW_SLOT_FIELDS(RW_LOAD)
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
  { char lp[1100]; cfg_path(lp, sizeof lp, "logros.txt"); logros_init(lp); }
  SDL_free(pp);
  cfg_load();
  detect_ui_lang();

  Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
  wflags |= SDL_WINDOW_FULLSCREEN;
#endif
  int win_w = WIDE_W * 3, win_h = SMS_H * 3;
  if (getenv("MOD_WINSIZE")) sscanf(getenv("MOD_WINSIZE"), "%dx%d", &win_w, &win_h);   // tests: other screen shapes
  win = SDL_CreateWindow("Master of Darkness", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, wflags);
  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
  pix = malloc(sizeof(uint32_t) * WIDE_W * SMS_H);
  load_art();

  for (int i = 0; i < SDL_NumJoysticks(); i++) open_pad(i);

  find_and_load_rom(argc, argv);
  if (!rom) { error_screen(); SDL_Quit(); return 0; }

  SDL_AudioSpec want = {0}, have;
  want.freq = SMS_AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
  audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);

  if (cfg.lang < 0) cfg.lang = cfg.ui_lang == 1;   // first time: the game text in the language of the menus
  apply_language();
  apply_suit();
  apply_palette();
  sms_init(&S, rom, (int)rom_size);
  mod_enhance_attach(&S, &E);
  mod_enhance_set_wide(&E, cfg.widescreen, MOD_WIDE_EXT);
  apply_game_options();
  load_music_pack();
  E.music_has = music_enabled_has;
  if (rom_crc != KNOWN_CRC) show_toast(T("Unknown ROM: some enhancements may not work", "ROM no reconocida: algunas mejoras pueden fallar"), 4000);
  else if (!art_ok) show_toast(T("Press SELECT or BACK to open the menu", "Pulsa SELECT o ATRÁS para abrir el menú"), 4000);

  const double frame_time = 1.0 / 60.0;
  double acc = 0;
  Uint64 last = SDL_GetPerformanceCounter();
  const double freq = (double)SDL_GetPerformanceFrequency();
  int16_t mixbuf[SMS_SAMPLES_PER_FRAME];

  // Automated test mode (desktop only): MOD_AUTOTEST="frames:out.bmp" MOD_INPUT=script MOD_MENU=1
  const char* autotest = getenv("MOD_AUTOTEST");
  if (autotest) {
    in_launcher = getenv("MOD_LAUNCHER") != NULL;
    if (getenv("MOD_CREDITS")) show_credits = 1;
    if (getenv("MOD_VIEWER")) {   // MOD_VIEWER=1 gallery, 2 manual, 3 ticket; MOD_VIEWER_IDX, MOD_VIEWER_ZOOM
      int vk = atoi(getenv("MOD_VIEWER"));
      if (vk == VIEW_TICKET) viewer_open_ticket(); else viewer_open_pack(vk);
      if (getenv("MOD_VIEWER_IDX")) viewer_show(atoi(getenv("MOD_VIEWER_IDX")));
      if (getenv("MOD_VIEWER_ZOOM")) viewer.zoom = atof(getenv("MOD_VIEWER_ZOOM"));
    }
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
        for (char* c = t; *c; c++) m |= *c == 'R' ? BTN_RIGHT : *c == 'L' ? BTN_LEFT : *c == 'U' ? BTN_UP : *c == 'D' ? BTN_DOWN : *c == '1' ? BTN_1 : *c == '2' ? BTN_2 : *c == 'P' ? BTN_PAUSE : 0;
        spans[ns].a = a; spans[ns].b = b; spans[ns].btn = m; ns++;
      }
    }
    if (f) fclose(f);
    for (int fr = 0; fr < total; fr++) {
      S.input = 0;
      for (int k = 0; k < ns; k++) if (fr >= spans[k].a && fr <= spans[k].b) S.input |= spans[k].btn;
      mod_text_frame(&S, lang_active());
      sms_run_frame(&S);
      mod_enhance_frame(&S, &E);
      for (int ev; (ev = mod_music_poll(&E)); ) if (ev & 0xFF) music_current = ev & 0xFF;   // which screen it is
    }
    if (getenv("MOD_EVENTS")) {
      // tests: l r u d = keys, a = enter, b = x key, A B L R X = gamepad, t<x>,<y>; = a tap (0..1)
      SDL_Event ev;
      for (const char* c = getenv("MOD_EVENTS"); *c; c++) {
        memset(&ev, 0, sizeof ev);
        if (*c == 't') {
          float tx, ty;
          if (sscanf(c + 1, "%f,%f", &tx, &ty) != 2) break;
          ev.type = SDL_FINGERDOWN; ev.tfinger.fingerId = 1; ev.tfinger.x = tx; ev.tfinger.y = ty;
          handle_event(&ev);
          ev.type = SDL_FINGERUP;
          handle_event(&ev);
          while (*c && *c != ';') c++;
          if (!*c) break;
          SDL_Delay(400);   // not a double tap
          continue;
        }
        int key = *c == 'l' ? SDLK_LEFT : *c == 'r' ? SDLK_RIGHT : *c == 'u' ? SDLK_UP : *c == 'd' ? SDLK_DOWN : *c == 'a' ? SDLK_RETURN : *c == 'b' ? SDLK_x : 0;
        if (key) { ev.type = SDL_KEYDOWN; ev.key.keysym.sym = key; }
        else {
          ev.type = SDL_CONTROLLERBUTTONDOWN;
          ev.cbutton.button = *c == 'A' ? SDL_CONTROLLER_BUTTON_A : *c == 'B' ? SDL_CONTROLLER_BUTTON_B : *c == 'X' ? SDL_CONTROLLER_BUTTON_X :
                              *c == 'L' ? SDL_CONTROLLER_BUTTON_DPAD_LEFT : SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        }
        handle_event(&ev);
      }
      if (getenv("MOD_EVENTS_LOG")) printf("sel=%d credits=%d viewer=%d prompt=%d cur=%d zoom=%.1f menu=%d\n", launch_sel, show_credits, viewer.kind, viewer.prompt, viewer.cur, viewer.zoom, menu_open);
    }
    if (getenv("MOD_SVOVER")) { game_mode = GM_SURVIVOR; sv_diff = atoi(getenv("MOD_SVOVER")); sv_over = 1; sv_end_round = 1; sv_end_stage = 2; }
    if (getenv("MOD_LOGROS_LIST")) { show_logros = 1; logros_page = atoi(getenv("MOD_LOGROS_LIST")); }
    if (getenv("MOD_MENU")) { menu_open = 1; menu_sel = atoi(getenv("MOD_MENU")); if (getenv("MOD_PAGE")) menu_page = atoi(getenv("MOD_PAGE")); }
    toast_until = 0;
    if (getenv("MOD_SUB")) { gram_w(RAM_SUBWEAPON, (uint8_t)atoi(getenv("MOD_SUB"))); gram_w(RAM_AMMO, getenv("MOD_AMMO") ? (uint8_t)atoi(getenv("MOD_AMMO")) : 9); }
    if (getenv("MOD_TOUCHDIR")) {   // a finger on the right arm of the cross
      cfg.touch = 1;
      int tw, th; SDL_GetRendererOutputSize(ren, &tw, &th);
      layout_touch(tw, th);
      SDL_Rect r = tz_rect[TZ_DPAD];
      fingers[0].used = 1; fingers[0].zone = TZ_DPAD;
      fingers[0].x = r.x + r.w * 0.85f; fingers[0].y = r.y + r.h * (getenv("MOD_TOUCHDIR")[0] == 'd' ? 0.2f : 0.5f);
    }
    present();
    int ww, wh; SDL_GetRendererOutputSize(ren, &ww, &wh);
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
    SDL_SaveBMP(sf, out);
    SDL_Quit();
    return 0;
  }

  // Loop test (desktop): MOD_LOOPTEST="frames:dir" runs the real loop as fast as it can, without a window to look
  // at; MOD_LOOPMODE=br starts a Boss Rush, with an attacking bot, invincible and bosses on their last energy.
  // Writes dir/fNNNNNN.bmp every 600 frames and prints the run's changes.
  const char* looptest = getenv("MOD_LOOPTEST");
  int lt_frames = 0; char lt_dir[256] = "";
  if (looptest) {
    sscanf(looptest, "%d:%255s", &lt_frames, lt_dir);
    in_launcher = 0;
    if (getenv("MOD_LOOPMODE") && !strcmp(getenv("MOD_LOOPMODE"), "br")) { br_new_run(); test_inv = 1; }
  }
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(&e);
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (now - last) / freq;
    last = now;
    if (dt > 0.1) dt = 0.1;
    if (looptest) {
      dt = 4.0 / 60.0;
      static int lt_last = -1, lt_bot;
      int fr = (int)S.st.frame;
      static int lt_end;
      if (fr >= lt_frames || (br_over && ++lt_end > 30)) {   // the result screen stops the game: picture of it and out
        running = 0;
        present();
        int ww, wh; SDL_GetRendererOutputSize(ren, &ww, &wh);
        SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
        char op[300]; snprintf(op, sizeof op, "%s/final.bmp", lt_dir);
        SDL_SaveBMP(sf, op); SDL_FreeSurface(sf);
      }
      lt_bot = (fr / 6) % 7;
      kb_input = (fr % 8 < 4 ? BTN_1 : 0) | (lt_bot < 3 ? BTN_RIGHT : lt_bot < 5 ? BTN_LEFT : 0) | (fr % 90 < 10 ? BTN_2 : 0);
      if (game_mode == GM_BOSSRUSH && br_fight && gram(0xCB3A) > 1) gram_w(0xCB3A, 1);
      // the bot cannot reach Dracula: after a while he is taken as beaten and the game jumps to its ending (AC35, bank 3)
      static int lt_drac;
      if (game_mode == GM_BOSSRUSH && br_fight && br_k == BR_BOSSES - 1 && !br_down && ++lt_drac == 2400) {
        br_boss_beaten();
        S.st.bank[2] = 3; S.st.ram[0x1FFF] = 3; S.st.cpu.pc = 0xAC35; S.st.cpu.sp = 0xDFE0;
      }
      int key = br_k << 8 | br_transit << 4 | br_fight << 3 | br_down << 2 | br_over;
      if (key != lt_last) { printf("LT %d boss=%d transit=%d fight=%d down=%d over=%d time=%u en=%d keep=%d w=%d s=%d a=%d\n", fr, br_k, br_transit, br_fight, br_down, br_over, br_frames,
                                   gram(0xCA3A), E.energy_keep, gram(RAM_WEAPON), gram(RAM_SUBWEAPON), gram(RAM_AMMO)); lt_last = key; }
      static int lt_shot;
      if (fr / 600 != lt_shot) {
        lt_shot = fr / 600;
        present();
        int ww, wh; SDL_GetRendererOutputSize(ren, &ww, &wh);
        SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
        char op[300]; snprintf(op, sizeof op, "%s/f%06d.bmp", lt_dir, fr);
        SDL_SaveBMP(sf, op); SDL_FreeSurface(sf);
      }
    }
    if (in_launcher && !art_ok) in_launcher = 0;   // no start screen art: straight to the game
    if (rw_count_reset) { rw_count = 0; rw_count_reset = 0; }
    rewinding = !menu_open && !in_launcher && cfg.triggers && !ADVANTAGES_BLOCKED && (any_trigger(trig_l) || key_rewind);
    fast_forward = !menu_open && !rewinding && !in_launcher && cfg.triggers && (!ADVANTAGES_BLOCKED || test_inv) && (any_trigger(trig_r) || key_ff);
    if (!menu_open && !in_launcher && !sv_over && !br_over) {
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
        S.input = kb_input | pad_buttons() | stick_dirs() | touch_input;
        // the game's hidden options menu (up + 1 + 2 and start on the title screen: hit check, round select...) is
        // not reachable in the modes where advantages are locked
        if (ADVANTAGES_BLOCKED && on_title_screens() && (S.input & (BTN_UP | BTN_1 | BTN_2)) == (BTN_UP | BTN_1 | BTN_2)) S.input &= (uint8_t)~BTN_UP;
        // faster CPU only while playing: menus and story screens time themselves with CPU loops
        S.cpu_mult = (cfg.turbo && mod_is_gameplay(&S)) ? 2 : 1;
        if (rewinding) {
          // one snapshot back (2 frames) per frame shown, then one frame to get the picture
          if (rw_pop()) { S.input = 0; mod_text_frame(&S, lang_active()); sms_run_frame(&S); mod_enhance_frame(&S, &E); mod_events_take(&E); }
          continue;
        }
        int steps = fast_forward ? 4 : 1;
        for (int k = 0; k < steps; k++) {
          cheats_frame();
          if (k == 0) suit_frame();
          mod_text_frame(&S, lang_active());
          if (game_mode == GM_BOSSRUSH) br_before_frame();
          sms_run_frame(&S);
          mod_enhance_frame(&S, &E);
          { uint32_t gev = mod_events_take(&E);
            if (!fast_forward && gev) vibrate_game(gev);
            if (game_mode == GM_SURVIVOR) sv_frame(gev);
            if (game_mode == GM_BOSSRUSH) br_frame(gev); else br_watch_frame();
            if (LOGROS_ON) {
              logros_in_t li = {gev, mod_is_gameplay(&S), gram(RAM_ROUND), gram(RAM_STAGE), (uint8_t)(gram(RAM_WEAPON) & 3), (uint8_t)(gram(RAM_SUBWEAPON) & 3), gram(RAM_AMMO)};
              logros_frame(&li);
            }
          }
          for (int ev; (ev = mod_music_poll(&E)); ) {
            int kind = ev & 0xF00, id = ev & 0xFF;
            if (LOGROS_ON && id) logros_music(id);
            if (game_mode == GM_SURVIVOR && id) sv_music(id);
            if (game_mode == GM_BOSSRUSH && id) br_music(id);
            if (kind == MOD_MUSIC_PLAY) { music_current = id; music_play(id); }
            else if (kind == MOD_MUSIC_STOP) { if (id) music_current = id; music_stop(); }
            else if (kind == MOD_MUSIC_FADE) music_fade();
          }
          if (cfg.triggers && !ADVANTAGES_BLOCKED && (++rw_tick & 1) == 0) rw_push();
          // several at once are shown one after another
          static Uint32 logro_shown_until;
          if (LOGROS_ON && (Sint32)(SDL_GetTicks() - logro_shown_until) >= 0) {
            int nl = logros_pop_new();
            if (nl >= 0) {
              char t[96]; snprintf(t, sizeof t, T("Achievement: %s", "Logro: %s"), logros_get(nl)->name[ES]);
              show_toast(t, 3000);
              logro_shown_until = SDL_GetTicks() + 3000;
              vibrate_sample(VIB_BOSS);
            }
          }
        }
        music_set_paused(mod_music_paused(&S));
        if (audio_dev && queued < 8 && !fast_forward) {
          for (int i = 0; i < SMS_SAMPLES_PER_FRAME; i++) mixbuf[i] = (int16_t)(S.audio[i] * cfg.volume / 10);
          music_mix(mixbuf, SMS_SAMPLES_PER_FRAME, cfg.volume);
          SDL_QueueAudio(audio_dev, mixbuf, sizeof mixbuf);
        }
      }
      if (frames == 4) acc = 0;
    }
    viewer_tick();
    present();
    if (menu_open || in_launcher) SDL_Delay(1);
  }
  cfg_save();
  logros_flush();
#ifdef __ANDROID__
  // "Exit" ends the whole app, not only the game thread
  SDL_AndroidSendMessage(0x8003, 0);
  SDL_Delay(200);
#endif
  SDL_Quit();
  exit(0);
}
