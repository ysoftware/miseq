#include "raylib.h"
#include "portaudio.h"
#include "raymath.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "midi.h"

#define NOTES_LIMIT 10000
const int FPS = 120;
const int SCREEN_WIDTH = 2500;
const int SCREEN_HEIGHT = 1000;

// notes
struct Note notes[NOTES_LIMIT];
int notes_count = 0;
int interacting_button_id = 0;

int console_lines_this_frame = 0;

// play sound debug

#define MAX_POLYPHONY       8
typedef short               SAMPLE_t;
#define A4_FREQUENCY        700
#define G3_FREQUENCY        196
#define SAMPLE_RATE         (44100)
#define SAMPLE_ZERO         (0)
#define DOUBLE_TO_SAMPLE(x) (SAMPLE_ZERO + (SAMPLE_t)(32767 * (x)))
#define FORMAT_NAME         "Signed 16 Bit"

float *debug_buffer = NULL;

// utils

double random_value() {
    double value = (double)GetRandomValue(0, 100000) / 100000;
    return value;
}

void create_notes() {
    srand(time(NULL));
    notes_count = 0;

    uint32_t current_tick = 0;

    double base_velocity = 80;
    double base_key = 64;
    double base_note_duration = 15;

    double wave1_random = 20 * random_value();
    double wave2_random = 15 * random_value();
    double wave3_random = 5 * random_value();

    for (double i = 0; i < 512; i++) {
        double wave1 = cos(i / wave1_random) * 64 * random_value();
        double wave2 = sin(i / wave2_random) * 40 * random_value();
        double wave3 = sin(i / wave3_random) * 9 * random_value();

        uint8_t key = (uint8_t) (base_key - wave1);
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

// USER INTERFACE

bool DrawButtonRectangle(char* title, int id, Rectangle frame) {
    assert(id != 0);

    bool is_interacting = interacting_button_id == id;
    bool is_mouse_over = CheckCollisionPointRec(GetMousePosition(), frame);

    if (interacting_button_id == 0 && is_mouse_over && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        is_interacting = true;
        interacting_button_id = id;
    }

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

    if (is_interacting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        interacting_button_id = 0;
        if (is_interacting && is_mouse_over) {
            return true;
        }
    }
    return false;
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

void DrawNotes(int view_x, int view_y, int view_width, int view_height) {
    const float default_key_height = 6;
    const float default_scroll_offset = 0;
    const float default_tick_width = 0.5;

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
            float preceding_ticks = (scroll_offset + view_width / 2) / tick_width;
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
        const float maxValue = 12;

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
        const float maxValue = scroll_content_width - view_width;

        if (adjust_scroll_offset_after_scaling != 0) {
            target += adjust_scroll_offset_after_scaling;
            scroll_offset += adjust_scroll_offset_after_scaling;
        }

        if (!IsKeyDown(KEY_LEFT_CONTROL)) {
            int scroll_power = view_width / 4;
            if (IsKeyDown(KEY_LEFT_SHIFT))  scroll_power *= 5;
            target += GetMouseWheelMove() * scroll_power;

            if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
                target = 0;
            }
        }

        scroll_offset -= (scroll_offset - target) * smoothing_factor;

        if (scroll_content_width > view_width) {
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
        view_y,
        scroll_content_width,
        128 * key_height, 
        GRAY
    );

    if (notes_count == 0) return;
    int draw_count = 0;
    for (int i = 0; i < notes_count; i++) {
        struct Note note = notes[i];

        int posX = note.start_tick * tick_width - (int) scroll_offset;
        int posY = 128 * key_height - note.key * key_height;
        int width = (note.end_tick - note.start_tick) * tick_width;
        int height = key_height;

        if (posX < view_width && posX + width > 0 && posY < view_height && posY + height > 0) {
            DrawRectangle(view_x + posX, view_y + posY, width, height, BLACK);
            draw_count += 1;
        }
    }

    // NOTE SELECTION
    static bool is_selecting = false;
    static Vector2 selection_start;
    static Vector2 selection_end;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        // TODO: check if mouse in view bounds
        if (!is_selecting) {
            is_selecting = true;
            selection_start = GetMousePosition();
        }
        selection_end = GetMousePosition();
    } else {
        is_selecting = false;
    }

    if (is_selecting) {
        char text[50];
        sprintf(text, "Start: %f, %f", selection_start.x, selection_start.y);
        DrawConsoleLine((char*) text);

        char text2[50];
        sprintf(text2, "End: %f, %f", selection_end.x, selection_end.y);
        DrawConsoleLine((char*) text2);

        DrawConsoleLine("Selecting");

        DrawRectangleV(selection_start, Vector2Subtract(selection_end, selection_start), BLUE);
    }
}

bool DrawButton(char* title, int id, int x, int y, int width, int height) {
    return DrawButtonRectangle(title, id, ((Rectangle) { x, y, width, height }));
}

// AUDIO

static float midiNoteToFrequency(uint8_t note) {
    return powf(2.0f, (note - 69) / 12.0f) * 440.0f;
}

typedef struct {
    struct Note* notes;
    int notes_count;
    uint32_t current_tick;

    struct {
        bool active;
        bool is_releasing;
        float phase_accumulator;
        float frequency;
        float attack;
    } active_notes[MAX_POLYPHONY];
} portaudioUserData;

// TODO: prerender the wave output and then return it to portaudio or save it to file
// TODO: use miniaudio to play .wav, do it non-ui-blocking 
// TODO: link audio lib statically
// TODO: export .wav in wav.h

static int portaudioCallback(
    const void *inputBuffer,
    void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData
) {
    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    portaudioUserData *data = (portaudioUserData*)userData;
    float *out = (float*)outputBuffer;
    int frames_per_tick = 500;

    for (unsigned int frame = 0; frame < framesPerBuffer; frame++) {
        if (frame % frames_per_tick == 0) {
            data->current_tick++;

            // Check for note starts and stops
            for (int i = 0; i < data->notes_count; i++) {
                struct Note note = data->notes[i];

                if (data->current_tick == note.start_tick) {
                    for (int j = 0; j < MAX_POLYPHONY; j++) {
                        if (!data->active_notes[j].active) {
                            data->active_notes[j].active = true;
                            data->active_notes[j].is_releasing = false;
                            data->active_notes[j].frequency = midiNoteToFrequency(note.key);
                            data->active_notes[j].attack = 0.0f;
                            break;
                        }
                    }
                } else if (data->current_tick == note.end_tick) {
                    for (int j = 0; j < MAX_POLYPHONY; j++) {
                        if (data->active_notes[j].active && data->active_notes[j].frequency == midiNoteToFrequency(note.key)) {
                            data->active_notes[j].is_releasing = true;
                            break;
                        }
                    }
                }
            }
        }

        float mixedSample = 0.0f;
        // Mix active notes
        for (int i = 0; i < MAX_POLYPHONY; i++) {
            if (data->active_notes[i].active) {
                float phase_increment = 2 * PI * data->active_notes[i].frequency / SAMPLE_RATE;
                data->active_notes[i].phase_accumulator += phase_increment;

                // TODO: not sure what this is
                while (data->active_notes[i].phase_accumulator > 2 * PI) {
                    data->active_notes[i].phase_accumulator -= 2 * PI;
                }

                float wave_value = sinf(data->active_notes[i].phase_accumulator);

                if (data->active_notes[i].is_releasing) {
                    data->active_notes[i].attack -= 0.001f; // release speed

                    if (data->active_notes[i].attack <= 0.0f) {
                        data->active_notes[i].active = false;
                    }
                } else {
                    data->active_notes[i].attack += 0.001f; // attack speed
                    if (data->active_notes[i].attack > 1.0f) {
                        data->active_notes[i].attack = 1.0f;
                    }
                }

                wave_value *= data->active_notes[i].attack;
                mixedSample += wave_value / MAX_POLYPHONY;
            }
        }
        
        *out++ = mixedSample; // left
        *out++ = mixedSample; // right
    }

    return paContinue;
}

int prepare_sound() {
    portaudioUserData data = {
        .notes = notes,
        .notes_count = notes_count,
        .current_tick = 0,
        .active_notes = { }
    };

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("[ERROR] Could not initialize audio: %s\n", Pa_GetErrorText(err));
        return err;
    }

    PaStream *stream;
    err = Pa_OpenDefaultStream(
        &stream,
        0, // input channels
        2, // output channels
        paFloat32, // sample format
        SAMPLE_RATE,
        1024, // frames per buffer
        portaudioCallback,
        &data
    );
    if (err != paNoError) {
        printf("[ERROR] Could not open audio stream: %s\n", Pa_GetErrorText(err));
        return err; 
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("[ERROR] Could not start audio stream: %s\n", Pa_GetErrorText(err));
        return err; 
    }

    Pa_Sleep(1024*15);

    err = Pa_StopStream(stream);
    if (err != paNoError) {
        printf("[ERROR] Could not start audio stream: %s\n", Pa_GetErrorText(err));
        return err; 
    }

    err = Pa_Terminate();
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return err;
    }

    printf("Playing sound...\n");

    return 0;
}

