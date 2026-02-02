// pico_ram.h
// PICO-8 memory layout - matches fake-08 exactly
// 32KB base (no userData) to fit 100KB budget

#ifndef PICO_RAM_H
#define PICO_RAM_H

#include "pico_config.h"

// Memory Map Addresses (matches PICO-8 spec)
// 0x0000-0x0FFF: Sprite sheet (0-127)
// 0x1000-0x1FFF: Sprite sheet (128-255) / Map (rows 32-63) shared
// 0x2000-0x2FFF: Map (rows 0-31)
// 0x3000-0x30FF: Sprite flags
// 0x3100-0x31FF: Music
// 0x3200-0x42FF: Sound effects
// 0x4300-0x5DFF: General use RAM
// 0x5E00-0x5EFF: Persistent cart data
// 0x5F00-0x5F3F: Draw state
// 0x5F40-0x5F7F: Hardware state
// 0x5F80-0x5FFF: GPIO pins
// 0x6000-0x7FFF: Screen buffer (8KB)
// 0x8000-0xFFFF: User data (NOT INCLUDED - saves 32KB)

// Note structure (2 bytes) - matches fake-08 bit layout exactly
typedef struct __attribute__((packed)) {
    uint8_t data[2];
} pico_note_t;

// Note accessors (bit layout matches fake-08)
static inline uint8_t pico_note_key(const pico_note_t* n) {
    return n->data[0] & 0x3F;
}
static inline uint8_t pico_note_waveform(const pico_note_t* n) {
    return ((n->data[1] & 0x01) << 2) | ((n->data[0] & 0xC0) >> 6);
}
static inline uint8_t pico_note_volume(const pico_note_t* n) {
    return (n->data[1] & 0x0E) >> 1;
}
static inline uint8_t pico_note_effect(const pico_note_t* n) {
    return (n->data[1] & 0x70) >> 4;
}
static inline uint8_t pico_note_custom(const pico_note_t* n) {
    return (n->data[1] & 0x80) >> 7;
}

// Note setters
static inline void pico_note_set_key(pico_note_t* n, uint8_t v) {
    n->data[0] = (n->data[0] & 0xC0) | (v & 0x3F);
}
static inline void pico_note_set_waveform(pico_note_t* n, uint8_t v) {
    n->data[0] = (n->data[0] & 0x3F) | ((v & 0x03) << 6);
    n->data[1] = (n->data[1] & 0xFE) | ((v >> 2) & 0x01);
}
static inline void pico_note_set_volume(pico_note_t* n, uint8_t v) {
    n->data[1] = (n->data[1] & 0xF1) | ((v & 0x07) << 1);
}
static inline void pico_note_set_effect(pico_note_t* n, uint8_t v) {
    n->data[1] = (n->data[1] & 0x8F) | ((v & 0x07) << 4);
}

// SFX Structure (68 bytes) - matches fake-08
typedef struct __attribute__((packed)) {
    pico_note_t notes[32];      // 64 bytes
    uint8_t editor_mode;        // filters + editor flag
    uint8_t speed;              // 1-255
    uint8_t loop_start;
    uint8_t loop_end;
} pico_sfx_t;

// Song Structure (4 bytes) - matches fake-08
typedef struct __attribute__((packed)) {
    uint8_t data[4];            // channel data, flags in MSB
} pico_song_t;

// Song accessors
static inline uint8_t pico_song_sfx(const pico_song_t* s, int ch) {
    return s->data[ch] & 0x7F;
}
static inline uint8_t pico_song_start(const pico_song_t* s) {
    return (s->data[0] >> 7) & 1;
}
static inline uint8_t pico_song_loop(const pico_song_t* s) {
    return (s->data[1] >> 7) & 1;
}
static inline uint8_t pico_song_stop(const pico_song_t* s) {
    return (s->data[2] >> 7) & 1;
}
static inline uint8_t pico_song_mode(const pico_song_t* s) {
    return (s->data[3] >> 7) & 1;
}

// Draw State (64 bytes at 0x5F00) - matches fake-08 EXACTLY
typedef struct __attribute__((packed)) {
    uint8_t draw_pal[16];       // 0x5F00: draw palette map (transparency in bit 4)
    uint8_t screen_pal[16];     // 0x5F10: screen palette map
    uint8_t clip_xb;            // 0x5F20: clip left (begin)
    uint8_t clip_yb;            // 0x5F21: clip top (begin)
    uint8_t clip_xe;            // 0x5F22: clip right (end, exclusive)
    uint8_t clip_ye;            // 0x5F23: clip bottom (end, exclusive)
    uint8_t unknown_5f24;       // 0x5F24
    uint8_t color;              // 0x5F25: pen color (lower nibble = color, upper = secondary)
    uint8_t text_x;             // 0x5F26: cursor X
    uint8_t text_y;             // 0x5F27: cursor Y
    int16_t camera_x;           // 0x5F28: camera X
    int16_t camera_y;           // 0x5F2A: camera Y
    uint8_t draw_mode;          // 0x5F2C
    uint8_t devkit_mode;        // 0x5F2D
    uint8_t persist_pal;        // 0x5F2E
    uint8_t pause_state;        // 0x5F2F
    uint8_t suppress_pause;     // 0x5F30
    uint8_t fillp[2];           // 0x5F31: fill pattern
    uint8_t fillp_trans;        // 0x5F33: fill pattern transparency bit
    uint8_t color_flag;         // 0x5F34: color setting flag (bit 1 = inverted fill)
    uint8_t line_invalid;       // 0x5F35
    uint8_t unknown_5f36;       // 0x5F36
    uint8_t unknown_5f37;       // 0x5F37
    uint8_t tline_w;            // 0x5F38: tline map width
    uint8_t tline_h;            // 0x5F39: tline map height
    uint8_t tline_x;            // 0x5F3A: tline map X offset
    uint8_t tline_y;            // 0x5F3B: tline map Y offset
    int16_t line_x;             // 0x5F3C: line endpoint X
    int16_t line_y;             // 0x5F3E: line endpoint Y
} pico_draw_state_t;

