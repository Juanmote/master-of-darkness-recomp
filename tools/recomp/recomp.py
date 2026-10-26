#!/usr/bin/env python3
"""Static recompiler: Master of Darkness (SMS) Z80 code -> C.

Every instruction is turned into C by specialising the instruction templates of
src/core/z80_impl.h (the same code the interpreter runs) with the operand bytes
read from the ROM. Control flow goes through a per-region dispatcher, so the
generated code keeps the exact behaviour of the original, including stack tricks.

usage: recomp.py ROM COVERAGE.bin OUT_DIR
  COVERAGE.bin: one byte per ROM offset, bit0 = an instruction started there
  (produced by tools/headless -c). Static analysis extends it.
"""
import re
import sys
import os
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
IMPL = os.path.join(HERE, '..', '..', 'src', 'core', 'z80_impl.h')

# ---------------------------------------------------------------- template parsing

def parse_table(src, name):
    m = re.search(r'static const uint8_t %s\[256\] = \{(.*?)\};' % name, src, re.S)
    vals = [int(v) for v in re.findall(r'\d+', m.group(1))]
    assert len(vals) == 256, name
    return vals


def function_body(src, signature_start):
    i = src.index(signature_start)
    i = src.index('{', i)
    depth = 0
    for j in range(i, len(src)):
        if src[j] == '{':
            depth += 1
        elif src[j] == '}':
            depth -= 1
            if depth == 0:
                return src[i + 1:j]
    raise ValueError(signature_start)


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


def parse_switch(fbody):
    """Returns {opcode: body} for the top-level 'switch (opcode)' in a function body."""
    fbody = strip_comments(fbody)
    i = fbody.index('switch (opcode)')
    i = fbody.index('{', i)
    depth = 0
    end = None
    for j in range(i, len(fbody)):
        if fbody[j] == '{':
            depth += 1
        elif fbody[j] == '}':
            depth -= 1
            if depth == 0:
                end = j
                break
    sw = fbody[i + 1:end]
    # split at labels that are at depth 0 of the switch block
    entries = []
    depth = 0
    pos = 0
    labels = []
    k = 0
    while k < len(sw):
        c = sw[k]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        elif depth == 0:
            m = re.match(r'case\s+(0x[0-9A-Fa-f]+)\s*:', sw[k:])
            if m and (k == 0 or not (sw[k - 1].isalnum() or sw[k - 1] == '_')):
                entries.append((pos, k, labels))
                labels = [int(m.group(1), 16)]
                k += m.end()
                pos = k
                continue
            m = re.match(r'default\s*:', sw[k:])
            if m and (k == 0 or not (sw[k - 1].isalnum() or sw[k - 1] == '_')):
                entries.append((pos, k, labels))
                labels = ['default']
                k += m.end()
                pos = k
                continue
        k += 1
    entries.append((pos, len(sw), labels))
    table = {}
    pending = []
    for start, stop, labs in entries:
        body = sw[start:stop].strip()
        if not labs:
            continue
        pending.extend(labs)
        if body == '':
            continue  # fall through to the next label
        body = re.sub(r'break\s*;\s*$', '', body).strip()
        for l in pending:
            table[l] = body
        pending = []
    return table


SRC = open(IMPL).read()
CYC_00 = parse_table(SRC, 'cyc_00')
CYC_ED = parse_table(SRC, 'cyc_ed')
CYC_DDFD = parse_table(SRC, 'cyc_ddfd')
T_MAIN = parse_switch(function_body(SRC, 'void exec_opcode(z80* const z, uint8_t opcode) {'))
T_ED = parse_switch(function_body(SRC, 'void exec_opcode_ed(z80* const z, uint8_t opcode) {'))
T_DDFD = parse_switch(function_body(SRC, 'void exec_opcode_ddfd(z80* const z, uint8_t opcode, uint16_t* const iz) {'))

# ---------------------------------------------------------------- specialisation

