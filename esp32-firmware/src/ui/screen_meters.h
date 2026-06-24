/*
 * MeterBridge — Main Meter Screen
 *
 * LANDSCAPE: horizontal bars | sidebar readouts (LUFS-M/S/I/LRA/Phase/Clips)
 * PORTRAIT (Option B): full-width bars | LUFS strip row | info panel
 *
 * All sidebar/strip label pointers stay the same regardless of orientation.
 * The update function works unchanged in both orientations.
 */

#ifndef SCREEN_METERS_H
#define SCREEN_METERS_H

#include "../network/udp_comm.h"
#include "theme.h"
#include <lvgl.h>

/* ─── Meter Mode (extern from main.cpp) ──────────────────────── */
extern uint8_t g_meter_mode;   /* 0=Classic 1=LRPeak 2=Mid-Side 3=LUFS */
/* Navigation to spectrum screen (defined in ui_manager.h) */
extern void _navigate_to_spectrum(lv_event_t* e);

/* Forward declaration for toast system (defined in ui_manager.h which
 * is always included before this file does its first render call)    */
void ui_show_toast(const char* msg);

/* Mode names for toast display */
/* Note: inline short_names[] inside the lambda callback is used instead */

/* ─── Meter Bar Configuration ────────────────────────────────── */

#define NUM_METER_BARS 6
#define METER_BAR_H 26
#define METER_GAP 6
#define METER_LABEL_W 58
#define METER_VALUE_W 60
#define METER_PEAK_HOLD_W 3

/* Segmented LED bar */
#define METER_NUM_SEGS 24
#define METER_SEG_GAP 2

/* dB range */
#define METER_DB_MIN -70.0f
#define METER_DB_MAX 12.0f
#define METER_DB_RANGE (METER_DB_MAX - METER_DB_MIN)

/* Meter type identifiers */
typedef enum {
  MTR_PEAK_L = 0,
  MTR_PEAK_R,
  MTR_RMS_L,
  MTR_RMS_R,
  MTR_TRUEPK_L,
  MTR_TRUEPK_R
} meter_id_t;

static const char *meter_labels[] = {"PEAK L", "PEAK R", "RMS  L",
                                     "RMS  R", "TP   L", "TP   R"};

/* ─── Meter UI Elements ──────────────────────────────────────── */

typedef struct {
  lv_obj_t *bar_bg;
  lv_obj_t *bar_fill;          /* legacy solid fill (hidden in segment mode) */
  lv_obj_t *segs[METER_NUM_SEGS]; /* discrete LED segments */
  lv_obj_t *peak_hold;
  lv_obj_t *label;
  lv_obj_t *value;
  float current_db;
  float peak_db;
  float display_db;
  uint32_t peak_time;
  int8_t   last_lit;      /* last segment count that was lit — dirty-bit opt */
  int8_t   last_peak_seg; /* last peak hold segment position */
} meter_bar_t;

static meter_bar_t _meters[NUM_METER_BARS];

/* ── Sidebar / strip label pointers (shared between landscape & portrait) ── */
static lv_obj_t *_lbl_lufs_m_val = NULL;
static lv_obj_t *_lbl_lufs_s_val = NULL;
static lv_obj_t *_lbl_lufs_i_val = NULL;
static lv_obj_t *_lbl_lufs_r_val = NULL;
static lv_obj_t *_lbl_phase_val  = NULL;
static lv_obj_t *_lbl_clip_val   = NULL;
static lv_obj_t *_btn_meter_src      = NULL;
static lv_obj_t *_lbl_meter_src_btn  = NULL;
static lv_obj_t *_bar_lufs_m = NULL;
static lv_obj_t *_bar_lufs_s = NULL;
static lv_obj_t *_bar_phase  = NULL;

/* ── Portrait-only info panel labels ── */
static lv_obj_t *_lbl_portrait_lufs_big = NULL;   /* Large LUFS-I value  */
static lv_obj_t *_lbl_portrait_track    = NULL;   /* Track name          */
static lv_obj_t *_lbl_portrait_project  = NULL;   /* Project name        */
static lv_obj_t *_lbl_portrait_section  = NULL;   /* Section / region    */
static lv_obj_t *_lbl_portrait_position = NULL;   /* Playback position   */
static lv_obj_t *_swatch_portrait_track = NULL;   /* Track color swatch  */

static lv_obj_t *_lbl_max_peak_val = NULL;
static float _session_max_tp_l = -70.0f;  /* Session MAX True Peak L */
static float _session_max_tp_r = -70.0f;  /* Session MAX True Peak R */

/* === Beat Pulse indicator === */
static lv_obj_t *_beat_pulse_dot     = NULL;
static uint16_t  _last_beat_in_bar   = 0;
static uint32_t  _beat_pulse_end_ms  = 0;

/* === Stereo Balance Bar === */
static lv_obj_t *_balance_bar_fill   = NULL;
static lv_obj_t *_balance_bar_center = NULL;

/* === LUFS History Graph === */
#define LUFS_HIST_SAMPLES 120  /* 120 × 250ms ≈ 30 seconds */
static float   _lufs_hist[LUFS_HIST_SAMPLES];  /* ring buffer */
static uint8_t _lufs_hist_head = 0;
static bool    _lufs_hist_full = false;
static lv_obj_t *_lufs_graph_canvas = NULL;
static lv_color_t *_lufs_graph_buf  = NULL;
#define LUFS_GRAPH_W 200
#define LUFS_GRAPH_H 48

/* === dBFS Reference Marker (drawn on meter bars in update) === */
extern float g_ref_db_line;  /* -18.0 default, from main.cpp */
extern bool  g_auto_dim_enabled;  /* used in settings only */

/* Mode cycle button label */
static lv_obj_t *_lbl_mode_btn = NULL;

/* Loudness target indicator on meter bars */
static lv_obj_t *_loudness_target_line = NULL;

static int meter_panel_h = 0;

/* ─── dB helpers ─────────────────────────────────────────────── */

static inline int db_to_px(float db, int bar_width) {
  if (db <= METER_DB_MIN) return 0;
  if (db >= METER_DB_MAX) return bar_width;
  return (int)((db - METER_DB_MIN) / METER_DB_RANGE * bar_width);
}

static inline lv_color_t db_to_color(float db) {
  if (db >= 0.0f)   return MB_COLOR_METER_CLIP;
  if (db >= -0.01f) return MB_COLOR_METER_RED;
  if (db >= -6.0f)  return MB_COLOR_METER_ORANGE;
  if (db >= -12.0f) return MB_COLOR_METER_YELLOW;
  return MB_COLOR_METER_GREEN;
}

/* ─── Segment dB thresholds & color mapping ──────────────────── */
/* Maps segment index [0..23] to a dB value and returns the color  */
static float _seg_db(int seg_idx) {
  /* 24 segments spanning -60 to +6 dB (2.75 dB per segment) */
  return -60.0f + (float)seg_idx * (66.0f / (float)METER_NUM_SEGS);
}

static lv_color_t _seg_color(int seg_idx) {
  if (seg_idx >= 22) return MB_COLOR_METER_CLIP;    /* +3 to +6  → red/clip  */
  if (seg_idx >= 20) return MB_COLOR_METER_RED;      /* -0.5 to +3 → red      */
  if (seg_idx >= 18) return MB_COLOR_METER_ORANGE;   /* -6 to -0.5 → orange   */
  if (seg_idx >= 14) return MB_COLOR_METER_YELLOW;   /* -17 to -6  → yellow   */
  return MB_COLOR_METER_GREEN;                        /* -60 to -17 → green    */
}

/* Dimmed version for unlit segments (LUT cached for performance) */
static lv_color_t _seg_dim_lut[METER_NUM_SEGS];
static bool _dim_lut_init = false;

static lv_color_t _seg_color_dim(int seg_idx) {
  if (!_dim_lut_init) {
    for (int i = 0; i < METER_NUM_SEGS; i++) {
        lv_color_t c = _seg_color(i);
        /* LVGL 8: extract 5-6-5 bit components, scale by ~15%, reconstruct */
        uint8_t r8 = (uint8_t)((LV_COLOR_GET_R(c) * 255 / 31) * 15 / 100);
        uint8_t g8 = (uint8_t)((LV_COLOR_GET_G(c) * 255 / 63) * 15 / 100);
        uint8_t b8 = (uint8_t)((LV_COLOR_GET_B(c) * 255 / 31) * 15 / 100);
        _seg_dim_lut[i] = lv_color_make(r8, g8, b8);
    }
    _dim_lut_init = true;
  }
  return _seg_dim_lut[seg_idx];
}

/* ─── Create Meter Bar (segmented LED style) ─────────────────── */

