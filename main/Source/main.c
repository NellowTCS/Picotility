#include "pico_vm.h"
#include "pico_config.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tt_app.h>
#include <tt_lvgl_toolbar.h>

// Display
/* PICO-8 native resolution is 128x128, 4bpp internal framebuffer.
 * We render to a 128x128 RGB565 canvas for LVGL. */
#define CANVAS_W PICO_SCREEN_WIDTH  /* 128 */
#define CANVAS_H PICO_SCREEN_HEIGHT /* 128 */

/* Pre-computed PICO-8 palette in RGB565 for direct buffer writes */
static const uint16_t pico_palette_565[16] = {
    0x0000, /*  0: black       */
    0x194A, /*  1: dark blue   */
    0x792A, /*  2: dark purple */
    0x042A, /*  3: dark green  */
    0xAA86, /*  4: brown       */
    0x5AA9, /*  5: dark gray   */
    0xC618, /*  6: light gray  */
    0xFFF5, /*  7: white       */
    0xF809, /*  8: red         */
    0xFD00, /*  9: orange      */
    0xFFA0, /* 10: yellow      */
    0x0706, /* 11: green       */
    0x2DFE, /* 12: blue        */
    0x83B3, /* 13: indigo      */
    0xFBB5, /* 14: pink        */
    0xFDEC, /* 15: peach       */
};

/* Keyboard mapping
 *
 * PICO-8 buttons:  Left, Right, Up, Down, O (action), X (action), Menu
 *
 * Keyboard layout for Tactility devices:
 *   WASD / arrow keys = directions
 *   Z/C/N             = O button (primary action)
 *   X/V/M             = X button (secondary action)
 *   P                 = Menu/Pause
 */
static void handle_key(pico_input_t* input, uint32_t ch, bool pressed) {
    switch (ch) {
        /* Directions */
        case 'a': case 'A': case LV_KEY_LEFT:
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_LEFT, pressed);
            break;
        case 'd': case 'D': case LV_KEY_RIGHT:
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_RIGHT, pressed);
            break;
        case 'w': case 'W': case LV_KEY_UP:
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_UP, pressed);
            break;
        case 's': case 'S': case LV_KEY_DOWN:
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_DOWN, pressed);
            break;
        /* O button (primary action) */
        case 'z': case 'Z': case 'c': case 'C': case 'n': case 'N':
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_O, pressed);
            break;
        /* X button (secondary action) */
        case 'x': case 'X': case 'v': case 'V': case 'm': case 'M':
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_X, pressed);
            break;
        /* Menu */
        case 'p': case 'P':
            pico_input_set_button(input, PICO_PLAYER_0, PICO_BTN_MENU, pressed);
            break;
        default:
            break;
    }
}

// App state
typedef enum {
    STATE_CART_SELECT,
    STATE_RUNNING,
} AppState;

typedef struct {
    pico_vm_t vm;
    AppState state;
    bool initialized;

    /* LVGL widgets */
    lv_obj_t* canvas;
    lv_obj_t* status_label;
    lv_obj_t* cart_list;
    lv_obj_t* parent;

    /* Canvas buffer, RGB565 (2 bytes per pixel) */
    uint16_t cbuf[CANVAS_W * CANVAS_H];

    /* Timer for emulation loop */
    lv_timer_t* emu_timer;
} PicotilityApp;

static PicotilityApp g_app;

/* Key hold-frame counters (same technique as Chip8).
 * Some keyboards don't reliably send RELEASED events. */
#define KEY_HOLD_FRAMES 6
#define NUM_BUTTONS 7
static uint8_t key_hold[NUM_BUTTONS]; /* indexed by button bit position */

/* Map PICO-8 button bit to key_hold index */
static int btn_to_idx(uint8_t btn) {
    switch (btn) {
        case PICO_BTN_LEFT:  return 0;
        case PICO_BTN_RIGHT: return 1;
        case PICO_BTN_UP:    return 2;
        case PICO_BTN_DOWN:  return 3;
        case PICO_BTN_O:     return 4;
        case PICO_BTN_X:     return 5;
        case PICO_BTN_MENU:  return 6;
        default: return -1;
    }
}

