#!/usr/bin/env python3 -I
"""Decode the component stream of a .uiscreen (FSIU) into a flat component list.
Layout found by comparing fpscounterscreen.uiscreen with its XML twin (little endian u32 words):
  HCIU 0 <n> 0xcb980001, then 'TAB' 2 5 <id> <hasParent> <class> <sibling> 0 and 'TAB' 2 1 0 0xcb980b91 <8 bytes>,
  then ten slots  0x7aaa1 <t> then (0x7bbbe = empty | 0x7bbb1 <n>) with n entries 'TAB' 2 2 <name> DE -1 [DE -1] 0 1 0 <value> 0,
  closing 0x7fff1, then MAIU BAIU 0 EMIU 0 0 0 0xcf980001.  Slot types: 1 bool, 2 enum string, 3 vector3, 5 float,
  6 rotation, 8 colour (4 words), 10 text string; 4, 7, 9 not seen with data yet.
usage: uidecode.py <assets>/xml/Screens/x.uiscreen [tables dir ok]"""
import re, struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uistrings
PRE = {1: 5, 2: 5, 10: 5, 5: 5, 7: 5, 3: 7, 6: 7, 4: 7, 8: 8, 9: 8}   # words between the property name and its value
TABB, PEB = b'\x00BAT\x02\x00\x00\x00\x02\x00\x00\x00', b'\xf1\xff\x07\x00'   # next entry header (TAB 2 2) or slot end
HC, TAB, DE, PA, VE, V1, PE, MA, ESIU = 0x55494348, 0x54414200, 0x7dee1, 0x7aaa1, 0x7bbbe, 0x7bbb1, 0x7fff1, 0x5549414d, 0x55495345
def value(b, k, e, t):
    """Value of one slot entry spanning b[k:e] (after 'TAB 2 2 <name>'): links (DE -1 pairs / 0), then '0 <n> ...' values."""
    W = lambda i: struct.unpack_from('<I', b, i)[0]; F = lambda i: struct.unpack_from('<f', b, i)[0]
    while W(k) == DE: k += 8
    if t == 1:   # bool: 0 <n> v v 0
        assert W(k) == 0 and W(k + 4) == 1; return bool(W(k + 8))
    while not (W(k) == 0 and W(k + 4) != 0 and W(k + 8) == 0 or t in (2, 10) and W(k) == 0 and W(k + 4) == 1): k += 4
    nv = W(k + 4); k += 12 if W(k + 8) == 0 else 8
    vs = []
    for _ in range(nv):
        if t in (2, 10): L = W(k); v = b[k+4:k+4+L].split(b'\0')[0].decode('latin1'); k += 4 + L
        elif t in (3, 6): v = (F(k), F(k + 4), F(k + 8)); k += 12
        elif t == 4: v = (F(k), F(k + 4)); k += 8
        elif t == 5: v = F(k); k += 4
        elif t == 7: v = W(k); k += 4
        elif t == 8: v = tuple(F(k + 4 * i) for i in range(4)); k += 16
        else: v = tuple(F(k + 4 * i) for i in range(4)); k += 16   # slot 9: rectangle (left, top, right, bottom)
        vs.append(v); k += 4   # 1 = another keyframe follows, 0 = last
        if k > e: raise ValueError('overrun')
    return vs[0] if nv == 1 else vs

def decode(b):
    g = uistrings.tables(b); names = lambda i: g[i] if len(g) > i else []
    classes = [t for t, _ in names(1)]; props = [t for t, _ in names(4)]; ids = [t for t, _ in names(6)]
    W = lambda k: struct.unpack_from('<I', b, k)[0]; F = lambda k: struct.unpack_from('<f', b, k)[0]
    k = b.index(b'HCIU'); comps = []
    while k + 4 <= len(b) and W(k) == HC:
        k += 4; assert W(k) == 0, k; k += 4
        c = {'id': ids[W(k)] if W(k) < len(ids) else W(k)}; k += 4; k += 4   # id index, 0xcb980001
        assert W(k) == TAB, hex(k)
        hdr = [W(k + 4 + 4 * i) for i in range(6)]; k += 32   # TAB 2 5 id parentFlag class sibling 0
        c['class'] = classes[hdr[4 - 1 + 0]] if False else None
        c['hasParent'] = hdr[3]; c['class'] = classes[hdr[4]] if hdr[4] < len(classes) else hdr[4]; c['order'] = hdr[5]
        assert W(k) == TAB; k += 24   # TAB 2 1 0 cb980b91 EMPS
        # skip the two words of the second record's tail (8 bytes) already counted
        c['props'] = {}
        while W(k) == PA:
            t = W(k + 4); k += 8
            if W(k) == VE: k += 4
            else:
                assert W(k) == V1, hex(k); n = W(k + 4); k += 8
                for _ in range(n):
                    assert W(k) == TAB and W(k + 4) == 2 and W(k + 8) == 2, hex(k)
                    nm = props[W(k + 12)] if W(k + 12) < len(props) else W(k + 12); k += 16
                    cand = [x for x in (b.find(TABB, k), b.find(PEB, k)) if x >= 0]   # strings are not 4-aligned: search bytes
                    e = min(cand) if cand else len(b)
                    try: v = value(b, k, e, t)
                    except Exception: v = ('raw', [W(i) for i in range(k, e, 4)])
                    k = e
                    c['props'][nm] = v
                    if W(k) == PE: pass
                assert W(k) == PE, hex(k); k += 4
            if W(k) == PE: k += 4
        assert W(k) == MA, hex(k); k += 32   # MAIU BAIU 0 EMIU 0 0 0 cf980001
        comps.append(c)
        if k + 4 <= len(b) and W(k) not in (HC, ESIU):   # trigger / event sections between components are not decoded: resync
            j = b.find(b'HCIU', k); j = j if j >= 0 else b.rfind(b'ESIU'); c['events'] = events(b[k:j]); k = j
    return comps, W(k) == ESIU
if __name__ == '__main__':
    for f in sys.argv[1:]:
        cs, ok = decode(open(f, 'rb').read()); print(f, 'complete' if ok else 'INCOMPLETE')
        for c in cs: print(' ', c['id'], c['class'], c['order'], c['props'])


def events(seg):
    """HTVE <u32 len><event name> ... CEIU <u32 len><command> ... FEIU: [(event, [(command, [string args])])]. Strings only; the numeric operands are not decoded."""
    out = []
    for h in seg.split(b'HTVE')[1:]:
        n = struct.unpack_from('<I', h, 0)[0]; name = h[4:4 + n].rstrip(b'\0').decode('latin1'); cmds = []
        for cm in h.split(b'CEIU')[1:]:
            n = struct.unpack_from('<I', cm, 0)[0]; cmd = cm[4:4 + n].rstrip(b'\0').decode('latin1')
            if re.fullmatch(r'[A-Za-z]\w*', cmd): cmds.append((cmd, [x.decode('latin1') for x in re.findall(rb'[ -~]{3,}', cm[4 + n:])]))
        out.append((name, cmds))
    return out


def children(comps):
    """parent index -> child indices; `hasParent` indexes the component list (0xffffffff = root)."""
    ch = {}
    for i, c in enumerate(comps):
        ch.setdefault(c['hasParent'], []).append(i)
    return ch
