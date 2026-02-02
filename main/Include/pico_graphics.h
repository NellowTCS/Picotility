// pico_graphics.h
// PICO-8 graphics subsystem

#ifndef PICO_GRAPHICS_H
#define PICO_GRAPHICS_H

#include "pico_config.h"
#include "pico_ram.h"

// Graphics Context
typedef struct {
    pico_ram_t* ram;
    uint16_t line_buffer[PICO_SCREEN_WIDTH];  // Line buffer for RGB565 conversion
    uint8_t dirty_top;
    uint8_t dirty_bottom;
    bool needs_flip;
    const uint8_t* font_data;
} pico_graphics_t;

// Initialization
void pico_graphics_init(pico_graphics_t* gfx, pico_ram_t* ram);
void pico_graphics_reset(pico_graphics_t* gfx);

// Screen Management
void pico_cls(pico_graphics_t* gfx, uint8_t color);
void pico_flip(pico_graphics_t* gfx);

// Drawing Primitives
void pico_pset(pico_graphics_t* gfx, int16_t x, int16_t y, uint8_t color);
uint8_t pico_pget(pico_graphics_t* gfx, int16_t x, int16_t y);
void pico_line(pico_graphics_t* gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void pico_rect(pico_graphics_t* gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void pico_rectfill(pico_graphics_t* gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void pico_circ(pico_graphics_t* gfx, int16_t x, int16_t y, int16_t r, uint8_t color);
void pico_circfill(pico_graphics_t* gfx, int16_t x, int16_t y, int16_t r, uint8_t color);
void pico_oval(pico_graphics_t* gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void pico_ovalfill(pico_graphics_t* gfx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

// Sprite Drawing
void pico_spr(pico_graphics_t* gfx, int16_t n, int16_t x, int16_t y, float w, float h, bool flip_x, bool flip_y);
void pico_sspr(pico_graphics_t* gfx, int16_t sx, int16_t sy, int16_t sw, int16_t sh,
               int16_t dx, int16_t dy, int16_t dw, int16_t dh, bool flip_x, bool flip_y);

// Map Drawing
void pico_map(pico_graphics_t* gfx, int16_t cell_x, int16_t cell_y, int16_t sx, int16_t sy,
              int16_t cell_w, int16_t cell_h, uint8_t layer);
uint8_t pico_mget(pico_graphics_t* gfx, int16_t x, int16_t y);
void pico_mset(pico_graphics_t* gfx, int16_t x, int16_t y, uint8_t val);

// Text Drawing
void pico_print(pico_graphics_t* gfx, const char* str, int16_t x, int16_t y, uint8_t color);
void pico_print_char(pico_graphics_t* gfx, char c, int16_t x, int16_t y, uint8_t color);

// State Management
void pico_camera(pico_graphics_t* gfx, int16_t x, int16_t y);
void pico_clip(pico_graphics_t* gfx, int16_t x, int16_t y, int16_t w, int16_t h);
void pico_color(pico_graphics_t* gfx, uint8_t color);
void pico_pal(pico_graphics_t* gfx, uint8_t c0, uint8_t c1, uint8_t p);
void pico_pal_reset(pico_graphics_t* gfx);
void pico_palt(pico_graphics_t* gfx, uint8_t color, bool transparent);
void pico_fillp(pico_graphics_t* gfx, uint16_t pattern);
uint8_t pico_fget(pico_graphics_t* gfx, int16_t n, uint8_t f);
void pico_fset(pico_graphics_t* gfx, int16_t n, uint8_t f, bool val);
uint8_t pico_sget(pico_graphics_t* gfx, int16_t x, int16_t y);
void pico_sset(pico_graphics_t* gfx, int16_t x, int16_t y, uint8_t color);

#endif // PICO_GRAPHICS_H
