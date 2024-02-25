.SILENT: build

ifeq ($(shell uname),Darwin) # macOS
build:
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
		main.c -o miseq.app; \
	echo "Build complete."

else ifeq ($(OS),Windows_NT) # Windows
build:
	gcc main.c \
		-o miseq.exe \
		-Iraylib-5.0_win/include \
		-Lraylib-5.0_win/lib -lraylib \
		-Iportaudio-19.7.0/include \
		-Lportaudio-19.7.0/Release -lportaudio_x64 \
		-lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm; \
	echo Build complete. 

else
build:
	@echo "Unsupported operating system:" $(OS)
	@exit 1
endif

