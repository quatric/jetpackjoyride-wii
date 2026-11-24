#!/usr/bin/env python3 -I
"""Property schema of the UI component classes (xml/Components/*.xml): baseDefinition chain and propertyDefinition
(name, type, default) in file order, base class first.
usage: uischema.py <assets>/xml/Components ComponentText"""
import os, re, sys
def schema(d, cls):
    p = os.path.join(d, cls + '.xml.txt')
    if not os.path.exists(p): p = os.path.join(d, cls + '.xml')
    t = open(p, errors='replace').read()
    b = re.search(r'baseDefinition="([^"]+)"', t)
    out = schema(d, b.group(1)) if b else []
    for m in re.finditer(r'<propertyDefinition\b([^>]*)/?>', t):
        a = dict(re.findall(r'(\w+)="([^"]*)"', m.group(1)))
        if 'name' in a: out.append((a['name'], a.get('type', ''), a.get('default')))
    return out
if __name__ == '__main__':
    for i, (n, t, df) in enumerate(schema(sys.argv[1], sys.argv[2])): print(i, n, t, df)