/* All PICO-8 button bits for iteration */
static const uint8_t all_buttons[NUM_BUTTONS] = {
    PICO_BTN_LEFT, PICO_BTN_RIGHT, PICO_BTN_UP, PICO_BTN_DOWN,
    PICO_BTN_O, PICO_BTN_X, PICO_BTN_MENU
};

// Shadow buffer for delta rendering
static uint8_t prev_framebuffer[PICO_FRAMEBUFFER_SIZE];

// Canvas rendering
static void render_display(bool force_full) {
    pico_ram_t* ram = &g_app.vm.ram;
    uint8_t* fb = ram->screen;
    uint16_t* buf = g_app.cbuf;

    /* Check if framebuffer changed at all (quick memcmp) */
    if (!force_full && memcmp(fb, prev_framebuffer, PICO_FRAMEBUFFER_SIZE) == 0) {
        return;
    }

    /* Delta render: only update changed pixels */
    for (int i = 0; i < PICO_FRAMEBUFFER_SIZE; i++) {
        if (fb[i] != prev_framebuffer[i] || force_full) {
            prev_framebuffer[i] = fb[i];
            /* Each byte holds 2 pixels (4bpp) */
            int px = i * 2;
            int lo = fb[i] & 0x0F;
            int hi = (fb[i] >> 4) & 0x0F;
            buf[px] = pico_palette_565[lo];
            buf[px + 1] = pico_palette_565[hi];
        }
    }

    lv_obj_invalidate(g_app.canvas);
}

// Return to cart select screen
static void return_to_menu(void) {
    g_app.state = STATE_CART_SELECT;
    g_app.vm.state = PICO_VM_STOPPED;

    if (g_app.canvas) lv_obj_add_flag(g_app.canvas, LV_OBJ_FLAG_HIDDEN);
    if (g_app.status_label) {
        lv_label_set_text(g_app.status_label, "Select a cart:");
        lv_obj_remove_flag(g_app.status_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_app.cart_list) {
        lv_obj_remove_flag(g_app.cart_list, LV_OBJ_FLAG_HIDDEN);
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_set_editing(grp, false);
            lv_group_focus_obj(g_app.cart_list);
        }
    }
}

/* Toolbar button callback, return to cart list */
static void on_cart_select_clicked(lv_event_t* e) {
    (void)e;
    if (g_app.state == STATE_RUNNING) {
        return_to_menu();
    }
}

// Emulation timer callback (~30 or 60 Hz)
static void emu_timer_cb(lv_timer_t* timer) {
    (void)timer;
    if (g_app.state != STATE_RUNNING) return;

    /* Decrement key hold counters and release expired keys */
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (key_hold[i] > 0) {
            key_hold[i]--;
            if (key_hold[i] == 0) {
                pico_input_set_button(&g_app.vm.input, PICO_PLAYER_0,
                                      all_buttons[i], false);
            }
        }
    }

    /* Update input state (copies current → previous for btnp) */
    pico_input_update(&g_app.vm.input);

    /* Step the VM (calls _update/_draw in Lua) */
    pico_vm_step(&g_app.vm);

    /* Check for VM error, show error and return to menu */
    if (g_app.vm.state == PICO_VM_ERROR) {
        if (g_app.status_label) {
            const char* err = pico_vm_get_error(&g_app.vm);
            lv_label_set_text_fmt(g_app.status_label, "Error: %s",
                                  err && err[0] ? err : "unknown");
        }
        g_app.state = STATE_CART_SELECT;
        if (g_app.canvas) lv_obj_add_flag(g_app.canvas, LV_OBJ_FLAG_HIDDEN);
        if (g_app.cart_list) lv_obj_remove_flag(g_app.cart_list, LV_OBJ_FLAG_HIDDEN);
        if (g_app.status_label) lv_obj_remove_flag(g_app.status_label, LV_OBJ_FLAG_HIDDEN);
        lv_group_t* grp = lv_group_get_default();
        if (grp) lv_group_set_editing(grp, false);
        return;
    }

    /* Render if anything changed */
    render_display(false);
}

