#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int ALLOC_SIZE = 1024;

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

    printf("Note on: %d velocity %d: ", key, velocity);
    print_hex(data + start, size - start); 
}

void append_note_off(uint8_t key, uint8_t velocity) {
    uint64_t start = size;
    
    append_byte(0x80); // channel 0
    append_byte(key);
    append_byte(velocity);

    printf("Note off: %d velocity %d: ", key, velocity);
    print_hex(data + start, size - start); 
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

    if (ticks > 0) {
        printf("Delay: %d ticks: ", ticks);
        print_hex(data + start, size - start); 
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

    printf("Header chunk:\n");
    print_hex(data, size);
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
    append_delta_time(0);
    append_note_on(0x24, 0x64);
    append_delta_time(30000);
    append_note_on(0x22, 0x64);
    append_delta_time(127);
    append_note_off(0x24, 0x64);
    append_delta_time(127);
    append_note_off(0x22, 0x64);

    // end of track meta event
    append_byte(0xff);
    append_byte(0x2f);
    append_byte(0);

    // finally set chunk length
    uint8_t* pointer = (uint8_t*)data + length_position;
    uint32_t length_value = (uint32_t) (size - chunk_start);
    uint32_t value = le_to_be(length_value);
    memcpy((uint64_t*)pointer, &value, 4);

    printf("\n");
    printf("Full track chunk\n");
    print_hex(data + chunk_start, size - chunk_start); 
}

void write_data(const char* filename, void* data, uint64_t size) {
    FILE* file = fopen("1.mid", "wb");
    
    if (file == NULL) {
        printf("Error opening file %s.\n", filename);
        return;
    }

    fwrite(data, 1, size, file);
    fclose(file);
    printf("Wrote %d bytes to %s.\n", size, filename);
}

int main() {
    data = malloc(ALLOC_SIZE);
    memset(data, 0, ALLOC_SIZE);

    append_header();
    printf("\n");

    append_track_chunk();
    printf("\n");

    printf("Full document:\n");
    print_hex(data, size);
    printf("\n");

    write_data("1.mid", data, size);
}

