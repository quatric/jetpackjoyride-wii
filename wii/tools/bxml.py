#!/usr/bin/env python3 -I
"""Halfbrick 'bxml' (binary XML) -> text XML.
'bxml', u16 version, u16 ?, u32 strtab_size, strtab (NUL separated, referenced by byte offset), u32 root_count,
then nodes: u8 kind(1 element, 4 text=u32 string offset), u32 name, u32 nattrs, nattrs*(u32 name, u32 value), u32 nchildren, children."""
import struct, sys
from xml.sax.saxutils import quoteattr

def parse(b):
    assert b[:4] == b'bxml', b[:4]
    n = struct.unpack_from('<I', b, 8)[0]; st = b[12:12+n]; o = 12+n
    def S(i): return st[i:st.index(b'\0', i)].decode('utf8', 'replace')
    def u32():
        nonlocal o; v = struct.unpack_from('<I', b, o)[0]; o += 4; return v
    def node():
        nonlocal o
        k = b[o]; o += 1
        if k == 4: return ('#text', [(S(u32()), '')], [])   # text node: one string offset
        if k != 1: raise ValueError('kind %d at %#x' % (k, o-1))
        name = S(u32()); na = u32(); at = [(S(u32()), S(u32())) for _ in range(na)]
        return (name, at, [node() for _ in range(u32())])
    return [node() for _ in range(u32())]

def dump(n, d=0, out=None):
    name, at, ch = n; pad = '  '*d
    if name == '#text': out.append(pad + at[0][0]); return
    a = ''.join(' %s=%s' % (k, quoteattr(v)) for k, v in at)
    if ch:
        out.append('%s<%s%s>' % (pad, name, a)); [dump(c, d+1, out) for c in ch]; out.append('%s</%s>' % (pad, name))
    else: out.append('%s<%s%s/>' % (pad, name, a))

def to_xml(b):
    out = []; [dump(r, 0, out) for r in parse(b)]; return '\n'.join(out)

if __name__ == '__main__':
    print(to_xml(open(sys.argv[1], 'rb').read()))
