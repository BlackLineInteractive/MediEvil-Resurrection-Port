#!/bin/sh
# print decompiled functions compactly: dc.sh ADDR...
cd "$(dirname "$0")/../decomp/ghidra_full"
for a in "$@"; do echo "== $a"; sed -n '/^{/,$p' FUN_${a}_*.c | grep -v '^\s*$' | grep -Ev '^  (undefined[0-9]*|int|uint|float|bool|char|byte|short|ushort|code) \**[a-zA-Z_][a-zA-Z0-9_]*( \[[0-9]+\])?;$' | sed '1d;$d'; done
