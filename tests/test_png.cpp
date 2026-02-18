#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>

extern "C" {
#include "pico_vm.h"
#include "pico_ram.h"
#include "pico_cart.h"
#include "pico_png_cart.h"
}

static char base_path[PATH_MAX];

static void resolve_cart_path(const char* rel_path, char* out_path, size_t out_size) {
    snprintf(out_path, out_size, "%s/%s", base_path, rel_path);
}

static char full_cart_path[PATH_MAX];

const char* get_cart_path(const char* rel_path) {
    resolve_cart_path(rel_path, full_cart_path, sizeof(full_cart_path));
    return full_cart_path;
}

typedef struct {
    const char* name;
    const char* lua_path;
    const char* png_path;
} CartCompare;

static CartCompare all_carts[] = {
    {"celeste", "carts/celeste.lua", "carts/Celeste.p8.png"},
    {"hund3d", "carts/hund3d.lua", "carts/hund3d.p8.png"},
};

static char* read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* data = (char*)malloc(size + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);
    
    if (out_size) *out_size = size;
    return data;
}

static void ltrim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (s != strstr(s, s)) {
        char* d = s;
        while (*s) *d++ = *s++;
        *d = '\0';
    }
}

static void rtrim(char* s) {
    int len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ' || s[len-1] == '\t')) {
        s[--len] = '\0';
    }
}

static void normalize_line(char* line) {
    ltrim(line);
    rtrim(line);
}

static int compare_files(const char* lua_path, const char* png_path, const char* name) {
    printf("=== Comparing: %s ===\n", name);
    printf("Source: %s\n", lua_path);
    printf("PNG:    %s\n", png_path);
    printf("\n");
    
    // Read the .lua source
    size_t lua_size;
    char* lua_code = read_file(lua_path, &lua_size);
    if (!lua_code) {
        printf("ERROR: Could not read %s\n", lua_path);
        return -1;
    }
    
    // Read PNG file
    size_t png_size;
    char* png_data = read_file(png_path, &png_size);
    if (!png_data) {
        printf("ERROR: Could not read %s\n", png_path);
        free(lua_code);
        return -1;
    }
    
    // Allocate buffers for RAM and decompressed code
    pico_ram_t ram;
    char lua_decompressed[32768];
    pico_cart_info_t info;
    
    // Load PNG cart directly
    int result = pico_png_cart_load_mem(
        (const uint8_t*)png_data, png_size,
        &ram, lua_decompressed, sizeof(lua_decompressed), &info
    );
    
    free(png_data);
    
    if (result < 0) {
        printf("ERROR: Failed to load PNG cart (result=%d)\n", result);
        free(lua_code);
        return -1;
    }
    
    printf("Decompressed code length: %d bytes\n", result);
    
    // Count non-empty lines in source
    int src_lines = 0;
    char* lua_copy = strdup(lua_code);
    char* tok = strtok(lua_copy, "\n");
    while (tok) {
        normalize_line(tok);
        if (strlen(tok) > 0) src_lines++;
        tok = strtok(NULL, "\n");
    }
    free(lua_copy);
    
    // Count non-empty lines in decompressed
    int png_lines = 0;
    char* decomp_copy = strdup(lua_decompressed);
    tok = strtok(decomp_copy, "\n");
    while (tok) {
        normalize_line(tok);
        if (strlen(tok) > 0) png_lines++;
        tok = strtok(NULL, "\n");
    }
    free(decomp_copy);
    
    printf("Source lines: %d, PNG lines: %d\n", src_lines, png_lines);
    
    // Simple line comparison
    printf("\n=== Line comparison ===\n");
    
    char* lua_lines[10000];
    int lua_line_count = 0;
    
    lua_copy = strdup(lua_code);
    tok = strtok(lua_copy, "\n");
    while (tok && lua_line_count < 10000) {
        lua_lines[lua_line_count++] = strdup(tok);
        tok = strtok(NULL, "\n");
    }
    
    char* decomp_lines[10000];
    int decomp_line_count = 0;
    
    decomp_copy = strdup(lua_decompressed);
    tok = strtok(decomp_copy, "\n");
    while (tok && decomp_line_count < 10000) {
        decomp_lines[decomp_line_count++] = strdup(tok);
        tok = strtok(NULL, "\n");
    }
    
    int max_lines = lua_line_count > decomp_line_count ? lua_line_count : decomp_line_count;
    int diffs = 0;
    
    for (int i = 0; i < max_lines; i++) {
        char lua_norm[1024] = {0};
        char decomp_norm[1024] = {0};
        
        if (i < lua_line_count) {
            strncpy(lua_norm, lua_lines[i], sizeof(lua_norm)-1);
            normalize_line(lua_norm);
        }
        if (i < decomp_line_count) {
            strncpy(decomp_norm, decomp_lines[i], sizeof(decomp_norm)-1);
            normalize_line(decomp_norm);
        }
        
        if (strcmp(lua_norm, decomp_norm) != 0) {
            if (diffs < 5) {
                printf("Line %d:\n", i+1);
                printf("  SRC: %.80s\n", i < lua_line_count ? lua_lines[i] : "(missing)");
                printf("  PNG: %.80s\n", i < decomp_line_count ? decomp_lines[i] : "(missing)");
            }
            diffs++;
        }
    }
    
    if (diffs > 0) {
        printf("\nTotal differences: %d (whitespace only may not be real differences)\n", diffs);
    } else {
        printf("\n✓ Perfect match!\n");
    }
    
    // Cleanup
    for (int i = 0; i < lua_line_count; i++) free(lua_lines[i]);
    for (int i = 0; i < decomp_line_count; i++) free(decomp_lines[i]);
    free(lua_copy);
    free(decomp_copy);
    free(lua_code);
    
    return diffs > 0 ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argv[0]) {
        char exe_path[PATH_MAX];
        char exe_dir[PATH_MAX];
        realpath(argv[0], exe_path);
        strcpy(exe_dir, dirname(exe_path));
        char* tests_dir = dirname(exe_dir);
        if (strcmp(exe_dir, "build") == 0) {
            strcpy(base_path, dirname(tests_dir));
        } else {
            strcpy(base_path, dirname(exe_dir));
        }
    } else {
        getcwd(base_path, sizeof(base_path));
    }
    
    printf("Base path: %s\n", base_path);
    
    int num_carts = sizeof(all_carts) / sizeof(all_carts[0]);
    printf("Testing %d carts\n\n", num_carts);
    
    int passed = 0;
    int failed = 0;
    
    for (int i = 0; i < num_carts; i++) {
        CartCompare* cart = &all_carts[i];
        
        char lua_full[PATH_MAX];
        char png_full[PATH_MAX];
        resolve_cart_path(cart->lua_path, lua_full, sizeof(lua_full));
        resolve_cart_path(cart->png_path, png_full, sizeof(png_full));
        
        int result = compare_files(lua_full, png_full, cart->name);
        
        if (result == 0) {
            passed++;
        } else {
            failed++;
        }
        printf("\n");
    }
    
    printf("=== Summary ===\n");
    printf("Passed: %d, Failed: %d\n", passed, failed);
    
    return failed > 0 ? 1 : 0;
}
