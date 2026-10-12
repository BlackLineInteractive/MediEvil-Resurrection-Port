#!/usr/bin/env python3
"""Map compiler errors (build/tx/*.err) to generated functions; append them to the stub list.
usage: transpile_fix.py [--dry]  -> prints counts; without --dry updates decomp/cpp/stubbed.txt"""
import os, re, sys, bisect
from collections import defaultdict
out = 'decomp/cpp'
ranges = defaultdict(list)
for l in open(os.path.join(out, 'lines.tsv')):
    p = l.split()
    if len(p) == 4:
        ranges[p[0]].append((int(p[2]), int(p[3]), p[1]))
bad, other = set(), 0
for fn in os.listdir('build/tx'):
    for l in open(os.path.join('build/tx', fn), errors='replace'):
        m = re.match(r'decomp/cpp/(game_\d+\.cpp):(\d+):\d+: (?:error|note: in instantiation)', l)
        if not m:
            if ': error' in l: other += 1
            continue
        r = ranges[m.group(1)]
        ln = int(m.group(2))
        i = bisect.bisect_right([a for a, _, _ in r], ln) - 1
        if i >= 0 and r[i][0] <= ln <= r[i][1]:
            bad.add(r[i][2])
print('failing functions', len(bad), 'errors outside generated files', other)
if '--dry' not in sys.argv:
    path = os.path.join(out, 'stubbed.txt')
    old = set(l.split()[0] for l in open(path)) if os.path.exists(path) else set()
    with open(path, 'w') as w:
        for a in sorted(old | bad):
            w.write(a + '\n')
    print('stub list', len(old | bad))