class Fetch:
    def __init__(self, read, addr):
        self.read = read
        self.addr = addr
        self.start = addr

    def b(self):
        v = self.read(self.addr)
        self.addr = (self.addr + 1) & 0xFFFF
        return v

    def w(self):
        lo = self.b()
        hi = self.b()
        return lo | (hi << 8)


def subst_operands(body, f, iz=None):
    """Replace operand fetches in textual order with constants."""
    if iz:
        body = body.replace('IZD', 'displace(z, *iz, nextb(z))')
        body = body.replace('IZH', '(*iz >> 8)').replace('IZL', '(*iz & 0xFF)')
    out = []
    pos = 0
    pat = re.compile(r'nextb\(z\)|nextw\(z\)|cond_jump\(z, |cond_call\(z, |cond_jr\(z, ')
    pending_suffix = []
    while True:
        m = pat.search(body, pos)
        if not m:
            break
        out.append(body[pos:m.start()])
        tok = m.group(0)
        if tok == 'nextb(z)':
            out.append('0x%02X' % f.b())
            pos = m.end()
        elif tok == 'nextw(z)':
            out.append('0x%04X' % f.w())
            pos = m.end()
        else:
            # cond_xxx(z, COND) -> cond_xxx_c(z, COND, OPERAND); the helper fetched the operand itself
            depth = 1
            k = m.end()
            while depth:
                if body[k] == '(':
                    depth += 1
                elif body[k] == ')':
                    depth -= 1
                k += 1
            cond = body[m.end():k - 1]
            name = tok.split('(')[0]
            if name == 'cond_jr':
                operand = '(int8_t)0x%02X' % f.b()
            else:
                operand = '0x%04X' % f.w()
            out.append('%s_c(z, %s, %s)' % (name, cond, operand))
            pos = k
    out.append(body[pos:])
    body = ''.join(out)
    if iz:
        body = body.replace('*iz', 'z->%s' % iz)
        body = re.sub(r'\biz\b', '(&z->%s)' % iz, body)
    return body


def spec_main(op, f):
    code = 'z->cyc += %d; inc_r(z);\n' % CYC_00[op]
    if op == 0xED:
        return code + spec_ed(f.b(), f)
    if op in (0xDD, 0xFD):
        return code + spec_ddfd(f.b(), f, 'ix' if op == 0xDD else 'iy')
    if op == 0xCB:
        return code + 'exec_opcode_cb(z, 0x%02X);' % f.b()
    body = T_MAIN.get(op, T_MAIN['default'])
    body = subst_operands(body, f)
    body = re.sub(r'\bopcode\b', '0x%02X' % op, body)
    if op == 0xFB:  # ei: process_interrupts() at the end of the step consumes the delay
        body += '\nz->iff_delay = 0; z->iff1 = 1; z->iff2 = 1;'
    return code + body


def spec_ed(op, f):
    body = T_ED.get(op, T_ED['default'])
    body = subst_operands(body, f)
    body = re.sub(r'\bopcode\b', '0x%02X' % op, body)
    return 'z->cyc += %d; inc_r(z);\n' % CYC_ED[op] + body


def spec_ddfd(op, f, iz):
    code = 'z->cyc += %d; inc_r(z);\n' % CYC_DDFD[op]
    if op in T_DDFD:
        body = subst_operands(T_DDFD[op], f, iz)
        body = re.sub(r'\bopcode\b', '0x%02X' % op, body)
        return code + body
    # default: behaves as the unprefixed opcode (and R is not incremented twice)
    inner = spec_main(op, f)
    return code + '{\n' + inner + '\n}\nz->r = (z->r & 0x80) | ((z->r - 1) & 0x7f);'


FLOW_RE = re.compile(r'\bjump\(|cond_jump_c\(|\bcall\(|cond_call_c\(|\bret\(|cond_ret\(|\bjr\(|cond_jr_c\(|z->pc\b|halted')


class Insn:
    __slots__ = ('addr', 'length', 'code', 'flow', 'targets', 'uncond', 'text')


