/*
 * MeterBridge — Settings Screen
 * WiFi config (dropdown scanner), meter preferences, display brightness.
 * "Codeine Crazy" aesthetic.
 */

#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include <lvgl.h>
#include <WiFi.h>
#include <Preferences.h>
#include "theme.h"
#include "../network/udp_comm.h"
#include "network/wifi_manager.h"

/* Globals from main.cpp */
extern WiFiManager wifiMgr;

/* Externs from screen_spectrum.h and ui_manager.h */
extern void _spec_refresh_ui(void);
extern void _navigate_to_spectrum(lv_event_t* e);

static lv_obj_t* _dd_ssid = NULL;
static lv_obj_t* _ta_ssid = NULL;
static lv_obj_t* _ta_pass = NULL;
static lv_obj_t* _kb = NULL;
static lv_obj_t* _lbl_wifi_status = NULL;
static lv_obj_t* _lbl_scan_status = NULL;
static lv_obj_t* _rot_btns[4]   = {NULL, NULL, NULL, NULL};
static lv_obj_t* _peak_hold_dd  = NULL;
static lv_obj_t* _theme_btns[MB_THEME_COUNT];  /* sized at compile time — no overflow */
static lv_obj_t* _conn_btns[3]   = {NULL, NULL, NULL};  /* WiFi | Serial | Both */
static lv_obj_t* _tick_rate_dd    = NULL;                /* tick rate dropdown */
static lv_obj_t* _brightness_slider = NULL;             /* display brightness slider */
static lv_obj_t* _brightness_label  = NULL;             /* brightness % readout */
static lv_obj_t* _spec_mode_btns[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

extern uint8_t  g_conn_mode;           /* defined in main.cpp */
extern uint32_t g_update_interval_ms;  /* defined in main.cpp */
extern uint8_t  g_brightness;          /* defined in main.cpp */
extern bool     g_show_fps;            /* defined in main.cpp */
extern uint8_t  g_spec_color_mode;     /* defined in main.cpp */

/* Store scanned SSIDs so dropdown index maps to a name */
#define MAX_SCAN_RESULTS 20
static char _scanned_ssids[MAX_SCAN_RESULTS][33];
static int16_t _scanned_rssi[MAX_SCAN_RESULTS];
static int  _scan_count = 0;
static bool _scan_in_progress = false;
static uint8_t _last_wifi_state = 255;

/* ─── WiFi Status Update ─────────────────────────────────────── */

static void _update_wifi_status_label(void) {
    if (!_lbl_wifi_status) return;
    
    extern WiFiManager wifiMgr;
    wifi_state_t st = wifiMgr.getState();
    
    static uint8_t last_reason = 0;
    uint8_t current_reason = wifiMgr._lastDisconnectReason;
    
    if (st == _last_wifi_state && last_reason == current_reason && st != WIFI_STATE_CONNECTING) return;
    
    _last_wifi_state = st;
    last_reason = current_reason;

    String statusText = wifiMgr.getStatusText();
    
    switch (st) {
        case WIFI_STATE_CONNECTED: {
            char buf[128];
            snprintf(buf, sizeof(buf), LV_SYMBOL_OK " Connected: %s  IP: %s",
                     wifiMgr.getSSID().c_str(), wifiMgr.getLocalIP().toString().c_str());
            lv_label_set_text(_lbl_wifi_status, buf);
            lv_obj_set_style_text_color(_lbl_wifi_status, lv_color_hex(0x64FFDA), 0);
            break;
        }
        case WIFI_STATE_CONNECTING:
        case WIFI_STATE_DISCONNECTED:
            /* Use the rich string from the backend so we see the error codes! */
            lv_label_set_text(_lbl_wifi_status, (String(LV_SYMBOL_REFRESH " ") + statusText).c_str());
            lv_obj_set_style_text_color(_lbl_wifi_status, MB_COLOR_ACCENT_MAGENTA, 0);
            break;
        case WIFI_STATE_AP_MODE:
            lv_label_set_text(_lbl_wifi_status, LV_SYMBOL_SETTINGS " Setup Mode (Connect as client to configure)");
            lv_obj_set_style_text_color(_lbl_wifi_status, MB_COLOR_ACCENT_AMBER, 0);
            break;
        default:
            lv_label_set_text(_lbl_wifi_status, LV_SYMBOL_WIFI " Idle");
            lv_obj_set_style_text_color(_lbl_wifi_status, MB_COLOR_TEXT_MUTED, 0);
            break;
    }
}

/* ─── Callbacks ─────────────────────────────────────────────── */

/* Show keyboard when password field is focused */
static void _ta_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);
    if (_kb) {
        lv_keyboard_set_textarea(_kb, ta);
        lv_obj_clear_flag(_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Hide keyboard when focus is lost */
static void _ta_defocus_cb(lv_event_t* e) {
    if (_kb) {
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Keyboard ready/cancel callback */
static void _kb_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(_kb, NULL);
    }
}

/* Scan timer task */
static void _wifi_scan_task(lv_timer_t* timer) {
    if (wifiMgr.isScanning() || wifiMgr.isConnecting()) return;
    Serial.println("[WIFI] Auto-scan triggered...");
    wifiMgr.scanNetworks();
}

/* Scan/Refresh button callback */
static void _wifi_scan_btn_cb(lv_event_t * e) {
    (void)e;
    Serial.println("[WIFI] Manual scan requested...");
    
    /* Disconnect from AP but keep STA mode alive */
    WiFi.disconnect(false);
    delay(100);
    
    /* Show scanning indicator if available */
    if (_lbl_scan_status) {
        lv_obj_clear_flag(_lbl_scan_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_lbl_scan_status, "Scanning...");
        lv_refr_now(NULL);
    }

    /* Perform actual synchronous scan */
    int16_t n = WiFi.scanNetworks(false /* async */, true /* show_hidden */);
    
    /* Hide scanning indicator */
    if (_lbl_scan_status) {
        lv_obj_add_flag(_lbl_scan_status, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (n <= 0) {
        Serial.printf("[WIFI] Scan complete: %d results\n", n);
        lv_dropdown_set_options(_dd_ssid, "No networks found - tap SCAN");
        return;
    }
    
    String options = "";
    
    /* VERY IMPORTANT: Grab the last saved network from NVMS! */
    extern WiFiManager wifiMgr;
    String saved_ssid = wifiMgr.getSSID();
    
    /* Cache the scan results locally before we delete the context */
    String temp_ssids[MAX_SCAN_RESULTS];
    int temp_rssi[MAX_SCAN_RESULTS];
    
    for (int i = 0; i < n && i < MAX_SCAN_RESULTS; ++i) {
        temp_ssids[i] = WiFi.SSID(i);
        temp_rssi[i] = WiFi.RSSI(i);
    }
    
    /* CRITICAL: explicitly tear down memory allocations */
    WiFi.scanDelete();

    _scan_count = 0;
    
    /* Pass 1: ALWAYS put the Saved Network at the very top (Index 0) if it exists, 
               even if it isn't currently responding to the active scan. */
    if (saved_ssid.length() > 0) {
        strncpy(_scanned_ssids[0], saved_ssid.c_str(), 32);
        _scanned_ssids[0][32] = '\0';
        options += LV_SYMBOL_OK " Saved: ";
        options += saved_ssid;
        _scan_count = 1;
    }
    
    /* Pass 2: Add all the scanned networks */
    for (int i = 0; i < n && _scan_count < MAX_SCAN_RESULTS; i++) {
        String ssid = temp_ssids[i];
        if (ssid.length() == 0) continue; // Skip hidden/empty
        
        /* Check for duplicates (which also skips the saved network if we already added it!) */
        bool dup = false;
        for (int j = 0; j < _scan_count; j++) {
            if (ssid == _scanned_ssids[j]) { dup = true; break; }
        }
        if (dup) continue; /* Skip */
        
        /* Save raw SSID for connect callback */
        strncpy(_scanned_ssids[_scan_count], ssid.c_str(), 32);
        _scanned_ssids[_scan_count][32] = '\0';
        
        /* Build dropdown option with signal strength */
        int rssi = temp_rssi[i];
        const char* sig_icon = (rssi > -50) ? LV_SYMBOL_WIFI :
                               (rssi > -70) ? LV_SYMBOL_WIFI :
                                              LV_SYMBOL_WARNING;
        if (_scan_count > 0) options += "\n";
        options += sig_icon;
        options += " ";
        options += ssid;
        options += "  (";
        options += String(rssi);
        options += " dBm)";
        
        _scan_count++;
    }
    
    /* Update dropdown UI component */
    if (_dd_ssid && options.length() > 0) {
        lv_dropdown_set_options(_dd_ssid, options.c_str());
        lv_dropdown_set_selected(_dd_ssid, 0); // Always auto-select the saved network or best network
    } else if (_dd_ssid) {
        lv_dropdown_set_options(_dd_ssid, "Hidden Network Found - Please Connect Manually");
    }
    
    Serial.printf("[WIFI] Found %d unique networks\n", _scan_count);
}

/* Back button callback — defined in ui_manager.h */
extern void _settings_back_from_settings(lv_event_t* e);
extern void _navigate_to_spectrum(lv_event_t* e);


/* WiFi connect callback — defined in main.cpp (has access to wifiMgr) */
extern void wifi_connect_from_settings(lv_event_t* e);

/* ─── New Feature Callbacks ──────────────────────────────────── */

/* Rotation: saves to NVS then reboots. lcd.setRotation() is applied at boot. */
static void _rotation_btn_cb(lv_event_t* e) {
    uint8_t rot = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (rot == g_display_rotation) return; /* No-op if already active */
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("rotation", rot);
    prefs.end();
    Serial.printf("[CFG] Rotation -> %d, restarting...\n", rot);
    delay(150);
    ESP.restart();
}

/* Peak hold: applies immediately + persists in NVS */
static void _peak_hold_dd_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    uint16_t sel = lv_dropdown_get_selected(dd);
    /* 0 sentinel = hold forever, stored as 0, applied as UINT32_MAX */
    static const uint32_t HOLD_VALUES[] = {500,1000,1500,2000,2500,3000,5000,0};
    uint32_t ms = HOLD_VALUES[sel < 8 ? sel : 4];
    g_peak_hold_ms = (ms == 0) ? UINT32_MAX : ms;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putULong("peak_hold", ms);
    prefs.end();
    Serial.printf("[CFG] Peak hold -> %u ms\n", ms);
}

/* Meter Height: XXL, XL, L, M, S, XS — saves selection then reboots */
static void _meter_height_dd_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    uint16_t sel = lv_dropdown_get_selected(dd);
    if (sel >= 6) sel = 1; /* default to XL */
    extern uint8_t g_meter_height_idx;
    if (sel == g_meter_height_idx) return;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("mtr_height", (uint8_t)sel);
    prefs.end();
    Serial.printf("[CFG] Meter height -> %d, restarting...\n", sel);
    delay(150);
    ESP.restart();
}

/* Loudness target preset: applies immediately + persists in NVS */
static lv_obj_t* _loudness_dd = NULL;
static lv_obj_t* _lbl_loudness_val = NULL;

static const float LOUDNESS_PRESETS[] = {
    /* ── Standard broadcast / streaming ── */
    -14.0f,    /* 0  Streaming (Spotify/YouTube)   */
    -16.0f,    /* 1  Podcast / Audiobook           */
    -23.0f,    /* 2  Broadcast (EBU R128)          */
    -27.0f,    /* 3  Film / Cinema                 */
    -18.0f,    /* 4  Classical / Dynamic           */
    /* ── Hot/loud targets ── */
    -12.0f,    /* 5  Hot Rock / EDM                */
    -10.0f,    /* 6  Very Hot                      */
     -9.0f,    /* 7  -9 LUFS                       */
     -8.0f,    /* 8  -8 LUFS                       */
     -6.0f,    /* 9  -6 LUFS                       */
     -4.0f,    /* 10 -4 LUFS                       */
     -3.0f,    /* 11 -3 LUFS                       */
     -2.0f,    /* 12 -2 LUFS                       */
     -1.0f,    /* 13 -1 LUFS                       */
     -0.1f,    /* 14 -0.1 dBFS (near-peak limit)   */
      0.0f,    /* 15 Off (no target line)          */
};
static const int NUM_LOUDNESS_PRESETS = 16;

static void _loudness_dd_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    uint16_t sel = lv_dropdown_get_selected(dd);
    if (sel >= NUM_LOUDNESS_PRESETS) sel = 15; /* default to Off */
    g_loudness_target = LOUDNESS_PRESETS[sel];
    g_loudness_preset = sel;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("lufs_preset", sel);
    prefs.end();
    /* Update the dB readout label */
    if (_lbl_loudness_val) {
        if (sel == 15) { /* Off */
            lv_label_set_text(_lbl_loudness_val, "OFF");
            lv_obj_set_style_text_color(_lbl_loudness_val, MB_COLOR_TEXT_MUTED, 0);
        } else {
            static char lbuf[16];
            if (sel == 14) /* -0.1 dBFS special case */
                snprintf(lbuf, sizeof(lbuf), "-0.1 dBFS");
            else
                snprintf(lbuf, sizeof(lbuf), "%.0f LUFS", g_loudness_target);
            lv_label_set_text(_lbl_loudness_val, lbuf);
            lv_obj_set_style_text_color(_lbl_loudness_val, MB_COLOR_ACCENT_PURPLE, 0);
        }
    }
    Serial.printf("[CFG] Loudness target -> %.1f LUFS (preset %d)\n", g_loudness_target, sel);
}

/* Theme: saves selection then reboots so full palette reloads cleanly */
static void _theme_btn_cb(lv_event_t* e) {
    uint8_t tid = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (tid == g_current_theme) return;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("theme", tid);
    prefs.end();
    Serial.printf("[CFG] Theme -> %d, restarting...\n", tid);
    delay(150);
    ESP.restart();
}

/* Connection mode: saves to NVS, applied live (no reboot needed) */
static void _conn_mode_btn_cb(lv_event_t* e) {
    uint8_t m = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (m == g_conn_mode) {
        lv_obj_add_state(_conn_btns[m], LV_STATE_CHECKED); /* keep it checked */
        return;
    }
    /* Update visual state of all three buttons */
    for (int i = 0; i < 3; i++) {
        if (_conn_btns[i]) {
            if (i == (int)m) lv_obj_add_state(_conn_btns[i], LV_STATE_CHECKED);
            else             lv_obj_clear_state(_conn_btns[i], LV_STATE_CHECKED);
        }
    }
    g_conn_mode = m;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("conn_mode", m);
    prefs.end();
    static const char* CMODE_NAMES[] = {"UDP", "Serial", "Both"};
    Serial.printf("[CFG] ConnMode -> %d (%s)\n", m, CMODE_NAMES[m]);
}

/* Brightness slider callback — live apply, NVS save */
static void _brightness_slider_cb(lv_event_t* e) {
    lv_obj_t* sl = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(sl);
    g_brightness = (uint8_t)val;
    /* Apply immediately to hardware — use local extern to avoid TU redeclaration issues */
    { extern void set_display_brightness(uint8_t); set_display_brightness((uint8_t)map(val, 0, 100, 0, 255)); }
    /* Update readout label */
    if (_brightness_label) {
        static char brbuf[8];
        lv_snprintf(brbuf, sizeof(brbuf), "%d%%", val);
        lv_label_set_text(_brightness_label, brbuf);
    }
    /* Persist to NVS */
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("brightness", (uint8_t)val);
    prefs.end();
    Serial.printf("[CFG] Brightness -> %d%%\n", val);
}

/* FPS overlay toggle — live apply, NVS persist. No reboot needed. */
static void _fps_toggle_cb(lv_event_t* e) {
    lv_obj_t* cb = lv_event_get_target(e);
    g_show_fps = (lv_obj_get_state(cb) & LV_STATE_CHECKED) != 0;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putBool("show_fps", g_show_fps);
    prefs.end();
    Serial.printf("[CFG] ShowFPS -> %d\n", (int)g_show_fps);
}

/* Spectrum Mode: saves selection, live update */
static void _spec_mode_btn_cb(lv_event_t* e) {
    uint8_t m = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (m == g_spec_color_mode) {
        lv_obj_add_state(_spec_mode_btns[m], LV_STATE_CHECKED);
        return;
    }
    for (int i = 0; i < 6; i++) {
        if (_spec_mode_btns[i]) {
            if (i == (int)m) lv_obj_add_state(_spec_mode_btns[i], LV_STATE_CHECKED);
            else             lv_obj_clear_state(_spec_mode_btns[i], LV_STATE_CHECKED);
        }
    }
    g_spec_color_mode = m;
    Preferences prefs;
    prefs.begin("meterbridge", false);
    prefs.putUChar("spec_mode", m);
    prefs.end();
    Serial.printf("[CFG] SpecMode -> %d\n", m);

    /* Live update spectrum bars if screen is already created */
    _spec_refresh_ui();
}


void ui_create_settings_screen(lv_obj_t* parent, meter_state_t* state) {
    lv_coord_t disp_w = lv_disp_get_hor_res(NULL);

    lv_obj_set_style_bg_color(parent, MB_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    /* ─── Header Bar ─── */
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, disp_w, 52);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, MB_COLOR_BG_SECONDARY, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_90, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, MB_COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_shadow_color(header, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_shadow_width(header, 12, 0);
    lv_obj_set_style_shadow_opa(header, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Title */
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  S E T T I N G S");
    lv_obj_set_pos(title, MB_SPACE_LG, 14);
    lv_obj_set_style_text_color(title, MB_COLOR_ACCENT_MAGENTA, 0);
    lv_obj_set_style_text_font(title, MB_FONT_TITLE, 0);
    lv_obj_set_style_text_letter_space(title, 3, 0);

    /* Back button */
    lv_obj_t* back = lv_btn_create(header);
    lv_obj_set_size(back, 90, 36);
    lv_obj_set_pos(back, disp_w - 110, 8);
    mb_style_transport_btn(back, MB_COLOR_ACCENT_MAGENTA);
    lv_obj_t* blbl = lv_label_create(back);
    lv_label_set_text(blbl, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(blbl, MB_FONT_LABEL, 0);
    lv_obj_center(blbl);
    lv_obj_add_event_cb(back, _settings_back_from_settings, LV_EVENT_CLICKED, NULL);

    /* FFT / Spectrum button — left of BACK */
    lv_obj_t* fft_btn = lv_btn_create(header);
    lv_obj_set_size(fft_btn, 90, 36);
    lv_obj_set_pos(fft_btn, disp_w - 210, 8);
    mb_style_transport_btn(fft_btn, MB_COLOR_ACCENT_SEAFOAM);
    lv_obj_t* fft_lbl = lv_label_create(fft_btn);
    lv_label_set_text(fft_lbl, LV_SYMBOL_AUDIO " FFT");
    lv_obj_set_style_text_font(fft_lbl, MB_FONT_LABEL, 0);
    lv_obj_center(fft_lbl);
    lv_obj_add_event_cb(fft_btn, _navigate_to_spectrum, LV_EVENT_CLICKED, NULL);

    /* ─── WiFi Status Banner ─── */
    _lbl_wifi_status = lv_label_create(parent);
    lv_obj_set_pos(_lbl_wifi_status, MB_SPACE_LG, 56);
    lv_obj_set_width(_lbl_wifi_status, disp_w - MB_SPACE_LG * 2);
    lv_label_set_text(_lbl_wifi_status, LV_SYMBOL_REFRESH " Checking...");
    lv_obj_set_style_text_color(_lbl_wifi_status, MB_COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_text_font(_lbl_wifi_status, MB_FONT_LABEL, 0);
    _update_wifi_status_label();  /* Set initial state */

    int y = 78;
    
    /* ═══ WiFi Section ═══ */
    lv_obj_t* wifi_title = mb_create_caption_label(parent, LV_SYMBOL_WIFI "  W I F I   N E T W O R K");
    lv_obj_set_pos(wifi_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(wifi_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;

    /* Network dropdown label */
    lv_obj_t* net_lbl = lv_label_create(parent);
    lv_label_set_text(net_lbl, "NETWORK:");
    lv_obj_set_pos(net_lbl, MB_SPACE_LG, y + 8);
    lv_obj_set_style_text_color(net_lbl, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(net_lbl, MB_FONT_SMALL, 0);

    /* WiFi dropdown */
    _dd_ssid = lv_dropdown_create(parent);
    lv_obj_set_size(_dd_ssid, 260, 38);
    lv_obj_set_pos(_dd_ssid, 92, y);
    
    /* Pre-fill saved network into dropdown and internal scanner buffer */
    extern WiFiManager wifiMgr;
    String saved_ssid = wifiMgr.getSSID();
    if (saved_ssid.length() > 0) {
        String opt = LV_SYMBOL_OK " Saved: ";
        opt += saved_ssid;
        lv_dropdown_set_options(_dd_ssid, opt.c_str());
        
        /* Inject into array index 0 so that immediate clicking Connect retrieves the right value! */
        strncpy(_scanned_ssids[0], saved_ssid.c_str(), 32);
        _scanned_ssids[0][32] = '\0';
        _scan_count = 1;
    } else {
        lv_dropdown_set_options(_dd_ssid, "Tap SCAN to find networks");
        _scan_count = 0;
    }
    
    lv_obj_set_style_bg_color(_dd_ssid, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_opa(_dd_ssid, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_dd_ssid, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(_dd_ssid, MB_FONT_LABEL, 0);
    lv_obj_set_style_border_color(_dd_ssid, MB_COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(_dd_ssid, 1, 0);
    lv_obj_set_style_radius(_dd_ssid, MB_BORDER_RADIUS, 0);
    /* Dropdown list styling */
    lv_obj_t* ddlist = lv_dropdown_get_list(_dd_ssid);
    lv_obj_set_style_bg_color(ddlist, MB_COLOR_BG_ELEVATED, 0);
    lv_obj_set_style_text_color(ddlist, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_border_color(ddlist, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_border_width(ddlist, 1, 0);
    lv_obj_set_style_max_height(ddlist, 200, 0);
    
    /* Scan button */
    lv_obj_t* scan_btn = lv_btn_create(parent);
    lv_obj_set_size(scan_btn, 80, 38);
    lv_obj_set_pos(scan_btn, 360, y);
    mb_style_transport_btn(scan_btn, MB_COLOR_ACCENT_SEAFOAM);
    lv_obj_t* scan_lbl = lv_label_create(scan_btn);
    lv_label_set_text(scan_lbl, LV_SYMBOL_REFRESH " SCAN");
    lv_obj_set_style_text_font(scan_lbl, MB_FONT_SMALL, 0);
    lv_obj_center(scan_lbl);
    lv_obj_add_event_cb(scan_btn, _wifi_scan_btn_cb, LV_EVENT_CLICKED, NULL);
    
    /* Scanning indicator (hidden by default) */
    _lbl_scan_status = lv_label_create(parent);
    lv_obj_set_pos(_lbl_scan_status, 450, y + 10);
    lv_label_set_text(_lbl_scan_status, LV_SYMBOL_REFRESH " Scanning...");
    lv_obj_set_style_text_color(_lbl_scan_status, MB_COLOR_ACCENT_MAGENTA, 0);
    lv_obj_set_style_text_font(_lbl_scan_status, MB_FONT_SMALL, 0);
    lv_obj_add_flag(_lbl_scan_status, LV_OBJ_FLAG_HIDDEN);
    y += 46;

    /* Password field */
    lv_obj_t* pass_lbl = lv_label_create(parent);
    lv_label_set_text(pass_lbl, "PASSWORD:");
    lv_obj_set_pos(pass_lbl, MB_SPACE_LG, y + 8);
    lv_obj_set_style_text_color(pass_lbl, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(pass_lbl, MB_FONT_SMALL, 0);

    _ta_pass = lv_textarea_create(parent);
    lv_obj_set_size(_ta_pass, 260, 38);
    lv_obj_set_pos(_ta_pass, 92, y);
    lv_textarea_set_one_line(_ta_pass, true);
    lv_textarea_set_placeholder_text(_ta_pass, "Enter password");
    
    /* Pre-fill saved password */
    extern WiFiManager wifiMgr;
    String pass = wifiMgr.getPass();
    if (pass.length() > 0) {
        lv_textarea_set_text(_ta_pass, pass.c_str());
    }

    lv_obj_set_style_bg_color(_ta_pass, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_opa(_ta_pass, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_ta_pass, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_border_color(_ta_pass, MB_COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(_ta_pass, 1, 0);
    lv_obj_set_style_radius(_ta_pass, MB_BORDER_RADIUS, 0);
    lv_obj_add_event_cb(_ta_pass, _ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(_ta_pass, _ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);
    y += 48;

    /* Connect button */
    lv_obj_t* conn_btn = lv_btn_create(parent);
    lv_obj_set_size(conn_btn, 140, 40);
    lv_obj_set_pos(conn_btn, 92, y);
    mb_style_transport_btn(conn_btn, MB_COLOR_ACCENT_SEAFOAM);
    lv_obj_t* conn_lbl = lv_label_create(conn_btn);
    lv_label_set_text(conn_lbl, LV_SYMBOL_OK " CONNECT");
    lv_obj_set_style_text_font(conn_lbl, MB_FONT_LABEL, 0);
    lv_obj_center(conn_lbl);
    lv_obj_add_event_cb(conn_btn, wifi_connect_from_settings, LV_EVENT_CLICKED, NULL);
    y += 58;

    /* ═══ Connection Mode Section ═══ */
    lv_obj_t* cm_title = mb_create_caption_label(parent,
        LV_SYMBOL_USB "  C O N N E C T I O N   M O D E");
    lv_obj_set_pos(cm_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(cm_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;

    lv_obj_t* cm_desc = lv_label_create(parent);
    lv_label_set_text(cm_desc, "Data source: WiFi/UDP  |  USB Serial  |  Both");
    lv_obj_set_pos(cm_desc, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(cm_desc, MB_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(cm_desc, MB_FONT_SMALL, 0);
    y += 20;

    static const char* CONN_LABELS[] = {LV_SYMBOL_WIFI " WiFi",
                                        LV_SYMBOL_USB  " Serial",
                                        LV_SYMBOL_WIFI "+USB   Both"};
    for (int ci = 0; ci < 3; ci++) {
        _conn_btns[ci] = lv_btn_create(parent);
        lv_obj_set_size(_conn_btns[ci], 120, 38);
        lv_obj_set_pos(_conn_btns[ci], MB_SPACE_LG + ci * 128, y);
        lv_obj_add_flag(_conn_btns[ci], LV_OBJ_FLAG_CHECKABLE);
        mb_style_transport_btn(_conn_btns[ci], MB_COLOR_ACCENT_BLUE);
        if (ci == (int)g_conn_mode)
            lv_obj_add_state(_conn_btns[ci], LV_STATE_CHECKED);
        lv_obj_t* cl = lv_label_create(_conn_btns[ci]);
        lv_label_set_text(cl, CONN_LABELS[ci]);
        lv_obj_set_style_text_font(cl, MB_FONT_SMALL, 0);
        lv_obj_center(cl);
        lv_obj_add_event_cb(_conn_btns[ci], _conn_mode_btn_cb,
                            LV_EVENT_CLICKED, (void*)(uintptr_t)ci);
    }
    y += 52;

    /* ═══ Tick Rate Section ═══ */
    lv_obj_t* tr_title = mb_create_caption_label(parent,
        LV_SYMBOL_REFRESH "  T I C K   R A T E");
    lv_obj_set_pos(tr_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(tr_title, MB_COLOR_ACCENT_AMBER, 0);
    y += 22;

    lv_obj_t* tr_desc = lv_label_create(parent);
    lv_label_set_text(tr_desc, "How often REAPER data is sent to this display");
    lv_obj_set_pos(tr_desc, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(tr_desc, MB_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(tr_desc, MB_FONT_SMALL, 0);
    y += 20;

    /* Preset table: {label, update_ms} */
    static const char* TR_DD_OPTIONS =
        "Max  50/s  (20ms)\n"
        "Fast 20/s  (50ms) default\n"
        "Std  10/s  (100ms)\n"
        "Smooth 4/s (250ms)\n"
        "Slow  2/s  (500ms)\n"
        "Min  1/s   (1000ms)";
    static const uint32_t TR_MS_VALUES[] = {20, 50, 100, 250, 500, 1000};
    static const uint8_t  TR_COUNT = 6;

    /* Find current selection index from g_update_interval_ms */
    uint8_t tr_sel = 1;  /* default: 50ms */
    for (uint8_t ti = 0; ti < TR_COUNT; ti++) {
        if (TR_MS_VALUES[ti] == g_update_interval_ms) { tr_sel = ti; break; }
    }

    _tick_rate_dd = lv_dropdown_create(parent);
    lv_obj_set_size(_tick_rate_dd, 280, 42);
    lv_obj_set_pos(_tick_rate_dd, MB_SPACE_LG, y);
    lv_dropdown_set_options(_tick_rate_dd, TR_DD_OPTIONS);
    lv_dropdown_set_selected(_tick_rate_dd, tr_sel);
    lv_obj_set_style_bg_color(_tick_rate_dd, MB_COLOR_BG_ELEVATED, 0);
    lv_obj_set_style_text_color(_tick_rate_dd, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(_tick_rate_dd, MB_FONT_SMALL, 0);
    lv_obj_add_event_cb(_tick_rate_dd, [](lv_event_t* e){
        uint8_t idx = (uint8_t)lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e));
        static const uint32_t VALS[] = {20, 50, 100, 250, 500, 1000};
        if (idx < 6) {
            uint32_t ms = VALS[idx];
            g_update_interval_ms = ms;
            Preferences prefs;
            prefs.begin("meterbridge", false);
            prefs.putULong("update_ms", ms);
            prefs.end();
            /* Notify relay via serial so it can adjust without restart */
            Serial.printf("UPDATE_MS_SET:%u\n", (unsigned)ms);
            Serial.printf("[CFG] TickRate -> %u ms (%.1f/s)\n",
                          (unsigned)ms, 1000.0f / (float)ms);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    y += 56;

    lv_obj_t* src_title = mb_create_caption_label(parent, LV_SYMBOL_AUDIO "  M E T E R   S O U R C E");
    lv_obj_set_pos(src_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(src_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 24;

    lv_obj_t* master_btn = lv_btn_create(parent);
    lv_obj_set_size(master_btn, 130, 38);
    lv_obj_set_pos(master_btn, MB_SPACE_LG, y);
    mb_style_transport_btn(master_btn, MB_COLOR_ACCENT_PURPLE);
    lv_obj_add_flag(master_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(master_btn, LV_STATE_CHECKED);
    lv_obj_t* ml = lv_label_create(master_btn);
    lv_label_set_text(ml, LV_SYMBOL_AUDIO " MASTER");
    lv_obj_set_style_text_font(ml, MB_FONT_LABEL, 0);
    lv_obj_center(ml);

    lv_obj_t* sel_btn = lv_btn_create(parent);
    lv_obj_set_size(sel_btn, 130, 38);
    lv_obj_set_pos(sel_btn, MB_SPACE_LG + 140, y);
    mb_style_transport_btn(sel_btn, MB_COLOR_ACCENT_PURPLE);
    lv_obj_add_flag(sel_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_t* sl = lv_label_create(sel_btn);
    lv_label_set_text(sl, LV_SYMBOL_SHUFFLE " SELECTED");
    lv_obj_set_style_text_font(sl, MB_FONT_LABEL, 0);
    lv_obj_center(sl);
    y += 52;

    /* ═══ Display Section ═══ */
    lv_obj_t* disp_title = mb_create_caption_label(parent, LV_SYMBOL_IMAGE "  D I S P L A Y");
    lv_obj_set_pos(disp_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(disp_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 24;

    lv_obj_t* bright_lbl = lv_label_create(parent);
    lv_label_set_text(bright_lbl, "BRIGHTNESS");
    lv_obj_set_pos(bright_lbl, MB_SPACE_LG, y + 4);
    lv_obj_set_style_text_color(bright_lbl, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(bright_lbl, MB_FONT_SMALL, 0);

    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_size(slider, 220, 14);
    lv_obj_set_pos(slider, 120, y + 4);
    lv_slider_set_range(slider, 10, 255);
    lv_slider_set_value(slider, 200, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_color(slider, MB_COLOR_ACCENT_PURPLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, MB_COLOR_ACCENT_MAGENTA, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 8, 0);
    lv_obj_set_style_radius(slider, 8, LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider, _brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    y += 42;

    /* === Rotation Section === */
    lv_obj_t* rot_title = mb_create_caption_label(parent,
        LV_SYMBOL_REFRESH "  R O T A T I O N");
    lv_obj_set_pos(rot_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(rot_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;
    static const char* ROT_LABELS[] = {"0 deg", "90 deg", "180 deg", "270 deg"};
    for (int ri = 0; ri < 4; ri++) {
        _rot_btns[ri] = lv_btn_create(parent);
        lv_obj_set_size(_rot_btns[ri], 80, 36);
        lv_obj_set_pos(_rot_btns[ri], MB_SPACE_LG + ri * 86, y);
        lv_obj_add_flag(_rot_btns[ri], LV_OBJ_FLAG_CHECKABLE);
        mb_style_transport_btn(_rot_btns[ri], MB_COLOR_ACCENT_PURPLE);
        if (ri == (int)g_display_rotation)
            lv_obj_add_state(_rot_btns[ri], LV_STATE_CHECKED);
        lv_obj_t* rl = lv_label_create(_rot_btns[ri]);
        lv_label_set_text(rl, ROT_LABELS[ri]);
        lv_obj_set_style_text_font(rl, MB_FONT_SMALL, 0);
        lv_obj_center(rl);
        lv_obj_add_event_cb(_rot_btns[ri], _rotation_btn_cb,
                            LV_EVENT_CLICKED, (void*)(uintptr_t)ri);
    }
    y += 52;

    /* === Peak Hold Section === */
    lv_obj_t* pk_title = mb_create_caption_label(parent,
        LV_SYMBOL_STOP "  P E A K   H O L D");
    lv_obj_set_pos(pk_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(pk_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;

    lv_obj_t* pk_lbl = lv_label_create(parent);
    lv_label_set_text(pk_lbl, "HOLD TIME:");
    lv_obj_set_pos(pk_lbl, MB_SPACE_LG, y + 9);
    lv_obj_set_style_text_color(pk_lbl, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(pk_lbl, MB_FONT_SMALL, 0);

    _peak_hold_dd = lv_dropdown_create(parent);
    lv_obj_set_size(_peak_hold_dd, 220, 38);
    lv_obj_set_pos(_peak_hold_dd, 104, y);
    lv_dropdown_set_options(_peak_hold_dd,
        "0.5 s  (Snappy)\n"
        "1 s\n"
        "1.5 s\n"
        "2 s\n"
        "2.5 s  (Default)\n"
        "3 s\n"
        "5 s  (Long)\n"
        "Hold Forever");
    {
        static const uint32_t HOLD_VALUES[] = {500,1000,1500,2000,2500,3000,5000,0};
        uint16_t pk_sel = 4;
        uint32_t stored = (g_peak_hold_ms == UINT32_MAX) ? 0 : g_peak_hold_ms;
        for (int pi = 0; pi < 8; pi++) {
            if (HOLD_VALUES[pi] == stored) { pk_sel = pi; break; }
        }
        lv_dropdown_set_selected(_peak_hold_dd, pk_sel);
    }
    lv_obj_set_style_bg_color(_peak_hold_dd, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_opa(_peak_hold_dd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_peak_hold_dd, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(_peak_hold_dd, MB_FONT_LABEL, 0);
    lv_obj_set_style_border_color(_peak_hold_dd, MB_COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(_peak_hold_dd, 1, 0);
    lv_obj_set_style_radius(_peak_hold_dd, MB_BORDER_RADIUS, 0);
    {
        lv_obj_t* pk_ddlist = lv_dropdown_get_list(_peak_hold_dd);
        lv_obj_set_style_bg_color(pk_ddlist, MB_COLOR_BG_ELEVATED, 0);
        lv_obj_set_style_text_color(pk_ddlist, MB_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_border_color(pk_ddlist, MB_COLOR_ACCENT_PURPLE, 0);
        lv_obj_set_style_border_width(pk_ddlist, 1, 0);
    }
    lv_obj_add_event_cb(_peak_hold_dd, _peak_hold_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    y += 54;

    /* === Meter Height Section === */
    lv_obj_t* mh_title = mb_create_caption_label(parent,
        LV_SYMBOL_SETTINGS "  M E T E R   H E I G H T");
    lv_obj_set_pos(mh_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(mh_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;

    lv_obj_t* mh_lbl = lv_label_create(parent);
    lv_label_set_text(mh_lbl, "SIZE:");
    lv_obj_set_pos(mh_lbl, MB_SPACE_LG, y + 9);
    lv_obj_set_style_text_color(mh_lbl, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(mh_lbl, MB_FONT_SMALL, 0);

    lv_obj_t* mheight_dd = lv_dropdown_create(parent);
    lv_obj_set_size(mheight_dd, 220, 38);
    lv_obj_set_pos(mheight_dd, 104, y);
    lv_dropdown_set_options(mheight_dd,
        "XXL  (Giant)\n"
        "XL   (Default)\n"
        "L    (Large)\n"
        "M    (Medium)\n"
        "S    (Small)\n"
        "XS   (Minimal)");
    extern uint8_t g_meter_height_idx;
    lv_dropdown_set_selected(mheight_dd, g_meter_height_idx < 6 ? g_meter_height_idx : 1);
    lv_obj_set_style_bg_color(mheight_dd, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_opa(mheight_dd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(mheight_dd, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(mheight_dd, MB_FONT_LABEL, 0);
    lv_obj_set_style_border_color(mheight_dd, MB_COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(mheight_dd, 1, 0);
    lv_obj_set_style_radius(mheight_dd, MB_BORDER_RADIUS, 0);
    {
        lv_obj_t* mh_ddlist = lv_dropdown_get_list(mheight_dd);
        lv_obj_set_style_bg_color(mh_ddlist, MB_COLOR_BG_ELEVATED, 0);
        lv_obj_set_style_text_color(mh_ddlist, MB_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_border_color(mh_ddlist, MB_COLOR_ACCENT_PURPLE, 0);
        lv_obj_set_style_border_width(mh_ddlist, 1, 0);
    }
    lv_obj_add_event_cb(mheight_dd, _meter_height_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    y += 54;

    /* === Loudness Target Section === */
    lv_obj_t* lt_title = mb_create_caption_label(parent,
        LV_SYMBOL_AUDIO "  L O U D N E S S   T A R G E T");
    lv_obj_set_pos(lt_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(lt_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;


    /* Loudness dropdown spans full useful width */
    _loudness_dd = lv_dropdown_create(parent);
    lv_obj_set_size(_loudness_dd, disp_w - MB_SPACE_LG * 2, 38);
    lv_obj_set_pos(_loudness_dd, MB_SPACE_LG, y);
    lv_dropdown_set_options(_loudness_dd,
        "-14 LUFS  Streaming\n"
        "-16 LUFS  Podcast\n"
        "-23 LUFS  Broadcast (EBU)\n"
        "-27 LUFS  Film / Cinema\n"
        "-18 LUFS  Classical\n"
        "-12 LUFS  Hot Rock / EDM\n"
        "-10 LUFS  Very Hot\n"
        "-9 LUFS\n"
        "-8 LUFS\n"
        "-6 LUFS\n"
        "-4 LUFS\n"
        "-3 LUFS\n"
        "-2 LUFS\n"
        "-1 LUFS\n"
        "-0.1 dBFS  (Near-peak)\n"
        "Off  (No target)");
    lv_dropdown_set_selected(_loudness_dd, g_loudness_preset < NUM_LOUDNESS_PRESETS ? g_loudness_preset : 15);
    lv_obj_set_style_bg_color(_loudness_dd, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_opa(_loudness_dd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_loudness_dd, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(_loudness_dd, MB_FONT_LABEL, 0);
    lv_obj_set_style_border_color(_loudness_dd, MB_COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(_loudness_dd, 1, 0);
    lv_obj_set_style_radius(_loudness_dd, MB_BORDER_RADIUS, 0);
    {
        lv_obj_t* lt_ddlist = lv_dropdown_get_list(_loudness_dd);
        lv_obj_set_style_bg_color(lt_ddlist, MB_COLOR_BG_ELEVATED, 0);
        lv_obj_set_style_text_color(lt_ddlist, MB_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_border_color(lt_ddlist, MB_COLOR_ACCENT_PURPLE, 0);
        lv_obj_set_style_border_width(lt_ddlist, 1, 0);
    }
    lv_obj_add_event_cb(_loudness_dd, _loudness_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    y += 44;

    /* Target dB readout — sits on its own row below the dropdown */
    _lbl_loudness_val = lv_label_create(parent);
    if (g_loudness_preset == 15) {  /* Off = last preset, index 15 */
        lv_label_set_text(_lbl_loudness_val, "OFF");
        lv_obj_set_style_text_color(_lbl_loudness_val, MB_COLOR_TEXT_MUTED, 0);
    } else {
        static char lbuf_init[12];
        snprintf(lbuf_init, sizeof(lbuf_init), "%.0f LUFS", g_loudness_target);
        lv_label_set_text(_lbl_loudness_val, lbuf_init);
        lv_obj_set_style_text_color(_lbl_loudness_val, MB_COLOR_ACCENT_PURPLE, 0);
    }
    lv_obj_set_pos(_lbl_loudness_val, MB_SPACE_LG, y);
    lv_obj_set_style_text_font(_lbl_loudness_val, MB_FONT_VALUE, 0);
    y += 36;

    /* === Brightness Section === */
    lv_obj_t* br_title = mb_create_caption_label(parent,
        LV_SYMBOL_IMAGE "  B R I G H T N E S S");
    lv_obj_set_pos(br_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(br_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;

    /* Dim label */
    lv_obj_t* br_lo = lv_label_create(parent);
    lv_label_set_text(br_lo, LV_SYMBOL_MINUS);
    lv_obj_set_pos(br_lo, MB_SPACE_LG, y + 9);
    lv_obj_set_style_text_color(br_lo, MB_COLOR_TEXT_MUTED, 0);

    /* Slider */
    _brightness_slider = lv_slider_create(parent);
    lv_obj_set_size(_brightness_slider, 280, 20);
    lv_obj_set_pos(_brightness_slider, MB_SPACE_LG + 20, y + 5);
    lv_slider_set_range(_brightness_slider, 10, 100);
    lv_slider_set_value(_brightness_slider, (int)g_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_brightness_slider, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_color(_brightness_slider, MB_COLOR_ACCENT_PURPLE,
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_brightness_slider, MB_COLOR_TEXT_PRIMARY,
                              LV_PART_KNOB);
    lv_obj_set_style_pad_all(_brightness_slider, 5, LV_PART_KNOB);
    lv_obj_add_event_cb(_brightness_slider, _brightness_slider_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);

    /* Bright label */
    lv_obj_t* br_hi = lv_label_create(parent);
    lv_label_set_text(br_hi, LV_SYMBOL_PLUS);
    lv_obj_set_pos(br_hi, MB_SPACE_LG + 20 + 290, y + 9);
    lv_obj_set_style_text_color(br_hi, MB_COLOR_TEXT_MUTED, 0);

    /* % readout */
    _brightness_label = lv_label_create(parent);
    {
        static char brinit[8];
        lv_snprintf(brinit, sizeof(brinit), "%d%%", (int)g_brightness);
        lv_label_set_text(_brightness_label, brinit);
    }
    lv_obj_set_pos(_brightness_label, MB_SPACE_LG + 20 + 300, y + 7);
    lv_obj_set_style_text_color(_brightness_label, MB_COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_text_font(_brightness_label, MB_FONT_LABEL, 0);
    y += 46;

    /* === Theme Section === */
    lv_obj_t* th_title = mb_create_caption_label(parent,
        LV_SYMBOL_IMAGE "  T H E M E");
    lv_obj_set_pos(th_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(th_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 22;
    /* Theme grid: 2 columns to safely fit 480px portrait width */
    {
        int btn_w = (disp_w - MB_SPACE_LG * 3) / 2;
        int btn_h = 38;
        int col_gap = MB_SPACE_LG;
        for (int ti = 0; ti < (int)MB_THEME_COUNT; ti++) {
            int col = ti % 2;
            int row = ti / 2;
            _theme_btns[ti] = lv_btn_create(parent);
            lv_obj_set_size(_theme_btns[ti], btn_w, btn_h);
            lv_obj_set_pos(_theme_btns[ti], MB_SPACE_LG + col * (btn_w + col_gap), y + row * 46);
            lv_obj_add_flag(_theme_btns[ti], LV_OBJ_FLAG_CHECKABLE);
            mb_style_transport_btn(_theme_btns[ti], MB_COLOR_ACCENT_PURPLE);
            if (ti == (int)g_current_theme)
                lv_obj_add_state(_theme_btns[ti], LV_STATE_CHECKED);
            lv_obj_t* tl = lv_label_create(_theme_btns[ti]);
            lv_label_set_text(tl, MB_THEME_NAMES[ti]);
            lv_obj_set_style_text_font(tl, MB_FONT_SMALL, 0);
            lv_obj_center(tl);
            lv_obj_add_event_cb(_theme_btns[ti], _theme_btn_cb,
                                LV_EVENT_CLICKED, (void*)(uintptr_t)ti);
        }
    }
    y += ((int)MB_THEME_COUNT / 2 + ((int)MB_THEME_COUNT % 2 ? 1 : 0)) * 46 + 10;


    /* ═══ Spectrum Mode Section ═══ */
    lv_obj_t* sm_title = mb_create_caption_label(parent, LV_SYMBOL_AUDIO "  S P E C T R U M   T H E M E");
    lv_obj_set_pos(sm_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(sm_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 24;
    static const char* SM_NAMES[] = {"Classic", "Neon", "Fire", "Ocean", "Rainbow", "Ghost"};
    for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
        _spec_mode_btns[i] = lv_btn_create(parent);
        lv_obj_set_size(_spec_mode_btns[i], 168, 38);
        lv_obj_set_pos(_spec_mode_btns[i], MB_SPACE_LG + col * 176, y + row * 46);
        lv_obj_add_flag(_spec_mode_btns[i], LV_OBJ_FLAG_CHECKABLE);
        mb_style_transport_btn(_spec_mode_btns[i], MB_COLOR_ACCENT_BLUE);
        if (i == (int)g_spec_color_mode)
            lv_obj_add_state(_spec_mode_btns[i], LV_STATE_CHECKED);
        lv_obj_t* sl = lv_label_create(_spec_mode_btns[i]);
        lv_label_set_text(sl, SM_NAMES[i]);
        lv_obj_set_style_text_font(sl, MB_FONT_SMALL, 0);
        lv_obj_center(sl);
        lv_obj_add_event_cb(_spec_mode_btns[i], _spec_mode_btn_cb,
                            LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
    y += 2 * 46 + 4;

    /* PREVIEW Button for Spectrum analyzer (directly to the analyzer screen) */
    lv_obj_t* preview_btn = lv_btn_create(parent);
    lv_obj_set_size(preview_btn, 180, 42);
    lv_obj_set_pos(preview_btn, MB_SPACE_LG, y);
    mb_style_transport_btn(preview_btn, MB_COLOR_ACCENT_SEAFOAM);
    lv_obj_t* prl = lv_label_create(preview_btn);
    lv_label_set_text(prl, LV_SYMBOL_EYE_OPEN " PREVIEW ANALYZER");
    lv_obj_set_style_text_font(prl, MB_FONT_LABEL, 0);
    lv_obj_center(prl);
    lv_obj_add_event_cb(preview_btn, _navigate_to_spectrum, LV_EVENT_CLICKED, NULL);

    y += 52;

    /* === On-screen keyboard (hidden) === */
    _kb = lv_keyboard_create(parent);
    lv_obj_set_size(_kb, disp_w, 200);
    lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(_kb, MB_COLOR_BG_ELEVATED, 0);
    lv_obj_set_style_bg_opa(_kb, LV_OPA_90, 0);
    lv_obj_set_style_text_color(_kb, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_border_color(_kb, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_border_width(_kb, 1, 0);
    lv_obj_add_event_cb(_kb, _kb_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(_kb, _kb_event_cb, LV_EVENT_CANCEL, NULL);

    /* === Version info === */
    lv_obj_t* ver = lv_label_create(parent);
    lv_label_set_text(ver, "M E T E R B R I D G E  v 2 . 0");
    lv_obj_set_pos(ver, MB_SPACE_LG, y + 4);
    lv_obj_set_style_text_color(ver, MB_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(ver, MB_FONT_SMALL, 0);
    lv_obj_set_style_text_letter_space(ver, 2, 0);
    y += 30;

    /* === Display Options Section === */
    lv_obj_t* fps_sect_title = mb_create_caption_label(parent,
        LV_SYMBOL_EYE_OPEN "  D I S P L A Y  O P T I O N S");
    lv_obj_set_pos(fps_sect_title, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(fps_sect_title, MB_COLOR_ACCENT_SEAFOAM, 0);
    y += 24;

    /* FPS counter checkbox */
    lv_obj_t* fps_cb = lv_checkbox_create(parent);
    lv_checkbox_set_text(fps_cb, "Show FPS counter");
    lv_obj_set_pos(fps_cb, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(fps_cb, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(fps_cb, MB_FONT_SMALL, 0);
    /* Checkbox box style */
    lv_obj_set_style_bg_color(fps_cb, MB_COLOR_BG_TERTIARY, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(fps_cb, MB_COLOR_ACCENT_PURPLE, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(fps_cb, 1, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(fps_cb, MB_COLOR_ACCENT_PURPLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (g_show_fps) lv_obj_add_state(fps_cb, LV_STATE_CHECKED);
    lv_obj_add_event_cb(fps_cb, _fps_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);
    y += 32;

    /* Auto-dim screensaver checkbox — non-default, clearly opt-in */
    extern bool g_auto_dim_enabled;
    lv_obj_t* dim_cb = lv_checkbox_create(parent);
    lv_checkbox_set_text(dim_cb, "Auto-dim screensaver (5/10 min, opt-in)");
    lv_obj_set_pos(dim_cb, MB_SPACE_LG, y);
    lv_obj_set_style_text_color(dim_cb, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(dim_cb, MB_FONT_SMALL, 0);
    lv_obj_set_style_bg_color(dim_cb, MB_COLOR_BG_TERTIARY, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(dim_cb, MB_COLOR_ACCENT_AMBER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(dim_cb, 1, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(dim_cb, MB_COLOR_ACCENT_AMBER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (g_auto_dim_enabled) lv_obj_add_state(dim_cb, LV_STATE_CHECKED);
    lv_obj_add_event_cb(dim_cb, [](lv_event_t* e) {
        lv_obj_t* cb = lv_event_get_target(e);
        g_auto_dim_enabled = (lv_obj_get_state(cb) & LV_STATE_CHECKED) != 0;
        Preferences prefs;
        prefs.begin("meterbridge", false);
        prefs.putBool("auto_dim", g_auto_dim_enabled);
        prefs.end();
        Serial.printf("[CFG] AutoDim -> %d\n", (int)g_auto_dim_enabled);
    }, LV_EVENT_VALUE_CHANGED, NULL);


    _ta_ssid = lv_textarea_create(parent);
    lv_obj_add_flag(_ta_ssid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(_ta_ssid, 1, 1);
}

/**
 * Update settings screen — call from ui_update() when settings is visible.
 * Polls async WiFi scan and refreshes status label.
 */
void _settings_screen_update(void) {
    _update_wifi_status_label();
}

/**
 * Get the currently selected SSID from the dropdown.
 */
static const char* _settings_get_selected_ssid(void) {
    if (!_dd_ssid || _scan_count <= 0) return "";
    uint16_t sel = lv_dropdown_get_selected(_dd_ssid);
    if (sel >= _scan_count) return "";
    return _scanned_ssids[sel];
}

#endif /* SCREEN_SETTINGS_H */
