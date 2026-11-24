#!/usr/bin/env python3 -I
"""Tables of a .uiscreen (FSIU): the counted string groups after the TSIU tag.
Group roles seen so far: 0 aliases ('~default'), 1 component classes, 4 property names, 6 instance ids; 2/3/5/7 empty or unknown.
An entry is u32 len + len bytes: NUL-terminated text, then 0-3 type-id bytes (e.g. 'position' carries f8 3c 00).
The component stream after the tables (HCIU ... ESIU) is not decoded.
usage: uistrings.py file.uiscreen [...]"""
import struct, sys
def tables(b):
    assert b[:4] == b'FSIU', b[:4]
    q = b.index(b'TSIU') + 4; groups = []
    while q + 4 <= len(b):
        n = struct.unpack_from('<I', b, q)[0]
        if n > 4000: break
        p = q + 4; g = []
        for _ in range(n):
            if p + 4 > len(b): return groups
            L = struct.unpack_from('<I', b, p)[0]
            if L > 400 or p + 4 + L > len(b): return groups
            s = b[p+4:p+4+L]; t = s.split(b'\0')[0]; g.append((t.decode('latin1'), s[len(t)+1:])); p += 4 + L
        groups.append(g); q = p
    return groups
if __name__ == '__main__':
    for f in sys.argv[1:]:
        print(f)
        for i, g in enumerate(tables(open(f, 'rb').read())):
            if g: print(' ', i, [t for t, _ in g])
