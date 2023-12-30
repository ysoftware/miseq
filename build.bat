@echo off

gcc main.c ^
    -o app.exe ^
    -Iraylib-5.0/include -Lraylib-5.0/lib -lraylib ^
    -lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm

echo Build complete.
