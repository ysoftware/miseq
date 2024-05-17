.SILENT: build

warnings := -Wall -Wextra

ifeq ($(OS), Windows_NT)
	raylib := -Ilib/raylib-5.0_win/include -Llib/raylib-5.0_win/lib -lraylib
	compiler := gcc -std=c99
	frameworks := -lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm
else ifeq ($(shell uname), Linux)
	raylib := -lraylib
	compiler := clang
	frameworks := -lGL -lm -lpthread -ldl -lrt -lX11 
else
	raylib := -Ilib/raylib-5.0_macos/include lib/raylib-5.0_macos/lib/libraylib.a
	compiler := clang
	frameworks := -framework CoreAudio \
				  -framework AudioToolbox -framework AudioUnit \
				  -framework CoreServices -framework Carbon \
				  -framework CoreVideo -framework IOKit \
				  -framework Cocoa -framework GLUT \
				  -framework OpenGL 
endif

all: dir plug app

dir:
	mkdir -p build

midi:
	$(compiler) $(warnings) -fPIC -c src/midi.c -o build/midi.o

wav:
	$(compiler) $(warnings) -fPIC -c src/wav.c -o build/wav.o

ui:
	$(compiler) $(warnings) -fPIC -c src/ui.c -o build/ui.o 

plug: midi wav ui
	$(compiler) $(warnings) -fPIC -c src/main.c -o build/plug.o
	$(compiler) $(warnings) -shared -o build/libplug.so build/plug.o build/ui.o build/wav.o build/midi.o

app:
	$(compiler) $(warnings) -o main.app src/main.c
