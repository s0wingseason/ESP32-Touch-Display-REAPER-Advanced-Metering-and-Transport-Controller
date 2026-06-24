/*
 * MeterBridge — UI Manager
 * Manages LVGL screen lifecycle, status bar, transport bar, navigation.
 * 
 * Status Bar Layout (800px wide):
 *   [Transport Icon] [Measure|Beat] [Time] | [Track/Source Label] | [Project] [Section] | [LUFS-I] [WiFi]
 * 
 * Transport Bar Layout:
 *   [REW] [STOP] [PLAY] [REC] [FWD] | [REPEAT] [METRO] | Tempo TimeSig | [SETTINGS]
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include "theme.h"
#include "../network/udp_comm.h"
#include "../display_config.hpp"

/* Forward declarations */
void ui_create_meter_screen(lv_obj_t* parent, meter_state_t* state, UDPComm* udp);
void ui_update_meter_screen(meter_state_t* state);
void ui_create_settings_screen(lv_obj_t* parent, meter_state_t* state);
void ui_create_spectrum_screen(lv_obj_t* parent, meter_state_t* state);
void ui_update_spectrum_screen(meter_state_t* state);
void ui_show_toast(const char* msg);   /* defined later in this file */

typedef enum { SCREEN_METERS = 0, SCREEN_SPECTRUM, SCREEN_SETTINGS, SCREEN_COUNT } screen_id_t;

static lv_obj_t* _screens[SCREEN_COUNT];
static screen_id_t _current_screen = SCREEN_METERS;
static lv_obj_t* _main_container = NULL;

/* ─── Status Bar Elements ──────────────────────────────────────── */
static lv_obj_t* _lbl_transport_icon = NULL;  /* ▶ ⏸ ■ ⏺ icon */
static lv_obj_t* _lbl_measure = NULL;         /* Bar|Beat display */
static lv_obj_t* _lbl_time_pos = NULL;        /* Time display (M:SS.s) */
static lv_obj_t* _lbl_meter_src = NULL;       /* "MASTER" or "TRK: <name>" */
static lv_obj_t* _swatch_track_color = NULL;   /* Track color indicator dot  */
static lv_obj_t* _lbl_project_name = NULL;    /* Project name */
static lv_obj_t* _lbl_section_name = NULL;    /* Current region/section */
static lv_obj_t* _lbl_lufs_i = NULL;          /* Integrated LUFS */
static lv_obj_t* _lbl_conn = NULL;            /* WiFi status */
static lv_obj_t* _lbl_fps  = NULL;            /* FPS overlay — absolute-pos, far right */

/* ─── Transport Bar Elements ───────────────────────────────────── */
static lv_obj_t* _btn_rew = NULL;
static lv_obj_t* _btn_play = NULL;
static lv_obj_t* _btn_stop = NULL;
static lv_obj_t* _btn_fwd = NULL;
static lv_obj_t* _btn_rec = NULL;
static lv_obj_t* _btn_repeat = NULL;
static lv_obj_t* _btn_metro = NULL;
static lv_obj_t* _lbl_tempo = NULL;
static lv_obj_t* _lbl_tsig = NULL;

extern uint8_t  g_brightness;

static UDPComm* _udp_ref = NULL;
static meter_state_t* _state_ref = NULL;

/* ─── Main.cpp Globals used in this file ──────────────────────── */
extern uint8_t  g_conn_mode;
extern uint32_t g_last_serial_data_ms;
extern bool     g_show_fps;
extern float    g_fps_display;
extern bool     g_ota_in_progress;
extern uint8_t  g_ota_progress;
extern float    g_loudness_target;
extern uint8_t  g_loudness_preset;
extern float    g_master_volume_db;

/* ─── Callbacks ────────────────────────────────────────────────── */

static void _transport_cb(lv_event_t* e) {
    if (!_udp_ref) return;
    lv_obj_t* b = lv_event_get_target(e);
    if (b == _btn_play)   _udp_ref->sendCommand(MB_CMD_PLAY);
    else if (b == _btn_stop)   _udp_ref->sendCommand(MB_CMD_STOP);
    else if (b == _btn_rec)    _udp_ref->sendCommand(MB_CMD_RECORD);
    else if (b == _btn_rew)    _udp_ref->sendCommand(MB_CMD_REWIND);
    else if (b == _btn_fwd)    _udp_ref->sendCommand(MB_CMD_FORWARD);
    else if (b == _btn_repeat) _udp_ref->sendCommand(MB_CMD_TOGGLE_REPEAT);
    else if (b == _btn_metro)  _udp_ref->sendCommand(MB_CMD_TOGGLE_METRONOME);
}

