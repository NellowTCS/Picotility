// pico_graphics.c
// PICO-8 graphics - matches fake-08 drawing logic

#include "pico_graphics.h"
#include "fontdata.h"
#include <string.h>
#include <stdlib.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) MIN(MAX(x, lo), hi)

// Apply camera offset
static inline void apply_camera(pico_ram_t* ram, int* x, int* y) {
    *x -= ram->ds.camera_x;
    *y -= ram->ds.camera_y;
}

// Check if within clip region (exclusive end bounds like fake-08)
static inline bool in_clip(pico_ram_t* ram, int x, int y) {
    return x >= ram->ds.clip_xb && x < ram->ds.clip_xe &&
           y >= ram->ds.clip_yb && y < ram->ds.clip_ye;
}

// Check if color is transparent (bit 4 of draw palette)
static inline bool is_transparent(pico_ram_t* ram, uint8_t c) {
    return (ram->ds.draw_pal[c & 0x0F] >> 4) != 0;
}

// Get draw-palette mapped color
static inline uint8_t get_pal_color(pico_ram_t* ram, uint8_t c) {
    return ram->ds.draw_pal[c & 0x0F] & 0x0F;
}

// Set pixel from sprite (no fill pattern)
static void set_pixel_sprite(pico_ram_t* ram, int x, int y, uint8_t col) {
    x &= 127;
    y &= 127;

    col = get_pal_color(ram, col);

    // Apply color bitmask if not 0xFF
    if (ram->hw.color_bitmask != 0xFF) {
        uint8_t write_mask = ram->hw.color_bitmask & 0x0F;
        uint8_t read_mask = ram->hw.color_bitmask >> 4;
        uint8_t src = pico_get_pixel(ram->screen, x, y);
        col = (src & ~write_mask) | (col & write_mask & read_mask);
    }

    pico_set_pixel(ram->screen, x, y, col);
}

// Set pixel from pen (with fill pattern support)
static void set_pixel_pen(pico_ram_t* ram, int x, int y) {
    x &= 127;
    y &= 127;

    uint8_t col = ram->ds.color;
    uint8_t col0 = col & 0x0F;
    uint8_t col1 = (col >> 4) & 0x0F;
    uint8_t final_c = col0;

    // Apply fill pattern
    uint8_t bit_pos = 15 - ((x & 3) + 4 * (y & 3));
    uint16_t fillp = ((uint16_t)ram->ds.fillp[1] << 8) | ram->ds.fillp[0];
    bool alt_color = (fillp >> bit_pos) & 1;

    if (alt_color) {
        if (ram->ds.fillp_trans & 1) {
            return;  // Transparent in fill pattern
        }
        final_c = col1;
    }

    final_c = get_pal_color(ram, final_c);

    // Apply color bitmask
    if (ram->hw.color_bitmask != 0xFF) {
        uint8_t write_mask = ram->hw.color_bitmask & 0x0F;
        uint8_t read_mask = ram->hw.color_bitmask >> 4;
        uint8_t src = pico_get_pixel(ram->screen, x, y);
        final_c = (src & ~write_mask) | (final_c & write_mask & read_mask);
    }

    pico_set_pixel(ram->screen, x, y, final_c);
}

// Safe set pixel (checks clip)
static void safe_set_pixel_pen(pico_ram_t* ram, int x, int y) {
    if (in_clip(ram, x, y)) {
        set_pixel_pen(ram, x, y);
    }
}

// Optimized horizontal line
static void h_line(pico_ram_t* ram, int x1, int x2, int y) {
    if (y < ram->ds.clip_yb || y >= ram->ds.clip_ye) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }

    int minx = CLAMP(x1, ram->ds.clip_xb, ram->ds.clip_xe - 1);
    int maxx = CLAMP(x2, ram->ds.clip_xb, ram->ds.clip_xe - 1);

    // Fast path: no fill pattern, no color bitmask
    bool fast = ram->hw.color_bitmask == 0xFF &&
                ram->ds.fillp[0] == 0 &&
                ram->ds.fillp[1] == 0 &&
                maxx - minx > 1;

    if (fast) {
        uint8_t* p = ram->screen + y * 64;
        uint8_t color = get_pal_color(ram, ram->ds.color);

        if (minx & 1) {
            p[minx / 2] = (p[minx / 2] & 0x0F) | (color << 4);
            minx++;
        }
        if ((maxx & 1) == 0) {
            p[maxx / 2] = (p[maxx / 2] & 0xF0) | color;
            maxx--;
        }
        if (minx <= maxx) {
            memset(p + minx / 2, color * 0x11, (maxx - minx + 1) / 2);
        }
    } else {
        for (int x = minx; x <= maxx; x++) {
            set_pixel_pen(ram, x, y);
        }
    }
}

