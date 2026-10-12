#!/usr/bin/env python3
"""Recover gamedata.bin asset names.

Engine hash (main.prx FUN_001be1e4): h = h*129 + (signed char)c over the file name
(directory stripped, extension kept), starting from 0, mod 2^32.

Because 129 is odd the step is invertible, so for a target hash and a candidate suffix we can
compute the hash the prefix must have and look it up in a table of known stems.

Usage: namehash.py <assets_dir> <strings.tsv> [extra_words.txt] > names.tsv
"""
import csv, glob, re, sys, collections

M = 0xFFFFFFFF
INV129 = pow(129, -1, 1 << 32)


def h129(s, h=0):
    for c in s.encode('latin1'):
        if c > 127:
            c -= 256
        h = (h * 129 + c) & M
    return h


def strip_suffix(target, suffix):
    """Hash the prefix must have so that prefix+suffix hashes to target."""
    h = target
    for c in reversed(suffix.encode('latin1')):
        if c > 127:
            c -= 256
        h = ((h - c) * INV129) & M
    return h


def main():
    assets_dir, strings_tsv = sys.argv[1], sys.argv[2]
    targets = {}
    for p in glob.glob(f'{assets_dir}/*/_index.csv'):
        for r in csv.DictReader(open(p)):
            targets.setdefault(int(r['hash'], 16), r['type'])

    words = set()
    srcs = [strings_tsv] + sys.argv[3:]
    for src in srcs:
        for line in open(src, encoding='utf-8', errors='replace'):
            for t in re.split(r'[^A-Za-z0-9_.-]+', line.split('\t', 2)[-1]):
                if t:
                    words.add(t)

    # stems: words without extension, plus the parts split on '_' joined back progressively
    stems = set()
    for w in words:
        base = w.rsplit('.', 1)[0] if '.' in w else w
        stems.add(base)
        stems.add(base.lower())
    exts = {t.lower() for t in targets.values()} | {
        'group', 'anim', 'psm', 'bnk', 'flip', 'skel', 'mesh', 'inst', 'subz', 'ents', 'ent', 'tcd',
        'tnv', 'vol', 'ltr', 'loc', 'nloc', 'rloc', 'bin', 'spl', 'land', 'snet', 'ccv', 'lit', 'nli',
        'atbl', 'comp', 'skl', 'mdl', 'tex',
        # type = first 4 chars of the extension upper-cased (FUN_00222310), so longer forms are possible
        'instance', 'instances', 'inst0', 'subzone', 'subz0', 'subzones', 'entities', 'entity', 'ents0',
        'tcd0', 'tnv0', 'volume', 'volumes', 'vol0', 'spline', 'splines', 'lights', 'light', 'lite',
        'locators', 'location', 'locs', 'nlocators', 'rlocators', 'binary', 'ccv0', 'landscape',
        'snetwork', 'ltr0', 'flipbook', 'anims', 'animation', 'bank', 'bnk0', 'binf', 'meshes'}
    suffixes = ['', '_abe', '_apkg', '_res', '_inc', '_lod', '_shadow', '_col', '_low', '_hi']

    found = {}
    for rounds in range(3):
        stem_h = {h129(s): s for s in stems}
        new = 0
        for t in targets:
            if t in found:
                continue
            for e in exts:
                for suf in suffixes:
                    tail = f'{suf}.{e}'
                    s = stem_h.get(strip_suffix(t, tail))
                    if s is not None:
                        found[t] = s + tail
                        break
                if t in found:
                    new += 1
                    break
        # learned names feed back as stems (and their '_'-prefixes) for the next round
        for n in found.values():
            base = n.rsplit('.', 1)[0]
            parts = base.split('_')
            for i in range(1, len(parts) + 1):
                stems.add('_'.join(parts[:i]))
        print(f'round {rounds}: +{new}, total {len(found)}/{len(targets)}', file=sys.stderr)
        if not new:
            break

    by_type = collections.Counter(targets[h] for h in found)
    tot = collections.Counter(targets.values())
    for t in sorted(tot, key=lambda k: -tot[k]):
        print(f'  {t:5s} {by_type[t]:5d}/{tot[t]}', file=sys.stderr)
    print('hash\ttype\tname')
    for h, n in sorted(found.items()):
        print(f'{h:08x}\t{targets[h]}\t{n}')


if __name__ == '__main__':
    main()
