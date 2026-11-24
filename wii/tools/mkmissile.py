#!/usr/bin/env python3 -I
"""MissileProbability.xml -> missile.txt: 'M <startDistance> <probability%> <chaseSpeed> <missileSpeed>'"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import bxml
r = bxml.parse(open(sys.argv[1], 'rb').read())[0][2]
g = lambda n: dict(next(c for c in r if c[0] == n)[1])['value']
p = [c for c in r if c[0] == 'probability']; last = dict(p[-1][1])
open(sys.argv[2], 'w').write('M %s %s %s %s\n' % (last['startDistance'], last['value'], g('chaseSpeed'), g('missileSpeed')))
