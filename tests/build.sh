#!/usr/bin/env bash
# Builds both Windows test programs with clang + lld. No mingw and no Windows SDK needed.
set -euo pipefail
cd "$(dirname "$0")"
llvm-dlltool -m i386:x86-64 -d kernel32.def -l kernel32.lib
for n in archive-test crtnull; do
    clang --target=x86_64-pc-windows-msvc -O1 -ffreestanding -fno-stack-protector -fno-builtin -c $n.c -o $n.o
    lld-link /entry:entry /subsystem:console /nodefaultlib $n.o kernel32.lib /out:$n.exe
done
ls -la archive-test.exe crtnull.exe
