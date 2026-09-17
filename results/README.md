# Raw results

- `windows11-26200-crtnull.txt`: `tests/crtnull.exe` on Windows 11 build 26200, default settings.
- `windows11-26200-crtnull-with-handler.txt`: the same with an empty invalid parameter handler
  installed. `msvcrt.dll` has no such handler, so its results do not change.
- `wine-11.13-crtnull.txt`: the same under Wine 11.13, both modes. The shell only sees the low
  8 bits of the exit code.
- `windows11-26200-wine-msvcrt-file-test.txt`: summary lines of Wine's `msvcrt_test.exe file`
  (built with the patch in `wine/`) on Windows 11.

`errno=22` is `EINVAL`. `errno=57005` would mean the function did not touch `errno`. `rewind` and
`clearerr` return `void`, so their `ret=` value is whatever was left in the register.

The Windows files were produced by an earlier build of `crtnull.exe` with German messages. The
message text was replaced with the current English wording and one line per call was written.
The values are unchanged.
