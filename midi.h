struct Note {
    uint8_t key;
    uint8_t velocity;
    uint32_t start_tick;
    uint32_t end_tick;
    bool is_selected;
};

struct NoteEvent {
    uint8_t key;
    uint8_t velocity;
    uint32_t tick;
    bool is_on;
};

void save_notes_midi_file(struct Note *notes, int notes_count);
