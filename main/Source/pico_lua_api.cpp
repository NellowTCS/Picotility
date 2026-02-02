// pico_lua_api.cpp
// Bridge between z8lua and Picotility subsystems

extern "C" {
#include "pico_lua_api.h"
#include "pico_graphics.h"
#include "pico_audio.h"
#include "pico_input.h"
}

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <cstring>

// Global VM pointer for API functions
static pico_vm_t* g_vm = nullptr;

// Helper to get subsystems
#define GFX (&g_vm->graphics)
#define RAM (&g_vm->ram)
#define AUD (&g_vm->audio)
#define INP (&g_vm->input)

// Graphics API
static int l_cls(lua_State* L) {
    uint8_t col = luaL_optinteger(L, 1, 0);
    pico_cls(GFX, col);
    return 0;
}

static int l_pset(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    uint8_t c = luaL_optinteger(L, 3, RAM->ds.color);
    pico_pset(GFX, x, y, c);
    return 0;
}

static int l_pget(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    lua_pushinteger(L, pico_pget(GFX, x, y));
    return 1;
}

static int l_line(lua_State* L) {
    int16_t x0 = lua_tointeger(L, 1);
    int16_t y0 = lua_tointeger(L, 2);
    int16_t x1, y1;
    uint8_t c;
    
    if (lua_gettop(L) >= 4) {
        x1 = lua_tointeger(L, 3);
        y1 = lua_tointeger(L, 4);
        c = luaL_optinteger(L, 5, RAM->ds.color);
    } else {
        // Line from last position
        x1 = x0;
        y1 = y0;
        x0 = RAM->ds.line_x;
        y0 = RAM->ds.line_y;
        c = luaL_optinteger(L, 3, RAM->ds.color);
    }
    
    pico_line(GFX, x0, y0, x1, y1, c);
    return 0;
}

static int l_rect(lua_State* L) {
    int16_t x0 = lua_tointeger(L, 1);
    int16_t y0 = lua_tointeger(L, 2);
    int16_t x1 = lua_tointeger(L, 3);
    int16_t y1 = lua_tointeger(L, 4);
    uint8_t c = luaL_optinteger(L, 5, RAM->ds.color);
    pico_rect(GFX, x0, y0, x1, y1, c);
    return 0;
}

static int l_rectfill(lua_State* L) {
    int16_t x0 = lua_tointeger(L, 1);
    int16_t y0 = lua_tointeger(L, 2);
    int16_t x1 = lua_tointeger(L, 3);
    int16_t y1 = lua_tointeger(L, 4);
    uint8_t c = luaL_optinteger(L, 5, RAM->ds.color);
    pico_rectfill(GFX, x0, y0, x1, y1, c);
    return 0;
}

static int l_circ(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    int16_t r = luaL_optinteger(L, 3, 4);
    uint8_t c = luaL_optinteger(L, 4, RAM->ds.color);
    pico_circ(GFX, x, y, r, c);
    return 0;
}

static int l_circfill(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    int16_t r = luaL_optinteger(L, 3, 4);
    uint8_t c = luaL_optinteger(L, 4, RAM->ds.color);
    pico_circfill(GFX, x, y, r, c);
    return 0;
}

static int l_oval(lua_State* L) {
    int16_t x0 = lua_tointeger(L, 1);
    int16_t y0 = lua_tointeger(L, 2);
    int16_t x1 = lua_tointeger(L, 3);
    int16_t y1 = lua_tointeger(L, 4);
    uint8_t c = luaL_optinteger(L, 5, RAM->ds.color);
    pico_oval(GFX, x0, y0, x1, y1, c);
    return 0;
}

static int l_ovalfill(lua_State* L) {
    int16_t x0 = lua_tointeger(L, 1);
    int16_t y0 = lua_tointeger(L, 2);
    int16_t x1 = lua_tointeger(L, 3);
    int16_t y1 = lua_tointeger(L, 4);
    uint8_t c = luaL_optinteger(L, 5, RAM->ds.color);
    pico_ovalfill(GFX, x0, y0, x1, y1, c);
    return 0;
}

static int l_spr(lua_State* L) {
    int16_t n = lua_tointeger(L, 1);
    int16_t x = lua_tointeger(L, 2);
    int16_t y = lua_tointeger(L, 3);
    float w = luaL_optnumber(L, 4, 1.0);
    float h = luaL_optnumber(L, 5, 1.0);
    bool flip_x = lua_toboolean(L, 6);
    bool flip_y = lua_toboolean(L, 7);
    pico_spr(GFX, n, x, y, w, h, flip_x, flip_y);
    return 0;
}

