#!/usr/bin/env python3
"""Builds our Spanish translation (es.txt) into src/app/text_es.h.

usage: construir.py ROM.sms [es.txt] [--ips salida.ips]
The ROM (Master of Darkness Europe/Brazil, CRC 96FB4D4B) is only read to know where each text is and
how many bytes it has; the header holds nothing but our own text, written in the same places.
"""
import sys, os, re, zlib, unicodedata
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from segmentos import *

HERE = os.path.dirname(os.path.abspath(__file__))
ACCENTS = {'Á': 0xB0, 'É': 0xB1, 'Í': 0xB2, 'Ó': 0xB3, 'Ú': 0xB4, 'Ñ': 0xB5, '¿': 0xB6, '¡': 0xB7, 'Ü': 0xB8}
PUNCT = {' ': 0x5F, '.': 0x3A, ',': 0x3B, '!': 0x3E, '?': 0x3F, '-': 0x3C, "'": 0x5B, '"': 0x5C, '/': 0x5E, '=': 0x3D}

def enc(text, accents, where):
    out = bytearray()
    for ch in text.upper():
        if ch in ACCENTS:
            if accents: out.append(ACCENTS[ch]); continue
            if ch in '¿¡': continue                    # these screens cannot show them
            ch = unicodedata.normalize('NFD', ch)[0]   # Á -> A (the gameplay font has no accents)
        if ch in PUNCT: out.append(PUNCT[ch])
        elif '0' <= ch <= '9' or 'A' <= ch <= 'Z': out.append(ord(ch))
        else: sys.exit('%s: character not available: %r' % (where, ch))
    return bytes(out)

def read_es(path):
    secs = {}; cur = None
    for raw in open(path, encoding='utf-8'):
        line = raw.rstrip('\n')
        if line.startswith('#'): continue
        m = re.match(r'^\[(.+)\]\s*$', line)
        if m: cur = m.group(1); secs[cur] = []; continue
        if cur is None: continue
        if line.endswith('|'): line = line[:-1]         # '|' marks the end when a line ends in spaces
        secs[cur].append(line)
    for k in secs:   # blank lines at the end of a section are not part of it
        while secs[k] and secs[k][-1] == '': secs[k].pop()
    return secs

def xline(s, default_x):
    m = re.match(r'^(\d+):(.*)$', s)
    return (int(m.group(1)), m.group(2)) if m else (default_x, s)

ERRORS = []
def fail(msg): ERRORS.append(msg)

