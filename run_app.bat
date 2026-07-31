@echo off
setlocal

set "PATH=D:\vbcoding\QT6\6.8.3\msvc2022_64\bin;%PATH%"
set "APP=%~dp0build\release\EmotionSprite.exe"

if not exist "%APP%" (
    echo Release application not found. Run build_release.bat first.
    pause
    exit /b 1
)

start "EmotionSprite" "%APP%"

