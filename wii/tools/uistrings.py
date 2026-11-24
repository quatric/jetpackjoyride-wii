#!/usr/bin/env python3 -I
"""List the strings of .uiscreen files (FSIU): class names, property names, instance ids, texts, file names.
usage: uistrings.py file.uiscreen [...]   (the stream after the table is not decoded yet)"""
import sys, struct
def strings(b):
    assert b[:4] == b'FSIU', b[:4]
    q = b.index(b'TSIU') + 4; end = b.index(b'HCIU', q) if b'HCIU' in b[q:] else len(b); out = []
    while q < end - 4:
        L = struct.unpack_from('<I', b, q)[0]
        s = b[q+4:q+4+L] if 1 < L < 200 else b''
        if s and s[0] and all(32 <= c < 127 or c == 0 for c in s):
            out.append(s.split(b'\0')[0].decode()); q += 4 + L
        else: q += 4
    return out
if __name__ == '__main__':
    for f in sys.argv[1:]:
        print(f, strings(open(f, 'rb').read()))