// Hardware State (64 bytes at 0x5F40) - matches fake-08
typedef struct __attribute__((packed)) {
    uint8_t half_rate;          // 0x5F40
    uint8_t reverb;             // 0x5F41 (unused - no reverb)
    uint8_t distort;            // 0x5F42
    uint8_t lowpass;            // 0x5F43
    uint32_t rng[2];            // 0x5F44: RNG state
    uint8_t btn[8];             // 0x5F4C: button states (8 players)
    uint8_t spr_mem_map;        // 0x5F54: sprite sheet mapping
    uint8_t scr_mem_map;        // 0x5F55: screen data mapping
    uint8_t map_mem_map;        // 0x5F56: map memory mapping
    uint8_t map_width;          // 0x5F57
    uint8_t print_attr;         // 0x5F58
    uint8_t print_char_dim;     // 0x5F59
    uint8_t print_tab;          // 0x5F5A
    uint8_t print_offset;       // 0x5F5B
    uint8_t btnp_delay;         // 0x5F5C
    uint8_t btnp_interval;      // 0x5F5D
    uint8_t color_bitmask;      // 0x5F5E: color bitmask (critical for drawing!)
    uint8_t alt_pal_flag;       // 0x5F5F
    uint8_t alt_pal[16];        // 0x5F60
    uint8_t alt_pal_lines[16];  // 0x5F70
} pico_hw_state_t;

// Main PICO RAM Structure (32KB: 0x0000-0x7FFF)
typedef struct __attribute__((packed)) {
    // 0x0000-0x1FFF: Sprite sheet (8KB for 256 sprites, but lower 128 here)
    uint8_t sprites[128 * 64];      // 8KB
    // 0x2000-0x2FFF: Map rows 0-31
    uint8_t map[128 * 32];          // 4KB
    // 0x3000-0x30FF: Sprite flags
    uint8_t spr_flags[256];         // 256B
    // 0x3100-0x31FF: Music patterns
    pico_song_t songs[64];          // 256B
    // 0x3200-0x42FF: Sound effects (64 x 68 bytes)
    pico_sfx_t sfx[64];             // 4352B
    // 0x4300-0x5DFF: General use RAM
    uint8_t general[6912];          // 6912B
    // 0x5E00-0x5EFF: Persistent cart data
    uint8_t persist[256];           // 256B
    // 0x5F00-0x5F3F: Draw state
    pico_draw_state_t ds;           // 64B
    // 0x5F40-0x5F7F: Hardware state
    pico_hw_state_t hw;             // 64B
    // 0x5F80-0x5FFF: GPIO pins
    uint8_t gpio[128];              // 128B
    // 0x6000-0x7FFF: Screen buffer
    uint8_t screen[PICO_FRAMEBUFFER_SIZE];  // 8KB
} pico_ram_t;

// Size check
#ifdef __cplusplus
static_assert(sizeof(pico_ram_t) == PICO_RAM_SIZE, "RAM size mismatch");
#else
_Static_assert(sizeof(pico_ram_t) == PICO_RAM_SIZE, "RAM size mismatch");
#endif

// API
void pico_ram_init(pico_ram_t* ram);
void pico_ram_reset(pico_ram_t* ram);

// Byte access
uint8_t pico_peek(pico_ram_t* ram, uint16_t addr);
void pico_poke(pico_ram_t* ram, uint16_t addr, uint8_t val);
uint16_t pico_peek2(pico_ram_t* ram, uint16_t addr);
void pico_poke2(pico_ram_t* ram, uint16_t addr, uint16_t val);
uint32_t pico_peek4(pico_ram_t* ram, uint16_t addr);
void pico_poke4(pico_ram_t* ram, uint16_t addr, uint32_t val);

// Bulk ops
void pico_memcpy(pico_ram_t* ram, uint16_t dest, uint16_t src, uint16_t len);
void pico_memset(pico_ram_t* ram, uint16_t dest, uint8_t val, uint16_t len);

// Nibble helpers - CORRECT bit order (matches fake-08)
// Low nibble = even X pixel, High nibble = odd X pixel
#define PICO_COMBINED_IDX(x, y) ((y) * 64 + (x) / 2)

static inline uint8_t pico_get_pixel(uint8_t* buf, int x, int y) {
    return (x & 1) ? (buf[PICO_COMBINED_IDX(x, y)] >> 4)
                   : (buf[PICO_COMBINED_IDX(x, y)] & 0x0F);
}

static inline void pico_set_pixel(uint8_t* buf, int x, int y, uint8_t c) {
    int idx = PICO_COMBINED_IDX(x, y);
    if (x & 1) {
        buf[idx] = (buf[idx] & 0x0F) | ((c & 0x0F) << 4);
    } else {
        buf[idx] = (buf[idx] & 0xF0) | (c & 0x0F);
    }
}

#endif // PICO_RAM_H
