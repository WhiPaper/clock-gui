#pragma once
/**
 * @file Buzzer.hpp
 * @brief KY-012 active buzzer driver via the kernel gpio-beeper input device.
 *
 * The gpio-beeper kernel driver exposes the buzzer as a standard Linux input
 * device (EV_SND / SND_BELL).  This class opens that device node and sends
 * input_event packets to turn the buzzer on/off, with automatic off-scheduling
 * via a one-shot LVGL timer.
 *
 * No libgpiod dependency.  The device is opened non-blocking so that the
 * write() calls never stall the LVGL main loop.
 *
 * @example
 *   Buzzer bz("/dev/input/event0");   // or use Buzzer::find() to auto-detect
 *   bz.beep(200);                     // 200 ms beep, non-blocking
 *
 * @note
 *   To find the correct event node at runtime:
 *     grep -rl "gpio-beeper" /sys/class/input/<N>/device/name
 *   or equivalently, use Buzzer::find() below.
 */

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include "lvgl/lvgl.h"

class Buzzer {
   public:
    /**
     * @brief Open a gpio-beeper input device by its event node path.
     * @param dev_path  Path such as "/dev/input/event0".
     * @throws std::runtime_error if the device cannot be opened.
     */
    explicit Buzzer(const std::string& dev_path) {
        fd_ = ::open(dev_path.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd_ < 0)
            throw std::runtime_error("Buzzer: cannot open " + dev_path + ": " +
                                     std::strerror(errno));
    }

    ~Buzzer() {
        // Cancel any pending asynchronous turn-off timer.
        if (off_timer_) {
            lv_timer_delete(off_timer_);
            off_timer_ = nullptr;
        }
        // Ensure buzzer is silenced before closing.
        send_bell_(false);
        if (fd_ >= 0)
            ::close(fd_);
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
        if (fd_ < 0 || duration_ms <= 0)
            return;

        // Reset any pending off-timer.
        if (off_timer_) {
            lv_timer_delete(off_timer_);
            off_timer_ = nullptr;
        }

        send_bell_(true);

        off_timer_ = lv_timer_create(beep_off_cb_, static_cast<uint32_t>(duration_ms), this);
        lv_timer_set_repeat_count(off_timer_, 1);
    }

    /** @return true if the device was opened successfully. */
    bool ok() const { return fd_ >= 0; }

    /**
     * @brief Find the first input device whose name is "gpio-beeper".
     *
     * Scans /dev/input/event0 … event31.  Returns the path of the first
     * matching node, or an empty string if none is found.
     *
     * Usage:
     *   std::string path = Buzzer::find();
     *   if (!path.empty()) buzzer = std::make_unique<Buzzer>(path);
     */
    static std::string find() {
        char name[256];
        for (int i = 0; i < 32; ++i) {
            std::string path = "/dev/input/event" + std::to_string(i);
            int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0)
                continue;
            name[0] = '\0';
            ::ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
            ::close(fd);
            if (std::string(name) == "gpio-beeper")
                return path;
        }
        return {};
    }

   private:
    /**
     * @brief Write an EV_SND / SND_BELL input event to the device.
     * @param on  true = beep on, false = beep off.
     */
    void send_bell_(bool on) {
        if (fd_ < 0)
            return;

        // Two events: the sound event itself, then a synchronisation report.
        struct input_event events[2]{};

        events[0].type  = EV_SND;
        events[0].code  = SND_BELL;
        events[0].value = on ? 1 : 0;

        events[1].type  = EV_SYN;
        events[1].code  = SYN_REPORT;
        events[1].value = 0;

        // Non-blocking write; ignore EAGAIN (buffer full) safely.
        ::write(fd_, events, sizeof(events));
    }

    static void beep_off_cb_(lv_timer_t* t) {
        Buzzer* self     = static_cast<Buzzer*>(lv_timer_get_user_data(t));
        self->off_timer_ = nullptr;
        self->send_bell_(false);
    }

    int         fd_        = -1;
    lv_timer_t* off_timer_ = nullptr;
};