#pragma once
// Defines the main clock user interface using LVGL 9.5.

#include <cstdio>
#include <ctime>

#include "Buzzer.hpp"
#include "lvgl/lvgl.h"

class ClockApp {
   public:
    // Initialize the application with an optional buzzer. The caller retains ownership of the
    // buzzer.
    explicit ClockApp(Buzzer* buzzer = nullptr) : buzzer_(buzzer) {}

    void init() {
        lv_obj_t* screen = lv_screen_active();

        lv_obj_set_style_bg_color(screen, lv_color_hex(0x0A0A12), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t* cont = lv_obj_create(screen);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_flex_main_place(cont, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(cont, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_row(cont, 4, 0);

        time_label_ = lv_label_create(cont);
        lv_obj_set_style_text_color(time_label_, lv_color_hex(0xF0F0F0), 0);
        lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_20, 0);
        lv_label_set_text(time_label_, "00:00:00");

        date_label_ = lv_label_create(cont);
        lv_obj_set_style_text_color(date_label_, lv_color_hex(0x8888AA), 0);
        lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_14, 0);
        lv_label_set_text(date_label_, "0000.00.00 (---)");
    }

    void update(const struct tm& t) {
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        lv_label_set_text(time_label_, time_buf);

        static const char* day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        char               date_buf[32];
        snprintf(date_buf, sizeof(date_buf), "%04d.%02d.%02d (%s)", t.tm_year + 1900, t.tm_mon + 1,
                 t.tm_mday, day_names[t.tm_wday]);
        lv_label_set_text(date_label_, date_buf);

        // Trigger audible time signals for testing.
        if (buzzer_) {
            buzzer_->beep(50);
        }
    }

   private:
    Buzzer*   buzzer_     = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
};