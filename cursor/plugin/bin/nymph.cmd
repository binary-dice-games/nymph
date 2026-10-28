@echo off
rem MIT License (c) 2026 Binary Dice Games
rem
rem Windows entry of the Cursor plugin's launcher (spec/12_cursor_integration/
rem DESIGN.md section 4.3). Cursor runs bin\nymph through the plugin root; on
rem Windows that resolves to this file, which runs the POSIX launcher `nymph`
rem next to it with the `sh` of Git for Windows, found even when it is not on
rem PATH (Git puts only git.exe there). Falls back to an `sh` on PATH.
set "NYMPH_SH=sh"
if exist "%ProgramFiles%\Git\bin\sh.exe" set "NYMPH_SH=%ProgramFiles%\Git\bin\sh.exe"
if exist "%LocalAppData%\Programs\Git\bin\sh.exe" set "NYMPH_SH=%LocalAppData%\Programs\Git\bin\sh.exe"
"%NYMPH_SH%" "%~dp0nymph" %*