// Key event handler
static void on_key_event(lv_event_t* e) {
    if (g_app.state != STATE_RUNNING) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        /* Save previous state, apply key, find which button changed */
        uint8_t prev = g_app.vm.input.btn_state[PICO_PLAYER_0];
        handle_key(&g_app.vm.input, key, true);
        uint8_t curr = g_app.vm.input.btn_state[PICO_PLAYER_0];
        uint8_t changed = (curr ^ prev) & curr; /* newly pressed bits */
        
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (changed & all_buttons[i]) {
                key_hold[i] = KEY_HOLD_FRAMES;
            }
        }
    } else if (code == LV_EVENT_RELEASED) {
        /* Accelerate release: clear after next frame's cycles */
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (key_hold[i] > 1) {
                key_hold[i] = 1;
            }
        }
    }
}

// Load and start a cart
static void start_cart_from_path(const char* path) {
    if (!pico_vm_load_cart(&g_app.vm, path)) {
        if (g_app.status_label) {
            const char* err = pico_vm_get_error(&g_app.vm);
            if (err && err[0]) {
                lv_label_set_text_fmt(g_app.status_label, "Error: %s", err);
            } else {
                lv_label_set_text_fmt(g_app.status_label, "Failed: %s", path);
            }
        }
        return;
    }

    /* Start the VM */
    pico_vm_run(&g_app.vm);
    g_app.state = STATE_RUNNING;

    /* Hide cart list, show canvas */
    if (g_app.cart_list) lv_obj_add_flag(g_app.cart_list, LV_OBJ_FLAG_HIDDEN);
    if (g_app.status_label) lv_obj_add_flag(g_app.status_label, LV_OBJ_FLAG_HIDDEN);
    if (g_app.canvas) lv_obj_remove_flag(g_app.canvas, LV_OBJ_FLAG_HIDDEN);

    /* Focus canvas for key input, editing mode passes arrow keys through */
    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_focus_obj(g_app.canvas);
        lv_group_set_editing(grp, true);
    }

    /* Reset shadow buffer */
    memset(prev_framebuffer, 0xFF, sizeof(prev_framebuffer));
    memset(key_hold, 0, sizeof(key_hold));

    /* Clear canvas buffer to black */
    memset(g_app.cbuf, 0, sizeof(g_app.cbuf));
    render_display(false);

    /* Update timer period based on cart's target FPS */
    if (g_app.emu_timer) {
        uint32_t period = 1000 / (g_app.vm.target_fps > 0 ? g_app.vm.target_fps : 30);
        lv_timer_set_period(g_app.emu_timer, period);
    }
}

// Cart list callbacks
static void on_cart_selected(lv_event_t* e) {
    const char* path = (const char*)lv_event_get_user_data(e);
    if (!path) return;
    start_cart_from_path(path);
}

// Scan SD card for .p8 cart files
#define TACTILITY_DIR "/sdcard/tactility"
#define CART_DIR TACTILITY_DIR "/picotility"

static void ensure_cart_dir(void) {
    mkdir(TACTILITY_DIR, 0755);
    mkdir(CART_DIR, 0755);
}

/* Case-insensitive string compare */
static int ci_strcmp(const char* a, const char* b) {
    while (*a && *b) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Dynamic path storage so LVGL event user_data pointers remain valid */
static char** cart_paths = NULL;
static int cart_count = 0;

static void free_cart_paths(void) {
    for (int i = 0; i < cart_count; i++) {
        free(cart_paths[i]);
    }
    free(cart_paths);
    cart_paths = NULL;
    cart_count = 0;
}

static void build_cart_list(lv_obj_t* list) {
    free_cart_paths();

    /* Ensure directory exists, then scan */
    ensure_cart_dir();
    DIR* dir = opendir(CART_DIR);
    if (!dir) {
        lv_list_add_text(list, "Put .p8/.p8.png in " CART_DIR "/");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        const char* name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4) continue;

        /* Accept .p8 and .p8.png extensions */
        bool valid = false;
        if (len >= 7 && ci_strcmp(&name[len - 7], ".p8.png") == 0) {
            valid = true;
        } else if (ci_strcmp(&name[len - 3], ".p8") == 0) {
            valid = true;
        }
        if (!valid) continue;

        /* Allocate path string */
        size_t path_len = strlen(CART_DIR) + 1 + len + 1;
        char* path = (char*)malloc(path_len);
        if (!path) break;
        snprintf(path, path_len, CART_DIR "/%s", name);

        /* Grow the pointer array */
        char** tmp = (char**)realloc(cart_paths, sizeof(char*) * (cart_count + 1));
        if (!tmp) {
            free(path);
            break;
        }
        cart_paths = tmp;
        cart_paths[cart_count] = path;

        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_FILE, name);
        lv_obj_add_event_cb(btn, on_cart_selected, LV_EVENT_CLICKED, path);
        cart_count++;
    }
    closedir(dir);

    if (cart_count == 0) {
        lv_list_add_text(list, "No carts found");
        lv_list_add_text(list, "Put .p8/.p8.png in " CART_DIR "/");
    }
}

