// Picotility.cpp — PICO-8 Emulator for Tactility
// Converted from main.c to C++ class pattern

#include "Picotility.h"

#include <app/event.h>
#include <app/manager.h>
#include <app/paths.h>
#include <app/scheduler.h>
#include <lvgl/widgets/toolbar.h>

#include <dirent.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#define PICO_DEBUG 1

#if PICO_DEBUG
#define PICO_LOG(fmt, ...) printf("[PICO] " fmt "\n", ##__VA_ARGS__)
#else
#define PICO_LOG(fmt, ...) ((void)0)
#endif

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
#define APP_ID "me.nellowtcs.picotility"

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

    /* Check if framebuffer changed at all (quick memcmp) */
    if (!forceFull && memcmp(fb, prev_framebuffer, PICO_FRAMEBUFFER_SIZE) == 0) {
        return;
    }

    if (!display) return;

    if (displayRow == nullptr || displayRowWidth != renderWidth ||
        sourceXMap == nullptr || sourceYMap == nullptr) return;

    if ((vm.frame_count % 60) == 0) {
        PICO_LOG("render: frame=%lu size=%dx%d offset=%d,%d full=%d",
                 (unsigned long)vm.frame_count, renderWidth, renderHeight,
                 displayOffsetX, displayOffsetY, forceFull ? 1 : 0);
    }

    /* Render in small bands. Four rows reduce native display calls by 4x
       while keeping the native buffer far below a full framebuffer. */
    const uint8_t* pal = ram->ds.screen_pal;
    bool dirtyBands[MAX_DISPLAY_BANDS] = {};
    for (int y = 0; y < CANVAS_H; y++) {
        const int band = y * renderHeight / CANVAS_H / DISPLAY_BUFFER_ROWS;
        for (int x = 0; x < CANVAS_W / 2; x++) {
            int index = y * (CANVAS_W / 2) + x;
            if (!forceFull && fb[index] == prev_framebuffer[index]) continue;
            prev_framebuffer[index] = fb[index];
            if (band >= 0 && band < MAX_DISPLAY_BANDS) {
                dirtyBands[band] = true;
            }
        }
    }
    for (int bandY = 0; bandY < renderHeight; bandY += DISPLAY_BUFFER_ROWS) {
        const int band = bandY / DISPLAY_BUFFER_ROWS;
        if (!forceFull && (band >= MAX_DISPLAY_BANDS || !dirtyBands[band])) continue;
        const int bandHeight = LV_MIN(DISPLAY_BUFFER_ROWS, renderHeight - bandY);
        for (int outputY = 0; outputY < bandHeight; outputY++) {
            const int sourceY = sourceYMap[bandY + outputY];
            for (int outputX = 0; outputX < renderWidth; outputX++) {
                const int sourceX = sourceXMap[outputX];
                const int sourceIndex = sourceY * (CANVAS_W / 2) + sourceX / 2;
                const uint8_t pixel = fb[sourceIndex];
                const uint8_t color = (sourceX & 1) ? (pixel >> 4) : (pixel & 0x0F);
                displayRow[outputY * renderWidth + outputX] = pico_palette_565[pal[color]];
            }
        }
        display_draw_bitmap(display, displayOffsetX, displayOffsetY + bandY,
                            displayOffsetX + renderWidth, displayOffsetY + bandY + bandHeight,
                            displayRow);
    }
}

/* ── Return to cart select screen ────────────────────────────────────── */

void Picotility::returnToMenu() {
    state = AppState::CartSelect;
    vm.state = PICO_VM_STOPPED;
    if (display && displayRow != nullptr) {
        memset(displayRow, 0, static_cast<size_t>(renderWidth) * DISPLAY_BUFFER_ROWS * sizeof(*displayRow));
        for (int y = 0; y < renderHeight; y += DISPLAY_BUFFER_ROWS) {
            const int bandHeight = LV_MIN(DISPLAY_BUFFER_ROWS, renderHeight - y);
            display_draw_bitmap(display, displayOffsetX, displayOffsetY + y,
                                displayOffsetX + renderWidth, displayOffsetY + y + bandHeight,
                                displayRow);
        }
    }

    if (canvas) lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    if (statusLabel) {
        lv_label_set_text(statusLabel, "Select a cart:");
        lv_obj_remove_flag(statusLabel, LV_OBJ_FLAG_HIDDEN);
    }
    if (cartList) {
        lv_obj_remove_flag(cartList, LV_OBJ_FLAG_HIDDEN);
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_remove_all_objs(grp);
            lv_group_add_obj(grp, cartList);
            lv_group_set_editing(grp, false);
            lv_group_focus_obj(cartList);
        }
    }
}

void Picotility::closeApp() {
    app_event_emit_close(app_scheduler_current_app_id());
}

/* ── Toolbar button callback — return to cart list ───────────────────── */

void Picotility::onCartSelectClicked(lv_event_t* e) {
    (void)e;
    if (!g_instance) return;
    if (g_instance->state == AppState::Running) {
        g_instance->returnToMenu();
    }
}

