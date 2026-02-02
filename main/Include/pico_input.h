// pico_input.h
// PICO-8 input handling

#ifndef PICO_INPUT_H
#define PICO_INPUT_H

#include "pico_config.h"
#include "pico_ram.h"

// PICO-8 button bits
#define PICO_BTN_LEFT   0x01
#define PICO_BTN_RIGHT  0x02
#define PICO_BTN_UP     0x04
#define PICO_BTN_DOWN   0x08
#define PICO_BTN_O      0x10    // Z/C/N
#define PICO_BTN_X      0x20    // X/V/M
#define PICO_BTN_MENU   0x40    // Pause/Menu

// Player indices
#define PICO_PLAYER_0   0
#define PICO_PLAYER_1   1

// Input Context
typedef struct {
    pico_ram_t* ram;
    uint8_t btn_state[2];       // Current state per player
    uint8_t btn_prev[2];        // Previous frame state
    bool touch_active;
    int16_t touch_x;
    int16_t touch_y;
    struct {
        int16_t x, y, w, h;
        uint8_t button;
    } touch_zones[7];           // D-pad (4) + O + X + Menu
} pico_input_t;

// Initialization
void pico_input_init(pico_input_t* input, pico_ram_t* ram);
void pico_input_setup_touch(pico_input_t* input, int16_t screen_w, int16_t screen_h);

// Update
void pico_input_update(pico_input_t* input);
void pico_input_touch(pico_input_t* input, bool pressed, int16_t x, int16_t y);
void pico_input_set_button(pico_input_t* input, uint8_t player, uint8_t button, bool pressed);

// Query Functions
bool pico_btn(pico_input_t* input, uint8_t button, uint8_t player);
bool pico_btnp(pico_input_t* input, uint8_t button, uint8_t player);

#endif // PICO_INPUT_H
