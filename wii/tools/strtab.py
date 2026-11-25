#!/usr/bin/env python3 -I
"""Dump a Halfbrick .str string table as KEY<TAB>text. Layout (little endian u32):
translations_header.str: 4 version, 64 hash, 4 total, 4 blob length, blob of NUL separated keys, then count and
count records of 10 words (key offset, len, len, ...). translations_<Language>.str: the same but with an extra 0 word after
the total, then the texts blob and count records of 3 words (offset, length in characters, length in bytes). Record i of the header names record i of the language.
usage: strtab.py <StringTables dir> <Language, e.g. English_US>"""
import struct, sys, os
d, lang = sys.argv[1], sys.argv[2]
def refs(b, hdr, stride):
    L = struct.unpack_from('<I', b, 72 if hdr else 76)[0]; s = 76 if hdr else 80
    blob, idx = b[s:s + L], b[s + L:]
    n = struct.unpack_from('<I', idx, 0)[0]
    out = []
    for i in range(n):
        o, l = struct.unpack_from('<I', idx, 4 + 4 * stride * i)[0], struct.unpack_from('<I', idx, 12 + 4 * stride * i)[0]   # words: offset, characters, bytes
        out.append(blob[o:o + l].decode('utf8', 'replace'))
        if hdr: out[-1] = (out[-1], struct.unpack_from('<I', idx, 4 + 4 * stride * i + 36)[0])
    return out
keys = refs(open(os.path.join(d, 'translations_header.str'), 'rb').read(), True, 10)
txt = refs(open(os.path.join(d, 'translations_%s.str' % lang), 'rb').read(), False, 3)
assert len(keys) == len(txt)
for k, i in keys: print(k + '\t' + txt[i].replace('\n', '\\n'))
