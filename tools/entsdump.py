#!/usr/bin/env python3
"""Dump ENTS (ENT0) entity records as a property tree.

Layout (main.prx FUN_001d95ac / FUN_001db478 / FUN_001d975c):
  'ENT0' u32 size; payload: u32 count; u32 recordOffset[count]      (offsets from payload start)
  record: u32 ?; u32 nameOffset ("Class:..." string; class = text before ':');
          token stream: u32 kind, [u32 stringOffset]
             1, 2, 5 : value / key strings      3 : begin block (name)      4 : end block
             the record ends when the block depth goes below zero
Usage: entsdump.py <file.ents> [maxRecords]
"""
import struct, sys


def cstr(d, o):
    return d[o:d.index(b'\0', o)].decode('latin1')


def main():
    d = open(sys.argv[1], 'rb').read()
    lim = int(sys.argv[2]) if len(sys.argv) > 2 else 1 << 30
    assert d[:4] == b'ENT0'
    base = 8
    n, = struct.unpack_from('<I', d, base)
    offs = struct.unpack_from(f'<{n}I', d, base + 4)
    for r, ro in enumerate(offs[:lim]):
        p = base + ro
        a, name = struct.unpack_from('<II', d, p)
        print(f'#{r} [{a}] {cstr(d, base + name)}')
        p += 8
        depth = 0
        line = []
        while True:
            k, = struct.unpack_from('<I', d, p)
            if k in (1, 2, 5):
                v, = struct.unpack_from('<I', d, p + 4)
                line.append(f'{k}:{cstr(d, base + v)!r}')
                p += 8
            elif k == 3:
                v, = struct.unpack_from('<I', d, p + 4)
                if line: print('  ' * (depth + 1) + ' '.join(line)); line = []
                print('  ' * (depth + 1) + f'{cstr(d, base + v)} {{')
                depth += 1
                p += 8
            elif k == 4:
                if line: print('  ' * (depth + 1) + ' '.join(line)); line = []
                depth -= 1
                p += 4
                if depth < 0:
                    break
                print('  ' * (depth + 1) + '}')
            else:
                line.append(f'?{k}')
                p += 4
            if len(line) >= 6:
                print('  ' * (depth + 1) + ' '.join(line)); line = []


if __name__ == '__main__':
    main()
