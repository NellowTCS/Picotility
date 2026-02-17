// Picotility.cpp — PICO-8 Emulator for Tactility
// Converted from main.c to C++ class pattern

#include "Picotility.h"

#include <dirent.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <tt_lvgl_toolbar.h>

/* ── Global instance pointer for static callbacks ────────────────────── */
static Picotility* g_instance = nullptr;

/* ── Pre-computed PICO-8 palette in RGB565 for direct buffer writes ─── */
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

/* ── Keyboard mapping ────────────────────────────────────────────────── */
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

/* ── Button mapping helpers ──────────────────────────────────────────── */

static constexpr int NUM_BUTTONS = 7;

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

static const uint8_t all_buttons[NUM_BUTTONS] = {
    PICO_BTN_LEFT, PICO_BTN_RIGHT, PICO_BTN_UP, PICO_BTN_DOWN,
    PICO_BTN_O, PICO_BTN_X, PICO_BTN_MENU
};

/* ── Shadow buffer for delta rendering ───────────────────────────────── */
static uint8_t prev_framebuffer[PICO_FRAMEBUFFER_SIZE];

/* ── Cart file management (file-scope statics) ───────────────────────── */

#define TACTILITY_DIR "/sdcard/tactility"
#define CART_DIR TACTILITY_DIR "/picotility"

static char** cart_paths = nullptr;
static int cart_count = 0;

static void free_cart_paths() {
    for (int i = 0; i < cart_count; i++) {
        free(cart_paths[i]);
    }
    free(cart_paths);
    cart_paths = nullptr;
    cart_count = 0;
}

static void ensure_cart_dir() {
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

/* ── Canvas rendering ────────────────────────────────────────────────── */

void Picotility::renderDisplay(bool forceFull) {
    pico_ram_t* ram = &vm.ram;
    uint8_t* fb = ram->screen;
    uint16_t* buf = cbuf;

    /* Check if framebuffer changed at all (quick memcmp) */
    if (!forceFull && memcmp(fb, prev_framebuffer, PICO_FRAMEBUFFER_SIZE) == 0) {
        return;
    }

    /* Delta render: only update changed pixels */
    for (int i = 0; i < PICO_FRAMEBUFFER_SIZE; i++) {
        if (fb[i] != prev_framebuffer[i] || forceFull) {
            prev_framebuffer[i] = fb[i];
            /* Each byte holds 2 pixels (4bpp) */
            int px = i * 2;
            int lo = fb[i] & 0x0F;
            int hi = (fb[i] >> 4) & 0x0F;
            buf[px] = pico_palette_565[lo];
            buf[px + 1] = pico_palette_565[hi];
        }
    }

    lv_obj_invalidate(canvas);
}

/* ── Return to cart select screen ────────────────────────────────────── */

void Picotility::returnToMenu() {
    state = AppState::CartSelect;
    vm.state = PICO_VM_STOPPED;

    if (canvas) lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    if (statusLabel) {
        lv_label_set_text(statusLabel, "Select a cart:");
        lv_obj_remove_flag(statusLabel, LV_OBJ_FLAG_HIDDEN);
    }
    if (cartList) {
        lv_obj_remove_flag(cartList, LV_OBJ_FLAG_HIDDEN);
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_set_editing(grp, false);
            lv_group_focus_obj(cartList);
        }
    }
}

/* ── Toolbar button callback — return to cart list ───────────────────── */

void Picotility::onCartSelectClicked(lv_event_t* e) {
    (void)e;
    if (!g_instance) return;
    if (g_instance->state == AppState::Running) {
        g_instance->returnToMenu();
    }
}

/* ── Emulation timer callback (~30 or 60 Hz) ─────────────────────────── */

