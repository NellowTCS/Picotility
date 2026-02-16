// main.c
// Picotility - PICO-8 Emulator for Tactility

#include <tt_app.h>
#include <tt_lvgl_toolbar.h>
#include <lvgl.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#include "pico_vm.h"
#include "pico_config.h"

#define CART_DIR "/sdcard/carts"
#define CANVAS_W 128
#define CANVAS_H 128

typedef enum {
    UI_STATE_PICKER,
    UI_STATE_GAME
} UIState;

typedef struct {
    pico_vm_t vm;
    char cart_path[256];
    bool running;
    bool initialized;
    lv_obj_t* cart_list;
    lv_obj_t* status_label;
    lv_obj_t* picker_container;
    lv_obj_t* game_canvas;
    lv_color_t* canvas_buf;    // LVGL canvas buffer
    lv_timer_t* game_timer;
    UIState ui_state;
} PicotilityApp;

static PicotilityApp* g_app = NULL;

static lv_color_t pico_palette[16];
static void init_palette(void) {
    pico_palette[0]  = lv_color_make(0x00, 0x00, 0x00);  // black
    pico_palette[1]  = lv_color_make(0x1D, 0x2B, 0x53);  // dark_blue
    pico_palette[2]  = lv_color_make(0x7E, 0x25, 0x53);  // dark_purple
    pico_palette[3]  = lv_color_make(0x00, 0x87, 0x51);  // dark_green
    pico_palette[4]  = lv_color_make(0xAB, 0x52, 0x36);  // brown
    pico_palette[5]  = lv_color_make(0x5F, 0x57, 0x4F);  // dark_grey
    pico_palette[6]  = lv_color_make(0xC2, 0xC3, 0xC7);  // light_grey
    pico_palette[7]  = lv_color_make(0xFF, 0xF1, 0xE8);  // white
    pico_palette[8]  = lv_color_make(0xFF, 0x00, 0x4D);  // red
    pico_palette[9]  = lv_color_make(0xFF, 0xA3, 0x00);  // orange
    pico_palette[10] = lv_color_make(0xFF, 0xEC, 0x27);  // yellow
    pico_palette[11] = lv_color_make(0x00, 0xE4, 0x36);  // green
    pico_palette[12] = lv_color_make(0x29, 0xAD, 0xFF);  // blue
    pico_palette[13] = lv_color_make(0x83, 0x76, 0x9C);  // indigo
    pico_palette[14] = lv_color_make(0xFF, 0x77, 0xA8);  // pink
    pico_palette[15] = lv_color_make(0xFF, 0xCC, 0xAA);  // peach
}

static bool endswith_p8(const char* fn) {
    size_t l = strlen(fn);
    return l > 3 && strcmp(fn + l - 3, ".p8") == 0;
}

static void clear_picker(PicotilityApp* app) {
    if (app->picker_container) {
        lv_obj_del(app->picker_container);
        app->picker_container = NULL;
        app->cart_list = NULL;
        app->status_label = NULL;
    }
}

static void render_to_canvas(PicotilityApp* app) {
    // Map emulator framebuffer (4bpp, 128x128) to LVGL buffer
    if (!app || !app->game_canvas || !app->canvas_buf) return;
    pico_ram_t* ram = &app->vm.ram;
    uint8_t* framebuffer = ram->screen;
    for (int y = 0; y < CANVAS_H; ++y) {
        for (int x = 0; x < CANVAS_W; ++x) {
            uint8_t byte = framebuffer[(y * CANVAS_W + x) / 2];
            uint8_t color_idx = (x & 1) ? (byte >> 4) : (byte & 0x0F);
            app->canvas_buf[y * CANVAS_W + x] = pico_palette[color_idx];
        }
    }
    // Mark canvas dirty to LVGL
    lv_canvas_refresh(app->game_canvas);
}

static void game_timer_cb(lv_timer_t* t) {
    PicotilityApp* app = (PicotilityApp*)t->user_data;
    if (!app || !app->running) return;
    pico_vm_step(&app->vm);
    render_to_canvas(app);
}

static void start_game_ui(PicotilityApp* app) {
    clear_picker(app);

    app->ui_state = UI_STATE_GAME;

    if (app->canvas_buf) free(app->canvas_buf);
    app->canvas_buf = malloc(CANVAS_W * CANVAS_H * sizeof(lv_color_t));
    memset(app->canvas_buf, 0, CANVAS_W * CANVAS_H * sizeof(lv_color_t));

    app->game_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(app->game_canvas, app->canvas_buf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(app->game_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(app->game_canvas, 2, 0);

    lv_obj_t* back_btn = lv_btn_create(lv_scr_act());
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_t* lbl = lv_label_create(back_btn);
    lv_label_set_text(lbl, "Back");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        PicotilityApp* app = g_app;
        if (!app) return;
        // Stop game
        app->running = false;
        if (app->game_timer) { lv_timer_del(app->game_timer); app->game_timer = NULL; }
        if (app->game_canvas) { lv_obj_del(app->game_canvas); app->game_canvas = NULL; }
        if (app->canvas_buf) { free(app->canvas_buf); app->canvas_buf = NULL; }
        // Clean up back button
        lv_obj_del(lv_event_get_target(e));
        // Show picker UI
        app->ui_state = UI_STATE_PICKER;
        // Use parent as parent for new picker UI
        lv_obj_t* parent = lv_scr_act();
        // Re-create picker
        extern void show_picker_ui(lv_obj_t* parent, PicotilityApp* app);
        show_picker_ui(parent, app);
    }, LV_EVENT_CLICKED, NULL);

    // Start the game
    pico_vm_run(&app->vm);
    app->running = true;
    app->game_timer = lv_timer_create(game_timer_cb, 1000 / app->vm.target_fps, app);
}

