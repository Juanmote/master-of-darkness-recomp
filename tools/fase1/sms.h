// Minimal Sega Master System core used as an analysis/reference harness.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../../third_party/z80/z80.h"

#define SMS_W 256
#define SMS_H 192
#define ROM_MAX (1 << 20)

enum { BTN_UP = 1, BTN_DOWN = 2, BTN_LEFT = 4, BTN_RIGHT = 8, BTN_1 = 16, BTN_2 = 32, BTN_PAUSE = 64 };

typedef struct {
  uint8_t vram[0x4000];
  uint8_t cram[32];
  uint8_t reg[16];
  uint16_t addr;
  uint8_t code;
  bool latch;
  uint8_t first;
  uint8_t buffer;
  uint8_t status;
  int line;
  int line_counter;
  bool line_irq;
  uint8_t vscroll_latched;
} vdp_t;

typedef struct sms sms_t;
// optional hooks for analysis tools
typedef struct {
  void (*on_exec)(sms_t*, uint16_t pc, int rom_off);
  void (*on_read)(sms_t*, uint16_t addr, int rom_off);
  void (*on_vdp_reg)(sms_t*, int reg, uint8_t val);
  void (*on_vram_write)(sms_t*, uint16_t addr, uint8_t val);
  void (*on_ram_write)(sms_t*, uint16_t addr, uint8_t val);
  void (*on_bank)(sms_t*, int slot, uint8_t bank);
} sms_hooks_t;

struct sms {
  z80 cpu;
  uint8_t rom[ROM_MAX];
  int rom_size;
  uint8_t ram[0x2000];
  uint8_t bank[3];
  uint8_t mapper_ctrl;
  uint8_t cart_ram[0x8000];
  vdp_t vdp;
  uint8_t input;      // active-high BTN_ bits
  uint8_t prev_input;
  uint32_t frame;
  uint32_t fb[SMS_W * SMS_H];        // ARGB
  uint8_t fb_idx[SMS_W * SMS_H];     // palette index (0..31)
  uint8_t line_hscroll[SMS_H];
  uint8_t line_vscroll[SMS_H];
  sms_hooks_t hooks;
  void* user;
};

void sms_init(sms_t* s, const uint8_t* rom, int size);
void sms_run_frame(sms_t* s);
int sms_rom_offset(sms_t* s, uint16_t addr); // -1 if not ROM
bool sms_save_png(sms_t* s, const char* path);
uint8_t sms_peek(sms_t* s, uint16_t addr);