def decode(read, addr):
    f = Fetch(read, addr)
    op = f.b()
    code = spec_main(op, f)
    ins = Insn()
    ins.addr = addr
    ins.length = (f.addr - addr) & 0xFFFF
    ins.code = code
    ins.flow = bool(FLOW_RE.search(code))
    nxt = (addr + ins.length) & 0xFFFF
    ins.targets = set()
    for m in re.finditer(r'\b(?:jump|call)\(z, (0x[0-9A-F]+)\)', code):
        ins.targets.add(int(m.group(1), 16))
    for m in re.finditer(r'cond_(?:jump|call)_c\(z, .*?, (0x[0-9A-F]{4})\)', code):
        ins.targets.add(int(m.group(1), 16))
    for m in re.finditer(r'cond_jr_c\(z, .*?, \(int8_t\)(0x[0-9A-F]{2})\)', code):
        d = int(m.group(1), 16)
        ins.targets.add((nxt + (d - 256 if d > 127 else d)) & 0xFFFF)
    m = re.search(r'z->pc \+= \(int8_t\) (0x[0-9A-F]{2})', code)
    if m:
        d = int(m.group(1), 16)
        ins.targets.add((nxt + (d - 256 if d > 127 else d)) & 0xFFFF)
    if 'z->pc -= 2' in code:
        ins.targets.add(addr)
    # unconditional transfers (no fall-through), decided from the opcode bytes
    b0, b1 = read(addr), read((addr + 1) & 0xFFFF)
    ins.uncond = (b0 in (0xC3, 0x18, 0xE9, 0xC9)
                  or (b0 in (0xDD, 0xFD) and b1 == 0xE9)
                  or (b0 == 0xED and b1 in (0x45, 0x4D, 0x55, 0x5D, 0x65, 0x6D, 0x75, 0x7D)))
    return ins

# ---------------------------------------------------------------- discovery

