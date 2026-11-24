#!/usr/bin/env python3 -I
"""Level file (bxml) -> level.txt + background tiles, for the Wii app.
usage: mklevel.py <assets>/xml/LevelFiles/warehouse.xml <sprites dir with manifest.json> <out dir>
level.txt: 'S speed yOffset ntiles' (sets back-to-front by priority) then 'T file w h' per tile; tiles are CMPR .spr files."""
import sys, os, json, subprocess
sys.path.insert(0, os.path.dirname(__file__))
import bxml
lv, sprites, out = sys.argv[1:4]
man = json.load(open(os.path.join(sprites, 'manifest.json')))
low = {k.lower(): k for k in man}
def find(p):
    p = p.strip().lower()
    for k in (p, 'textures/' + p):
        if k in low: return low[k]
    return None
root = bxml.parse(open(lv, 'rb').read())[0]
sets = []
def walk(n):
    if n[0] == 'backgroundSet':
        a = dict(n[1]); tiles = [c[2][0][1][0][0] for c in n[2] if c[0] == 'background' and c[2]]
        sets.append((int(a.get('priority', 0)), float(a.get('paralaxSpeed', 1)), float(a.get('yOffset', 0)), tiles))
    for c in n[2]: walk(c)
walk(root)
os.makedirs(out, exist_ok=True); lines = []; si = 0
from PIL import Image
for pr, sp, yo, tiles in sorted(sets, key=lambda s: s[0]):
    ok = []
    for t in tiles:
        k = find(t)
        png = os.path.join(sprites, k[:-4] + '.png') if k else None
        if png and os.path.exists(png): ok.append((t, png))
        else: print('missing', t)
    if not ok: continue
    lines.append('S %g %g %d' % (sp, yo, len(ok)))
    for i, (t, png) in enumerate(ok):
        fn = 'lv%d_%d.spr' % (si, i); w, h = Image.open(png).size
        subprocess.check_call([sys.executable, '-I', os.path.join(os.path.dirname(__file__), 'mkspr.py'), '--cmpr', png, os.path.join(out, fn)])
        lines.append('T %s %d %d' % (fn, w, h))
    si += 1
open(os.path.join(out, 'level.txt'), 'w').write('\n'.join(lines) + '\n'); print(si, 'sets')
