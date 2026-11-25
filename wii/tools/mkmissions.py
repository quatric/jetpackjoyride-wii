#!/usr/bin/env python3 -I
"""Write missions.txt from xml/MissionSystem/Missions.xml (bxml) and the English string table:
one line per active mission whose type this port can track: TYPE<TAB>param1<TAB>param2<TAB>text (%i/%s filled in).
usage: mkmissions.py <assets dir> <out>"""
import os, subprocess, sys, xml.etree.ElementTree as ET
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import bxml
A, out = sys.argv[1], sys.argv[2]
TRACKED = {'PLAY_GAME', 'COINS_NOT_COLLECT', 'VEHICLE_COLLECT_ANY', 'TOKENS_COLLECT_GAME', 'TOKENS_COLLECT_TOTAL', 'COINS_COLLECT_TOTAL',
           'COINS_COLLECT_GAME', 'SPEND_COINS', 'FLY_DIST', 'FLY_NO_VEHICLE', 'FLY_DIST_TOTAL', 'CHANGE_GRAVITY', 'RUN_DISTANCE',
           'RUN_DISTANCE_TOTAL', 'RUB_HEAD_DISTANCE', 'RUB_HEAD_DISTANCE_TOTAL', 'VEHICLE_FLY_TOTAL_ANY', 'CLOSE_CALL', 'CLOSE_CALL_TOTAL',
           'SCIENTISTS_HIGH_FIVE', 'SCIENTISTS_HIGH_FIVE_TOTAL'}
txt = {}
for l in subprocess.check_output([sys.executable, '-I', os.path.join(here, 'strtab.py'), os.path.join(A, 'StringTables'), 'English_US']).decode('utf8').splitlines():
    k, t = l.split('\t', 1); txt[k] = t
def clean(t):   # U+E005 is the game's metre glyph; other private-use glyphs are icons the Wii font lacks
    return ''.join('M' if c == '' else '' if 0xe000 <= ord(c) <= 0xf8ff else c for c in t).replace('\\n', ' ').strip().upper()
root = ET.fromstring(bxml.to_xml(open(os.path.join(A, 'xml/MissionSystem/Missions.xml'), 'rb').read()))
lines = []
for m in root.iter('Mission'):
    a = m.attrib; h = a['MISSION_HASH']
    if a.get('Active') != 'YES' or h not in TRACKED: continue
    p1, p2 = a.get('Param1', '0'), a.get('Param2', '0')
    if h.startswith('CLOSE_CALL'):   # Param1 names what to dodge, Param2 is the count: written as count, kind (0 missiles, 1 zappers)
        if p1 not in ('MISSILES', 'GATES'): continue
        p1, p2 = p2, '0' if p1 == 'MISSILES' else '1'
    t = txt.get(a['DESCRIPTION_STRING'], a.get('DEVELOPER_DESCRIPTION', h))
    for v in ((p1,) if h.startswith('CLOSE_CALL') else (p1, p2)): t = t.replace('%i', v, 1)
    if h == 'SPEND_COINS': p1 = '1'   # "Buy a new item in the shop": one purchase
    lines.append('\t'.join((h, p1, p2, clean(t))))
assert lines, 'no missions'
open(out, 'w').write('\n'.join(lines) + '\n')
