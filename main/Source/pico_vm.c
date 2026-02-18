// pico_vm.c
// PICO-8 Virtual Machine implementation

#include "pico_vm.h"
#include "pico_lua_api.h"
#include <string.h>
#include <stdio.h>

#define LUA_CODE_BUFFER_SIZE (32 * 1024)  // 32KB max Lua code

#define PICO_DEBUG 1

#if PICO_DEBUG
#define PICO_LOG(fmt, ...) printf("[PICO] " fmt "\n", ##__VA_ARGS__)
#else
#define PICO_LOG(fmt, ...) ((void)0)
#endif

bool pico_vm_init(pico_vm_t* vm) {
    memset(vm, 0, sizeof(pico_vm_t));

    vm->state = PICO_VM_STOPPED;
    vm->target_fps = PICO_FPS_DEFAULT;

    pico_ram_init(&vm->ram);
    pico_graphics_init(&vm->graphics, &vm->ram);
    pico_audio_init(&vm->audio, &vm->ram);
    pico_input_init(&vm->input, &vm->ram);

    vm->lua_code = malloc(LUA_CODE_BUFFER_SIZE);
    if (!vm->lua_code) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to allocate Lua buffer");
        return false;
    }

    // Initialize Lua state
    if (!pico_lua_init(vm)) {
        free(vm->lua_code);
        vm->lua_code = NULL;
        return false;
    }

    return true;
}

void pico_vm_shutdown(pico_vm_t* vm) {
    pico_vm_stop(vm);

    pico_lua_shutdown(vm);

    if (vm->lua_code) {
        free(vm->lua_code);
        vm->lua_code = NULL;
    }

    pico_audio_shutdown(&vm->audio);
}

void pico_vm_reset(pico_vm_t* vm) {
    // Tear down old Lua state (Byeeeeeee)
    pico_lua_shutdown(vm);

    // Reset hardware
    pico_ram_reset(&vm->ram);
    pico_graphics_reset(&vm->graphics);
    pico_audio_reset(&vm->audio);

    vm->frame_count = 0;
    vm->has_init    = false;
    vm->has_update  = false;
    vm->has_update60 = false;
    vm->has_draw    = false;
    vm->error_msg[0] = '\0';

    // Bring up a fresh Lua state
    if (!pico_lua_init(vm)) {
        // pico_lua_init already wrote the error_msg
        vm->state = PICO_VM_ERROR;
    }
}

static void detect_callbacks(pico_vm_t* vm) {
    vm->has_init     = true;
    vm->has_update   = true;
    vm->has_update60 = true;
    vm->has_draw     = true;
}

bool pico_vm_load_cart(pico_vm_t* vm, const char* path) {
    pico_vm_reset(vm);

    // Check reset didn't fail (e.g. Lua reinit OOM)
    if (vm->state == PICO_VM_ERROR) return false;

    int lua_len = pico_cart_load(path, &vm->ram, vm->lua_code,
                                  LUA_CODE_BUFFER_SIZE, &vm->cart_info);

    if (lua_len < 0) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to load cart: %s", path);
        return false;
    }

    vm->lua_code_len = lua_len;

    pico_cart_load_data(path, &vm->ram);

    // Load Lua code into interpreter
    if (!pico_lua_load(vm, vm->lua_code, vm->lua_code_len)) {
        if (vm->error_msg[0] == '\0') {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to load Lua code");
        }
        return false;
    }

    detect_callbacks(vm);

    // If cart uses _update60, run at 60 FPS; otherwise 30 FPS.
    bool has_update60_src = strstr(vm->lua_code, "_update60") != NULL;
    vm->target_fps = has_update60_src ? PICO_FPS_60 : PICO_FPS_DEFAULT;

    return true;
}

bool pico_vm_load_cart_mem(pico_vm_t* vm, const uint8_t* data, size_t len) {
    pico_vm_reset(vm);

    if (vm->state == PICO_VM_ERROR) return false;

    int lua_len = pico_cart_load_mem(data, len, &vm->ram, vm->lua_code,
                                      LUA_CODE_BUFFER_SIZE, &vm->cart_info);

    if (lua_len < 0) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to load embedded cart");
        return false;
    }

    vm->lua_code_len = lua_len;

    if (!pico_lua_load(vm, vm->lua_code, vm->lua_code_len)) {
        return false;
    }

    detect_callbacks(vm);

    bool has_update60_src = strstr(vm->lua_code, "_update60") != NULL;
    vm->target_fps = has_update60_src ? PICO_FPS_60 : PICO_FPS_DEFAULT;

    return true;
}

void pico_vm_unload_cart(pico_vm_t* vm) {
    pico_vm_stop(vm);
    pico_vm_reset(vm);

    memset(&vm->cart_info, 0, sizeof(vm->cart_info));
    vm->lua_code_len = 0;
}

void pico_vm_run(pico_vm_t* vm) {
    if (vm->state == PICO_VM_RUNNING) return;

    vm->state = PICO_VM_RUNNING;
    vm->frame_count = 0;

    // Call _init() if present
    if (vm->has_init) {
        if (!pico_lua_call_init(vm)) {
            vm->state = PICO_VM_ERROR;
        }
    }
}

void pico_vm_stop(pico_vm_t* vm) {
    vm->state = PICO_VM_STOPPED;
}

void pico_vm_pause(pico_vm_t* vm) {
    if (vm->state == PICO_VM_RUNNING) {
        vm->state = PICO_VM_PAUSED;
    }
}

void pico_vm_resume(pico_vm_t* vm) {
    if (vm->state == PICO_VM_PAUSED) {
        vm->state = PICO_VM_RUNNING;
    }
}

void pico_vm_step(pico_vm_t* vm) {
    if (vm->state != PICO_VM_RUNNING) {
        PICO_LOG("step: not running (state=%d)", vm->state);
        return;
    }

    // Call _update60() preferred over _update()
    if (vm->has_update60) {
        if (!pico_lua_call_update60(vm)) {
            // Only treat as error if the message is non-empty
            if (vm->error_msg[0] != '\0') {
                PICO_LOG("step: update60 error: %s", vm->error_msg);
                vm->state = PICO_VM_ERROR;
                return;
            }
        }
    } else if (vm->has_update) {
        if (!pico_lua_call_update(vm)) {
            if (vm->error_msg[0] != '\0') {
                PICO_LOG("step: update error: %s", vm->error_msg);
                vm->state = PICO_VM_ERROR;
                return;
            }
        }
    }

    // Call _draw()
    if (vm->has_draw) {
        if (!pico_lua_call_draw(vm)) {
            if (vm->error_msg[0] != '\0') {
                PICO_LOG("step: draw error: %s", vm->error_msg);
                vm->state = PICO_VM_ERROR;
                return;
            }
        }
    }

    pico_audio_update(&vm->audio);
    pico_flip(&vm->graphics);

    vm->frame_count++;
}

void pico_vm_main_loop(pico_vm_t* vm) {
    while (vm->state == PICO_VM_RUNNING) {
        pico_vm_step(vm);
    }
}

const char* pico_vm_get_error(pico_vm_t* vm) {
    return vm->error_msg;
}

uint32_t pico_vm_get_fps(pico_vm_t* vm) {
    return vm->target_fps;
}

uint32_t pico_vm_get_frame_count(pico_vm_t* vm) {
    return vm->frame_count;
}
