#include "raylib.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

const int FPS = 300;
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

struct Note {
    uint8_t key;
    uint8_t velocity;
    uint32_t start_tick;
    uint32_t end_tick;
};

struct Note notes[10000];
uint32_t notes_count = 0;

// for midi rendering
void* data;
uint64_t size = 0;

uint32_t le_to_be(uint32_t num) {
    uint8_t b[4] = {0};
    *(uint32_t*)b = num;
    uint8_t tmp = 0;
    tmp = b[0];
    b[0] = b[3];
    b[3] = tmp;
    tmp = b[1];
    b[1] = b[2];
    b[2] = tmp;
    return *(uint32_t*)b;
}

void print_hex(const void* data, uint64_t size) {
    const uint8_t* bytes = (const uint8_t*)data;

    for (uint64_t i = 0; i < size; ++i) {
        printf("%02X ", bytes[i]);

        if (i % 10 == 9 && i + 1 < size) {
            printf("\n");
        }
    }
    printf("\n");
}

void append_string(const char* bytes) {
    char* pointer = (char*)data + size;
    uint64_t len = strlen(bytes);
    memcpy(pointer, bytes, len);
    size += len;
}

void append_byte(int8_t byte) {
    int8_t* pointer = (int8_t*)data + size;
    memcpy(pointer, &byte, 1);
    size += 1;
}

void append_note_on(uint8_t key, uint8_t velocity) {
    uint64_t start = size;
    
    append_byte(0x90); // channel 0
    append_byte(key);
    append_byte(velocity);
}

void append_note_off(uint8_t key, uint8_t velocity) {
    uint64_t start = size;
    
    append_byte(0x80); // channel 0
    append_byte(key);
    append_byte(velocity);
}

void append_delta_time(uint32_t ticks) {
    uint64_t start = size;

    if (ticks <= 127) {
        append_byte((uint8_t) ticks);
    } else { // VLQ format
        uint32_t ticks_left = ticks;
        uint8_t buffer[5]; // maximum 5 bytes for a 32-bit number
        int count = 0;

        while (ticks_left > 0) {
            buffer[count++] = (uint8_t)(ticks_left & 0x7F);
            ticks_left >>= 7;
        }

        // reverse the order of bytes and set the MSB to 1 for all but the last byte
        for (int i = count - 1; i >= 0; --i) {
            uint8_t byte = buffer[i];
            if (i != 0) {
                byte |= 0x80; // set MSB to 1 for continuation
            }
            append_byte(byte);
        }
    }
}

void append_header() {
    append_string("MThd");

    // chunk length (4 bytes): header is always 6
    append_byte(0);
    append_byte(0);
    append_byte(0);
    append_byte(6);

    // format type (2 bytes): 0 single, 1 multi synced, 2 multi independent
    append_byte(0);
    append_byte(0);

    // number of tracks (2 bytes)
    append_byte(0);
    append_byte(1);

    // timing resolution (2 bytes)
    append_byte(0x00);
    append_byte(0x60);
}

void append_track_chunk() {
    append_string("MTrk");

    // reserve chunk length (4 bytes) to set later
    uint64_t length_position = size;
    append_byte(0);
    append_byte(0);
    append_byte(0);
    append_byte(0);

    uint64_t chunk_start = size;

    // track events
    // remap them from notes

    // end of track meta event
    append_byte(0xff);
    append_byte(0x2f);
    append_byte(0);

    // finally set chunk length
    uint8_t* pointer = (uint8_t*)data + length_position;
    uint32_t length_value = (uint32_t) (size - chunk_start);
    uint32_t value = le_to_be(length_value);
    memcpy((uint64_t*)pointer, &value, 4);
}

void write_data(const char* filename, void* data, uint64_t size) {
    FILE* file = fopen("1.mid", "wb");
    
    if (file == NULL) {
        printf("Error opening file %s.\n", filename);
        return;
    }

    fwrite(data, 1, size, file);
    fclose(file);
    printf("Written %d bytes to %s.\n", size, filename);
}

