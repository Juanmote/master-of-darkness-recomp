// Runtime support for the recompiled code (generated/recomp_*.c).
#pragma once
#include "../core/sms.h"
#include "../core/z80_impl.h"

typedef int (*recomp_fn)(sms_t* s, z80* const z);

extern unsigned long rt_deadline;
extern unsigned long rt_entry_cyc;
extern int rt_single;

// Conditional transfers with the operand already known (the interpreter's
// versions fetch it from memory). Same behaviour as cond_jump/cond_call/cond_jr.
static inline void cond_jump_c(z80* const z, bool condition, uint16_t addr) {
  if (condition) jump(z, addr);
  z->mem_ptr = addr;
}
static inline void cond_call_c(z80* const z, bool condition, uint16_t addr) {
  if (condition) { call(z, addr); z->cyc += 7; }
  z->mem_ptr = addr;
}
static inline void cond_jr_c(z80* const z, bool condition, int8_t d) {
  if (condition) { jr(z, d); z->cyc += 5; }
}

// Checked at every dispatch (after each control transfer).
static inline int rt_should_exit(sms_t* s, z80* const z) {
  if (z->cyc >= rt_deadline) return 1;
  if (rt_single && z->cyc != rt_entry_cyc) return 1;
  if (z->halted || z->nmi_pending) return 1;
  if (z->iff1 && sms_irq_line(s)) return 1;
  return 0;
}

extern recomp_fn recomp_fixed_region;
extern recomp_fn recomp_bank_regions[];
extern const int recomp_bank_count;
extern const int recomp_instruction_count;

// Runs recompiled code until the deadline / an interrupt / an unknown address.
// Returns 0 if nothing could be run (caller falls back to the interpreter).
int recomp_run(sms_t* s, unsigned long target);