void Picotility::emuTimerCb(lv_timer_t* timer) {
    (void)timer;
    if (!g_instance) return;
    if (g_instance->state != AppState::Running) return;

    auto* self = g_instance;

    /* Decrement key hold counters and release expired keys */
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (self->keyHold[i] > 0) {
            self->keyHold[i]--;
            if (self->keyHold[i] == 0) {
                pico_input_set_button(&self->vm.input, PICO_PLAYER_0,
                                      all_buttons[i], false);
            }
        }
    }

    /* Update input state (copies current -> previous for btnp) */
    pico_input_update(&self->vm.input);

    /* Step the VM (calls _update/_draw in Lua) */
    pico_vm_step(&self->vm);

    /* Check for VM error — show error and return to menu */
    if (self->vm.state == PICO_VM_ERROR) {
        if (self->statusLabel) {
            const char* err = pico_vm_get_error(&self->vm);
            lv_label_set_text_fmt(self->statusLabel, "Error: %s",
                                  err && err[0] ? err : "unknown");
        }
        self->state = AppState::CartSelect;
        if (self->canvas) lv_obj_add_flag(self->canvas, LV_OBJ_FLAG_HIDDEN);
        if (self->cartList) lv_obj_remove_flag(self->cartList, LV_OBJ_FLAG_HIDDEN);
        if (self->statusLabel) lv_obj_remove_flag(self->statusLabel, LV_OBJ_FLAG_HIDDEN);
        lv_group_t* grp = lv_group_get_default();
        if (grp) lv_group_set_editing(grp, false);
        return;
    }

    /* Render if anything changed */
    self->renderDisplay(false);
}

/* ── Key event handler ───────────────────────────────────────────────── */

void Picotility::onKeyEvent(lv_event_t* e) {
    if (!g_instance) return;
    if (g_instance->state != AppState::Running) return;

    auto* self = g_instance;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        /* Save previous state, apply key, find which button changed */
        uint8_t prev = self->vm.input.btn_state[PICO_PLAYER_0];
        handle_key(&self->vm.input, key, true);
        uint8_t curr = self->vm.input.btn_state[PICO_PLAYER_0];
        uint8_t changed = (curr ^ prev) & curr; /* newly pressed bits */

        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (changed & all_buttons[i]) {
                self->keyHold[i] = KEY_HOLD_FRAMES;
            }
        }
    } else if (code == LV_EVENT_RELEASED) {
        /* Accelerate release: clear after next frame's cycles */
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (self->keyHold[i] > 1) {
                self->keyHold[i] = 1;
            }
        }
    }
}

/* ── Load and start a cart ───────────────────────────────────────────── */

void Picotility::startCartFromPath(const char* path) {
    if (!pico_vm_load_cart(&vm, path)) {
        if (statusLabel) {
            const char* err = pico_vm_get_error(&vm);
            if (err && err[0]) {
                lv_label_set_text_fmt(statusLabel, "Error: %s", err);
            } else {
                lv_label_set_text_fmt(statusLabel, "Failed: %s", path);
            }
        }
        return;
    }

    /* Start the VM */
    pico_vm_run(&vm);
    state = AppState::Running;

    /* Hide cart list, show canvas */
    if (cartList) lv_obj_add_flag(cartList, LV_OBJ_FLAG_HIDDEN);
    if (statusLabel) lv_obj_add_flag(statusLabel, LV_OBJ_FLAG_HIDDEN);
    if (canvas) lv_obj_remove_flag(canvas, LV_OBJ_FLAG_HIDDEN);

    /* Focus canvas for key input — editing mode passes arrow keys through */
    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_focus_obj(canvas);
        lv_group_set_editing(grp, true);
    }

    /* Reset shadow buffer */
    memset(prev_framebuffer, 0xFF, sizeof(prev_framebuffer));
    memset(keyHold, 0, sizeof(keyHold));

    /* Clear canvas buffer to black */
    memset(cbuf, 0, sizeof(cbuf));
    renderDisplay(false);

    /* Update timer period based on cart's target FPS */
    if (emuTimer) {
        uint32_t period = 1000 / (vm.target_fps > 0 ? vm.target_fps : 30);
        lv_timer_set_period(emuTimer, period);
    }
}

/* ── Cart selected callback ──────────────────────────────────────────── */

