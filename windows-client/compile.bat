@echo off
set MINGW_PATH=C:\mingw64\bin
set PATH=%MINGW_PATH%;%PATH%

echo Compiling resources...
windres resource.rc -O coff -o resource.res

echo Compiling application...
g++ main.cpp resource.res -o FlappyBird.exe -mwindows -static -lgdiplus -lgdi32 -lwinmm

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! FlappyBird.exe created.
) else (
    echo Compilation failed.
)

pause
