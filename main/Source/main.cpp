// main.cpp — Picotility: PICO-8 Emulator for Tactility
#include "Picotility.h"

#include <app/event.h>
#include <app/manager.h>
#include <app/scheduler.h>
#include <lvgl_window_manager/window_manager.h>
#include <tactility/check.h>
#include <tactility/concurrent/task_event_group.h>

extern "C" {

int main(int argc, char* argv[]) {
    AppInstanceId appInstanceId = app_scheduler_current_app_id();
    Picotility app;

    TaskEventGroup eventGroup {};
    task_event_group_construct(&eventGroup);

    AppEventSubscription subscription {};
    check(app_event_subscribe(&subscription, &eventGroup) == ERROR_NONE);

    WindowId window = window_manager_create_ext(
        appInstanceId, Picotility::createWidgets, Picotility::destroyWidgets, &app);

    bool shouldClose = false;
    while (!shouldClose) {
        task_event_group_wait_any(&eventGroup, nullptr, portMAX_DELAY);

        AppEvent event {};
        while (app_event_poll(&subscription, &event) == ERROR_NONE) {
            if (event.type == APP_EVENT_CLOSE) {
                shouldClose = true;
                break;
            }
        }
    }

    window_manager_remove(window);
    check(app_event_unsubscribe(&subscription) == ERROR_NONE);
    task_event_group_destruct(&eventGroup);
    return 0;
}

}
