#!/usr/bin/env python3
"""Read main.prx memory by Ghidra address (image base 0).
usage: prxread.py ADDR COUNT [w|f|s]   (words as hex+float, or a C string)"""
import struct, sys
P = 'extracted/PSP_GAME/USRDIR/prx/main.prx'
d = open(P, 'rb').read()
phoff = struct.unpack_from('<I', d, 0x1c)[0]
segs = [struct.unpack_from('<8I', d, phoff + i * 32) for i in range(struct.unpack_from('<H', d, 0x2c)[0])]
def off(a):
    for t, o, v, _, fs, ms, *_ in segs:
        if v <= a < v + fs: return o + a - v
        if v <= a < v + ms: return None   # bss
    raise ValueError(hex(a))
def word(a):
    o = off(a); return 0 if o is None else struct.unpack_from('<I', d, o)[0]
if __name__ == '__main__':
    a = int(sys.argv[1], 16); n = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    if len(sys.argv) > 3 and sys.argv[3] == 's':
        o = off(a); print(d[o:d.index(b'\0', o)].decode('latin1')); sys.exit()
    for i in range(n):
        w = word(a + 4 * i); f = struct.unpack('<f', struct.pack('<I', w))[0]
        print(f'{a+4*i:08x}: {w:08x}  {f:.6g}')
