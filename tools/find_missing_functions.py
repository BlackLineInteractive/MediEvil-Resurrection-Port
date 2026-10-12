#!/usr/bin/env python3
"""Code pointers in .rodata/.data (runs of >= 2 into .text) that are not inside any known function.
Usage: find_missing_functions.py <main.prx> <functions.tsv> > missing.txt"""
import bisect, struct, sys

d = open(sys.argv[1], 'rb').read()
shoff, = struct.unpack_from('<I', d, 0x20)
shnum, shstrndx = struct.unpack_from('<HH', d, 0x30)
secs = [struct.unpack_from('<10I', d, shoff + i * 40) for i in range(shnum)]
st = secs[shstrndx]
name = lambda o: d[st[4] + o:d.index(b'\0', st[4] + o)].decode()
text = next(s for s in secs if name(s[0]) == '.text')
lo, hi = text[3], text[3] + text[5]

F = []
for l in open(sys.argv[2]):
    if l.startswith('addr'):
        continue
    a, _, sz, _ = l.rstrip('\n').split('\t')
    F.append((int(a, 16), int(sz)))
F.sort()
starts = [a for a, _ in F]

targets = set()
for s in secs:
    if name(s[0]) not in ('.rodata', '.data'):
        continue
    run = []
    for i in range(0, s[5] - 3, 4):
        w, = struct.unpack_from('<I', d, s[4] + i)
        if lo <= w < hi and w % 4 == 0:
            run.append(w)
            continue
        if len(run) >= 2:
            targets.update(run)
        run = []

for t in sorted(targets):
    i = bisect.bisect_right(starts, t) - 1
    if not (i >= 0 and t < F[i][0] + F[i][1]):
        print(f'{t:08x}')
