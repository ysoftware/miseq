#!/bin/bash
set -e

clang \
    -Iraylib-5.0_win/include \
    -framework CoreVideo \
    -framework IOKit \
    -framework Cocoa \
    -framework GLUT \
    -framework OpenGL \
    raylib-5.0_macos/lib/libraylib.a \
    main.c -o miseq.app

echo "Build complete."
