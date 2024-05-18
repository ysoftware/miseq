#include "raylib.h"
#include "raymath.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "shared.h"
#include "midi.h"
#include "wav.h"
#include "ui.h"

// TODO: generate wave in a background thread
// TODO: add undo for breaking actions: notes delete, generation

#define NOTES_LIMIT 10000

// predeclarations
void create_waveform();
void create_sound();

typedef struct {
    bool is_displaying_waveform;
    struct Note notes[NOTES_LIMIT];
    int notes_count;
    float waveform_samples[2048 * NOTES_LIMIT];
    int waveform_samples_count;
    Sound *sound;
} State;

State *state = NULL;

// functions

double random_value() {
    double value = (double)GetRandomValue(0, 100000) / 100000;
    return value;
}

void create_notes() {
    srand(time(NULL));
    state->notes_count = 0;

    uint32_t current_tick = 0;

    double base_velocity = 80;
    double base_key = 64;
    double base_note_duration = 20;

    double wave1_random = 20 * random_value();
    double wave2_random = 15 * random_value();
    double wave3_random = 5 * random_value();

    for (double i = 0; i < 100; i++) {
        double wave1 = cos(i / wave1_random) * 30 * random_value();
        double wave2 = sin(i / wave2_random) * 40 * random_value();
        double wave3 = sin(i / wave3_random) * 10 * random_value();

        uint8_t key = (uint8_t) (base_key - wave1);
        uint8_t velocity = (uint8_t) (base_velocity - wave2);
        uint8_t note_duration = (uint8_t) (base_note_duration - wave3);
    
        state->notes[state->notes_count] = (struct Note) {
            key,
            velocity,
            current_tick,
            current_tick + note_duration,
            .is_selected = false,
            .is_deleted = false
        };

        state->notes_count += 1;
        current_tick += note_duration;
    }
}

// USER INTERFACE

void DrawNotes(float view_x, float view_y, float view_width, float view_height) {
    // draw notes
    if (state->notes_count == 0) return;

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

    float key_height = 3 + scroll_zoom_state.zoom_y * 10;
    float tick_width = 0.05 + scroll_zoom_state.zoom_x * 3;
    float content_size = tick_width * state->notes[state->notes_count-1].end_tick; // NOTE: only considering notes are sorted
    assert(content_size > 0);

    float scroll_offset = scroll_zoom_state.scroll * (content_size - view_width);

    // for the next frame
    scroll_zoom_state.scroll_speed = view_width / content_size;
    if (content_size < view_width)  scroll_zoom_state.target_scroll = 0.0f; // when content is not wide enough, reset scroll

    // background
    struct Rectangle background_rect = {
        fmax(view_x, view_x-scroll_offset),
        view_y,
        fmin(view_width+fmin(0, scroll_offset), content_size-fmax(0, scroll_offset)),
        view_height
    };
    DrawRectangleRec(background_rect, GRAY);

    // selection state
    bool did_edit_notes = false;
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

    for (int i = 0; i < state->notes_count; i++) {
        struct Note note = state->notes[i];
        if (note.is_deleted)  continue;

        struct Rectangle note_rect = {
            view_x - scroll_offset + (note.start_tick * tick_width),
            view_y + (view_height / 2) + (64 - note.key) * key_height,
            (note.end_tick - note.start_tick) * tick_width,
            key_height 
        };

        struct Rectangle note_clipped_rect = GetCollisionRec(background_rect, note_rect);
        bool is_inside_selection_rect = CheckCollisionRecs(selection_rectangle, note_rect);

        Color note_color; 
        if (is_inside_selection_rect && IsKeyDown(KEY_LEFT_CONTROL) == note.is_selected) {
            if (note.is_selected) {
                note_color = RED;
            } else {
                note_color = YELLOW;
            }
        } else {
            note_color = note.is_selected ? GREEN : BLACK;
        }

        if (note_clipped_rect.width > 0) {
            DrawRectangleRec(note_clipped_rect, note_color);
        }

        // save selection state on mouse release
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && is_inside_selection_rect) {
            state->notes[i].is_selected = !IsKeyDown(KEY_LEFT_CONTROL);
        }

        if (IsKeyPressed(KEY_D) && state->notes[i].is_selected) {
            state->notes[i].is_deleted = true;
            did_edit_notes = true;
        }
    }

    if (did_edit_notes) {
        create_waveform();
        create_sound();
    }

    if (DrawButton("Waveform", 100, view_x + view_width - 20 - 160, view_y + 20, 160, 40)) {
        state->is_displaying_waveform = true;
    }
}

