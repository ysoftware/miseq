#!/bin/bash
set -e
clang \
    -Iraylib-5.0_macos/include \
    raylib-5.0_macos/lib/libraylib.a \
    -Iportaudio-19.7.0/include \
    portaudio-19.7.0/lib/.libs/libportaudio.a \
    -framework CoreAudio \
    -framework AudioToolbox \
    -framework AudioUnit \
    -framework CoreServices \
    -framework Carbon \
    -framework CoreVideo \
    -framework IOKit \
    -framework Cocoa \
    -framework GLUT \
    -framework OpenGL \
    main.c -o miseq.app

echo "Build complete."
