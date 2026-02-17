// pico_cart.c
// PICO-8 cartridge loading

#include "pico_cart.h"
#include "pico_png_cart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static uint8_t hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static uint8_t hex_to_byte(const char* s) {
    return (hex_char_to_nibble(s[0]) << 4) | hex_char_to_nibble(s[1]);
}

typedef enum {
    SECTION_NONE,
    SECTION_LUA,
    SECTION_GFX,
    SECTION_GFF,
    SECTION_MAP,
    SECTION_SFX,
    SECTION_MUSIC,
    SECTION_LABEL,
} cart_section_t;

static cart_section_t identify_section(const char* line) {
    if (strncmp(line, "__lua__", 7) == 0) return SECTION_LUA;
    if (strncmp(line, "__gfx__", 7) == 0) return SECTION_GFX;
    if (strncmp(line, "__gff__", 7) == 0) return SECTION_GFF;
    if (strncmp(line, "__map__", 7) == 0) return SECTION_MAP;
    if (strncmp(line, "__sfx__", 7) == 0) return SECTION_SFX;
    if (strncmp(line, "__music__", 9) == 0) return SECTION_MUSIC;
    if (strncmp(line, "__label__", 9) == 0) return SECTION_LABEL;
    return SECTION_NONE;
}

void pico_cart_parse_gfx(const char* data, size_t len, pico_ram_t* ram) {
    int row = 0;
    const char* p = data;
    const char* end = data + len;
    
    while (p < end && row < 128) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        if (*p == '_' && p + 1 < end && *(p+1) == '_') break;
        
        int col = 0;
        while (p < end && col < 128 && *p != '\n' && *p != '\r') {
            uint8_t nibble = hex_char_to_nibble(*p);
            int idx = row * 64 + col / 2;
            uint8_t* sheet = ram->sprites;
            
            if (col & 1) {
                sheet[idx] = (sheet[idx] & 0x0F) | (nibble << 4);
            } else {
                sheet[idx] = (sheet[idx] & 0xF0) | nibble;
            }
            
            col++;
            p++;
        }
        
        while (p < end && *p != '\n') p++;
        if (p < end && *p == '\n') p++;
        row++;
    }
}

void pico_cart_parse_gff(const char* data, size_t len, pico_ram_t* ram) {
    int sprite = 0;
    const char* p = data;
    const char* end = data + len;
    
    while (p < end && sprite < 256) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (p >= end) break;
        if (*p == '_' && p + 1 < end && *(p+1) == '_') break;
        
        while (p + 1 < end && sprite < 256 && *p != '\n' && *p != '\r') {
            if (isxdigit((unsigned char)*p) && isxdigit((unsigned char)*(p+1))) {
                ram->spr_flags[sprite++] = hex_to_byte(p);
                p += 2;
            } else {
                p++;
            }
        }
        
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }
}

void pico_cart_parse_map(const char* data, size_t len, pico_ram_t* ram) {
    int row = 0;
    const char* p = data;
    const char* end = data + len;
    
    while (p < end && row < 32) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        if (*p == '_' && p + 1 < end && *(p+1) == '_') break;
        
        int col = 0;
        while (p + 1 < end && col < 128 && *p != '\n' && *p != '\r') {
            if (isxdigit((unsigned char)*p) && isxdigit((unsigned char)*(p+1))) {
                ram->map[row * 128 + col] = hex_to_byte(p);
                col++;
                p += 2;
            } else {
                p++;
            }
        }
        
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
        row++;
    }
}

void pico_cart_parse_sfx(const char* data, size_t len, pico_ram_t* ram) {
    int sfx_idx = 0;
    const char* p = data;
    const char* end = data + len;
    
    while (p < end && sfx_idx < 64) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        if (*p == '_' && p + 1 < end && *(p+1) == '_') break;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        
        pico_sfx_t* sfx = &ram->sfx[sfx_idx];
        
        if (p + 8 <= end) {
            sfx->editor_mode = hex_to_byte(p); p += 2;
            sfx->speed = hex_to_byte(p); p += 2;
            sfx->loop_start = hex_to_byte(p); p += 2;
            sfx->loop_end = hex_to_byte(p); p += 2;
        }
        
        for (int n = 0; n < 32 && p + 5 <= end; n++) {
            uint8_t b0 = hex_to_byte(p);
            uint8_t b1 = hex_char_to_nibble(p[2]);
            uint8_t b2 = hex_char_to_nibble(p[3]);
            uint8_t b3 = hex_char_to_nibble(p[4]);
            
            sfx->notes[n].data[0] = b0;
            sfx->notes[n].data[1] = (b1 << 4) | (b2 << 1) | (b3 >> 2);
            
            p += 5;
        }
        
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
        sfx_idx++;
    }
}