void DrawWaveform(float view_x, float view_y, float view_width, float view_height) {
    if (state->waveform_samples_count == 0)  return;

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

    int samples_to_draw_count = state->waveform_samples_count;

    // TODO: select proper value for wave_amplitude
    float sample_width = scroll_zoom_state.zoom_x * 1.0f;
    float wave_amplitude = 100 + scroll_zoom_state.zoom_y * (view_height * 3 - 100);

    float content_size = sample_width * samples_to_draw_count;
    float scroll_offset = scroll_zoom_state.scroll * (content_size - view_width);
    assert(content_size > 0);

    // for the next frame
    scroll_zoom_state.scroll_speed = view_width / content_size;
    if (content_size < view_width)  scroll_zoom_state.target_scroll = 0.0f; // when content is not wide enough, reset scroll

    // background
    // TODO: abstract these calculations for 'normal scrollable content views'
    struct Rectangle background_rect = {
        fmax(view_x, view_x-scroll_offset),
        view_y,
        fmin(view_width+fmin(0, scroll_offset), content_size-fmax(0, scroll_offset)),
        view_height, 
    };
    DrawRectangleRec(background_rect, GRAY);

    // draw section separators every 1 sec
    int samples_per_section = SAMPLE_RATE;

    // TODO: adjust this drawing code to bounds check
    for (int i = 0; i < samples_to_draw_count; i++) {
        float value = state->waveform_samples[i * 2];

        float posX = i * sample_width - scroll_offset;
        float posY = view_height - (view_height/2 - (-1.0f * value * wave_amplitude));
        Vector2 position = { view_x + posX, view_y + posY };
        Vector2 size = { 2, view_height / 50 };

        if (posX < view_width && posX > 0 && posY < view_height && posY > 0) {
            DrawRectangleV(position, size, BLACK);
        }

        if (i % samples_per_section == 0) {
            struct Rectangle separator_rect = {
                view_x - scroll_offset + (i * sample_width),
                view_y,
                5,
                view_height
            };
            DrawRectangleRec(separator_rect, DARKGRAY);
        }
    }

    if (DrawButton("MIDI", 100, view_x + view_width - 20 - 160, view_y + 20, 160, 40)) {
        state->is_displaying_waveform = false;
    }
}

// AUDIO

static float midiNoteToFrequency(uint8_t note) {
    return powf(2.0f, (note - 69) / 12.0f) * 440.0f;
}

typedef struct {
    struct Note* notes;
    int notes_count;
    uint32_t current_frame;

    struct {
        bool active;
        bool is_releasing;
        float phase_accumulator;
        float frequency;
        float attack;
        int identifier;
    } active_notes[MAX_POLYPHONY];
} SoundState;

// TODO: export .wav in wav.h

static int note_identifier(struct Note note) {
    return 1000000000 + note.start_tick * 1000 + note.key;
}

static void create_samples_from_notes(float *buffer, SoundState *data, uint32_t frames_per_buffer, int frames_per_tick) {
    float attack_frames_length = SAMPLE_RATE / 25;
    float attack_decrement = 1.0f / attack_frames_length;

    float release_frames_length = SAMPLE_RATE / 3;
    float release_increment = 1.0f / release_frames_length;

    for (uint32_t buf_frame = 0; buf_frame < frames_per_buffer; buf_frame++) {
        
        // Start or stop notes
        if (data->current_frame % frames_per_tick == 0) {
            uint32_t current_tick = data->current_frame / frames_per_tick;

            for (int note_index = 0; note_index < data->notes_count; note_index++) {
                struct Note note = data->notes[note_index];
                if (note.is_deleted)  continue;

                // TODO: test various polyphony settings
                if (current_tick == note.start_tick) {
                    for (int i = 0; i < MAX_POLYPHONY; i++) {
                        if (!data->active_notes[i].active) { // search for a first not yet active note
                            data->active_notes[i].active = true;
                            data->active_notes[i].is_releasing = false;
                            data->active_notes[i].frequency = midiNoteToFrequency(note.key);
                            data->active_notes[i].attack = 0.0f;
                            data->active_notes[i].identifier = note_identifier(note);
                            break;
                        } else if (i == MAX_POLYPHONY - 1) {
                            printf("[ERROR] MAX_POLYPHONY exceeded. Starting a midi event is impossible.\n");
                            assert(false);
                        }
                    }
                } else if (current_tick == note.end_tick) {
                    int note_id = note_identifier(note);

                    for (int i = 0; i < MAX_POLYPHONY; i++) {

                        if (data->active_notes[i].identifier == note_id) { // search for the note by its identifier
                            assert(data->active_notes[i].active);
                            data->active_notes[i].is_releasing = true;
                            break;
                        } else if (i == MAX_POLYPHONY - 1) {
                            printf("[ERROR] MAX_POLYPHONY exceeded. Stopping a midi event is impossible.\n");
                            assert(false);
                        }
                    }
                }
            }
        }

        // Calculate attack/release and phase values
        for (int i = 0; i < MAX_POLYPHONY; i++) {
            if (!data->active_notes[i].active)  continue;

            float phase_increment = 2 * PI * data->active_notes[i].frequency / SAMPLE_RATE;
            data->active_notes[i].phase_accumulator += phase_increment;

            // keep the phase in 2PI range
            while (data->active_notes[i].phase_accumulator > 2 * PI) {
                data->active_notes[i].phase_accumulator -= 2 * PI;
            }

            // note is fully playing when is_releasing==false and attack==1.0f
            // otherwise attack will slowly increase or decrease depending on is_releasing
            if (data->active_notes[i].is_releasing) {
                data->active_notes[i].attack -= release_increment;

                if (data->active_notes[i].attack <= 0.0f) {
                    data->active_notes[i].active = false;
                }
            } else {
                data->active_notes[i].attack += attack_decrement;
                if (data->active_notes[i].attack > 1.0f) {
                    data->active_notes[i].attack = 1.0f;
                }
            }
        }

        // Mix active notes
        float mixedSample = 0.0f;
        for (int i = 0; i < MAX_POLYPHONY; i++) {
            if (!data->active_notes[i].active)  continue;

            float wave_value = sinf(data->active_notes[i].phase_accumulator);
            wave_value *= data->active_notes[i].attack;
            mixedSample += wave_value / MAX_POLYPHONY;
        }

        // TODO: normalize the wave

        *buffer++ = mixedSample; // left
        *buffer++ = mixedSample; // right
        data->current_frame++;
    }
}