// Optimized vertical line
static void v_line(pico_ram_t* ram, int x, int y1, int y2) {
    if (x < ram->ds.clip_xb || x >= ram->ds.clip_xe) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    int miny = CLAMP(y1, ram->ds.clip_yb, ram->ds.clip_ye - 1);
    int maxy = CLAMP(y2, ram->ds.clip_yb, ram->ds.clip_ye - 1);

    bool fast = ram->hw.color_bitmask == 0xFF &&
                ram->ds.fillp[0] == 0 &&
                ram->ds.fillp[1] == 0;

    if (fast) {
        uint8_t color = get_pal_color(ram, ram->ds.color);
        uint8_t mask = (x & 1) ? 0x0F : 0xF0;
        uint8_t nibble = (x & 1) ? (color << 4) : color;

        for (int y = miny; y <= maxy; y++) {
            int idx = PICO_COMBINED_IDX(x, y);
            ram->screen[idx] = (ram->screen[idx] & mask) | nibble;
        }
    } else {
        for (int y = miny; y <= maxy; y++) {
            set_pixel_pen(ram, x, y);
        }
    }
}

// Public API

void pico_graphics_init(pico_graphics_t* gfx, pico_ram_t* ram) {
    memset(gfx, 0, sizeof(pico_graphics_t));
    gfx->ram = ram;
    gfx->font_data = defaultFontBinaryData;
    pico_graphics_reset(gfx);
}

void pico_graphics_reset(pico_graphics_t* gfx) {
    gfx->dirty_top = 0;
    gfx->dirty_bottom = 127;
    gfx->needs_flip = true;
}

void pico_cls(pico_graphics_t* gfx, uint8_t color) {
    pico_ram_t* ram = gfx->ram;
    color &= 0x0F;
    memset(ram->screen, color | (color << 4), PICO_FRAMEBUFFER_SIZE);

    ram->ds.text_x = 0;
    ram->ds.text_y = 0;

    ram->ds.clip_xb = 0;
    ram->ds.clip_yb = 0;
    ram->ds.clip_xe = 128;
    ram->ds.clip_ye = 128;

    gfx->dirty_top = 0;
    gfx->dirty_bottom = 127;
    gfx->needs_flip = true;
}

void pico_flip(pico_graphics_t* gfx) {
    gfx->needs_flip = false;
    gfx->dirty_top = 128;
    gfx->dirty_bottom = 0;
}

void pico_pset(pico_graphics_t* gfx, int16_t x, int16_t y, uint8_t col) {
    pico_ram_t* ram = gfx->ram;
    pico_color(gfx, col);

    int ix = x, iy = y;
    apply_camera(ram, &ix, &iy);

    if (in_clip(ram, ix, iy)) {
        set_pixel_pen(ram, ix, iy);
        if (iy < gfx->dirty_top) gfx->dirty_top = iy;
        if (iy > gfx->dirty_bottom) gfx->dirty_bottom = iy;
        gfx->needs_flip = true;
    }
}

uint8_t pico_pget(pico_graphics_t* gfx, int16_t x, int16_t y) {
    pico_ram_t* ram = gfx->ram;
    int ix = x, iy = y;
    apply_camera(ram, &ix, &iy);

    if (ix >= 0 && ix < 128 && iy >= 0 && iy < 128) {
        return pico_get_pixel(ram->screen, ix, iy);
    }
    return 0;
}

