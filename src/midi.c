#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "midi.h"

struct NoteEvent {
    uint8_t key;
    uint8_t velocity;
    uint32_t tick;
    bool is_on;
};

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

static void append_string(void *data, int *size, const char* bytes) {
    char* pointer = (char*)data + *size;
    int len = strlen(bytes);
    memcpy(pointer, bytes, len);
    *size += len;
}

static void append_byte(void *data, int *size, int8_t byte) {
    int8_t* pointer = (int8_t*)data + *size;
    memcpy(pointer, &byte, 1);
    *size += 1;
}

static void append_note_on(void *data, int *size, uint8_t key, uint8_t velocity) {
    append_byte(data, size, 0x90); // channel 0
    append_byte(data, size, key);
    append_byte(data, size, velocity);
}

static void append_note_off(void *data, int *size, uint8_t key, uint8_t velocity) {
    append_byte(data, size, 0x80); // channel 0
    append_byte(data, size, key);
    append_byte(data, size, velocity);
}

static void append_delta_time(void *data, int *size, uint32_t ticks) {
    if (ticks <= 127) {
        append_byte(data, size, (uint8_t) ticks);
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
            append_byte(data, size, byte);
        }
    }
}

static void append_header(void *data, int *size) {
    append_string(data, size, "MThd");

    // chunk length (4 bytes): header is always 6
    append_byte(data, size, 0);
    append_byte(data, size, 0);
    append_byte(data, size, 0);
    append_byte(data, size, 6);

    // format type (2 bytes): 0 single, 1 multi synced, 2 multi independent
    append_byte(data, size, 0);
    append_byte(data, size, 0);

    // number of tracks (2 bytes)
    append_byte(data, size, 0);
    append_byte(data, size, 1);

    // timing resolution (2 bytes)
    append_byte(data, size, 0x00);
    append_byte(data, size, 0x60);
}

static int compare_note_events(const void *a, const void *b) {
    const struct NoteEvent *note1 = (const struct NoteEvent *)a;
    const struct NoteEvent *note2 = (const struct NoteEvent *)b;
    if (note1->tick < note2->tick) return -1;
    else if (note1->tick > note2->tick) return 1;
    else return 0;
}

static void append_events_from_notes(void *data, int *size, struct Note *notes, int notes_count) {
    int events_count = notes_count * 2;
    struct NoteEvent events[events_count];

    for (int i = 0; i < notes_count; i++) {
        struct Note note = notes[i];
        events[i] = (struct NoteEvent) {
            note.key, note.velocity, note.start_tick, true
        };
        events[notes_count + i] = (struct NoteEvent) {
            note.key, note.velocity, note.end_tick, false
        };
    }

    qsort(events, events_count, sizeof(struct NoteEvent), compare_note_events);

    uint32_t current_tick = 0;    
    for (int i = 0; i < events_count; i++) {
        struct NoteEvent event = events[i];

        append_delta_time(data, size, event.tick - current_tick);
        current_tick = event.tick;

        if (event.is_on) {
            append_note_on(data, size, event.key, event.velocity);
        } else {
            append_note_off(data, size, event.key, event.velocity);
        }
    }
}

static void append_track_chunk_start(void *data, int *size, int *chunk_size, int *length_position) {
    append_string(data, size, "MTrk");

    // reserve chunk length (4 bytes) to set later
    *length_position = *size;
    append_byte(data, size, 0);
    append_byte(data, size, 0);
    append_byte(data, size, 0);
    append_byte(data, size, 0);

    *chunk_size = *size;
}

static void append_track_chunk_end(void *data, int *size, int chunk_start, int length_position) {
    append_byte(data, size, 0xff);
    append_byte(data, size, 0x2f);
    append_byte(data, size, 0);

    // finally set chunk length
    uint8_t* pointer = (uint8_t*)data + length_position;
    uint32_t length_value = (uint32_t) (*size - chunk_start);
    uint32_t value = le_to_be(length_value);
    memcpy((int*)pointer, &value, 4);
}

static void write_data(const char* filename, void* data, int size) {
    if (size == 0) {
        printf("Warning! There is no midi data to write to the file %s\n", filename);
        return;
    }

    FILE* file = fopen(filename, "wb");
    
    if (file == NULL) {
        printf("Error opening file %s.\n", filename);
        return;
    }

    fwrite(data, 1, size, file);
    fclose(file);
    printf("Written %d bytes to %s.\n", size, filename);
}

void save_notes_midi_file(struct Note *notes, int notes_count) {
    int size = 0;
    void *data = malloc(notes_count * sizeof(struct NoteEvent) + 100);

    append_header(data, &size);

    int chunk_size, length_position;
    append_track_chunk_start(data, &size, &chunk_size, &length_position);
    append_events_from_notes(data, &size, notes, notes_count);
    append_track_chunk_end(data, &size, chunk_size, length_position);

    write_data("1.mid", data, size);

    free(data);
}
