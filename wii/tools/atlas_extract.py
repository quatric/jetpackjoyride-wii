#!/usr/bin/env python3 -I
"""Slice every atlas page into per-sprite PNGs named by the .tex path whose hash matches.
usage: atlas_extract.py <assets dir> <out dir>"""
import os, sys, json, struct
sys.path.insert(0, os.path.dirname(__file__))
from atlas import parse
from hashname import file_string_hash
from tex2png import decode

root, out = sys.argv[1:3]
import re
# sprite source .tex files are not shipped; recover names from path-like strings in all assets (+ optional extra file, e.g. the .so)
pat = re.compile(rb'[A-Za-z0-9_./\\ -]{1,200}\.tex')
names = set()
extra = sys.argv[3:]
for r, _, fs in os.walk(root):
    for f in fs:
        if f.endswith(('.atlas', '.ogg', '.mp3', '.ttf', '.otf')): continue
        try: b = open(os.path.join(r, f), 'rb').read()
        except OSError: continue
        names.update(m.decode('latin1').strip() for m in pat.findall(b))
        if f.endswith('.tex'): names.add(os.path.relpath(os.path.join(r, f), root))
for e in extra: names.update(m.decode('latin1').strip() for m in pat.findall(open(e, 'rb').read()))
by_hash = {}
for p in names:
    for v in {p, p.lstrip('/'), 'textures/' + p.lstrip('/')}: by_hash[file_string_hash(v)] = v
manifest = {}; miss = 0; total = 0
for r, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith('.atlas'): continue
        ap = os.path.join(r, f)
        try: d = parse(open(ap, 'rb').read())
        except Exception as e: print('skip', ap, e); continue
        page_path = ap[:-6] + '0.tex'
        page = decode(open(page_path, 'rb').read())[0] if os.path.exists(page_path) else None
        pw, ph = d['hdr'][0], d['hdr'][1]
        for h, idx in d['names']:
            total += 1
            hv = struct.unpack('<I', bytes.fromhex(h[:8]))[0]
            name = by_hash.get(hv)
            rec = d['recs'][idx]
            if name is None: miss += 1; name = 'unknown/%08x.tex' % hv
            u0, v0, u1, v1 = rec['uv'][:4]
            box = (round(u0*pw), round(v0*ph), round(u1*pw), round(v1*ph))
            manifest[name] = dict(atlas=os.path.relpath(ap, root), box=box, w=rec['w'], h=rec['h'], off=rec['off'])
            if page is not None:
                o = os.path.join(out, name[:-4] + '.png'); os.makedirs(os.path.dirname(o), exist_ok=True)
                page.crop(box).save(o)
json.dump(manifest, open(os.path.join(out, 'manifest.json'), 'w'), indent=0)
print(total, 'sprites,', miss, 'unnamed')