static int l_sspr(lua_State* L) {
    int16_t sx = lua_tointeger(L, 1);
    int16_t sy = lua_tointeger(L, 2);
    int16_t sw = lua_tointeger(L, 3);
    int16_t sh = lua_tointeger(L, 4);
    int16_t dx = lua_tointeger(L, 5);
    int16_t dy = lua_tointeger(L, 6);
    int16_t dw = luaL_optinteger(L, 7, sw);
    int16_t dh = luaL_optinteger(L, 8, sh);
    bool flip_x = lua_toboolean(L, 9);
    bool flip_y = lua_toboolean(L, 10);
    pico_sspr(GFX, sx, sy, sw, sh, dx, dy, dw, dh, flip_x, flip_y);
    return 0;
}

static int l_map(lua_State* L) {
    int16_t cell_x = luaL_optinteger(L, 1, 0);
    int16_t cell_y = luaL_optinteger(L, 2, 0);
    int16_t sx = luaL_optinteger(L, 3, 0);
    int16_t sy = luaL_optinteger(L, 4, 0);
    int16_t cell_w = luaL_optinteger(L, 5, 128);
    int16_t cell_h = luaL_optinteger(L, 6, 64);
    uint8_t layer = luaL_optinteger(L, 7, 0);
    pico_map(GFX, cell_x, cell_y, sx, sy, cell_w, cell_h, layer);
    return 0;
}

static int l_mget(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    lua_pushinteger(L, pico_mget(GFX, x, y));
    return 1;
}

static int l_mset(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    uint8_t v = lua_tointeger(L, 3);
    pico_mset(GFX, x, y, v);
    return 0;
}

static int l_fget(lua_State* L) {
    int16_t n = lua_tointeger(L, 1);
    if (lua_gettop(L) >= 2) {
        uint8_t f = lua_tointeger(L, 2);
        lua_pushboolean(L, pico_fget(GFX, n, f));
    } else {
        lua_pushinteger(L, pico_fget(GFX, n, 0xFF));
    }
    return 1;
}

static int l_fset(lua_State* L) {
    int16_t n = lua_tointeger(L, 1);
    uint8_t f = lua_tointeger(L, 2);
    bool v = lua_gettop(L) >= 3 ? lua_toboolean(L, 3) : true;
    pico_fset(GFX, n, f, v);
    return 0;
}

static int l_sget(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    lua_pushinteger(L, pico_sget(GFX, x, y));
    return 1;
}

static int l_sset(lua_State* L) {
    int16_t x = lua_tointeger(L, 1);
    int16_t y = lua_tointeger(L, 2);
    uint8_t c = luaL_optinteger(L, 3, RAM->ds.color);
    pico_sset(GFX, x, y, c);
    return 0;
}

static int l_camera(lua_State* L) {
    int16_t x = luaL_optinteger(L, 1, 0);
    int16_t y = luaL_optinteger(L, 2, 0);
    pico_camera(GFX, x, y);
    return 0;
}

static int l_clip(lua_State* L) {
    if (lua_gettop(L) == 0) {
        pico_clip(GFX, 0, 0, 128, 128);
    } else {
        int16_t x = lua_tointeger(L, 1);
        int16_t y = lua_tointeger(L, 2);
        int16_t w = lua_tointeger(L, 3);
        int16_t h = lua_tointeger(L, 4);
        pico_clip(GFX, x, y, w, h);
    }
    return 0;
}

static int l_color(lua_State* L) {
    uint8_t c = luaL_optinteger(L, 1, 6);
    pico_color(GFX, c);
    return 0;
}

static int l_pal(lua_State* L) {
    if (lua_gettop(L) == 0) {
        pico_pal_reset(GFX);
    } else {
        uint8_t c0 = lua_tointeger(L, 1);
        uint8_t c1 = luaL_optinteger(L, 2, c0);
        uint8_t p = luaL_optinteger(L, 3, 0);
        pico_pal(GFX, c0, c1, p);
    }
    return 0;
}

static int l_palt(lua_State* L) {
    uint8_t c = lua_tointeger(L, 1);
    bool t = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) : true;
    pico_palt(GFX, c, t);
    return 0;
}

static int l_fillp(lua_State* L) {
    uint16_t p = luaL_optinteger(L, 1, 0);
    pico_fillp(GFX, p);
    return 0;
}