void DrawDebugBuffer(float view_x, float view_y, float view_width, float view_height) {
    DrawRectangle(
        0, 
        view_y,
        SCREEN_WIDTH,
        view_height, 
        GRAY
    );

    for (int i = 0; i < 1000 * 512; i++) {
        float value = debug_buffer[i * 6];

        float posX = i * 0.2;
        float posY = view_height - (view_height/2 - (-1.0f * value * view_height/2));
        Vector2 position = { view_x + posX, view_y + posY };
        Vector2 size = { 1, view_height / 50 };

        if (posX < view_width && posX > 0 && posY < view_height && posY > 0) {
            DrawRectangleV(position, size, BLACK);
        }
    }

    if (IsKeyPressed(KEY_W)) {
        free(debug_buffer);
        debug_buffer = NULL;
    }
}

void play_midi() {
    prepare_sound();

    // for debug we call portaudioCallback and display the samples until the user presses w on the keyboard
    debug_buffer = malloc(512 * 100000 * sizeof(float));
    portaudioUserData data = {
        .notes = notes,
        .notes_count = notes_count,
        .current_tick = 0,
        .active_notes = { }
    };

    for (int i = 0; i < 10000; i++) {
        portaudioCallback(
            NULL, 
            (void*)&debug_buffer[512 * i], 
            256,
            NULL,
            0,
            &data
        );
    }
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "miseq");
    SetTargetFPS(FPS);

    create_notes();
    
    while(!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(DARKGRAY);
            console_lines_this_frame = 0;

            const int notes_panel_top_offset = 200;
            if (debug_buffer == NULL) {
                DrawNotes(
                    0, 
                    notes_panel_top_offset,
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT - notes_panel_top_offset
                );
            } 
            else {
                DrawDebugBuffer(
                    0, 
                    notes_panel_top_offset,
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT - notes_panel_top_offset
                );
            }

            if (DrawButton("Generate", 1, SCREEN_WIDTH - 170, 20, 160, 40)) {
                create_notes();
            }

            if (DrawButton("Export MIDI", 2, SCREEN_WIDTH - 340, 20, 160, 40)) {
                save_notes_midi_file(notes, notes_count);
            }

            if (DrawButton("Play", 3, SCREEN_WIDTH - 510, 20, 160, 40)) {
                play_midi();
            }

        EndDrawing();
    }
    CloseWindow();
}

