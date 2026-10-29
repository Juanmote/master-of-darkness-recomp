#!/usr/bin/env python3
"""Exports the game's graphics (from the user's own ROM, via emulator dumps) as sheets to redraw in HD.

usage: export_textures.py rom out_dir section_dump... --seq name:file...
  section dump: directory with ram.bin/vram.bin/cram.bin taken at the start of a section (headless -o)
  --seq: VRAM sequences (headless VRAMDUMP) to collect the sprite tiles seen while playing

Per section (R1-1 ... R5-1):
  mapa_completo.png      whole section as the game builds it (192x10 metatiles)
  fondo_tiles.png        the 256 background tiles of the section, 16x16 grid of 8x8 tiles
  fondo_tiles_x4.png     same, 4x bigger: template to draw the HD version (each cell 32x32)
  fondo_indice.txt       cell -> VRAM tile and content hash (used by the HD loader)
  sprites_tiles(_x4).png / sprites_indice.txt: sprite tiles seen while playing that section
"""
import sys, os, hashlib
from PIL import Image

LV = [0, 85, 170, 255]
def color(c): return (LV[c & 3], LV[(c >> 2) & 3], LV[(c >> 4) & 3])

def tile_pixels(vram, t):
    out = []
    for y in range(8):
        b = vram[t * 32 + y * 4: t * 32 + y * 4 + 4]
        out.append([sum(((b[i] >> (7 - x)) & 1) << i for i in range(4)) for x in range(8)])
    return out

def tile_img(vram, t, pal, transparent=False):
    im = Image.new('RGBA', (8, 8))
    for y, row in enumerate(tile_pixels(vram, t)):
        for x, c in enumerate(row):
            im.putpixel((x, y), (0, 0, 0, 0) if (transparent and c == 0) else color(pal[c]) + (255,))
    return im

def tile_hash(vram, t): return hashlib.sha1(bytes(vram[t * 32: t * 32 + 32])).hexdigest()[:12]

def sheet(tiles, path, cols=16):
    rows = max(1, (len(tiles) + cols - 1) // cols)
    im = Image.new('RGBA', (cols * 8, rows * 8), (0, 0, 0, 0))
    for i, t in enumerate(tiles): im.paste(t, ((i % cols) * 8, (i // cols) * 8))
    im.save(path)
    im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(path.replace('.png', '_x4.png'))

def render_map(rom, ram, vram, cram):
    R = lambda a: ram[(a - 0xC000) & 0x1FFF]
    bank = R(0xC6A2); mt = R(0xC6AC) | R(0xC6AD) << 8
    cache = {}
    img = Image.new('RGB', (192 * 16, 160))
    for row in range(10):
        for col in range(192):
            m = R(0xD000 + row * 0xC0 + col)
            o = bank * 0x4000 + (mt - 0x8000) + m * 4
            for k, (dx, dy) in enumerate([(0, 0), (8, 0), (0, 8), (8, 8)]):
                t = 256 + rom[o + k]
                if t not in cache: cache[t] = tile_img(vram, t, cram[:16]).convert('RGB')
                img.paste(cache[t], (col * 16 + dx, row * 16 + dy))
    return img

def main():
    rom = open(sys.argv[1], 'rb').read(); out = sys.argv[2]
    args = sys.argv[3:]
    seqs = {}
    if '--seq' in args:
        i = args.index('--seq')
        for a in args[i + 1:]:
            name, f = a.split(':', 1); seqs.setdefault(name, []).append(f)
        args = args[:i]
    for d in args:
        ram = open(d + '/ram.bin', 'rb').read(); vram = open(d + '/vram.bin', 'rb').read(); cram = open(d + '/cram.bin', 'rb').read()
        name = 'R%d-%d' % (ram[0x1FC0] + 1, ram[0x1FC1] + 1)
        od = os.path.join(out, name); os.makedirs(od, exist_ok=True)
        render_map(rom, ram, vram, cram).save(os.path.join(od, 'mapa_completo.png'))
        tiles = [tile_img(vram, 256 + i, cram[:16]) for i in range(256)]
        sheet(tiles, os.path.join(od, 'fondo_tiles.png'))
        with open(os.path.join(od, 'fondo_indice.txt'), 'w') as f:
            f.write('# celda (fila,columna) tile_vram hash\n')
            for i in range(256): f.write('%d,%d %d %s\n' % (i // 16, i % 16, 256 + i, tile_hash(vram, 256 + i)))
        print(name)
    files = [f for v in seqs.values() for f in v]
    if files: sprites(out, files)

def sprites(out, files):
    # sprite tiles seen while playing (only those in the sprite table), one per pattern,
    # with the palette it showed most often; the same sheet for the whole game
    from collections import Counter, OrderedDict
    pats = OrderedDict()
    for f in files:
        data = open(f, 'rb').read(); step = 0x4000 + 33
        for k in range(0, len(data) - step + 1, step):
            v = data[k:k + 0x4000]; c = bytes(data[k + 0x4000:k + 0x4020])
            base = 256 if data[k + 0x4020] & 4 else 0
            used = set()
            for j in range(64):
                sy = v[0x3F00 + j]
                if sy == 0xD0: break
                if 0xC0 <= sy < 0xF0: continue
                used.add(base + v[0x3F81 + j * 2])
            for t in sorted(used):
                pat = bytes(v[t * 32:t * 32 + 32])
                if any(pat): pats.setdefault(pat, Counter())[c[16:32]] += 1
    od = os.path.join(out, 'sprites'); os.makedirs(od, exist_ok=True)
    spr, idx = [], []
    for pat, pals in pats.items():
        spr.append(tile_img(pat, 0, pals.most_common(1)[0][0], transparent=True))
        idx.append(hashlib.sha1(pat).hexdigest()[:12])
    sheet(spr, os.path.join(od, 'sprites_tiles.png'))
    with open(os.path.join(od, 'sprites_indice.txt'), 'w') as f:
        f.write('# celda (fila,columna) hash\n')
        for i, h in enumerate(idx): f.write('%d,%d %s\n' % (i // 16, i % 16, h))
    print('sprites', len(spr))

main()