void unload_sound() {
    if (state->sound == NULL)  return;
    StopSound(*state->sound);
    UnloadSound(*state->sound);
    free(state->sound);
    state->sound = NULL;
}

// TODO: audio error handling
void create_sound() {
    unload_sound();

    Wave wave = (struct Wave) {
        .frameCount = state->waveform_samples_count,
        .sampleRate = SAMPLE_RATE,
        .sampleSize = sizeof(float),
        .channels = NUMBER_OF_CHANNELS,
        .data = state->waveform_samples
    };

    state->sound = malloc(sizeof(Sound));
    *state->sound = LoadSoundFromWave(wave);
}

void create_waveform() {
    SoundState data = {
        .notes = state->notes,
        .notes_count = state->notes_count,
        .current_frame = 0,
        .active_notes = { }
    };

    uint32_t end_tick = state->notes[state->notes_count-1].end_tick; // NOTE: only considering notes are sorted
    int current_frame;
    int frames_per_tick = SAMPLE_RATE / 100;

    for (current_frame = 0; current_frame < 10000; current_frame++) {
        create_samples_from_notes(
            &state->waveform_samples[FRAMES_PER_BUFFER * current_frame * NUMBER_OF_CHANNELS],
            &data, 
            FRAMES_PER_BUFFER,
            frames_per_tick
        );

        bool did_produce_silence = state->waveform_samples[FRAMES_PER_BUFFER * current_frame * NUMBER_OF_CHANNELS] == 0;
        uint32_t current_tick = data.current_frame / frames_per_tick;
        if (current_tick > end_tick && did_produce_silence)  break;
    }

    state->waveform_samples_count = current_frame * FRAMES_PER_BUFFER;
}

// plugin life cycle

void plug_init() {
    state = malloc(sizeof(*state));
    assert(state != NULL && "Buy more RAM lol");
    memset(state, 0, sizeof(*state));

    create_notes();
    create_waveform();
    create_sound();
}

void plug_cleanup() {
    unload_sound();
    CloseAudioDevice();
}

void *plug_pre_reload() {
    return state;
}

void plug_post_reload(void *old_state) {
    state = old_state;
}

void plug_update() {
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    BeginDrawing();
        ClearBackground(DARKGRAY);
        ClearConsole();

        const int notes_panel_top_offset = 200;
        if (!state->is_displaying_waveform) {
            DrawNotes(
                20,
                notes_panel_top_offset,
                screen_width-40,
                screen_height - notes_panel_top_offset - 20
            );
        } 
        else {
            DrawWaveform(
                20, 
                notes_panel_top_offset,
                screen_width-40,
                screen_height - notes_panel_top_offset - 20
            );
        }

        if (DrawButton("Generate", 1, screen_width - 170, 20, 160, 40)) {
            unload_sound();
            create_notes();
            create_waveform();
            create_sound();
        }

        if (DrawButton("Export MIDI", 2, screen_width - 340, 20, 160, 40)) {
            save_notes_midi_file(state->notes, state->notes_count);
        }

        if (DrawButton("Export WAV", 3, screen_width - 340, 70, 160, 40)) {
            save_notes_wave_file(state->waveform_samples, 0);
        }

        if (state->sound == NULL) {
            DrawButton("Sound Init...", 4, screen_width - 510, 20, 160, 40);
        } else {
            if (IsSoundPlaying(*state->sound)) {
                if (DrawButton("Stop", 4, screen_width - 510, 20, 160, 40)) {
                    StopSound(*state->sound);
                }
            } else {
                if (DrawButton("Play", 4, screen_width - 510, 20, 160, 40)) {
                    PlaySound(*state->sound);
                }
            }
        }

    EndDrawing();
}
