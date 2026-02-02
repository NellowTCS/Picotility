// pico_vm.h
// PICO-8 Virtual Machine

#ifndef PICO_VM_H
#define PICO_VM_H

#include "pico_config.h"
#include "pico_ram.h"
#include "pico_graphics.h"
#include "pico_audio.h"
#include "pico_input.h"
#include "pico_cart.h"

// VM State
typedef enum {
    PICO_VM_STOPPED,
    PICO_VM_RUNNING,
    PICO_VM_PAUSED,
    PICO_VM_ERROR,
} pico_vm_state_t;

// VM Context
typedef struct {
    pico_vm_state_t state;
    
    // Subsystems
    pico_ram_t ram;
    pico_graphics_t graphics;
    pico_audio_t audio;
    pico_input_t input;
    
    // Current cart
    pico_cart_info_t cart_info;
    char* lua_code;
    size_t lua_code_len;
    
    // Lua state (opaque pointer)
    void* lua_state;
    
    // Callback flags
    bool has_init;
    bool has_update;
    bool has_update60;
    bool has_draw;
    
    // Timing
    uint32_t frame_count;
    uint32_t target_fps;
    uint32_t last_frame_time;
    
    // Error handling
    char error_msg[128];
} pico_vm_t;

// Initialization
bool pico_vm_init(pico_vm_t* vm);
void pico_vm_shutdown(pico_vm_t* vm);
void pico_vm_reset(pico_vm_t* vm);

// Cart Management
bool pico_vm_load_cart(pico_vm_t* vm, const char* path);
bool pico_vm_load_cart_mem(pico_vm_t* vm, const uint8_t* data, size_t len);
void pico_vm_unload_cart(pico_vm_t* vm);

// Execution
void pico_vm_run(pico_vm_t* vm);
void pico_vm_stop(pico_vm_t* vm);
void pico_vm_pause(pico_vm_t* vm);
void pico_vm_resume(pico_vm_t* vm);
void pico_vm_step(pico_vm_t* vm);
void pico_vm_main_loop(pico_vm_t* vm);

// Query
const char* pico_vm_get_error(pico_vm_t* vm);
uint32_t pico_vm_get_fps(pico_vm_t* vm);
uint32_t pico_vm_get_frame_count(pico_vm_t* vm);

#endif // PICO_VM_H