void pico_line(pico_graphics_t* gfx, int16_t x0, int16_t y0,
               int16_t x1, int16_t y1, uint8_t col) {
    pico_ram_t* ram = gfx->ram;

    ram->ds.line_x = x1;
    ram->ds.line_y = y1;
    ram->ds.line_invalid = 0;

    int ix0 = x0, iy0 = y0, ix1 = x1, iy1 = y1;
    apply_camera(ram, &ix0, &iy0);
    apply_camera(ram, &ix1, &iy1);

    pico_color(gfx, col);

    // Vertical line optimization
    if (ix0 == ix1) {
        v_line(ram, ix0, iy0, iy1);
    }
    // Horizontal line optimization
    else if (iy0 == iy1) {
        h_line(ram, ix0, ix1, iy0);
    }
    // Bresenham for diagonals
    else {
        int dx = abs(ix1 - ix0), sx = ix0 < ix1 ? 1 : -1;
        int dy = -abs(iy1 - iy0), sy = iy0 < iy1 ? 1 : -1;
        int err = dx + dy;

        for (;;) {
            safe_set_pixel_pen(ram, ix0, iy0);
            if (ix0 == ix1 && iy0 == iy1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; ix0 += sx; }
            if (e2 <= dx) { err += dx; iy0 += sy; }
        }
    }

    gfx->needs_flip = true;
}

void pico_rect(pico_graphics_t* gfx, int16_t x0, int16_t y0,
               int16_t x1, int16_t y1, uint8_t col) {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }

    pico_line(gfx, x0, y0, x1, y0, col);
    pico_line(gfx, x0, y1, x1, y1, col);
    pico_line(gfx, x0, y0, x0, y1, col);
    pico_line(gfx, x1, y0, x1, y1, col);
}

void pico_rectfill(pico_graphics_t* gfx, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, uint8_t col) {
    pico_ram_t* ram = gfx->ram;

    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }

    int ix0 = x0, iy0 = y0, ix1 = x1, iy1 = y1;
    apply_camera(ram, &ix0, &iy0);
    apply_camera(ram, &ix1, &iy1);

    pico_color(gfx, col);

    for (int y = iy0; y <= iy1; y++) {
        h_line(ram, ix0, ix1, y);
    }

    gfx->needs_flip = true;
}

void pico_circ(pico_graphics_t* gfx, int16_t ox, int16_t oy,
               int16_t r, uint8_t col) {
    pico_ram_t* ram = gfx->ram;
    pico_color(gfx, col);

    int ix = ox, iy = oy;
    apply_camera(ram, &ix, &iy);

    int x = r;
    int y = 0;
    int dec = 1 - x;

    while (y <= x) {
        safe_set_pixel_pen(ram, ix + x, iy + y);
        safe_set_pixel_pen(ram, ix + y, iy + x);
        safe_set_pixel_pen(ram, ix - x, iy + y);
        safe_set_pixel_pen(ram, ix - y, iy + x);
        safe_set_pixel_pen(ram, ix - x, iy - y);
        safe_set_pixel_pen(ram, ix - y, iy - x);
        safe_set_pixel_pen(ram, ix + x, iy - y);
        safe_set_pixel_pen(ram, ix + y, iy - x);

        y++;
        if (dec < 0) {
            dec += 2 * y + 1;
        } else {
            x--;
            dec += 2 * (y - x) + 1;
        }
    }

    gfx->needs_flip = true;
}

void pico_circfill(pico_graphics_t* gfx, int16_t ox, int16_t oy,
                   int16_t r, uint8_t col) {
    pico_ram_t* ram = gfx->ram;
    pico_color(gfx, col);

    int ix = ox, iy = oy;
    apply_camera(ram, &ix, &iy);

    if (r == 0) {
        safe_set_pixel_pen(ram, ix, iy);
    } else if (r == 1) {
        safe_set_pixel_pen(ram, ix, iy - 1);
        h_line(ram, ix - 1, ix + 1, iy);
        safe_set_pixel_pen(ram, ix, iy + 1);
    } else if (r > 0) {
        int x = -r, y = 0, err = 2 - 2 * r;
        do {
            h_line(ram, ix - x, ix + x, iy + y);
            h_line(ram, ix - x, ix + x, iy - y);
            int sr = err;
            if (sr > x) err += ++x * 2 + 1;
            if (sr <= y) err += ++y * 2 + 1;
        } while (x < 0);
    }

    gfx->needs_flip = true;
}

