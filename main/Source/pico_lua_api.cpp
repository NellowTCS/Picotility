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
#include <cstdio>

#define PICO_DEBUG 1
#if PICO_DEBUG
#define PICO_LOG(fmt, ...) printf("[PICO] " fmt "\n", ##__VA_ARGS__)
#else
#define PICO_LOG(fmt, ...) ((void)0)
#endif

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

static int l_cursor(lua_State* L) {
    // cursor(x, [y]) — set cursor position for print
    if (lua_gettop(L) >= 2) {
        RAM->ds.text_x = luaL_checkinteger(L, 1);
        RAM->ds.text_y = luaL_checkinteger(L, 2);
    } else {
        RAM->ds.text_x = luaL_checkinteger(L, 1);
    }
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
    
    if (len <= PICO_RAM_SIZE && dest <= PICO_RAM_SIZE - len && src <= PICO_RAM_SIZE - len) {
        memmove(((uint8_t*)RAM) + dest, ((uint8_t*)RAM) + src, len);
    }
    return 0;
}

static int l_memset(lua_State* L) {
    uint16_t dest = lua_tointeger(L, 1);
    uint8_t val = lua_tointeger(L, 2);
    uint16_t len = lua_tointeger(L, 3);
    
    if (len <= PICO_RAM_SIZE && dest <= PICO_RAM_SIZE - len) {
        memset(((uint8_t*)RAM) + dest, val, len);
    }
    return 0;
}

static int l_chr(lua_State* L) {
    // chr(c1, [c2, ...]) — convert numbers to characters
    int n = lua_gettop(L);
    char buf[256];
    int len = 0;
    for (int i = 1; i <= n && len < (int)sizeof(buf) - 1; i++) {
        int c = luaL_checkinteger(L, i);
        if (c >= 0 && c <= 255) {
            buf[len++] = (char)c;
        }
    }
    buf[len] = '\0';
    lua_pushlstring(L, buf, len);
    return 1;
}

// PICO-8 Table Functions (not provided by z8lua or standard Lua)

static int l_add(lua_State* L) {
    // add(t, v, [i]) — insert v into table t, return v
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = luaL_len(L, 1);
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        int i = lua_tointeger(L, 3);
        if (i < 1) i = 1;
        if (i > n + 1) i = n + 1;
        // Shift elements up
        for (int j = n; j >= i; j--) {
            lua_rawgeti(L, 1, j);
            lua_rawseti(L, 1, j + 1);
        }
        lua_pushvalue(L, 2);
        lua_rawseti(L, 1, i);
    } else {
        lua_pushvalue(L, 2);
        lua_rawseti(L, 1, n + 1);
    }
    lua_pushvalue(L, 2);
    return 1;
}

static int l_del(lua_State* L) {
    // del(t, v) — delete first occurrence of v from t, return v
    if (!lua_istable(L, 1)) return 0;
    int n = luaL_len(L, 1);
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_rawequal(L, -1, 2)) {
            lua_pop(L, 1);
            // Shift elements down
            for (int j = i; j < n; j++) {
                lua_rawgeti(L, 1, j + 1);
                lua_rawseti(L, 1, j);
            }
            lua_pushnil(L);
            lua_rawseti(L, 1, n);
            lua_pushvalue(L, 2);
            return 1;
        }
        lua_pop(L, 1);
    }
    return 0;
}

static int l_deli(lua_State* L) {
    // deli(t, [i]) — delete element at index i (default: last), return it
    if (!lua_istable(L, 1)) return 0;
    int n = luaL_len(L, 1);
    int i = luaL_optinteger(L, 2, n);
    if (i < 1 || i > n) return 0;
    lua_rawgeti(L, 1, i); // return value
    for (int j = i; j < n; j++) {
        lua_rawgeti(L, 1, j + 1);
        lua_rawseti(L, 1, j);
    }
    lua_pushnil(L);
    lua_rawseti(L, 1, n);
    return 1;
}

static int l_count(lua_State* L) {
    // count(t, [v]) — count elements, or occurrences of v
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    int n = luaL_len(L, 1);
    if (lua_gettop(L) < 2) {
        lua_pushinteger(L, n);
        return 1;
    }
    int c = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_rawequal(L, -1, 2)) c++;
        lua_pop(L, 1);
    }
    lua_pushinteger(L, c);
    return 1;
}