def build(rom, secs):
    patches = []   # (offset, bytes)
    report = []
    def take(name):
        if name not in secs: fail('missing section [%s]' % name); return []
        return secs.pop(name)
    # pages: same number of lines (the picture sits beside the first ones), same bytes
    covered = {}
    for code, a in PAGES:
        orig, end = parse_page(rom, a)
        budget = end - a
        # room on each row: the widest original line of its half (the picture sits beside one half)
        gmax = [max(x + len(t) for i, (x, t) in enumerate(orig) if (i < 4) == half) for half in (True, False)]
        width = [gmax[0] if i < 4 else gmax[1] for i in range(len(orig))]
        lines = [xline(l, 0) for l in take(code)]
        if len(lines) > len(orig): fail('[%s] %d lines, the page has %d' % (code, len(lines), len(orig))); continue
        lines += [(0, '')] * (len(orig) - len(lines))
        acc = code != 'OUIJA'
        data = [[x, bytearray(enc(t, acc, code))] for x, t in lines]
        if code == 'OUIJA':   # the board spells each word in the time of the original one
            for i, (x, t) in enumerate(data):
                if len(t) > len(orig[i][1]): fail('[OUIJA] word %d has %d letters, room for %d' % (i + 1, len(t), len(orig[i][1])))
        bad = False
        for i, (x, t) in enumerate(data):
            if t and x + len(t) > width[i]:
                fail('[%s] line %d too long (%d > %d): %s' % (code, i + 1, x + len(t), width[i], lines[i][1])); bad = True
        size = 1 + sum(len(t) for _, t in data) + 2 * (len(data) - 1) + 2
        if size > budget: fail('[%s] %d bytes over (%d of %d)' % (code, size - budget, size, budget)); continue
        if bad: continue
        spare = budget - size
        # The game does not clear the screen between the pages of a chapter: every row must cover
        # what the previous page wrote there (spaces from the same column), as the original does.
        prev = covered.get(code)
        if prev:
            for i in range(len(data)):
                ps, pe = prev[i] if i < len(prev) else (None, None)
                if ps is None: continue
                x, t = data[i]
                if not t: x = ps
                if ps < x: t[:0] = b'\x5F' * (x - ps); x = ps
                if x + len(t) < pe: t += b'\x5F' * (pe - x - len(t))
                data[i][0] = x
        size = 1 + sum(len(t) for _, t in data) + 2 * (len(data) - 1) + 2
        if size > budget:
            fail('[%s] %d bytes over once the previous page is covered' % (code, size - budget))
            if os.environ.get('VERBOSE'):
                for i, (x, t) in enumerate(data): fail('    %d %2d:%s|  (prev %s)' % (i, x, t.decode('latin1').replace('_', ' '), prev[i] if prev and i < len(prev) else '-'))
            continue
        spare = budget - size
        # the rest: spaces where text may go (rows 1-4 right of the picture, rows 5-9 within the width)
        for i in range(len(data) - 1, -1, -1):
            x, t = data[i]
            if i < 4 and x < 8 and not (orig[i][1] and orig[i][0] < 8): continue
            lim = width[i] if (i >= 4 or x >= 8) else orig[i][0] + len(orig[i][1])
            add = min(max(lim - (x + len(t)), 0), spare)
            t += b'\x5F' * add; spare -= add
        if spare: fail('[%s] cannot fill %d bytes' % (code, spare)); continue
        nxt = CHAIN.get(code)
        if nxt:   # what the next page has to cover: only the letters (spaces are already blank)
            covered[nxt] = [(x + len(t) - len(t.lstrip(b'\x5F')), x + len(t.rstrip(b'\x5F'))) if t.strip(b'\x5F') else (None, None)
                            for x, t in data]
        out = bytearray([data[0][0]]) + data[0][1]
        for x, t in data[1:]: out += bytes([0xFF, x]) + t
        out += b'\xFF\xFF'
        assert len(out) == budget
        patches.append((a, bytes(out))); report.append('%-6s %3d/%3d bytes' % (code, size, budget))
    # ending: words wrapped in lines of up to 24, same bytes
    orig, end = parse_scroll(rom, SCROLL)
    budget = end - SCROLL
    words = ' '.join(take('FINAL')).split()
    lines, cur = [], ''
    for w in words:
        if cur and len(enc(cur + ' ' + w, True, 'FINAL')) > TEXT_W: lines.append(cur); cur = w
        else: cur = (cur + ' ' + w).strip()
    lines.append(cur)
    data = [bytearray(enc(l, True, 'FINAL')) for l in lines]
    size = sum(1 + len(t) for t in data) + 1
    if size > budget: fail('[FINAL] %d bytes over' % (size - budget)); return patches, report
    spare = budget - size
    for t in data:
        add = min(TEXT_W - len(t), spare); t += b'\x5F' * add; spare -= add
    while spare >= 2:   # still room: blank lines at the end
        n = min(TEXT_W, spare - 1); data.append(bytearray(b'\x5F' * n)); spare -= n + 1
    if spare: data[-1] += b'\x5F'; spare = 0   # (cannot happen with lines shorter than 24)
    out = b''.join(bytes([len(t)]) + t for t in data) + b'\x00'
    assert len(out) == budget, (len(out), budget)
    patches.append((SCROLL, out)); report.append('FINAL  %d/%d bytes, %d lines (original %d)' % (size, budget, len(lines), len(orig)))
    # boss boxes (shown while playing: no accents), same lines and bytes per box
    for bx in parse_boxes(rom, *BOXES):
        name = 'JEFE %04X' % bx['start']
        lines = take(name)
        if len(lines) != len(bx['lines']): fail('[%s] needs %d lines' % (name, len(bx['lines']))); continue
        orig_size = bx['end'] - bx['start']
        data = []
        for (pre, t), l in zip(bx['lines'], lines):
            x, txt = xline(l, pre[-1])
            e = bytearray(enc(txt, False, name))
            if x + len(e) > 15: fail('[%s] line too long (%d): %s' % (name, x + len(e), txt))
            data.append([pre[:-1] + bytes([x]), e])
        tail = bytes(bx.get('tail', b''))
        size = sum(len(p) + len(t) for p, t in data) + len(tail)
        if size > orig_size: fail('[%s] %d bytes over' % (name, size - orig_size)); continue
        spare = orig_size - size
        for d in reversed(data):
            room = 15 - (d[0][-1] + len(d[1]))
            add = min(room, spare); d[1] += b'\x5F' * add; spare -= add
        if spare: fail('[%s] cannot fill %d bytes' % (name, spare)); continue
        out = b''.join(p + t for p, t in data) + tail
        assert len(out) == orig_size
        patches.append((bx['start'], out)); report.append('%s %d/%d' % (name, size, orig_size))
    # fixed fields
    for name, off, n in FIXED:
        lines = take(name)
        t = enc(lines[0] if lines else '', False, name)
        if len(t) > n: fail('[%s] %d characters, room for %d' % (name, len(t), n)); continue
        patches.append((off, t + b'\x5F' * (n - len(t))))
    if secs: fail('unknown sections: %s' % ', '.join(secs))
    return patches, report