void pico_oval(pico_graphics_t* gfx, int16_t x0, int16_t y0,
               int16_t x1, int16_t y1, uint8_t col) {
    pico_ram_t* ram = gfx->ram;
    pico_color(gfx, col);

    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }

    int a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;
    long dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a;
    long err = dx + dy + b1 * a * a;

    int ix0 = x0, iy0 = y0, ix1 = x1;
    apply_camera(ram, &ix0, &iy0);
    ix1 = ix0 + a;

    iy0 += (b + 1) / 2;
    int iy1 = iy0 - b1;
    a *= 8 * a; b1 = 8 * b * b;

    do {
        safe_set_pixel_pen(ram, ix1, iy0);
        safe_set_pixel_pen(ram, ix0, iy0);
        safe_set_pixel_pen(ram, ix0, iy1);
        safe_set_pixel_pen(ram, ix1, iy1);
        long e2 = 2 * err;
        if (e2 >= dx) { ix0++; ix1--; err += dx += b1; }
        if (e2 <= dy) { iy0++; iy1--; err += dy += a; }
    } while (ix0 <= ix1);

    while (iy0 - iy1 < b) {
        safe_set_pixel_pen(ram, ix0 - 1, iy0);
        safe_set_pixel_pen(ram, ix1 + 1, iy0++);
        safe_set_pixel_pen(ram, ix0 - 1, iy1);
        safe_set_pixel_pen(ram, ix1 + 1, iy1--);
    }

    gfx->needs_flip = true;
}

void pico_ovalfill(pico_graphics_t* gfx, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, uint8_t col) {
    pico_ram_t* ram = gfx->ram;
    pico_color(gfx, col);

    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }

    int a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;
    long dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a;
    long err = dx + dy + b1 * a * a;

    int ix0 = x0, iy0 = y0, ix1 = x1;
    apply_camera(ram, &ix0, &iy0);
    ix1 = ix0 + a;

    iy0 += (b + 1) / 2;
    int iy1 = iy0 - b1;
    a *= 8 * a; b1 = 8 * b * b;

    do {
        h_line(ram, ix0, ix1, iy0);
        h_line(ram, ix0, ix1, iy1);
        long e2 = 2 * err;
        if (e2 >= dx) { ix0++; ix1--; err += dx += b1; }
        if (e2 <= dy) { iy0++; iy1--; err += dy += a; }
    } while (ix0 <= ix1);

    while (iy0 - iy1 <= b) {
        h_line(ram, ix0 - 1, ix1 + 1, iy0++);
        h_line(ram, ix0 - 1, ix1 + 1, iy1--);
    }

    gfx->needs_flip = true;
}

void pico_spr(pico_graphics_t* gfx, int16_t n, int16_t x, int16_t y,
              float w, float h, bool flip_x, bool flip_y) {
    if (n < 0 || n >= 256) return;

    int sw = (int)(w * 8);
    int sh = (int)(h * 8);
    int sx = (n % 16) * 8;
    int sy = (n / 16) * 8;

    pico_sspr(gfx, sx, sy, sw, sh, x, y, sw, sh, flip_x, flip_y);
}

void pico_sspr(pico_graphics_t* gfx,
               int16_t sx, int16_t sy, int16_t sw, int16_t sh,
               int16_t dx, int16_t dy, int16_t dw, int16_t dh,
               bool flip_x, bool flip_y) {
    pico_ram_t* ram = gfx->ram;

    int idx = dx, idy = dy;
    apply_camera(ram, &idx, &idy);

    // Handle negative dest dimensions
    if (dw < 0) { flip_x = !flip_x; dw = -dw; idx -= dw; }
    if (dh < 0) { flip_y = !flip_y; dh = -dh; idy -= dh; }

    if (dw == 0 || dh == 0) return;

    // Source coordinates in 16.16 fixed point
    int spr_x = sx << 16;
    int spr_y = sy << 16;
    int spr_w = sw << 16;
    int spr_h = sh << 16;

    int ddx = spr_w / dw;
    int ddy = spr_h / dh;

    // Clipping
    if (idx < ram->ds.clip_xb) {
        int nclip = ram->ds.clip_xb - idx;
        idx = ram->ds.clip_xb;
        dw -= nclip;
        if (!flip_x) spr_x += nclip * ddx;
        else spr_w -= nclip * ddx;
    }
    if (idx + dw > ram->ds.clip_xe) {
        dw = ram->ds.clip_xe - idx;
    }
    if (idy < ram->ds.clip_yb) {
        int nclip = ram->ds.clip_yb - idy;
        idy = ram->ds.clip_yb;
        dh -= nclip;
        if (!flip_y) spr_y += nclip * ddy;
        else spr_h -= nclip * ddy;
    }
    if (idy + dh > ram->ds.clip_ye) {
        dh = ram->ds.clip_ye - idy;
    }

    if (dw <= 0 || dh <= 0) return;

    // Draw
    for (int py = 0; py < dh; py++) {
        int spy = flip_y ? ((spr_y + spr_h - ddy - py * ddy) >> 16)
                         : ((spr_y + py * ddy) >> 16);
        if (spy < 0 || spy > 127) continue;

        for (int px = 0; px < dw; px++) {
            int spx = flip_x ? ((spr_x + spr_w - ddx - px * ddx) >> 16)
                             : ((spr_x + px * ddx) >> 16);
            if (spx < 0 || spx > 127) continue;

            uint8_t col = pico_get_pixel(ram->sprites, spx, spy);

            if (!is_transparent(ram, col)) {
                set_pixel_sprite(ram, idx + px, idy + py, col);
            }
        }
    }

    gfx->needs_flip = true;
}

