#!/usr/bin/env python3 -I
"""Convert every level file whose background tiles all resolve into <out>/<n>/ (via mklevel.py) and write <out>/count.txt, names.txt.
usage: mklevels.py <assets>/xml/LevelFiles <sprites dir with manifest.json> <out dir>"""
import sys, os, json, subprocess
sys.path.insert(0, os.path.dirname(__file__))
import bxml
d, sprites, out = sys.argv[1:4]
low = {k.lower() for k in json.load(open(os.path.join(sprites, 'manifest.json'))) if not k.startswith('unknown')}
# The regular rotation. The rest are licensed or seasonal event stages (and 'TV' duplicates) whose
# backgrounds are assembled from atlas fragments, which this simple strip-layer model draws as scattered bits.
KEEP = {'Room1', 'Room1City', 'Room1Forest', 'Room1Volcano', 'Room1Warehouse', 'warehouse', 'cave', 'caveVolcano', 'forest', 'hills', 'sewer'}
n = 0; names = []
for f in sorted(os.listdir(d)):
    p = os.path.join(d, f)
    if not os.path.isfile(p): continue
    if f[:-4] not in KEEP: continue
    try: root = bxml.parse(open(p, 'rb').read())[0]
    except Exception: continue
    tiles = []
    def walk(x):
        if x[0] == 'background' and x[2]: tiles.append(x[2][0][1][0][0].strip().lower())
        for c in x[2]: walk(c)
    walk(root)
    if not tiles or not all(t in low or 'textures/' + t in low for t in tiles): continue
    subprocess.check_call([sys.executable, '-I', os.path.join(os.path.dirname(__file__), 'mklevel.py'), p, sprites, os.path.join(out, str(n))], stdout=subprocess.DEVNULL)
    names.append(f); n += 1
open(os.path.join(out, 'count.txt'), 'w').write('%d\n' % n); open(os.path.join(out, 'names.txt'), 'w').write('\n'.join(names) + '\n')
print(n, 'levels')
