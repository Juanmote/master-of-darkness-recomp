// Music pack player: reads Ogg Vorbis tracks from a zip (miniz) and decodes them as they play
// (stb_vorbis), resampled to the emulator's 44100 Hz mono output.
#include "music.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "miniz.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

// The game's tracks (sound driver ids 0x81-0x8F). Names are what each one is used for.
const music_track_t music_tracks[] = {
  {0x87, "87", "Introducción (la leyenda)", false, "Intro (the legend)"},
  {0x83, "83", "Pantalla de título", true, "Title screen"},
  {0x85, "85", "Presentación de ronda", false, "Round intro"},
  {0x8B, "8B", "Ronda 1", true, "Round 1"},
  {0x89, "89", "Ronda 2", true, "Round 2"},
  {0x8F, "8F", "Ronda 3", true, "Round 3"},
  {0x8C, "8C", "Ronda 4", true, "Round 4"},
  {0x84, "84", "Ronda 5", true, "Round 5"},
  {0x88, "88", "Por identificar (88)", true, "Unidentified (88)"},
  {0x8E, "8E", "Por identificar (8E)", true, "Unidentified (8E)"},
  {0x81, "81", "Escena superada", false, "Scene clear"},
  {0x86, "86", "Ronda superada", false, "Round clear"},
  {0x8A, "8A", "Vida perdida", false, "Life lost"},
  {0x82, "82", "Continuar / fin de partida", true, "Continue / game over"},
  {0x8D, "8D", "Historia y final", false, "Story and ending"},
};
const int music_track_count = sizeof music_tracks / sizeof music_tracks[0];

#define OUT_RATE 44100

static mz_zip_archive zip;
static bool zip_open;
static int entry_main[256], entry_intro[256];   // zip entry index + 1 (0 = none)
static int n_replaced;

static struct {
  int id;
  bool intro;            // playing the _intro part
  uint8_t* data;         // compressed ogg in memory
  stb_vorbis* v;
  int channels, rate;
  float frac;            // resampling position between prev and next
  float prev, next;
  float buf[4096]; int buf_n, buf_pos;
  float fade, fade_step; // 1 = full volume
  bool paused;
  bool active;
} P;

static int track_index(int id) {
  for (int i = 0; i < music_track_count; i++) if (music_tracks[i].id == id) return i;
  return -1;
}

static void close_stream(void) {
  if (P.v) stb_vorbis_close(P.v);
  P.v = NULL;
  if (P.data) mz_free(P.data);
  P.data = NULL;
}

void music_unload(void) {
  music_stop();
  if (zip_open) mz_zip_reader_end(&zip);
  zip_open = false;
  memset(entry_main, 0, sizeof entry_main);
  memset(entry_intro, 0, sizeof entry_intro);
  n_replaced = 0;
}

// file name (any folder inside the zip) -> track: "8b.ogg", "8B_intro.ogg", "8b - ronda 1.ogg"...
static void index_entry(const char* path, int idx) {
  const char* name = strrchr(path, '/');
  name = name ? name + 1 : path;
  size_t n = strlen(name);
  if (n < 6 || !isxdigit((unsigned char)name[0]) || !isxdigit((unsigned char)name[1])) return;
  char ext[5] = {0};
  for (int i = 0; i < 4; i++) ext[i] = (char)tolower((unsigned char)name[n - 4 + i]);
  if (strcmp(ext, ".ogg")) return;
  char hx[3] = {name[0], name[1], 0};
  int id = (int)strtol(hx, NULL, 16);
  if (track_index(id) < 0) return;
  bool intro = false;
  for (const char* p = name + 2; *p && p < name + n - 4; p++)
    if (tolower((unsigned char)p[0]) == 'i' && tolower((unsigned char)p[1]) == 'n' && tolower((unsigned char)p[2]) == 't' &&
        tolower((unsigned char)p[3]) == 'r' && tolower((unsigned char)p[4]) == 'o') { intro = true; break; }
  if (intro) entry_intro[id] = idx + 1; else entry_main[id] = idx + 1;
}

