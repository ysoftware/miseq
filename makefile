.SILENT: build

# Detect OS Environment
ifeq ($(OS),Windows_NT)
detected_OS := Windows
else
detected_OS := $(shell uname)
endif


# Build Command
build:
ifeq ($(detected_OS),Windows)
	gcc  -Wall -Wextra \
		main.c \
		midi.c \
		ui.c \
		-std=c99 \
		-o miseq.exe \
		-Iraylib-5.0_win/include \
		-Lraylib-5.0_win/lib -lraylib \
		-Iportaudio-19.7.0/include \
		-Lportaudio-19.7.0/Release -lportaudio_x64 \
		-lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm
else ifeq ($(detected_OS),Darwin)
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
		main.c \
		midi.c \
		ui.c \
		-o miseq.app
else
	@echo "Unsupported operating system."
	@exit 1
endif

	@echo "Build complete"
