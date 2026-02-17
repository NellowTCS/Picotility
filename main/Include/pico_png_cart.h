// pico_png_cart.h
// PNG-format PICO-8 cartridge loader
// Handles steganographic data extraction from 160x205 RGBA PNGs

#ifndef PICO_PNG_CART_H
#define PICO_PNG_CART_H

#include <stddef.h>
#include <stdint.h>
#include "pico_ram.h"
#include "pico_cart.h"

// Load a PNG-format PICO-8 cartridge from memory.
// Returns the length of extracted Lua code on success, -1 on failure.
int pico_png_cart_load_mem(const uint8_t* data, size_t len,
                           pico_ram_t* ram, char* lua_code,
                           size_t lua_code_size, pico_cart_info_t* info);

#endif // PICO_PNG_CART_H
