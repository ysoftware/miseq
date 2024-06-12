#ifndef SHARED_INCLUDES
#define SHARED_INCLUDES

typedef struct {
    uint8_t key;
    uint8_t velocity;
    uint32_t start_tick;
    uint32_t end_tick;
    bool is_selected;
    bool is_deleted;
} Note;

// audio setup
#define MAX_POLYPHONY       16
#define A4_FREQUENCY        700
#define FRAMES_PER_BUFFER   256
#define NUMBER_OF_CHANNELS  2
#define SAMPLE_RATE         (44100)

#endif // SHARED_INCLUDES
