#!/usr/bin/env python3 -I
"""Draw a decoded .uiscreen to a PNG with uilayout's rectangles, to check a layout by eye (Pillow).
Textures are stretched to their rect (no nine-patch), Quads are filled with their colour, text is shown as its key.
usage: uipreview.py x.uiscreen <png dir from tex2png.py> out.png"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uidecode, uilayout
from PIL import Image, ImageDraw


def main(scr, pngdir, out):
    cs, _ = uidecode.decode(open(scr, 'rb').read())
    vr = uilayout.visible_rects(cs)
    im = Image.new('RGBA', (960, 640), (40, 40, 48, 255)); d = ImageDraw.Draw(im)
    for i, (x, y, w, h) in vr.items():
        c = cs[i]; p = c['props']; cl = c['class']
        if w <= 0 or h <= 0 or w > 4000 or h > 4000: continue
        t = p.get('texture'); t = t[0] if isinstance(t, list) else t
        if isinstance(t, str) and t:
            f = os.path.join(pngdir, t.replace('\\', '/').lower().replace('.tex', '.png'))
            if not os.path.exists(f):   # the PNG names keep the original case
                g = os.path.join(pngdir, t.replace('\\', '/').replace('.tex', '.png'))
                f = g if os.path.exists(g) else None
            if f:
                tx = Image.open(f).convert('RGBA').resize((max(1, int(w)), max(1, int(h))))
                im.alpha_composite(tx, (int(x), int(y))) if 0 <= x < 960 and 0 <= y < 640 else None
                continue
        if cl.endswith('Quad'):
            col = p.get('colour')
            if isinstance(col, tuple) and len(col) == 4 and col[3] > 0:
                ov = Image.new('RGBA', (960, 640), (0, 0, 0, 0))
                ImageDraw.Draw(ov).rectangle([x, y, x + w, y + h], fill=tuple(int(v) for v in col))
                im.alpha_composite(ov)
        elif cl.endswith('Text') and isinstance(p.get('text'), str):
            d.text((x, y + h / 2 - 5), p['text'], fill=(255, 255, 255, 255))
    im.convert('RGB').save(out)


if __name__ == '__main__':
    main(*sys.argv[1:4])
