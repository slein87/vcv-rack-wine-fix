#!/usr/bin/env bash
# Runs crtnull.exe for every function in funcs.txt against msvcrt.dll and ucrtbase.dll under Wine.
# The shell only sees the low 8 bits of the exit code: 0x05 = 0xC0000005, 0x17 = 0xC0000417.
set -euo pipefail
cd "$(dirname "$0")"
WINE=${WINE:-wine}; export WINEDEBUG=${WINEDEBUG:--all}
[ -f crtnull.exe ] || ./build.sh
for mode in "" h; do for dll in msvcrt.dll ucrtbase.dll; do for f in $(cat funcs.txt); do
    rc=0; "$WINE" crtnull.exe "|$dll|$f|$mode" > out.tmp 2>/dev/null || rc=$?
    line=$(grep -a "^$dll" out.tmp | head -1 | tr -d '\r')
    case "$line" in *returned*|*missing*) ;; *) line="${line:-$dll $f} -> CRASHED";; esac
    printf '%s | exit=0x%02X\n' "$line" "$rc"
done; done; done
rm -f out.tmp
