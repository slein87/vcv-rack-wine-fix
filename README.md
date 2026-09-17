# vcv-rack-wine-fix

Fix for VCV Rack 2 (Windows build) crashing under Wine, including the case where the Rack
plugin takes down Ableton Live the moment it is dropped on a track.

## TL;DR

- **Symptom:** Under Wine, the Windows build of VCV Rack 2 (Free and Pro) crashes on "Save".
  Loaded as a VST3/VST2/CLAP plugin it crashes the host as soon as it is instantiated. Ableton
  Live 12 reports "A serious program error has occurred" and quits. Cardinal has the same problem
  ([#275](https://github.com/DISTRHO/Cardinal/issues/275), [#854](https://github.com/DISTRHO/Cardinal/issues/854)).
- **Cause:** Two bugs meet. Rack calls `fopen()` on a directory, gets `NULL`, and passes it
  unchecked to `fread()`
  ([`src/system.cpp`, `archiveDirectory()`](https://github.com/VCVRack/Rack/blob/v2/src/system.cpp#L477)).
  Microsoft's `msvcrt.dll` answers a `NULL` stream with "0 bytes read, `errno = EINVAL`". Wine's
  `msvcrt` locks the stream before looking at it and faults in
  `RtlEnterCriticalSection(NULL + 0x30)`.
- **Fix 1, available today:** `./vcv-rack-wine-fix` patches 58 bytes in `libRack.dll` so the read
  loop is skipped for a `NULL` stream. It keeps a backup and can be reverted.
- **Fix 2, the proper one:** Wine's `msvcrt` should validate the stream the way Windows does.
  [`wine/`](wine/) holds a tested diff that does this for 19 stdio functions. With it an
  unmodified `libRack.dll` works. The diff is AI-generated and therefore not submitted to Wine,
  see [AI disclosure](#ai-disclosure).
- **Who is affected:** anyone who runs the *Windows* build of Rack under Wine. In practice that
  means people running a Windows DAW under Wine (Ableton Live, FL Studio), because such a host
  cannot load the native Linux plugin. Nothing here is specific to Ableton.

## Quick start

Close the DAW and Rack first.

```bash
git clone https://github.com/slein87/vcv-rack-wine-fix
cd vcv-rack-wine-fix
./vcv-rack-wine-fix --check     # reports whether your libRack.dll needs the patch
./vcv-rack-wine-fix             # patches it, original is kept as libRack.dll.orig
./vcv-rack-wine-fix --revert    # restores the original
```

Without arguments the script looks for `Rack2Pro` and `Rack2Free` in `$WINEPREFIX`, `~/.wine`
and `~/.wine-ableton` (the default prefix of
[ableton-linux](https://github.com/shibco/ableton-linux)). Use `--prefix DIR` or pass the path
to `libRack.dll` for anything else. Python 3 is the only requirement.

The VST3, VST2 and CLAP plugins and `Rack.exe` all load the same `libRack.dll`, so one patch
covers every way of running Rack.

**After every Rack update the installer replaces `libRack.dll`. Run the script again.** It finds
the code by byte pattern and changes nothing unless the pattern matches exactly once, so a build
it does not recognise is left alone. If it reports that the pattern is gone, run
`tests/run-archive-test.sh`. When that passes on an unpatched DLL, VCV has fixed the bug and
this repository is obsolete for you.

Tested with Rack Free 2.6.6 and Rack Pro 2.6.6 (win-x64) on Wine 11.13.

## The problem in detail

### What Rack does

A `.vcv` patch is a Zstandard compressed tar archive of Rack's autosave directory.
`rack::system::archiveDirectory()` walks that directory with libarchive and copies every entry
into the archive:

```cpp
FILE* f = std::fopen(entrySourcePath.c_str(), "rb");
DEFER({std::fclose(f);});
char buf[1 << 16];
ssize_t len;
while ((len = std::fread(buf, 1, sizeof(buf), f)) > 0) {
	archive_write_data(a, buf, len);
}
```

libarchive also returns the directories, and the first entry is always the directory itself.
`fopen()` on a directory fails, so `f` is `NULL` at least once on every call, and nothing checks
it. The standalone application reaches this code when you save a patch. The plugin reaches it
right after "Loading template patch", which is why the host dies on load.

### What Windows does

Measured on Windows 11 (build 26200) with [`tests/crtnull.c`](tests/crtnull.c), one process per
function so that a crash cannot hide the rest:

| CRT | `NULL` stream |
|---|---|
| `msvcrt.dll` | all 25 tested functions return an error and set `errno = EINVAL` (`fread`/`fwrite` 0, `fgets` `NULL`, `fgetc`/`fputc`/`fputs` `EOF`, wide variants `WEOF`, `ftell`/`fseek`/`_fileno` -1, `feof`/`ferror` 0). `_fseeki64` is the one exception and crashes on Windows too |
| `ucrtbase.dll` | default: the process is terminated (`0xC0000409`). With an invalid parameter handler installed: error return and `EINVAL`, as above |

Rack is built with mingw against `msvcrt.dll`, so on Windows the `NULL` stream is silently
tolerated and nobody ever noticed the bug.

### What Wine does

In Wine 11.13, 21 of those 25 functions crash with `0xC0000005` (the code in current Wine master
is the same). Only
`fclose`, `ungetc`, `ungetwc` and `setvbuf` validate the stream. `fread()` looks like this:

```c
_lock_file(file);
ret = _fread_nolock(ptr, size, nmemb, file);
```

The lock lives at offset `0x30` of Wine's `FILE` structure, so a `NULL` stream ends in
`RtlEnterCriticalSection(0x30)` and a read from address `0x50`.

### Why it went unnoticed

- Correct programs check the result of `fopen()`, so they never call `fread(NULL)`.
- Programs built against the UCRT (Visual Studio 2015 and later) already die on Windows, so
  their authors fix the bug before anyone runs them under Wine. Only programs linked against the
  old `msvcrt.dll` survive on Windows and crash under Wine. That is mostly mingw builds.
- Rack has a native Linux version. The Windows build under Wine is only needed when the host
  itself runs under Wine, and running Ableton Live that way has become practical only recently.
- The crash is misleading. It happens inside `ntdll`, right after a burst of GCC exceptions
  with a "collided unwind" in the log. That looks like a bug in Wine's exception handling. Those
  exceptions are unrelated and handled correctly.
- Cardinal ships a workaround (`CARDINAL_UNDER_WINE`) that opens `Z:\dev\null` when `fopen()`
  fails. That hides the crash on load, but files that failed to open are archived empty.

### How to recognise it in a log

```
VCV Rack: Loading template patch
...
seh:dispatch_exception code=c0000005 (EXCEPTION_ACCESS_VIOLATION) ... addr=<ntdll>+0x...
seh:dispatch_exception  info[0]=0000000000000000
seh:dispatch_exception  info[1]=0000000000000050
... rcx=0000000000000030 ...
```

The faulting address resolves to `RtlEnterCriticalSection` (`winedump -j export ntdll.dll`).
The pair `rcx=0x30`, read from `0x50` is the signature. Live's own log only says
`EXCEPTION_ACCESS_VIOLATION`.

## Verification

| Run | Result |
|---|---|
| `msvcrt!fread(buf, 1, 16, NULL)` from a freestanding EXE, Wine 11.13 | page fault at the same `ntdll` address as the crash in Live |
| the same with unmodified Wine 11.17 (distribution package, fresh prefix) | same crash, also for `fwrite`, `fgets`, `fgetc`, `fputs`, `ftell`, `feof`. `fclose` returns `EOF` |
| `archiveDirectory()` from unmodified `libRack.dll` (Free and Pro 2.6.6), Wine 11.13 | same page fault |
| `archiveDirectory()` from patched `libRack.dll`, Wine 11.13 | returns, archive is identical to the source directory (including a file larger than one 64 KB read) |
| Rack Pro 2.6.6 VST3 with patched `libRack.dll` inside Ableton Live 12.4.6 (ableton-linux, Wine 11.13) | plugin loads, audio works, Live set and `.vcv` patch save, and both come back after restarting Live. The saved `.vcv` contains the full `patch.json`. Before the patch: five crashes in five attempts |
| Wine's own `msvcrt_test.exe file` with the new test, **Windows 11**, native `msvcrt.dll` | 1151 tests, 0 failures. The expected values in the new test match Windows |
| same test, Wine 11.13 unpatched | crashes in the new test |
| same test, Wine 11.13 with patched `msvcrt.dll` | 1147 tests, 0 failures |
| **unmodified** `libRack.dll` with patched Wine `msvcrt.dll` | `archiveDirectory()` returns, archive identical |

Raw output is in [`results/`](results/). Everything can be reproduced with the scripts in
[`tests/`](tests/), which build with clang and lld alone. No mingw and no Windows SDK is needed.

```bash
tests/build.sh
WINEPREFIX=~/.wine tests/run-archive-test.sh "/path/to/libRack.dll"
WINEPREFIX=~/.wine tests/run-crtnull-wine.sh
# on Windows: copy tests/ over and run run-crtnull-windows.bat
```

## The Wine patch

[`wine/msvcrt-null-stream.diff`](wine/) is **not submitted to Wine**, see
[AI disclosure](#ai-disclosure) and [`wine/README.md`](wine/README.md). It adds
`MSVCRT_CHECK_PMT(file != NULL)` to `fread`, `fwrite`, `fgets`, `fgetws`, `fputs`, `fputws`,
`fgetc`, `fgetwc`, `_getw`, `fputc`, `fputwc`, `_putw`, `_ftelli64`, `_fseeki64`, `feof`,
`ferror`, `_fileno`, `rewind` and `clearerr` in `dlls/msvcrt/file.c`, plus `test_null_stream()`
in `dlls/msvcrt/tests/file.c`. `getc`, `putc`, `ftell` and `fseek` are covered through the
functions they call. It was written against `wine-11.13` and applies cleanly to master.

```bash
git clone --depth 1 --branch wine-11.13 https://gitlab.winehq.org/wine/wine.git
cd wine && git apply ../vcv-rack-wine-fix/wine/msvcrt-null-stream.diff
mkdir ../build && cd ../build
../wine/configure --enable-win64 --with-mingw=clang   # add --without-... for missing libraries
make -j"$(nproc)" dlls/msvcrt/all dlls/msvcrt/tests/all
```

Not measured: the return values of `msvcr80.dll` to `msvcr120.dll`. They share the source file
with `msvcrt` and `ucrtbase`, so the patch applies to them as well.

## Upstream status

| Where | Status |
|---|---|
| Wine (bug report with the measurements, no patch because of Wine's LLM policy) | [Wine bug 60344](https://bugs.winehq.org/show_bug.cgi?id=60344) |
| VCV Rack (VCV does not accept code contributions) | reported to VCV support on 2026-09-17 |
| Cardinal | [comment in #854](https://github.com/DISTRHO/Cardinal/issues/854#issuecomment-5720689742) |
| ableton-linux (its Wine build could carry the fix) | [shibco/ableton-linux#317](https://github.com/shibco/ableton-linux/issues/317) |

The right place for the fix is Rack (check the `fopen()` result, or skip entries that are not
regular files) and Wine (validate the stream). The binary patch in this repository is a stopgap
until one of them ships.

## AI disclosure

The analysis, the binary patch script, the test programs and the Wine diff in this repository
were written with an AI coding assistant (Claude Code). The numbers in `results/` come from real
runs on Windows 11 and Wine 11.13, and the scripts in `tests/` reproduce them.

Wine does not accept LLM-generated code
([Developer FAQ](https://gitlab.winehq.org/wine/wine/-/wikis/Developer-FAQ)). The diff in `wine/`
is therefore not submitted to Wine and must not be copied into it. The Wine bug report contains
the description and the measured Windows behaviour only.

## Notes

- The analysis rests on Rack's public source code and on Rack Free, which is GPLv3. Rack Pro
  contains the same code and is handled by the same byte pattern. This repository contains no
  files from VCV Rack.
- Patching a binary is at your own risk. Keep the `.orig` backup until you have saved and
  reloaded a patch successfully.

## License

The script, the tests and the documentation are under the [MIT license](LICENSE). The diff in
`wine/` is a change to Wine and is under Wine's license, LGPL 2.1 or later.
