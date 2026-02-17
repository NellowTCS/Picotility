// pico_config.h
// Picotility configuration and memory budget

#ifndef PICO_CONFIG_H
#define PICO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// PICO-8 Constants
#define PICO_SCREEN_WIDTH   128
#define PICO_SCREEN_HEIGHT  128
#define PICO_FRAMEBUFFER_SIZE (PICO_SCREEN_WIDTH * PICO_SCREEN_HEIGHT / 2)  // 8KB (4-bit)

#define PICO_SPRITE_SIZE    8       // 8x8 pixels per sprite
#define PICO_SPRITES_COUNT  256     // Total sprites
#define PICO_MAP_WIDTH      128
#define PICO_MAP_HEIGHT     64      // Full map (32 rows shared with sprites)

#define PICO_SFX_COUNT      64
#define PICO_MUSIC_COUNT    64
#define PICO_CHANNELS       4

#define PICO_PALETTE_SIZE   16      // 16 colors
#define PICO_FPS_DEFAULT    30
#define PICO_FPS_60         60

// Memory Budget Configuration
// Target: 100KB total, ~80KB used, ~20KB headroom

// Core PICO RAM - reduced from 64KB to 32KB (no userData area)
#define PICO_RAM_SIZE       0x8000  // 32KB

// Lua VM budget
#define PICO_LUA_HEAP_SIZE  (40 * 1024)  // 40KB for Lua

// Audio buffer (no reverb)
#define PICO_AUDIO_BUFFER_SIZE  1024     // 1KB audio buffer

// Display conversion buffer (single line)
#define PICO_LINE_BUFFER_SIZE   (PICO_SCREEN_WIDTH * 2)  // 256 bytes RGB565

// Feature Flags
#define PICO_ENABLE_AUDIO       1   // Audio synthesis
#define PICO_ENABLE_MUSIC       1   // Music playback
#define PICO_ENABLE_REVERB      0   // NO REVERB - saves ~18KB
#define PICO_ENABLE_EXTENDED_MEM 0  // NO userData - saves ~32KB
#define PICO_ENABLE_TOUCH       1   // Virtual gamepad

// Debug features (disable for release)
#define PICO_DEBUG_MEMORY       0   // Memory usage tracking
#define PICO_DEBUG_TIMING       0   // Frame timing stats

// Pre-computed PICO-8 palette in RGB565 format
static const uint16_t PICO_PALETTE_RGB565[16] = {
    0x0000,  //  0: black       #000000
    0x194A,  //  1: dark blue   #1D2B53
    0x792A,  //  2: dark purple #7E2553
    0x042A,  //  3: dark green  #008751
    0xAA86,  //  4: brown       #AB5236
    0x5AA9,  //  5: dark gray   #5F574F
    0xC618,  //  6: light gray  #C2C3C7
    0xFFF5,  //  7: white       #FFF1E8
    0xF809,  //  8: red         #FF004D
    0xFD00,  //  9: orange      #FFA300
    0xFFA0,  // 10: yellow      #FFEC27
    0x0706,  // 11: green       #00E436
    0x2DFE,  // 12: blue        #29ADFF
    0x83B3,  // 13: indigo      #83769C
    0xFBB5,  // 14: pink        #FF77A8
    0xFDEC,  // 15: peach       #FFCCAA
};

// Fixed-Point Math (16.16 format for PICO-8 compatibility)
typedef int32_t fix32_t;

#define FIX32_SHIFT     16
#define FIX32_ONE       (1 << FIX32_SHIFT)
#define FIX32_HALF      (1 << (FIX32_SHIFT - 1))

#define INT_TO_FIX32(x) ((fix32_t)((x) * FIX32_ONE))
#define FIX32_TO_INT(x) ((int32_t)((x) >> FIX32_SHIFT))
#define FLOAT_TO_FIX32(x) ((fix32_t)((x) * FIX32_ONE))
#define FIX32_TO_FLOAT(x) ((float)(x) / FIX32_ONE)

#define FIX32_MUL(a, b) ((fix32_t)(((int64_t)(a) * (b)) >> FIX32_SHIFT))
#define FIX32_DIV(a, b) ((fix32_t)(((int64_t)(a) << FIX32_SHIFT) / (b)))

#endif // PICO_CONFIG_H
