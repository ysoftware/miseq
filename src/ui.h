#include "raylib.h"

#define Console(Format, Arguments...) { \
    char text[50]; \
    sprintf(text, Format, Arguments); \
    DrawConsoleLine((char*) text); \
}

// scroll value: 0.0 -> 1.0, only one axis, can bounce by going negative or above 1.0
// zoom values: 0.01 -> 0.5 -> 0.99, 0.5 is the default
struct ScrollZoom {
    float scroll_speed;
    float scroll;
    float target_scroll;
    float zoom_x;
    float target_zoom_x;
    float zoom_y;
    float target_zoom_y;
};

void process_scroll_interaction(struct ScrollZoom *state);
bool DrawButtonRectangle(char* title, int id, Rectangle frame);
bool DrawButton(char* title, int id, int x, int y, int width, int height);
void DrawConsoleLine(char* string);
void ClearConsole();
