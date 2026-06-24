/*
 * MeterBridge — UI Theme System
 *
 * Runtime-switchable themes. All color tokens are lv_color_t globals
 * (not #defines) so mb_theme_load() can change them at startup based
 * on the NVS preference — no recompile required.
 *
 * Themes:
 *   0 — Codeine Crazy  (deep purple / neon — default)
 *   1 — Pro Broadcast  (studio grey / amber)
 *   2 — Retro LED      (pure black / bright green)
 *   3 — Neon Synth     (ultra dark / cyan + magenta)
 *   4 — VU Classic     (warm walnut / analog amber)
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

/* ─── Theme ID Enum ──────────────────────────────────────────── */

typedef enum {
    MB_THEME_CODEINE_CRAZY    = 0,
    MB_THEME_PRO_BROADCAST    = 1,
    MB_THEME_RETRO_LED        = 2,
    MB_THEME_NEON_SYNTH       = 3,
    MB_THEME_VU_CLASSIC       = 4,
    MB_THEME_DEEP_OCEAN       = 5,
    MB_THEME_INFERNO          = 6,
    MB_THEME_ARCTIC_FROST     = 7,
    MB_THEME_PSYCHEDELIC_MAN  = 8,
    MB_THEME_MIDNIGHT_STUDIO  = 9,
    MB_THEME_CHROMATIC        = 10,
    MB_THEME_COUNT            = 11
} mb_theme_id_t;

static const char* MB_THEME_NAMES[MB_THEME_COUNT] = {
    "Codeine Crazy",
    "Pro Broadcast",
    "Retro LED",
    "Neon Synth",
    "VU Classic",
    "Deep Ocean",
    "Inferno",
    "Arctic Frost",
    "Psychedelic Spaceman",
    "Midnight Studio",
    "Chromatic"
};

/* ─── Runtime Color Tokens (defined in main.cpp) ─────────────── */

extern lv_color_t MB_COLOR_BG_PRIMARY;
extern lv_color_t MB_COLOR_BG_SECONDARY;
extern lv_color_t MB_COLOR_BG_TERTIARY;
extern lv_color_t MB_COLOR_BG_ELEVATED;

extern lv_color_t MB_COLOR_BORDER;
extern lv_color_t MB_COLOR_BORDER_ACCENT;

extern lv_color_t MB_COLOR_TEXT_PRIMARY;
extern lv_color_t MB_COLOR_TEXT_SECONDARY;
extern lv_color_t MB_COLOR_TEXT_MUTED;

extern lv_color_t MB_COLOR_METER_GREEN;
extern lv_color_t MB_COLOR_METER_YELLOW;
extern lv_color_t MB_COLOR_METER_ORANGE;
extern lv_color_t MB_COLOR_METER_RED;
extern lv_color_t MB_COLOR_METER_CLIP;
extern lv_color_t MB_COLOR_METER_BG;

extern lv_color_t MB_COLOR_ACCENT_MAGENTA;
extern lv_color_t MB_COLOR_ACCENT_SEAFOAM;
extern lv_color_t MB_COLOR_ACCENT_ROSE;
extern lv_color_t MB_COLOR_ACCENT_AMBER;
extern lv_color_t MB_COLOR_ACCENT_PURPLE;
extern lv_color_t MB_COLOR_ACCENT_AUBURN;
extern lv_color_t MB_COLOR_ACCENT_BLUE;

extern lv_color_t MB_COLOR_PLAY;
extern lv_color_t MB_COLOR_STOP;
extern lv_color_t MB_COLOR_RECORD;
extern lv_color_t MB_COLOR_ACTIVE;

extern lv_color_t MB_COLOR_PHASE_GOOD;
extern lv_color_t MB_COLOR_PHASE_WARN;
extern lv_color_t MB_COLOR_PHASE_BAD;

/* ─── Runtime Configurable Knobs ─────────────────────────────── */

/* Peak hold duration in ms — default 2500, user-configurable */
extern uint32_t g_peak_hold_ms;

