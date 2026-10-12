#!/usr/bin/env python3
"""PSM0 texture -> PNG (base mip level). Usage: psm2png.py [--linear] <in.psm>... <outdir>

Layout (sizes verified against all 3636 PSM files in gamedata.bin):
  0x00 'PSM0'
  0x04 u16 mipFlag (0 = none, 3 = mip chain follows the base level)
  0x06 u8  format, u8 flags (0x80 = swizzled?)   format = PSP GE pixel format:
                                                 1 = RGBA5551, 4 = CLUT4, 5 = CLUT8
  0x08 u16 width, u16 height
  0x0C u16 ?, u16 ?
  CLUT formats only:
  0x10 16-byte palette header (u16 x? ; entries/4 at +6)
  0x20 palette, RGBA8888 x (16 | 256)
  then pixels.
"""
import struct, sys, os, zlib


def write_png(path, w, h, rgba):
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def unswizzle(data, width_bytes, height):
    """PSP GE swizzle: 16-byte x 8-row blocks stored contiguously, row-major over blocks."""
    out = bytearray(len(data))
    bw = width_bytes // 16
    i = 0
    for by in range(0, height, 8):
        for bx in range(bw):
            for r in range(8):
                dst = (by + r) * width_bytes + bx * 16
                out[dst:dst + 16] = data[i:i + 16]
                i += 16
    return bytes(out)


def decode(d, swizzle=None):
    assert d[:4] == b'PSM0', 'not PSM0'
    fmt, flags = d[6], d[7]
    w, h = struct.unpack_from('<HH', d, 8)
    if swizzle is None:
        swizzle = bool(flags & 0x80)
    if fmt in (4, 5):
        ncol = 16 if fmt == 4 else 256
        pal = [d[0x20 + i * 4:0x24 + i * 4] for i in range(ncol)]
        off = 0x20 + ncol * 4
        bpp = 4 if fmt == 4 else 8
        wb = w * bpp // 8
        px = d[off:off + wb * h]
        if swizzle and wb >= 16:
            px = unswizzle(px, wb, h)
        out = bytearray()
        if fmt == 4:
            for b in px:
                out += pal[b & 15] + pal[b >> 4]
        else:
            for b in px:
                out += pal[b]
    elif fmt == 1:
        wb = w * 2
        px = d[0x10:0x10 + wb * h]
        if swizzle:
            px = unswizzle(px, wb, h)
        out = bytearray()
        for (v,) in struct.iter_unpack('<H', px):
            out += bytes(((v & 31) * 255 // 31, (v >> 5 & 31) * 255 // 31, (v >> 10 & 31) * 255 // 31,
                          255 if v & 0x8000 else 0))
    else:
        raise ValueError(f'unsupported format {fmt}')
    return w, h, bytes(out)


if __name__ == '__main__':
    args = sys.argv[1:]
    sw = None
    if args and args[0] in ('--linear', '--swizzled'):
        sw = args.pop(0) == '--swizzled'
    *ins, outdir = args
    os.makedirs(outdir, exist_ok=True)
    for p in ins:
        w, h, rgba = decode(open(p, 'rb').read(), sw)
        write_png(os.path.join(outdir, os.path.basename(p) + '.png'), w, h, rgba)
