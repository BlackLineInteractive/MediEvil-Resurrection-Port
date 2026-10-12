#!/usr/bin/env python3
"""Extract entity/component templates registered in main.prx.

Every template is registered by a static initialiser:
    FUN_001d7f58(&obj, "name", &tokens, &info, size)          (CEntityTemplate ctor)
  tokens : same token stream as ENTS records but with absolute pointers:
           u32 kind, [u32 char*]   1/2/5 = string, 3 = begin block, 4 = end block;
           the whole stream is a single top-level block
           (0 for component templates that carry no defaults)
  info   : {char* name, u32 ?, char* baseType, ...}
Output (one block per template, ENTS-dump style):
    template <name> base=<baseType> size=<size>
      <token tree>
Usage: entity_templates.py <main.prx> <decomp_dir> > assets/entity_templates.txt
"""
import glob, os, re, struct, sys

REG = re.compile(r'FUN_001d7f58\(&DAT_[0-9a-f]+,"([^"]+)",(?:&DAT_([0-9a-f]+)|0),&PTR_s_[A-Za-z0-9_]*_([0-9a-f]{8}),(0x[0-9a-f]+|\d+)\)')


def main():
    prx, ddir = sys.argv[1:3]
    d = open(prx, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shnum, _ = struct.unpack_from('<HH', d, 0x30)
    secs = [struct.unpack_from('<10I', d, shoff + i * 40) for i in range(shnum)]

    def off(va):
        for s in secs:
            if s[1] == 1 and s[3] and s[3] <= va < s[3] + s[5]:
                return s[4] + va - s[3]
        return None

    def cstr(va):
        o = off(va)
        return None if o is None else d[o:d.index(b'\0', o)].decode('latin1')

    regs = {}
    for p in sorted(glob.glob(os.path.join(ddir, '*.c'))):
        for m in REG.finditer(open(p, errors='replace').read()):
            regs[m.group(1)] = (int(m.group(2), 16) if m.group(2) else 0, int(m.group(3), 16), int(m.group(4), 0))

    for name in sorted(regs):
        tok, info, size = regs[name]
        base = cstr(struct.unpack_from('<I', d, off(info) + 8)[0]) if off(info) is not None else None
        print(f'template {name} base={base} size={size:#x}')
        if not tok:
            continue
        o = off(tok)
        depth = 0
        while True:
            k, = struct.unpack_from('<I', d, o)
            if k in (1, 2, 5):
                print('  ' * (depth + 1) + f'{k} {cstr(struct.unpack_from("<I", d, o + 4)[0])}')
                o += 8
            elif k == 3:
                print('  ' * (depth + 1) + f'{{ {cstr(struct.unpack_from("<I", d, o + 4)[0])}')
                depth += 1
                o += 8
            elif k == 4:
                depth -= 1
                o += 4
                print('  ' * (depth + 1) + '}')
                if depth <= 0:   # the stream is one top-level block
                    break
            else:
                print('  ' * (depth + 1) + f'? {k}')
                break


if __name__ == '__main__':
    main()
