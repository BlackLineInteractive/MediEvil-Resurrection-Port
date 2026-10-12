#!/usr/bin/env python3
"""Ghidra decompilation of main.prx -> compilable C++ (src/Decomp/Runtime.h model).

    tools/transpile.py [--in decomp/transpile_in] [--out decomp/cpp] [--chunks 128]
                       [--stub-list decomp/cpp/stubbed.txt]

Input is tools/ghidra_scripts/ExportForTranspile.java output. Every function becomes f_<addr>; data
stays at its main.prx address in guest memory. Functions listed in the stub list (filled in by
tools/transpile_fix.py from compiler errors) are emitted as Unimplemented() calls.
"""
import argparse, os, re, sys
from collections import defaultdict

BASE_TYPES = {
    'void', 'bool', 'char', 'short', 'int', 'long', 'float', 'double', 'float10', 'uchar', 'ushort', 'uint',
    'ulong', 'longlong', 'ulonglong', 'byte', 'sbyte', 'word', 'dword', 'qword', 'code', 'string', 'pointer',
    'undefined', 'undefined1', 'undefined2', 'undefined3', 'undefined4', 'undefined5', 'undefined6',
    'undefined7', 'undefined8', 'wchar_t', 'char16_t', 'wchar16', 'unsigned', 'signed', 'sdword', 'uint3',
    'int3', 'uint5', 'uint6', 'uint7', 'int5', 'int6', 'int7', 'ImageBaseOffset32', 'addr',
}
TYPEDEFS = '''using undefined = uint8_t; using undefined1 = uint8_t; using undefined2 = uint16_t; using undefined3 = uint32_t;
using undefined4 = ::psp::U4; using undefined5 = uint64_t; using undefined6 = uint64_t; using undefined7 = uint64_t;
using undefined8 = uint64_t; using uchar = uint8_t; using ushort = uint16_t; using uint = uint32_t; using ulong = uint32_t;
using longlong = int64_t; using ulonglong = uint64_t; using byte = uint8_t; using sbyte = int8_t; using word = uint16_t;
using dword = uint32_t; using qword = uint64_t; using float10 = double; using string = char; using pointer = ::psp::GP<uint8_t>;
using uint3 = uint32_t; using int3 = int32_t; using uint5 = uint64_t; using uint6 = uint64_t; using uint7 = uint64_t;
using int5 = int64_t; using int6 = int64_t; using int7 = int64_t; using sdword = int32_t; using wchar16 = uint16_t;
using ImageBaseOffset32 = uint32_t; using addr = uint32_t;
using code = ::psp::code; using U4 = ::psp::U4; template <class T> using GP = ::psp::GP<T>; template <int N> using VB = ::psp::VB<N>;
'''
# Ghidra's "long" is 4 bytes on this target
LONG_FIX = [(re.compile(r'\bunsigned long\b'), 'uint'), (re.compile(r'\blong long\b'), 'longlong'),
            (re.compile(r'\blong\b'), 'int')]

ID = r'[A-Za-z_][A-Za-z0-9_]*'
TOKEN = re.compile(r'\s+|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|'
                   r'0[xX][0-9a-fA-F]+[uUlL]*|\d+\.?\d*(?:[eE][-+]?\d+)?[fFuUlL]*|' + ID + r'|\S', re.S)


def cxx_type(t, unknown_types):
    """Ghidra type text ('int *', 'undefined1[16]', 'undefined * *', 'struct Foo') -> C++ type."""
    t = t.strip().replace('struct ', '').replace('const ', '')
    for r, s in LONG_FIX:
        t = r.sub(s, t)
    m = re.match(r'^(.*?)\s*\[(\d+)\]$', t)
    if m:
        el = m.group(1).strip()
        es = 1 if el in ('undefined1', 'undefined', 'byte', 'char', 'uchar') else 2 if el in ('short', 'ushort', 'undefined2') else 4
        return 'VB<%d>' % (int(m.group(2)) * es)
    stars = t.count('*')
    base = t.replace('*', '').strip()
    if base not in BASE_TYPES:
        unknown_types.add(base)
        base = 'undefined'
    out = base
    for _ in range(stars):
        out = 'GP<%s>' % out
    return out


def scalar_for_size(size):
    return {1: 'uint8_t', 2: 'uint16_t', 4: 'U4', 8: 'uint64_t'}.get(size, 'uint8_t')


