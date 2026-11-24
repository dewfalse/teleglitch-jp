@echo off
setlocal
where cl.exe >nul 2>nul || (echo Open the x86 Native Tools Command Prompt for VS 2022. & exit /b 1)
cd /d "%~dp0..\.."
if not exist Teleglitch.exe (echo Copy the MOD source folder into a test copy of the game. & exit /b 1)
if not exist teleglitch-jp\build mkdir teleglitch-jp\build
cl /nologo /W4 /O2 /MT /EHsc teleglitch-jp\tests\lua_runner.cpp /Fo:teleglitch-jp\build\lua_runner.obj /Fe:teleglitch-jp\build\lua_runner.exe
if errorlevel 1 exit /b 1
teleglitch-jp\build\lua_runner.exe teleglitch-jp\tests\tutorial_test.lua
if errorlevel 1 exit /b 1
teleglitch-jp\build\lua_runner.exe teleglitch-jp\tests\narrative_test.lua
if errorlevel 1 exit /b 1
teleglitch-jp\build\lua_runner.exe teleglitch-jp\tests\inventory_test.lua
if errorlevel 1 exit /b 1
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 teleglitch-jp\tests\renderer_test.cpp /Fo:teleglitch-jp\build\renderer_test.obj /Fe:teleglitch-jp\build\renderer_test.exe /link opengl32.lib gdi32.lib user32.lib
if errorlevel 1 exit /b 1
teleglitch-jp\build\renderer_test.exe
exit /b %errorlevel%
