# Wine patch (tested here, not submitted)

`msvcrt-null-stream.diff` changes `dlls/msvcrt/file.c` and `dlls/msvcrt/tests/file.c` so that the
stdio functions validate a `NULL` stream the way native `msvcrt.dll` does.

**It was generated with an AI coding assistant. The Wine project does not accept LLM-generated
code** ([Developer FAQ](https://gitlab.winehq.org/wine/wine/-/wikis/Developer-FAQ),
[Clean Room Guidelines](https://gitlab.winehq.org/wine/wine/-/wikis/Clean-Room-Guidelines)), so
this diff is not submitted to Wine and must not be copied into Wine. It is here to document what
was built and tested, and for Wine forks that set their own rules. The Wine bug report describes
the problem and the measured Windows behaviour, so a Wine developer can write the fix
independently. The change itself is small: the same `MSVCRT_CHECK_PMT(file != NULL)` check that
`fclose()` already has.

The diff is derived from Wine's source and is under Wine's license, the GNU Lesser General
Public License, version 2.1 or later. The MIT license in the repository root does not apply to it.

Base: `wine-11.13`, applies cleanly to master. Apply with `git apply`. Build steps and test
results are in the main README.