def global_access(name, addr, size, gtype, unknown):
    """-> (value expr, address expr)"""
    a = '0x%xu' % addr
    gtype = gtype.strip()
    m = re.match(r'^(.*?)\s*\[(\d+)\]$', gtype)
    if m or gtype == 'string':
        el = cxx_type(m.group(1), unknown) if m else 'char'
        if el in ('undefined', 'undefined1'):
            el = 'uint8_t'
        return 'GP<%s>(%s)' % (el, a), 'GP<%s>(%s)' % (el, a)
    if gtype in ('undefined',):
        t = scalar_for_size(size)
    elif gtype in ('pointer', 'undefined *'):
        t = 'GP<uint8_t>'
    else:
        t = cxx_type(gtype, unknown) or scalar_for_size(size)
    return '(*GP<%s>(%s))' % (t, a), 'GP<%s>(%s)' % (t, a)


def parse_params(s):
    s = s.strip()
    if s in ('', 'void'):
        return []
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '(<[':
            depth += 1
        elif ch in ')>]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


class Func:
    pass


def load(indir):
    funcs = {}
    names = {}
    dup = set()
    for line in open(os.path.join(indir, 'functions.tsv')):
        p = line.rstrip('\n').split('\t')
        if len(p) >= 2:
            if p[1] in names or p[1].startswith(('caseD_', 'switchD_', 'LAB_')):
                dup.add(p[1])
            names[p[1]] = int(p[0], 16)
    for d in dup:
        names.pop(d, None)
    for fn in sorted(os.listdir(indir)):
        if not fn.endswith('.c'):
            continue
        txt = open(os.path.join(indir, fn), encoding='utf-8', errors='replace').read()
        f = Func()
        f.addr = int(fn[:8], 16)
        f.globals, f.locals = [], []
        body = []
        for ln in txt.split('\n'):
            if ln.startswith('//@F '):
                _, a, sz, nm = ln.split(' ', 3)
                f.size, f.name = int(sz), nm
            elif ln.startswith('//@G '):
                p = ln[5:].split(' ', 3)
                if p[1] != '?':
                    f.globals.append((p[0], int(p[1], 16), int(p[2]), p[3]))
            elif ln.startswith('//@L '):
                p = ln[5:].split(' ', 3)
                f.locals.append((p[0], int(p[1]), int(p[2]), p[3]))
            elif ln.startswith('//@P '):
                pass
            else:
                body.append(ln)
        f.text = '\n'.join(body)
        f.nid = re.search(r'/\* (\w+) NID (0x[0-9A-Fa-f]+) \*/', f.text)
        funcs[f.addr] = f
    return funcs, names


def find_header(text, name):
    """Return (ret, params, start, end-of-header index) of the definition of `name`."""
    m = re.search(r'^([^\n;{}]*?)(?:\b\w+::)?\b' + re.escape(name) + r'\s*\(([^)]*(?:\([^)]*\)[^)]*)*)\)\s*\n\s*\{', text, re.M)
    if not m:
        return None
    return m.group(1).strip(), m.group(2), m.start(), m.end() - 1


def rewrite_types(code, unknown):
    """T *... -> GP<T>, T (*) [N] -> GP<VB<N>> (in declarations and casts)."""
    # bit-reinterpreting casts marked by ExportForTranspile.java
    code = re.sub(r'\(\s*__bc_(' + ID + r')\s*\)', r'(::psp::BC<\1>)', code)
    code = re.sub(r'\b__bc_(?=' + ID + r')', '', code)
    for r, s in LONG_FIX:
        code = r.sub(s, code)
    def arr_ptr(m):
        n = int(m.group(2)) * (1 if m.group(1) in ('undefined1', 'undefined', 'byte', 'char', 'uchar') else 2 if m.group(1) in ('short', 'ushort', 'undefined2') else 4)
        out = 'VB<%d>' % n
        for _ in range(max(1, m.group(0).count('*'))):
            out = 'GP<%s>' % out
        return out
    code = re.sub(r'\b(' + ID + r')\s*\(\s*(?:\*\s*)+\)\s*\[(\d+)\]', arr_ptr, code)
    def arr_ptr_named(m):
        n = int(m.group(4)) * (1 if m.group(1) in ('undefined1', 'undefined', 'byte', 'char', 'uchar') else 2 if m.group(1) in ('short', 'ushort', 'undefined2') else 4)
        out = 'VB<%d>' % n
        for _ in range(m.group(2).count('*')):
            out = 'GP<%s>' % out
        return out + ' ' + m.group(3)
    code = re.sub(r'\b(' + ID + r')\s*\(\s*((?:\*\s*)+)(' + ID + r')\s*\)\s*\[(\d+)\]', arr_ptr_named, code)
    def ptr(m):
        base, stars = m.group(1), m.group(2).count('*')
        if base not in BASE_TYPES:
            if base in unknown_struct_names:
                pass
            else:
                return m.group(0)
            base = 'undefined'
        out = base
        for _ in range(stars):
            out = 'GP<%s>' % out
        return out + ' '
    code = re.sub(r'\b(' + ID + r')\s*((?:\*\s*)+)(?=[A-Za-z_)(\[])', ptr, code)
    return code