static int l_foreach(lua_State* L) {
    // foreach(t, f) — call f(v) for each element in t
    if (lua_isnil(L, 1)) {
        return 0;  // Do nothing if table is nil
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int n = luaL_len(L, 1);
    for (int i = 1; i <= n; i++) {
        lua_pushvalue(L, 2);
        lua_rawgeti(L, 1, i);
        lua_call(L, 1, 0);
    }
    return 0;
}

// all() iterator: returns next element each call
static int all_iterator(lua_State* L) {
    int i = lua_tointeger(L, lua_upvalueindex(2));
    int n = luaL_len(L, lua_upvalueindex(1));
    if (i > n) return 0;
    lua_rawgeti(L, lua_upvalueindex(1), i);
    lua_pushinteger(L, i + 1);
    lua_replace(L, lua_upvalueindex(2));
    return 1;
}

static int l_all(lua_State* L) {
    // all(t): returns iterator function for use in for loops
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1);       // upvalue 1: table
    lua_pushinteger(L, 1);     // upvalue 2: index
    lua_pushcclosure(L, all_iterator, 2);
    return 1;
}

// PICO-8 Utility Functions

static uint32_t rng_state = 1;

static int l_rnd(lua_State* L) {
    // rnd(x) — random number [0, x), or random element from table
    if (lua_istable(L, 1)) {
        int n = luaL_len(L, 1);
        if (n == 0) return 0;
        rng_state = rng_state * 1103515245 + 12345;
        int i = ((rng_state >> 16) % n) + 1;
        lua_rawgeti(L, 1, i);
        return 1;
    }
    double max_val = luaL_optnumber(L, 1, 1.0);
    rng_state = rng_state * 1103515245 + 12345;
    double r = (double)(rng_state >> 16) / 65536.0;
    lua_pushnumber(L, r * max_val);
    return 1;
}

static int l_srand(lua_State* L) {
    rng_state = (uint32_t)lua_tointeger(L, 1);
    if (rng_state == 0) rng_state = 1;
    return 0;
}

static int l_sub(lua_State* L) {
    // sub(s, i, [j]) — PICO-8 substring (1-indexed, inclusive end)
    size_t len;
    const char* s = luaL_checklstring(L, 1, &len);
    int i = luaL_optinteger(L, 2, 1);
    int j = luaL_optinteger(L, 3, (int)len);
    // Convert to 0-indexed
    if (i < 1) i = 1;
    if (j > (int)len) j = (int)len;
    if (i > j) {
        lua_pushliteral(L, "");
        return 1;
    }
    lua_pushlstring(L, s + i - 1, j - i + 1);
    return 1;
}

static int l_printh(lua_State* L) {
    // printh(s) — debug print (goes to ESP log)
    const char* s = lua_tostring(L, 1);
    if (s) {
        printf("[PICO-8] %s\n", s);
    }
    return 0;
}

static int l_split(lua_State* L) {
    // split(s, [sep], [convert]) — split string into table
    // If sep is nil or not provided, split into individual characters
    // If convert is false, don't convert numbers
    if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
        lua_newtable(L);  // Return empty table instead of nil
        return 1;
    }
    
    const char* str = luaL_checkstring(L, 1);
    const char* sep = NULL;
    bool convert = true;
    
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        sep = lua_tostring(L, 2);
    }
    if (lua_gettop(L) >= 3) {
        convert = lua_toboolean(L, 3);
    }
    
    lua_newtable(L);
    
    if (sep == NULL) {
        // Split into individual characters
        int idx = 1;
        while (*str) {
            char c[2] = {*str, '\0'};
            lua_pushlstring(L, c, 1);
            lua_rawseti(L, -2, idx++);
            str++;
        }
    } else {
        // Split by separator
        size_t sep_len = strlen(sep);
        if (sep_len == 0) {
            // Empty separator: split into chars
            int idx = 1;
            while (*str) {
                char c[2] = {*str, '\0'};
                lua_pushlstring(L, c, 1);
                lua_rawseti(L, -2, idx++);
                str++;
            }
        } else {
            int idx = 1;
            const char* start = str;
            const char* p = str;
            while (*p) {
                if (strncmp(p, sep, sep_len) == 0) {
                    size_t len = p - start;
                    if (len > 0 || convert) {
                        if (convert) {
                            // Try to convert to number
                            char* end;
                            double num = strtod(start, &end);
                            if (end == start + len) {
                                lua_pushnumber(L, num);
                            } else {
                                lua_pushlstring(L, start, len);
                            }
                        } else {
                            lua_pushlstring(L, start, len);
                        }
                        lua_rawseti(L, -2, idx++);
                    }
                    start = p + sep_len;
                    p = start;
                } else {
                    p++;
                }
            }
            // Last segment
            size_t len = p - start;
            if (len > 0 || convert) {
                if (convert) {
                    char* end;
                    double num = strtod(start, &end);
                    if (end == start + len) {
                        lua_pushnumber(L, num);
                    } else {
                        lua_pushlstring(L, start, len);
                    }
                } else {
                    lua_pushlstring(L, start, len);
                }
                lua_rawseti(L, -2, idx++);
            }
        }
    }
    return 1;
}

