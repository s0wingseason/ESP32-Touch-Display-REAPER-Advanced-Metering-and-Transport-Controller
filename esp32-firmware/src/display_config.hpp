/*
 * MeterBridge — Display Configuration
 * 
 * LovyanGFX panel configuration for the Elecrow CrowPanel 7" (800x480).
 * Auto-detects board version and configures accordingly.
 * 
 * ESP32-S3-WROOM-1-N4R8
 * Display: 800x480 RGB parallel TFT
 * Touch: GT911 capacitive (I2C)
 */

#ifndef DISPLAY_CONFIG_HPP
#define DISPLAY_CONFIG_HPP

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
/* Panel_RGB / Bus_RGB are ESP32-S3-specific, not in the generic v1_init */
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

/* ─── Display Dimensions ─────────────────────────────────────── */
#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  480

/* ─── CrowPanel 7" Pin Definitions (RGB Parallel Bus) ─────── */

/* RGB Data Bus */
#define TFT_B0  15
#define TFT_B1   7
#define TFT_B2   6
#define TFT_B3   5
#define TFT_B4   4

#define TFT_G0   9
#define TFT_G1  46
#define TFT_G2   3
#define TFT_G3   8
#define TFT_G4  16
#define TFT_G5   1

#define TFT_R0  14
#define TFT_R1  21
#define TFT_R2  47
#define TFT_R3  48
#define TFT_R4  45

/* RGB Control Signals (verified working - community confirmed) */
#define TFT_HSYNC  39
#define TFT_VSYNC  40
#define TFT_DE     41
#define TFT_PCLK    0

/* Backlight */
#define TFT_BL     2

/* Touch I2C */
#define TOUCH_SDA  19
#define TOUCH_SCL  20
#define TOUCH_INT  18
#define TOUCH_RST  38

/* I2C for PCA9557 IO expander (V3.0 boards) */
#define I2C_SDA    19
#define I2C_SCL    20

/* GT911 I2C Address */
#define TOUCH_ADDR 0x5D

/* PCA9557 I2C Address (V3.0 only) */
#define PCA9557_ADDR 0x19

/* Backlight controller I2C address (some revisions) */
#define BACKLIGHT_I2C_ADDR 0x30

/* ─── LovyanGFX Display Class ────────────────────────────────── */

class LGFX_CrowPanel : public lgfx::LGFX_Device {
    lgfx::Panel_RGB _panel_instance;
    lgfx::Bus_RGB   _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;

public:
    LGFX_CrowPanel(void) {
        /* ── Bus Configuration (RGB Parallel) ── */
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;
            cfg.pin_d0  = TFT_B0;
            cfg.pin_d1  = TFT_B1;
            cfg.pin_d2  = TFT_B2;
            cfg.pin_d3  = TFT_B3;
            cfg.pin_d4  = TFT_B4;
            cfg.pin_d5  = TFT_G0;
            cfg.pin_d6  = TFT_G1;
            cfg.pin_d7  = TFT_G2;
            cfg.pin_d8  = TFT_G3;
            cfg.pin_d9  = TFT_G4;
            cfg.pin_d10 = TFT_G5;
            cfg.pin_d11 = TFT_R0;
            cfg.pin_d12 = TFT_R1;
            cfg.pin_d13 = TFT_R2;
            cfg.pin_d14 = TFT_R3;
            cfg.pin_d15 = TFT_R4;

            cfg.pin_henable = TFT_DE;
            cfg.pin_vsync   = TFT_VSYNC;
            cfg.pin_hsync   = TFT_HSYNC;
            cfg.pin_pclk    = TFT_PCLK;

            cfg.freq_write = 12000000; /* 12 MHz pixel clock — confirmed stable on CrowPanel 7".
                                         * 15MHz and 20MHz both caused signal integrity failures.
                                         * Theoretical max: ~31 FPS full-frame; ~60+ FPS with dirty rendering. */

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 40;
            cfg.hsync_pulse_width = 48;
            cfg.hsync_back_porch  = 40;

            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 1;
            cfg.vsync_pulse_width = 31;
            cfg.vsync_back_porch  = 13;

            cfg.pclk_active_neg = 1;
            cfg.de_idle_high    = 0;
            cfg.pclk_idle_high  = 0;

            _bus_instance.config(cfg);
        }

        /* ── Panel Configuration ── */
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = SCREEN_WIDTH;
            cfg.memory_height = SCREEN_HEIGHT;
            cfg.panel_width   = SCREEN_WIDTH;
            cfg.panel_height  = SCREEN_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;

            _panel_instance.config(cfg);
        }

        /* ── Backlight (PWM) ── */
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        /* ── Touch (GT911, I2C) ── */
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = SCREEN_WIDTH - 1;
            cfg.y_min = 0;
            cfg.y_max = SCREEN_HEIGHT - 1;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;

            cfg.i2c_port = 0;
            cfg.i2c_addr = TOUCH_ADDR;
            cfg.pin_sda  = TOUCH_SDA;
            cfg.pin_scl  = TOUCH_SCL;
            cfg.pin_int  = TOUCH_INT;
            cfg.pin_rst  = TOUCH_RST;
            cfg.freq     = 400000;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        _panel_instance.setBus(&_bus_instance);
        setPanel(&_panel_instance);
    }
};

/* ─── PCA9557 IO Expander Init (V3.0 boards) ─────────────────── */

#include <Wire.h>

/**
 * Initialize the PCA9557 IO expander found on CrowPanel V3.0 boards.
 * This must be called before touch initialization on V3.0 hardware.
 * Returns true if the expander was found and initialized.
 */
static inline bool pca9557_init() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.beginTransmission(PCA9557_ADDR);
    
    if (Wire.endTransmission() != 0) {
        /* PCA9557 not found — likely V1.x board, which is fine */
        return false;
    }
    
    /* Configure PCA9557: set all pins as outputs */
    Wire.beginTransmission(PCA9557_ADDR);
    Wire.write(0x03); /* Configuration register */
    Wire.write(0x00); /* All outputs */
    Wire.endTransmission();
    
    /* Set output state: enable touch and display */
    Wire.beginTransmission(PCA9557_ADDR);
    Wire.write(0x01); /* Output register */
    Wire.write(0x05); /* Enable relevant peripherals */
    Wire.endTransmission();
    
    return true;
}

/**
 * Set display backlight brightness via I2C controller (some revisions).
 * @param brightness 0-255
 */
static inline void set_backlight_i2c(uint8_t brightness) {
    Wire.beginTransmission(BACKLIGHT_I2C_ADDR);
    Wire.write(brightness);
    Wire.endTransmission();
}

#endif /* DISPLAY_CONFIG_HPP */