unknown_struct_names = set()


def match_paren_back(s, i):
    """s[i] == ')': index of the matching '('."""
    depth = 0
    while i >= 0:
        if s[i] == ')':
            depth += 1
        elif s[i] == '(':
            depth -= 1
            if depth == 0:
                return i
        i -= 1
    return -1


def rewrite_fields(code):
    """x._A_B_ -> Fld<A,B>(x)"""
    out = code
    pat = re.compile(r'\._(\d+)_(\d+)_')
    while True:
        m = pat.search(out)
        if not m:
            return out
        end = m.start()
        j = end - 1
        if out[j] == ')':
            st = match_paren_back(out, j)
            # include a preceding '*' deref inside the parens already; also a cast/call name before '('
            k = st
            while k > 0 and re.match(r'[A-Za-z0-9_:>]', out[k - 1]):
                k -= 1
            st = k
        elif out[j] == ']':
            depth = 0
            k = j
            while k >= 0:
                if out[k] == ']':
                    depth += 1
                elif out[k] == '[':
                    depth -= 1
                    if depth == 0:
                        break
                k -= 1
            st = k
            while st > 0 and re.match(r'[A-Za-z0-9_]', out[st - 1]):
                st -= 1
        else:
            st = j
            while st > 0 and re.match(r'[A-Za-z0-9_]', out[st - 1]):
                st -= 1
        base = out[st:end]
        out = out[:st] + '::psp::Fld<%s,%s>(%s)' % (m.group(1), m.group(2), base) + out[m.end():]


def stack_local_type(ltype, size, unknown):
    ltype = ltype.strip()
    m = re.match(r'^(.*?)\s*\[(\d+)\]$', ltype)
    if m:
        el = m.group(1).strip()
        n = int(m.group(2))
        if el in ('undefined1', 'undefined', 'byte', 'char', 'uchar') or True:
            return 'arr', size, (cxx_type(el, unknown) or 'uint8_t')
    t = cxx_type(ltype, unknown)
    if t is None or t == 'undefined' and size != 1:
        t = scalar_for_size(size)
    return 'val', size, t


