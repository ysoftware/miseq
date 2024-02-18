@echo off

gcc main.c ^
    -o miseq.exe ^
    -Iraylib-5.0_win/include ^
    -Lraylib-5.0_win/lib -lraylib ^
    -Iportaudio-19.7.0/include ^
    -Lportaudio-19.7.0/Release -lportaudio_x64 ^
    -lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm

IF %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
)

echo Build complete.
