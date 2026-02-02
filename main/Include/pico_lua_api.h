// pico_lua_api.h
// Bridge between z8lua and Picotility subsystems

#ifndef PICO_LUA_API_H
#define PICO_LUA_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pico_vm.h"

// Initialize Lua state and register PICO-8 API
bool pico_lua_init(pico_vm_t* vm);

// Shutdown Lua state
void pico_lua_shutdown(pico_vm_t* vm);

// Load and execute Lua code
bool pico_lua_load(pico_vm_t* vm, const char* code, size_t len);

// Call PICO-8 callbacks
bool pico_lua_call_init(pico_vm_t* vm);
bool pico_lua_call_update(pico_vm_t* vm);
bool pico_lua_call_update60(pico_vm_t* vm);
bool pico_lua_call_draw(pico_vm_t* vm);

// Get last error message
const char* pico_lua_get_error(pico_vm_t* vm);

#ifdef __cplusplus
}
#endif

#endif // PICO_LUA_API_H
