"""Mortar FileStringHash: Bob Jenkins lookup2, initval 0x805, input lowercased, '\\' -> '/'.
Atlas entries are keyed by this hash of the sprite's .tex path (e.g. 'textures/effects/foo.tex')."""
import struct
M = 0xffffffff
def _mix(a, b, c):
    a = (a-b-c) & M; a ^= c >> 13
    b = (b-c-a) & M; b ^= (a << 8) & M
    c = (c-a-b) & M; c ^= b >> 13
    a = (a-b-c) & M; a ^= c >> 12
    b = (b-c-a) & M; b ^= (a << 16) & M
    c = (c-a-b) & M; c ^= b >> 5
    a = (a-b-c) & M; a ^= c >> 3
    b = (b-c-a) & M; b ^= (a << 10) & M
    c = (c-a-b) & M; c ^= b >> 15
    return a, b, c
def file_string_hash(s, init=0x805):
    k = bytes(47 if ch == 92 else (ch+32 if 65 <= ch <= 90 else ch) for ch in s.encode('latin1'))
    L = len(k); a = b = 0x9e3779b9; c = init; o = 0; n = L
    while n >= 12:
        a = (a+struct.unpack_from('<I', k, o)[0]) & M; b = (b+struct.unpack_from('<I', k, o+4)[0]) & M
        c = (c+struct.unpack_from('<I', k, o+8)[0]) & M
        a, b, c = _mix(a, b, c); o += 12; n -= 12
    c = (c+L) & M
    for i, ch in enumerate(k[o:]):
        if i < 4: a = (a+(ch << (8*i))) & M
        elif i < 8: b = (b+(ch << (8*(i-4)))) & M
        else: c = (c+(ch << (8*(i-7)))) & M
    return _mix(a, b, c)[2]