void Picotility::onNavigationClicked(lv_event_t* e) {
    (void)e;
    if (!g_instance) return;
    if (g_instance->state == AppState::Running) {
        g_instance->returnToMenu();
    } else {
        g_instance->closeApp();
    }
}

/* ── Emulation timer callback (~30 or 60 Hz) ─────────────────────────── */

void Picotility::emuTimerCb(lv_timer_t* timer) {
    (void)timer;
    if (!g_instance) {
        PICO_LOG("timer: no instance");
        return;
    }
    if (g_instance->state != AppState::Running) {
        return;
    }

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

    if ((self->vm.frame_count % 60) == 0) {
        PICO_LOG("frame: %lu state=%d", (unsigned long)self->vm.frame_count,
                 (int)self->vm.state);
    }

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
    if (!g_instance) {
        PICO_LOG("key: no instance");
        return;
    }
    if (g_instance->state != AppState::Running) {
        PICO_LOG("key: not running (state=%d)", (int)g_instance->state);
        return;
    }

    auto* self = g_instance;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        PICO_LOG("key: press key=%lu", (unsigned long)key);

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
        uint32_t key = lv_event_get_key(e);
        handle_key(&self->vm.input, key, false);
        
        /* Accelerate release: clear after next frame's cycles */
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (self->keyHold[i] > 1) {
                self->keyHold[i] = 1;
            }
        }
    }
}

/* Touch event handler */

void Picotility::onTouchEvent(lv_event_t* e) {
    if (!g_instance) return;
    if (g_instance->state != AppState::Running) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    /* Convert screen coordinates to the centered square game area. */
    int16_t cx = point.x - g_instance->displayOffsetX;
    int16_t cy = point.y - g_instance->displayOffsetY;
    if (cx < 0 || cy < 0 || cx >= g_instance->renderWidth || cy >= g_instance->renderHeight) {
        pico_input_touch(&g_instance->vm.input, false, 0, 0);
        return;
    }
    cx = static_cast<int16_t>((cx * CANVAS_W) / g_instance->renderWidth);
    cy = static_cast<int16_t>((cy * CANVAS_H) / g_instance->renderHeight);

    bool pressed = (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING);
    pico_input_touch(&g_instance->vm.input, pressed, cx, cy);
}

/* ── Load and start a cart ───────────────────────────────────────────── */

