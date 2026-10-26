#include "recomp_rt.h"

unsigned long rt_deadline;
unsigned long rt_entry_cyc;
int rt_single;

int recomp_run(sms_t* s, unsigned long target) {
  z80* const z = &s->st.cpu;
  rt_deadline = target;
  rt_entry_cyc = z->cyc;
  int progress = 0;
  for (;;) {
    recomp_fn fn = NULL;
    uint16_t pc = z->pc;
    if (pc < 0x8000) {
      if (s->st.bank[0] == 0 && s->st.bank[1] == 1) fn = recomp_fixed_region;
    } else if (pc < 0xC000) {
      int b = s->st.bank[2] % (s->rom_size / 0x4000);
      if (b < recomp_bank_count) fn = recomp_bank_regions[b];
    }
    if (!fn) return progress;
    unsigned long c0 = z->cyc;
    if (fn(s, z)) return 1;
    if (z->cyc == c0) return progress;  // address not recompiled
    progress = 1;
  }
}
