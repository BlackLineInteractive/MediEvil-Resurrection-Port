#!/usr/bin/env python3
"""Minimal ISO9660 extractor (PSP UMD images). Usage: iso_extract.py <iso> <outdir>"""
import os, struct, sys

SECTOR = 2048

def read_dir(f, lba, size, path, out, listing):
    f.seek(lba * SECTOR)
    data = f.read(size)
    pos = 0
    while pos < len(data):
        rlen = data[pos]
        if rlen == 0:  # pad to next sector
            pos = (pos // SECTOR + 1) * SECTOR
            continue
        rec = data[pos:pos + rlen]
        elba = struct.unpack_from('<I', rec, 2)[0]
        esize = struct.unpack_from('<I', rec, 10)[0]
        flags = rec[25]
        nlen = rec[32]
        name = rec[33:33 + nlen]
        pos += rlen
        if name in (b'\x00', b'\x01'):
            continue
        name = name.decode('ascii', 'replace').split(';')[0]
        p = os.path.join(path, name)
        if flags & 2:
            os.makedirs(os.path.join(out, p), exist_ok=True)
            read_dir(f, elba, esize, p, out, listing)
        else:
            listing.append((p, elba, esize))
            with open(os.path.join(out, p), 'wb') as o:
                f.seek(elba * SECTOR)
                rem = esize
                while rem:
                    chunk = f.read(min(rem, 1 << 20))
                    o.write(chunk)
                    rem -= len(chunk)
            f.seek(0)

def main():
    iso, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    listing = []
    with open(iso, 'rb') as f:
        f.seek(16 * SECTOR)
        pvd = f.read(SECTOR)
        assert pvd[1:6] == b'CD001'
        root = pvd[156:156 + 34]
        lba = struct.unpack_from('<I', root, 2)[0]
        size = struct.unpack_from('<I', root, 10)[0]
        read_dir(f, lba, size, '', out, listing)
    for p, l, s in sorted(listing):
        print(f'{l:8d} {s:12d} {p}')

if __name__ == '__main__':
    main()
