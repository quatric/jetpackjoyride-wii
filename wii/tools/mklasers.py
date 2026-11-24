#!/usr/bin/env python3 -I
"""LazerPatterns.xml (decoded with bxml.py) -> lasers.txt for the Wii app.
One line per pattern: P <patternTime> <nLasers> y... <nProb> <startDistance> <weight> ..."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import bxml
root = bxml.parse(open(sys.argv[1], 'rb').read())[0]
out = []
for n in root[2]:
    if n[0] != 'lazer_pattern': continue
    a = dict(n[1]); ys = []; pr = []
    for c in n[2]:
        if c[0] == 'probability': d = dict(c[1]); pr.append((int(float(d['startDistance'])), float(d['value'])))
        if c[0] == 'lazer':
            for p in c[2]:
                if p[0] == 'position': ys.append(float(dict(p[1])['y'])); break
    out.append('P %d %d %s %d %s' % (int(a['patternTime']), len(ys), ' '.join('%g' % y for y in ys), len(pr), ' '.join('%d %g' % p for p in pr)))
open(sys.argv[2], 'w').write('\n'.join(out) + '\n'); print(len(out), 'patterns')
