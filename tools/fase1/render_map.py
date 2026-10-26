#!/usr/bin/env python3
"""Render the current level section held in RAM (D000, 192x10 metatiles) using VRAM/CRAM dumps.
usage: render_map.py rom dumpdir out.png"""
import sys
from PIL import Image
rom = open(sys.argv[1], 'rb').read()
d = sys.argv[2]
ram = open(d + '/ram.bin', 'rb').read(); vram = open(d + '/vram.bin', 'rb').read(); cram = open(d + '/cram.bin', 'rb').read()
R = lambda a: ram[a - 0xC000]
bank = R(0xC6A2); mt = R(0xC6AC) | R(0xC6AD) << 8
lv = [0, 85, 170, 255]
pal = [(lv[c & 3], lv[(c >> 2) & 3], lv[(c >> 4) & 3]) for c in cram]
def tile(t):
    im = Image.new('RGB', (8, 8)); px = im.load()
    for y in range(8):
        b = vram[t * 32 + y * 4: t * 32 + y * 4 + 4]
        for x in range(8):
            bit = 7 - x
            c = sum(((b[i] >> bit) & 1) << i for i in range(4))
            px[x, y] = pal[c]
    return im
cache = {}
W = 192
img = Image.new('RGB', (W * 16, 160))
for row in range(10):
    for col in range(W):
        m = R(0xD000 + row * 0xC0 + col)
        o = bank * 0x4000 + (mt - 0x8000) + m * 4
        for k, (dx, dy) in enumerate([(0, 0), (8, 0), (0, 8), (8, 8)]):
            t = 256 + rom[o + k]
            if t not in cache: cache[t] = tile(t)
            img.paste(cache[t], (col * 16 + dx, row * 16 + dy))
img.save(sys.argv[3])
print('bank', bank, 'metatiles', hex(mt))
