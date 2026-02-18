// pico_input.c
// PICO-8 input implementation

#include "pico_input.h"
#include <string.h>
#include <stdio.h>

#define PICO_DEBUG 1
#if PICO_DEBUG
#define PICO_LOG(fmt, ...) printf("[PICO] " fmt "\n", ##__VA_ARGS__)
#else
#define PICO_LOG(fmt, ...) ((void)0)
#endif

void pico_input_init(pico_input_t* input, pico_ram_t* ram) {
    memset(input, 0, sizeof(pico_input_t));
    input->ram = ram;
    PICO_LOG("input: init");
}

void pico_input_setup_touch(pico_input_t* input, 
                            int16_t screen_w, int16_t screen_h) {
    int16_t btn_size = screen_h / 6;
    int16_t margin = btn_size / 4;
    
    // D-pad (left side, bottom area)
    int16_t dpad_x = margin;
    int16_t dpad_y = screen_h - btn_size * 3 - margin;
    
    // Left
    input->touch_zones[0].x = dpad_x;
    input->touch_zones[0].y = dpad_y + btn_size;
    input->touch_zones[0].w = btn_size;
    input->touch_zones[0].h = btn_size;
    input->touch_zones[0].button = PICO_BTN_LEFT;
    
    // Right
    input->touch_zones[1].x = dpad_x + btn_size * 2;
    input->touch_zones[1].y = dpad_y + btn_size;
    input->touch_zones[1].w = btn_size;
    input->touch_zones[1].h = btn_size;
    input->touch_zones[1].button = PICO_BTN_RIGHT;
    
    // Up
    input->touch_zones[2].x = dpad_x + btn_size;
    input->touch_zones[2].y = dpad_y;
    input->touch_zones[2].w = btn_size;
    input->touch_zones[2].h = btn_size;
    input->touch_zones[2].button = PICO_BTN_UP;
    
    // Down
    input->touch_zones[3].x = dpad_x + btn_size;
    input->touch_zones[3].y = dpad_y + btn_size * 2;
    input->touch_zones[3].w = btn_size;
    input->touch_zones[3].h = btn_size;
    input->touch_zones[3].button = PICO_BTN_DOWN;
    
    // Action buttons (right side)
    int16_t action_x = screen_w - btn_size * 2 - margin;
    int16_t action_y = screen_h - btn_size * 2 - margin;
    
    // O button
    input->touch_zones[4].x = action_x;
    input->touch_zones[4].y = action_y + btn_size / 2;
    input->touch_zones[4].w = btn_size;
    input->touch_zones[4].h = btn_size;
    input->touch_zones[4].button = PICO_BTN_O;
    
    // X button
    input->touch_zones[5].x = action_x + btn_size;
    input->touch_zones[5].y = action_y;
    input->touch_zones[5].w = btn_size;
    input->touch_zones[5].h = btn_size;
    input->touch_zones[5].button = PICO_BTN_X;
    
    // Menu button (top center)
    input->touch_zones[6].x = (screen_w - btn_size) / 2;
    input->touch_zones[6].y = margin;
    input->touch_zones[6].w = btn_size;
    input->touch_zones[6].h = btn_size / 2;
    input->touch_zones[6].button = PICO_BTN_MENU;
}

void pico_input_update(pico_input_t* input) {
    input->btn_prev[0] = input->btn_state[0];
    input->btn_prev[1] = input->btn_state[1];
    
    // Sync with hardware state
    for (int i = 0; i < 2; i++) {
        input->ram->hw.btn[i] = input->btn_state[i];
    }
}

void pico_input_touch(pico_input_t* input, bool pressed, 
                      int16_t x, int16_t y) {
    input->touch_active = pressed;
    input->touch_x = x;
    input->touch_y = y;
    
    input->btn_state[0] &= ~(PICO_BTN_LEFT | PICO_BTN_RIGHT | 
                              PICO_BTN_UP | PICO_BTN_DOWN |
                              PICO_BTN_O | PICO_BTN_X | PICO_BTN_MENU);
    
    if (!pressed) return;
    
    for (int i = 0; i < 7; i++) {
        if (x >= input->touch_zones[i].x &&
            x < input->touch_zones[i].x + input->touch_zones[i].w &&
            y >= input->touch_zones[i].y &&
            y < input->touch_zones[i].y + input->touch_zones[i].h) {
            input->btn_state[0] |= input->touch_zones[i].button;
        }
    }
}

void pico_input_set_button(pico_input_t* input, uint8_t player,
                           uint8_t button, bool pressed) {
    if (player > 1) return;
    
    if (pressed) {
        input->btn_state[player] |= button;
    } else {
        input->btn_state[player] &= ~button;
    }
}

bool pico_btn(pico_input_t* input, uint8_t button, uint8_t player) {
    if (player > 1) return false;
    return (input->btn_state[player] & button) != 0;
}

bool pico_btnp(pico_input_t* input, uint8_t button, uint8_t player) {
    if (player > 1) return false;
    return (input->btn_state[player] & button) != 0 &&
           (input->btn_prev[player] & button) == 0;
}
