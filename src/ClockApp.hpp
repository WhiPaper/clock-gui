#pragma once
// Defines the main clock user interface using LVGL 9.5.

#include <cstdio>
#include <ctime>


#include "DrowsinessBridge.hpp"
#include "QrOverlay.hpp"
#include "lvgl/lvgl.h"
#include "sleepcare_proto.h"

class ClockApp {
   public:
    explicit ClockApp() {}

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

        // HR / score info label
        info_label_ = lv_label_create(cont);
        lv_obj_set_style_text_color(info_label_, lv_color_hex(0x5599FF), 0);
        lv_obj_set_style_text_font(info_label_, &lv_font_montserrat_14, 0);
        lv_label_set_text(info_label_, "");

        alert_label_ = lv_label_create(cont);
        lv_obj_set_style_text_font(alert_label_, &lv_font_montserrat_20, 0);
        lv_label_set_text(alert_label_, "");
    }

    /** Called from main loop on every new RiskFrame from sleepcare-ws. */
    void apply_risk(const RiskBridge& bridge) {
        uint8_t state       = bridge.state();
        uint8_t alert_level = bridge.alert_level();
        uint8_t qr          = bridge.qr_ready();
        float   fused       = bridge.fused_score();
        uint16_t bpm        = bridge.hr_bpm();

        // --- Update info label ---
        char info_buf[48];
        if (bpm > 0)
            std::snprintf(info_buf, sizeof(info_buf), "BPM %u  fused %.2f", bpm, fused);
        else
            std::snprintf(info_buf, sizeof(info_buf), "fused %.2f", fused);
        lv_label_set_text(info_label_, info_buf);

        // --- Update alert label color + text based on state & level ---
        switch (state) {
            case SC_STATE_SUSPECT:
                lv_obj_set_style_text_color(alert_label_, lv_color_hex(0xFF9500), 0); // orange
                lv_label_set_text(alert_label_, "주의");
                break;
            case SC_STATE_ALERTING:
                if (alert_level == 3) {
                    lv_obj_set_style_text_color(alert_label_, lv_color_hex(0xFF0000), 0); // pure red
                    lv_label_set_text(alert_label_, "일어나세요!!!");
                } else if (alert_level == 2) {
                    lv_obj_set_style_text_color(alert_label_, lv_color_hex(0xFF3B30), 0); // slightly lighter red
                    lv_label_set_text(alert_label_, "졸음 감지!");
                } else {
                    lv_obj_set_style_text_color(alert_label_, lv_color_hex(0xFF9500), 0); // orange-red
                    lv_label_set_text(alert_label_, "주의");
                }
                break;
            case SC_STATE_COOLDOWN:
                lv_obj_set_style_text_color(alert_label_, lv_color_hex(0xFFCC00), 0); // yellow
                lv_label_set_text(alert_label_, "회복 중");
                break;
            default:
                lv_label_set_text(alert_label_, "");
                break;
        }

        // --- QR overlay ---
        if (qr && !qr_overlay_.is_visible()) {
            qr_overlay_.show();
        } else if (!qr && qr_overlay_.is_visible()) {
            qr_overlay_.hide();
        }
    }

    void update(const struct tm& t) {
        char time_buf[16];
        std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
                      t.tm_hour, t.tm_min, t.tm_sec);
        lv_label_set_text(time_label_, time_buf);

        static const char* day_names[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        char date_buf[32];
        std::snprintf(date_buf, sizeof(date_buf), "%04d.%02d.%02d (%s)",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      day_names[t.tm_wday]);
        lv_label_set_text(date_label_, date_buf);


    }

   private:

    QrOverlay   qr_overlay_;
    lv_obj_t*   time_label_  = nullptr;
    lv_obj_t*   date_label_  = nullptr;
    lv_obj_t*   info_label_  = nullptr;
    lv_obj_t*   alert_label_ = nullptr;
};