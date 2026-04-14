#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <memory>

#include "Buzzer.hpp"
#include "ClockApp.hpp"
#include "DrowsinessBridge.hpp"
#include "lvgl/lvgl.h"

// Provide a custom tick source for LVGL based on the monotonic clock.
static uint32_t lv_tick_cb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int main(void) {
    lv_init();
    lv_tick_set_cb(lv_tick_cb);

    // adafruit-st7735r uses the DRM/KMS "tiny" driver → /dev/dri/card0.
    lv_display_t* disp = lv_linux_drm_create();
    if (!disp) {
        fprintf(stderr, "[clock] ERROR: failed to create DRM display\n");
        return 1;
    }

    // Set the file descriptor for the DRM device.
    if (lv_linux_drm_set_file(disp, "/dev/dri/card2", -1) != LV_RESULT_OK) {
        fprintf(stderr, "[clock] ERROR: failed to open DRM device\n");
        return 1;
    }

    // Initialise the KY-012 buzzer via libgpiod (GPIO14 / BCM 14 on gpiochip0).
    std::unique_ptr<Buzzer> buzzer;
    try {
        buzzer = std::make_unique<Buzzer>();  // defaults: /dev/gpiochip0, GPIO14
        fprintf(stderr, "[clock] Buzzer ready on GPIO14\n");
    } catch (const std::exception& e) {
        fprintf(stderr, "[clock] WARNING: buzzer unavailable: %s\n", e.what());
    }

    ClockApp app(buzzer.get());
    app.init();

    const char* shm_name = std::getenv("EARSYS_SHM_NAME");
    if (!shm_name || shm_name[0] == '\0') {
        shm_name = "/earsys_drowsy_shm";
    }
    DrowsinessBridge drowsy_bridge(shm_name);

    int last_second = -1;
    while (true) {
        bool is_drowsy = drowsy_bridge.poll();
        app.set_drowsy(is_drowsy);

        time_t     now    = time(nullptr);
        struct tm* tm_now = localtime(&now);

        if (tm_now && tm_now->tm_sec != last_second) {
            last_second = tm_now->tm_sec;
            app.update(*tm_now);
        }

        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms == 0 || delay_ms > 5)
            delay_ms = 5;
        usleep(delay_ms * 1000);
    }

    return 0;
}