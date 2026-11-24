#!/usr/bin/env python3 -I
"""Self-checks for the converters (no APK needed): python3 -I wii/tools/test_tools.py"""
import os, struct, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import hashname
from PIL import Image

# FileStringHash ignores case and slash direction, and differs per path
assert hashname.file_string_hash('Textures\\Barry.PNG') == hashname.file_string_hash('textures/barry.png')
assert hashname.file_string_hash('textures/a.tex') != hashname.file_string_hash('textures/b.tex')

def spr(args, png):
    with tempfile.TemporaryDirectory() as d:
        p, o = os.path.join(d, 'in.png'), os.path.join(d, 'out.spr'); png.save(p)
        subprocess.check_call([sys.executable, '-I', os.path.join(here, 'mkspr.py')] + [a.replace('IN', p).replace('OUT', o) for a in args])
        return open(o, 'rb').read()

# RGBA8: 'SPR0', padded size to x4 and real size in the header, 4 bytes per padded pixel
b = spr(['IN', 'OUT'], Image.new('RGBA', (6, 5), (255, 0, 0, 255)))
assert b[:4] == b'SPR0' and struct.unpack('>HHHH', b[4:12]) == (8, 8, 6, 5) and len(b) == 12 + 8 * 8 * 4, (b[:12], len(b))
# CMPR: 'SPR1', padded to x8, half a byte per pixel
b = spr(['--cmpr', 'IN', 'OUT'], Image.new('RGBA', (10, 9), (0, 255, 0, 255)))
assert b[:4] == b'SPR1' and struct.unpack('>HHHH', b[4:12]) == (16, 16, 10, 9) and len(b) == 12 + 16 * 16 // 2, (b[:12], len(b))
# a crop box is applied before padding
b = spr(['IN', 'OUT', '0', '0', '4', '4'], Image.new('RGBA', (32, 32), (0, 0, 255, 255)))
assert struct.unpack('>HHHH', b[4:12]) == (4, 4, 4, 4)
print('ok')
