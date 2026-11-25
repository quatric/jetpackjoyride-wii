#!/usr/bin/env python3 -I
"""Write missions.txt from xml/MissionSystem/Missions.xml (bxml) and the English string table:
one line per active mission whose type this port can track: TYPE<TAB>param1<TAB>param2<TAB>text (%i/%s filled in)<TAB>difficulty;
also ranks.txt next to it: the 16 MISSION_LEVEL_n rank titles.
usage: mkmissions.py <assets dir> <out>"""
import os, subprocess, sys, xml.etree.ElementTree as ET
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import bxml
A, out = sys.argv[1], sys.argv[2]
TRACKED = {'PLAY_GAME', 'COINS_NOT_COLLECT', 'VEHICLE_COLLECT_ANY', 'TOKENS_COLLECT_GAME', 'TOKENS_COLLECT_TOTAL', 'COINS_COLLECT_TOTAL',
           'COINS_COLLECT_GAME', 'SPEND_COINS', 'FLY_DIST', 'FLY_NO_VEHICLE', 'FLY_DIST_TOTAL', 'CHANGE_GRAVITY', 'RUN_DISTANCE',
           'RUN_DISTANCE_TOTAL', 'RUB_HEAD_DISTANCE', 'RUB_HEAD_DISTANCE_TOTAL', 'VEHICLE_FLY_TOTAL_ANY', 'CLOSE_CALL', 'CLOSE_CALL_TOTAL',
           'SCIENTISTS_HIGH_FIVE', 'SCIENTISTS_HIGH_FIVE_TOTAL', 'SLOTS_CASH_IN', 'SLOTS_WIN_TOTAL',
           'TOUCH_ALARM', 'TOUCH_ALARM_TOTAL', 'VEHICLE_COLLECT', 'VEHICLE_DISTANCE', 'VEHICLE_FLY_TOTAL', 'SCIENTISTS_KNOCK_OVER',
           'SLIDE_DIST_TOTAL', 'START_FROM_MENU', 'USE_ITEM'}
VEH = {'Mech': 0, 'Bad Ass Hog': 1, 'Bird': 2, 'Gravity Suit': 3, 'Teleporter': 4}   # the app's vehicle numbers
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
    if h == 'USE_ITEM':   # 'use any gadget' and 'use a head start' are trackable here, once each
        if p1 not in ('PERK_ANY', 'CONSUMABLE_HEADSTART', 'CONSUMABLE_BOMB'): continue   # bombs are the Final Blast
        h, p1 = {'PERK_ANY': 'USE_ITEM_PERK', 'CONSUMABLE_HEADSTART': 'USE_ITEM_HEADSTART', 'CONSUMABLE_BOMB': 'USE_ITEM_BOMB'}[p1], '1'
    named = h in ('VEHICLE_COLLECT', 'VEHICLE_DISTANCE', 'VEHICLE_FLY_TOTAL', 'SCIENTISTS_KNOCK_OVER')
    if named:   # Param1 names the vehicle, Param2 is the count/distance: written as count, vehicle number
        if p1 not in VEH: continue
        name, p1, p2 = p1, p2, str(VEH[p1])
    if h.startswith('CLOSE_CALL'):   # Param1 names what to dodge, Param2 is the count: written as count, kind (0 missiles, 1 zappers)
        if p1 not in ('MISSILES', 'GATES'): continue
        p1, p2 = p2, '0' if p1 == 'MISSILES' else '1'
    t = txt.get(a['DESCRIPTION_STRING'], a.get('DEVELOPER_DESCRIPTION', h))
    if named: t = t.replace('%s', name).replace('%i', p1)
    for v in ((p1,) if h.startswith('CLOSE_CALL') else (p1, p2)): t = t.replace('%i', v, 1)
    if h == 'SPEND_COINS': p1 = '1'   # "Buy a new item in the shop": one purchase
    lines.append('\t'.join((h, p1, p2, clean(t), a.get('DIFFICULTY', '1'))))
assert lines, 'no missions'
open(out, 'w').write('\n'.join(lines) + '\n')
ranks = [clean(txt['MISSION_LEVEL_%d' % i]) for i in range(1, 17) if 'MISSION_LEVEL_%d' % i in txt]
open(os.path.join(os.path.dirname(out), 'ranks.txt'), 'w').write('\n'.join(ranks) + '\n')
