@echo off
rem Runs crtnull.exe for every function in funcs.txt on real Windows and writes result-windows.txt
rem next to this file. The EXE is copied to %TEMP% first, the log is written there and copied back.
setlocal enabledelayedexpansion
set SRC=%~dp0
set LOG=%TEMP%\crtnull-result.txt
copy /y "%SRC%crtnull.exe" "%TEMP%\crtnull.exe" >nul
copy /y "%SRC%funcs.txt" "%TEMP%\crtnull-funcs.txt" >nul
ver > "%LOG%"
for %%M in ("" h) do for %%D in (msvcrt.dll ucrtbase.dll) do (
  for /f "usebackq delims=" %%L in ("%TEMP%\crtnull-funcs.txt") do for %%F in (%%L) do (
    "%TEMP%\crtnull.exe" "|%%D|%%F|%%~M" >> "%LOG%" 2>&1
    echo    exit=!ERRORLEVEL! >> "%LOG%"
  )
)
copy /y "%LOG%" "%SRC%result-windows.txt" >nul
