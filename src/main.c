#include <stdio.h>
#include <dlfcn.h>
#include "raylib.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

void *plugin_handle;
void (*plug_init)(void);
void (*plug_update)(void);
void (*plug_cleanup)(void);
void* (*plug_pre_reload)(void);
void (*plug_post_reload)(void*);

char *library_path = "./build/libplug.so";
time_t last_library_load_time = 0;

bool load_library(void) {
    if (plugin_handle) dlclose(plugin_handle);

    plugin_handle = dlopen(library_path, RTLD_LAZY);
    if (!plugin_handle) goto fail;
    plug_init = dlsym(plugin_handle, "plug_init");
    if (!plug_init) goto fail;
    plug_update = dlsym(plugin_handle, "plug_update");
    if (!plug_update) goto fail;
    plug_pre_reload = dlsym(plugin_handle, "plug_pre_reload");
    if (!plug_pre_reload) goto fail;
    plug_post_reload = dlsym(plugin_handle, "plug_post_reload");
    if (!plug_post_reload) goto fail;
    plug_cleanup = dlsym(plugin_handle, "plug_cleanup");
    if (!plug_cleanup) goto fail;

    last_library_load_time = time(NULL);
    return true; 

fail:
    printf("Error: %s\n", dlerror());
    if (plugin_handle) dlclose(plugin_handle);
    return false;
}

bool load_library_if_modified(void) {
    struct stat attributes;
    stat(library_path, &attributes);
    time_t modified_time = attributes.st_mtime;

    time_t now = time(NULL);
    
    // TODO: make it not depend on time, but rather waiting until the library is ready to be loaded 
    // (finished writing to the file)
    bool file_was_modified = modified_time > last_library_load_time && modified_time + 0.5 < now;

    if (file_was_modified) {
        load_library();
        return true;
    }

    return false;
}

int main(void) {
    if (!load_library()) return 1;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitAudioDevice();
    InitWindow(1200, 800, "miseq");
    SetTargetFPS(120);

    plug_init();

    while(!WindowShouldClose()) {
        void *state = plug_pre_reload();
        if (load_library_if_modified()) {
            printf("Hotreloading successful\n");
            plug_post_reload(state);
        }

        plug_update();
    }

    plug_cleanup();
    dlclose(plugin_handle);
    CloseWindow();
    return 0;
}