/* Current display rotation (LV_DISP_ROT_NONE / _90 / _180 / _270) */
extern uint8_t  g_display_rotation;

/* Peak hold ballistic duration in ms (UINT32_MAX = hold forever) */
extern uint32_t g_peak_hold_ms;

/* Current active theme id */
extern uint8_t  g_current_theme;

/* Current spectral color mode */
extern uint8_t  g_spec_color_mode;

/* ─── Layout Constants (fixed, independent of theme) ─────────── */

#define MB_STATUS_BAR_HEIGHT    38
#define MB_TRANSPORT_HEIGHT     62
#define MB_MAIN_AREA_HEIGHT     (SCREEN_HEIGHT - MB_STATUS_BAR_HEIGHT - MB_TRANSPORT_HEIGHT)
#define MB_SIDEBAR_WIDTH        200
#define MB_MAIN_METER_WIDTH     (SCREEN_WIDTH - MB_SIDEBAR_WIDTH)

#define MB_SPACE_XXS   2
#define MB_SPACE_XS    4
#define MB_SPACE_SM    8
#define MB_SPACE_MD    12
#define MB_SPACE_LG    16
#define MB_SPACE_XL    24
#define MB_SPACE_XXL   32

#define MB_BORDER_WIDTH     1
#define MB_BORDER_RADIUS    6
#define MB_SHADOW_OFS_X     0
#define MB_SHADOW_OFS_Y     4
#define MB_SHADOW_SPREAD    12

#define MB_METER_BAR_WIDTH  24
#define MB_METER_BAR_GAP    6
#define MB_METER_LABEL_W    48

/* ─── Animation Durations ─────────────────────────────────────── */

#define MB_ANIM_FAST       100
#define MB_ANIM_NORMAL     250
#define MB_ANIM_SLOW       500
#define MB_ANIM_METER_FALL 180
/* MB_ANIM_PEAK_HOLD is now g_peak_hold_ms (runtime configurable) */

/* ─── Font References ─────────────────────────────────────────── */

#define MB_FONT_TITLE       &lv_font_montserrat_20
#define MB_FONT_LABEL       &lv_font_montserrat_14
#define MB_FONT_VALUE       &lv_font_montserrat_28
#define MB_FONT_VALUE_LG    &lv_font_montserrat_36
#define MB_FONT_VALUE_XL    &lv_font_montserrat_48
#define MB_FONT_SMALL       &lv_font_montserrat_12
#define MB_FONT_TRANSPORT   &lv_font_montserrat_24
#define MB_FONT_STATUS      &lv_font_montserrat_16

/* ─── Theme Loader ────────────────────────────────────────────── */

/**
 * Set all runtime color tokens to the chosen theme palette.
 * Call once in setup() after reading NVS preferences.
 */
