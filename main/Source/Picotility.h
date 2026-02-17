// Picotility.h — PICO-8 Emulator app class for Tactility
#pragma once

#include <tt_app.h>
#include <lvgl.h>
#include <TactilityCpp/App.h>

extern "C" {
#include "pico_vm.h"
#include "pico_config.h"
}

#define CANVAS_W PICO_SCREEN_WIDTH   /* 128 */
#define CANVAS_H PICO_SCREEN_HEIGHT  /* 128 */

class Picotility final : public App {

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

    // Canvas buffer — RGB565 (2 bytes per pixel)
    uint16_t cbuf[CANVAS_W * CANVAS_H]{};

    // Timer for emulation loop
    lv_timer_t* emuTimer = nullptr;

    // Key hold-frame counters
    static constexpr int KEY_HOLD_FRAMES = 6;
    uint8_t keyHold[7]{};

    // Methods
    void renderDisplay(bool forceFull);
    void returnToMenu();
    void startCartFromPath(const char* path);
    void buildCartList(lv_obj_t* list);

    // Static callbacks
    static void onCartSelectClicked(lv_event_t* e);
    static void emuTimerCb(lv_timer_t* timer);
    static void onKeyEvent(lv_event_t* e);
    static void onCartSelected(lv_event_t* e);

public:
    void onShow(AppHandle context, lv_obj_t* parent) override;
    void onHide(AppHandle context) override;
};
