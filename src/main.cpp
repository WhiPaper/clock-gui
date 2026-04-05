#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <memory>

#include "Buzzer.hpp"
#include "ClockApp.hpp"
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

    lv_display_t* disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "[clock] ERROR: failed to create fbdev display\n");
        return 1;
    }
    lv_result_t res = lv_linux_fbdev_set_file(disp, "/dev/fb0");
    if (res != LV_RESULT_OK) {
        fprintf(stderr, "[clock] ERROR: failed to open /dev/fb0\n");
        return 1;
    }

    // Initialise the KY-012 buzzer via direct GPIO (sysfs, GPIO14 / BCM 14).
    std::unique_ptr<Buzzer> buzzer;
    try {
        buzzer = std::make_unique<Buzzer>(14);
        fprintf(stderr, "[clock] Buzzer ready on GPIO14\n");
    } catch (const std::exception& e) {
        fprintf(stderr, "[clock] WARNING: buzzer unavailable: %s\n", e.what());
    }

    ClockApp app(buzzer.get());
    app.init();

    int last_second = -1;
    while (true) {
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