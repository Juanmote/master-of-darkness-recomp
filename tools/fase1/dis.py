#!/usr/bin/env python3
"""Disassemble a range of the ROM.
usage: dis.py rom.sms ADDR [count] [--bank N]
ADDR is the CPU address (hex). For 0x4000-0x7FFF the bank defaults to 1,
for 0x8000-0xBFFF use --bank to choose the ROM bank mapped in slot 2."""
import sys
from z80dis import z80

rom = open(sys.argv[1], 'rb').read()
addr = int(sys.argv[2], 16)
count = int(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith('--') else 40
bank = None
if '--bank' in sys.argv:
    bank = int(sys.argv[sys.argv.index('--bank') + 1], 0)

def off(a):
    if a < 0x4000: return a
    if a < 0x8000: return (bank if bank is not None and a >= 0x4000 and False else 1) * 0x4000 + a - 0x4000
    return (bank if bank is not None else 2) * 0x4000 + a - 0x8000

a = addr
for _ in range(count):
    o = off(a)
    data = rom[o:o + 4]
    try:
        txt = z80.disasm(data, a)
        n = z80.decode(data, a).len
    except Exception:
        txt, n = 'db %02X' % data[0], 1
    print('%04X  %-12s %s' % (a, rom[o:o + n].hex(' '), txt))
    a += n
