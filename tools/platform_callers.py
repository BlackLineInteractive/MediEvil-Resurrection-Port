#!/usr/bin/env python3
"""Which game functions call each PSP import directly (the game's platform layer).
usage: platform_callers.py [prefix...]   e.g. sceIo sceKernelAlloc"""
import os, re, sys
from collections import defaultdict
d = 'decomp/transpile_in'
imports = {}
for fn in os.listdir(d):
    if fn.endswith('.c'):
        t = open(os.path.join(d, fn), errors='replace').read()
        m = re.search(r'/\* (\w+) NID', t)
        if m:
            imports[re.search(r'^//@F \S+ \S+ (\S+)', t, re.M).group(1)] = fn[:8]
callers = defaultdict(set)
sizes = {}
for fn in os.listdir(d):
    if not fn.endswith('.c'):
        continue
    t = open(os.path.join(d, fn), errors='replace').read()
    sizes[fn[:8]] = int(re.search(r'^//@F \S+ (\S+)', t, re.M).group(1))
    for name in set(re.findall(r'\b(sce\w+|__sce\w+)\s*\(', t)):
        if name in imports and imports[name] != fn[:8]:
            callers[name].add(fn[:8])
pre = sys.argv[1:] or ['']
for name in sorted(callers):
    if any(name.startswith(p) for p in pre):
        print('%-34s %s' % (name, ' '.join('%s(%d)' % (c, sizes[c]) for c in sorted(callers[name]))))
