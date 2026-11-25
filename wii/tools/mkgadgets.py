#!/usr/bin/env python3 -I
"""Write gadgets.txt: the real PERK_*_DESC text for each shop slot, in the app's shop order. Blank line = no real counterpart.
usage: mkgadgets.py <StringTables dir> <out>"""
import subprocess, sys, os
d = {}
for l in subprocess.check_output([sys.executable, '-I', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'strtab.py'), sys.argv[1], 'English_US']).decode('utf8').splitlines():
    k, t = l.split('\t', 1); d[k] = t.replace('\\n', ' ')
order = ['MAGNET', 'SLOW_MISSILES', 'DUD_MISSILES', None, 'RUBBER_BONES', 'COIN_MULTIPLIER', 'START_IN_VEHICLE', 'LEAD_BOOTS', 'DUD_ZAPPERS',
         'IDENTIFY_VEHICLES', 'BRICK', 'FRUIT_MISSILES', 'ICY_EXTERIOR', 'ROBOT_DOG', 'TOKEN_MAGNET', 'BONUS_SLOT', 'BOOST_MACHINE', 'SLOT_GUARANTEE',
         'FLYING_PIG', 'GROUND_BOOST', 'NO_SCIENTISTS', 'HEADSTART']
desc = lambda k: d['ITEM_HEADSTART_DESC'].split('!')[0].strip() + '!' if k == 'HEADSTART' else d['PERK_%s_DESC' % k].strip()   # the consumable's text ends with a count line: keep the first sentence
open(sys.argv[2], 'w').write(''.join((desc(k) if k else '') + '\n' for k in order))
