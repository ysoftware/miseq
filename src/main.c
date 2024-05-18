#include <stdio.h>
#include <dlfcn.h>
#include "raylib.h"

const int FPS = 120;

void *plugin_handle;
void (*plug_init)();
void (*plug_update)();
void* (*plug_pre_reload)();
void (*plug_post_reload)(void*);

bool load_library() {
    if (plugin_handle) dlclose(plugin_handle);
    plugin_handle = dlopen("./build/libplug.so", RTLD_LAZY);

    if (!plugin_handle) {
        fprintf(stderr, "Error: %s\n", dlerror());
        return false;
    }

    plug_init = dlsym(plugin_handle, "plug_init");
    if (!plug_init) goto defer;

    plug_update = dlsym(plugin_handle, "plug_update");
    if (!plug_update) goto defer;

    plug_pre_reload = dlsym(plugin_handle, "plug_pre_reload");
    if (!plug_pre_reload) goto defer;

    plug_post_reload = dlsym(plugin_handle, "plug_post_reload");
    if (!plug_post_reload) goto defer;

    return 0;

defer:
    fprintf(stderr, "Error: %s\n", dlerror());
    dlclose(plugin_handle);
    return false;
}

int main() {
    InitAudioDevice();
    InitWindow(2500, 1000, "miseq");
    SetTargetFPS(FPS);

    int code = load_library();
    if (code != 0) return code;
    plug_init();

    while(!WindowShouldClose()) {
        if (IsKeyPressed(KEY_BACKSLASH)) {
            void *state = plug_pre_reload();
            load_library();
            plug_post_reload(state);
            printf("Hotreloading successful\n");
        }
        plug_update();
    }

    dlclose(plugin_handle);
    CloseWindow();
}
