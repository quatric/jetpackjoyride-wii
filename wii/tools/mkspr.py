#!/usr/bin/env python3 -I
"""PNG -> .spr for the Wii app: 'SPR0', u16 w,h (padded to x4), u16 realw,realh, GX_TF_RGBA8 tiled data."""
import sys, struct
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGBA'); rw, rh = im.size
w, h = (rw+3)//4*4, (rh+3)//4*4
pad = Image.new('RGBA', (w, h), (0, 0, 0, 0)); pad.paste(im, (0, 0)); px = pad.load()
out = bytearray()
for ty in range(0, h, 4):
    for tx in range(0, w, 4):
        ar = bytearray(); gb = bytearray()
        for y in range(4):
            for x in range(4):
                r, g, b, a = px[tx+x, ty+y]; ar += bytes((a, r)); gb += bytes((g, b))
        out += ar + gb
open(sys.argv[2], 'wb').write(b'SPR0' + struct.pack('>4H', w, h, rw, rh) + out)
