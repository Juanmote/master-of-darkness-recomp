// Start screen music: an original piece written for this project (D minor, 100 bpm), played by three chip
// voices made here sample by sample, like the console's sound chip: a pulse wave for the melody, a square
// one for the harpsichord-like broken chords and a triangle for the bass. No audio file and no third-party
// music. A 2-bar opening, then a 16-bar loop (A: bars 1-8, B: bars 9-16).
#include "launcher_music.h"
#include <math.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RATE 44100
#define TICK (RATE * 60 / 100 / 4)   // a sixteenth note at 100 bpm

// notes are MIDI numbers; 0 rest, 1 hold the previous note
enum { R = 0, H = 1 };
enum {
  G3 = 55, A3 = 57, Bb3 = 58, C4 = 60, Cs4 = 61, D4 = 62, Eb4 = 63, E4 = 64, F4 = 65, G4 = 67, A4 = 69, Bb4 = 70,
  C5 = 72, Cs5 = 73, D5 = 74, Eb5 = 75, E5 = 76, F5 = 77, G5 = 79, A5 = 81
};

// melody in eighth notes, 8 per bar
static const unsigned char melody[16][8] = {
  {A4, H, D5, H, F5, E5, D5, Cs5},   // Dm
  {D5, H, H, H, Bb4, H, C5, D5},     // Bb
  {G5, H, F5, E5, D5, H, Bb4, H},    // Gm
  {Cs5, H, E5, H, A4, H, H, H},      // A
  {F5, H, E5, F5, A5, H, F5, D5},    // Dm
  {C5, H, F5, H, A5, G5, F5, E5},    // F
  {D5, E5, F5, G5, E5, H, Cs5, H},   // Gm | A
  {D5, H, H, H, H, H, R, R},         // Dm
  {F5, H, D5, H, Bb4, H, D5, F5},    // Bb
  {E5, H, C5, H, G4, H, C5, E5},     // C
  {A5, H, G5, F5, E5, H, F5, H},     // F
  {D5, H, H, E5, F5, H, A4, H},      // Dm
  {Bb4, H, D5, H, G5, H, F5, E5},    // Gm
  {G5, H, Eb5, H, Bb4, H, G4, H},    // Eb
  {A4, H, Cs5, H, E5, H, G5, H},     // A
  {F5, E5, D5, Cs5, E5, H, A4, H},   // A7
};

// chord of each half bar: root, third, fifth (the broken chords and the bass come from it)
typedef struct { unsigned char n[3]; } chord_t;
#define DM {{D4, F4, A4}}
#define BB {{Bb3, D4, F4}}
#define GM {{G3, Bb3, D4}}
#define AA {{A3, Cs4, E4}}
#define FF {{F4, A4, C5}}
#define CC {{C4, E4, G4}}
#define EB {{Eb4, G4, Bb4}}
static const chord_t chords[16][2] = {
  {DM, DM}, {BB, BB}, {GM, GM}, {AA, AA}, {DM, DM}, {FF, FF}, {GM, AA}, {DM, DM},
  {BB, BB}, {CC, CC}, {FF, FF}, {DM, DM}, {GM, GM}, {EB, EB}, {AA, AA}, {AA, AA},
};
// broken chord: indexes into root, third, fifth, root + 12
static const unsigned char arp_pattern[16] = {0, 1, 2, 3, 2, 1, 2, 1, 0, 1, 2, 3, 2, 3, 2, 1};

static struct {
  long pos;                 // samples since the start
  double ph_lead, ph_arp, ph_bass;
  double env_lead, env_arp, env_bass;
  double f_lead, f_arp, f_bass;
  double lp_lead, lp_arp;   // one-pole low-pass: softer edges than a raw square
  double fade;
  long last_tick;
} M;

static double freq(int midi) { return 440.0 * pow(2.0, (midi - 69) / 12.0); }

void lmusic_reset(void) { memset(&M, 0, sizeof M); M.last_tick = -1; }


// what plays at a sixteenth note: bar (-2, -1 = the opening), step 0-15
static void start_tick(long tick) {
  long bar = tick / 16 - 2;
  int step = (int)(tick % 16);
  int b = bar < 0 ? 0 : (int)(bar % 16);
  const chord_t* c = &chords[b][step / 8];
  // broken chord, every sixteenth, one octave up in the B half
  int k = arp_pattern[step];
  int note = k == 3 ? c->n[0] + 12 : c->n[k];
  if (b >= 8 && bar >= 0) note += 12;
  M.f_arp = freq(note); M.env_arp = 1.0;
  // bass on the eighths: root, root, octave, fifth
  if ((step & 1) == 0) {
    static const int bass_step[8] = {0, 0, 12, 7, 0, 0, 12, 7};
    M.f_bass = freq(c->n[0] - 12 + bass_step[step / 2]);
    M.env_bass = 1.0;
  }
  // melody on the eighths, not in the opening
  if (bar >= 0 && (step & 1) == 0) {
    int m = melody[b][step / 2];
    if (m == R) M.f_lead = 0;
    else if (m != H) { M.f_lead = freq(m); M.env_lead = 1.0; }
  }
}

void lmusic_mix(int16_t* out, int n, int volume) {
  for (int i = 0; i < n; i++, M.pos++) {
    long tick = M.pos / TICK;
    if (tick != M.last_tick) {
      // past the opening the 16 bars loop
      long t = tick < 32 ? tick : 32 + (tick - 32) % 256;
      start_tick(t);
      M.last_tick = tick;
    }
    // melody: 25 % pulse, slight vibrato after the attack, slow decay
    double s_lead = 0;
    if (M.f_lead > 0) {
      double vib = 1.0 + 0.004 * sin(M.pos * 2.0 * M_PI * 5.5 / RATE) * (1.0 - M.env_lead);
      M.ph_lead += M.f_lead * vib / RATE; M.ph_lead -= floor(M.ph_lead);
      s_lead = (M.ph_lead < 0.25 ? 1.0 : -1.0) * (0.55 + 0.45 * M.env_lead);
      M.env_lead *= 0.99993;
    }
    M.lp_lead += (s_lead - M.lp_lead) * 0.35;
    // broken chords: square, plucked (fast decay)
    M.ph_arp += M.f_arp / RATE; M.ph_arp -= floor(M.ph_arp);
    double s_arp = (M.ph_arp < 0.5 ? 1.0 : -1.0) * M.env_arp;
    M.env_arp *= 0.99975;
    M.lp_arp += (s_arp - M.lp_arp) * 0.25;
    // bass: triangle
    M.ph_bass += M.f_bass / RATE; M.ph_bass -= floor(M.ph_bass);
    double s_bass = (M.ph_bass < 0.5 ? 4.0 * M.ph_bass - 1.0 : 3.0 - 4.0 * M.ph_bass) * (0.4 + 0.6 * M.env_bass);
    M.env_bass *= 0.99985;
    if (M.fade < 1.0) M.fade += 1.0 / (RATE * 1.5);   // 1.5 s fade-in
    double s = (0.16 * M.lp_lead + 0.08 * M.lp_arp + 0.20 * s_bass) * M.fade;
    int v = out[i] + (int)(s * 32767.0 * volume / 10);
    out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
  }
}