void pico_map(pico_graphics_t* gfx,
              int16_t cell_x, int16_t cell_y,
              int16_t sx, int16_t sy,
              int16_t cell_w, int16_t cell_h,
              uint8_t layer) {
    for (int cy = 0; cy < cell_h; cy++) {
        for (int cx = 0; cx < cell_w; cx++) {
            int mx = cell_x + cx;
            int my = cell_y + cy;

            if (mx < 0 || mx >= 128 || my < 0 || my >= 64) continue;

            uint8_t tile = pico_mget(gfx, mx, my);
            if (tile == 0) continue;

            if (layer != 0) {
                uint8_t flags = pico_fget(gfx, tile, 0xFF);
                if ((flags & layer) == 0) continue;
            }

            pico_spr(gfx, tile, sx + cx * 8, sy + cy * 8, 1, 1, false, false);
        }
    }
}

uint8_t pico_mget(pico_graphics_t* gfx, int16_t x, int16_t y) {
    pico_ram_t* ram = gfx->ram;
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return 0;

    if (y < 32) {
        return ram->map[y * 128 + x];
    } else {
        // Shared area with sprite sheet upper half (0x1000-0x1FFF)
        // ram->sprites is uint8_t[128*64] = 8192 bytes; offset 0x1000 is valid.
        return ram->sprites[0x1000 + (y - 32) * 128 + x];
    }
}

void pico_mset(pico_graphics_t* gfx, int16_t x, int16_t y, uint8_t val) {
    pico_ram_t* ram = gfx->ram;
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;

    if (y < 32) {
        ram->map[y * 128 + x] = val;
    } else {
        ram->sprites[0x1000 + (y - 32) * 128 + x] = val;
    }
}

void pico_print(pico_graphics_t* gfx, const char* str,
                int16_t x, int16_t y, uint8_t col) {
    // Cursor fallback is already handled in the Lua layer (l_print); by the time
    // we arrive here we *should* have explicit coordinates. (if not we have bigger problems)
    pico_ram_t* ram = gfx->ram;

    const uint8_t* font = gfx->font_data;
    int char_w = font ? font[0] : 4;
    int char_h = font ? font[2] : 5;
    int16_t start_x = x;

    while (*str) {
        char c = *str++;
        if (c == '\n') {
            x = start_x;
            y += char_h + 1;
            continue;
        }
        pico_print_char(gfx, c, x, y, col);
        x += ((uint8_t)c >= 128 && font) ? font[1] : char_w;
    }

    ram->ds.text_x = x;
    ram->ds.text_y = y;
}

void pico_print_char(pico_graphics_t* gfx, char c,
                     int16_t x, int16_t y, uint8_t col) {
    const uint8_t* font = gfx->font_data;
    if (!font) return;

    uint8_t ch = (uint8_t)c;
    if (ch < 16) return;

    int width = font[0];     /* 4 for standard font */
    int height = font[2];    /* 5 for standard font */
    int offset_x = font[3];
    int offset_y = font[4];

    /* Glyph data: 8 bytes per char starting from char 16, at offset 128 */
    const uint8_t* glyph = &font[128 + (ch - 16) * 8];

    /* For chars >= 128, use the wider width */
    if (ch >= 128) {
        width = font[1];  /* 8 for extended chars */
    }

    for (int row = 0; row < height; row++) {
        uint8_t bits = glyph[row];
        for (int px = 0; px < width; px++) {
            if (bits & (1 << px)) {
                pico_pset(gfx, x + offset_x + px, y + offset_y + row, col);
            }
        }
    }
}

