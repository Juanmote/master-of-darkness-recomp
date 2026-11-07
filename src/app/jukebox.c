// The game's own music outside the game (music test on the start screen): a second, silent console runs
// only the game's sound driver (ROM bank 9, entry 8000, called once per frame from the game's interrupt)
// from a few bytes of code put in its RAM, and the track is asked for the way the game does it (C106).
// The game's own console is not touched.
#include "jukebox.h"
#include "../core/sms.h"
#include <string.h>

static sms_t J;
static bool on;
static float dc_x, dc_y;   // DC blocker: the console's output sits far from 0, which would click on start and stop
static bool dc_primed;

// at D800 (map RAM, which the driver never uses), with interrupts off:
//   DI ; LD SP,DFF0
//   loop: LD A,9 ; LD (FFFF),A     driver bank in slot 2
//         CALL 8000                one driver step
//   wait: IN A,(BF) ; AND 80 ; JR Z,wait     until the next frame (VDP frame flag)
//         JP loop
static const uint8_t stub[] = {
  0xF3, 0x31, 0xF0, 0xDF,
  0x3E, 0x09, 0x32, 0xFF, 0xFF,
  0xCD, 0x00, 0x80,
  0xDB, 0xBF, 0xE6, 0x80, 0x28, 0xFA,
  0xC3, 0x04, 0xD8,
};

void jukebox_play(const uint8_t* rom, int rom_size, int id) {
  sms_init(&J, rom, rom_size);
  memcpy(&J.st.ram[0x1800], stub, sizeof stub);
  J.st.cpu.pc = 0xD800;
  J.st.ram[0x0106] = (uint8_t)id;   // the track, as the game asks for it
  dc_primed = false;
  on = true;
}

void jukebox_stop(void) { on = false; }
bool jukebox_active(void) { return on; }

void jukebox_mix(int16_t* out, int n, int volume) {
  if (!on || n != SMS_SAMPLES_PER_FRAME) return;
  sms_run_frame(&J);
  if (!dc_primed) { dc_x = J.audio[0]; dc_y = 0; dc_primed = true; }
  for (int i = 0; i < n; i++) {
    float x = J.audio[i];
    dc_y = x - dc_x + 0.9995f * dc_y;
    dc_x = x;
    int v = out[i] + (int)(dc_y * volume / 10);
    out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
  }
}
