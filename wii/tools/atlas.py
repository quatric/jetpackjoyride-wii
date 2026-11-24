#!/usr/bin/env python3 -I
"""Parse Halfbrick .atlas (TextureAtlasTool, magic 0xfe000003) - plain, not encrypted.
Layout (little endian), derived from libmortargame.so SerializedTextureAtlasHeader::Read /
Mortar::Read(TextureAtlasPage&) / Mortar::Read(TextureAtlasRec&):
  u32 magic; u32 len + tool string; u32 pagew, pageh, ?, ?; u32 bool; u32 ?; u32 n + n*u32;
  page: u32 nrecs; nrecs * rec; u32 nnames; nnames * (u8 len(=12) + hash[len] + u32 recidx)
  rec: u32 w,h, u32 x,y,w2,h2(@0x44..), f32 x8 (uv/corner), u32 @0x60, f32 off x,y, i32 a,b."""
import struct, sys, json

def parse(b):
    o = 0
    def u32():
        nonlocal o; v = struct.unpack_from('<I', b, o)[0]; o += 4; return v
    magic = u32(); assert magic == 0xfe000003, hex(magic)
    n = u32(); tool = b[o:o+n].decode('latin1'); o += n
    hdr = [u32() for _ in range(7)]
    cnt = hdr[6]
    pre = [u32() for _ in range(cnt)]
    nrec = u32()
    recs = []
    for _ in range(nrec):
        r = struct.unpack_from('<6I8fI2f2i', b, o); o += 76
        recs.append(dict(w=r[0], h=r[1], rect=r[2:6], uv=[round(x, 6) for x in r[6:14]], flags=r[14], off=r[15:17], link=r[17:19]))
    nn = u32(); names = []
    for _ in range(nn):
        L = b[o]; o += 1; h = b[o:o+L].hex(); o += L; idx = u32(); names.append((h, idx))
    return dict(tool=tool, hdr=hdr, pre=pre, recs=recs, names=names, end=o, size=len(b))

if __name__ == '__main__':
    d = parse(open(sys.argv[1], 'rb').read())
    print(json.dumps(d)[:2000] if len(sys.argv) < 3 else json.dumps(d))
