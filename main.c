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
#include "ui.h"

#define NOTES_LIMIT 10000
const int FPS = 120;
const int SCREEN_WIDTH = 2500;
const int SCREEN_HEIGHT = 1000;

// notes
struct Note notes[NOTES_LIMIT];
int notes_count = 0;

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

    for (double i = 0; i < 200; i++) {
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
            current_tick + note_duration,
            false
        };

        notes[notes_count] = note;
        notes_count += 1;

        current_tick += note_duration;
    }
}

// USER INTERFACE

void DrawNotes(int view_x, int view_y, int view_width, int view_height) {
    // draw notes
    if (notes_count == 0) return;

    static struct ScrollZoom scroll_zoom_state = {
        .zoom_x = 0.5f,
        .zoom_y = 0.5f,
        .target_zoom_x = 0.5f,
        .target_zoom_y = 0.5f
    };

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        scroll_zoom_state.target_zoom_x = 0.5f;
        scroll_zoom_state.target_zoom_y = 0.5f;
        scroll_zoom_state.target_scroll = 0;
    } else {
        process_scroll_interaction(&scroll_zoom_state);
    }

    float key_height = scroll_zoom_state.zoom_y * 12;
    float tick_width = scroll_zoom_state.zoom_x * 2;
    float content_size = tick_width * notes[notes_count-1].end_tick; // TODO: only considering notes are sorted
    if (content_size < view_width)  scroll_zoom_state.target_scroll = 0.0f;
    float scroll_offset = scroll_zoom_state.scroll * (content_size - view_width);

    // for the next frame
    assert(content_size > 0);
    scroll_zoom_state.scroll_speed = view_width / content_size;

    // background
    DrawRectangle(
        -scroll_offset, 
        view_y,
        content_size,
        128 * key_height, 
        GRAY
    );

    // selection state
    static Vector2 selection_first_point;
    Vector2 mouse_position = GetMousePosition();
    struct Rectangle view_rectangle = (struct Rectangle) { view_x, view_y, view_width, view_height };
    bool is_mouse_in_bounds = CheckCollisionPointRec(mouse_position, view_rectangle);
    
    bool is_selecting = !Vector2Equals(selection_first_point, Vector2Zero());
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !is_selecting && is_mouse_in_bounds) {
        selection_first_point = mouse_position;
        is_selecting = true;
    }

    struct Rectangle selection_rectangle;

    // selection rect
    if ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) && is_selecting) {
        Vector2 selection_second_point = mouse_position;

        float start_x = fmin(selection_first_point.x, selection_second_point.x);
        float start_y = fmin(selection_first_point.y, selection_second_point.y);
        Vector2 start = (struct Vector2) {
            fmin(view_x + view_width, fmax(view_x, start_x)),
            fmin(view_y + view_height, fmax(view_y, start_y)),
        };

        float end_x = fmax(selection_first_point.x, selection_second_point.x);
        float end_y = fmax(selection_first_point.y, selection_second_point.y);
        Vector2 end = (struct Vector2) {
            fmin(view_x + view_width, fmax(view_x, end_x)),
            fmin(view_y + view_height, fmax(view_y, end_y)),
        };

        Vector2 size = Vector2Subtract(end, start);
        selection_rectangle = (struct Rectangle) { start.x, start.y, size.x, size.y };
       
        if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            DrawRectangleRec(selection_rectangle, BLUE);
        } else {
            selection_first_point = Vector2Zero();
        }
    } else {
        selection_first_point = Vector2Zero();
    }

    for (int i = 0; i < notes_count; i++) {
        struct Note note = notes[i];

        int posX = note.start_tick * tick_width - (int) scroll_offset;
        int posY = 128 * key_height - note.key * key_height;
        int width = (note.end_tick - note.start_tick) * tick_width;
        int height = key_height;

        struct Rectangle note_rectangle = { view_x + posX, view_y + posY, width, height };
        bool is_being_selected = CheckCollisionRecs(selection_rectangle, note_rectangle);
        Color note_color = is_being_selected || note.is_selected ? GREEN : BLACK;

        if (posX < view_width && posX + width > 0 && posY < view_height && posY + height > 0) {
            DrawRectangleRec(note_rectangle, note_color);
        }

        // save selection state on mouse release
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !note.is_selected && is_being_selected) {
            notes[i].is_selected = true;
        }
    }

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
    int samples_count = 1000 * 512;

    static struct ScrollZoom scroll_zoom_state = {
        .zoom_x = 0.5f,
        .zoom_y = 0.5f,
        .target_zoom_x = 0.5f,
        .target_zoom_y = 0.5f
    };

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        scroll_zoom_state.target_zoom_x = 0.5f;
        scroll_zoom_state.target_zoom_y = 0.5f;
        scroll_zoom_state.target_scroll = 0;
    } else {
        process_scroll_interaction(&scroll_zoom_state);
    }

    float sample_width = scroll_zoom_state.zoom_x * 1.0f;
    float wave_amplitude = scroll_zoom_state.zoom_y * 4000;
    float content_size = sample_width * samples_count;
    if (content_size < view_width)  scroll_zoom_state.target_scroll = 0.0f;
    float scroll_offset = scroll_zoom_state.scroll * (content_size - view_width);

    // for the next frame
    assert(content_size > 0);
    scroll_zoom_state.scroll_speed = view_width / content_size;

    DrawRectangle(
        -scroll_offset, 
        view_y,
        content_size,
        view_height, 
        GRAY
    );

    for (int i = 0; i < samples_count; i++) {
        float value = debug_buffer[i * 6];

        float posX = i * sample_width - (int) scroll_offset;
        float posY = view_height - (view_height/2 - (-1.0f * value * wave_amplitude));
        Vector2 position = { view_x + posX, view_y + posY };
        Vector2 size = { 2, view_height / 50 };

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
    /* prepare_sound(); */

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
            ClearConsole();

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