static void _create_meter_bar(lv_obj_t *parent, meter_bar_t *m,
                               const char *name, int x_pos, int y_pos, int bar_w, int bar_h, int bar_dim, bool vertical) {
  
  /* Background / Container */
  m->bar_bg = lv_obj_create(parent);
  lv_obj_set_pos(m->bar_bg, x_pos, y_pos);
  lv_obj_set_size(m->bar_bg, bar_w, bar_h);
  lv_obj_set_style_bg_color(m->bar_bg, lv_color_hex(0x0A0A12), 0);
  lv_obj_set_style_bg_opa(m->bar_bg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(m->bar_bg, MB_COLOR_BORDER, 0);
  lv_obj_set_style_border_width(m->bar_bg, 1, 0);
  lv_obj_set_style_radius(m->bar_bg, 1, 0);
  lv_obj_set_style_pad_all(m->bar_bg, 2, 0);
  lv_obj_clear_flag(m->bar_bg, LV_OBJ_FLAG_SCROLLABLE);

  /* Label creation */
  m->label = lv_label_create(parent);
  lv_label_set_text(m->label, name);
  lv_obj_set_style_text_color(m->label, MB_COLOR_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(m->label, MB_FONT_SMALL, 0);
  lv_obj_set_style_text_letter_space(m->label, 1, 0);

  /* dB value readout */
  m->value = lv_label_create(parent);
  lv_label_set_text(m->value, "--.-");
  lv_obj_set_style_text_color(m->value, MB_COLOR_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(m->value, MB_FONT_LABEL, 0);

  if (vertical) {
    /* Vertical Layout: Value at top, then Label, then Bar below */
    lv_obj_set_style_text_align(m->label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(m->value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(m->label, bar_dim + 12); /* slightly wider for label text */
    lv_obj_set_width(m->value, bar_dim + 12);
    
    lv_obj_align_to(m->value, m->bar_bg, LV_ALIGN_OUT_TOP_MID, 0, -2);
    lv_obj_align_to(m->label, m->value, LV_ALIGN_OUT_TOP_MID, 0, -2);
  } else {
    /* Horizontal Layout: Label Left, Bar Center, Value Right */
    lv_obj_set_pos(m->label, MB_SPACE_SM, y_pos + 4);
    lv_obj_set_pos(m->value, x_pos + bar_dim + MB_SPACE_SM, y_pos + 4);
  }

  /* Hidden legacy solid fill (kept for compatibility) */
  m->bar_fill = lv_obj_create(m->bar_bg);
  lv_obj_remove_style_all(m->bar_fill);
  lv_obj_add_flag(m->bar_fill, LV_OBJ_FLAG_HIDDEN);

  /* Create discrete segments */
  int inner_w = vertical ? bar_w - 4 : bar_w - 6;
  int inner_h = vertical ? bar_h - 6 : bar_h - 6;
  
  int seg_total = vertical ? inner_h : inner_w;
  int seg_step = (seg_total - (METER_NUM_SEGS - 1) * METER_SEG_GAP) / METER_NUM_SEGS;

  for (int i = 0; i < METER_NUM_SEGS; i++) {
    m->segs[i] = lv_obj_create(m->bar_bg);
    lv_obj_remove_style_all(m->segs[i]);
    int step_px = i * (seg_step + METER_SEG_GAP);

    if (vertical) {
        /* Vertical: segments stack bottom-up */
        lv_obj_set_size(m->segs[i], inner_w, seg_step);
        lv_obj_set_pos(m->segs[i], 0, inner_h - step_px - seg_step);
    } else {
        /* Horizontal: segments stack left-to-right */
        lv_obj_set_size(m->segs[i], seg_step, inner_h);
        lv_obj_set_pos(m->segs[i], step_px, 0);
    }
    
    lv_obj_set_style_bg_color(m->segs[i], _seg_color_dim(i), 0);
    lv_obj_set_style_bg_opa(m->segs[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(m->segs[i], 1, 0);
  }

  /* Peak hold indicator */
  m->peak_hold = lv_obj_create(m->bar_bg);
  lv_obj_remove_style_all(m->peak_hold);
  if (vertical) lv_obj_set_size(m->peak_hold, inner_w, 2);
  else          lv_obj_set_size(m->peak_hold, METER_PEAK_HOLD_W, inner_h);
  
  lv_obj_set_style_bg_color(m->peak_hold, MB_COLOR_TEXT_PRIMARY, 0);
  lv_obj_set_style_bg_opa(m->peak_hold, LV_OPA_70, 0);
  lv_obj_add_flag(m->peak_hold, LV_OBJ_FLAG_HIDDEN);

  m->current_db  = METER_DB_MIN;
  m->peak_db     = METER_DB_MIN;
  m->display_db  = METER_DB_MIN;
  m->peak_time   = 0;
  m->last_lit    = -1;
  m->last_peak_seg = -1;
}

/* ─── Update Meter Bar (segmented, dirty-optimized) ─────────────────────── */


static void _update_meter_bar(meter_bar_t *m, float db, int bar_dim, bool vertical) {
  m->current_db = db;

  /* Smoothing — fast attack, slow release */
  float target = db;
  if (target > m->display_db) {
    m->display_db = target;
  } else {
    if (m->display_db <= METER_DB_MIN + 0.1f && target <= METER_DB_MIN + 0.1f) {
        m->display_db = METER_DB_MIN; /* Snap to floor to prevent micro-fluctuation dirty-marks */
    } else {
        m->display_db += (target - m->display_db) * 0.15f;
    }
  }

  /* Peak hold */
  if (db > m->peak_db || millis() - m->peak_time > g_peak_hold_ms) {
    m->peak_db   = db;
    m->peak_time = millis();
  }

  /* Compute how many segments should be lit */
  float disp = m->display_db;
  int inner_dim = vertical ? bar_dim - 6 : bar_dim - 6;
  int seg_step = (inner_dim - (METER_NUM_SEGS - 1) * METER_SEG_GAP) / METER_NUM_SEGS;

  int new_lit = 0;
  for (int i = 0; i < METER_NUM_SEGS; i++) {
    if (disp >= _seg_db(i)) new_lit = i + 1;
  }

  /* Dirty-optimised segment update */
  if (new_lit != (int)m->last_lit) {
    int lo = (new_lit < (int)m->last_lit) ? new_lit        : (int)m->last_lit;
    int hi = (new_lit < (int)m->last_lit) ? (int)m->last_lit : new_lit;
    for (int i = lo; i < hi; i++) {
        if (i < new_lit) lv_obj_set_style_bg_color(m->segs[i], _seg_color(i),     0);
        else             lv_obj_set_style_bg_color(m->segs[i], _seg_color_dim(i), 0);
    }
    m->last_lit = (int8_t)new_lit;
  }

  /* Peak hold marker */
  int peak_seg = 0;
  for (int i = METER_NUM_SEGS - 1; i >= 0; i--) {
    if (m->peak_db >= _seg_db(i)) { peak_seg = i; break; }
  }
  if (m->peak_db > METER_DB_MIN + 5.0f) {
    if (peak_seg != (int)m->last_peak_seg) {
      int step_px = peak_seg * (seg_step + METER_SEG_GAP);
      if (vertical) lv_obj_set_pos(m->peak_hold, 0, inner_dim - step_px - 2);
      else          lv_obj_set_pos(m->peak_hold, step_px, 0);
      
      lv_obj_clear_flag(m->peak_hold, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(m->peak_hold, _seg_color(peak_seg), 0);
      m->last_peak_seg = (int8_t)peak_seg;
    }
  } else {
    if (m->last_peak_seg != -1) {
      lv_obj_add_flag(m->peak_hold, LV_OBJ_FLAG_HIDDEN);
      m->last_peak_seg = -1;
    }
  }

  /* Numeric readout — only update text when value changes meaningfully */
  static char buf[8][12];  /* one buffer per bar (up to 8) */
  int bar_idx = (int)(m - _meters);
  if (bar_idx < 0 || bar_idx >= NUM_METER_BARS) bar_idx = 0;
  char newval[12];
  if (db <= -60.0f) lv_snprintf(newval, sizeof(newval), " -inf");
  else              lv_snprintf(newval, sizeof(newval), "%5.1f", db);
  
  if (strcmp(buf[bar_idx], newval) != 0) {
    memcpy(buf[bar_idx], newval, sizeof(newval));
    lv_label_set_text(m->value, newval);
    lv_obj_set_style_text_color(m->value, db_to_color(db), 0);
  }
}

/* ─── Sidebar readout helper (landscape sidebar & portrait LUFS strip) ─── */

static lv_obj_t *_create_readout(lv_obj_t *parent, const char *title, int y,
                                 lv_color_t color, lv_obj_t **val_lbl) {
  lv_obj_t *lbl_title = mb_create_caption_label(parent, title);
  lv_obj_set_pos(lbl_title, MB_SPACE_SM, y);

  *val_lbl = lv_label_create(parent);
  lv_label_set_text(*val_lbl, "--.--");
  lv_obj_set_pos(*val_lbl, MB_SPACE_SM, y + 12);
  lv_obj_set_style_text_color(*val_lbl, color, 0);
  lv_obj_set_style_text_font(*val_lbl, MB_FONT_TITLE, 0);  /* 20px — fits in 38px row */

  return *val_lbl;
}

/* Portrait LUFS strip column helper — places title+value in a column cell */
static void _create_strip_col(lv_obj_t *parent, const char *title,
                               lv_color_t val_color, lv_obj_t **val_lbl,
                               int col_x, int col_w, int col_h) {
  /* Left separator line (skip for col 0) */
  if (col_x > 0) {
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_pos(sep, col_x, 4);
    lv_obj_set_size(sep, 1, col_h - 8);
    lv_obj_set_style_bg_color(sep, MB_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
  }

  /* Title label */
  lv_obj_t *ttl = mb_create_caption_label(parent, title);
  lv_obj_set_pos(ttl, col_x + MB_SPACE_XS, 5);
  lv_obj_set_width(ttl, col_w - MB_SPACE_XS * 2);

  /* Value label — 20pt fits compact 68px portrait strip */
  *val_lbl = lv_label_create(parent);
  lv_label_set_text(*val_lbl, "--.-");
  lv_obj_set_pos(*val_lbl, col_x + MB_SPACE_XS, 18);
  lv_obj_set_style_text_color(*val_lbl, val_color, 0);
  lv_obj_set_style_text_font(*val_lbl, MB_FONT_TITLE, 0); /* 20pt — compact */
}

/* ─── Create Meter Screen ─────────────────────────────────────── */

void ui_create_meter_screen(lv_obj_t *parent, meter_state_t *state,
                            UDPComm *udp) {
  lv_coord_t disp_w = lv_disp_get_hor_res(NULL);
  lv_coord_t disp_h = lv_disp_get_ver_res(NULL);
  bool portrait = (disp_w < disp_h);

  /* Runtime content area height (works for both orientations) */
  int content_h = (int)disp_h - MB_STATUS_BAR_HEIGHT - MB_TRANSPORT_HEIGHT;

  /* ── Meter panel dimensions ── */
  int main_w = portrait ? (int)disp_w : MB_MAIN_METER_WIDTH;
  int bar_w  = main_w - METER_LABEL_W - METER_VALUE_W - MB_SPACE_SM * 4;

  /* Meter Height preference indices mapping */
  /* [XXL, XL, L, M, S, XS] */
  static const int PORTRAIT_HEIGHTS[]  = {480, 360, 300, 240, 180, 120};
  static const int LANDSCAPE_HEIGHTS[] = {42, 34, 26, 20, 14, 10};

  extern uint8_t g_meter_height_idx;
  uint8_t h_idx = g_meter_height_idx < 6 ? g_meter_height_idx : 1;

  /* In portrait, meter_panel_h is top area for vertical meters */
  if (portrait) meter_panel_h = PORTRAIT_HEIGHTS[h_idx];
  else          meter_panel_h = content_h;

  /* ── Meter panel ── */
  lv_obj_t *meter_panel = mb_create_panel(parent);
  lv_obj_set_size(meter_panel, main_w, meter_panel_h);
  lv_obj_set_pos(meter_panel, 0, 0);
  lv_obj_set_style_pad_all(meter_panel, MB_SPACE_SM, 0);
  if (!portrait) lv_obj_set_style_border_side(meter_panel, LV_BORDER_SIDE_RIGHT, 0);

    if (portrait) {
    /* ── PORTRAIT: 3 L/R paired groups [PEAK L|R] [RMS L|R] [TP L|R] ──
     *
     * micro_gap  = smudge between L and R within the same type pair
     * group_gap  = breathing room between PEAK / RMS / TP groups
     * v_bar_w    = maximised bar width filling available screen width
     * FREQ btn   = anchored at the BOTTOM of the meter panel (not top)
     */
    int avail_w        = main_w - MB_SPACE_SM * 2;
    int micro_gap      = 2;    /* hair-thin gap between L and R of the same type */
    /* Target ~40% narrower bars filling 57% of avail_w.                   */
    int v_bar_w        = (avail_w * 57 / 100) / 6;
    if (v_bar_w < 12)  v_bar_w = 12;
    int pair_w         = 2 * v_bar_w + micro_gap;   /* width of one L/R pair     */
    int side_margin    = 4;    /* symmetric left+right margin within content area  */
    int group_gap      = (avail_w - 3 * pair_w - 2 * side_margin) / 2;
    if (group_gap < 16) group_gap = 16;
    int left_start     = side_margin; /* = (avail_w - 3*pair_w - 2*group_gap) / 2 */

    int top_label_h    = 50;   /* room for type + channel label rows above bars  */
    int freq_btn_h     = 28;   /* SPECTRUM shortcut anchored at bottom           */
    int v_bar_h        = meter_panel_h - top_label_h - freq_btn_h - 10;
    if (v_bar_h < 40)  v_bar_h = 40;
    int v_y            = top_label_h;

    /* Per-bar absolute X positions — symmetric around screen centre.      */
    int bar_x_arr[NUM_METER_BARS];
    for (int g = 0; g < 3; g++) {
        int gx = left_start + g * (pair_w + group_gap);
        bar_x_arr[g * 2]     = gx;
        bar_x_arr[g * 2 + 1] = gx + v_bar_w + micro_gap;
    }

    /* Meter bars with short L/R channel labels */
    static const char *chan_labels[] = {"L", "R", "L", "R", "L", "R"};
    for (int i = 0; i < NUM_METER_BARS; i++) {
        _create_meter_bar(meter_panel, &_meters[i], chan_labels[i],
                          bar_x_arr[i], v_y, v_bar_w, v_bar_h, v_bar_h, true);
    }

    /* Group type labels (PEAK / RMS / TP) centred above each pair */
    static const char *type_labels[] = {"PEAK", "RMS", "TP"};
    for (int g = 0; g < 3; g++) {
        int gx = left_start + g * (pair_w + group_gap);  /* matches bar_x_arr formula */
        lv_obj_t *tl = mb_create_caption_label(meter_panel, type_labels[g]);
        lv_obj_set_pos(tl, gx, 6);
        lv_obj_set_size(tl, pair_w, 16);
        lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, 0);
    }

    /* Reference dBFS marker lines */
    {
        float ref_frac = (g_ref_db_line - (-60.0f)) / 66.0f;
        ref_frac = ref_frac < 0.0f ? 0.0f : (ref_frac > 1.0f ? 1.0f : ref_frac);
        int inner_h = v_bar_h - 6;
        int ref_y_off = (int)((1.0f - ref_frac) * inner_h);
        for (int i = 0; i < NUM_METER_BARS; i++) {
            lv_obj_t *rl = lv_obj_create(meter_panel);
            lv_obj_remove_style_all(rl);
            lv_obj_set_size(rl, v_bar_w, 1);
            lv_obj_set_pos(rl, bar_x_arr[i], v_y + ref_y_off + 2);
            lv_obj_set_style_bg_color(rl, lv_color_hex(0x4488FF), 0);
            lv_obj_set_style_bg_opa(rl, LV_OPA_60, 0);
            lv_obj_clear_flag(rl, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    /* Beat pulse dot — center gap between PEAK and RMS, at label height */
    _beat_pulse_dot = lv_obj_create(meter_panel);
    lv_obj_remove_style_all(_beat_pulse_dot);
    lv_obj_set_size(_beat_pulse_dot, 8, 8);
    lv_obj_set_pos(_beat_pulse_dot, left_start + pair_w + group_gap / 2 - 4, top_label_h / 2 - 4);
    lv_obj_set_style_radius(_beat_pulse_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_beat_pulse_dot, MB_COLOR_ACCENT_SEAFOAM, 0);
    lv_obj_set_style_bg_opa(_beat_pulse_dot, LV_OPA_TRANSP, 0);

    /* SPECTRUM shortcut — anchored at bottom of meter panel, full width
     * (previously floated over bars at top-right, causing overlap with FPS) */
    {
        int fb_y = meter_panel_h - freq_btn_h - 2;
        lv_obj_t *spec_btn = lv_btn_create(meter_panel);
        lv_obj_set_size(spec_btn, avail_w - 2, freq_btn_h);
        lv_obj_set_pos(spec_btn, MB_SPACE_SM, fb_y);
        mb_style_transport_btn(spec_btn, MB_COLOR_ACCENT_SEAFOAM);
        lv_obj_t *sl = lv_label_create(spec_btn);
        lv_label_set_text(sl, LV_SYMBOL_AUDIO "  SPECTRUM ANALYZER");
        lv_obj_set_style_text_font(sl, MB_FONT_SMALL, 0);
        lv_obj_center(sl);
        lv_obj_add_event_cb(spec_btn, _navigate_to_spectrum, LV_EVENT_CLICKED, NULL);
    }
  } else {
    /* ── LANDSCAPE: Row-based horizontal layout ── */
    /* dB scale header */
    lv_obj_t *scale = lv_obj_create(meter_panel);
    lv_obj_set_size(scale, bar_w, 16);
    lv_obj_set_pos(scale, METER_LABEL_W + MB_SPACE_SM, MB_SPACE_XS);
    lv_obj_set_style_bg_opa(scale, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scale, 0, 0);
    lv_obj_set_style_pad_all(scale, 0, 0);
    lv_obj_clear_flag(scale, LV_OBJ_FLAG_SCROLLABLE);

    static const float scale_dbs[] = {-48, -36, -24, -18, -12, -6, -3, 0};
    static const char *scale_strs[] = {"-48", "-36", "-24", "-18",
                                       "-12", "-6",  "-3",  "0"};
    for (int i = 0; i < 8; i++) {
      int px = db_to_px(scale_dbs[i], bar_w);
      lv_obj_t *sl = lv_label_create(scale);
      lv_label_set_text(sl, scale_strs[i]);
      lv_obj_set_pos(sl, px - 8, 0);
      lv_obj_set_style_text_color(sl, MB_COLOR_TEXT_MUTED, 0);
      lv_obj_set_style_text_font(sl, &lv_font_montserrat_12, 0);
    }

    int y_start = 24;
    int cur_bar_h = LANDSCAPE_HEIGHTS[h_idx];
    for (int i = 0; i < NUM_METER_BARS; i++) {
      int y = y_start + i * (cur_bar_h + METER_GAP);
      int bar_x = METER_LABEL_W + MB_SPACE_SM;
      _create_meter_bar(meter_panel, &_meters[i], meter_labels[i], bar_x, y, bar_w, cur_bar_h, bar_w, false);
    }

    /* ── Reference dBFS marker lines (landscape, vertical lines on bars) ── */
    {
      int ref_px = db_to_px(g_ref_db_line, bar_w);
      int cur_h  = LANDSCAPE_HEIGHTS[h_idx];
      for (int i = 0; i < NUM_METER_BARS; i++) {
        int bar_y = y_start + i * (cur_h + METER_GAP);
        lv_obj_t *ref_mark = lv_obj_create(meter_panel);
        lv_obj_remove_style_all(ref_mark);
        lv_obj_set_size(ref_mark, 1, cur_h);
        lv_obj_set_pos(ref_mark, METER_LABEL_W + MB_SPACE_SM + ref_px, bar_y);
        lv_obj_set_style_bg_color(ref_mark, lv_color_hex(0x4488FF), 0);
        lv_obj_set_style_bg_opa(ref_mark, LV_OPA_70, 0);
        lv_obj_clear_flag(ref_mark, LV_OBJ_FLAG_CLICKABLE);
      }
    }

    /* ── Beat pulse indicator dot (landscape) ── */
    _beat_pulse_dot = lv_obj_create(meter_panel);
    lv_obj_remove_style_all(_beat_pulse_dot);
    lv_obj_set_size(_beat_pulse_dot, 12, 12);
    lv_obj_set_pos(_beat_pulse_dot, bar_w + METER_LABEL_W + MB_SPACE_SM - 16,
                   y_start + NUM_METER_BARS * (LANDSCAPE_HEIGHTS[h_idx] + METER_GAP) + 4);
    lv_obj_set_style_radius(_beat_pulse_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_beat_pulse_dot, MB_COLOR_ACCENT_SEAFOAM, 0);
    lv_obj_set_style_bg_opa(_beat_pulse_dot, LV_OPA_TRANSP, 0);

    /* ── Stereo balance bar (landscape, below meters) ── */
    {
      int bal_y  = y_start + NUM_METER_BARS * (LANDSCAPE_HEIGHTS[h_idx] + METER_GAP) + 2;
      int bal_x  = METER_LABEL_W + MB_SPACE_SM;
      int bal_w  = bar_w;
      int bal_h  = 6;

      /* Background track */
      lv_obj_t *bal_bg = lv_obj_create(meter_panel);
      lv_obj_remove_style_all(bal_bg);
      lv_obj_set_size(bal_bg, bal_w, bal_h);
      lv_obj_set_pos(bal_bg, bal_x, bal_y);
      lv_obj_set_style_bg_color(bal_bg, lv_color_hex(0x111118), 0);
      lv_obj_set_style_bg_opa(bal_bg, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(bal_bg, 3, 0);
      lv_obj_set_style_border_color(bal_bg, MB_COLOR_BORDER, 0);
      lv_obj_set_style_border_width(bal_bg, 1, 0);
      lv_obj_clear_flag(bal_bg, LV_OBJ_FLAG_CLICKABLE);

      /* Center tick */
      _balance_bar_center = lv_obj_create(bal_bg);
      lv_obj_remove_style_all(_balance_bar_center);
      lv_obj_set_size(_balance_bar_center, 2, bal_h);
      lv_obj_set_pos(_balance_bar_center, bal_w / 2 - 1, 0);
      lv_obj_set_style_bg_color(_balance_bar_center, MB_COLOR_TEXT_MUTED, 0);
      lv_obj_set_style_bg_opa(_balance_bar_center, LV_OPA_40, 0);

      /* Fill indicator */
      _balance_bar_fill = lv_obj_create(bal_bg);
      lv_obj_remove_style_all(_balance_bar_fill);
      lv_obj_set_size(_balance_bar_fill, 4, bal_h);
      lv_obj_set_pos(_balance_bar_fill, bal_w / 2 - 2, 0);
      lv_obj_set_style_bg_color(_balance_bar_fill, MB_COLOR_ACCENT_SEAFOAM, 0);
      lv_obj_set_style_bg_opa(_balance_bar_fill, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(_balance_bar_fill, 2, 0);
    }
  }   /* end !portrait landscape block */

  if (!portrait) {
    /* ── LANDSCAPE: right sidebar ── */
    lv_obj_t *sidebar = mb_create_panel(parent);
    lv_obj_set_size(sidebar, MB_SIDEBAR_WIDTH, content_h);
    lv_obj_set_pos(sidebar, main_w, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);

    int sy = MB_SPACE_XS;
    _create_readout(sidebar, "LUFS-M", sy, MB_COLOR_ACCENT_MAGENTA, &_lbl_lufs_m_val); sy += 38;
    _create_readout(sidebar, "LUFS-S", sy, MB_COLOR_ACCENT_SEAFOAM, &_lbl_lufs_s_val); sy += 38;
    _create_readout(sidebar, "LUFS-I", sy, MB_COLOR_ACCENT_PURPLE,  &_lbl_lufs_i_val); sy += 38;
    /* Loudness target reference */
    if (g_loudness_target < -0.1f) {
      _create_readout(sidebar, "TARGET", sy, MB_COLOR_ACCENT_AMBER, &_loudness_target_line); sy += 38;
    }
    _create_readout(sidebar, "LRA",    sy, MB_COLOR_ACCENT_AMBER,   &_lbl_lufs_r_val); sy += 38;
    _create_readout(sidebar, "PHASE",  sy, MB_COLOR_PHASE_GOOD,     &_lbl_phase_val);  sy += 38;

    lv_obj_t *clip_title = mb_create_caption_label(sidebar, "CLIPS");
    lv_obj_set_pos(clip_title, MB_SPACE_SM, sy);
    _lbl_clip_val = lv_label_create(sidebar);
    lv_label_set_text(_lbl_clip_val, "0 / 0");
    lv_obj_set_pos(_lbl_clip_val, MB_SPACE_SM, sy + 12);
    lv_obj_set_style_text_color(_lbl_clip_val, MB_COLOR_METER_GREEN, 0);
    lv_obj_set_style_text_font(_lbl_clip_val, MB_FONT_SMALL, 0);
    /* Long-press clip label to reset session MAX TP + relay clip counters */
    lv_obj_add_flag(_lbl_clip_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_lbl_clip_val, [](lv_event_t* e) {
        (void)e;
        /* Reset display-side session MAX True Peak */
        _session_max_tp_l = -70.0f;
        _session_max_tp_r = -70.0f;
        /* BUG-M2 fix: send reset_clips command to relay → REAPER Lua bridge
         * so the relay's clip_count_l/r sourced from Lua resets too.
         * Without this the clip count reappears immediately on the next packet. */
        if (_udp_ref) _udp_ref->sendCommand(MB_CMD_RESET_CLIPS);
        Serial.println("[UI] Clip counters + session MAX TP reset");
    }, LV_EVENT_LONG_PRESSED, NULL);
    sy += 28;

    /* MAX TP readout */
    _create_readout(sidebar, "MAX TP", sy, MB_COLOR_METER_RED, &_lbl_max_peak_val);
    sy += 38;

    /* ── LUFS history graph (landscape sidebar, below MAX TP) ── */
    if (sy + LUFS_GRAPH_H + 16 < content_h - 120) {
      lv_obj_t *gh_title = mb_create_caption_label(sidebar, "LUFS-M HISTORY");
      lv_obj_set_pos(gh_title, MB_SPACE_SM, sy);
      sy += 14;
      /* Allocate canvas buffer (PSRAM preferred) */
      size_t cbuf_sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR(LUFS_GRAPH_W, LUFS_GRAPH_H);
      _lufs_graph_buf = (lv_color_t*)ps_malloc(cbuf_sz);
      if (!_lufs_graph_buf) _lufs_graph_buf = (lv_color_t*)malloc(cbuf_sz);
      if (_lufs_graph_buf) {
        _lufs_graph_canvas = lv_canvas_create(sidebar);
        lv_obj_set_pos(_lufs_graph_canvas, MB_SPACE_SM - 2, sy);
        lv_canvas_set_buffer(_lufs_graph_canvas, _lufs_graph_buf, LUFS_GRAPH_W, LUFS_GRAPH_H, LV_IMG_CF_TRUE_COLOR);
        lv_canvas_fill_bg(_lufs_graph_canvas, lv_color_hex(0x090912), LV_OPA_COVER);
        sy += LUFS_GRAPH_H + 8;
      }
    }

    /* ── Bottom-anchored button stack (3 buttons, 30px each + 6px gap) ── */
    int btn_w = MB_SIDEBAR_WIDTH - MB_SPACE_SM * 2;
    int btn_h = 28;

    /* SPECTRUM nav button — top of the stack */
    lv_obj_t *btn_spec = lv_btn_create(sidebar);
    lv_obj_set_size(btn_spec, btn_w, btn_h);
    lv_obj_set_pos(btn_spec, MB_SPACE_SM, content_h - 106);
    lv_obj_set_style_bg_color(btn_spec, MB_COLOR_ACCENT_MAGENTA, 0);
    lv_obj_set_style_bg_opa(btn_spec, LV_OPA_80, 0);
    lv_obj_set_style_radius(btn_spec, 4, 0);
    lv_obj_set_style_border_width(btn_spec, 0, 0);
    lv_obj_set_style_shadow_width(btn_spec, 0, 0);
    lv_obj_set_style_pad_all(btn_spec, 2, 0);
    lv_obj_t *lbl_spec = lv_label_create(btn_spec);
    lv_label_set_text(lbl_spec, LV_SYMBOL_AUDIO " SPECTRUM");
    lv_obj_set_style_text_font(lbl_spec, MB_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_spec, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(lbl_spec);
    lv_obj_add_event_cb(btn_spec, _navigate_to_spectrum, LV_EVENT_CLICKED, NULL);

    /* MASTER / track source toggle — middle */
    _btn_meter_src = lv_btn_create(sidebar);
    lv_obj_set_size(_btn_meter_src, btn_w, btn_h);
    lv_obj_set_pos(_btn_meter_src, MB_SPACE_SM, content_h - 72);
    lv_obj_set_style_bg_color(_btn_meter_src, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_bg_opa(_btn_meter_src, LV_OPA_80, 0);
    lv_obj_set_style_radius(_btn_meter_src, 4, 0);
    lv_obj_set_style_border_width(_btn_meter_src, 0, 0);
    lv_obj_set_style_shadow_width(_btn_meter_src, 0, 0);
    lv_obj_set_style_pad_all(_btn_meter_src, 2, 0);
    _lbl_meter_src_btn = lv_label_create(_btn_meter_src);
    lv_label_set_text(_lbl_meter_src_btn, LV_SYMBOL_AUDIO " MASTER");
    lv_obj_set_style_text_font(_lbl_meter_src_btn, MB_FONT_SMALL, 0);
    lv_obj_set_style_text_color(_lbl_meter_src_btn, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(_lbl_meter_src_btn);
    lv_obj_add_event_cb(_btn_meter_src, _meter_src_toggle_cb, LV_EVENT_CLICKED, NULL);

    /* MODE cycle button — bottom */
    lv_obj_t *btn_mode = lv_btn_create(sidebar);
    lv_obj_set_size(btn_mode, btn_w, btn_h);
    lv_obj_set_pos(btn_mode, MB_SPACE_SM, content_h - 38);
    lv_obj_set_style_bg_color(btn_mode, MB_COLOR_ACCENT_SEAFOAM, 0);
    lv_obj_set_style_bg_opa(btn_mode, LV_OPA_60, 0);
    lv_obj_set_style_radius(btn_mode, 4, 0);
    lv_obj_set_style_border_width(btn_mode, 0, 0);
    lv_obj_set_style_shadow_width(btn_mode, 0, 0);
    lv_obj_set_style_pad_all(btn_mode, 2, 0);
    _lbl_mode_btn = lv_label_create(btn_mode);
    lv_label_set_text(_lbl_mode_btn, "MODE: CLASSIC");
    lv_obj_set_style_text_font(_lbl_mode_btn, MB_FONT_SMALL, 0);
    lv_obj_set_style_text_color(_lbl_mode_btn, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(_lbl_mode_btn);
    /* Cycle mode on tap; persist with NVS; show toast */
    lv_obj_add_event_cb(btn_mode, [](lv_event_t* ev) {
        (void)ev;
        g_meter_mode = (g_meter_mode + 1) % 4;
        /* Persist */
        Preferences prefs;
        prefs.begin("meterbridge", false);
        prefs.putUChar("meter_mode", g_meter_mode);
        prefs.end();
        /* Update button label */
        static const char* short_names[] = {"CLASSIC","PEAK L/R","MID/SIDE","LUFS"};
        if (_lbl_mode_btn) lv_label_set_text(_lbl_mode_btn,
            (String("MODE: ") + short_names[g_meter_mode]).c_str());
        /* Toast announcement */
        static char toast_buf[32];
        snprintf(toast_buf, sizeof(toast_buf), "%s  MODE",
                 short_names[g_meter_mode]);
        ui_show_toast(toast_buf);
    }, LV_EVENT_CLICKED, NULL);

  } else {
    /* ── PORTRAIT Option B: LUFS strip + info panel ── */

    int strip_y = meter_panel_h + 4;
    int strip_h = 68;            /* was 106 — compact 20pt font fits fine */
    int col_w   = (int)disp_w / 6;

    /* LUFS strip panel */
    lv_obj_t *strip = mb_create_panel(parent);
    lv_obj_set_size(strip, (int)disp_w, strip_h);
    lv_obj_set_pos(strip, 0, strip_y);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_set_style_border_side(strip, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    _create_strip_col(strip, "LUFS-M", MB_COLOR_ACCENT_MAGENTA, &_lbl_lufs_m_val, 0 * col_w, col_w, strip_h);
    _create_strip_col(strip, "LUFS-S", MB_COLOR_ACCENT_SEAFOAM, &_lbl_lufs_s_val, 1 * col_w, col_w, strip_h);
    _create_strip_col(strip, "LUFS-I", MB_COLOR_ACCENT_PURPLE,  &_lbl_lufs_i_val, 2 * col_w, col_w, strip_h);
    _create_strip_col(strip, "LRA",    MB_COLOR_ACCENT_AMBER,   &_lbl_lufs_r_val, 3 * col_w, col_w, strip_h);
    _create_strip_col(strip, "PHASE",  MB_COLOR_PHASE_GOOD,     &_lbl_phase_val,  4 * col_w, col_w, strip_h);

    /* Last column: CLIPS + source button */
    int cx = 5 * col_w;
    lv_obj_t *sep = lv_obj_create(strip);
    lv_obj_set_pos(sep, cx, 4);
    lv_obj_set_size(sep, 1, strip_h - 8);
    lv_obj_set_style_bg_color(sep, MB_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    lv_obj_t *clip_title = mb_create_caption_label(strip, "CLIPS");
    lv_obj_set_pos(clip_title, cx + MB_SPACE_XS, 5);  /* align with other strip cols */

    _lbl_clip_val = lv_label_create(strip);
    lv_label_set_text(_lbl_clip_val, "0/0");
    lv_obj_set_pos(_lbl_clip_val, cx + MB_SPACE_XS, 18);  /* 20pt, matches strip cols */
    lv_obj_set_style_text_color(_lbl_clip_val, MB_COLOR_METER_GREEN, 0);
    lv_obj_set_style_text_font(_lbl_clip_val, MB_FONT_TITLE, 0);  /* was MB_FONT_VALUE (28pt) — overlapped MSTR */

    /* MASTER/TRACK toggle — repositioned below clip value (was strip_h-34=34, overlapped) */
    _btn_meter_src = lv_btn_create(strip);
    lv_obj_set_size(_btn_meter_src, col_w - MB_SPACE_XS * 2, 20);
    lv_obj_set_pos(_btn_meter_src, cx + MB_SPACE_XS, 44);  /* 18(val)+26(20pt)=44, +0px gap */
    lv_obj_set_style_bg_color(_btn_meter_src, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_bg_opa(_btn_meter_src, LV_OPA_80, 0);
    lv_obj_set_style_radius(_btn_meter_src, 4, 0);
    lv_obj_set_style_border_width(_btn_meter_src, 0, 0);
    lv_obj_set_style_shadow_width(_btn_meter_src, 0, 0);
    lv_obj_set_style_pad_all(_btn_meter_src, 1, 0);
    _lbl_meter_src_btn = lv_label_create(_btn_meter_src);
    lv_label_set_text(_lbl_meter_src_btn, "MSTR");
    lv_obj_set_style_text_font(_lbl_meter_src_btn, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lbl_meter_src_btn, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(_lbl_meter_src_btn);
    lv_obj_add_event_cb(_btn_meter_src, _meter_src_toggle_cb, LV_EVENT_CLICKED, NULL);

    /* ── Portrait Info Panel ── */
    int info_y = strip_y + strip_h + 4;
    int info_h = content_h - info_y;
    if (info_h > 40) {
      lv_obj_t *info = mb_create_panel(parent);
      lv_obj_set_size(info, (int)disp_w, info_h);
      lv_obj_set_pos(info, 0, info_y);
      lv_obj_set_style_pad_all(info, MB_SPACE_SM, 0);
      lv_obj_set_style_border_side(info, LV_BORDER_SIDE_TOP, 0);
      lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);

      /* Left half: large LUFS-I value + LRA + MAX TP */
      int half_w = (int)disp_w / 2 - MB_SPACE_SM;

      lv_obj_t *lufs_cap = mb_create_caption_label(info, "INTEGRATED LUFS");
      lv_obj_set_pos(lufs_cap, MB_SPACE_SM, MB_SPACE_XS);

      _lbl_portrait_lufs_big = lv_label_create(info);
      lv_label_set_text(_lbl_portrait_lufs_big, "--.--");
      lv_obj_set_pos(_lbl_portrait_lufs_big, MB_SPACE_SM, 16);
      lv_obj_set_style_text_color(_lbl_portrait_lufs_big, MB_COLOR_ACCENT_PURPLE, 0);
      lv_obj_set_style_text_font(_lbl_portrait_lufs_big, MB_FONT_VALUE_LG, 0); /* 36pt, was 20pt */

      /* MAX TRUE PEAK readout — repositioned below 36pt LUFS-I value
       * 36pt font ~45px tall: y_start(16) + 45 = 61. Add gap → y=66.     */
      lv_obj_t *mtp_cap = mb_create_caption_label(info, "MAX TRUE PEAK");
      lv_obj_set_pos(mtp_cap, MB_SPACE_SM, 66);
      _lbl_max_peak_val = lv_label_create(info);
      lv_label_set_text(_lbl_max_peak_val, "L: ---   R: ---");
      lv_obj_set_pos(_lbl_max_peak_val, MB_SPACE_SM, 78);
      lv_obj_set_style_text_color(_lbl_max_peak_val, MB_COLOR_METER_RED, 0);
      lv_obj_set_style_text_font(_lbl_max_peak_val, MB_FONT_SMALL, 0);

      /* Vertical divider */
      lv_obj_t *vdiv = lv_obj_create(info);
      lv_obj_set_pos(vdiv, half_w, 0);
      lv_obj_set_size(vdiv, 1, info_h - MB_SPACE_SM);
      lv_obj_set_style_bg_color(vdiv, MB_COLOR_BORDER, 0);
      lv_obj_set_style_bg_opa(vdiv, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(vdiv, 0, 0);

      /* Right half: track info stack */
      int rx = half_w + MB_SPACE_SM;
      int rw = (int)disp_w - rx - MB_SPACE_SM;

      /* Track color swatch + track name */
      _swatch_portrait_track = lv_obj_create(info);
      lv_obj_set_pos(_swatch_portrait_track, rx, MB_SPACE_XS + 2);
      lv_obj_set_size(_swatch_portrait_track, 12, 12);
      lv_obj_set_style_bg_color(_swatch_portrait_track, lv_color_hex(0xB4B4C8), 0);
      lv_obj_set_style_bg_opa(_swatch_portrait_track, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(_swatch_portrait_track, MB_COLOR_BORDER, 0);
      lv_obj_set_style_border_width(_swatch_portrait_track, 1, 0);
      lv_obj_set_style_radius(_swatch_portrait_track, 2, 0);

      _lbl_portrait_track = lv_label_create(info);
      lv_label_set_text(_lbl_portrait_track, "MASTER");
      lv_obj_set_pos(_lbl_portrait_track, rx + 16, MB_SPACE_XS);
      lv_obj_set_width(_lbl_portrait_track, rw - 18);
      lv_obj_set_style_text_color(_lbl_portrait_track, MB_COLOR_TEXT_PRIMARY, 0);
      lv_obj_set_style_text_font(_lbl_portrait_track, MB_FONT_LABEL, 0);
      lv_label_set_long_mode(_lbl_portrait_track, LV_LABEL_LONG_CLIP);

      /* Project name */
      lv_obj_t *proj_cap = mb_create_caption_label(info, "PROJECT");
      lv_obj_set_pos(proj_cap, rx, 28);

      _lbl_portrait_project = lv_label_create(info);
      lv_label_set_text(_lbl_portrait_project, "(No Project)");
      lv_obj_set_pos(_lbl_portrait_project, rx, 42);
      lv_obj_set_width(_lbl_portrait_project, rw);
      lv_obj_set_style_text_color(_lbl_portrait_project, MB_COLOR_TEXT_SECONDARY, 0);
      lv_obj_set_style_text_font(_lbl_portrait_project, MB_FONT_SMALL, 0);
      lv_label_set_long_mode(_lbl_portrait_project, LV_LABEL_LONG_CLIP);

      /* Section / region */
      lv_obj_t *sec_cap = mb_create_caption_label(info, "SECTION");
      lv_obj_set_pos(sec_cap, rx, 62);

      _lbl_portrait_section = lv_label_create(info);
      lv_label_set_text(_lbl_portrait_section, "---");
      lv_obj_set_pos(_lbl_portrait_section, rx, 76);
      lv_obj_set_width(_lbl_portrait_section, rw);
      lv_obj_set_style_text_color(_lbl_portrait_section, MB_COLOR_ACCENT_AMBER, 0);
      lv_obj_set_style_text_font(_lbl_portrait_section, MB_FONT_SMALL, 0);
      lv_label_set_long_mode(_lbl_portrait_section, LV_LABEL_LONG_CLIP);

      /* Playback position */
      if (info_h > 110) {
        lv_obj_t *pos_cap = mb_create_caption_label(info, "POSITION");
        lv_obj_set_pos(pos_cap, rx, 96);

        _lbl_portrait_position = lv_label_create(info);
        lv_label_set_text(_lbl_portrait_position, "0:00.0");
        lv_obj_set_pos(_lbl_portrait_position, rx, 110);
        lv_obj_set_style_text_color(_lbl_portrait_position, MB_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(_lbl_portrait_position, MB_FONT_LABEL, 0);
      }
    }
  }
}

/* ─── Update Meter Screen ─────────────────────────────────────── */

void ui_update_meter_screen(meter_state_t *state) {
  lv_coord_t disp_w = lv_disp_get_hor_res(NULL);
  lv_coord_t disp_h = lv_disp_get_ver_res(NULL);
  bool portrait = (disp_w < disp_h);

  int main_w = portrait ? (int)disp_w : MB_MAIN_METER_WIDTH;
  int bar_dim = main_w - METER_LABEL_W - METER_VALUE_W - MB_SPACE_SM * 4;
  if (portrait) {
    /* BUG-M1 fix: compute v_bar_h the same way as ui_create_meter_screen()
     * so _update_meter_bar() uses the correct seg_step for peak-hold positions. */
    extern uint8_t g_meter_height_idx;
    static const int PORTRAIT_HEIGHTS[] = {480, 360, 300, 240, 180, 120};
    uint8_t h_idx = g_meter_height_idx < 6 ? g_meter_height_idx : 1;
    int v_bar_h = PORTRAIT_HEIGHTS[h_idx] - 100;
    if (v_bar_h < 40) v_bar_h = 40;
    bar_dim = v_bar_h;  /* matches what was passed as bar_h / bar_dim to _create_meter_bar() */
  }

  _update_meter_bar(&_meters[MTR_PEAK_L],   state->peak_l,      bar_dim, portrait);
  _update_meter_bar(&_meters[MTR_PEAK_R],   state->peak_r,      bar_dim, portrait);
  _update_meter_bar(&_meters[MTR_RMS_L],    state->rms_l,       bar_dim, portrait);
  _update_meter_bar(&_meters[MTR_RMS_R],    state->rms_r,       bar_dim, portrait);
  _update_meter_bar(&_meters[MTR_TRUEPK_L], state->true_peak_l, bar_dim, portrait);
  _update_meter_bar(&_meters[MTR_TRUEPK_R], state->true_peak_r, bar_dim, portrait);

  uint32_t _now = millis();

  /* ── Beat pulse indicator (flashes teal dot on each new beat) ── */
  if (_beat_pulse_dot) {
    if (state->beat_in_bar != _last_beat_in_bar) {
      _last_beat_in_bar = state->beat_in_bar;
      _beat_pulse_end_ms = _now + 80;
      lv_obj_set_style_bg_opa(_beat_pulse_dot, LV_OPA_COVER, 0);
    }
    if (_now >= _beat_pulse_end_ms && _beat_pulse_end_ms > 0) {
      lv_obj_set_style_bg_opa(_beat_pulse_dot, LV_OPA_TRANSP, 0);
      _beat_pulse_end_ms = 0;
    }
  }

  /* ── Stereo balance bar (landscape only) ── */
  if (_balance_bar_fill) {
    /* Compute balance from RMS L vs R. Range [-1.0 L, +1.0 R], 0 = center */
    float diff = state->rms_r - state->rms_l;  /* +ve = more R, -ve = more L */
    diff = diff < -12.0f ? -12.0f : (diff > 12.0f ? 12.0f : diff);
    float balance = diff / 12.0f;  /* normalise to [-1,+1] */
    static float _last_bal = 99.0f;
    if (fabsf(balance - _last_bal) > 0.01f) {
      _last_bal = balance;
      lv_coord_t bg_w = lv_obj_get_content_width(lv_obj_get_parent(_balance_bar_fill));
      int half = bg_w / 2;
      int fill_w = (int)(fabsf(balance) * half);
      if (fill_w < 2) fill_w = 2;
      int fill_x = (balance <= 0.0f) ? (half - fill_w) : half;
      lv_obj_set_size(_balance_bar_fill, fill_w, lv_obj_get_height(_balance_bar_fill));
      lv_obj_set_x(_balance_bar_fill, fill_x);
      lv_color_t bal_col = (fabsf(balance) < 0.1f) ? MB_COLOR_METER_GREEN :
                           (fabsf(balance) < 0.4f) ? MB_COLOR_ACCENT_SEAFOAM :
                           (fabsf(balance) < 0.7f) ? MB_COLOR_ACCENT_AMBER :
                                                      MB_COLOR_METER_RED;
      lv_obj_set_style_bg_color(_balance_bar_fill, bal_col, 0);
    }
  }

  /* ── Session MAX True Peak tracking + display ── */
  {
    if (state->true_peak_l > _session_max_tp_l) _session_max_tp_l = state->true_peak_l;
    if (state->true_peak_r > _session_max_tp_r) _session_max_tp_r = state->true_peak_r;
    if (_lbl_max_peak_val) {
      static uint32_t _max_tp_last_t = 0;
      if (_now - _max_tp_last_t >= 1000) {
        _max_tp_last_t = _now;
        float mx = (_session_max_tp_l > _session_max_tp_r) ? _session_max_tp_l : _session_max_tp_r;
        char max_buf[32];
        lv_coord_t _dw = lv_disp_get_hor_res(NULL);
        lv_coord_t _dh = lv_disp_get_ver_res(NULL);
        if (_dw < _dh) {
          /* Portrait: show L and R separately */
          char lb[10], rb[10];
          if (_session_max_tp_l <= -60.0f) snprintf(lb, sizeof(lb), "-inf");
          else snprintf(lb, sizeof(lb), "%.1f", _session_max_tp_l);
          if (_session_max_tp_r <= -60.0f) snprintf(rb, sizeof(rb), "-inf");
          else snprintf(rb, sizeof(rb), "%.1f", _session_max_tp_r);
          snprintf(max_buf, sizeof(max_buf), "L: %s   R: %s", lb, rb);
        } else {
          /* Landscape: single worst-case value in sidebar */
          if (mx <= -60.0f) snprintf(max_buf, sizeof(max_buf), "-inf");
          else              snprintf(max_buf, sizeof(max_buf), "%.1f", mx);
        }
        lv_label_set_text(_lbl_max_peak_val, max_buf);
        lv_obj_set_style_text_color(_lbl_max_peak_val,
          mx >= 0.0f  ? MB_COLOR_METER_CLIP   :
          mx >= -1.0f ? MB_COLOR_METER_RED     :
          mx >= -6.0f ? MB_COLOR_METER_YELLOW  :
                        MB_COLOR_METER_GREEN, 0);
      }
    }
  }

  /* ── LUFS-M history graph (250ms sample rate, 30-sec window) ── */
  if (_lufs_graph_canvas && _lufs_graph_buf) {
    static uint32_t _hist_last_ms = 0;
    if (_now - _hist_last_ms >= 250) {
      _hist_last_ms = _now;
      _lufs_hist[_lufs_hist_head] = state->lufs_momentary;
      _lufs_hist_head = (_lufs_hist_head + 1) % LUFS_HIST_SAMPLES;
      if (_lufs_hist_head == 0) _lufs_hist_full = true;
      /* Redraw canvas — wipe and draw column lines */
      lv_canvas_fill_bg(_lufs_graph_canvas, lv_color_hex(0x090912), LV_OPA_COVER);
      int n = _lufs_hist_full ? LUFS_HIST_SAMPLES : (int)_lufs_hist_head;
      if (n > 0) {
        /* Draw target zone band: [-18, -14] as dim green background */
        static const float GRAPH_DB_MIN = -40.0f, GRAPH_DB_MAX = 0.0f;
        float range = GRAPH_DB_MAX - GRAPH_DB_MIN;
        int tgt_y1 = (int)((GRAPH_DB_MAX - (g_loudness_target + 2.0f)) / range * LUFS_GRAPH_H);
        int tgt_y2 = (int)((GRAPH_DB_MAX - (g_loudness_target - 2.0f)) / range * LUFS_GRAPH_H);
        tgt_y1 = tgt_y1 < 0 ? 0 : (tgt_y1 >= LUFS_GRAPH_H ? LUFS_GRAPH_H - 1 : tgt_y1);
        tgt_y2 = tgt_y2 < 0 ? 0 : (tgt_y2 >= LUFS_GRAPH_H ? LUFS_GRAPH_H - 1 : tgt_y2);
        lv_draw_rect_dsc_t zone_dsc;
        lv_draw_rect_dsc_init(&zone_dsc);
        zone_dsc.bg_color = lv_color_hex(0x002010);
        zone_dsc.bg_opa   = LV_OPA_COVER;
        zone_dsc.radius   = 0;
        lv_canvas_draw_rect(_lufs_graph_canvas, 0, tgt_y1, LUFS_GRAPH_W, tgt_y2 - tgt_y1 + 1, &zone_dsc);
        /* Draw history bars */
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.width = 1;
        for (int j = 0; j < n && j < LUFS_GRAPH_W; j++) {
          int src_idx = (int)((_lufs_hist_head - n + j + LUFS_HIST_SAMPLES) % LUFS_HIST_SAMPLES);
          float val = _lufs_hist[src_idx];
          if (val < GRAPH_DB_MIN) val = GRAPH_DB_MIN;
          if (val > GRAPH_DB_MAX) val = GRAPH_DB_MAX;
          float frac = (val - GRAPH_DB_MIN) / range;
          int bar_h_px = (int)(frac * LUFS_GRAPH_H);
          int x_px = (int)((float)j * LUFS_GRAPH_W / (float)(n > 1 ? n - 1 : 1));
          x_px = x_px >= LUFS_GRAPH_W ? LUFS_GRAPH_W - 1 : x_px;
          line_dsc.color =
            (val >= g_loudness_target - 2.0f && val <= g_loudness_target + 2.0f)
              ? MB_COLOR_METER_GREEN
              : (val > g_loudness_target + 2.0f)
                  ? MB_COLOR_METER_RED
                  : MB_COLOR_ACCENT_MAGENTA;
          /* LVGL8: lv_canvas_draw_line takes a const lv_point_t array + count */
          lv_point_t pts[2] = {
            {(lv_coord_t)x_px, (lv_coord_t)(LUFS_GRAPH_H - 1)},
            {(lv_coord_t)x_px, (lv_coord_t)(LUFS_GRAPH_H - 1 - bar_h_px)}
          };
          lv_canvas_draw_line(_lufs_graph_canvas, pts, 2, &line_dsc);
        }

      }
    }
  }



  /* ── LUFS / sidebar readouts — dirty-bit guarded ── */
  /* Static shadow buffers: only call lv_label_set_text() when content changed */
  static char buf[32];
  static char _s_lufs_m[12] = {0}, _s_lufs_s[12] = {0}, _s_lufs_i[12] = {0};
  static char _s_lra[12]    = {0}, _s_phase[10]   = {0}, _s_clips[16]  = {0};
  static char _s_src[24]    = {0};

  if (_lbl_lufs_m_val) {
    /* ── LUFS 1-second averaging accumulator ── */
    static float _acc_m = 0, _acc_s = 0, _acc_i = 0;
    static uint16_t _acc_n = 0;
    static uint32_t _lufs_last_ms = 0;

    /* Accumulate every frame (skip silence floor) */
    if (state->lufs_momentary > -89.9f) {
      _acc_m += state->lufs_momentary;
      _acc_s += state->lufs_short;
      _acc_i += state->lufs_integrated;
      _acc_n++;
    }

    uint32_t now_ms = millis();
    bool lufs_tick = (now_ms - _lufs_last_ms >= 1000);

    if (lufs_tick && _acc_n > 0) {
      _lufs_last_ms = now_ms;
      float avg_m = _acc_m / _acc_n;
      float avg_s = _acc_s / _acc_n;
      float avg_i = _acc_i / _acc_n;
      _acc_m = _acc_s = _acc_i = 0;
      _acc_n = 0;

      /* LUFS-M */
      snprintf(buf, sizeof(buf), "%.1f", avg_m);
      if (strcmp(buf, _s_lufs_m) != 0) {
        strncpy(_s_lufs_m, buf, sizeof(_s_lufs_m) - 1); _s_lufs_m[sizeof(_s_lufs_m)-1] = '\0';
        lv_label_set_text(_lbl_lufs_m_val, buf); }

      /* LUFS-S */
      snprintf(buf, sizeof(buf), "%.1f", avg_s);
      if (strcmp(buf, _s_lufs_s) != 0) {
        strncpy(_s_lufs_s, buf, sizeof(_s_lufs_s) - 1); _s_lufs_s[sizeof(_s_lufs_s)-1] = '\0';
        lv_label_set_text(_lbl_lufs_s_val, buf); }

      /* LUFS-I */
      snprintf(buf, sizeof(buf), "%.1f", avg_i);
      if (strcmp(buf, _s_lufs_i) != 0) {
        strncpy(_s_lufs_i, buf, sizeof(_s_lufs_i) - 1); _s_lufs_i[sizeof(_s_lufs_i)-1] = '\0';
        lv_label_set_text(_lbl_lufs_i_val, buf); }

    } else if (lufs_tick && _acc_n == 0) {
      /* No valid samples in the last second — show dashes */
      _lufs_last_ms = now_ms;
      if (strcmp("--.--", _s_lufs_m) != 0) {
        strncpy(_s_lufs_m, "--.--", sizeof(_s_lufs_m) - 1);
        lv_label_set_text(_lbl_lufs_m_val, "--.--"); }
      if (strcmp("--.--", _s_lufs_s) != 0) {
        strncpy(_s_lufs_s, "--.--", sizeof(_s_lufs_s) - 1);
        lv_label_set_text(_lbl_lufs_s_val, "--.--"); }
      if (strcmp("--.--", _s_lufs_i) != 0) {
        strncpy(_s_lufs_i, "--.--", sizeof(_s_lufs_i) - 1);
        lv_label_set_text(_lbl_lufs_i_val, "--.--"); }
    }

    /* ── Loudness target indicator ── */
    if (_loudness_target_line && g_loudness_target < -0.1f) {
      static char _s_target[12] = "";
      char tbuf[12];
      snprintf(tbuf, sizeof(tbuf), "%.0f", g_loudness_target);
      if (strcmp(tbuf, _s_target) != 0) {
        strncpy(_s_target, tbuf, sizeof(_s_target) - 1);
        lv_label_set_text(_loudness_target_line, tbuf);
      }
      /* Color by proximity: LUFS-I vs target */
      static int8_t _tgt_zone = -1;
      float diff = state->lufs_integrated - g_loudness_target;
      int8_t zone = (diff > -2.0f && diff < 2.0f) ? 0   /* On target (green) */
                  : (diff > -5.0f && diff < 5.0f) ? 1   /* Close (yellow) */
                  : 2;                                    /* Off target (red) */
      if (zone != _tgt_zone) {
        _tgt_zone = zone;
        lv_obj_set_style_text_color(_loudness_target_line,
            zone == 0 ? MB_COLOR_METER_GREEN :
            zone == 1 ? MB_COLOR_METER_YELLOW :
                        MB_COLOR_METER_RED, 0);
      }
    }

    /* LRA */
    if (state->lufs_range < -0.5f)
      snprintf(buf, sizeof(buf), "--.--");
    else
      snprintf(buf, sizeof(buf), "%.1f", state->lufs_range);
    if (strcmp(buf, _s_lra) != 0) {
      strncpy(_s_lra, buf, sizeof(_s_lra) - 1); _s_lra[sizeof(_s_lra)-1] = '\0';
      lv_label_set_text(_lbl_lufs_r_val, buf); }

    /* Phase — 2-decimal precision; also update color only on zone change */
    float ph = state->phase_correlation;
    snprintf(buf, sizeof(buf), "%.2f", ph);
    if (strcmp(buf, _s_phase) != 0) {
      strncpy(_s_phase, buf, sizeof(_s_phase) - 1); _s_phase[sizeof(_s_phase)-1] = '\0';
      lv_label_set_text(_lbl_phase_val, buf); }
    /* Phase color zone: update only when zone boundary crossed (saves style writes) */
    static int8_t _ph_zone = -1;
    int8_t new_zone = (ph > 0.5f) ? 0 : (ph > 0.0f) ? 1 : 2;
    if (new_zone != _ph_zone) {
      _ph_zone = new_zone;
      lv_obj_set_style_text_color(_lbl_phase_val,
          new_zone == 0 ? MB_COLOR_PHASE_GOOD :
          new_zone == 1 ? MB_COLOR_PHASE_WARN :
                          MB_COLOR_PHASE_BAD, 0);
    }

    /* Clips */
    uint16_t cl = state->clip_count_l, cr = state->clip_count_r;
    snprintf(buf, sizeof(buf), "%d/%d", cl, cr);
    if (strcmp(buf, _s_clips) != 0) {
      strncpy(_s_clips, buf, sizeof(_s_clips) - 1); _s_clips[sizeof(_s_clips)-1] = '\0';
      lv_label_set_text(_lbl_clip_val, buf);
      lv_obj_set_style_text_color(_lbl_clip_val,
          (cl > 0 || cr > 0) ? MB_COLOR_METER_RED : MB_COLOR_METER_GREEN, 0);
    }

    /* Meter source button */
    if (_lbl_meter_src_btn) {
      bool master = (state->meter_source == MB_SRC_MASTER);
      const char *src_str = portrait ? (master ? "MSTR" : "TRCK")
                                     : (master ? LV_SYMBOL_AUDIO " MASTER"
                                               : LV_SYMBOL_AUDIO " TRACK");
      if (strcmp(src_str, _s_src) != 0) {
        strncpy(_s_src, src_str, sizeof(_s_src) - 1);
        lv_label_set_text(_lbl_meter_src_btn, src_str);
        lv_obj_set_style_bg_color(_btn_meter_src,
            master ? MB_COLOR_ACCENT_PURPLE : MB_COLOR_ACCENT_SEAFOAM, 0);
      }
    }
  }

  /* ── Portrait info panel update ── */
  if (_lbl_portrait_lufs_big) {
    /* Large integrated LUFS */
    static char _last_lufs_big[12] = "";
    if (state->lufs_integrated <= -89.9f) {
      if (strcmp(_last_lufs_big, "--.--") != 0) {
        strncpy(_last_lufs_big, "--.--", sizeof(_last_lufs_big) - 1);
        lv_label_set_text(_lbl_portrait_lufs_big, "--.--");
      }
    } else {
      snprintf(buf, sizeof(buf), "%.1f", state->lufs_integrated);
      if (strcmp(_last_lufs_big, buf) != 0) {
        strncpy(_last_lufs_big, buf, sizeof(_last_lufs_big) - 1);
        lv_label_set_text(_lbl_portrait_lufs_big, buf);
      }
    }
    /* Color by loudness level */
    float li = state->lufs_integrated;
    static int _last_lufs_zone = -1;
    int zone = (li > -9.0f)  ? 0 :
               (li > -14.0f) ? 1 :
               (li > -23.0f) ? 2 : 3;
    if (zone != _last_lufs_zone) {
      _last_lufs_zone = zone;
      lv_color_t lufs_col = (zone == 0) ? MB_COLOR_METER_CLIP :
                            (zone == 1) ? MB_COLOR_METER_ORANGE :
                            (zone == 2) ? MB_COLOR_ACCENT_PURPLE :
                                          MB_COLOR_TEXT_MUTED;
      lv_obj_set_style_text_color(_lbl_portrait_lufs_big, lufs_col, 0);
    }
  }

  if (_lbl_portrait_track) {
    /* Track color swatch */
    if (_swatch_portrait_track) {
      static uint16_t _last_color_r = 999, _last_color_g = 999, _last_color_b = 999;
      if (state->track_color_r != _last_color_r || state->track_color_g != _last_color_g || state->track_color_b != _last_color_b) {
        _last_color_r = state->track_color_r;
        _last_color_g = state->track_color_g;
        _last_color_b = state->track_color_b;
        lv_color_t tc = lv_color_make(state->track_color_r, state->track_color_g, state->track_color_b);
        lv_obj_set_style_bg_color(_swatch_portrait_track, tc, 0);
      }
    }
    /* Track name */
    static char _last_trk[64] = "";
    if (state->meter_source == MB_SRC_MASTER) {
      if (strcmp(_last_trk, "MASTER") != 0) {
        strncpy(_last_trk, "MASTER", sizeof(_last_trk) - 1);
        lv_label_set_text(_lbl_portrait_track, "MASTER");
      }
    } else {
      snprintf(buf, sizeof(buf), "TRK %d: %s", state->track_index, state->track_name);
      if (strcmp(_last_trk, buf) != 0) {
        strncpy(_last_trk, buf, sizeof(_last_trk) - 1);
        lv_label_set_text(_lbl_portrait_track, buf);
      }
    }
  }

  if (_lbl_portrait_project) {
    static char _last_proj[64] = "";
    const char* val = state->project_name[0] ? state->project_name : "(No Project)";
    if (strcmp(_last_proj, val) != 0) {
      strncpy(_last_proj, val, sizeof(_last_proj) - 1);
      lv_label_set_text(_lbl_portrait_project, val);
    }
  }

  if (_lbl_portrait_section) {
    static char _last_sec[64] = "";
    const char* val = state->section_name[0] ? state->section_name : "---";
    if (strcmp(_last_sec, val) != 0) {
      strncpy(_last_sec, val, sizeof(_last_sec) - 1);
      lv_label_set_text(_lbl_portrait_section, val);
    }
  }

  if (_lbl_portrait_position) {
    float secs = state->position_secs;
    int m  = (int)(secs / 60.0f);
    float s = secs - m * 60.0f;
    snprintf(buf, sizeof(buf), "%d:%05.2f", m, s);
    static char _last_pos[16] = "";
    if (strcmp(_last_pos, buf) != 0) {
      strncpy(_last_pos, buf, sizeof(_last_pos) - 1);
      lv_label_set_text(_lbl_portrait_position, buf);
    }
  }
}

#endif /* SCREEN_METERS_H */