def gen_function(f, names, faddr_of, unknown, stub):
    hdr = find_header(f.text, f.name)
    if hdr is None:
        return None, None
    ret, params, hs, he = hdr
    ret_c = rewrite_types(ret + ' x', unknown)[:-2].strip() or 'void'
    plist = [rewrite_types(p, unknown) for p in parse_params(params)]
    variadic = any(p.strip() == '...' for p in plist)
    plist = [p for p in plist if p.strip() != '...']
    if ret_c == 'void' and re.search(r'\breturn\s+[^;\s]', f.text[he:]):
        ret_c = '::psp::RV'
    sig = '%s f_%08x(%s)' % (ret_c, f.addr, ', '.join(plist))
    if f.nid or f.size <= 8 and re.search(r'NID', f.text):
        return None, None   # import stub: generated separately
    if stub:
        body = '{ ::psp::Unimplemented(0x%08xu, "%s"); %s }' % (f.addr, f.name, '' if ret_c == 'void' else 'return {};')
        return sig, body
    body = f.text[he:]
    # strip Ghidra comments early (they may contain quotes)
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    # 1. field access before anything renames identifiers
    body = rewrite_fields(body)
    # 2. types in declarations and casts
    body = rewrite_types(body, unknown)
    # declarations of value arrays: undefined1 auVar1 [16]; -> VB<16> auVar1;
    body = re.sub(r'^(\s*)(undefined1|undefined|byte)\s+(' + ID + r')\s*\[(\d+)\];', r'\1VB<\4> \3;', body, flags=re.M)
    body = re.sub(r'^(\s*)(undefined4|uint|int|float)\s+(' + ID + r')\s*\[(\d+)\];', lambda m: '%sVB<%d> %s;' % (m.group(1), int(m.group(4)) * 4, m.group(3)), body, flags=re.M)
    # 3. stack locals live in guest memory
    stack = {}
    minoff = 0
    for (nm, off, size, lt) in f.locals:
        kind, sz, t = stack_local_type(lt, size, unknown)
        stack[nm] = (kind, off, sz, t)
        minoff = min(minoff, off)
    for nm in stack:
        body = re.sub(r'^\s*[^;\n()=]*\b' + re.escape(nm) + r'\b\s*(\[\d+\]\s*)*;[ \t]*\n', '', body, flags=re.M)
    uses_frame = [False]
    globs = {}
    for (nm, a, size, gt) in f.globals:
        globs[nm] = global_access(nm, a, size, gt, unknown)
    # 4. token pass: identifiers -> globals / locals / functions; strings -> guest strings
    out = []
    toks = TOKEN.findall(body)
    i = 0
    prev = ''
    def last_sig(k=1):
        n = 0
        for j in range(len(out) - 1, -1, -1):
            if not out[j].isspace():
                n += 1
                if n == k:
                    return j
        return -1
    while i < len(toks):
        t = toks[i]
        if t.isspace():
            out.append(t); i += 1; continue
        j = i + 1
        while j < len(toks) and toks[j].isspace():
            j += 1
        nxt = toks[j] if j < len(toks) else ''
        if t.startswith('"'):
            out.append(t if prev == 'STRNCPY' else '::psp::GStr(%s)' % t)
        elif re.match(ID, t):
            if t == 'builtin_strncpy':
                out.append('::psp::builtin_strncpy'); prev = 'STRNCPY_PEND'; i += 1; continue
            l1, l2 = last_sig(1), last_sig(2)
            addr_of = l1 >= 0 and out[l1] == '&' and (l2 < 0 or not re.match(r'[A-Za-z0-9_)\]]', out[l2][-1:])
                                                     or out[l2] == ')' and re.search(r'\(\s*(?:' + '|'.join(sorted(BASE_TYPES)) + r'|GP<[^()]*>)\s*\)$', ''.join(out[max(0, l2 - 12):l2 + 1])))
            if addr_of:
                del out[l1 + 1:]
            if t in stack:
                kind, off, sz, ty = stack[t]
                a = '(fr.base%+d)' % off
                if kind == 'arr' and ty in ('uint8_t', 'undefined1', 'undefined') and sz in (4, 8, 12, 16, 64):
                    e = ('GP<VB<%d>>(%s)' if addr_of else '(*GP<VB<%d>>(%s))') % (sz, a)
                    if addr_of: out.pop()
                    out.append(e)
                elif kind == 'arr':
                    e = 'GP<%s>(%s)' % (ty, a)
                    if addr_of: out.pop()
                    out.append(e)
                elif addr_of:
                    out.pop(); out.append('GP<%s>(%s)' % (ty, a))
                else:
                    out.append('(*GP<%s>(%s))' % (ty, a))
            elif t in globs:
                val, adr = globs[t]
                if addr_of:
                    out.pop(); out.append(adr)
                else:
                    out.append(val)
            elif nxt == ':' and prev in (';', '{', '}', ''):
                out.append(t)
            elif re.match(r'^_(DAT|FUN|PTR)_[0-9a-f]{8}$', t):
                out.append('(*GP<U4>(0x%xu))' % int(t[-8:], 16))
            elif re.match(r'^[a-z]Ram[0-9a-f]{8}$', t):
                ty = {'i': 'int', 'f': 'float', 'u': 'uint', 'c': 'char', 'p': 'GP<uint8_t>', 's': 'short'}.get(t[0], 'U4')
                if addr_of: out.pop()
                out.append(('GP<%s>(0x%xu)' if addr_of else '(*GP<%s>(0x%xu))') % (ty, int(t[-8:], 16)))
            elif re.match(r'^stack0x[0-9a-f]{8}$', t):
                uses_frame[0] = True
                if addr_of: out.pop()
                out.append(('GP<U4>(fr.base+0x%x)' if addr_of else '(*GP<U4>(fr.base+0x%x))') % int(t[-8:], 16))
            elif re.match(r'^_local_\w+$', t) and t[1:] in stack:
                kind, off, sz, ty = stack[t[1:]]
                out.append('(*GP<U4>(fr.base%+d))' % off)
            elif t in names and t not in KEYWORDS and t not in stack:
                a = names[t]
                if nxt == '(':
                    out.append('::psp::Caller<&f_%08x>{}' % a)
                else:
                    if addr_of: out.pop()
                    out.append('0x%xu' % a)
            elif t.startswith('func_0x') and nxt == '(':
                out.append('::psp::CodeRef{0x%xu}' % int(t[7:], 16))
            elif re.match(r'^(DAT|PTR|s|u|switchD|LAB|caseD|fde|cie|PTR_s|PTR_FUN|PTR_DAT|PTR_LAB)_[0-9a-f]{8}$', t) or re.match(r'^(DAT|PTR|s|u)_\w*_?[0-9a-f]{8}$', t):
                a = int(t[-8:], 16)
                if t.startswith('LAB_') and (nxt == ':' or prev == 'goto'):
                    out.append(t)
                else:
                    if addr_of: out.pop()
                    out.append(('0x%xu' if addr_of or t.startswith(('LAB_', 'caseD_', 'switchD_')) else '(*GP<U4>(0x%xu))') % a)
            else:
                out.append(t)
        else:
            out.append(t)
            if prev == 'STRNCPY_PEND' and t == ',':
                prev = 'STRNCPY'; i += 1; continue
        prev = t if prev not in ('STRNCPY_PEND',) else prev
        if prev == 'STRNCPY' and t.startswith('"'):
            prev = ''
        i += 1
    body = ''.join(out)
    body = re.sub(r'\(\*\(\*GP<GP<uint8_t>>\((0x[0-9a-f]+u)\)\)\)\(', r'(*GP<GP<code>>(\1))(', body)
    frame = ''
    if stack or uses_frame[0]:
        frame = '\n  ::psp::Frame fr(%du);' % (-minoff + 16)
    body = body.replace('{', '{' + frame, 1)
    return sig, body