static inline void mb_theme_load(uint8_t id) {
    g_current_theme = id;

    switch (id) {

    /* ── 0: Codeine Crazy ── deep purple / neon violet (default) */
    default:
    case MB_THEME_CODEINE_CRAZY:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x06030E);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x0D0718);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x150C24);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x1E1233);
        MB_COLOR_BORDER          = lv_color_hex(0x2D1B4E);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x4A2D7A);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xEDE6F5);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x9B8ABF);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x5E4A80);
        MB_COLOR_METER_GREEN     = lv_color_hex(0x7B68EE);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xBB86FC);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xE040FB);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF006E);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF0055);
        MB_COLOR_METER_BG        = lv_color_hex(0x0A0614);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xC158DC);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x7C4DFF);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xE91E8C);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xBB86FC);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x9B30FF);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x6A0DAD);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x651FFF);
        MB_COLOR_PLAY            = lv_color_hex(0xBB86FC);
        MB_COLOR_STOP            = lv_color_hex(0x7B6A9E);
        MB_COLOR_RECORD          = lv_color_hex(0xFF006E);
        MB_COLOR_ACTIVE          = lv_color_hex(0xE040FB);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0xBB86FC);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xE040FB);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF006E);
        break;

    /* ── 1: Pro Broadcast ── neutral grey / warm amber — studio console */
    case MB_THEME_PRO_BROADCAST:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x141414);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x1E1E1E);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x282828);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x333333);
        MB_COLOR_BORDER          = lv_color_hex(0x3A3A3A);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0xFF8C00);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xFFFFFF);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0xB0BEC5);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x546E7A);
        MB_COLOR_METER_GREEN     = lv_color_hex(0x00C853);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xFFD600);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xFF6D00);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF1744);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF0000);
        MB_COLOR_METER_BG        = lv_color_hex(0x0A0A0A);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xFF8C00);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x29B6F6);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF4081);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xFFAB00);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x7E57C2);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0xBF360C);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x0288D1);
        MB_COLOR_PLAY            = lv_color_hex(0x00C853);
        MB_COLOR_STOP            = lv_color_hex(0x546E7A);
        MB_COLOR_RECORD          = lv_color_hex(0xFF1744);
        MB_COLOR_ACTIVE          = lv_color_hex(0xFF8C00);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x00C853);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xFFD600);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF1744);
        break;

    /* ── 2: Retro LED ── pure black / bright phosphor green */
    case MB_THEME_RETRO_LED:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x000000);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x080808);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x101010);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x1A1A1A);
        MB_COLOR_BORDER          = lv_color_hex(0x1C3A1C);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x33FF33);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0x33FF33);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x22AA22);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x115511);
        MB_COLOR_METER_GREEN     = lv_color_hex(0x33FF33);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xFFFF00);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xFF8800);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF2200);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF0000);
        MB_COLOR_METER_BG        = lv_color_hex(0x020602);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0x33FF33);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x00FF88);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF4400);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xFFAA00);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x00CCFF);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x884400);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x00AAFF);
        MB_COLOR_PLAY            = lv_color_hex(0x33FF33);
        MB_COLOR_STOP            = lv_color_hex(0x228822);
        MB_COLOR_RECORD          = lv_color_hex(0xFF2200);
        MB_COLOR_ACTIVE          = lv_color_hex(0xFFFF00);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x33FF33);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xFFAA00);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF2200);
        break;

    /* ── 3: Neon Synth ── ultra dark / cyan + hot pink synthwave */
    case MB_THEME_NEON_SYNTH:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x05000F);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x0A0020);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x120030);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x1A0040);
        MB_COLOR_BORDER          = lv_color_hex(0x2A0060);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x00FFFF);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xFFFFFF);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x00DDCC);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x004455);
        MB_COLOR_METER_GREEN     = lv_color_hex(0x00FFFF);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0x00FF88);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xFF00FF);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF007F);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF0044);
        MB_COLOR_METER_BG        = lv_color_hex(0x020008);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xFF00FF);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x00FFFF);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF007F);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xFFEE00);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0xAA00FF);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x660088);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x0066FF);
        MB_COLOR_PLAY            = lv_color_hex(0x00FFFF);
        MB_COLOR_STOP            = lv_color_hex(0x004455);
        MB_COLOR_RECORD          = lv_color_hex(0xFF007F);
        MB_COLOR_ACTIVE          = lv_color_hex(0xFF00FF);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x00FFFF);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xFF00FF);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF007F);
        break;

    /* ── 4: VU Classic ── warm walnut / analog amber */
    case MB_THEME_VU_CLASSIC:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x1A1008);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x22160A);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x2C1E0E);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x3A2812);
        MB_COLOR_BORDER          = lv_color_hex(0x5A3A18);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0xE8A030);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xF5DEB3);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0xC8A878);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x7A5E30);
        MB_COLOR_METER_GREEN     = lv_color_hex(0x8BC653);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xE8C840);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xE87820);
        MB_COLOR_METER_RED       = lv_color_hex(0xE83820);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF2000);
        MB_COLOR_METER_BG        = lv_color_hex(0x0E0804);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xE8A030);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x8BC653);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xE83820);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xE8C840);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0xA06020);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x6A3010);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x4A8080);
        MB_COLOR_PLAY            = lv_color_hex(0x8BC653);
        MB_COLOR_STOP            = lv_color_hex(0x7A5E30);
        MB_COLOR_RECORD          = lv_color_hex(0xE83820);
        MB_COLOR_ACTIVE          = lv_color_hex(0xE8A030);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x8BC653);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xE8C840);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xE83820);
        break;

    /* ── 5: Deep Ocean ── bioluminescent blue-teal on abyss black */
    case MB_THEME_DEEP_OCEAN:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x000810);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x001220);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x001A30);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x002540);
        MB_COLOR_BORDER          = lv_color_hex(0x003355);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x00AADD);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xC8F0FF);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x60B8D8);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x204060);
        MB_COLOR_METER_GREEN     = lv_color_hex(0x00FFCC);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0x00DDFF);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0x3380FF);
        MB_COLOR_METER_RED       = lv_color_hex(0x0055FF);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF3300);
        MB_COLOR_METER_BG        = lv_color_hex(0x000508);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0x00CCFF);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x00FFCC);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0x0066FF);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0x00AADD);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x0088CC);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x003366);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x0044BB);
        MB_COLOR_PLAY            = lv_color_hex(0x00FFCC);
        MB_COLOR_STOP            = lv_color_hex(0x204060);
        MB_COLOR_RECORD          = lv_color_hex(0xFF3300);
        MB_COLOR_ACTIVE          = lv_color_hex(0x00CCFF);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x00FFCC);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0x00AADD);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF3300);
        break;

    /* ── 6: Inferno ── jet black / molten amber / fire orange */
    case MB_THEME_INFERNO:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x080200);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x100500);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x180800);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x220C00);
        MB_COLOR_BORDER          = lv_color_hex(0x3A1400);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0xFF6600);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xFFE8CC);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0xCC8855);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x663322);
        MB_COLOR_METER_GREEN     = lv_color_hex(0xFFAA00);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xFF7700);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xFF4400);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF1100);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFFFFFF);
        MB_COLOR_METER_BG        = lv_color_hex(0x040100);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xFF6600);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0xFFAA00);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF2200);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xFF8800);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0xDD4400);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x882200);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0xFFCC44);
        MB_COLOR_PLAY            = lv_color_hex(0xFFAA00);
        MB_COLOR_STOP            = lv_color_hex(0x663322);
        MB_COLOR_RECORD          = lv_color_hex(0xFF1100);
        MB_COLOR_ACTIVE          = lv_color_hex(0xFF6600);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0xFFAA00);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xFF6600);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF1100);
        break;

    /* ── 7: Arctic Frost ── near-black charcoal / ice blue / white */
    case MB_THEME_ARCTIC_FROST:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x030810);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x071020);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x0C1828);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x122030);
        MB_COLOR_BORDER          = lv_color_hex(0x1A3050);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x88CCFF);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xF0F8FF);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x88BBDD);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x334466);
        MB_COLOR_METER_GREEN     = lv_color_hex(0xCCEEFF);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0x88DDFF);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0x44AAFF);
        MB_COLOR_METER_RED       = lv_color_hex(0x2266FF);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF4444);
        MB_COLOR_METER_BG        = lv_color_hex(0x02050A);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0x88CCFF);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0xAAEEFF);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0x4499FF);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0x77BBFF);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x5599EE);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x224466);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x3377CC);
        MB_COLOR_PLAY            = lv_color_hex(0xAAEEFF);
        MB_COLOR_STOP            = lv_color_hex(0x334466);
        MB_COLOR_RECORD          = lv_color_hex(0xFF4444);
        MB_COLOR_ACTIVE          = lv_color_hex(0x88CCFF);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0xAAEEFF);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0x88CCFF);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF4444);
        break;

    /* ── 8: Psychedelic Spaceman ── void black / nebula / alien acid trip */
    case MB_THEME_PSYCHEDELIC_MAN:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x04000C);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x08011A);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x0E0328);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x160540);
        MB_COLOR_BORDER          = lv_color_hex(0x280B55);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0xFF44FF);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xFFFFFF);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0xDD88FF);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x5A2288);
        /* Meter: teal → alien lime → UV violet → supernova pink → white-hot */
        MB_COLOR_METER_GREEN     = lv_color_hex(0x00FFAA);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xAAFF00);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xFF00FF);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF0088);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFFFFFF);
        MB_COLOR_METER_BG        = lv_color_hex(0x020008);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xFF44FF);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x00FFAA);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF0088);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xAAFF00);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x8833FF);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x440088);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x00BBFF);
        MB_COLOR_PLAY            = lv_color_hex(0x00FFAA);
        MB_COLOR_STOP            = lv_color_hex(0x5A2288);
        MB_COLOR_RECORD          = lv_color_hex(0xFF0088);
        MB_COLOR_ACTIVE          = lv_color_hex(0xFF44FF);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x00FFAA);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xAAFF00);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF0088);
        break;

    /* ── 9: Midnight Studio ── dark navy / broadcast G→Y→O→R / magenta glow
     * Inspired by: the main screen promotional render.
     * Traditional metering colors grounded in a rich navy/slate environment
     * with magenta transport accents — the look of a premium hardware meter. */
    case MB_THEME_MIDNIGHT_STUDIO:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x0D0F14);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x111521);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x181E2E);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x1E2840);
        MB_COLOR_BORDER          = lv_color_hex(0x1E2851);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x4A34A8);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xEEF0FF);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x8899CC);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x334466);
        /* Broadcast-standard meter colors — green safe, red danger */
        MB_COLOR_METER_GREEN     = lv_color_hex(0x00E676);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0xFFEA00);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xFF6D00);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF1744);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF0055);
        MB_COLOR_METER_BG        = lv_color_hex(0x070810);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xC158DC);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x00E676);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF1744);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xFFEA00);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x7B30FF);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x3D1880);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x3D7AFF);
        MB_COLOR_PLAY            = lv_color_hex(0x00E676);
        MB_COLOR_STOP            = lv_color_hex(0x334466);
        MB_COLOR_RECORD          = lv_color_hex(0xFF1744);
        MB_COLOR_ACTIVE          = lv_color_hex(0xC158DC);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x00E676);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xFFEA00);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF1744);
        break;

    /* ── 10: Chromatic ── spectral cyan → seafoam → chartreuse → magenta
     * Inspired by: the spectrum analyzer promotional render.
     * Each meter zone maps one step of the visible spectrum — low frequencies
     * glow cyan, mids shift green, upper-mids go chartreuse, peaks bloom magenta. */
    case MB_THEME_CHROMATIC:
        MB_COLOR_BG_PRIMARY      = lv_color_hex(0x030508);
        MB_COLOR_BG_SECONDARY    = lv_color_hex(0x060A10);
        MB_COLOR_BG_TERTIARY     = lv_color_hex(0x0A1018);
        MB_COLOR_BG_ELEVATED     = lv_color_hex(0x0E1820);
        MB_COLOR_BORDER          = lv_color_hex(0x103040);
        MB_COLOR_BORDER_ACCENT   = lv_color_hex(0x00DDCC);
        MB_COLOR_TEXT_PRIMARY    = lv_color_hex(0xD0FFFA);
        MB_COLOR_TEXT_SECONDARY  = lv_color_hex(0x55CCBB);
        MB_COLOR_TEXT_MUTED      = lv_color_hex(0x1A4A44);
        /* Spectral gradient: cyan (safe) → seafoam → chartreuse → magenta (hot) */
        MB_COLOR_METER_GREEN     = lv_color_hex(0x00FFDD);
        MB_COLOR_METER_YELLOW    = lv_color_hex(0x00FF88);
        MB_COLOR_METER_ORANGE    = lv_color_hex(0xAAFF00);
        MB_COLOR_METER_RED       = lv_color_hex(0xFF00CC);
        MB_COLOR_METER_CLIP      = lv_color_hex(0xFF88FF);
        MB_COLOR_METER_BG        = lv_color_hex(0x020305);
        MB_COLOR_ACCENT_MAGENTA  = lv_color_hex(0xFF00CC);
        MB_COLOR_ACCENT_SEAFOAM  = lv_color_hex(0x00FFDD);
        MB_COLOR_ACCENT_ROSE     = lv_color_hex(0xFF44AA);
        MB_COLOR_ACCENT_AMBER    = lv_color_hex(0xAAFF00);
        MB_COLOR_ACCENT_PURPLE   = lv_color_hex(0x8800FF);
        MB_COLOR_ACCENT_AUBURN   = lv_color_hex(0x004440);
        MB_COLOR_ACCENT_BLUE     = lv_color_hex(0x00AAFF);
        MB_COLOR_PLAY            = lv_color_hex(0x00FFDD);
        MB_COLOR_STOP            = lv_color_hex(0x1A4A44);
        MB_COLOR_RECORD          = lv_color_hex(0xFF00CC);
        MB_COLOR_ACTIVE          = lv_color_hex(0x00FFDD);
        MB_COLOR_PHASE_GOOD      = lv_color_hex(0x00FFDD);
        MB_COLOR_PHASE_WARN      = lv_color_hex(0xAAFF00);
        MB_COLOR_PHASE_BAD       = lv_color_hex(0xFF00CC);
        break;
    }
}

