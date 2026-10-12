#!/usr/bin/env python3
"""Name vtables (classes) from class-like strings used in their installers or slot functions.

Evidence, strongest first:
  "Class::Method"            -> class Class, and the containing slot is Method
  "Class" (bare C[A-Z]... identifier string) in an installer -> class Class (factory/registry name)
Usage: name_classes.py <vtables.tsv> <decomp_dir> > classes.tsv
"""
import collections, csv, glob, os, re, sys

QUAL = re.compile(r'"[^"\n]*?\b(C[A-Z][A-Za-z0-9_]*)::(~?[A-Za-z_][A-Za-z0-9_]*)')
BARE = re.compile(r'"(C[A-Z][a-z][A-Za-z0-9_]*)"')


def main():
    vtsv, ddir = sys.argv[1:3]
    src = {}
    for p in glob.glob(os.path.join(ddir, '*.c')):
        src[os.path.basename(p).rsplit('_', 1)[1][:-2]] = open(p, errors='replace').read()

    print('vtable\tslots\tclass\tevidence')
    for r in csv.DictReader(open(vtsv), delimiter='\t'):
        slots = [s for s in r['first_slots'].split(',') if s]
        inst = [s for s in r['installers'].split(',') if s]
        votes = collections.Counter()
        ev = []
        for fn in inst + slots:
            code = src.get(fn, '')
            for c, m in QUAL.findall(code):
                votes[c] += 3
                ev.append(f'{fn}:{c}::{m}')
            if fn in inst:
                for c in BARE.findall(code):
                    votes[c] += 1
                    ev.append(f'{fn}:"{c}"')
        if votes:
            best, score = votes.most_common(1)[0]
            print(f"{r['vtable']}\t{r['slots']}\t{best}\t{'; '.join(ev[:4])}")


if __name__ == '__main__':
    main()
