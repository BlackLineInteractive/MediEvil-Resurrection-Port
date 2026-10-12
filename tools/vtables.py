#!/usr/bin/env python3
"""Find C++ vtables in main.prx and the functions that install them (ctors/dtors).

A vtable = run of >= 2 consecutive words in .rodata/.data that are function entry points.
Installers = decompiled functions with `*param_N = &PTR_..._<vtable>` (Ghidra output).
Usage: vtables.py <main.prx> <functions.tsv> <decomp_dir> > vtables.tsv
"""
import glob, os, re, struct, sys, collections


def main():
    prx, ftsv, ddir = sys.argv[1:4]
    d = open(prx, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shnum, shstrndx = struct.unpack_from('<HH', d, 0x30)
    secs = [struct.unpack_from('<10I', d, shoff + i * 40) for i in range(shnum)]
    st = secs[shstrndx]
    name = lambda o: d[st[4] + o:d.index(b'\0', st[4] + o)].decode()
    funcs = set()
    for line in open(ftsv):
        a = line.split('\t')[0]
        if a != 'addr':
            funcs.add(int(a, 16))

    vt = {}   # start addr -> [slots]
    for s in secs:
        if name(s[0]) not in ('.rodata', '.data'):
            continue
        va, fo, size = s[3], s[4], s[5]
        run = []
        for i in range(0, size - 3, 4):
            w, = struct.unpack_from('<I', d, fo + i)
            if w in funcs and w >= 0x40:
                if not run:
                    run_start = va + i
                run.append(w)
            else:
                if len(run) >= 2:
                    vt[run_start] = run
                run = []
        if len(run) >= 2:
            vt[run_start] = run

    # Ghidra may label a vtable from a slot past an RTTI-less header (offset-to-top words), so match ranges
    starts = sorted(vt)
    def owning(addr):
        import bisect
        i = bisect.bisect_right(starts, addr) - 1
        if i >= 0 and starts[i] <= addr < starts[i] + 4 * len(vt[starts[i]]) + 8:
            return starts[i]
        return None

    inst = collections.defaultdict(set)
    pat = re.compile(r'\*\(?[^=;\n]*\b(?:param_\d+|this|[a-z]Var\d+)[^=;\n]*=\s*&PTR_(?:LAB|FUN|DAT)?_?([0-9a-f]{8})')
    for p in glob.glob(os.path.join(ddir, '*.c')):
        fn = os.path.basename(p).rsplit('_', 1)[1][:-2]
        for m in pat.finditer(open(p, errors='replace').read()):
            o = owning(int(m.group(1), 16))
            if o is not None:
                inst[o].add(fn)

    print('vtable\tslots\tinstallers\tfirst_slots')
    for a in starts:
        print(f'{a:08x}\t{len(vt[a])}\t{",".join(sorted(inst[a]))}\t{",".join(f"{x:08x}" for x in vt[a][:6])}')


if __name__ == '__main__':
    main()
