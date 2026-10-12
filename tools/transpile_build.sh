#!/bin/sh
# Regenerate decomp/cpp and syntax-check every chunk into build/tx/*.err (parallel).
cd "$(dirname "$0")/.." || exit 1
python3 tools/transpile.py "$@" || exit 1
mkdir -p build/tx; rm -f build/tx/*.err
ls decomp/cpp/game_*.cpp | xargs -P 10 -I{} sh -c 'clang++ -std=c++17 -fsyntax-only -ferror-limit=0 -w -Isrc -Idecomp/cpp {} 2> build/tx/$(basename {}).err; true'
echo "errors: $(cat build/tx/*.err | grep -c 'error:')"
python3 tools/transpile_fix.py --dry
