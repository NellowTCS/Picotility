// pico_ram.c
// PICO-8 memory implementation

#include "pico_ram.h"
#include <string.h>

void pico_ram_init(pico_ram_t* ram) {
    memset(ram, 0, sizeof(pico_ram_t));
    pico_ram_reset(ram);
}

void pico_ram_reset(pico_ram_t* ram) {
    // Clear everything up to (but not including) persistent cart data
    memset(ram, 0, 0x5E00);
    // Clear draw state through screen (0x5F00-0x7FFF), skip persist (0x5E00-0x5EFF)
    memset(&ram->ds, 0, 0x8000 - 0x5F00);

    // Default palettes: identity mapping, color 0 transparent (PICO-8 default)
    for (int i = 0; i < 16; i++) {
        ram->ds.draw_pal[i] = i | (i == 0 ? 0x10 : 0);  // color 0 has transparency bit set
        ram->ds.screen_pal[i] = i;
    }

    // Default clip rect (full screen, exclusive end)
    ram->ds.clip_xb = 0;
    ram->ds.clip_yb = 0;
    ram->ds.clip_xe = 128;
    ram->ds.clip_ye = 128;

    ram->ds.color = 6;  // Light gray default
    ram->ds.text_x = 0;
    ram->ds.text_y = 0;
    ram->ds.camera_x = 0;
    ram->ds.camera_y = 0;
    ram->ds.fillp[0] = 0;
    ram->ds.fillp[1] = 0;
    ram->ds.fillp_trans = 0;
    ram->ds.line_invalid = 1;

    // Hardware defaults (match fake-08)
    ram->hw.color_bitmask = 0xFF;
    ram->hw.spr_mem_map = 0x00;
    ram->hw.scr_mem_map = 0x60;
    ram->hw.map_mem_map = 0x20;
    ram->hw.map_width = 128;
    ram->hw.btnp_delay = 15;
    ram->hw.btnp_interval = 4;
}

uint8_t pico_peek(pico_ram_t* ram, uint16_t addr) {
    if (addr >= PICO_RAM_SIZE) {
        return 0;
    }
    return ((uint8_t*)ram)[addr];
}

void pico_poke(pico_ram_t* ram, uint16_t addr, uint8_t val) {
    if (addr >= PICO_RAM_SIZE) {
        return;
    }
    ((uint8_t*)ram)[addr] = val;
}

uint16_t pico_peek2(pico_ram_t* ram, uint16_t addr) {
    if (addr + 1 >= PICO_RAM_SIZE) {
        return 0;
    }
    uint8_t* p = ((uint8_t*)ram) + addr;
    return p[0] | (p[1] << 8);
}

void pico_poke2(pico_ram_t* ram, uint16_t addr, uint16_t val) {
    if (addr + 1 >= PICO_RAM_SIZE) {
        return;
    }
    uint8_t* p = ((uint8_t*)ram) + addr;
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
}

uint32_t pico_peek4(pico_ram_t* ram, uint16_t addr) {
    if (addr + 3 >= PICO_RAM_SIZE) {
        return 0;
    }
    uint8_t* p = ((uint8_t*)ram) + addr;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void pico_poke4(pico_ram_t* ram, uint16_t addr, uint32_t val) {
    if (addr + 3 >= PICO_RAM_SIZE) {
        return;
    }
    uint8_t* p = ((uint8_t*)ram) + addr;
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}

void pico_memcpy(pico_ram_t* ram, uint16_t dest, uint16_t src, uint16_t len) {
    if (dest >= PICO_RAM_SIZE || src >= PICO_RAM_SIZE) {
        return;
    }
    if (dest + len > PICO_RAM_SIZE) {
        len = PICO_RAM_SIZE - dest;
    }
    if (src + len > PICO_RAM_SIZE) {
        len = PICO_RAM_SIZE - src;
    }
    memmove(((uint8_t*)ram) + dest, ((uint8_t*)ram) + src, len);
}

void pico_memset(pico_ram_t* ram, uint16_t dest, uint8_t val, uint16_t len) {
    if (dest >= PICO_RAM_SIZE) {
        return;
    }
    if (dest + len > PICO_RAM_SIZE) {
        len = PICO_RAM_SIZE - dest;
    }
    memset(((uint8_t*)ram) + dest, val, len);
}