static int l_print(lua_State* L) {
    const char* str = lua_tostring(L, 1);
    if (!str) return 0;
    
    int16_t x, y;
    uint8_t c;
    
    if (lua_gettop(L) >= 3) {
        x = lua_tointeger(L, 2);
        y = lua_tointeger(L, 3);
        c = luaL_optinteger(L, 4, RAM->ds.color);
    } else {
        x = RAM->ds.text_x;
        y = RAM->ds.text_y;
        c = luaL_optinteger(L, 2, RAM->ds.color);
    }
    
    pico_print(GFX, str, x, y, c);
    return 0;
}

// Input API

static int l_btn(lua_State* L) {
    if (lua_gettop(L) == 0) {
        // Return bitfield of all buttons
        lua_pushinteger(L, INP->btn_state[0]);
    } else {
        uint8_t i = lua_tointeger(L, 1);
        uint8_t p = luaL_optinteger(L, 2, 0);
        lua_pushboolean(L, pico_btn(INP, 1 << i, p));
    }
    return 1;
}

static int l_btnp(lua_State* L) {
    if (lua_gettop(L) == 0) {
        // Return bitfield of just-pressed buttons
        lua_pushinteger(L, INP->btn_state[0] & ~INP->btn_prev[0]);
    } else {
        uint8_t i = lua_tointeger(L, 1);
        uint8_t p = luaL_optinteger(L, 2, 0);
        lua_pushboolean(L, pico_btnp(INP, 1 << i, p));
    }
    return 1;
}

// Audio API

static int l_sfx(lua_State* L) {
    int8_t n = lua_tointeger(L, 1);
    int8_t channel = luaL_optinteger(L, 2, -1);
    uint8_t offset = luaL_optinteger(L, 3, 0);
    uint8_t length = luaL_optinteger(L, 4, 32);
    pico_sfx(AUD, n, channel, offset, length);
    return 0;
}

static int l_music(lua_State* L) {
    int8_t n = lua_tointeger(L, 1);
    uint16_t fade_ms = luaL_optinteger(L, 2, 0);
    uint8_t mask = luaL_optinteger(L, 3, 0);
    pico_music(AUD, n, fade_ms, mask);
    return 0;
}

// Memory API

static int l_peek(lua_State* L) {
    uint16_t addr = lua_tointeger(L, 1);
    lua_pushinteger(L, pico_peek(RAM, addr));
    return 1;
}

static int l_poke(lua_State* L) {
    uint16_t addr = lua_tointeger(L, 1);
    uint8_t val = lua_tointeger(L, 2);
    pico_poke(RAM, addr, val);
    return 0;
}

static int l_peek2(lua_State* L) {
    uint16_t addr = lua_tointeger(L, 1);
    lua_pushinteger(L, pico_peek2(RAM, addr));
    return 1;
}

static int l_poke2(lua_State* L) {
    uint16_t addr = lua_tointeger(L, 1);
    uint16_t val = lua_tointeger(L, 2);
    pico_poke2(RAM, addr, val);
    return 0;
}

static int l_peek4(lua_State* L) {
    uint16_t addr = lua_tointeger(L, 1);
    lua_pushnumber(L, (int32_t)pico_peek4(RAM, addr));
    return 1;
}

static int l_poke4(lua_State* L) {
    uint16_t addr = lua_tointeger(L, 1);
    uint32_t val = lua_tonumber(L, 2);
    pico_poke4(RAM, addr, val);
    return 0;
}

static int l_memcpy(lua_State* L) {
    uint16_t dest = lua_tointeger(L, 1);
    uint16_t src = lua_tointeger(L, 2);
    uint16_t len = lua_tointeger(L, 3);
    
    if (dest + len <= PICO_RAM_SIZE && src + len <= PICO_RAM_SIZE) {
        memmove(((uint8_t*)RAM) + dest, ((uint8_t*)RAM) + src, len);
    }
    return 0;
}

static int l_memset(lua_State* L) {
    uint16_t dest = lua_tointeger(L, 1);
    uint8_t val = lua_tointeger(L, 2);
    uint16_t len = lua_tointeger(L, 3);
    
    if (dest + len <= PICO_RAM_SIZE) {
        memset(((uint8_t*)RAM) + dest, val, len);
    }
    return 0;
}

// System API

static int l_time(lua_State* L) {
    lua_pushnumber(L, g_vm->frame_count / 30.0);
    return 1;
}

