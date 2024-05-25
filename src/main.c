#include <stdio.h>
#include <dlfcn.h>
#include "raylib.h"

void *plugin_handle;
void (*plug_init)();
void (*plug_update)();
void (*plug_cleanup)();
void* (*plug_pre_reload)();
void (*plug_post_reload)(void*);

bool load_library() {
    if (plugin_handle) dlclose(plugin_handle);
    plugin_handle = dlopen("./build/libplug.so", RTLD_LAZY);
    if (!plugin_handle) goto defer;

    plug_init = dlsym(plugin_handle, "plug_init");
    if (!plug_init) goto defer;

    plug_update = dlsym(plugin_handle, "plug_update");
    if (!plug_update) goto defer;

    plug_pre_reload = dlsym(plugin_handle, "plug_pre_reload");
    if (!plug_pre_reload) goto defer;

    plug_post_reload = dlsym(plugin_handle, "plug_post_reload");
    if (!plug_post_reload) goto defer;

    plug_cleanup = dlsym(plugin_handle, "plug_cleanup");
    if (!plug_cleanup) goto defer;

    return true; 

defer:
    printf("Error: %s\n", dlerror());
    if (plugin_handle) dlclose(plugin_handle);
    return false;
}

int main() {
    if (!load_library()) return 1;

    InitAudioDevice();
    InitWindow(2500, 1000, "miseq");
    SetTargetFPS(120);

    plug_init();

    while(!WindowShouldClose()) {
        if (IsKeyPressed(KEY_BACKSLASH)) {
            void *state = plug_pre_reload();
            if (!load_library()) return 1;
            plug_post_reload(state);
            printf("Hotreloading successful\n");
        }
        plug_update();
    }

    plug_cleanup();
    dlclose(plugin_handle);
    CloseWindow();
    return 0;
}