static void _settings_cb(lv_event_t* e) {
    (void)e;
    /* Gear icon = direct toggle: Meters ↔ Settings (any other screen → Settings) */
    if (_current_screen == SCREEN_SETTINGS) {
        /* Already on settings — go back to meters */
        lv_scr_load_anim(_screens[SCREEN_METERS], LV_SCR_LOAD_ANIM_MOVE_RIGHT, MB_ANIM_NORMAL, 0, false);
        _current_screen = SCREEN_METERS;
    } else {
        /* Go directly to settings from wherever we are */
        lv_scr_load_anim(_screens[SCREEN_SETTINGS], LV_SCR_LOAD_ANIM_MOVE_LEFT, MB_ANIM_NORMAL, 0, false);
        _current_screen = SCREEN_SETTINGS;
    }
}

/* Spectrum screen toggle — called by spectrum button on meter screen (if present) */
static void _spectrum_cb(lv_event_t* e) {
    (void)e;
    if (_current_screen == SCREEN_SPECTRUM) {
        lv_scr_load_anim(_screens[SCREEN_METERS], LV_SCR_LOAD_ANIM_MOVE_RIGHT, MB_ANIM_NORMAL, 0, false);
        _current_screen = SCREEN_METERS;
    } else if (_current_screen == SCREEN_METERS) {
        lv_scr_load_anim(_screens[SCREEN_SPECTRUM], LV_SCR_LOAD_ANIM_MOVE_LEFT, MB_ANIM_NORMAL, 0, false);
        _current_screen = SCREEN_SPECTRUM;
    }
}

/* Non-static wrapper so settings screen back button can call it */
void _settings_back_from_settings(lv_event_t* e) {
    (void)e;
    lv_scr_load_anim(_screens[SCREEN_METERS], LV_SCR_LOAD_ANIM_MOVE_RIGHT, MB_ANIM_NORMAL, 0, false);
    _current_screen = SCREEN_METERS;
}

/* Non-static wrapper so settings screen can show spectrum analyzer */
void _navigate_to_spectrum(lv_event_t* e) {
    (void)e;
    lv_scr_load_anim(_screens[SCREEN_SPECTRUM], LV_SCR_LOAD_ANIM_MOVE_LEFT, MB_ANIM_NORMAL, 0, false);
    _current_screen = SCREEN_SPECTRUM;
}

/* ─── Toast Notification System ───────────────────────────────── */
/* A semi-transparent pill label that appears in the center of the
 * meter screen for 2 seconds, then auto-hides.                    */

static lv_obj_t*  _toast_obj   = NULL;
static lv_obj_t*  _toast_lbl   = NULL;
static lv_timer_t* _toast_timer = NULL;

static void _toast_hide_cb(lv_timer_t* t) {
    (void)t;
    if (_toast_obj) lv_obj_add_flag(_toast_obj, LV_OBJ_FLAG_HIDDEN);
    lv_timer_del(_toast_timer);
    _toast_timer = NULL;
}

/* Call this from anywhere to show a brief toast message */
void ui_show_toast(const char* msg) {
    if (!_toast_obj || !_toast_lbl) return;
    lv_label_set_text(_toast_lbl, msg);
    lv_obj_clear_flag(_toast_obj, LV_OBJ_FLAG_HIDDEN);
    /* Reset timer if already running */
    if (_toast_timer) {
        lv_timer_reset(_toast_timer);
    } else {
        _toast_timer = lv_timer_create(_toast_hide_cb, 2000, NULL);
        lv_timer_set_repeat_count(_toast_timer, 1);
    }
}

/* Meter source toggle callback (on main screen) */
static void _meter_src_toggle_cb(lv_event_t* e) {
    if (!_state_ref || !_udp_ref) return;
    if (_state_ref->meter_source == MB_SRC_MASTER) {
        _udp_ref->sendCommand(MB_CMD_SET_METER_SRC, MB_SRC_SELECTED_TRACK);
    } else {
        _udp_ref->sendCommand(MB_CMD_SET_METER_SRC, MB_SRC_MASTER);
    }
}

