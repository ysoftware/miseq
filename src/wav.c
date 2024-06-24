#include "raylib.h"

#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "shared.h"
#include "wav.h"

void save_notes_wave_file(void* samples, int samples_count, char *filename) {
    Wave wave = (Wave) {
        .frameCount = samples_count,
        .sampleRate = SAMPLE_RATE,
        .sampleSize = 32, // NOTE: I don't know why this has to be 32 and not sizeof(float) like in another place. but this works and I did not yet have time to explore this mystery
        .channels = NUMBER_OF_CHANNELS,
        .data = samples
    };

    ExportWave(wave, filename);
    printf("Samples exported: %d\n", wave.frameCount);
    printf("Sample rate: %d\n", wave.sampleRate);
    printf("Channels: %d\n", wave.channels);
    printf("Sample size: %d\n", wave.sampleSize);
}
