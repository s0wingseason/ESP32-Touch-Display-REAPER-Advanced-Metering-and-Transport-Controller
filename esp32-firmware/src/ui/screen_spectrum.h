/*
 * MeterBridge — Spectrum Analyzer Screen
 *
 * 16-band FFT spectrum display inspired by TC Electronic Clarity M
 * and iZotope Insight: gradient-colored vertical bars, dB gridlines,
 * peak hold dots, frequency labels, and theme-aware color mapping.
 */

#ifndef SCREEN_SPECTRUM_H
#define SCREEN_SPECTRUM_H

#include "../network/udp_comm.h"
#include "theme.h"
#include <lvgl.h>
#include <Preferences.h>

/* Back button defined in ui_manager.h — navigates back to Meters */
extern void _settings_back_from_settings(lv_event_t* e);
extern uint8_t g_spec_color_mode;

/* ── Configuration ──────────────────────────────────────────── */

#define SPEC_BAND_COUNT     16
#define SPEC_DB_MIN         -60.0f
#define SPEC_DB_MAX         0.0f
#define SPEC_DB_RANGE       (SPEC_DB_MAX - SPEC_DB_MIN)
#define SPEC_BAR_GAP        4
#define SPEC_LABEL_H        16
#define SPEC_PEAK_HOLD_MS   2000
#define SPEC_GRID_DB_COUNT  7

typedef enum {
    SPEC_MODE_CLASSIC = 0,    /* Green -> Yellow -> Orange -> Red */
    SPEC_MODE_NEON    = 1,    /* Purple -> Magenta -> Cyan -> Blue */
    SPEC_MODE_FIRE    = 2,    /* Red -> Orange -> Yellow -> White */
    SPEC_MODE_OCEAN   = 3,    /* Blue -> Cyan -> Seafoam -> White */
    SPEC_MODE_RAINBOW = 4,    /* Full rainbow spread */
    SPEC_MODE_GHOST   = 5,    /* Monochrome accent only */
    SPEC_MODE_COUNT   = 6
} mb_spec_mode_t;

static const char* _spec_band_labels[SPEC_BAND_COUNT] = {
    "31", "44", "63", "88", "125", "177", "250", "354",
    "500", "707", "1k", "1.4k", "2k", "4k", "8k", "16k"
};

/* dB gridline values */
static const float _spec_grid_db[SPEC_GRID_DB_COUNT] = {
    -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f
};
static const char* _spec_grid_labels[SPEC_GRID_DB_COUNT] = {
    "-6", "-12", "-18", "-24", "-36", "-48", "-60"
};

/* ── UI Elements ────────────────────────────────────────────── */

typedef struct {
    lv_obj_t* bar_bg;      /* Background rectangle (well) */
    lv_obj_t* bar_fill;    /* Filled portion */
    lv_obj_t* peak_dot;    /* Peak hold indicator */
    lv_obj_t* label;       /* Frequency label below */
    float     current_db;
    float     peak_db;
    uint32_t  peak_time;
    int16_t   last_h;      /* Dirty-bit: last rendered height */
    int16_t   last_peak_y; /* Dirty-bit: last peak Y position */
    uint8_t   last_mode;   /* Dirty-bit for color mode */
} spec_bar_t;

static spec_bar_t _spec_bars[SPEC_BAND_COUNT];
static lv_obj_t* _spec_container = NULL;
static lv_obj_t* _spec_title_lbl = NULL;
static lv_obj_t* _spec_peak_info_lbl = NULL;
static uint32_t  _spec_last_update_ms = 0;
#define SPEC_UPDATE_INTERVAL_MS 40  /* 25Hz — smooth performance */

/* ── Extern globals ─────────────────────────────────────────── */
extern float    g_loudness_target;
extern uint8_t  g_loudness_preset;

/* ── Theme-aware color mapping ──────────────────────────────── */

