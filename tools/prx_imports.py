#!/usr/bin/env python3
"""List PSP imports of a PRX/ELF: library, NID, stub address.  Usage: prx_imports.py <prx> > imports.tsv

.lib.stub entries (SceLibStubTable): u32 libName, u16 version, u16 attr, u8 entLen, u8 varCount,
u16 funcCount, u32 nidTable, u32 stubTable[, u32 varTable]. Pointers are module-relative; .rel.lib.stub
relocates them, so for an un-relocated PRX they are plain virtual addresses.
"""
import struct, sys


def main():
    d = open(sys.argv[1], 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shnum, shstrndx = struct.unpack_from('<HH', d, 0x30)
    secs = [struct.unpack_from('<10I', d, shoff + i * 40) for i in range(shnum)]
    st = secs[shstrndx]
    name = lambda o: d[st[4] + o:d.index(b'\0', st[4] + o)].decode()
    # virtual address -> file offset through the section table
    def off(va):
        for s in secs:
            if s[1] == 1 and s[3] <= va < s[3] + s[5]:
                return s[4] + va - s[3]
        raise ValueError(hex(va))
    stub = next(s for s in secs if name(s[0]) == '.lib.stub')
    pos, end = stub[4], stub[4] + stub[5]
    print('lib\tnid\tstub')
    while pos < end:
        lib_p, ver, attr, entlen, varc, funcc, nid_p, stub_p = struct.unpack_from('<IHHBBHII', d, pos)
        lib = d[off(lib_p):d.index(b'\0', off(lib_p))].decode()
        for i in range(funcc):
            nid, = struct.unpack_from('<I', d, off(nid_p) + i * 4)
            print(f'{lib}\t{nid:08X}\t{stub_p + i * 8:08x}')
        pos += entlen * 4


if __name__ == '__main__':
    main()
