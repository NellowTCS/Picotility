// pico_cart.h
// PICO-8 cartridge loading

#ifndef PICO_CART_H
#define PICO_CART_H

#include <stddef.h>
#include "pico_config.h"
#include "pico_ram.h"

// Cart Info
#define PICO_CART_TITLE_LEN 64
#define PICO_CART_AUTHOR_LEN 32

typedef struct {
    char title[PICO_CART_TITLE_LEN];
    char author[PICO_CART_AUTHOR_LEN];
    uint8_t version;
    bool valid;
} pico_cart_info_t;

// Cart Loading
// Returns Lua code length, or -1 on error
int pico_cart_load(const char* path, pico_ram_t* ram,
                   char* lua_code, size_t lua_code_size,
                   pico_cart_info_t* info);

int pico_cart_load_mem(const uint8_t* data, size_t len, pico_ram_t* ram,
                       char* lua_code, size_t lua_code_size,
                       pico_cart_info_t* info);

// Cart Data Section Parsing
void pico_cart_parse_gfx(const char* data, size_t len, pico_ram_t* ram);
void pico_cart_parse_gff(const char* data, size_t len, pico_ram_t* ram);
void pico_cart_parse_map(const char* data, size_t len, pico_ram_t* ram);
void pico_cart_parse_sfx(const char* data, size_t len, pico_ram_t* ram);
void pico_cart_parse_music(const char* data, size_t len, pico_ram_t* ram);

// Persistence
bool pico_cart_save_data(const char* cart_path, pico_ram_t* ram);
bool pico_cart_load_data(const char* cart_path, pico_ram_t* ram);

#endif // PICO_CART_H