static lv_color_t _spec_get_band_color(int b_idx, int mode) {
    switch (mode) {
        case SPEC_MODE_NEON:
            if      (b_idx < 4)  return lv_color_hex(0xFF00FF); /* Magenta */
            else if (b_idx < 8)  return lv_color_hex(0x9D00FF); /* Purple */
            else if (b_idx < 12) return lv_color_hex(0x00FFFF); /* Cyan */
            else                 return lv_color_hex(0x0080FF); /* Blue */
        
        case SPEC_MODE_FIRE:
            if      (b_idx < 4)  return lv_color_hex(0x800000); /* Dark Red */
            else if (b_idx < 8)  return lv_color_hex(0xFF4500); /* Orange Red */
            else if (b_idx < 12) return lv_color_hex(0xFFD700); /* Gold */
            else                 return lv_color_hex(0xFFFFFF); /* White */

        case SPEC_MODE_OCEAN:
            if      (b_idx < 4)  return lv_color_hex(0x000080); /* Navy */
            else if (b_idx < 8)  return lv_color_hex(0x008080); /* Teal */
            else if (b_idx < 12) return lv_color_hex(0x20B2AA); /* Light Sea Green */
            else                 return lv_color_hex(0xE0FFFF); /* Light Cyan */

        case SPEC_MODE_RAINBOW: {
            /* 16-step rainbow */
            static const uint32_t rb[] = {
                0xFF0000, 0xFF4000, 0xFF8000, 0xFFBF00,
                0xFFFF00, 0xBFFF00, 0x80FF00, 0x40FF00,
                0x00FF00, 0x00FF40, 0x00FF80, 0x00FFBF,
                0x00FFFF, 0x00BFFF, 0x0080FF, 0x0040FF
            };
            return lv_color_hex(rb[b_idx]);
        }

        case SPEC_MODE_GHOST:
            return MB_COLOR_ACCENT_SEAFOAM;

        case SPEC_MODE_CLASSIC:
        default:
            if      (b_idx < 4)  return MB_COLOR_METER_GREEN;
            else if (b_idx < 8)  return MB_COLOR_METER_YELLOW;
            else if (b_idx < 12) return MB_COLOR_METER_ORANGE;
            else                 return MB_COLOR_METER_RED;
    }
}

/* ── Shared UI Refresh ──────────────────────────────────────── */

static void _spec_refresh_ui(void) {
    /* Force refresh colors in next update by invalidating last_mode */
    for (int i = 0; i < SPEC_BAND_COUNT; i++) _spec_bars[i].last_mode = 255;
    
    /* UI feedback: update title with mode name */
    if (_spec_title_lbl) {
        static const char* mode_names[] = {"CLASSIC", "NEON", "FIRE", "OCEAN", "RAINBOW", "GHOST"};
        lv_label_set_text_fmt(_spec_title_lbl, LV_SYMBOL_AUDIO "  SPECTRUM: %s", mode_names[g_spec_color_mode]);
    }
}

/* ── Tap to cycle modes ─────────────────────────────────────── */

static void _spec_touch_cb(lv_event_t* e) {
    g_spec_color_mode = (g_spec_color_mode + 1) % SPEC_MODE_COUNT;

    /* Sticky save to NVS */
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("spec_mode", g_spec_color_mode);
    prefs.end();

    _spec_refresh_ui();
}

/* ── Create Spectrum Screen ─────────────────────────────────── */