/* ─── LVGL Theme Initialization ──────────────────────────────── */

/**
 * Apply current color tokens to LVGL's default theme.
 * Call after mb_theme_load() and after display is init'd.
 */
static inline void mb_theme_init(void) {
    lv_disp_t* disp = lv_disp_get_default();
    if (!disp) return;

    lv_disp_set_bg_color(disp, MB_COLOR_BG_PRIMARY);

    lv_theme_t* theme = lv_theme_default_init(
        disp,
        MB_COLOR_ACCENT_PURPLE,
        MB_COLOR_ACCENT_MAGENTA,
        true,
        MB_FONT_LABEL
    );
    lv_disp_set_theme(disp, theme);
}

/* ─── Widget Factory Functions ────────────────────────────────── */

static inline lv_obj_t* mb_create_panel(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_style_bg_color(panel, MB_COLOR_BG_SECONDARY, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_90, 0);
    lv_obj_set_style_border_color(panel, MB_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(panel, MB_BORDER_WIDTH, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_radius(panel, MB_BORDER_RADIUS, 0);
    lv_obj_set_style_pad_all(panel, MB_SPACE_SM, 0);
    lv_obj_set_style_shadow_color(panel, MB_COLOR_ACCENT_PURPLE, 0);
    lv_obj_set_style_shadow_width(panel, 8, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
    lv_obj_set_style_shadow_spread(panel, 2, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static inline lv_obj_t* mb_create_value_label(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, MB_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(label, MB_FONT_VALUE, 0);
    return label;
}

static inline lv_obj_t* mb_create_caption_label(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(label, MB_FONT_SMALL, 0);
    lv_obj_set_style_text_letter_space(label, 3, 0);
    return label;
}

static inline void mb_style_transport_btn(lv_obj_t* btn, lv_color_t color) {
    lv_obj_set_style_bg_color(btn, MB_COLOR_BG_TERTIARY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, MB_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(btn, MB_BORDER_WIDTH, 0);
    lv_obj_set_style_radius(btn, MB_BORDER_RADIUS, 0);
    lv_obj_set_style_text_color(btn, MB_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(btn, MB_FONT_TRANSPORT, 0);
    lv_obj_set_style_shadow_color(btn, color, 0);
    lv_obj_set_style_shadow_width(btn, 6, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_spread(btn, 1, 0);
    lv_obj_set_style_bg_color(btn, color, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, MB_COLOR_BG_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 16, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_spread(btn, 4, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, color, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btn, MB_COLOR_BG_PRIMARY, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btn, color, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_50, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(btn, 12, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_spread(btn, 3, LV_STATE_CHECKED);
}

#endif /* UI_THEME_H */
