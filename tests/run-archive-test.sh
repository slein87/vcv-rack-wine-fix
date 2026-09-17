#!/usr/bin/env bash
# Calls rack::system::archiveDirectory() from a libRack.dll under Wine and compares the
# archive with the source directory. Close the plugin host first.
#   usage: run-archive-test.sh /path/to/libRack.dll
# WINE and WINEPREFIX are taken from the environment (defaults: wine, ~/.wine).
set -euo pipefail
cd "$(dirname "$0")"
LIB=$(readlink -f "$1"); WINE=${WINE:-wine}; export WINEDEBUG=${WINEDEBUG:--all}
[ -f archive-test.exe ] || ./build.sh
T=$(mktemp -d); mkdir -p "$T/in/modules"; echo '{}' > "$T/in/patch.json"
head -c 200000 /dev/urandom | base64 > "$T/in/modules/large.txt"    # more than one 64 KB read
"$WINE" archive-test.exe "|a|Z:${LIB//\//\\}|Z:${T//\//\\}\\in" 2>&1 | grep -aE 'calling|returned|page fault|failed' || true
if [ -s "$T/in.tar.zst" ] && mkdir "$T/out" && tar --zstd -xf "$T/in.tar.zst" -C "$T/out" && diff -r "$T/in" "$T/out"; then
    echo "OK: archive matches the source directory"
else
    echo "FAILED: no usable archive (crash in fread(NULL)?)"
fi
rm -rf "$T"