bool music_load_pack(const char* zip_path) {
  music_unload();
  memset(&zip, 0, sizeof zip);
  if (!mz_zip_reader_init_file(&zip, zip_path, 0)) return false;
  zip_open = true;
  int n = (int)mz_zip_reader_get_num_files(&zip);
  for (int i = 0; i < n; i++) {
    char name[512];
    if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;
    mz_zip_reader_get_filename(&zip, i, name, sizeof name);
    index_entry(name, i);
  }
  for (int i = 0; i < music_track_count; i++) if (entry_main[music_tracks[i].id]) n_replaced++;
  if (!n_replaced) { music_unload(); return false; }
  return true;
}

bool music_pack_loaded(void) { return zip_open && n_replaced > 0; }
int music_pack_tracks(void) { return n_replaced; }
bool music_has(int id) { return id >= 0 && id < 256 && zip_open && entry_main[id] != 0; }

static bool open_part(int id, bool intro) {
  close_stream();
  int e = intro ? entry_intro[id] : entry_main[id];
  if (!e) return false;
  size_t size = 0;
  P.data = mz_zip_reader_extract_to_heap(&zip, (mz_uint)(e - 1), &size, 0);
  if (!P.data) return false;
  int err = 0;
  P.v = stb_vorbis_open_memory(P.data, (int)size, &err, NULL);
  if (!P.v) { mz_free(P.data); P.data = NULL; return false; }
  stb_vorbis_info info = stb_vorbis_get_info(P.v);
  P.channels = info.channels; P.rate = (int)info.sample_rate;
  P.intro = intro;
  P.buf_n = P.buf_pos = 0;
  return true;
}

void music_play(int id) {
  if (!music_has(id)) { music_stop(); return; }
  bool ok = (entry_intro[id] && open_part(id, true)) || open_part(id, false);
  if (!ok) { music_stop(); return; }
  P.id = id; P.active = true; P.paused = false;
  P.fade = 1.0f; P.fade_step = 0.0f;
  P.frac = 0.0f; P.prev = P.next = 0.0f;
}

void music_stop(void) { close_stream(); P.active = false; }

void music_fade(void) { if (P.active) P.fade_step = 1.0f / OUT_RATE; }

void music_set_paused(bool paused) { P.paused = paused; }

// next mono sample at the file's own rate (false at the very end of a track that does not loop)
static bool next_source_sample(float* out) {
  int tries = 0;
  while (P.buf_pos >= P.buf_n) {
    if (++tries > 3) return false;   // empty or broken file
    int frames = stb_vorbis_get_samples_float_interleaved(P.v, P.channels, P.buf, 4096 - 4096 % P.channels);
    if (frames > 0) {
      if (P.channels > 1)
        for (int i = 0; i < frames; i++) {
          float s = 0;
          for (int c = 0; c < P.channels; c++) s += P.buf[i * P.channels + c];
          P.buf[i] = s / P.channels;
        }
      P.buf_n = frames; P.buf_pos = 0;
      break;
    }
    // end of this part: intro -> main part; the main part loops or ends
    if (P.intro) { if (!open_part(P.id, false)) return false; continue; }
    int ti = track_index(P.id);
    if (ti >= 0 && music_tracks[ti].loops) { stb_vorbis_seek_start(P.v); continue; }
    return false;
  }
  *out = P.buf[P.buf_pos++];
  return true;
}

void music_mix(int16_t* out, int n, int volume) {
  if (!P.active || P.paused || !P.v) return;
  float step = (float)P.rate / OUT_RATE;
  for (int i = 0; i < n; i++) {
    P.frac += step;
    while (P.frac >= 1.0f) {
      P.frac -= 1.0f;
      P.prev = P.next;
      if (!next_source_sample(&P.next)) { music_stop(); return; }
    }
    float s = (P.prev + (P.next - P.prev) * P.frac) * P.fade;
    if (P.fade_step > 0) { P.fade -= P.fade_step; if (P.fade <= 0) { music_stop(); return; } }
    int v = out[i] + (int)(s * 20000.0f * volume / 10);
    out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
  }
}

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
