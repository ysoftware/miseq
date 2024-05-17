#include <stdio.h>
#include <dlfcn.h>
#include "raylib.h"

void *plugin_handle;
void (*plug_init)();
void (*plug_update)();
void (*plug_pre_reload)();
void*(*plug_post_realod)(void*);

int load_library() {
    plugin_handle = dlopen("./build/libplug.so", RTLD_LAZY);

    if (!plugin_handle) {
        fprintf(stderr, "Error: %s\n", dlerror());
        return 1;
    }

    plug_init = dlsym(plugin_handle, "plug_init");
    if (!plug_init) {
        fprintf(stderr, "Error: %s\n", dlerror());
        dlclose(plugin_handle);
        return 2;
    }

    return 0;
}

int main() {
    printf("Hello from the main!\n");

    int code = load_library();
    if (code != 0) return code;

    printf("Calling plug_init...\n");
    plug_init();

    dlclose(plugin_handle);
    return 0;
}
