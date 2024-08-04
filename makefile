warnings := -Wall -Wextra -g
miniaudio = -Ilib

ifeq ($(shell uname), Linux)
	raylib := -Ilib/raylib-5.0_linux_i386/include
	compiler := clang
	frameworks := -lGL -lm -lpthread -ldl -lrt -lX11 -lraylib
else # macos
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
	$(compiler) $(warnings) $(raylib) -fPIC -c src/wav.c -o build/wav.o

build/ui.o: src/ui.c
	$(compiler) $(warnings) $(raylib) -fPIC -c src/ui.c -o build/ui.o 

build/libplug.so: build build/midi.o build/wav.o build/ui.o src/plug.c
	touch build/libplug.lock
	$(compiler) $(warnings) $(miniaudio) $(raylib) -fPIC -c src/plug.c -o build/plug.o
	$(compiler) $(warnings) $(raylib) $(frameworks) -shared -o build/libplug.so build/plug.o build/ui.o build/wav.o build/midi.o
	rm build/libplug.lock

miseq.app: src/main.c
	$(compiler) $(miniaudio) $(warnings) $(raylib) $(frameworks) -o miseq.app src/main.c
