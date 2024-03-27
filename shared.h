#ifndef SHARED_INCLUDES
#define SHARED_INCLUDES

struct Note {
    uint8_t key;
    uint8_t velocity;
    uint32_t start_tick;
    uint32_t end_tick;
    bool is_selected;
};
#endif // SHARED_INCLUDES
