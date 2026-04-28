#pragma once
/**
 * @file QrOverlay.hpp
 * @brief LVGL overlay that renders a QR code from /run/sleepcare/qr.bin.
 *
 * File format (written by sleepcare-ws / qr_writer.cpp):
 *   [4B] magic  "SCQR"
 *   [2B] width  uint16_t LE  (modules)
 *   [2B] height uint16_t LE  (modules)
 *   [N*M bytes] module data  1 byte per module: 1=black, 0=white
 *
 * Rendering: the overlay creates an lv_canvas sized to fit the display
 * with 4× pixel scaling of each QR module.
 */

#include <cstdio>
#include <cstring>
#include <vector>

#include "lvgl/lvgl.h"

class QrOverlay {
   public:
    static constexpr const char* kQrBinPath = "/run/sleepcare/qr.bin";
    static constexpr int         kScale     = 4;   /* pixels per QR module */

    explicit QrOverlay() = default;
    ~QrOverlay()         = default;

    /**
     * Read qr.bin and render the QR code as an LVGL canvas overlay.
     * Safe to call multiple times (re-renders on each call).
     */
    void show() {
        /* --- read file --------------------------------------------------- */
        FILE* f = std::fopen(kQrBinPath, "rb");
        if (!f) {
            std::fprintf(stderr, "[qr] cannot open %s\n", kQrBinPath);
            return;
        }

        char     magic[4] = {};
        uint16_t w = 0, h = 0;
        std::fread(magic, 1, 4, f);
        std::fread(&w, 2, 1, f);
        std::fread(&h, 2, 1, f);

        if (std::memcmp(magic, "SCQR", 4) != 0 || w == 0 || h == 0 ||
            w > 200 || h > 200) {
            std::fclose(f);
            std::fprintf(stderr, "[qr] invalid qr.bin header\n");
            return;
        }

        modules_.resize(static_cast<size_t>(w) * h);
        std::fread(modules_.data(), 1, modules_.size(), f);
        std::fclose(f);

        qr_w_ = w;
        qr_h_ = h;

        /* --- allocate canvas buffer (RGB565) ----------------------------- */
        int sw = w * kScale;
        int sh = h * kScale;
        canvas_buf_.assign(static_cast<size_t>(sw) * sh * 2, 0xFF); /* white */

        /* --- create / resize canvas ------------------------------------- */
        lv_obj_t* screen = lv_screen_active();
        if (!canvas_) {
            canvas_ = lv_canvas_create(screen);
            lv_obj_set_style_bg_color(canvas_, lv_color_white(), 0);
            lv_obj_align(canvas_, LV_ALIGN_CENTER, 0, 0);
        }
        lv_canvas_set_buffer(canvas_, canvas_buf_.data(), sw, sh,
                             LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(canvas_, lv_color_white(), LV_OPA_COVER);

        /* --- draw modules ----------------------------------------------- */
        lv_layer_t layer;
        lv_canvas_init_layer(canvas_, &layer);

        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color     = lv_color_black();
        dsc.bg_opa       = LV_OPA_COVER;
        dsc.border_width = 0;
        dsc.radius       = 0;

        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                if (modules_[static_cast<size_t>(row) * w + col] == 0) continue;

                lv_area_t area;
                area.x1 = col * kScale;
                area.y1 = row * kScale;
                area.x2 = area.x1 + kScale - 1;
                area.y2 = area.y1 + kScale - 1;
                lv_draw_rect(&layer, &dsc, &area);
            }
        }

        lv_canvas_finish_layer(canvas_, &layer);

        lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
    }

    void hide() {
        if (canvas_) lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
    }

    bool is_visible() const {
        return canvas_ && !lv_obj_has_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
    }

   private:
    lv_obj_t*             canvas_     = nullptr;
    std::vector<uint8_t>  canvas_buf_;
    std::vector<uint8_t>  modules_;
    uint16_t              qr_w_       = 0;
    uint16_t              qr_h_       = 0;
};
