#!/bin/sh
# Run the translated game for N seconds and print the host backtrace of where it is (guest f_ frames).
cd "$(dirname "$0")/.." || exit 1
./build/medievil-gametest extracted/PSP_GAME/USRDIR/prx/main.prx main > build/run.log 2>&1 & P=$!
sleep "${1:-8}"
lldb -p $P --batch -o 'bt 40' 2>/dev/null | grep -o 'f_[0-9a-f]\{8\}([^)]*)\|game_[0-9]*\.cpp:[0-9]*' | paste - - | awk '!s[$0]++' | head -${2:-20}
kill -9 $P 2>/dev/null