def main():
    rom = open(sys.argv[1], 'rb').read()
    cov = open(sys.argv[2], 'rb').read()
    outdir = sys.argv[3]
    nbanks = len(rom) // 0x4000

    # region key: 'F' (fixed 0x0000-0x7FFF, banks 0/1) or bank number for 0x8000-0xBFFF
    def reader(region):
        if region == 'F':
            return lambda a: rom[a] if a < 0x8000 else 0xFF
        base = region * 0x4000
        return lambda a: rom[base + (a - 0x8000)] if 0x8000 <= a < 0xC000 else 0xFF

    def in_region(region, a):
        return a < 0x8000 if region == 'F' else 0x8000 <= a < 0xC000

    insns = defaultdict(dict)
    work = []
    seeds = defaultdict(set)

    def add(region, a):
        if not in_region(region, a) or a in insns[region]:
            return
        work.append((region, a))

    for v in [0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x66]:
        add('F', v)
    for off, flag in enumerate(cov):
        if flag & 1:
            if off < 0x8000:
                add('F', off)
                seeds['F'].add(off)
            else:
                add(off >> 14, 0x8000 + (off & 0x3FFF))
                seeds[off >> 14].add(0x8000 + (off & 0x3FFF))

    tables = 0
    while work:
        region, a = work.pop()
        if a in insns[region]:
            continue
        rd = reader(region)
        ins = decode(rd, a)
        insns[region][a] = ins
        nxt = (a + ins.length) & 0xFFFF
        if not ins.uncond:
            add(region, nxt)
        for t in ins.targets:
            if t < 0x8000:
                add('F', t)
            elif t < 0xC000 and region != 'F':
                add(region, t)
        # jump tables: LD HL,nn ; JP/CALL 0x0006
        if rd(a) == 0x21:
            tbl = rd(a + 1) | (rd(a + 2) << 8)
            if rd(a + 3) in (0xC3, 0xCD) and rd(a + 4) == 0x06 and rd(a + 5) == 0x00:
                tregion = 'F' if tbl < 0x8000 else (region if region != 'F' else None)
                if tregion is not None and in_region(tregion, tbl):
                    trd = reader(tregion)
                    for k in range(64):
                        e = trd(tbl + 2 * k) | (trd(tbl + 2 * k + 1) << 8)
                        if e < 0x0080 or not (e < 0x8000 or (tregion != 'F' and 0x8000 <= e < 0xC000)):
                            break
                        if tbl <= e < tbl + 2 * (k + 1):
                            break
                        add('F' if e < 0x8000 else tregion, e)
                        tables += 1

    os.makedirs(outdir, exist_ok=True)
    regions = sorted(insns.keys(), key=lambda r: -1 if r == 'F' else r)
    total = 0
    stats = []
    for region in regions:
        name = 'fixed' if region == 'F' else 'bank%02d' % region
        items = sorted(insns[region].values(), key=lambda i: i.addr)
        total += len(items)
        stats.append('%s: %d instrucciones (%d vistas en ejecución)' % (name, len(items), len(seeds[region])))
        lines = ['// Generated by tools/recomp/recomp.py - do not edit.',
                 '#include "../src/recomp/recomp_rt.h"', '',
                 'int recomp_%s(sms_t* s, z80* const z) {' % name,
                 'dispatch:',
                 '  if (rt_should_exit(s, z)) return 1;',
                 '  switch (z->pc) {']
        for ins in items:
            lines.append('  case 0x%04X: goto L_%04X;' % (ins.addr, ins.addr))
        lines.append('  default: return 0;')
        lines.append('  }')
        for idx, ins in enumerate(items):
            nxt = (ins.addr + ins.length) & 0xFFFF
            lines.append('L_%04X: {' % ins.addr)
            if ins.flow:
                lines.append('  z->pc = 0x%04X;' % nxt)
            for l in ins.code.split('\n'):
                lines.append('  ' + l.strip())
            if ins.flow:
                lines.append('  goto dispatch; }')
            else:
                # Leave at the same instruction boundary where the interpreter would take
                # an interrupt: after port accesses (they can raise/lower the VDP IRQ line),
                # after the instruction that follows EI, and when the line's cycles run out.
                prev = items[idx - 1] if idx else None
                after_ei = prev is not None and (prev.addr + prev.length) & 0xFFFF == ins.addr and 'z->iff_delay = 0' in prev.code
                if 'z->iff_delay = 0; z->iff1 = 1' in ins.code:
                    pass  # EI: the next instruction always runs before an interrupt
                elif 'port_in' in ins.code or 'port_out' in ins.code or 'outi(' in ins.code or 'ini(' in ins.code or after_ei:
                    lines.append('  if (rt_should_exit(s, z)) { z->pc = 0x%04X; return 1; }' % nxt)
                else:
                    lines.append('  if (z->cyc >= rt_deadline) { z->pc = 0x%04X; return 1; }' % nxt)
                lines.append('}')
                follows = idx + 1 < len(items) and items[idx + 1].addr == nxt
                if not follows:
                    lines.append('  z->pc = 0x%04X; goto dispatch;' % nxt)
        lines.append('}')
        open(os.path.join(outdir, 'recomp_%s.c' % name), 'w').write('\n'.join(lines) + '\n')

    # region table
    lines = ['// Generated by tools/recomp/recomp.py - do not edit.', '#include "../src/recomp/recomp_rt.h"', '']
    for region in regions:
        name = 'fixed' if region == 'F' else 'bank%02d' % region
        lines.append('int recomp_%s(sms_t* s, z80* const z);' % name)
    lines.append('')
    lines.append('recomp_fn recomp_fixed_region = recomp_fixed;')
    lines.append('recomp_fn recomp_bank_regions[%d] = {' % nbanks)
    for b in range(nbanks):
        lines.append('  %s,' % ('recomp_bank%02d' % b if b in insns else 'NULL'))
    lines.append('};')
    lines.append('const int recomp_bank_count = %d;' % nbanks)
    lines.append('const int recomp_instruction_count = %d;' % total)
    open(os.path.join(outdir, 'recomp_regions.c'), 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(stats))
    print('total', total, 'instructions; jump table entries', tables)


if __name__ == '__main__':
    main()
