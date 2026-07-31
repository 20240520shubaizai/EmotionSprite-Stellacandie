@echo off
setlocal

set "PATH=D:\vbcoding\QT6\6.8.3\msvc2022_64\bin;%PATH%"
set "APP=%~dp0build\qt683\EmotionSprite.exe"

if not exist "%APP%" (
    echo EmotionSprite.exe not found. Run build_debug.bat first.
    pause
    exit /b 1
)

start "EmotionSprite" "%APP%"

