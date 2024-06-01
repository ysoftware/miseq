#include "raylib.h"

#define Console(Format, Arguments...) { \
    char text[50]; \
    sprintf(text, Format, Arguments); \
    DrawConsoleLine((char*) text); \
}

// scroll value: 0.0 -> 1.0, only one axis, can bounce by going negative or above 1.0
// zoom values: 0.01 -> 0.5 -> 0.99, 0.5 is the default
typedef struct {
    float scroll_speed;
    float scroll;
    float target_scroll;
    float zoom_x;
    float target_zoom_x;
    float zoom_y;
    float target_zoom_y;
} ScrollZoom;

// processes scroll with mouse wheel and default modifiers (shift to go faster, ctrl to zoom)
void process_scroll_interaction(ScrollZoom *state, float content_size, float view_width, float *scroll_offset);

// calculates visible content rect for horizontal scroll
Rectangle calculate_content_rect(float view_x, float view_width, float view_y, float view_height, float content_size, float content_offset);

bool DrawButtonRectangle(char* title, int id, Rectangle frame);
bool DrawButton(char* title, int id, int x, int y, int width, int height);
void DrawConsoleLine(char* string);
void ClearConsole(void);