void create_notes() {
    notes_count = 0;

    uint32_t current_tick = 0;
    uint32_t note_duration = 60;

    double base_velocity = 80;
    double base_key = 60;
    double base_note_duration = 15;

    for (double i = 0; i < 10000; i++) {
        double wave1 = cos(i / 4) * 19;
        double wave2 = sin(i / 15) * 50;
        double wave3 = sin(i / 3) * 7;

        uint8_t key = (uint8_t) (base_key - wave1 - wave3);
        uint8_t velocity = (uint8_t) (base_velocity - wave2);
        uint8_t note_duration = (uint8_t) (base_note_duration - wave3);
    
        struct Note note = {
            key,
            velocity,
            current_tick,
            current_tick + note_duration
        };

        notes[notes_count] = note;
        notes_count += 1;

        current_tick += note_duration;
    }
}

void DrawNotes() {
    const float default_key_height = 8;
    const float default_scroll_offset = 0;
    const float default_tick_width = 1;

    static float scroll_offset = default_scroll_offset;
    static float key_height = default_key_height;
    static float tick_width = default_tick_width;

    double smoothing_factor = GetFrameTime() * 10;
    float scroll_content_width = tick_width * notes[notes_count-1].end_tick; // TODO: only considering notes are sorted
    float adjust_scroll_offset_after_scaling = 0;

    // SCALE WIDTH
    {
        static float target = default_tick_width;
        const float minValue = 0.1;
        const float maxValue = 8;

        if (IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_LEFT_SHIFT)) {
            target += GetMouseWheelMove() * 0.05;

            if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
                target = default_tick_width;
            }
        }

        float change = (tick_width - target) * smoothing_factor;
        tick_width -= change;

        if (change != 0) {
            float preceding_ticks = (scroll_offset + SCREEN_WIDTH / 2) / tick_width;
            adjust_scroll_offset_after_scaling -= preceding_ticks * change;
        }

        if (target < minValue)  target = minValue;
        else if (target > maxValue)  target = maxValue;

        float diff = tick_width - target;
        if (diff < 0.0005 && diff > -0.0005) {
            tick_width = target;
        }
    }

    // SCALE HEIGHT
    {
        static float target = default_key_height;
        const float minValue = 4;
        const float maxValue = 8;

        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT)) {
            target += GetMouseWheelMove() * 1;

            if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
                target = default_key_height;
            }
        }

        key_height -= (key_height - target) * smoothing_factor;

        if (target < minValue)  target = minValue;
        else if (target > maxValue)  target = maxValue;

        float diff = key_height - target;
        if (diff < 0.0005 && diff > -0.0005) {
            key_height = target;
        }    
    }

    // SCROLL
    {
        static float target = default_scroll_offset;
        const float minValue = 0;
        const float maxValue = scroll_content_width - SCREEN_WIDTH;

        if (adjust_scroll_offset_after_scaling != 0) {
            target += adjust_scroll_offset_after_scaling;
            scroll_offset += adjust_scroll_offset_after_scaling;
        }

        if (!IsKeyDown(KEY_LEFT_CONTROL)) {
            int scroll_power = SCREEN_WIDTH / 4;
            if (IsKeyDown(KEY_LEFT_SHIFT))  scroll_power *= 5;
            target += GetMouseWheelMove() * scroll_power;

            if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
                target = 0;
            }
        }

        scroll_offset -= (scroll_offset - target) * smoothing_factor;

        if (scroll_content_width > SCREEN_WIDTH) {
            if (target < minValue)  target = minValue;
            else if (target > maxValue)  target = maxValue;
        } else {
            target = 0;
        }

        float diff = scroll_offset - target;
        if (diff < 0.0005 && diff > -0.0005) {
            scroll_offset = target;
        }
    }

    DrawRectangle(
        -scroll_offset, 
        0,
        scroll_content_width,
        128 * key_height, 
        GRAY
    );

    int draw_count = 0;
    for (int i = 0; i < notes_count; i++) {
        struct Note note = notes[i];

        int posX = note.start_tick * tick_width - (int) scroll_offset;
        int posY = note.key * key_height;
        int width = (note.end_tick - note.start_tick) * tick_width;
        int height = key_height;

        if (posX < SCREEN_WIDTH && posX + width > 0) {
            DrawRectangle(posX, posY, width, height, BLACK);
            draw_count += 1;
        }
    }

    char text[100];
    sprintf(text, "Scroll offset: %f\nKey height: %fTick width: %f\nDraw count: %d", scroll_offset, key_height, tick_width, draw_count);
    DrawText(text, 10, 10, 20, LIGHTGRAY);
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "miseq");
    SetTargetFPS(FPS);

    create_notes();
    
    while(!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawNotes();
        EndDrawing();
    }
    CloseWindow();
}

