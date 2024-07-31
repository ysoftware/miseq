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
// TODO: switch to Raylib MusicStream and add playback tracking
// TODO: proper waveform drawing

#define NOTES_LIMIT 10000

// predeclarations
void create_waveform(void);
void create_sound(void);

typedef struct {
    Vector2 selection_first_point;
    ScrollZoom notes_scroll_zoom_state;
    ScrollZoom waveform_scroll_zoom_state;
    char *error_message;

    bool is_displaying_waveform;
    Note notes[NOTES_LIMIT];
    int notes_count;
    float waveform_samples[2048 * NOTES_LIMIT];
    int waveform_samples_count;

    Music *sound;
} State;

State *state = NULL;

// functions

float inverse_lerp(float a, float b, float f) {
    if (a == b)  return 0;
    return (f - a) / (b - a);
}

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

double random_value(void) {
    double value = (double)GetRandomValue(0, 100000) / 100000;
    return value;
}

void create_notes(void) {
    srand(time(0));
    state->notes_count = 0;

    uint32_t current_tick = 0;

    double base_velocity = 80;
    double base_key = 100;
    double base_note_duration = 20;

    double wave1_random = 20 * random_value();
    double wave2_random = 15 * random_value();
    double wave3_random = 5 * random_value();

    for (double i = 0; i < 10; i++) {
        double wave1 = cos(i / wave1_random) * 60 * random_value();
        double wave2 = sin(i / wave2_random) * 40 * random_value();
        double wave3 = sin(i / wave3_random) * 10 * random_value();

        uint8_t key = (uint8_t) (base_key - wave1);
        uint8_t velocity = (uint8_t) (base_velocity - wave2);
        uint8_t note_duration = (uint8_t) (base_note_duration - wave3);
    
        state->notes[state->notes_count] = (Note) {
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
    if (state->notes_count == 0) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        state->notes_scroll_zoom_state.target_zoom_x = 0.5f;
        state->notes_scroll_zoom_state.target_zoom_y = 0.5f;
        state->notes_scroll_zoom_state.target_scroll = 0;
    }

    float key_height = 3 + state->notes_scroll_zoom_state.zoom_y * 10;
    float tick_width = 0.05 + state->notes_scroll_zoom_state.zoom_x * 3;
    float content_size = tick_width * state->notes[state->notes_count-1].end_tick; // NOTE: only considering notes are sorted
    assert(content_size > 0);

    float scroll_offset = 0;
    process_scroll_interaction(
        &state->notes_scroll_zoom_state, 
        content_size, 
        view_width,
        &scroll_offset
    );

    Rectangle background_rect = calculate_content_rect(
        view_x, view_width, 
        view_y, view_height, 
        content_size, scroll_offset
    );
    DrawRectangleRec(background_rect, GRAY);

    // selection state
    bool did_edit_notes = false;
    Vector2 mouse_position = GetMousePosition();
    Rectangle view_rectangle = (Rectangle) { view_x, view_y, view_width, view_height };
    bool is_mouse_in_bounds = CheckCollisionPointRec(mouse_position, view_rectangle);
    
    bool is_selecting = !Vector2Equals(state->selection_first_point, Vector2Zero());
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !is_selecting && is_mouse_in_bounds) {
        state->selection_first_point = mouse_position;
        is_selecting = true;
    }

    Rectangle selection_rectangle = { };

    // selection rect
    if ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) && is_selecting) {
        Vector2 selection_second_point = mouse_position;

        float start_x = fmin(state->selection_first_point.x, selection_second_point.x);
        float start_y = fmin(state->selection_first_point.y, selection_second_point.y);
        Vector2 start = (Vector2) {
            fmin(view_x + view_width, fmax(view_x, start_x)),
            fmin(view_y + view_height, fmax(view_y, start_y)),
        };

        float end_x = fmax(state->selection_first_point.x, selection_second_point.x);
        float end_y = fmax(state->selection_first_point.y, selection_second_point.y);
        Vector2 end = (Vector2) {
            fmin(view_x + view_width, fmax(view_x, end_x)),
            fmin(view_y + view_height, fmax(view_y, end_y)),
        };

        Vector2 size = Vector2Subtract(end, start);
        selection_rectangle = (Rectangle) { start.x, start.y, size.x, size.y };
       
        if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            DrawRectangleRec(selection_rectangle, BLUE);
        } else {
            state->selection_first_point = Vector2Zero();
        }
    } else {
        state->selection_first_point = Vector2Zero();
    }

    for (int i = 0; i < state->notes_count; i++) {
        Note note = state->notes[i];
        if (note.is_deleted)  continue;

        Rectangle note_rect = {
            view_x - scroll_offset + (note.start_tick * tick_width),
            view_y + (view_height / 2) + (64 - note.key) * key_height,
            (note.end_tick - note.start_tick) * tick_width,
            key_height 
        };

        Rectangle note_clipped_rect = GetCollisionRec(background_rect, note_rect);
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

    Console("Samples count    %d", state->waveform_samples_count);

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        state->waveform_scroll_zoom_state.target_zoom_x = 0.5f;
        state->waveform_scroll_zoom_state.target_zoom_y = 0.5f;
        state->waveform_scroll_zoom_state.target_scroll = 0;
    }

    /* // TODO: select proper value for wave_amplitude */
    float sample_width = state->waveform_scroll_zoom_state.zoom_x * 1.0f;
    float wave_amplitude = 100 + state->waveform_scroll_zoom_state.zoom_y * (view_height * 3 - 100);
    float content_size = sample_width * state->waveform_samples_count;
    assert(content_size > 0);

    Console("Sample width %f", sample_width);

    float scroll_offset = 0;
    process_scroll_interaction(
        &state->waveform_scroll_zoom_state, 
        content_size, 
        view_width,
        &scroll_offset
    );

    Rectangle background_rect = calculate_content_rect(
        view_x, view_width, 
        view_y, view_height, 
        content_size, scroll_offset
    );
    DrawRectangleRec(background_rect, GRAY);

    int draw_calls_count = 0;

    if (sample_width > 0.3) { // draw actual wave (zoomed in)

        float sample_width_percent = inverse_lerp(0.3, 1, sample_width);
        float line_thickness = lerp(2.0, 3.5, sample_width_percent);
        int skipping_frames = (int) lerp(3, 1, sample_width_percent);

        Console("Line thickness %f", line_thickness);
        Console("Skipping frames %d", skipping_frames);

        float previous_value = 0;
        for (int i = 0; i < state->waveform_samples_count; i++) {
            float value = state->waveform_samples[i * 2];

            if (i == 0) { // nothing to draw since there is only 1 point yet
                previous_value = value;
                continue;
            }

            if (i % skipping_frames != 0)  continue;

            // draw line
            float x0 = (i - 1) * sample_width - scroll_offset + view_x;
            float x1 = i * sample_width - scroll_offset + view_x;
            float y0 = view_height - (view_height/2 - (-1.0f * previous_value * wave_amplitude)) + view_y;
            float y1 = view_height - (view_height/2 - (-1.0f * value * wave_amplitude)) + view_y;

            previous_value = value;

            float left_edge = view_x;
            float right_edge = background_rect.x + background_rect.width;
            float top_edge = view_y;
            float bottom_edge = background_rect.y + background_rect.height;

            // check if not beyond the bounds
            if (x1 > right_edge || x0 < left_edge)  continue;
            if ((y0 < top_edge && y1 < top_edge) || (y0 > bottom_edge && y1 > bottom_edge))  continue;

            Vector2 point1 = (Vector2) {
                fmax(left_edge, x0),
                fmin(bottom_edge, fmax(top_edge, y0))
            };

            Vector2 point2 = (Vector2) {
                fmin(right_edge, x1),
                fmax(top_edge, fmin(bottom_edge, y1))
            };

            DrawLineEx(point1, point2, line_thickness, BLACK);
            draw_calls_count += 1;
        }
    } else { // draw zoomed out
        float sample_width_percent = inverse_lerp(0.01, 0.3, sample_width);
        float skipping_frames_value = lerp(400, 20, sample_width_percent);
        int skipping_frames = (int) skipping_frames_value;

        float highest_value = 0;
        float sum = 0;
        for (int i = 0; i < state->waveform_samples_count; i++) {
            float value = state->waveform_samples[i * 2];

            if (fabs(value) > highest_value) {
                highest_value = fabs(value);
            }

            sum += value * value;

            if (i % skipping_frames != 0)  continue;
            int j = i / skipping_frames;

            float x = (j * skipping_frames - 1) * sample_width - scroll_offset + view_x;
            float width = sample_width * skipping_frames;

            float height_peak = highest_value * 2 * wave_amplitude;
            float y_peak = view_y + view_height / 2 - height_peak / 2;
            DrawRectangleRec((Rectangle) { x, y_peak, width, height_peak }, BLACK);
            draw_calls_count += 1;

            float rms_value = sqrt(sum / skipping_frames);
            float height_rms = rms_value * 2 * wave_amplitude;
            float y_rms = view_y + view_height / 2 - height_rms / 2;
            DrawRectangleRec((Rectangle) { x, y_rms, width, height_rms }, BLUE);
            draw_calls_count += 1;

            highest_value = 0;
            sum = 0;
        }

        Console("sample_width %f%%", sample_width_percent);
        Console("Skipping frames %d", skipping_frames);
    }

    // vertical line separator every second (excluding 0)
    for (int i = SAMPLE_RATE; i < state->waveform_samples_count; i += SAMPLE_RATE) {
        Rectangle separator_rect = {
            view_x - scroll_offset + (i * sample_width),
            view_y,
            3,
            view_height
        };

        DrawRectangleRec(separator_rect, DARKGRAY);
    }

    Console("Draw calls count %d", draw_calls_count);

    if (DrawButton("MIDI", 100, view_x + view_width - 20 - 160, view_y + 20, 160, 40)) {
        state->is_displaying_waveform = false;
    }
}

