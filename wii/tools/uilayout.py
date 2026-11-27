#!/usr/bin/env python3 -I
"""Absolute rectangles for a decoded .uiscreen, in the game's 960x640 screen (origin top-left, y down).
Rule found by checking ResultsScreen_New (LeftSide at -240 / rightSide at +240, both 480 wide, tile the screen):
a component's `position` is the offset of its CENTRE from its parent's centre. `dock` places it inside the parent first:
None = at position; Fill = the parent's rect; Top/Bottom/Left/Right = flush to that edge (full width/height on the other
axis unless the component has its own size), then `position` is added. DistributeH/V, TopLeft and table layouts are treated
as Fill/None (not modelled). Invisible components hide their subtree.
usage: uilayout.py x.uiscreen [textures-only]   -> id class x y w h texture"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uidecode

SW, SH = 960.0, 640.0


def num(v, d=0.0):
    if isinstance(v, list): v = v[0]
    return float(v) if isinstance(v, (int, float)) else d


def layout(comps):
    """-> list of (index, x, y, w, h, visible) with x,y the top-left corner."""
    ch = uidecode.children(comps)
    out = {}

    def place(i, px, py, pw, ph):
        p = comps[i]['props']
        pos = p.get('position'); pos = pos[0] if isinstance(pos, list) else pos
        ox, oy = (num(pos[0]), num(pos[1])) if isinstance(pos, tuple) and len(pos) >= 2 else (0.0, 0.0)
        w, h = num(p.get('width'), -1), num(p.get('height'), -1)
        dock = p.get('dock', 'None')
        if dock == 'Fill' or w < 0: w = pw if dock in ('Fill', 'Top', 'Bottom') or w < 0 else w
        if h < 0 or dock in ('Fill', 'Left', 'Right') and h < 0: h = ph
        cx, cy = px + pw / 2 + ox, py + ph / 2 + oy
        x, y = cx - w / 2, cy - h / 2
        if dock == 'Top': y = py + oy
        elif dock == 'Bottom': y = py + ph - h + oy
        elif dock == 'Left': x = px + ox
        elif dock == 'Right': x = px + pw - w + ox
        vis = p.get('visible', True) is not False
        out[i] = (x, y, w, h, vis)
        for j in ch.get(i, []): place(j, x, y, w, h)

    for r in [i for i, c in enumerate(comps) if c['hasParent'] in (0xffffffff,)]:
        place(r, 0.0, 0.0, SW, SH)
    return out


def visible_rects(comps):
    """Rects of components whose whole ancestor chain is visible."""
    lay = layout(comps); ok = {}
    for i, c in enumerate(comps):
        par = c['hasParent']
        ok[i] = par == 0xffffffff or lay[i][4] and ok.get(par, True)   # a screen's root starts hidden and is shown by a trigger
    return {i: lay[i][:4] for i in lay if ok[i]}


if __name__ == '__main__':
    cs, _ = uidecode.decode(open(sys.argv[1], 'rb').read())
    vr = visible_rects(cs)
    for i, (x, y, w, h) in vr.items():
        t = cs[i]['props'].get('texture')
        if len(sys.argv) > 2 and not isinstance(t, str): continue
        if len(sys.argv) > 2 and not t: continue
        print(f"{cs[i]['id']} {cs[i]['class'][9:]} {x:.0f} {y:.0f} {w:.0f} {h:.0f} {t if isinstance(t, str) else ''}")
