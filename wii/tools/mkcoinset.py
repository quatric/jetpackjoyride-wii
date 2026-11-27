#!/usr/bin/env python3 -I
"""Vehicle coin pattern set from Configs/coins_patterns/Streaks_<V>Coins_1.json -> text for the Wii app.
First line 'H <TopHalfWeight> <BottomHalfWeight>'; then per pattern '= v0 s0 [v1 s1]' (weight v from startDistance s, in metres ridden)
followed by the pattern file's rows.  usage: mkcoinset.py <Configs>/coins_patterns/Streaks_X.json <assets>/patterns out.txt"""
import sys, os, json
cfg, pd, out = sys.argv[1:4]
c = json.load(open(cfg))['coin_pattern_file']
with open(out, 'w') as o:
    o.write('H %d %d\n' % (c.get('TopHalfWeight', {}).get('value', 50), c.get('BottomHalfWeight', {}).get('value', 50)))
    for p in c['coin_pattern']:
        pr = p['probability']; pr = pr if isinstance(pr, list) else [pr]
        f = os.path.join(pd, os.path.basename(p['filename']))
        if not os.path.exists(f): continue
        o.write('= ' + ' '.join('%d %d' % (x['value'], x['startDistance']) for x in pr[:2]) + '\n' + open(f).read().strip('\r\n') + '\n')