void Picotility::startCartFromPath(const char* path) {
    PICO_LOG("startCart: loading %s", path);
    if (!pico_vm_load_cart(&vm, path)) {
        const char* err = pico_vm_get_error(&vm);
        PICO_LOG("startCart: load failed: %s", err ? err : "unknown");
        if (statusLabel) {
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
    PICO_LOG("startCart: running fps=%lu draw=%d update=%d update60=%d",
             (unsigned long)vm.target_fps, vm.has_draw ? 1 : 0,
             vm.has_update ? 1 : 0, vm.has_update60 ? 1 : 0);

    /* Hide cart list, show canvas */
    if (cartList) lv_obj_add_flag(cartList, LV_OBJ_FLAG_HIDDEN);
    if (statusLabel) lv_obj_add_flag(statusLabel, LV_OBJ_FLAG_HIDDEN);
    if (canvas) lv_obj_remove_flag(canvas, LV_OBJ_FLAG_HIDDEN);

    /* Isolate gameplay input from the app's navigation group. This prevents
       Tab/arrow keys from moving focus through the cart UI while playing. */
    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_remove_all_objs(grp);
        lv_group_add_obj(grp, canvas);
        lv_group_focus_obj(canvas);
        lv_group_set_editing(grp, true);
    }

    /* Reset shadow buffer */
    memset(prev_framebuffer, 0xFF, sizeof(prev_framebuffer));
    memset(keyHold, 0, sizeof(keyHold));

    memset(displayRow, 0, sizeof(displayRow));
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

    /* Scan user-installed carts first. Packaged carts are scanned below. */
    ensure_cart_dir();
    char assetsDir[256] = {};
    bool hasPackagedAssets = app_paths_get_assets_directory(APP_ID, assetsDir, sizeof(assetsDir)) == ERROR_NONE;
    const char* directories[] = { CART_DIR, assetsDir };
    const int directoryCount = hasPackagedAssets ? 2 : 1;
    for (int directoryIndex = 0; directoryIndex < directoryCount; directoryIndex++) {
        char cartDirectory[320] = {};
        if (directoryIndex == 0) {
            snprintf(cartDirectory, sizeof(cartDirectory), "%s", directories[directoryIndex]);
        } else {
            snprintf(cartDirectory, sizeof(cartDirectory), "%s/carts", directories[directoryIndex]);
        }

        DIR* dir = opendir(cartDirectory);
        if (!dir) continue;
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
        size_t path_len = strlen(cartDirectory) + 1 + len + 1;
        char* path = (char*)malloc(path_len);
        if (!path) break;
        snprintf(path, path_len, "%s/%s", cartDirectory, name);

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
    }

    if (cart_count == 0) {
        lv_list_add_text(list, "No carts found");
        lv_list_add_text(list, "Put .p8/.p8.png in " CART_DIR "/");
    }
}

/* ── App lifecycle ───────────────────────────────────────────────────── */

void Picotility::onShow(AppInstanceId app, lv_obj_t* parent) {
    (void)app;
    PICO_LOG("lifecycle: onShow");
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
    displayOffsetX = 0;
    displayOffsetY = 0;
    renderWidth = CANVAS_W;
    renderHeight = CANVAS_H;
    free(displayRow);
    displayRow = nullptr;
    displayRowWidth = 0;
    free(sourceXMap);
    sourceXMap = nullptr;
    free(sourceYMap);
    sourceYMap = nullptr;

    if (device_get_first_active_by_type(&DISPLAY_TYPE, &display) == ERROR_NONE) {
    }

    /* Initialize VM */
    if (!pico_vm_init(&vm)) {
        lv_obj_t* lbl = lv_label_create(parent);
        lv_label_set_text(lbl, "Failed to initialize PICO-8 VM");
        return;
    }
    initialized = true;

    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* toolbar = lvgl_toolbar_create(parent, "Picotility");
    lvgl_toolbar_set_nav_action(toolbar, LV_SYMBOL_LEFT, onNavigationClicked, nullptr);

    /* Main content container */
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

    /* Transparent LVGL input surface; game pixels use the native display API. */
    canvas = lv_obj_create(cont);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(canvas, LV_PCT(100));
    lv_obj_set_flex_grow(canvas, 1);

    /* Key input on canvas */
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_group_t* grp = lv_group_get_default();
    if (grp) lv_group_add_obj(grp, canvas);
    lv_obj_add_event_cb(canvas, onKeyEvent, LV_EVENT_KEY, nullptr);
    lv_obj_add_event_cb(canvas, onKeyEvent, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(canvas, onTouchEvent, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(canvas, onTouchEvent, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(canvas, onTouchEvent, LV_EVENT_PRESSING, nullptr);

    lv_obj_add_flag(cartList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(canvas);
    lv_area_t canvasArea;
    lv_obj_get_coords(canvas, &canvasArea);
    const int availableWidth = LV_MAX(1, lv_area_get_width(&canvasArea));
    const int availableHeight = LV_MAX(1, lv_area_get_height(&canvasArea));
    const int squareSize = LV_MIN(availableWidth, availableHeight);
    renderWidth = squareSize;
    renderHeight = squareSize;
    displayOffsetX = canvasArea.x1 + (availableWidth - squareSize) / 2;
    displayOffsetY = canvasArea.y1 + (availableHeight - squareSize) / 2;
    sourceXMap = static_cast<uint16_t*>(malloc(static_cast<size_t>(renderWidth) * sizeof(uint16_t)));
    sourceYMap = static_cast<uint16_t*>(malloc(static_cast<size_t>(renderHeight) * sizeof(uint16_t)));
    if (sourceXMap != nullptr) {
        for (int x = 0; x < renderWidth; x++) sourceXMap[x] = (x * CANVAS_W) / renderWidth;
    }
    if (sourceYMap != nullptr) {
        for (int y = 0; y < renderHeight; y++) sourceYMap[y] = (y * CANVAS_H) / renderHeight;
    }
    if (display != nullptr) {
        displayRow = static_cast<uint16_t*>(calloc(
            static_cast<size_t>(renderWidth) * DISPLAY_BUFFER_ROWS, sizeof(uint16_t)));
        if (displayRow != nullptr) {
            displayRowWidth = renderWidth;
        }
    }
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(cartList, LV_OBJ_FLAG_HIDDEN);
    if (grp) {
        lv_group_remove_all_objs(grp);
        lv_group_add_obj(grp, cartList);
        lv_group_focus_obj(cartList);
        lv_group_set_editing(grp, false);
    }

    /* Emulation timer (starts paused — only runs when Running) */
    emuTimer = lv_timer_create(emuTimerCb, 1000 / PICO_FPS_DEFAULT, nullptr);

    /* Seed random */
    srand((unsigned)lv_tick_get());
}

Picotility::Picotility() = default;

void Picotility::onHide() {
    PICO_LOG("lifecycle: onHide state=%d frame=%lu",
             (int)state, (unsigned long)vm.frame_count);

    if (emuTimer) {
        lv_timer_delete(emuTimer);
        emuTimer = nullptr;
    }
    if (initialized) {
        pico_vm_shutdown(&vm);
        initialized = false;
    }
    if (display) {
        device_put(display);
        display = nullptr;
    }
    free(displayRow);
    displayRow = nullptr;
    displayRowWidth = 0;
    free_cart_paths();
    state = AppState::CartSelect;

    // Null all UI pointers (LVGL objects invalid after hide)
    canvas = nullptr;
    statusLabel = nullptr;
    cartList = nullptr;
    parentWidget = nullptr;

    g_instance = nullptr;
}

void Picotility::createWidgets(lv_obj_t* parent, void* userData) {
    auto* app = static_cast<Picotility*>(userData);
    app->onShow(0, parent);
}

void Picotility::destroyWidgets(void* userData) {
    auto* app = static_cast<Picotility*>(userData);
    app->onHide();
}
