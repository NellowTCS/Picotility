// main.c
// Picotility - PICO-8 Emulator for Tactility

#include <tt_app.h>
#include <tt_lvgl_toolbar.h>
#include <lvgl.h>
#include <string.h>
#include <stdlib.h>

#include "pico_vm.h"
#include "pico_config.h"

// App State
typedef struct {
    pico_vm_t vm;
    lv_obj_t* canvas;
    lv_color_t* canvas_buf;
    lv_timer_t* game_timer;
    char cart_path[256];
    bool running;
    bool initialized;
} PicotilityApp;

static PicotilityApp* g_app = NULL;

// PICO-8 palette in LVGL format
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
    
    g_app = NULL;
    free(app);
}

static void on_show(AppHandle app_handle, void* data, lv_obj_t* parent) {
    PicotilityApp* app = (PicotilityApp*)data;
    if (!app) return;
    
    // Create toolbar
    lv_obj_t* toolbar = tt_lvgl_toolbar_create_for_app(parent, app_handle);
    (void)toolbar;  // Suppress unused warning
    
    // Create main container
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(container, 10, 0);
    
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
    
    // Status
    lv_obj_t* status = lv_label_create(container);
    lv_label_set_text(status, "Place .p8 files in /sdcard/carts/");
    lv_obj_set_style_text_color(status, lv_color_hex(0x888888), 0);
}

// Entry Point
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