void Picotility::onCartSelected(lv_event_t* e) {
    if (!g_instance) return;
    const char* path = (const char*)lv_event_get_user_data(e);
    if (!path) return;
    g_instance->startCartFromPath(path);
}

/* ── Build cart list from SD card ────────────────────────────────────── */

void Picotility::buildCartList(lv_obj_t* list) {
    free_cart_paths();

    /* Ensure directory exists, then scan */
    ensure_cart_dir();
    DIR* dir = opendir(CART_DIR);
    if (!dir) {
        lv_list_add_text(list, "Put .p8/.p8.png in " CART_DIR "/");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
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
        lv_obj_add_event_cb(btn, onCartSelected, LV_EVENT_CLICKED, path);
        cart_count++;
    }
    closedir(dir);

    if (cart_count == 0) {
        lv_list_add_text(list, "No carts found");
        lv_list_add_text(list, "Put .p8/.p8.png in " CART_DIR "/");
    }
}

/* ── App lifecycle ───────────────────────────────────────────────────── */

void Picotility::onShow(AppHandle app, lv_obj_t* parent) {
    g_instance = this;

    // Zero-init relevant state
    memset(&vm, 0, sizeof(vm));
    parentWidget = parent;
    state = AppState::CartSelect;
    canvas = nullptr;
    statusLabel = nullptr;
    cartList = nullptr;
    emuTimer = nullptr;
    initialized = false;
    memset(keyHold, 0, sizeof(keyHold));
    memset(cbuf, 0, sizeof(cbuf));

    /* Initialize VM */
    if (!pico_vm_init(&vm)) {
        lv_obj_t* lbl = lv_label_create(parent);
        lv_label_set_text(lbl, "Failed to initialize PICO-8 VM");
        return;
    }
    initialized = true;

    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

    /* Toolbar with cart select button */
    lv_obj_t* toolbar = tt_lvgl_toolbar_create_for_app(parent, app);
    lv_obj_align(toolbar, LV_ALIGN_TOP_MID, 0, 0);
    tt_lvgl_toolbar_add_text_button_action(toolbar, LV_SYMBOL_LIST,
                                           onCartSelectClicked, nullptr);

    /* Container below toolbar — fills remaining space */
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
    statusLabel = lv_label_create(cont);
    lv_label_set_text(statusLabel, "Select a cart:");
    lv_obj_set_width(statusLabel, LV_PCT(100));
    lv_obj_set_style_text_font(statusLabel, lv_font_get_default(), 0);

    /* Cart list (visible in CartSelect state) */
    cartList = lv_list_create(cont);
    lv_obj_set_width(cartList, LV_PCT(100));
    lv_obj_set_flex_grow(cartList, 1);
    buildCartList(cartList);

    /* Canvas for PICO-8 display (hidden until cart starts) */
    canvas = lv_canvas_create(cont);
    lv_canvas_set_buffer(canvas, cbuf, CANVAS_W, CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(canvas, CANVAS_W, CANVAS_H);

    /* Key input on canvas */
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_group_t* grp = lv_group_get_default();
    if (grp) lv_group_add_obj(grp, canvas);
    lv_obj_add_event_cb(canvas, onKeyEvent, LV_EVENT_KEY, nullptr);
    lv_obj_add_event_cb(canvas, onKeyEvent, LV_EVENT_RELEASED, nullptr);

    /* Emulation timer (starts paused — only runs when Running) */
    emuTimer = lv_timer_create(emuTimerCb, 1000 / PICO_FPS_DEFAULT, nullptr);

    /* Seed random */
    srand((unsigned)lv_tick_get());
}

void Picotility::onHide(AppHandle app) {
    (void)app;

    if (emuTimer) {
        lv_timer_delete(emuTimer);
        emuTimer = nullptr;
    }
    if (initialized) {
        pico_vm_shutdown(&vm);
        initialized = false;
    }
    free_cart_paths();
    state = AppState::CartSelect;

    // Null all UI pointers (LVGL objects invalid after hide)
    canvas = nullptr;
    statusLabel = nullptr;
    cartList = nullptr;
    parentWidget = nullptr;

    g_instance = nullptr;
}
