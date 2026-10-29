// Sega Master System core (CPU interpreter + VDP mode 4 + SN76489 PSG).
// Used as the runtime for the port and as the reference for later recompilation.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "z80.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMS_W 256
#define SMS_H 192
#define SMS_SPR_OVER 8          // sprite pixels kept beyond the right edge (x 256..263)
#define SMS_AUDIO_RATE 44100
#define SMS_FPS 60
#define SMS_SAMPLES_PER_FRAME (SMS_AUDIO_RATE / SMS_FPS)  // 735

enum { BTN_UP = 1, BTN_DOWN = 2, BTN_LEFT = 4, BTN_RIGHT = 8, BTN_1 = 16, BTN_2 = 32, BTN_PAUSE = 64 };

typedef struct {
  uint8_t vram[0x4000];
  uint8_t cram[32];
  uint8_t reg[16];
  uint16_t addr;
  uint8_t code;
  uint8_t latch;
  uint8_t first;
  uint8_t buffer;
  uint8_t status;
  uint8_t vscroll_latched;
  int32_t line;
  int32_t line_counter;
  uint8_t line_irq;
} sms_vdp_t;

typedef struct {
  uint16_t tone[3];
  uint8_t vol[4];
  uint8_t noise;
  uint8_t latch;
  uint16_t lfsr;
  int32_t counter[4];
  uint8_t output[4];
  uint32_t frac;     // fixed point clock accumulator
} sms_psg_t;

// Everything that changes while running (saved in save states).
typedef struct {
  z80 cpu;                // function pointers are fixed up on load
  uint8_t ram[0x2000];
  uint8_t bank[3];
  uint8_t mapper_ctrl;
  sms_vdp_t vdp;
  sms_psg_t psg;
  uint32_t frame;
  uint8_t prev_input;
} sms_state_t;

typedef struct sms sms_t;
typedef struct {
  void (*on_exec)(sms_t*, uint16_t pc, int rom_off);
  void (*on_vram_write)(sms_t*, uint16_t addr, uint8_t val);
  void (*on_ram_write)(sms_t*, uint16_t addr, uint8_t val);
  void (*on_vdp_reg)(sms_t*, int reg, uint8_t val);
} sms_hooks_t;

struct sms {
  sms_state_t st;
  const uint8_t* rom;
  int rom_size;
  uint8_t input;                                   // active-high BTN_ bits
  int cpu_mult;                                    // CPU speed multiplier (1 = original, 2 = no slowdown)
  // per-frame outputs
  uint8_t fb_idx[SMS_H][SMS_W];                    // CRAM index per pixel
  uint8_t fb_bg[SMS_H][SMS_W];                     // same, background layer only
  uint8_t fb_spr[SMS_H][SMS_W];                    // 1 where a sprite pixel is visible
  uint8_t fb_sprtile[SMS_H][SMS_W];                // sprite table tile number of that pixel
  uint8_t spr_over[SMS_H][SMS_SPR_OVER];           // sprite pixels past x=255 (0 = none, else 16+c)
  uint8_t line_hscroll[SMS_H];
  uint8_t line_vscroll[SMS_H];
  uint8_t line_reg0[SMS_H];
  uint8_t line_display[SMS_H];
  uint8_t line_cram[SMS_H][32];                    // CRAM snapshot per line (raster palette effects)
  int16_t audio[SMS_SAMPLES_PER_FRAME];
  uint8_t sat_mid[256];                            // sprite table as displayed (copied at line 96)
  // psg write log for the current frame
  struct { uint32_t cyc; uint8_t val; } psg_log[1024];
  int psg_log_n;
  uint64_t frame_start_cyc;
  sms_hooks_t hooks;
  void* user;
  // recompiled CPU (NULL = interpreter only)
  int (*recomp_run)(sms_t* s, unsigned long target);
  void (*recomp_verify)(sms_t* s, const sms_state_t* before, int psg_n_before);
  uint32_t interp_steps;   // instructions run by the interpreter fallback (this frame)
  uint16_t last_interp_pc;
  uint8_t ei_shadow;       // last instruction was EI: no interrupt at this boundary
  // Address hooks for enhancements (fixed area 0x0000-0x7FFF only). pc_hook returns
  // true when it has replaced the instruction (it must then update pc/cycles itself).
  const uint8_t* hook_map;  // 0x8000 entries, nonzero = call pc_hook
  bool (*pc_hook)(sms_t* s, uint16_t pc);
};

void sms_init(sms_t* s, const uint8_t* rom, int size);
void sms_reset(sms_t* s);
void sms_run_frame(sms_t* s);
int sms_rom_offset(const sms_t* s, uint16_t addr);  // -1 if not ROM
uint8_t sms_peek(const sms_t* s, uint16_t addr);
uint32_t sms_color(uint8_t cram_value);               // ARGB8888
bool sms_irq_line(sms_t* s);

size_t sms_state_size(void);
void sms_save_state(const sms_t* s, void* out);
bool sms_load_state(sms_t* s, const void* in, size_t size);

#ifdef __cplusplus
}
#endif