void pico_camera(pico_graphics_t* gfx, int16_t x, int16_t y) {
    gfx->ram->ds.camera_x = x;
    gfx->ram->ds.camera_y = y;
}

void pico_clip(pico_graphics_t* gfx, int16_t x, int16_t y,
               int16_t w, int16_t h) {
    pico_ram_t* ram = gfx->ram;

    if (w < 0) {
        ram->ds.clip_xb = 0;
        ram->ds.clip_yb = 0;
        ram->ds.clip_xe = 128;
        ram->ds.clip_ye = 128;
    } else {
        ram->ds.clip_xb = CLAMP(x, 0, 128);
        ram->ds.clip_yb = CLAMP(y, 0, 128);
        ram->ds.clip_xe = CLAMP(x + w, 0, 128);
        ram->ds.clip_ye = CLAMP(y + h, 0, 128);
    }
}

void pico_color(pico_graphics_t* gfx, uint8_t col) {
    gfx->ram->ds.color = col;
}

void pico_pal(pico_graphics_t* gfx, uint8_t c0, uint8_t c1, uint8_t p) {
    pico_ram_t* ram = gfx->ram;
    c0 &= 0x0F;
    c1 &= 0x0F;
    if (p == 0) {
        // Preserve transparency flag (bit 4), replace only the color bits (0-3)
        ram->ds.draw_pal[c0] = (ram->ds.draw_pal[c0] & 0x10) | c1;
    } else {
        ram->ds.screen_pal[c0] = c1;
    }
}

void pico_pal_reset(pico_graphics_t* gfx) {
    pico_ram_t* ram = gfx->ram;
    for (int i = 0; i < 16; i++) {
        ram->ds.draw_pal[i] = i;
        ram->ds.screen_pal[i] = i;
    }
    // PICO-8 default: color 0 is transparent
    ram->ds.draw_pal[0] |= 0x10;
}

void pico_palt(pico_graphics_t* gfx, uint8_t col, bool transparent) {
    pico_ram_t* ram = gfx->ram;
    col &= 0x0F;
    if (transparent) {
        ram->ds.draw_pal[col] |= 0x10;
    } else {
        ram->ds.draw_pal[col] &= 0x0F;
    }
}

void pico_fillp(pico_graphics_t* gfx, uint16_t pattern) {
    pico_ram_t* ram = gfx->ram;
    ram->ds.fillp[0] = pattern & 0xFF;
    ram->ds.fillp[1] = (pattern >> 8) & 0xFF;
}

uint8_t pico_fget(pico_graphics_t* gfx, int16_t n, uint8_t f) {
    pico_ram_t* ram = gfx->ram;
    if (n < 0 || n >= 256) return 0;

    uint8_t flags = ram->spr_flags[n];
    if (f == 0xFF) return flags;
    return (flags >> f) & 1;
}

void pico_fset(pico_graphics_t* gfx, int16_t n, uint8_t f, bool val) {
    pico_ram_t* ram = gfx->ram;
    if (n < 0 || n >= 256) return;

    if (f == 0xFF) {
        ram->spr_flags[n] = val ? 0xFF : 0;
    } else if (val) {
        ram->spr_flags[n] |= (1 << f);
    } else {
        ram->spr_flags[n] &= ~(1 << f);
    }
}

uint8_t pico_sget(pico_graphics_t* gfx, int16_t x, int16_t y) {
    pico_ram_t* ram = gfx->ram;
    if (x < 0 || x >= 128 || y < 0 || y >= 128) return 0;
    return pico_get_pixel(ram->sprites, x, y);
}

void pico_sset(pico_graphics_t* gfx, int16_t x, int16_t y, uint8_t col) {
    pico_ram_t* ram = gfx->ram;
    if (x < 0 || x >= 128 || y < 0 || y >= 128) return;
    pico_set_pixel(ram->sprites, x, y, col & 0x0F);
}
