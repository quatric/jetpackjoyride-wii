#!/usr/bin/env python3 -I
"""ObstaclePatterns.xml -> zappers.txt.
'G <probability%> <startDistance>' then per pattern
'Z minLen maxLen scaleTime startAngle clockwise rotateTime onInterval offInterval allowedWithLasers nprob (startDistance weight)...'"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import bxml
root = bxml.parse(open(sys.argv[1], 'rb').read())[0]
g = dict(next(c for c in root[2] if c[0] == 'probability')[1])
out = ['G %s %s' % (g['value'], g['startDistance'])]
for n in root[2]:
    if n[0] != 'obstacle_pattern': continue
    v = {c[0]: dict(c[1]).get('value') for c in n[2] if c[0] != 'probability'}
    pr = [dict(c[1]) for c in n[2] if c[0] == 'probability']
    b = lambda k, d: 1 if v.get(k, d) == 'true' else 0
    out.append('Z %s %s %s %s %d %s %s %s %d %d %s' % (v['minScale'], v['maxScale'], v.get('scaleTime', 0), v.get('startAngle', 0),
        b('clockwise', 'true'), v.get('rotateTime', 0), v.get('onInterval', 1), v.get('offInterval', 0), b('allowedWithLasers', 'true'),
        len(pr), ' '.join('%s %s' % (p['startDistance'], p['value']) for p in pr)))
open(sys.argv[2], 'w').write('\n'.join(out) + '\n'); print(len(out) - 1, 'patterns')