static int l_stat(lua_State* L) {
    int n = lua_tointeger(L, 1);
    switch (n) {
        case 0: lua_pushinteger(L, 0); break;  // Memory usage (KB)
        case 1: lua_pushinteger(L, 100); break;  // CPU usage (%)
        case 7: lua_pushinteger(L, g_vm->target_fps); break;  // FPS
        default: lua_pushinteger(L, 0); break;
    }
    return 1;
}

// Registration

static const luaL_Reg pico_api[] = {
    // Graphics
    {"cls", l_cls},
    {"pset", l_pset},
    {"pget", l_pget},
    {"line", l_line},
    {"rect", l_rect},
    {"rectfill", l_rectfill},
    {"circ", l_circ},
    {"circfill", l_circfill},
    {"oval", l_oval},
    {"ovalfill", l_ovalfill},
    {"spr", l_spr},
    {"sspr", l_sspr},
    {"map", l_map},
    {"mapdraw", l_map},
    {"mget", l_mget},
    {"mset", l_mset},
    {"fget", l_fget},
    {"fset", l_fset},
    {"sget", l_sget},
    {"sset", l_sset},
    {"camera", l_camera},
    {"clip", l_clip},
    {"color", l_color},
    {"pal", l_pal},
    {"palt", l_palt},
    {"fillp", l_fillp},
    {"print", l_print},
    
    // Input
    {"btn", l_btn},
    {"btnp", l_btnp},
    
    // Audio
    {"sfx", l_sfx},
    {"music", l_music},
    
    // Memory
    {"peek", l_peek},
    {"poke", l_poke},
    {"peek2", l_peek2},
    {"poke2", l_poke2},
    {"peek4", l_peek4},
    {"poke4", l_poke4},
    {"memcpy", l_memcpy},
    {"memset", l_memset},
    
    // System
    {"time", l_time},
    {"t", l_time},
    {"stat", l_stat},
    
    {NULL, NULL}
};

static void register_api(lua_State* L) {
    lua_pushglobaltable(L);
    luaL_setfuncs(L, pico_api, 0);
    lua_pop(L, 1);
}

// Public Interface

extern "C" {

bool pico_lua_init(pico_vm_t* vm) {
    g_vm = vm;
    
    lua_State* L = luaL_newstate();
    if (!L) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to create Lua state");
        return false;
    }
    
    // Provide memory to z8lua for @ % $ operators
    lua_setpico8memory(L, (const unsigned char*)&vm->ram);
    
    // Open standard libraries (base, pico8, table, string, coroutine)
    luaL_openlibs(L);
    
    // Register our API functions
    register_api(L);
    
    vm->lua_state = L;
    return true;
}

void pico_lua_shutdown(pico_vm_t* vm) {
    if (vm->lua_state) {
        lua_close((lua_State*)vm->lua_state);
        vm->lua_state = nullptr;
    }
    g_vm = nullptr;
}

bool pico_lua_load(pico_vm_t* vm, const char* code, size_t len) {
    lua_State* L = (lua_State*)vm->lua_state;
    if (!L) return false;
    
    int err = luaL_loadbuffer(L, code, len, "cart");
    if (err != LUA_OK) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Load error: %s", 
                 lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    
    err = lua_pcall(L, 0, 0, 0);
    if (err != LUA_OK) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Run error: %s", 
                 lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    
    return true;
}

static bool call_function(lua_State* L, const char* name, char* error_buf, size_t buf_size) {
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return true;  // Not an error, function just doesn't exist
    }
    
    int err = lua_pcall(L, 0, 0, 0);
    if (err != LUA_OK) {
        snprintf(error_buf, buf_size, "%s error: %s", name, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool pico_lua_call_init(pico_vm_t* vm) {
    return call_function((lua_State*)vm->lua_state, "_init", 
                         vm->error_msg, sizeof(vm->error_msg));
}

bool pico_lua_call_update(pico_vm_t* vm) {
    return call_function((lua_State*)vm->lua_state, "_update", 
                         vm->error_msg, sizeof(vm->error_msg));
}

bool pico_lua_call_update60(pico_vm_t* vm) {
    return call_function((lua_State*)vm->lua_state, "_update60", 
                         vm->error_msg, sizeof(vm->error_msg));
}

bool pico_lua_call_draw(pico_vm_t* vm) {
    return call_function((lua_State*)vm->lua_state, "_draw", 
                         vm->error_msg, sizeof(vm->error_msg));
}

const char* pico_lua_get_error(pico_vm_t* vm) {
    return vm->error_msg;
}

} // extern "C"