void ui_create_spectrum_screen(lv_obj_t* parent, meter_state_t* state) {
    (void)state;

    _spec_container = lv_obj_create(parent);
    lv_obj_remove_style_all(_spec_container);
    lv_obj_set_size(_spec_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(_spec_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_spec_container, MB_COLOR_BG_PRIMARY, 0);
    lv_obj_clear_flag(_spec_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_spec_container, _spec_touch_cb, LV_EVENT_CLICKED, NULL);

    lv_coord_t disp_w = lv_disp_get_hor_res(NULL);
    lv_coord_t disp_h = lv_disp_get_ver_res(NULL);
    bool portrait = (disp_w < disp_h);

    lv_coord_t pad = portrait ? 4 : 10;
    lv_coord_t top_bar_h = 32;
    lv_coord_t db_axis_w = portrait ? 22 : 30;
    lv_coord_t freq_axis_h = 20;
    lv_coord_t bot_pad = 8;

    lv_coord_t bars_x = pad + db_axis_w;
    lv_coord_t bars_y = pad + top_bar_h + 4;
    lv_coord_t bars_w = disp_w - bars_x - pad;
    lv_coord_t bars_h = disp_h - bars_y - freq_axis_h - bot_pad;
    
    int gap = portrait ? 2 : SPEC_BAR_GAP;
    lv_coord_t bar_w = (bars_w - (SPEC_BAND_COUNT - 1) * gap) / SPEC_BAND_COUNT;

    _spec_title_lbl = lv_label_create(_spec_container);
    lv_label_set_text(_spec_title_lbl, LV_SYMBOL_AUDIO "  SPECTRUM   ANALYZER");
    lv_obj_set_style_text_color(_spec_title_lbl, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(_spec_title_lbl, MB_FONT_LABEL, 0);
    lv_obj_set_style_text_letter_space(_spec_title_lbl, 2, 0);
    lv_obj_set_pos(_spec_title_lbl, pad, pad + 6);

    _spec_peak_info_lbl = lv_label_create(_spec_container);
    lv_label_set_text(_spec_peak_info_lbl, "Peak:  ---");
    lv_obj_set_style_text_color(_spec_peak_info_lbl, MB_COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_text_font(_spec_peak_info_lbl, MB_FONT_SMALL, 0);
    
    if (portrait) {
        lv_obj_align(_spec_peak_info_lbl, LV_ALIGN_TOP_LEFT, pad, pad + 24);
    } else {
        lv_obj_align(_spec_peak_info_lbl, LV_ALIGN_TOP_MID, 40, pad + 8);
    }

    lv_obj_t* back_btn = lv_btn_create(_spec_container);
    int btn_w = portrait ? 70 : 84;
    lv_obj_set_size(back_btn, btn_w, 28);
    lv_obj_set_pos(back_btn, disp_w - pad - btn_w, pad + 2);
    lv_obj_set_style_bg_color(back_btn, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_add_event_cb(back_btn, _settings_back_from_settings, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, portrait ? "BACK" : LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(back_lbl, MB_FONT_SMALL, 0);
    lv_obj_center(back_lbl);

    for (int g = 0; g < SPEC_GRID_DB_COUNT; g++) {
        float ratio = (_spec_grid_db[g] - SPEC_DB_MIN) / SPEC_DB_RANGE;
        lv_coord_t gy = bars_y + bars_h - (lv_coord_t)(ratio * bars_h);
        lv_obj_t* line = lv_obj_create(_spec_container);
        lv_obj_remove_style_all(line);
        lv_obj_set_pos(line, bars_x, gy);
        lv_obj_set_size(line, bars_w, 1);
        lv_obj_set_style_bg_opa(line, LV_OPA_20, 0);
        lv_obj_set_style_bg_color(line, MB_COLOR_BORDER, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* db_lbl = lv_label_create(_spec_container);
        lv_label_set_text(db_lbl, _spec_grid_labels[g]);
        lv_obj_set_style_text_color(db_lbl, MB_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_font(db_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(db_lbl, (portrait ? 2 : pad), gy - 5);
    }

    for (int i = 0; i < SPEC_BAND_COUNT; i++) {
        lv_coord_t x = bars_x + i * (bar_w + gap);

        _spec_bars[i].bar_bg = lv_obj_create(_spec_container);
        lv_obj_remove_style_all(_spec_bars[i].bar_bg);
        lv_obj_set_pos(_spec_bars[i].bar_bg, x, bars_y);
        lv_obj_set_size(_spec_bars[i].bar_bg, bar_w, bars_h);
        lv_obj_set_style_bg_opa(_spec_bars[i].bar_bg, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(_spec_bars[i].bar_bg, MB_COLOR_METER_BG, 0);
        lv_obj_set_style_radius(_spec_bars[i].bar_bg, 1, 0);
        lv_obj_clear_flag(_spec_bars[i].bar_bg, LV_OBJ_FLAG_SCROLLABLE);

        _spec_bars[i].bar_fill = lv_obj_create(_spec_bars[i].bar_bg);
        lv_obj_remove_style_all(_spec_bars[i].bar_fill);
        lv_obj_set_style_bg_opa(_spec_bars[i].bar_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_grad_color(_spec_bars[i].bar_fill, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_grad_dir(_spec_bars[i].bar_fill, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_main_stop(_spec_bars[i].bar_fill, 0, 0);
        lv_obj_set_style_bg_grad_stop(_spec_bars[i].bar_fill, 180, 0);
        lv_obj_set_pos(_spec_bars[i].bar_fill, 0, bars_h);
        lv_obj_set_size(_spec_bars[i].bar_fill, bar_w, 0);
        lv_obj_clear_flag(_spec_bars[i].bar_fill, LV_OBJ_FLAG_SCROLLABLE);

        _spec_bars[i].peak_dot = lv_obj_create(_spec_bars[i].bar_bg);
        lv_obj_remove_style_all(_spec_bars[i].peak_dot);
        lv_obj_set_size(_spec_bars[i].peak_dot, bar_w, 2);
        lv_obj_set_style_bg_opa(_spec_bars[i].peak_dot, LV_OPA_90, 0);
        lv_obj_set_style_bg_color(_spec_bars[i].peak_dot, MB_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_pos(_spec_bars[i].peak_dot, 0, bars_h - 2);

        _spec_bars[i].label = lv_label_create(_spec_container);
        lv_label_set_text(_spec_bars[i].label, _spec_band_labels[i]);
        lv_obj_set_style_text_color(_spec_bars[i].label, MB_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_font(_spec_bars[i].label, portrait ? MB_FONT_SMALL : &lv_font_montserrat_10, 0);
        lv_obj_set_pos(_spec_bars[i].label, x + (bar_w/2) - (portrait ? 4 : 8), bars_y + bars_h + 4);

        _spec_bars[i].current_db = SPEC_DB_MIN;
        _spec_bars[i].peak_db = SPEC_DB_MIN;
        _spec_bars[i].last_h = -1;
        _spec_bars[i].last_peak_y = -1;
        _spec_bars[i].last_mode = 255;
    }
}

void ui_update_spectrum_screen(meter_state_t* state) {
    if (!_spec_container) return;

    uint32_t now = millis();
    if (now - _spec_last_update_ms < SPEC_UPDATE_INTERVAL_MS) return;
    _spec_last_update_ms = now;

    lv_coord_t disp_w = lv_disp_get_hor_res(NULL);
    lv_coord_t disp_h = lv_disp_get_ver_res(NULL);
    bool portrait = (disp_w < disp_h);

    const lv_coord_t bars_y_base = (portrait ? 4 : 10) + 32 + 4;
    const lv_coord_t bars_h = disp_h - bars_y_base - 20 - 8;

    int peak_band_idx = 0;
    float peak_band_db = SPEC_DB_MIN;

    for (int i = 0; i < SPEC_BAND_COUNT; i++) {
        float target_db = state->spectrum_bands[i];
        if (isnan(target_db) || isinf(target_db)) target_db = SPEC_DB_MIN;
        if (target_db < SPEC_DB_MIN) target_db = SPEC_DB_MIN;

        float cur = _spec_bars[i].current_db;
        if (target_db > cur) cur = target_db;
        else cur += (target_db - cur) * 0.20f;
        _spec_bars[i].current_db = cur;

        if (cur > peak_band_db) { peak_band_db = cur; peak_band_idx = i; }

        float ratio = (cur - SPEC_DB_MIN) / SPEC_DB_RANGE;
        int16_t bar_h = (int16_t)(ratio * bars_h);

        /* Mode/Color dirty check */
        if (g_spec_color_mode != _spec_bars[i].last_mode) {
            _spec_bars[i].last_mode = g_spec_color_mode;
            lv_color_t c = _spec_get_band_color(i, g_spec_color_mode);
            
            /* Ghost mode: translucent + no gradient interference */
            if (g_spec_color_mode == SPEC_MODE_GHOST) {
                lv_obj_set_style_bg_opa(_spec_bars[i].bar_fill, LV_OPA_40, 0);
                lv_obj_set_style_bg_grad_dir(_spec_bars[i].bar_fill, LV_GRAD_DIR_NONE, 0);
            } else {
                lv_obj_set_style_bg_opa(_spec_bars[i].bar_fill, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_grad_dir(_spec_bars[i].bar_fill, LV_GRAD_DIR_VER, 0);
            }

            lv_obj_set_style_bg_color(_spec_bars[i].bar_fill, c, 0);
            /* Static shadow color to avoid CPU calc in update */
            lv_obj_set_style_shadow_color(_spec_bars[i].bar_fill, c, 0);
            lv_obj_set_style_shadow_width(_spec_bars[i].bar_fill, (g_spec_color_mode == SPEC_MODE_GHOST) ? 15 : 6, 0);
            lv_obj_set_style_shadow_opa(_spec_bars[i].bar_fill, (g_spec_color_mode == SPEC_MODE_GHOST) ? LV_OPA_50 : LV_OPA_30, 0);
        }

        if (bar_h != _spec_bars[i].last_h) {
            _spec_bars[i].last_h = bar_h;
            lv_obj_set_pos(_spec_bars[i].bar_fill, 0, bars_h - bar_h);
            lv_obj_set_height(_spec_bars[i].bar_fill, bar_h);
        }

        if (cur > _spec_bars[i].peak_db) {
            _spec_bars[i].peak_db = cur;
            _spec_bars[i].peak_time = now;
        } else if (now - _spec_bars[i].peak_time > SPEC_PEAK_HOLD_MS) {
            _spec_bars[i].peak_db += (SPEC_DB_MIN - _spec_bars[i].peak_db) * 0.05f;
        }

        float peak_ratio = (_spec_bars[i].peak_db - SPEC_DB_MIN) / SPEC_DB_RANGE;
        int16_t peak_y = bars_h - (int16_t)(peak_ratio * bars_h) - 2;
        if (peak_y < 0) peak_y = 0;
        if (peak_y != _spec_bars[i].last_peak_y) {
            _spec_bars[i].last_peak_y = peak_y;
            lv_obj_set_y(_spec_bars[i].peak_dot, peak_y);
        }
    }

    /* Update peak info label */
    if (_spec_peak_info_lbl) {
        static char peak_buf[40];
        if (peak_band_db > SPEC_DB_MIN + 1.0f) {
            snprintf(peak_buf, sizeof(peak_buf), "Peak: %sHz  %.1fdB",
                     _spec_band_labels[peak_band_idx], peak_band_db);
        } else {
            snprintf(peak_buf, sizeof(peak_buf), "Peak:  ---");
        }
        lv_label_set_text(_spec_peak_info_lbl, peak_buf);
    }
}

#endif /* SCREEN_SPECTRUM_H */