static lv_obj_t* _make_tbtn(lv_obj_t* par, const char* sym, lv_color_t c, int w = 50) {
    lv_obj_t* btn = lv_btn_create(par);
    lv_obj_set_size(btn, w, 40);
    mb_style_transport_btn(btn, c);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, sym);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, _transport_cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

/* ─── Helper: Create a status bar label ────────────────────────── */
static lv_obj_t* _sbar_label(lv_obj_t* parent, const char* text, 
                               lv_color_t color, const lv_font_t* font) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

/* ─── Helper: Vertical separator bar ───────────────────────────── */
static lv_obj_t* _sbar_sep(lv_obj_t* parent) {
    lv_obj_t* sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 1, 16);
    lv_obj_set_style_bg_color(sep, MB_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    return sep;
}

/* ─── Initialize UI ───────────────────────────────────────────── */

static void ui_init(meter_state_t* state, UDPComm* udp) {
    _state_ref = state;
    _udp_ref = udp;
    mb_theme_init();

    /* === Meter Screen === */
    _screens[SCREEN_METERS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screens[SCREEN_METERS], MB_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_screens[SCREEN_METERS], LV_OPA_COVER, 0);
    lv_obj_clear_flag(_screens[SCREEN_METERS], LV_OBJ_FLAG_SCROLLABLE);

    /* Runtime display dimensions (accommodate portrait rotation) */
    lv_coord_t disp_w = lv_disp_get_hor_res(NULL);
    lv_coord_t disp_h = lv_disp_get_ver_res(NULL);
    bool portrait = (disp_w < disp_h);
    int content_h = (int)disp_h - MB_STATUS_BAR_HEIGHT - MB_TRANSPORT_HEIGHT;

    /* ── Status Bar ── */
    lv_obj_t* sbar = lv_obj_create(_screens[SCREEN_METERS]);
    lv_obj_set_size(sbar, disp_w, MB_STATUS_BAR_HEIGHT);
    lv_obj_align(sbar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(sbar, MB_COLOR_BG_SECONDARY, 0);
    lv_obj_set_style_bg_opa(sbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sbar, MB_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(sbar, 1, 0);
    lv_obj_set_style_border_side(sbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_hor(sbar, MB_SPACE_SM, 0);
    lv_obj_set_style_radius(sbar, 0, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(sbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sbar, MB_SPACE_SM, 0);

    /* Transport state icon (play/stop/rec indicator) */
    _lbl_transport_icon = _sbar_label(sbar, LV_SYMBOL_STOP, MB_COLOR_TEXT_MUTED, MB_FONT_LABEL);

    /* Measure|Beat — fixed width so flex row doesn't reflow as content changes */
    _lbl_measure = _sbar_label(sbar, "1|1", MB_COLOR_ACCENT_AMBER, MB_FONT_STATUS);
    lv_obj_set_style_text_letter_space(_lbl_measure, 1, 0);
    lv_obj_set_style_min_width(_lbl_measure, 58, 0);   /* Fits "999|16" */
    lv_obj_set_style_text_align(_lbl_measure, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_lbl_measure, LV_LABEL_LONG_CLIP);

    /* Time position — fixed width to prevent jitter */
    _lbl_time_pos = _sbar_label(sbar, "0:00.0", MB_COLOR_ACCENT_AMBER, MB_FONT_STATUS);
    lv_obj_set_style_min_width(_lbl_time_pos, 72, 0);  /* Fits "99:59.9" */
    lv_obj_set_style_text_align(_lbl_time_pos, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_lbl_time_pos, LV_LABEL_LONG_CLIP);

    _sbar_sep(sbar);

    /* Meter source indicator ("MASTER" or "TRK: Bass Guitar") */
    /* Track color swatch — tiny colored rectangle shown in TRACK mode */
    _swatch_track_color = lv_obj_create(sbar);
    lv_obj_set_size(_swatch_track_color, 8, 14);
    lv_obj_set_style_bg_color(_swatch_track_color, lv_color_hex(0x777788), 0);
    lv_obj_set_style_bg_opa(_swatch_track_color, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_swatch_track_color, MB_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(_swatch_track_color, 1, 0);
    lv_obj_set_style_radius(_swatch_track_color, 2, 0);
    lv_obj_add_flag(_swatch_track_color, LV_OBJ_FLAG_HIDDEN);  /* hidden until TRACK mode */

    _lbl_meter_src = _sbar_label(sbar, "MASTER", MB_COLOR_TEXT_PRIMARY, MB_FONT_STATUS);
    lv_obj_set_style_text_letter_space(_lbl_meter_src, 2, 0);

    _sbar_sep(sbar);

    /* Project name */
    _lbl_project_name = _sbar_label(sbar, "", MB_COLOR_TEXT_SECONDARY, MB_FONT_SMALL);
    lv_obj_set_width(_lbl_project_name, 100);
    lv_label_set_long_mode(_lbl_project_name, LV_LABEL_LONG_DOT);

    /* Section / Region name */
    _lbl_section_name = _sbar_label(sbar, "", MB_COLOR_ACCENT_MAGENTA, MB_FONT_SMALL);
    lv_obj_set_width(_lbl_section_name, portrait ? 60 : 80);
    lv_label_set_long_mode(_lbl_section_name, LV_LABEL_LONG_DOT);

    _sbar_sep(sbar);

    /* Integrated LUFS */
    _lbl_lufs_i = _sbar_label(sbar, "LUFS-I --.--", MB_COLOR_ACCENT_SEAFOAM, MB_FONT_STATUS);

    /* WiFi connection indicator */
    _lbl_conn = _sbar_label(sbar, LV_SYMBOL_WIFI " --", MB_COLOR_TEXT_MUTED, MB_FONT_SMALL);

    /* FPS overlay — absolute-positioned at right edge of status bar.
     * Does NOT join the flex row so it never displaces other items. */
    _lbl_fps = lv_label_create(_screens[SCREEN_METERS]);
    lv_label_set_text(_lbl_fps, "--fps");
    lv_obj_set_style_text_color(_lbl_fps, lv_color_white(), 0);
    lv_obj_set_style_text_font(_lbl_fps, &lv_font_montserrat_12, 0);
    /* Align to top-right corner inside status bar area */
    lv_obj_align(_lbl_fps, LV_ALIGN_TOP_RIGHT, -MB_SPACE_SM, (MB_STATUS_BAR_HEIGHT - 12) / 2);
    if (!g_show_fps) lv_obj_add_flag(_lbl_fps, LV_OBJ_FLAG_HIDDEN);

    /* ── Transport Bar ── */
    lv_obj_t* tbar = lv_obj_create(_screens[SCREEN_METERS]);
    lv_obj_set_size(tbar, disp_w, MB_TRANSPORT_HEIGHT);
    lv_obj_align(tbar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(tbar, MB_COLOR_BG_SECONDARY, 0);
    lv_obj_set_style_bg_opa(tbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tbar, MB_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(tbar, 1, 0);
    lv_obj_set_style_border_side(tbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(tbar, 0, 0);
    lv_obj_set_style_pad_hor(tbar, MB_SPACE_SM, 0);
    lv_obj_clear_flag(tbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(tbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tbar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tbar, MB_SPACE_XS, 0);

    /* Dynamic sizing for transport buttons based on orientation */
    int main_btn_w = portrait ? 58 : 50;
    int side_btn_w = portrait ? 48 : 42;

    _btn_rew  = _make_tbtn(tbar, LV_SYMBOL_PREV, MB_COLOR_ACTIVE, side_btn_w);
    _btn_stop = _make_tbtn(tbar, LV_SYMBOL_STOP, MB_COLOR_STOP, main_btn_w);
    _btn_play = _make_tbtn(tbar, LV_SYMBOL_PLAY, MB_COLOR_PLAY, portrait ? 64 : 54);
    _btn_rec  = _make_tbtn(tbar, LV_SYMBOL_DUMMY, MB_COLOR_RECORD, main_btn_w);
    _btn_fwd  = _make_tbtn(tbar, LV_SYMBOL_NEXT, MB_COLOR_ACTIVE, side_btn_w);

    _btn_repeat = _make_tbtn(tbar, LV_SYMBOL_LOOP, MB_COLOR_ACCENT_PURPLE, main_btn_w);
    lv_obj_add_flag(_btn_repeat, LV_OBJ_FLAG_CHECKABLE);
    _btn_metro = _make_tbtn(tbar, LV_SYMBOL_AUDIO, MB_COLOR_ACCENT_AMBER, main_btn_w);
    lv_obj_add_flag(_btn_metro, LV_OBJ_FLAG_CHECKABLE);

    _lbl_tempo = lv_label_create(tbar);
    lv_label_set_text(_lbl_tempo, "120.0");
    lv_obj_set_style_text_color(_lbl_tempo, MB_COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_text_font(_lbl_tempo, MB_FONT_LABEL, 0);

    _lbl_tsig = lv_label_create(tbar);
    lv_label_set_text(_lbl_tsig, "4/4");
    lv_obj_set_style_text_color(_lbl_tsig, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(_lbl_tsig, MB_FONT_SMALL, 0);

    lv_obj_t* sbtn = _make_tbtn(tbar, LV_SYMBOL_SETTINGS, MB_COLOR_TEXT_SECONDARY, main_btn_w);
    lv_obj_remove_event_cb(sbtn, _transport_cb);
    lv_obj_add_event_cb(sbtn, _settings_cb, LV_EVENT_CLICKED, NULL);

    /* Rotate button — cycles rotation and reboots */
    lv_obj_t* rbtn = _make_tbtn(tbar, LV_SYMBOL_REFRESH, MB_COLOR_ACCENT_SEAFOAM, side_btn_w);
    lv_obj_remove_event_cb(rbtn, _transport_cb);
    lv_obj_add_event_cb(rbtn, [](lv_event_t* ev) {
        (void)ev;
        uint8_t next = (g_display_rotation + 1) % 4;
        Preferences prefs;
        prefs.begin("meterbridge", false);
        prefs.putUChar("rotation", next);
        prefs.end();
        Serial.printf("[CFG] Rotation -> %d, restarting...\n", next);
        delay(200);
        ESP.restart();
    }, LV_EVENT_CLICKED, NULL);

    /* ── Main Content Area ── */
    _main_container = lv_obj_create(_screens[SCREEN_METERS]);
    lv_obj_set_size(_main_container, (int)disp_w, content_h);
    lv_obj_set_pos(_main_container, 0, MB_STATUS_BAR_HEIGHT);
    lv_obj_set_style_bg_color(_main_container, MB_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_main_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_main_container, 0, 0);
    lv_obj_set_style_radius(_main_container, 0, 0);
    lv_obj_set_style_pad_all(_main_container, 0, 0);
    lv_obj_clear_flag(_main_container, LV_OBJ_FLAG_SCROLLABLE);
    ui_create_meter_screen(_main_container, state, udp);

    /* ── Toast overlay — sits above all meter content, centered ── */
    _toast_obj = lv_obj_create(_screens[SCREEN_METERS]);
    lv_obj_set_size(_toast_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(_toast_obj, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_color(_toast_obj, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(_toast_obj, LV_OPA_90, 0);
    lv_obj_set_style_border_color(_toast_obj, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_border_width(_toast_obj, 2, 0);
    lv_obj_set_style_radius(_toast_obj, 24, 0);
    lv_obj_set_style_pad_hor(_toast_obj, 24, 0);
    lv_obj_set_style_pad_ver(_toast_obj, 10, 0);
    lv_obj_clear_flag(_toast_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_toast_obj, LV_OBJ_FLAG_FLOATING); /* don't affect layout */
    _toast_lbl = lv_label_create(_toast_obj);
    lv_label_set_text(_toast_lbl, "");
    lv_obj_set_style_text_color(_toast_lbl, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(_toast_lbl, MB_FONT_LABEL, 0);
    lv_obj_add_flag(_toast_obj, LV_OBJ_FLAG_HIDDEN); /* hidden until triggered */

    /* === Settings Screen === */
    _screens[SCREEN_SETTINGS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screens[SCREEN_SETTINGS], MB_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_screens[SCREEN_SETTINGS], LV_OPA_COVER, 0);
    /* Vertical scroll enabled so tall settings content is reachable */
    lv_obj_set_scroll_dir(_screens[SCREEN_SETTINGS], LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_screens[SCREEN_SETTINGS], LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_all(_screens[SCREEN_SETTINGS], 0, 0);
    ui_create_settings_screen(_screens[SCREEN_SETTINGS], state);

    /* === Spectrum Analyzer Screen === */
    _screens[SCREEN_SPECTRUM] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screens[SCREEN_SPECTRUM], MB_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_screens[SCREEN_SPECTRUM], LV_OPA_COVER, 0);
    lv_obj_clear_flag(_screens[SCREEN_SPECTRUM], LV_OBJ_FLAG_SCROLLABLE);
    ui_create_spectrum_screen(_screens[SCREEN_SPECTRUM], state);

    lv_scr_load(_screens[SCREEN_METERS]);
}

/* ─── Screen Update Forward Declarations ────────────────────────── */
extern void ui_update_meter_screen(meter_state_t* state);
extern void _settings_screen_update(void);

/* Connection mode + serial data timestamp — defined in main.cpp */
extern uint8_t  g_conn_mode;
extern uint32_t g_last_serial_data_ms;
extern bool     g_show_fps;
extern float    g_fps_display;

/* ─── Update UI (called every frame from Core 1) ──────────────── */

static void ui_update(meter_state_t* state) {
    if (!state) return;
    static char buf[48];
    uint8_t tf = state->transport_flags;

    /* ── Transport Icon (dirty-bit guarded) ── */
    {
        static uint8_t _last_tf = 0xFF;
        if (tf != _last_tf) {
            _last_tf = tf;
            if (tf & MB_TRANSPORT_RECORDING) {
                lv_label_set_text(_lbl_transport_icon, LV_SYMBOL_DUMMY);
                lv_obj_set_style_text_color(_lbl_transport_icon, MB_COLOR_RECORD, 0);
            } else if (tf & MB_TRANSPORT_PLAYING) {
                lv_label_set_text(_lbl_transport_icon, LV_SYMBOL_PLAY);
                lv_obj_set_style_text_color(_lbl_transport_icon, MB_COLOR_PLAY, 0);
            } else if (tf & MB_TRANSPORT_PAUSED) {
                lv_label_set_text(_lbl_transport_icon, LV_SYMBOL_PAUSE);
                lv_obj_set_style_text_color(_lbl_transport_icon, MB_COLOR_ACCENT_AMBER, 0);
            } else {
                lv_label_set_text(_lbl_transport_icon, LV_SYMBOL_STOP);
                lv_obj_set_style_text_color(_lbl_transport_icon, MB_COLOR_TEXT_MUTED, 0);
            }
        }
    }

    /* ── Measure | Beat (dirty-bit) ── */
    {
        static char _last_measure[16] = "";
        snprintf(buf, sizeof(buf), "%d|%d", state->measure, state->beat_in_bar);
        if (strcmp(buf, _last_measure) != 0) {
            strncpy(_last_measure, buf, sizeof(_last_measure));
            lv_label_set_text(_lbl_measure, buf);
        }
    }

    /* ── Time Position (dirty-bit) ── */
    {
        static char _last_time[16] = "";
        float s = state->position_secs;
        int mins = (int)(s / 60.0f);
        float secs = s - mins * 60.0f;
        snprintf(buf, sizeof(buf), "%d:%04.1f", mins, secs);
        if (strcmp(buf, _last_time) != 0) {
            strncpy(_last_time, buf, sizeof(_last_time));
            lv_label_set_text(_lbl_time_pos, buf);
        }
    }

    /* ── Meter Source Indicator + track color swatch (dirty-bit) ── */
    {
        static uint8_t _last_src = 0xFF;
        static char _last_trk[MB_MAX_TRACK_NAME_LEN] = "";
        if (state->meter_source != _last_src ||
            (state->meter_source != MB_SRC_MASTER && strcmp(state->track_name, _last_trk) != 0)) {
            _last_src = state->meter_source;
            if (state->meter_source == MB_SRC_MASTER) {
                lv_label_set_text(_lbl_meter_src, "MASTER");
                lv_obj_set_style_text_color(_lbl_meter_src, MB_COLOR_TEXT_PRIMARY, 0);
                if (_swatch_track_color) lv_obj_add_flag(_swatch_track_color, LV_OBJ_FLAG_HIDDEN);
            } else {
                strncpy(_last_trk, state->track_name, MB_MAX_TRACK_NAME_LEN);
                snprintf(buf, sizeof(buf), "TRK: %s", state->track_name);
                lv_label_set_text(_lbl_meter_src, buf);
                lv_color_t tc = lv_color_make(state->track_color_r, state->track_color_g, state->track_color_b);
                lv_obj_set_style_text_color(_lbl_meter_src, tc, 0);
                if (_swatch_track_color) {
                    lv_obj_set_style_bg_color(_swatch_track_color, tc, 0);
                    lv_obj_clear_flag(_swatch_track_color, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    /* ── Project Name (dirty-bit) ── */
    {
        static char _last_proj[MB_MAX_PROJECT_NAME_LEN] = "";
        if (state->project_name[0] != '\0' && strcmp(state->project_name, _last_proj) != 0) {
            strncpy(_last_proj, state->project_name, MB_MAX_PROJECT_NAME_LEN);
            lv_label_set_text(_lbl_project_name, state->project_name);
        }
    }

    /* ── Section/Region Name (dirty-bit) ── */
    {
        static char _last_sect[MB_MAX_SECTION_NAME_LEN] = "";
        const char* sect = (state->section_name[0] != '\0') ? state->section_name : "";
        if (strcmp(sect, _last_sect) != 0) {
            strncpy(_last_sect, sect, MB_MAX_SECTION_NAME_LEN);
            lv_label_set_text(_lbl_section_name, sect);
        }
    }

    /* ── Integrated LUFS (dirty-bit) ── */
    {
        static char _last_lufs_i[24] = "";
        if (state->lufs_integrated <= -70.0f)
            snprintf(buf, sizeof(buf), "LUFS-I --.--");
        else
            snprintf(buf, sizeof(buf), "LUFS-I %.1f", state->lufs_integrated);
        if (strcmp(buf, _last_lufs_i) != 0) {
            strncpy(_last_lufs_i, buf, sizeof(_last_lufs_i));
            lv_label_set_text(_lbl_lufs_i, buf);
        }
    }

    /* ── Connection Indicator (dirty-bit) ── */
    {
        static char _last_conn[24] = "";
        bool live = state->connected;
        const char* conn_text;
        lv_color_t conn_color;
        if (g_conn_mode == 1) {
            conn_text = live ? LV_SYMBOL_USB " USB" : LV_SYMBOL_USB " --";
        } else if (g_conn_mode == 2) {
            conn_text = live ? LV_SYMBOL_WIFI "+" LV_SYMBOL_USB : LV_SYMBOL_WIFI "+" LV_SYMBOL_USB " --";
        } else {
            conn_text = live ? LV_SYMBOL_WIFI : LV_SYMBOL_WIFI " --";
        }
        conn_color = live ? MB_COLOR_ACCENT_SEAFOAM : MB_COLOR_TEXT_MUTED;
        if (strcmp(conn_text, _last_conn) != 0) {
            strncpy(_last_conn, conn_text, sizeof(_last_conn));
            lv_label_set_text(_lbl_conn, conn_text);
            lv_obj_set_style_text_color(_lbl_conn, conn_color, 0);
        }
    }

    /* ── Transport Bar Values (dirty-bit) ── */
    {
        static char _last_tempo[12] = "";
        snprintf(buf, sizeof(buf), "%.1f", state->tempo_bpm);
        if (strcmp(buf, _last_tempo) != 0) {
            strncpy(_last_tempo, buf, sizeof(_last_tempo));
            lv_label_set_text(_lbl_tempo, buf);
        }
    }
    {
        static char _last_tsig[8] = "";
        snprintf(buf, sizeof(buf), "%d/%d", state->time_sig_num, state->time_sig_den);
        if (strcmp(buf, _last_tsig) != 0) {
            strncpy(_last_tsig, buf, sizeof(_last_tsig));
            lv_label_set_text(_lbl_tsig, buf);
        }
    }

    /* Transport button states — dirty-bit guarded to avoid marking buttons
     * dirty every frame (which forces LVGL to redraw them even if unchanged) */
    {
        static uint8_t _last_tf_btns = 0xFF;
        if (tf != _last_tf_btns) {
            _last_tf_btns = tf;
            if (tf & MB_TRANSPORT_PLAYING) lv_obj_add_state(_btn_play, LV_STATE_CHECKED);
            else lv_obj_clear_state(_btn_play, LV_STATE_CHECKED);

            if (tf & MB_TRANSPORT_RECORDING) lv_obj_add_state(_btn_rec, LV_STATE_CHECKED);
            else lv_obj_clear_state(_btn_rec, LV_STATE_CHECKED);

            if (tf & MB_TRANSPORT_REPEAT) lv_obj_add_state(_btn_repeat, LV_STATE_CHECKED);
            else lv_obj_clear_state(_btn_repeat, LV_STATE_CHECKED);

            if (tf & MB_TRANSPORT_METRONOME) lv_obj_add_state(_btn_metro, LV_STATE_CHECKED);
            else lv_obj_clear_state(_btn_metro, LV_STATE_CHECKED);
        }
    }

    /* Update meter bars */
    if (_current_screen == SCREEN_METERS) ui_update_meter_screen(state);

    /* Update spectrum bars */
    if (_current_screen == SCREEN_SPECTRUM) ui_update_spectrum_screen(state);
    
    /* Update settings screen (WiFi scan polling + status) */
    if (_current_screen == SCREEN_SETTINGS) _settings_screen_update();

    /* ── FPS overlay — update once/second, dirty-bit guarded ── */
    if (_lbl_fps) {
        static float _last_fps_shown = -1.0f;
        /* show/hide based on g_show_fps flag */
        if (g_show_fps) {
            lv_obj_clear_flag(_lbl_fps, LV_OBJ_FLAG_HIDDEN);
            /* only reformat when value changed by >= 0.5 fps */
            if (fabsf(g_fps_display - _last_fps_shown) >= 0.5f) {
                _last_fps_shown = g_fps_display;
                static char _fps_buf[12];
                snprintf(_fps_buf, sizeof(_fps_buf), "%.0ffps", g_fps_display);
                lv_label_set_text(_lbl_fps, _fps_buf);
            }
        } else {
            lv_obj_add_flag(_lbl_fps, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* ── OTA Progress Overlay ── */
    {
        static lv_obj_t* _ota_overlay = NULL;
        static lv_obj_t* _ota_bar     = NULL;
        static lv_obj_t* _ota_lbl     = NULL;
        static uint8_t   _ota_last_pct = 255;

        if (g_ota_in_progress && !_ota_overlay) {
            /* Create fullscreen overlay */
            _ota_overlay = lv_obj_create(lv_layer_top());
            lv_obj_remove_style_all(_ota_overlay);
            lv_obj_set_size(_ota_overlay, LV_PCT(100), LV_PCT(100));
            lv_obj_set_style_bg_opa(_ota_overlay, LV_OPA_80, 0);
            lv_obj_set_style_bg_color(_ota_overlay, lv_color_hex(0x000000), 0);
            lv_obj_set_layout(_ota_overlay, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(_ota_overlay, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(_ota_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            _ota_lbl = lv_label_create(_ota_overlay);
            lv_label_set_text(_ota_lbl, "Updating firmware...");
            lv_obj_set_style_text_color(_ota_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(_ota_lbl, &lv_font_montserrat_20, 0);

            _ota_bar = lv_bar_create(_ota_overlay);
            lv_obj_set_size(_ota_bar, 400, 24);
            lv_bar_set_range(_ota_bar, 0, 100);
            lv_obj_set_style_bg_color(_ota_bar, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_set_style_bg_color(_ota_bar, MB_COLOR_ACCENT_SEAFOAM, LV_PART_INDICATOR);
            lv_obj_set_style_radius(_ota_bar, 4, LV_PART_MAIN);
            lv_obj_set_style_radius(_ota_bar, 4, LV_PART_INDICATOR);
            _ota_last_pct = 255;
        }
        if (_ota_overlay && g_ota_in_progress) {
            if (g_ota_progress != _ota_last_pct) {
                _ota_last_pct = g_ota_progress;
                lv_bar_set_value(_ota_bar, g_ota_progress, LV_ANIM_ON);
                static char _ota_txt[32];
                snprintf(_ota_txt, sizeof(_ota_txt), "Updating firmware... %d%%", g_ota_progress);
                lv_label_set_text(_ota_lbl, _ota_txt);
            }
        }
        if (_ota_overlay && !g_ota_in_progress) {
            lv_obj_del(_ota_overlay);
            _ota_overlay = NULL;
            _ota_bar = NULL;
            _ota_lbl = NULL;
        }
    }
}

/* Include screen implementations (they use types defined above) */
#include "screen_meters.h"
#include "screen_spectrum.h"
#include "screen_settings.h"

#endif /* UI_MANAGER_H */
