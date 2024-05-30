warnings := -Wall -Wextra -g

ifeq ($(OS), Windows_NT)
	raylib := -Ilib/raylib-5.0_win/include -Llib/raylib-5.0_win/lib -lraylib
	compiler := gcc -std=c99
	frameworks := -lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm
else ifeq ($(shell uname), Linux)
	raylib := -lraylib
	compiler := clang
	frameworks := -lGL -lm -lpthread -ldl -lrt -lX11
else
	raylib := -Ilib/raylib-5.0_macos/include
	compiler := clang
	frameworks := -framework CoreAudio -framework OpenGL \
				  -framework AudioToolbox -framework AudioUnit \
				  -framework CoreServices -framework Carbon \
				  -framework CoreVideo -framework IOKit \
				  -framework Cocoa -framework GLUT \
				  -framework CoreFoundation -framework AppKit \
				  -lraylib -Llib/raylib-5.0_macos/lib 
endif

all: miseq.app build build/libplug.so

build:
	mkdir -p build

build/midi.o: src/midi.c
	$(compiler) $(warnings) -fPIC -c src/midi.c -o build/midi.o

build/wav.o: src/wav.c
	$(compiler) $(warnings) -fPIC -c src/wav.c -o build/wav.o

build/ui.o: src/ui.c
	$(compiler) $(warnings) $(raylib) -fPIC -c src/ui.c -o build/ui.o 

build/libplug.so: build build/midi.o build/wav.o build/ui.o src/plug.c
	$(compiler) $(warnings) $(raylib) -fPIC -c src/plug.c -o build/plug.o
	$(compiler) $(warnings) $(raylib) -shared -o build/libplug.so build/plug.o build/ui.o build/wav.o build/midi.o $(frameworks)

miseq.app: src/main.c
	$(compiler) $(warnings) $(raylib) -o miseq.app src/main.c $(frameworks)