void pico_cart_parse_music(const char* data, size_t len, pico_ram_t* ram) {
    int pattern_idx = 0;
    const char* p = data;
    const char* end = data + len;
    
    while (p < end && pattern_idx < 64) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        if (*p == '_' && p + 1 < end && *(p+1) == '_') break;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        
        pico_song_t* song = &ram->songs[pattern_idx];
        
        if (p + 10 <= end) {
            song->data[0] = hex_to_byte(p); p += 2;
            if (*p == ' ') p++;
            song->data[1] = hex_to_byte(p); p += 2;
            song->data[2] = hex_to_byte(p); p += 2;
            song->data[3] = hex_to_byte(p); p += 2;
        }
        
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
        pattern_idx++;
    }
}

int pico_cart_load_mem(const uint8_t* data, size_t len, pico_ram_t* ram,
                       char* lua_code, size_t lua_code_size,
                       pico_cart_info_t* info) {
    if (!data || len == 0 || !ram || !lua_code) return -1;
    
    lua_code[0] = '\0';
    if (info) {
        memset(info, 0, sizeof(pico_cart_info_t));
    }
    
    /* Detect PNG format (0x89 'P' 'N' 'G') and delegate to PNG loader */
    if (len >= 4 && data[0] == 0x89 && data[1] == 'P' &&
        data[2] == 'N' && data[3] == 'G') {
        return pico_png_cart_load_mem(data, len, ram, lua_code,
                                      lua_code_size, info);
    }

    const char* text = (const char*)data;
    if (len < 16) {
        return -1;
    }
    if (strncmp(text, "pico-8 cartridge", 16) != 0) {
        return -1;
    }
    
    const char* p = text;
    const char* end = text + len;
    cart_section_t current_section = SECTION_NONE;
    const char* section_start = NULL;
    size_t lua_len = 0;
    
    while (p < end) {
        const char* line_start = p;
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
        
        cart_section_t new_section = identify_section(line_start);
        
        if (new_section != SECTION_NONE) {
            if (current_section != SECTION_NONE && section_start) {
                size_t section_len = line_start - section_start;
                
                switch (current_section) {
                    case SECTION_LUA:
                        if (section_len < lua_code_size) {
                            memcpy(lua_code, section_start, section_len);
                            lua_code[section_len] = '\0';
                            lua_len = section_len;
                        }
                        break;
                    case SECTION_GFX:
                        pico_cart_parse_gfx(section_start, section_len, ram);
                        break;
                    case SECTION_GFF:
                        pico_cart_parse_gff(section_start, section_len, ram);
                        break;
                    case SECTION_MAP:
                        pico_cart_parse_map(section_start, section_len, ram);
                        break;
                    case SECTION_SFX:
                        pico_cart_parse_sfx(section_start, section_len, ram);
                        break;
                    case SECTION_MUSIC:
                        pico_cart_parse_music(section_start, section_len, ram);
                        break;
                    default:
                        break;
                }
            }
            
            current_section = new_section;
            section_start = p;
        }
    }
    
    if (current_section != SECTION_NONE && section_start) {
        size_t section_len = end - section_start;
        
        switch (current_section) {
            case SECTION_LUA:
                if (section_len < lua_code_size) {
                    memcpy(lua_code, section_start, section_len);
                    lua_code[section_len] = '\0';
                    lua_len = section_len;
                }
                break;
            case SECTION_GFX:
                pico_cart_parse_gfx(section_start, section_len, ram);
                break;
            case SECTION_GFF:
                pico_cart_parse_gff(section_start, section_len, ram);
                break;
            case SECTION_MAP:
                pico_cart_parse_map(section_start, section_len, ram);
                break;
            case SECTION_SFX:
                pico_cart_parse_sfx(section_start, section_len, ram);
                break;
            case SECTION_MUSIC:
                pico_cart_parse_music(section_start, section_len, ram);
                break;
            default:
                break;
        }
    }
    
    if (info) {
        info->valid = true;
    }
    
    return (int)lua_len;
}

int pico_cart_load(const char* path, pico_ram_t* ram,
                   char* lua_code, size_t lua_code_size,
                   pico_cart_info_t* info) {
    if (!path || !ram || !lua_code) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 1024 * 1024) {
        fclose(f);
        return -1;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(data);
        return -1;
    }
    
    int result = pico_cart_load_mem(data, size, ram, lua_code, lua_code_size, info);
    
    free(data);
    return result;
}

bool pico_cart_save_data(const char* cart_path, pico_ram_t* ram) {
    if (!cart_path || !ram) return false;
    char save_path[256];
    snprintf(save_path, sizeof(save_path), "%s.sav", cart_path);
    
    FILE* f = fopen(save_path, "wb");
    if (!f) return false;
    
    size_t written = fwrite(ram->persist, 1, 256, f);
    fclose(f);
    
    return written == 256;
}

bool pico_cart_load_data(const char* cart_path, pico_ram_t* ram) {
    if (!cart_path || !ram) return false;
    char save_path[256];
    int written = snprintf(save_path, sizeof(save_path), "%s.sav", cart_path);
    if (written < 0 || (size_t)written >= sizeof(save_path)) return false;
    
    FILE* f = fopen(save_path, "rb");
    if (!f) return false;
    
    size_t read = fread(ram->persist, 1, 256, f);
    fclose(f);
    
    return read == 256;
}
