// wasm_shim.c
// Thin WASM export layer for Picotility
// Wraps the pure C emulator APIs for JavaScript consumption

#include "pico_vm.h"
#include "pico_cart.h"
#include "pico_png_cart.h"
#include "pico_lua_api.h"
#include <emscripten/emscripten.h>
#include <string.h>
#include <stdio.h>

#define LUA_CODE_BUFFER_SIZE (32 * 1024)

static pico_vm_t vm;
static char lua_code[LUA_CODE_BUFFER_SIZE];

// EM_JS: runs inside Emscripten closure, has access to HEAPU8
EM_JS(void, js_init_heap, (), {
    globalThis.__picoHeapU8 = HEAPU8;
});

EMSCRIPTEN_KEEPALIVE
int wasm_init(void) {
    js_init_heap();
    if (!pico_vm_init(&vm)) {
        return -1;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_shutdown(void) {
    pico_vm_shutdown(&vm);
}

// Cart loading
// Accepts raw .p8 or .p8.png bytes; auto-detects format via magic header.

EMSCRIPTEN_KEEPALIVE
int wasm_load_cart(const uint8_t* data, int len) {
    if (!data || len <= 0) return -1;

    pico_vm_reset(&vm);

    int lua_len = pico_cart_load_mem(data, (size_t)len, &vm.ram,
                                     lua_code, LUA_CODE_BUFFER_SIZE,
                                     &vm.cart_info);
    if (lua_len < 0) {
        snprintf(vm.error_msg, sizeof(vm.error_msg), "Failed to load cart");
        return -1;
    }

    vm.lua_code_len = (size_t)lua_len;

    if (!pico_lua_load(&vm, lua_code, vm.lua_code_len)) {
        if (vm.error_msg[0] == '\0') {
            snprintf(vm.error_msg, sizeof(vm.error_msg), "Failed to load Lua code");
        }
        return -1;
    }

    // Detect callbacks
    vm.has_init    = strstr(lua_code, "function _init") != NULL;
    vm.has_update  = strstr(lua_code, "function _update()") != NULL ||
                     strstr(lua_code, "function _update (") != NULL;
    vm.has_update60 = strstr(lua_code, "function _update60") != NULL;
    vm.has_draw    = strstr(lua_code, "function _draw") != NULL;

    vm.target_fps = vm.has_update60 ? 60 : 30;

    return 0;
}

// Execution

EMSCRIPTEN_KEEPALIVE
void wasm_run(void) {
    pico_vm_run(&vm);
}

EMSCRIPTEN_KEEPALIVE
void wasm_step(void) {
    pico_input_update(&vm.input);

    if (vm.state != PICO_VM_RUNNING) return;

    pico_vm_step(&vm);
}

EMSCRIPTEN_KEEPALIVE
void wasm_stop(void) {
    pico_vm_stop(&vm);
}

// Input

EMSCRIPTEN_KEEPALIVE
void wasm_set_button(int button, int pressed) {
    pico_input_set_button(&vm.input, PICO_PLAYER_0,
                          (uint8_t)button, (bool)pressed);
}

// Framebuffer

EMSCRIPTEN_KEEPALIVE
uint8_t* wasm_get_framebuffer(void) {
    return vm.ram.screen;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_dirty_top(void) {
    return vm.graphics.dirty_top;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_dirty_bottom(void) {
    return vm.graphics.dirty_bottom;
}

EMSCRIPTEN_KEEPALIVE
int wasm_needs_flip(void) {
    return vm.graphics.needs_flip ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_flip(void) {
    pico_flip(&vm.graphics);
}

// Audio

EMSCRIPTEN_KEEPALIVE
int wasm_audio_fill(int16_t* out_buf, int max_samples) {
    uint16_t n = (uint16_t)max_samples;
    if (n > PICO_AUDIO_BUFFER_SIZE) n = PICO_AUDIO_BUFFER_SIZE;
    pico_audio_fill(&vm.audio, out_buf, n);
    return (int)n;
}

// Query

EMSCRIPTEN_KEEPALIVE
const char* wasm_get_error(void) {
    return vm.error_msg;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_fps(void) {
    return (int)vm.target_fps;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_frame_count(void) {
    return (int)vm.frame_count;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_state(void) {
    return (int)vm.state;
}
