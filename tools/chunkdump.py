#!/usr/bin/env python3
"""Dump the chunk tree of a MediEvil asset.

Chunk = char tag[4]; [u32 0 for containers]; u32 size; payload.  Containers (tag followed by a zero
word) hold child chunks in their payload. Usage: chunkdump.py <file> [--hex N]
"""
import struct, sys


def is_tag(b):
    return len(b) == 4 and all(48 <= c < 123 or c in (32, 37, 95) for c in b)


def walk(d, off, end, depth, hexn, out):
    while off + 8 <= end:
        tag = d[off:off + 4]
        if not is_tag(tag):
            out.append(f"{'  ' * depth}@{off:#x} ?? {d[off:off + 16].hex(' ')}")
            return
        a, b = struct.unpack_from('<II', d, off + 4)
        if a == 0 and off + 12 + b <= end and b and is_tag(d[off + 12:off + 16]):
            out.append(f"{'  ' * depth}@{off:#x} {tag.decode()} [container {b:#x}]")
            walk(d, off + 12, off + 12 + b, depth + 1, hexn, out)
            off += 12 + b
        else:
            body = d[off + 8:off + 8 + a]
            txt = body.split(b'\0')[0].decode('latin1') if tag == b'NAME' else body[:hexn].hex(' ')
            out.append(f"{'  ' * depth}@{off:#x} {tag.decode()} {a:#x}: {txt}")
            off += 8 + a
    if off < end:
        out.append(f"{'  ' * depth}@{off:#x} trailing {end - off} bytes: {d[off:off + 16].hex(' ')}")


if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    hexn = int(sys.argv[3]) if len(sys.argv) > 3 else 24
    out = []
    walk(d, 0, len(d), 0, hexn, out)
    print('\n'.join(out))
