#!/usr/bin/env python3 -I
"""Chain the game's standard coin patterns (patterns/coinPattern<N>.txt) into one pattern.txt; '=' starts a new pattern.
usage: mkpatterns.py <assets>/patterns out/pattern.txt"""
import sys, os, re, random
d, out = sys.argv[1:3]
fs = sorted((f for f in os.listdir(d) if re.fullmatch(r'coinPattern\d+\.txt', f)), key=lambda f: int(re.findall(r'\d+', f)[0]))
fs += ['coinPatternRings.txt', 'coinPatternIHeartCoin.txt']   # CoinPatterns_1.json gives these the same weight (5) as the numbered ones
random.Random(7).shuffle(fs)
with open(out, 'w') as o:
    for f in fs:
        o.write('=\n' + open(os.path.join(d, f)).read().strip('\r\n') + '\n')
print(len(fs), 'patterns')
