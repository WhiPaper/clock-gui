    #pragma once
    /**
    * @file Buzzer.hpp
    * @brief KY-012 active buzzer driver via libgpiod v2 (userspace GPIO).
    *
    * Uses the libgpiod v2 C API to drive GPIO14 (BCM, physical pin 8) directly
    * from userspace.  No kernel driver or device tree overlay is required.
    *
    * libgpiod v2 (≥ 2.0) is the API shipped by Yocto scarthgap (meta-oe).
    * The v1 API (gpiod_chip_get_line, gpiod_line_request_output, etc.) was
    * removed in v2 — this file uses the new request-based interface.
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
        * @brief Open a gpiochip and request the given line as output (libgpiod v2).
        * @param chip_path  gpiochip device node (default: "/dev/gpiochip0").
        * @param gpio_pin   BCM GPIO line offset (default: 14).
        * @throws std::runtime_error if the chip or line cannot be opened.
        */
        explicit Buzzer(const char* chip_path = "/dev/gpiochip0", unsigned int gpio_pin = 14) {
            // --- Open chip ---
            chip_ = gpiod_chip_open(chip_path);
            if (!chip_)
                throw std::runtime_error(std::string("Buzzer: cannot open ") +
                                        chip_path + ": " + std::strerror(errno));

            // --- Build line settings: output, initially LOW ---
            gpiod_line_settings* settings = gpiod_line_settings_new();
            if (!settings) {
                gpiod_chip_close(chip_);
                chip_ = nullptr;
                throw std::runtime_error("Buzzer: gpiod_line_settings_new() failed");
            }
            gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
            gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

            // --- Build request config ---
            gpiod_request_config* req_cfg = gpiod_request_config_new();
            if (!req_cfg) {
                gpiod_line_settings_free(settings);
                gpiod_chip_close(chip_);
                chip_ = nullptr;
                throw std::runtime_error("Buzzer: gpiod_request_config_new() failed");
            }
            gpiod_request_config_set_consumer(req_cfg, "clockos-buzzer");

            // --- Build line config ---
            gpiod_line_config* line_cfg = gpiod_line_config_new();
            if (!line_cfg) {
                gpiod_request_config_free(req_cfg);
                gpiod_line_settings_free(settings);
                gpiod_chip_close(chip_);
                chip_ = nullptr;
                throw std::runtime_error("Buzzer: gpiod_line_config_new() failed");
            }

            unsigned int offsets[1] = {gpio_pin};
            if (gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings) < 0) {
                gpiod_line_config_free(line_cfg);
                gpiod_request_config_free(req_cfg);
                gpiod_line_settings_free(settings);
                gpiod_chip_close(chip_);
                chip_ = nullptr;
                throw std::runtime_error("Buzzer: gpiod_line_config_add_line_settings() failed: " +
                                        std::string(std::strerror(errno)));
            }

            // --- Request the lines ---
            request_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);

            // Cleanup intermediate objects (not needed after request)
            gpiod_line_config_free(line_cfg);
            gpiod_request_config_free(req_cfg);
            gpiod_line_settings_free(settings);

            if (!request_) {
                gpiod_chip_close(chip_);
                chip_ = nullptr;
                throw std::runtime_error("Buzzer: cannot request GPIO" +
                                        std::to_string(gpio_pin) +
                                        " as output: " + std::strerror(errno));
            }

            gpio_pin_ = gpio_pin;
        }

        ~Buzzer() {
            if (off_timer_) {
                lv_timer_delete(off_timer_);
                off_timer_ = nullptr;
            }
            set_line_(false);
            if (request_) {
                gpiod_line_request_release(request_);
                request_ = nullptr;
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
            if (!request_ || duration_ms <= 0)
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
        bool ok() const { return request_ != nullptr; }

    private:
        void set_line_(bool on) {
            if (!request_) return;
            const enum gpiod_line_value val =
                on ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
            unsigned int offsets[1] = {gpio_pin_};
            enum gpiod_line_value   values[1]  = {val};
            gpiod_line_request_set_values_subset(request_, 1, offsets, values);
        }

        static void beep_off_cb_(lv_timer_t* t) {
            Buzzer* self     = static_cast<Buzzer*>(lv_timer_get_user_data(t));
            self->off_timer_ = nullptr;
            self->set_line_(false);
        }

        gpiod_chip*         chip_      = nullptr;
        gpiod_line_request* request_   = nullptr;
        unsigned int        gpio_pin_  = 14;
        lv_timer_t*         off_timer_ = nullptr;
    };