#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "ui.h"

const float theta = 0.000005f;
int console_lines_this_frame = 0;
int interacting_button_id = 0;

void process_scroll_interaction(struct ScrollZoom *state) {
    double smoothing_factor = GetFrameTime() * 10;

    // scroll controls
    if (!IsKeyDown(KEY_LEFT_CONTROL)) {
        // 25% of scroll view (per full wheel scroll?)
        float scroll_power = 0.1f * state->scroll_speed;
        if (IsKeyDown(KEY_LEFT_SHIFT))  scroll_power *= 5;
        state->target_scroll += GetMouseWheelMove() * scroll_power;
    }

    // smooth scroll
    state->scroll -= (state->scroll - state->target_scroll) * smoothing_factor;

    // scroll under/overflow
    if (state->target_scroll < 0.0f)  state->target_scroll = 0.0f;
    else if (state->target_scroll > 1.0f)  state->target_scroll = 1.0f;

    // scroll theta
    float diff_scroll = state->scroll - state->target_scroll;
    if (diff_scroll < theta && diff_scroll > -theta) {
        state->scroll = state->target_scroll;
    }

    // zoom x controls
    if (IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_LEFT_SHIFT)) {
        state->target_zoom_x += GetMouseWheelMove() * 0.05;
    }

    // smooth zoom x
    float change_x = (state->zoom_x - state->target_zoom_x) * smoothing_factor;
    state->zoom_x -= change_x;

    // zoom x under/overflow
    if (state->target_zoom_x < 0.01f)  state->target_zoom_x = 0.01f;
    else if (state->target_zoom_x > 0.99f)  state->target_zoom_x = 0.99f;
    if (state->zoom_x < 0.01f)  state->zoom_x = 0.01f;
    else if (state->zoom_x > 0.99f)  state->zoom_x = 0.99f;

    // zoom x theta
    float diff_zoom_x = state->zoom_x - state->target_zoom_x;
    if (diff_zoom_x < theta && diff_zoom_x > -theta) {
        state->zoom_x = state->target_zoom_x;
    }

    // zoom y controls
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT)) {
        state->target_zoom_y += GetMouseWheelMove() * 0.05;
    }

    // smooth zoom y
    float change_y = (state->zoom_y - state->target_zoom_y) * smoothing_factor;
    state->zoom_y -= change_y;

    // zoom y under/overflow
    if (state->target_zoom_y < 0.01f)  state->target_zoom_y = 0.01f;
    else if (state->target_zoom_y > 0.99f)  state->target_zoom_y = 0.99f;
    if (state->zoom_y < 0.01f)  state->zoom_y = 0.01f;
    else if (state->zoom_y > 0.99f)  state->zoom_y = 0.99f;

    // zoom y theta
    float diff_zoom_y = state->zoom_y - state->target_zoom_y;
    if (diff_zoom_y < theta && diff_zoom_y > -theta) {
        state->zoom_y = state->target_zoom_y;
    }
}

bool DrawButtonRectangle(char* title, int id, Rectangle frame) {
    assert(id != 0);

    bool did_click_button = false;
    bool is_mouse_over = CheckCollisionPointRec(GetMousePosition(), frame);
    bool is_interacting = interacting_button_id == id;

#if 1
    did_click_button = is_mouse_over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
#else
    // set interacting 
    if (interacting_button_id == 0 && is_mouse_over && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        is_interacting = true;
        interacting_button_id = id;
    }
    if (is_interacting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        interacting_button_id = 0;
        if (is_interacting && is_mouse_over) {
            did_click_button = true;
        }
    }
#endif

    Color background_color;
    Color title_color;
    if (is_mouse_over && is_interacting) { // pressing down
        background_color = CLITERAL(Color){ 130, 130, 130, 255 };
        title_color = CLITERAL(Color){ 10, 10, 10, 255 };
    } else if (is_mouse_over && interacting_button_id == 0) { // hover over
        background_color = CLITERAL(Color){ 180, 180, 180, 255 };
        title_color = CLITERAL(Color){ 50, 50, 50, 255 };
    } else if (is_interacting) { // interacting but pointer is outside
        background_color = CLITERAL(Color){ 200, 200, 200, 255 };
        title_color = CLITERAL(Color){ 150, 150, 150, 255 };
    } else { // idle
        background_color = CLITERAL(Color){ 200, 200, 200, 255 };
        title_color = CLITERAL(Color){ 50, 50, 50, 255 };
    }

    DrawRectangleRounded(frame, 1.0, 10, background_color);

    int title_font_size = 20;
    int text_width = MeasureText(title, title_font_size);
    DrawText(
        title,
        frame.x + frame.width / 2 - text_width / 2,
        frame.y + frame.height / 2 - title_font_size / 2,
        title_font_size,
        title_color 
    );

    return did_click_button;
}

bool DrawButton(char* title, int id, int x, int y, int width, int height) {
    return DrawButtonRectangle(title, id, ((Rectangle) { x, y, width, height }));
}

void DrawConsoleLine(char* string) {
    int console_width = 600;
    int line_height = 25;
    int console_top_offset = 10;

    if (console_lines_this_frame == 0) {
        DrawRectangle(
            0,
            0,
            console_width,
            console_top_offset,
            BLACK 
        );
    }
    
    DrawRectangle(
        0,
        console_lines_this_frame * line_height + console_top_offset,
        console_width,
        line_height,
        BLACK 
    );

    DrawText(string, 10, console_lines_this_frame * line_height + console_top_offset, 25, WHITE);
    console_lines_this_frame += 1;
}

void ClearConsole() {
    console_lines_this_frame = 0;
}
