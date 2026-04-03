#pragma once
/**
 * @file Buzzer.hpp
 * @brief GPIO buzzer driver utilizing libgpiod v2 and LVGL asynchronous timers.
 *
 * The beep() method is non-blocking. It activates the GPIO line and schedules
 * an automatic deactivation using a one-shot LVGL timer.
 *
 * @example
 *   Buzzer bz("/dev/gpiochip0", 17);
 *   bz.beep(200); // Generate a 200-millisecond beep.
 */

#include <gpiod.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

#include "lvgl/lvgl.h"

class Buzzer {
   public:
    /**
     * @brief Construct a new Buzzer object.
     * @param chip_path The path to the GPIO character device (e.g., "/dev/gpiochip0").
     * @param line_offset The GPIO line number to control on the specified chip.
     */
    Buzzer(const std::string& chip_path, unsigned int line_offset) {
        chip_ = gpiod_chip_open(chip_path.c_str());
        if (!chip_)
            throw std::runtime_error("Buzzer: cannot open " + chip_path);

        gpiod_line_settings* settings = gpiod_line_settings_new();
        if (!settings) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            throw std::runtime_error("Buzzer: gpiod_line_settings_new failed");
        }

        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

        gpiod_line_config* line_cfg = gpiod_line_config_new();
        if (!line_cfg) {
            gpiod_line_settings_free(settings);
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            throw std::runtime_error("Buzzer: gpiod_line_config_new failed");
        }

        gpiod_line_config_add_line_settings(line_cfg, &line_offset, 1, settings);
        gpiod_line_settings_free(settings);

        gpiod_request_config* req_cfg = gpiod_request_config_new();
        if (req_cfg)
            gpiod_request_config_set_consumer(req_cfg, "buzzer");

        request_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);

        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);

        if (!request_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            throw std::runtime_error("Buzzer: gpiod_chip_request_lines failed");
        }

        line_offset_ = line_offset;
    }

    ~Buzzer() {
        // Cancel any pending asynchronous turn-off timers before releasing hardware.
        if (off_timer_) {
            lv_timer_delete(off_timer_);
            off_timer_ = nullptr;
        }
        if (request_) {
            gpiod_line_request_set_value(request_, line_offset_, GPIOD_LINE_VALUE_INACTIVE);
            gpiod_line_request_release(request_);
        }
        if (chip_)
            gpiod_chip_close(chip_);
    }

    Buzzer(const Buzzer&)            = delete;
    Buzzer& operator=(const Buzzer&) = delete;

    void beep(int duration_ms) {
        if (!request_ || duration_ms <= 0)
            return;

        // Cancel and restart the off-timer if one is already armed.
        if (off_timer_) {
            lv_timer_delete(off_timer_);
            off_timer_ = nullptr;
        }

        gpiod_line_request_set_value(request_, line_offset_, GPIOD_LINE_VALUE_ACTIVE);

        off_timer_ = lv_timer_create(beep_off_cb_, static_cast<uint32_t>(duration_ms), this);
        lv_timer_set_repeat_count(off_timer_, 1);
    }

    bool ok() const {
        return request_ != nullptr;
    }

   private:
    static void beep_off_cb_(lv_timer_t* t) {
        Buzzer* self     = static_cast<Buzzer*>(lv_timer_get_user_data(t));
        self->off_timer_ = nullptr;
        gpiod_line_request_set_value(self->request_, self->line_offset_, GPIOD_LINE_VALUE_INACTIVE);
    }

    gpiod_chip*         chip_        = nullptr;
    gpiod_line_request* request_     = nullptr;
    lv_timer_t*         off_timer_   = nullptr;
    unsigned int        line_offset_ = 0;
};