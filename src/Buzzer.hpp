#pragma once
/**
 * @file Buzzer.hpp
 * @brief KY-012 active buzzer driver via libgpiod (userspace GPIO).
 *
 * Uses the libgpiod C API to drive GPIO14 (BCM, physical pin 8) directly
 * from userspace.  No kernel driver or device tree overlay is required.
 *
 * libgpiod is the modern, kernel-recommended replacement for the deprecated
 * sysfs GPIO interface (/sys/class/gpio).
 *
 * Build dependency:
 *   - Link against: -lgpiod
 *   - Yocto recipe:  DEPENDS += "libgpiod"
 *
 * @example
 *   Buzzer bz;          // default: /dev/gpiochip0, GPIO14
 *   bz.beep(200);       // 200 ms non-blocking beep
 */

#include <gpiod.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include "lvgl/lvgl.h"

class Buzzer {
   public:
    /**
     * @brief Open a gpiochip and request the given line as output.
     * @param chip_path  gpiochip device node (default: "/dev/gpiochip0").
     * @param gpio_pin   BCM GPIO line offset (default: 14).
     * @throws std::runtime_error if the chip or line cannot be opened.
     */
    explicit Buzzer(const char* chip_path = "/dev/gpiochip0", unsigned int gpio_pin = 14) {
        chip_ = gpiod_chip_open(chip_path);
        if (!chip_)
            throw std::runtime_error(std::string("Buzzer: cannot open ") +
                                     chip_path + ": " + std::strerror(errno));

        line_ = gpiod_chip_get_line(chip_, gpio_pin);
        if (!line_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            throw std::runtime_error("Buzzer: cannot get GPIO line " +
                                     std::to_string(gpio_pin));
        }

        // Request the line as output, initially LOW (buzzer off).
        if (gpiod_line_request_output(line_, "clockos-buzzer", 0) < 0) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            throw std::runtime_error("Buzzer: cannot request GPIO" +
                                     std::to_string(gpio_pin) +
                                     " as output: " + std::strerror(errno));
        }
    }

    ~Buzzer() {
        if (off_timer_) {
            lv_timer_delete(off_timer_);
            off_timer_ = nullptr;
        }
        set_line_(false);
        if (line_) {
            gpiod_line_release(line_);
            line_ = nullptr;
        }
        if (chip_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    Buzzer(const Buzzer&)            = delete;
    Buzzer& operator=(const Buzzer&) = delete;

    /**
     * @brief Generate a non-blocking beep of the given duration.
     * @param duration_ms  Beep length in milliseconds (must be > 0).
     *
     * If a beep is already in progress the off-timer is restarted, extending
     * the beep duration.
     */
    void beep(int duration_ms) {
        if (!line_ || duration_ms <= 0)
            return;

        if (off_timer_) {
            lv_timer_delete(off_timer_);
            off_timer_ = nullptr;
        }

        set_line_(true);

        off_timer_ =
            lv_timer_create(beep_off_cb_, static_cast<uint32_t>(duration_ms), this);
        lv_timer_set_repeat_count(off_timer_, 1);
    }

    /** @return true if the GPIO line was claimed successfully. */
    bool ok() const { return line_ != nullptr; }

   private:
    void set_line_(bool on) {
        if (line_)
            gpiod_line_set_value(line_, on ? 1 : 0);
    }

    static void beep_off_cb_(lv_timer_t* t) {
        Buzzer* self     = static_cast<Buzzer*>(lv_timer_get_user_data(t));
        self->off_timer_ = nullptr;
        self->set_line_(false);
    }

    gpiod_chip* chip_      = nullptr;
    gpiod_line* line_      = nullptr;
    lv_timer_t* off_timer_ = nullptr;
};