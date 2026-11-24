#!/usr/bin/env python3 -I
"""Decode Halfbrick .tex files (raw packed pixels, no mips) to PNG.
Header: 'TEX\\1', u32 flags, 4x(bits,chan) @8, u16 w,h @0x14, u32 w @0x1c, u32 h @0x20, u32 size @0x30, data @0x34.
chan: 1=A 2=R 3=G 4=B 0=unused. Channels listed MSB-first within a little-endian pixel."""
import struct, sys, os
from PIL import Image


MOD = [[2, 8, -2, -8], [5, 17, -5, -17], [9, 29, -9, -29], [13, 42, -13, -42], [18, 60, -18, -60], [24, 80, -24, -80], [33, 106, -33, -106], [47, 183, -47, -183]]
def _cl(x): return 0 if x < 0 else 255 if x > 255 else x
def _etc1_block(d):
    hi, lo = struct.unpack('>II', d)
    diff = (hi >> 1) & 1; flip = hi & 1; t1 = (hi >> 5) & 7; t2 = (hi >> 2) & 7
    if diff:
        r, g, b = (hi >> 27) & 31, (hi >> 19) & 31, (hi >> 11) & 31
        dr, dg, db = (hi >> 24) & 7, (hi >> 16) & 7, (hi >> 8) & 7
        dr -= 8 if dr > 3 else 0; dg -= 8 if dg > 3 else 0; db -= 8 if db > 3 else 0
        e = lambda v: (v << 3) | (v >> 2)
        c1 = (e(r), e(g), e(b)); c2 = (e((r+dr) & 31), e((g+dg) & 31), e((b+db) & 31))
    else:
        e = lambda v: (v << 4) | v
        c1 = (e((hi >> 28) & 15), e((hi >> 20) & 15), e((hi >> 12) & 15)); c2 = (e((hi >> 24) & 15), e((hi >> 16) & 15), e((hi >> 8) & 15))
    out = {}
    for i in range(16):
        x, y = i >> 2, i & 3
        sel = ((lo >> (16+i)) & 1) * 2 + ((lo >> i) & 1)
        sub = (x < 2) if not flip else (y < 2)
        c, t = (c1, t1) if sub else (c2, t2)
        m = MOD[t][sel]; out[(x, y)] = tuple(_cl(v+m) for v in c)
    return out
def etc1_alpha(data, w, h):
    """flag 6: ETC1 RGB plane (w*h/2 bytes) then 8-bit raster alpha plane (w*h bytes)."""
    im = Image.new('RGBA', (w, h)); p = im.load(); o = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            for (x, y), c in _etc1_block(data[o:o+8]).items():
                p[bx+x, by+y] = c + (data[w*h//2 + (by+y)*w + bx+x],)
            o += 8
    return im

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
    if flags == 6 and w % 4 == 0 and h % 4 == 0 and size == w*h*3//2:
        return etc1_alpha(data, w, h), flags, None
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