def main():
    rom = open(sys.argv[1], 'rb').read()
    if zlib.crc32(rom) != 0x96FB4D4B: sys.exit('the ROM must be Master of Darkness (Europe, Brazil), CRC 96FB4D4B')
    es = next((a for a in sys.argv[2:] if a.endswith('.txt')), os.path.join(HERE, 'es.txt'))
    patches, report = build(rom, read_es(es))
    if ERRORS: sys.exit('\n'.join(ERRORS))
    print('\n'.join(report))
    # only the bytes that change are kept
    recs = []
    for off, data in patches:
        i = 0
        while i < len(data):
            if data[i] == rom[off + i]: i += 1; continue
            j = i
            while j < len(data) and (data[j] != rom[off + j] or (j + 1 < len(data) and data[j + 1] != rom[off + j + 1])): j += 1
            recs.append((off + i, data[i:j])); i = j
    total = sum(len(d) for _, d in recs)
    hdr = os.path.join(HERE, '..', '..', 'src', 'app', 'text_es.h')
    with open(hdr, 'w') as f:
        f.write('// Spanish translation of the game text, by the Re-Master-Ed project (tools/traduccion/es.txt).\n')
        f.write('// Generated by tools/traduccion/construir.py: %d changes, %d bytes. Do not edit by hand.\n' % (len(recs), total))
        f.write('// Applied on a copy of the original ROM (CRC 96FB4D4B); codes 0xB0-0xB8 are the extra letters (mod_text).\n')
        f.write('#pragma once\n#include <stdint.h>\n\n')
        f.write('static const uint8_t text_es_data[] = {\n')
        for off, d in recs:
            f.write('  0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X,' % (off >> 16, (off >> 8) & 255, off & 255, len(d) >> 8, len(d) & 255))
            f.write(''.join(' 0x%02X,' % b for b in d) + '\n')
        f.write('};\n// records: offset (3 bytes), length (2 bytes), data\n')
    print('%d changes, %d bytes -> %s' % (len(recs), total, os.path.normpath(hdr)))
    if '--ips' in sys.argv:
        p = bytearray(b'PATCH')
        for off, d in recs: p += off.to_bytes(3, 'big') + len(d).to_bytes(2, 'big') + d
        p += b'EOF'
        open(sys.argv[sys.argv.index('--ips') + 1], 'wb').write(p)

if __name__ == '__main__':
    main()