// AUDIO

static float midiNoteToFrequency(uint8_t note) {
    return powf(2.0f, (note - 69) / 12.0f) * 440.0f;
}

typedef struct {
    Note* notes;
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

static int note_identifier(Note note) {
    return 1000000000 + note.start_tick * 1000 + note.key;
}

static bool create_samples_from_notes(float *buffer, SoundState *data, uint32_t frames_per_buffer, int frames_per_tick) {
    state->error_message = NULL;

    float attack_frames_length = SAMPLE_RATE / 25;
    float attack_decrement = 1.0f / attack_frames_length;

    float release_frames_length = SAMPLE_RATE / 0.2;
    float release_increment = 1.0f / release_frames_length;

    for (uint32_t buf_frame = 0; buf_frame < frames_per_buffer; buf_frame++) {
        
        // Start or stop notes
        if (data->current_frame % frames_per_tick == 0) {
            uint32_t current_tick = data->current_frame / frames_per_tick;

            for (int note_index = 0; note_index < data->notes_count; note_index++) {
                Note note = data->notes[note_index];
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
                            state->error_message = "MAX_POLYPHONY exceeded. Starting a midi event is impossible.";
                            printf("%s\n", state->error_message);
                            return false;
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
                            state->error_message = "MAX_POLYPHONY exceeded. Stopping a midi event is impossible.";
                            printf("%s\n", state->error_message);
                            return false;
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

        *buffer++ = mixedSample; // left
        *buffer++ = mixedSample; // right
        data->current_frame++;
    }

    return true;
}

void unload_sound(void) {
    if (state->sound == NULL)  return;
    add_breadcrumbs(); 

    StopMusicStream(*state->sound);
    UnloadMusicStream(*state->sound);
    state->sound = NULL;
}

// TODO: audio error handling
void create_sound(void) {
    unload_sound();
    if (state->waveform_samples_count == 0)  return;
}

void create_waveform(void) {
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
        bool success = create_samples_from_notes(
            &state->waveform_samples[FRAMES_PER_BUFFER * current_frame * NUMBER_OF_CHANNELS],
            &data, 
            FRAMES_PER_BUFFER,
            frames_per_tick
        );

        if (!success) {
            state->waveform_samples_count = 0;
            return;
        }

        bool did_produce_silence = state->waveform_samples[FRAMES_PER_BUFFER * current_frame * NUMBER_OF_CHANNELS] == 0;
        uint32_t current_tick = data.current_frame / frames_per_tick;
        if (current_tick > end_tick && did_produce_silence)  break;
    }

    state->waveform_samples_count = current_frame * FRAMES_PER_BUFFER;
}

// plugin life cycle

void plug_init(void) {
    state = malloc(sizeof(*state));
    assert(state != NULL && "Buy more RAM lol");
    memset(state, 0, sizeof(*state));
    
    create_notes();
    create_waveform();
    create_sound();
    
    state->waveform_scroll_zoom_state = (ScrollZoom) {
        .zoom_x = 0.5f,
        .zoom_y = 0.5f,
        .target_zoom_x = 0.5f,
        .target_zoom_y = 0.5f
    };
    
    state->notes_scroll_zoom_state = (ScrollZoom) {
        .zoom_x = 0.5f,
        .zoom_y = 0.5f,
        .target_zoom_x = 0.5f,
        .target_zoom_y = 0.5f
    };
}

void plug_cleanup(void) {
    unload_sound();
    CloseAudioDevice();
    free(state);
    state = NULL;
}

void *plug_pre_reload(void) {
    return state;
}

void plug_post_reload(void *old_state) {
    state = old_state;
}

void plug_update(void) {
    assert(state != NULL && "Plugin state is not initialized.");
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    BeginDrawing();
        ClearBackground(DARKGRAY);
        ClearConsole();

        Console("FPS: %d", GetFPS());

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
            save_notes_wave_file(state->waveform_samples, state->waveform_samples_count, "export.wav");
        }

        if (state->sound == NULL) {
            DrawButton("Sound Init...", 4, screen_width - 510, 20, 160, 40);
        } else {
            if (IsMusicStreamPlaying(*state->sound)) {
                if (DrawButton("Stop", 4, screen_width - 510, 20, 160, 40)) {
                    StopMusicStream(*state->sound);
                }
            } else {
                if (DrawButton("Play", 4, screen_width - 510, 20, 160, 40)) {
                    PlayMusicStream(*state->sound);
                }
            }
        }
    
        if (state->error_message != NULL) {
            DrawConsoleLine(state->error_message);
        }

    EndDrawing();
}
