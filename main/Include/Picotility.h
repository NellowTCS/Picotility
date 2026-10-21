// Picotility.h — PICO-8 Emulator app class for Tactility
#pragma once

#include <app/instance.h>
#include <lvgl.h>

extern "C" {
#include <tactility/device.h>
#include <tactility/drivers/display.h>
#include "pico_vm.h"
#include "pico_config.h"
}

#define CANVAS_W PICO_SCREEN_WIDTH   /* 128 */
#define CANVAS_H PICO_SCREEN_HEIGHT  /* 128 */

class Picotility final {

private:
    enum class AppState {
        CartSelect,
        Running,
    };

    // Non-copyable, non-movable (raw pointers to LVGL objects)
    Picotility(const Picotility&) = delete;
    Picotility& operator=(const Picotility&) = delete;
    Picotility(Picotility&&) = delete;
    Picotility& operator=(Picotility&&) = delete;

    // VM
    pico_vm_t vm{};
    AppState state = AppState::CartSelect;
    bool initialized = false;

    // LVGL widgets (nulled in onHide, recreated in onShow)
    lv_obj_t* canvas = nullptr;
    lv_obj_t* statusLabel = nullptr;
    lv_obj_t* cartList = nullptr;
    lv_obj_t* parentWidget = nullptr;

    // Native display row buffer. The PICO framebuffer remains packed 4bpp in pico_ram_t.
    uint16_t* displayRow = nullptr;
    int displayRowWidth = 0;
    static constexpr int DISPLAY_BUFFER_ROWS = 4;
    static constexpr int MAX_DISPLAY_BANDS = (480 + DISPLAY_BUFFER_ROWS - 1) / DISPLAY_BUFFER_ROWS;
    int renderWidth = CANVAS_W;
    int renderHeight = CANVAS_H;
    uint16_t* sourceXMap = nullptr;
    uint16_t* sourceYMap = nullptr;
    Device* display = nullptr;
    int displayOffsetX = 0;
    int displayOffsetY = 0;

    // Timer for emulation loop
    lv_timer_t* emuTimer = nullptr;

    // Key hold-frame counters
    static constexpr int KEY_HOLD_FRAMES = 6;
    uint8_t keyHold[7]{};

    // Methods
    void renderDisplay(bool forceFull);
    void returnToMenu();
    void closeApp();
    void startCartFromPath(const char* path);
    void buildCartList(lv_obj_t* list);

    // Static callbacks
    static void onCartSelectClicked(lv_event_t* e);
    static void onNavigationClicked(lv_event_t* e);
    static void emuTimerCb(lv_timer_t* timer);
    static void onKeyEvent(lv_event_t* e);
    static void onTouchEvent(lv_event_t* e);
    static void onCartSelected(lv_event_t* e);

public:
    Picotility();
    
    static void createWidgets(lv_obj_t* parent, void* userData);
    static void destroyWidgets(void* userData);
    void onShow(AppInstanceId app, lv_obj_t* parent);
    void onHide();
};