// Persistent Cart Data API

static bool cartdata_enabled = false;

static int l_cartdata(lua_State* L) {
    // cartdata(id) — enable persistent data access
    // We accept the ID but don't actually persist to flash
    (void)luaL_checkstring(L, 1);
    cartdata_enabled = true;
    lua_pushboolean(L, 1);
    return 1;
}

static int l_dget(lua_State* L) {
    // dget(n) — read persistent slot n (0-63) as fixed-point number
    if (!cartdata_enabled) {
        lua_pushnumber(L, 0);
        return 1;
    }
    int n = luaL_checkinteger(L, 1);
    if (n < 0 || n > 63) {
        lua_pushnumber(L, 0);
        return 1;
    }
    // Read 4 bytes from persist area as 16.16 fixed-point (little-endian)
    uint8_t* p = RAM->persist + n * 4;
    int32_t raw = (int32_t)(p[0] | ((uint32_t)p[1] << 8) |
                            ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    lua_pushnumber(L, (double)raw / 65536.0);
    return 1;
}

static int l_dset(lua_State* L) {
    // dset(n, val) — write val to persistent slot n (0-63)
    if (!cartdata_enabled) return 0;
    int n = luaL_checkinteger(L, 1);
    if (n < 0 || n > 63) return 0;
    double val = luaL_checknumber(L, 2);
    int32_t raw = (int32_t)(val * 65536.0);
    uint8_t* p = RAM->persist + n * 4;
    p[0] = (uint8_t)(raw & 0xFF);
    p[1] = (uint8_t)((raw >> 8) & 0xFF);
    p[2] = (uint8_t)((raw >> 16) & 0xFF);
    p[3] = (uint8_t)((raw >> 24) & 0xFF);
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
        case 30: lua_pushboolean(L, false); break;  // Keyboard available (no key pressed)
        case 31: lua_pushnil(L); break;  // Keyboard character
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
    {"cursor", l_cursor},
    
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
    {"chr", l_chr},
    
    // Table functions (PICO-8 specific)
    {"add", l_add},
    {"del", l_del},
    {"deli", l_deli},
    {"count", l_count},
    {"foreach", l_foreach},
    {"all", l_all},

    // Utility
    {"rnd", l_rnd},
    {"srand", l_srand},
    {"sub", l_sub},
    {"split", l_split},
    {"printh", l_printh},

    // Persistent data
    {"cartdata", l_cartdata},
    {"dget", l_dget},
    {"dset", l_dset},

    // System
    {"time", l_time},
    {"t", l_time},
    {"stat", l_stat},

    {NULL, NULL}
};

static void register_api(lua_State* L) {
    lua_pushglobaltable(L);
    luaL_setfuncs(L, pico_api, 0);
    
    // Add unpack (table.unpack)
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "unpack");
    lua_setglobal(L, "unpack");
    
    lua_pop(L, 1);
}

// Public Interface

extern "C" {

bool pico_lua_init(pico_vm_t* vm) {
    PICO_LOG("lua: init");
    g_vm = vm;
    cartdata_enabled = false;

    lua_State* L = luaL_newstate();
    if (!L) {
        PICO_LOG("lua: failed to create state");
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to create Lua state");
        return false;
    }
    
    // Provide memory to z8lua for @ % $ operators
    lua_setpico8memory(L, (const unsigned char*)&vm->ram);
    
    // Open standard libraries (base, pico8, table, string, coroutine)
    luaL_openlibs(L);
    
    // Register our API functions
    register_api(L);

    // Seed RNG
    rng_state = (uint32_t)(uintptr_t)L ^ 0xDEADBEEF;

    vm->lua_state = L;
    PICO_LOG("lua: init done");
    return true;
}

void pico_lua_shutdown(pico_vm_t* vm) {
    PICO_LOG("lua: shutdown");
    if (vm->lua_state) {
        lua_close((lua_State*)vm->lua_state);
        vm->lua_state = nullptr;
    }
    g_vm = nullptr;
}

bool pico_lua_load(pico_vm_t* vm, const char* code, size_t len) {
    PICO_LOG("lua: load %zu bytes", len);
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
    if (!L) {
        snprintf(error_buf, buf_size, "Lua state not initialized");
        return false;
    }
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