KEYWORDS = {'if', 'else', 'while', 'for', 'do', 'switch', 'case', 'default', 'return', 'goto', 'break', 'continue',
            'sizeof', 'void', 'int', 'float', 'char'}


def join_tokens(toks):
    s = []
    for t in toks:
        if s and re.match(r'[A-Za-z0-9_]', s[-1][-1:]) and re.match(r'[A-Za-z0-9_"\']', t[:1]):
            s.append(' ')
        s.append(t)
        if t in (';', '{', '}') or t.startswith('//'):
            s.append('\n')
    return ''.join(s)


def thunk_for(f_addr, ret_c, plist):
    lines = ['static ::psp::CallRet t_%08x(const ::psp::CallRegs& r) {' % f_addr, '  int ni = 0, nf = 0; (void)ni; (void)nf;']
    args = []
    for k, p in enumerate(plist):
        m = re.match(r'^(.*?)(' + ID + r')$', p.strip())
        ty = m.group(1).strip() if m else p
        if not ty:
            ty = 'int'
        lines.append('  auto a%d = ::psp::ArgFrom<%s>(r, ni, nf);' % (k, ty))
        args.append('a%d' % k)
    call = 'f_%08x(%s)' % (f_addr, ', '.join(args))
    if ret_c == 'void':
        lines.append('  %s; return {};' % call)
    else:
        lines.append('  auto v = %s; ::psp::CallRet c; c.v = ::psp::UB(v);' % call)
        lines.append('  if constexpr (std::is_floating_point_v<decltype(v)>) c.f = float(v);')
        lines.append('  return c;')
    lines.append('}')
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--in', dest='indir', default='decomp/transpile_in')
    ap.add_argument('--out', default='decomp/cpp')
    ap.add_argument('--chunks', type=int, default=128)
    ap.add_argument('--stub-list', default=None)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    stub_list = args.stub_list or os.path.join(args.out, 'stubbed.txt')
    stubbed = set()
    if os.path.exists(stub_list):
        stubbed = {int(l.split()[0], 16) for l in open(stub_list) if l.strip()}
    funcs, names = load(args.indir)
    unknown = set()
    decls, imports, gen, thunks = [], [], {}, []
    for a, f in sorted(funcs.items()):
        if f.nid:
            imports.append(f)
            continue
        sig, body = gen_function(f, names, None, unknown, a in stubbed)
        if sig is None:
            stubbed_reason = 'noheader'
            hdr_sig = 'void f_%08x()' % a
            gen[a] = (hdr_sig, '{ ::psp::Unimplemented(0x%08xu, "%s"); }' % (a, f.name), f.name)
            decls.append(hdr_sig + ';')
            thunks.append('static ::psp::CallRet t_%08x(const ::psp::CallRegs&) { f_%08x(); return {}; }' % (a, a))
            continue
        gen[a] = (sig, body, f.name)
        decls.append(sig + ';')
        m = re.match(r'^(.*?)\bf_[0-9a-f]{8}\((.*)\)$', sig, re.S)
        thunks.append(thunk_for(a, m.group(1).strip(), parse_params(m.group(2))))
    # every name Ghidra knows but which has no body (externals, failed decompiles) gets an import-style stub
    have = set(funcs)
    for nm, a in names.items():
        if a not in have:
            f = Func(); f.addr = a; f.name = nm; f.nid = None; imports.append(f)
    imp_sig = 'U4 f_%08x(U4 a0 = {}, U4 a1 = {}, U4 a2 = {}, U4 a3 = {}, U4 a4 = {}, U4 a5 = {}, U4 a6 = {}, U4 a7 = {})'
    for f in imports:
        decls.append(imp_sig % f.addr + ';')
        thunks.append('static ::psp::CallRet t_%08x(const ::psp::CallRegs& r) { ::psp::CallRet c; c.v = uint32_t(f_%08x(r.i[0], r.i[1], r.i[2], r.i[3], r.i[4], r.i[5], r.i[6], r.i[7])); return c; }' % (f.addr, f.addr))

    with open(os.path.join(args.out, 'decls.h'), 'w') as w:
        w.write('// generated by tools/transpile.py - do not edit\n#pragma once\n#include "Decomp/Runtime.h"\n'
                '#include "Decomp/Caller.h"\nnamespace psp::game {\n' + TYPEDEFS + '\n'.join(sorted(set(decls))) + '\n}\n')
    with open(os.path.join(args.out, 'imports.cpp'), 'w') as w:
        w.write('// generated: import stubs (weak; src/Decomp/Hle.cpp overrides by defining the same f_<addr>)\n'
                '#include "decls.h"\n#include <cstdio>\nnamespace psp::game {\n')
        for f in imports:
            label = (f.nid.group(1) + ' ' + f.nid.group(2)) if f.nid else 'no body'
            w.write('__attribute__((weak)) ' + imp_sig.replace(' = {}', '') % f.addr +
                    ' { ::psp::Unimplemented(0x%08xu, "%s (%s)"); return {}; }\n' % (f.addr, f.name, label))
        w.write('}\n')
    with open(os.path.join(args.out, 'thunks.cpp'), 'w') as w:
        w.write('// generated: register-image thunks for calls by address\n#include "decls.h"\n#include <algorithm>\n'
                '#include <vector>\nnamespace psp::game {\n' + '\n'.join(thunks) + '\n}\nnamespace psp {\n'
                'struct ThunkEntry { uint32_t a; Thunk t; };\nstatic const ThunkEntry kThunks[] = {\n')
        allf = sorted([a for a in gen] + [f.addr for f in imports])
        for a in allf:
            w.write('  {0x%08xu, game::t_%08x},\n' % (a, a))
        w.write('};\nThunk FindThunk(uint32_t addr) {\n  auto it = std::lower_bound(std::begin(kThunks), std::end(kThunks), addr,'
                ' [](const ThunkEntry& e, uint32_t a) { return e.a < a; });\n'
                '  return (it != std::end(kThunks) && it->a == addr) ? it->t : nullptr;\n}\n}\n')
    keys = sorted(gen)
    per = (len(keys) + args.chunks - 1) // args.chunks
    index = []
    for c in range(args.chunks):
        part = keys[c * per:(c + 1) * per]
        if not part:
            break
        path = os.path.join(args.out, 'game_%03d.cpp' % c)
        line = 1
        buf = ['// generated by tools/transpile.py - do not edit\n#include "decls.h"\nnamespace psp::game {\n']
        line += 3
        for a in part:
            sig, body, nm = gen[a]
            text = '// %s\n%s\n%s\n\n' % (nm, sig, body)
            n = text.count('\n')
            index.append('%s\t%08x\t%d\t%d' % (os.path.basename(path), a, line, line + n - 1))
            buf.append(text)
            line += n
        buf.append('}\n')
        new = ''.join(buf)
        if not os.path.exists(path) or open(path).read() != new:
            open(path, 'w').write(new)
    open(os.path.join(args.out, 'lines.tsv'), 'w').write('\n'.join(index) + '\n')
    print('functions %d (stubbed %d), imports %d, unknown types %d: %s' % (
        len(gen), len(stubbed), len(imports), len(unknown), ' '.join(sorted(unknown)[:30])))


if __name__ == '__main__':
    main()
