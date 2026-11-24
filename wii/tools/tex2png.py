#!/usr/bin/env python3 -I
"""Decode Halfbrick .tex files (raw packed pixels, no mips) to PNG.
Header: 'TEX\\1', u32 flags, 4x(bits,chan) @8, u16 w,h @0x14, u32 w @0x1c, u32 h @0x20, u32 size @0x30, data @0x34.
chan: 1=A 2=R 3=G 4=B 0=unused. Channels listed MSB-first within a little-endian pixel."""
import struct, sys, os
from PIL import Image

def decode(b):
    assert b[:4] == b'TEX\1'
    flags = struct.unpack_from('<I', b, 4)[0]
    spec = [(b[8+2*i], b[9+2*i]) for i in range(4)]
    spec = [s for s in spec if s[0]]
    w, h = struct.unpack_from('<II', b, 0x1c)
    size = struct.unpack_from('<I', b, 0x30)[0]
    data = b[0x34:0x34+size]
    bpp = sum(s[0] for s in spec)
    nb = (bpp + 7)//8
    if w*h*nb != size:
        return None, flags, (w, h, size, spec)
    out = bytearray(w*h*4)
    for i in range(w*h):
        v = int.from_bytes(data[i*nb:(i+1)*nb], 'little')
        px = [0, 0, 0, 255]  # R G B A
        shift = bpp
        for bits, ch in spec:
            shift -= bits
            x = (v >> shift) & ((1 << bits)-1)
            px[{2: 0, 3: 1, 4: 2, 1: 3}[ch]] = x*255//((1 << bits)-1)
        out[i*4:i*4+4] = bytes(px)
    return Image.frombytes('RGBA', (w, h), bytes(out)), flags, None

if __name__ == '__main__':
    src, dst = sys.argv[1:3]
    bad = 0; n = 0
    for r, _, fs in os.walk(src):
        for f in fs:
            if not f.endswith('.tex'): continue
            p = os.path.join(r, f); n += 1
            im, fl, info = decode(open(p, 'rb').read())
            if im is None:
                bad += 1; print('UNSUPPORTED', p, fl, info); continue
            o = os.path.join(dst, os.path.relpath(p, src))[:-4] + '.png'
            os.makedirs(os.path.dirname(o), exist_ok=True); im.save(o)
    print(n, 'tex,', bad, 'unsupported')
