#!/usr/bin/env python3 -I
"""Decode the component stream of a .uiscreen (FSIU) into a flat component list.
Layout found by comparing fpscounterscreen.uiscreen with its XML twin (little endian u32 words):
  HCIU 0 <n> 0xcb980001, then 'TAB' 2 5 <id> <hasParent> <class> <sibling> 0 and 'TAB' 2 1 0 0xcb980b91 <8 bytes>,
  then ten slots  0x7aaa1 <t> then (0x7bbbe = empty | 0x7bbb1 <n>) with n entries 'TAB' 2 2 <name> DE -1 [DE -1] 0 1 0 <value> 0,
  closing 0x7fff1, then MAIU BAIU 0 EMIU 0 0 0 0xcf980001.  Slot types: 1 bool, 2 enum string, 3 vector3, 5 float,
  6 rotation, 8 colour (4 words), 10 text string; 4, 7, 9 not seen with data yet.
usage: uidecode.py <assets>/xml/Screens/x.uiscreen [tables dir ok]"""
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uistrings
PRE = {1: 5, 2: 5, 10: 5, 5: 5, 7: 5, 3: 7, 6: 7, 4: 7, 8: 8, 9: 8}   # words between the property name and its value
HC, TAB, DE, PA, VE, V1, PE, MA, ESIU = 0x55494348, 0x54414200, 0x7dee1, 0x7aaa1, 0x7bbbe, 0x7bbb1, 0x7fff1, 0x5549414d, 0x55495345
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
                    k += 4 * PRE.get(t, 5)   # DE -1 links and the 0 1 0 marker: 5 words, 7 for vectors, 8 for colour
                    if t in (1,): v = bool(W(k)); k += 4
                    elif t in (2, 10):
                        L = W(k); v = b[k+4:k+4+L].split(b'\0')[0].decode('latin1'); k += 4 + L
                    elif t in (3, 6): v = (F(k), F(k + 4), F(k + 8)); k += 12
                    elif t == 5: v = F(k); k += 4
                    elif t == 8: v = tuple(F(k + 4 * i) for i in range(4)); k += 16
                    else:   # slots 4 (vector2), 7 (int), 9 (5 words): read up to the next entry or slot end
                        j = k; raw = []
                        while W(j) not in (TAB, PE): raw.append(W(j)); j += 4
                        if raw: raw.pop()   # trailing 0
                        v = tuple(struct.unpack('<f', struct.pack('<I', x))[0] for x in raw) if t == 4 else tuple(raw)
                        k = j - 4 if False else j - 4 + 4 * 0
                        k = j; k -= 4
                    if t in (1, 2, 3, 5, 6, 8, 10): k += 4   # trailing 0
                    else: k += 4
                    c['props'][nm] = v
                    if W(k) == PE: pass
                assert W(k) == PE, hex(k); k += 4
            if W(k) == PE: k += 4
        assert W(k) == MA, hex(k); k += 32   # MAIU BAIU 0 EMIU 0 0 0 cf980001
        comps.append(c)
        if k + 4 <= len(b) and W(k) not in (HC, ESIU):   # trigger / event sections between components are not decoded: resync
            j = b.find(b'HCIU', k); k = j if j >= 0 else b.rfind(b'ESIU')
    return comps, W(k) == ESIU
if __name__ == '__main__':
    for f in sys.argv[1:]:
        cs, ok = decode(open(f, 'rb').read()); print(f, 'complete' if ok else 'INCOMPLETE')
        for c in cs: print(' ', c['id'], c['class'], c['order'], c['props'])