// App lifecycle
static void onShowApp(AppHandle app, void* data, lv_obj_t* parent) {
    (void)data;
    memset(&g_app, 0, sizeof(g_app));
    g_app.parent = parent;
    g_app.state = STATE_CART_SELECT;

    /* Initialize VM */
    if (!pico_vm_init(&g_app.vm)) {
        /* Can't do much without the VM */
        lv_obj_t* lbl = lv_label_create(parent);
        lv_label_set_text(lbl, "Failed to initialize PICO-8 VM");
        return;
    }
    g_app.initialized = true;

    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

    /* Toolbar with cart select button */
    lv_obj_t* toolbar = tt_lvgl_toolbar_create_for_app(parent, app);
    lv_obj_align(toolbar, LV_ALIGN_TOP_MID, 0, 0);
    tt_lvgl_toolbar_add_text_button_action(toolbar, LV_SYMBOL_LIST,
                                           on_cart_select_clicked, NULL);

    /* Container below toolbar, fills remaining space */
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_width(cont, LV_PCT(100));
    lv_obj_set_flex_grow(cont, 1);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_set_style_pad_gap(cont, 2, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(cont, 0, 0);

    /* Status label */
    g_app.status_label = lv_label_create(cont);
    lv_label_set_text(g_app.status_label, "Select a cart:");
    lv_obj_set_width(g_app.status_label, LV_PCT(100));
    lv_obj_set_style_text_font(g_app.status_label, lv_font_get_default(), 0);

    /* Cart list (visible in CART_SELECT state) */
    g_app.cart_list = lv_list_create(cont);
    lv_obj_set_width(g_app.cart_list, LV_PCT(100));
    lv_obj_set_flex_grow(g_app.cart_list, 1);
    build_cart_list(g_app.cart_list);

    /* Canvas for PICO-8 display (hidden until cart starts) */
    g_app.canvas = lv_canvas_create(cont);
    lv_canvas_set_buffer(g_app.canvas, g_app.cbuf, CANVAS_W, CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_add_flag(g_app.canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(g_app.canvas, CANVAS_W, CANVAS_H);

    /* Key input on canvas */
    lv_obj_add_flag(g_app.canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_group_t* grp = lv_group_get_default();
    if (grp) lv_group_add_obj(grp, g_app.canvas);
    lv_obj_add_event_cb(g_app.canvas, on_key_event, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(g_app.canvas, on_key_event, LV_EVENT_RELEASED, NULL);

    /* Emulation timer (starts paused, only runs when STATE_RUNNING) */
    g_app.emu_timer = lv_timer_create(emu_timer_cb, 1000 / PICO_FPS_DEFAULT, NULL);

    /* Seed random */
    srand((unsigned)lv_tick_get());
}

static void onHideApp(AppHandle app, void* data) {
    (void)app;
    (void)data;
    if (g_app.emu_timer) {
        lv_timer_delete(g_app.emu_timer);
        g_app.emu_timer = NULL;
    }
    if (g_app.initialized) {
        pico_vm_shutdown(&g_app.vm);
        g_app.initialized = false;
    }
    free_cart_paths();
    g_app.state = STATE_CART_SELECT;
}

// Entry point
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    tt_app_register((AppRegistration){
        .onShow = onShowApp,
        .onHide = onHideApp,
    });
    return 0;
}
