#!/usr/bin/env python3
"""Extractor for MediEvil: Resurrection (PSP) USRDIR/gamedata.bin.

Format (all offsets in 2048-byte sectors):
  TOCS header : 'TOCS' u32 0x30, u32 ?, u32 1, then N x {char name[32]; u32 tocSize; u32 tocSector}
                terminated by an empty name.
  TOC0 group  : 'TOC0' u32 nBlocks, u32 nAssets, u32 0
                Block[nBlocks] 8B : u24 sector, u8 flags, u16 assetIndex, u16 nextCopy (0xFFFF = none)
                Asset[nAssets] 20B: u32 nameHash, u32 size, char type[4],
                                    u16 kind (1 = file, 2 = group marker), u16 block,
                                    u16 bstLeft, u16 bstRight   (binary search tree keyed by nameHash)
  Blocks with assetIndex != their own index are duplicate copies of the same data laid out elsewhere
  on the disc for streaming; the extractor reads the primary copy (Asset.block).

Asset payloads are uncompressed; most begin with their own chunk magic (LND0, PSM0, SBZ0, ENT0, ...).

Usage:
  gamedata_extract.py gamedata.bin --list                 # list groups
  gamedata_extract.py gamedata.bin out/ [--group NAME...] [--names docs/names.tsv]
                                                          # extract (all groups by default)
"""
import argparse, csv, os, struct, sys

SECTOR = 2048


def read_groups(f):
    f.seek(0)
    hdr = f.read(SECTOR)
    if hdr[:4] != b'TOCS':
        sys.exit('not a TOCS archive')
    groups, pos = [], 16
    while pos + 40 <= len(hdr):
        name = hdr[pos:pos + 32].split(b'\0')[0]
        if not name:
            break
        size, sector = struct.unpack_from('<II', hdr, pos + 32)
        groups.append((name.decode(), size, sector))
        pos += 40
    return groups


def read_toc(f, size, sector):
    f.seek(sector * SECTOR)
    g = f.read(size)
    if g[:4] != b'TOC0':
        raise ValueError('bad TOC0 magic')
    n_blocks, n_assets = struct.unpack_from('<II', g, 4)
    blocks = [struct.unpack_from('<IHH', g, 16 + i * 8) for i in range(n_blocks)]
    base = 16 + n_blocks * 8
    assets = []
    for i in range(n_assets):
        h, sz, typ, kind, blk, left, right = struct.unpack_from('<II4sHHHH', g, base + i * 20)
        assets.append(dict(index=i, hash=h, size=sz, type=typ.rstrip(b'\0').decode('latin1'),
                           kind=kind, block=blk, left=left, right=right,
                           sector=blocks[blk][0] & 0xFFFFFF, flags=blocks[blk][0] >> 24))
    return blocks, assets


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('archive')
    ap.add_argument('out', nargs='?')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--group', action='append', help='group name (without .group.toc); repeatable')
    ap.add_argument('--names', help='hash->name table from namehash.py (hash<TAB>type<TAB>name)')
    a = ap.parse_args()

    names = {}
    if a.names:
        for line in open(a.names):
            h, _, n = line.rstrip('\n').split('\t')
            if h != 'hash':
                names[int(h, 16)] = n

    with open(a.archive, 'rb') as f:
        groups = read_groups(f)
        if a.list or not a.out:
            for gi, (name, size, sector) in enumerate(groups):
                n = read_toc(f, size, sector)[1] if size else []
                print(f'{gi:02d} {name:40s} sector={sector:7d} assets={len(n)}')
            return

        want = set(a.group or [])
        total = 0
        for gi, (name, size, sector) in enumerate(groups):
            # Names repeat in the second half of the table and can be cut at 32 chars,
            # so the table index keeps directories unique.
            gname = name.split('.group')[0]
            if (want and gname not in want) or not size:
                continue
            _, assets = read_toc(f, size, sector)
            gname = f'{gi:02d}_{gname}'
            gdir = os.path.join(a.out, gname)
            os.makedirs(gdir, exist_ok=True)
            with open(os.path.join(gdir, '_index.csv'), 'w', newline='') as idx:
                w = csv.writer(idx)
                w.writerow(['index', 'hash', 'name', 'type', 'size', 'kind', 'sector', 'flags', 'magic', 'file'])
                for e in assets:
                    magic, fn = '', ''
                    if e['kind'] == 1 and e['size']:
                        f.seek(e['sector'] * SECTOR)
                        data = f.read(e['size'])
                        magic = data[:4].decode('latin1') if data[:4].isascii() else data[:4].hex()
                        known = names.get(e['hash'])
                        fn = f"{e['index']:05d}_{known}" if known else \
                            f"{e['index']:05d}_{e['hash']:08x}.{e['type'].lower()}"
                        tdir = os.path.join(gdir, e['type'])
                        os.makedirs(tdir, exist_ok=True)
                        with open(os.path.join(tdir, fn), 'wb') as o:
                            o.write(data)
                        fn = f"{e['type']}/{fn}"
                        total += 1
                    w.writerow([e['index'], f"{e['hash']:08x}", names.get(e['hash'], ''), e['type'], e['size'], e['kind'],
                                e['sector'], e['flags'], magic, fn])
            print(f'{gname}: {len(assets)} entries')
        print(f'extracted {total} files')


if __name__ == '__main__':
    main()
