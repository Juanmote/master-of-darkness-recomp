#include "z80_impl.h"

// MARK: interface
// initialises a z80 struct. Note that read_byte, write_byte, port_in, port_out
// and userdata must be manually set by the user afterwards.
void z80_init(z80* const z) {
  z->read_byte = NULL;
  z->write_byte = NULL;
  z->port_in = NULL;
  z->port_out = NULL;
  z->userdata = NULL;

  z->cyc = 0;

  z->pc = 0;
  z->sp = 0xFFFF;
  z->ix = 0;
  z->iy = 0;
  z->mem_ptr = 0;

  // af and sp are set to 0xFFFF after reset,
  // and the other values are undefined (z80-documented)
  z->a = 0xFF;
  z->b = 0;
  z->c = 0;
  z->d = 0;
  z->e = 0;
  z->h = 0;
  z->l = 0;

  z->a_ = 0;
  z->b_ = 0;
  z->c_ = 0;
  z->d_ = 0;
  z->e_ = 0;
  z->h_ = 0;
  z->l_ = 0;
  z->f_ = 0;

  z->i = 0;
  z->r = 0;

  z->sf = 1;
  z->zf = 1;
  z->yf = 1;
  z->hf = 1;
  z->xf = 1;
  z->pf = 1;
  z->nf = 1;
  z->cf = 1;

  z->iff_delay = 0;
  z->interrupt_mode = 0;
  z->iff1 = 0;
  z->iff2 = 0;
  z->halted = 0;
  z->int_pending = 0;
  z->nmi_pending = 0;
  z->int_data = 0;
}

// executes the next instruction in memory + handles interrupts
void z80_step(z80* const z) {
  if (z->halted) {
    exec_opcode(z, 0x00);
  } else {
    const uint8_t opcode = nextb(z);
    exec_opcode(z, opcode);
  }

  process_interrupts(z);
}

// outputs to stdout a debug trace of the emulator
void z80_debug_output(z80* const z) {
  printf("PC: %04X, AF: %04X, BC: %04X, DE: %04X, HL: %04X, SP: %04X, "
         "IX: %04X, IY: %04X, I: %02X, R: %02X",
      z->pc, (z->a << 8) | get_f(z), get_bc(z), get_de(z), get_hl(z), z->sp,
      z->ix, z->iy, z->i, z->r);

  printf("\t(%02X %02X %02X %02X), cyc: %lu\n", rb(z, z->pc), rb(z, z->pc + 1),
      rb(z, z->pc + 2), rb(z, z->pc + 3), z->cyc);
}

// function to call when an NMI is to be serviced
void z80_gen_nmi(z80* const z) {
  z->nmi_pending = 1;
}

// function to call when an INT is to be serviced
void z80_gen_int(z80* const z, uint8_t data) {
  z->int_pending = 1;
  z->int_data = data;
}


// processes pending interrupts exactly like the end of z80_step (used by the recompiled runner)
void z80_process_interrupts(z80* const z) { process_interrupts(z); }

// executes one instruction without taking interrupts (used to verify recompiled blocks)
void z80_exec_one(z80* const z) {
  if (z->halted) exec_opcode(z, 0x00);
  else exec_opcode(z, nextb(z));
  if (z->iff_delay) { z->iff_delay = 0; z->iff1 = 1; z->iff2 = 1; }
}