static void cart_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = lv_event_get_target(e);
        const char* filename = (const char*)lv_obj_get_user_data(btn);
        if (g_app && filename) {
            char fullpath[256];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", CART_DIR, filename);
            strncpy(g_app->cart_path, fullpath, sizeof(g_app->cart_path)-1);
            bool ok = pico_vm_load_cart(&g_app->vm, g_app->cart_path);
            if (g_app->status_label) {
                if (ok)
                    lv_label_set_text_fmt(g_app->status_label, "Loaded: %s", g_app->cart_path);
                else
                    lv_label_set_text_fmt(g_app->status_label, "Failed to load: %s", g_app->cart_path);
            }
            if (ok) {
                // Transition to game UI
                start_game_ui(g_app);
            }
        }
    }
}

void show_picker_ui(lv_obj_t* parent, PicotilityApp* app) {
    // Picker container (column flex)
    lv_obj_t* container = lv_obj_create(parent);
    app->picker_container = container;
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x191d24), 0);
    lv_obj_set_style_pad_all(container, 8, 0);
    // Title
    lv_obj_t* label = lv_label_create(container);
    lv_label_set_text(label, "Picotility");
    lv_obj_set_style_text_color(label, lv_color_hex(0x29ADFF), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    // Info label
    lv_obj_t* info = lv_label_create(container);
    lv_label_set_text(info, "PICO-8 Emulator for Tactility");
    lv_obj_set_style_text_color(info, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    // Status label
    lv_obj_t* status = lv_label_create(container);
    lv_label_set_text(status, "Place .p8 files in /sdcard/carts/");
    lv_obj_set_style_text_color(status, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    app->status_label = status;
    // Cart List
    lv_obj_t* cart_list = lv_obj_create(container);
    app->cart_list = cart_list;
    lv_obj_set_size(cart_list, LV_PCT(100), LV_PCT(45));
    lv_obj_set_flex_flow(cart_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(cart_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(cart_list, 6, 0);
    // Scan directory
    DIR* dir = opendir(CART_DIR);
    int found = 0;
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG && endswith_p8(entry->d_name)) {
                lv_obj_t* btn = lv_btn_create(cart_list);
                lv_obj_set_width(btn, LV_PCT(100));
                lv_obj_t* lbl = lv_label_create(btn);
                lv_label_set_text(lbl, entry->d_name);
                lv_obj_center(lbl);
                char* fname = lv_mem_alloc(strlen(entry->d_name) + 1);
                strcpy(fname, entry->d_name);
                lv_obj_set_user_data(btn, fname);
                lv_obj_add_event_cb(btn, cart_btn_event_cb, LV_EVENT_CLICKED, NULL);
                found++;
            }
        }
        closedir(dir);
    }
    if (found == 0) {
        lv_obj_t* empty = lv_label_create(cart_list);
        lv_label_set_text(empty, "No .p8 files found in /sdcard/carts/");
    }
    app->ui_state = UI_STATE_PICKER;
}

// App Lifecycle
static void* app_create_data(void) {
    PicotilityApp* app = malloc(sizeof(PicotilityApp));
    if (!app) return NULL;
    memset(app, 0, sizeof(PicotilityApp));
    g_app = app;
    init_palette();
    if (!pico_vm_init(&app->vm)) {
        free(app);
        return NULL;
    }
    app->initialized = true;
    return app;
}

static void app_destroy_data(void* data) {
    PicotilityApp* app = (PicotilityApp*)data;
    if (!app) return;
    if (app->initialized) {
        pico_vm_shutdown(&app->vm);
    }
    if (app->canvas_buf) {
        free(app->canvas_buf);
    }
    if (app->game_timer) {
        lv_timer_del(app->game_timer);
    }
    if (app->picker_container) {
        lv_obj_del(app->picker_container);
    }
    if (app->game_canvas) {
        lv_obj_del(app->game_canvas);
    }
    g_app = NULL;
    free(app);
}

static void on_show(AppHandle app_handle, void* data, lv_obj_t* parent) {
    PicotilityApp* app = (PicotilityApp*)data;
    if (!app) return;
    // Toolbar at the top
    lv_obj_t* toolbar = tt_lvgl_toolbar_create_for_app(parent, app_handle);
    (void)toolbar;
    // Show picker UI
    show_picker_ui(parent, app);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    tt_app_register((AppRegistration) {
        .createData = app_create_data,
        .destroyData = app_destroy_data,
        .onShow = on_show,
    });
    return 0;
}
