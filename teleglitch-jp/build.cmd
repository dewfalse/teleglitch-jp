@echo off
setlocal
where cl.exe >nul 2>nul || (echo Open the x86 Native Tools Command Prompt for VS 2022. & exit /b 1)
cd /d "%~dp0" || exit /b 1
if not exist build mkdir build
pwsh -NoProfile -File "tools\build_narratives.ps1"
if errorlevel 1 exit /b 1
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /LD src\renderer.cpp /Fo:build\renderer.obj /Fe:teleglitch_jp.dll /link /DEF:src\exports.def /IMPLIB:build\teleglitch_jp.lib opengl32.lib gdi32.lib user32.lib
exit /b %errorlevel%
