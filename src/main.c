#include <stdio.h>
#include <dlfcn.h>
#include "raylib.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

void *plugin_handle;
void (*plug_init)(void);
void (*plug_update)(void);
void (*plug_cleanup)(void);
void* (*plug_pre_reload)(void);
void (*plug_post_reload)(void*);

char *library_path = "./build/libplug.so";
char *library_lockfile_path = "./build/libplug.lock";
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
    printf("main.c: load_library: Error: %s\n", dlerror());
    if (plugin_handle) dlclose(plugin_handle);
    return false;
}

bool is_library_file_modified(void) {
    struct stat attributes;
    stat(library_path, &attributes);
    time_t modified_time = attributes.st_mtime;

    // check if existing file is newer than last loaded version
    if (modified_time <= last_library_load_time)  return false;

    // check if lock file exists
    if (access(library_lockfile_path, F_OK) == 0)  return false;

    return true;
}

int main(void) {
    if (!load_library()) return 1;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_MSAA_4X_HINT);

    InitWindow(1200, 800, "miseq");
    SetTargetFPS(120);

    plug_init();

    while(!WindowShouldClose()) {
        if (is_library_file_modified()) {
            void *state = plug_pre_reload();
            if (!load_library())  break;
            plug_post_reload(state);
            printf("Hotreloading successful\n");
        }

        if (IsKeyPressed(KEY_BACKSLASH)) {
            void *state = plug_pre_reload();
            if (!load_library())  break;
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
