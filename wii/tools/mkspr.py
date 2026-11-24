#!/usr/bin/env python3 -I
"""PNG -> .spr for the Wii app.
  default : 'SPR0', u16 w,h (padded to x4), u16 realw,realh, GX_TF_RGBA8 tiled data (4 bytes/px)
  --cmpr  : 'SPR1', same header (padded to x8), GX_TF_CMPR data (0.5 byte/px, 1-bit alpha) - for backgrounds/opaque art
usage: mkspr.py [--cmpr] in.png out.spr [x0 y0 x1 y1]   (optional crop box)"""
import sys, struct
from PIL import Image
args = [a for a in sys.argv[1:] if a != '--cmpr']; cmpr = '--cmpr' in sys.argv
im = Image.open(args[0]).convert('RGBA')
if len(args) > 5: im = im.crop(tuple(map(int, args[2:6])))
rw, rh = im.size
q = 8 if cmpr else 4
w, h = (rw+q-1)//q*q, (rh+q-1)//q*q
pad = Image.new('RGBA', (w, h), (0, 0, 0, 0)); pad.paste(im, (0, 0)); px = pad.load()

def c565(c): return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)
def un565(v): return (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
def dist(a, b): return sum((x-y)**2 for x, y in zip(a, b))

def block(x0, y0):
    P = [px[x0+i % 4, y0+i//4] for i in range(16)]
    op = [p[:3] for p in P if p[3] >= 128]; clear = len(op) < 16
    if not op: return struct.pack('>HHI', 0, 0xFFFF if False else 0, 0xFFFFFFFF)   # all transparent: c0<=c1, every index 3
    lo = tuple(min(c[k] for c in op) for k in range(3)); hi = tuple(max(c[k] for c in op) for k in range(3))
    a, b = c565(hi), c565(lo)
    if clear:   # c0 <= c1: 3 colours + transparent
        if a > b: a, b = b, a
    elif a < b: a, b = b, a
    if a == b and not clear: b = a ^ 1 if a & 1 == 0 else a - 1; a, b = (a, b) if a > b else (b, a)
    A, B = un565(a), un565(b)
    pal = [A, B, tuple((x+y)//2 for x, y in zip(A, B))] if clear else [A, B, tuple((2*x+y)//3 for x, y in zip(A, B)), tuple((x+2*y)//3 for x, y in zip(A, B))]
    bits = 0
    for i, p in enumerate(P):
        k = 3 if (p[3] < 128 and clear) else min(range(len(pal)), key=lambda j: dist(p[:3], pal[j]))
        row, col = i // 4, i % 4
        bits |= k << (30 - 2*(row*4 + col))   # GX: row-major, first pixel in the high bits
    return struct.pack('>HHI', a, b, bits)

out = bytearray()
if cmpr:
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            for by, bx in ((0, 0), (0, 4), (4, 0), (4, 4)): out += block(tx+bx, ty+by)
    magic = b'SPR1'
else:
    for ty in range(0, h, 4):
        for tx in range(0, w, 4):
            ar = bytearray(); gb = bytearray()
            for y in range(4):
                for x in range(4):
                    r, g, b, a = px[tx+x, ty+y]; ar += bytes((a, r)); gb += bytes((g, b))
            out += ar + gb
    magic = b'SPR0'
open(args[1], 'wb').write(magic + struct.pack('>4H', w, h, rw, rh) + out)
