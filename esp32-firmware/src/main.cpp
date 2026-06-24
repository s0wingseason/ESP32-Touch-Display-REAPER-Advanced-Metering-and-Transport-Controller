/*
 * MeterBridge — Full Application
 * 
 * ESP32-S3 CrowPanel 7" REAPER Advanced Metering & Transport Controller
 * 
 * Verified hardware configuration:
 *   - qio_opi memory (4MB QIO Flash + 8MB OPI PSRAM)
 *   - ARDUINO_USB_MODE=1 (no CDC_ON_BOOT)
 *   - PCLK=GPIO0, DE=GPIO41, VSYNC=GPIO40
 *   - EK9716BD3 timing: hsync 40/48/40, vsync 1/31/13
 *   - PCA9557 IO expander at 0x19 (V3.0 boards)
 */

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "display_config.hpp"
#include "network/wifi_manager.h"
#include "network/udp_comm.h"
#include "ui/ui_manager.h"

/* ─── Display Helper ───────────────────────────────────────────── */
extern void set_display_brightness(uint8_t val);

/* ─── Theme Color Globals (extern declared in theme.h) ─────────── */

lv_color_t MB_COLOR_BG_PRIMARY;
lv_color_t MB_COLOR_BG_SECONDARY;
lv_color_t MB_COLOR_BG_TERTIARY;
lv_color_t MB_COLOR_BG_ELEVATED;
lv_color_t MB_COLOR_BORDER;
lv_color_t MB_COLOR_BORDER_ACCENT;
lv_color_t MB_COLOR_TEXT_PRIMARY;
lv_color_t MB_COLOR_TEXT_SECONDARY;
lv_color_t MB_COLOR_TEXT_MUTED;
lv_color_t MB_COLOR_METER_GREEN;
lv_color_t MB_COLOR_METER_YELLOW;
lv_color_t MB_COLOR_METER_ORANGE;
lv_color_t MB_COLOR_METER_RED;
lv_color_t MB_COLOR_METER_CLIP;
lv_color_t MB_COLOR_METER_BG;
lv_color_t MB_COLOR_ACCENT_MAGENTA;
lv_color_t MB_COLOR_ACCENT_SEAFOAM;
lv_color_t MB_COLOR_ACCENT_ROSE;
lv_color_t MB_COLOR_ACCENT_AMBER;
lv_color_t MB_COLOR_ACCENT_PURPLE;
lv_color_t MB_COLOR_ACCENT_AUBURN;
lv_color_t MB_COLOR_ACCENT_BLUE;
lv_color_t MB_COLOR_PLAY;
lv_color_t MB_COLOR_STOP;
lv_color_t MB_COLOR_RECORD;
lv_color_t MB_COLOR_ACTIVE;
lv_color_t MB_COLOR_PHASE_GOOD;
lv_color_t MB_COLOR_PHASE_WARN;
lv_color_t MB_COLOR_PHASE_BAD;

/* ─── Runtime Configurable State ────────────────────────────────── */
uint32_t g_peak_hold_ms        = 2500;  /* Peak hold ballistic (ms) */
uint8_t  g_display_rotation    = 0;     /* LV_DISP_ROT_NONE */
uint8_t  g_current_theme       = 0;     /* MB_THEME_CODEINE_CRAZY */
uint8_t  g_conn_mode           = 0;     /* 0=UDP/WiFi, 1=USB Serial, 2=Both */
uint32_t g_last_serial_data_ms = 0;     /* millis() when last SMETER received */
uint32_t g_update_interval_ms  = 50;    /* Relay / Lua bridge tick rate [20-2500ms]     */
uint8_t  g_spec_color_mode     = 0;     /* 0=Classic, 1=Neon, 2=Fire, 3=Rainbow, etc.   */
uint8_t  g_brightness          = 80;    /* Display brightness pct [0-100]               */

uint8_t  g_meter_mode          = 0;     /* 0=Classic 1=LRPeak 2=Mid-Side 3=LUFS         */
uint8_t  g_meter_height_idx    = 1;     /* 0=XXL 1=XL 2=L 3=M 4=S 5=XS (default XL)      */
bool     g_show_fps             = true;  /* Show FPS overlay label in status bar          */
float    g_fps_display          = 0.0f; /* 1-second rolling FPS for UI overlay           */
bool     g_ota_in_progress      = false; /* True during OTA firmware upload               */
uint8_t  g_ota_progress         = 0;    /* OTA progress 0-100%                           */
float    g_loudness_target      = -14.0f; /* Loudness target in LUFS (default Spotify)    */
uint8_t  g_loudness_preset      = 0;    /* 0=Streaming 1=Podcast 2=Broadcast 3=Film 4=Classical 5=Off */
float    g_master_volume_db     = 0.0f; /* Master volume from REAPER (dB)                */
bool     g_auto_dim_enabled     = false; /* Screensaver dim (opt-in, default OFF)          */
float    g_ref_db_line          = -18.0f; /* Reference dBFS marker line on meter bars      */
uint32_t g_last_touch_ms        = 0;     /* millis() of last touch — for auto-dim wake     */

/* ─── LVGL Integration ─────────────────────────────────────────── */

static LGFX_CrowPanel lcd;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = NULL;
static lv_color_t* buf2 = NULL;

void set_display_brightness(uint8_t val) {
    lcd.setBrightness(val);
}

/* LVGL flush callback — synchronous pixel transfer.
 * Stable single-core: lv_disp_flush_ready() runs in the same task as
 * lv_timer_handler() (Arduino loop on Core 1), satisfying LVGL's
 * threading requirement. FPS is maximized by the pixel clock in display_config.hpp. */
static void lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* data) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels((lgfx::rgb565_t*)&data->full, w * h);
    lcd.endWrite();
    lv_disp_flush_ready(disp);
}

/* LVGL touch read callback */
static void lvgl_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t tx, ty;
    if (lcd.getTouch(&tx, &ty)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = tx;
        data->point.y = ty;
        g_last_touch_ms = millis();   /* BUG-11 fix: wake auto-dim on physical touch */
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ─── Networking ───────────────────────────────────────────────── */
WiFiManager wifiMgr;
UDPComm     udpComm;

/* stateMux: spinlock protecting multi-byte string fields written from Core 0
 * (AsyncUDP callbacks) and read from Core 1 (loop/LVGL). Declared extern in
 * udp_comm.h so the handler methods can acquire it without a global include. */
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

/* ─── Screenshot Server ─────────────────────────────────────────
 * BMP-over-HTTP on port 8099.  All TCP blocking work runs on
 * Core 0 (a dedicated FreeRTOS task) so loop() / LVGL never stalls.
 *
 * Handoff protocol (Core 0 ↔ Core 1):
 *   1. Core 0 task: raise _snap_take_req  (want new snapshot)
 *   2. Core 1 loop(): detect flag → lv_snapshot_take() → set _snap_ready
 *   3. Core 0 task: read _snap_ptr, send BMP, lv_snapshot_free, done       */

static volatile bool   _snap_take_req  = false;   /* Core0 → Core1 */
static volatile bool   _snap_ready     = false;   /* Core1 → Core0 */
static lv_img_dsc_t*   _snap_ptr       = nullptr; /* guarded by req/ready flags */

/* Write a 24-bit BMP header to client (BGR row-order, bottom-up) */
static void _bmp_send(WiFiClient& client, lv_img_dsc_t* img) {
    uint32_t w = img->header.w;
    uint32_t h = img->header.h;
    uint32_t row_bytes = w * 3;          /* 24-bit RGB, no padding (w is multiple of 4 here) */
    uint32_t px_size  = row_bytes * h;
    uint32_t file_sz  = 54 + px_size;   /* 14-byte file header + 40-byte DIB header */

    /* HTTP header so browsers render it inline */
    client.printf("HTTP/1.1 200 OK\r\n"
                  "Content-Type: image/bmp\r\n"
                  "Content-Length: %u\r\n"
                  "Connection: close\r\n\r\n", (unsigned)(54 + px_size));

    /* BMP file header (14 bytes) */
    uint8_t fh[14] = {'B','M',
        (uint8_t)(file_sz),(uint8_t)(file_sz>>8),(uint8_t)(file_sz>>16),(uint8_t)(file_sz>>24),
        0,0,0,0,  /* reserved */
        54,0,0,0  /* pixel data offset */
    };
    client.write(fh, 14);

    /* DIB header BITMAPINFOHEADER (40 bytes) */
    uint8_t dib[40];
    memset(dib, 0, 40);
    dib[0]=40;                                           /* header size */
    dib[4]=(uint8_t)w; dib[5]=(uint8_t)(w>>8); dib[6]=(uint8_t)(w>>16); dib[7]=(uint8_t)(w>>24);
    /* height negative = top-down; positive = bottom-up (standard). We send bottom-up. */
    dib[8]=(uint8_t)h; dib[9]=(uint8_t)(h>>8); dib[10]=(uint8_t)(h>>16); dib[11]=(uint8_t)(h>>24);
    dib[12]=1; dib[13]=0;    /* color planes */
    dib[14]=24; dib[15]=0;   /* bits per pixel */
    /* compression=0 (BI_RGB), image size=0 (ok for BI_RGB), rest zero */
    client.write(dib, 40);

    /* Pixel data: BMP is bottom-up, LVGL is top-down → reverse row order.
     * LVGL CF_TRUE_COLOR = RGB565; convert to BGR888 on-the-fly.       */
    const lv_color_t* src = (const lv_color_t*)img->data;
    static uint8_t row_buf[800 * 3];   /* max 800px row, static = no stack pressure */
    for (int y = (int)h - 1; y >= 0; y--) {   /* BMP bottom-up */
        const lv_color_t* row = src + y * w;
        for (uint32_t x = 0; x < w; x++) {
            /* RGB565 → BGR888 */
            uint16_t px = lv_color_to16(row[x]);
            row_buf[x*3 + 0] = ((px & 0x001F) << 3);   /* B */
            row_buf[x*3 + 1] = ((px & 0x07E0) >> 3);   /* G */
            row_buf[x*3 + 2] = ((px & 0xF800) >> 8);   /* R */
        }
        client.write(row_buf, w * 3);
    }
}

/* Core 0 task — handles ALL TCP work for port 8099.
 * Blocks freely (client.write, drain loop) without touching LVGL. */
static void _screenshot_task(void* /*pv*/) {
    /* Wait for WiFi before starting listener */
    while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));

    WiFiServer srv(8099);
    srv.begin();
    Serial.printf("[SCREENSHOT] Listening on %s:8099 (Core 0)\n",
                  WiFi.localIP().toString().c_str());

    for (;;) {
        WiFiClient client = srv.available();
        if (!client) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        /* Drain HTTP request bytes — blocking is fine on Core 0 */
        uint32_t t0 = millis();
        while (client.connected() && millis() - t0 < 500) {
            if (client.available()) { client.read(); t0 = millis(); }
            vTaskDelay(1);
        }

        /* Ask Core 1 to take a snapshot */
        _snap_ready    = false;
        _snap_take_req = true;
        t0 = millis();
        while (!_snap_ready && millis() - t0 < 3000) vTaskDelay(pdMS_TO_TICKS(5));

        if (_snap_ready && _snap_ptr) {
            uint32_t sw = _snap_ptr->header.w, sh = _snap_ptr->header.h;
            _bmp_send(client, _snap_ptr);
            /* ps_free is thread-safe; lv_snapshot_free is just ps_free */
            lv_snapshot_free(_snap_ptr);
            _snap_ptr = nullptr;
            Serial.printf("[SCREENSHOT] Sent %dx%d BMP\n", (int)sw, (int)sh);
        } else {
            client.print("HTTP/1.1 503 Service Unavailable\r\n\r\nTimeout");
            Serial.println("[SCREENSHOT] ERROR: snapshot timeout or NULL");
        }
        client.stop();
    }
}


/* ─── Remote Control Server (port 8080) ─────────────────────────────
 * Accepts HTTP from a PC/browser or AI agent (curl) and injects synthetic
 * LVGL touch events via a virtual pointer indev polled on Core 1.
 * All TCP work runs on Core 0 — same pattern as the screenshot server.
 *
 * Shared queue: Core 0 writes _rmt_queue under _rmt_mux, sets
 * _rmt_pending = true.  Core 1 (lvgl_remote_cb) latches it once idle.
 * ─────────────────────────────────────────────────────────────────── */

enum RmtType : uint8_t { RMT_NONE=0, RMT_TAP, RMT_SWIPE, RMT_PRESS };
struct RmtCmd {
    RmtType  type    = RMT_NONE;
    int16_t  x1=400, y1=240, x2=400, y2=240;
    uint16_t steps   = 1;
    uint32_t hold_ms = 150;
};
static volatile bool _rmt_pending = false;
static RmtCmd        _rmt_queue;           /* written Core 0, read Core 1 */
static portMUX_TYPE  _rmt_mux = portMUX_INITIALIZER_UNLOCKED;

/* Self-hosted HTML control panel — stored in .rodata (flash) */
static const char RCTRL_HTML[] =
R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeterBridge Remote</title>
<style>
body{font-family:monospace;background:#111;color:#eee;margin:0;padding:10px}
h2{margin:0 0 8px;color:#0af;font-size:15px}
.row{display:flex;gap:6px;align-items:center;margin:5px 0;flex-wrap:wrap}
input[type=number]{background:#222;color:#eee;border:1px solid #444;padding:3px 6px;width:64px;border-radius:4px}
button{background:#1a6;color:#fff;border:none;padding:5px 10px;border-radius:4px;cursor:pointer;font:12px monospace}
button:hover{filter:brightness(1.3)}
.red{background:#a22}.blue{background:#16a}.grey{background:#445}
#st{font-size:11px;color:#8b8;padding:3px 0;word-break:break-all}
#shot{display:block;max-width:100%;border:1px solid #333;border-radius:4px;background:#1a1a1a;margin:6px 0;min-height:60px}
#log{height:90px;overflow-y:auto;font-size:11px;background:#1a1a1a;border:1px solid #333;border-radius:4px;padding:4px}
</style></head><body>
<h2>&#9889; MeterBridge Remote</h2>
<div id="st">Connecting...</div>
<img id="shot" alt="Screen (waiting for WiFi)">
<div class="row"><b>Nav&nbsp;</b>
  <button onclick="swipe(-1)">&#9664; Prev</button>
  <button onclick="swipe(1)">Next &#9654;</button>
  <button onclick="scroll_(-1)">&#9650; Up</button>
  <button onclick="scroll_(1)">&#9660; Dn</button>
  <button class="grey" onclick="rShot()">&#128247; Snap</button>
</div>
<div class="row"><b>Tap&nbsp;&nbsp;</b>
  X:<input type="number" id="tx" value="400" min="0" max="800">
  Y:<input type="number" id="ty" value="240" min="0" max="480">
  <button onclick="doTap()">Tap</button>
  <button onclick="doPress()">Long Press</button>
</div>
<div class="row"><b>Transport</b>
  <button onclick="cmd('INJECT_PLAY')">&#9654; Play</button>
  <button class="red" onclick="cmd('INJECT_STOP')">&#9632; Stop</button>
</div>
<div class="row"><b>Meter&nbsp;</b>
  <button onclick="meter(-6,-8,-18,-20,-6,-8,-14,-14,-14,3,0.9,0,0)">-6 dBFS</button>
  <button onclick="meter(-12,-11,-20,-19,-12,-11,-18,-18,-20,4,0.85,0,0)">-12 dB</button>
  <button onclick="meter(-24,-24,-32,-32,-24,-24,-30,-30,-35,2,0.8,0,0)">-24 dB</button>
  <button class="blue" onclick="meter(-60,-60,-60,-60,-60,-60,-70,-70,-70,0,1,0,0)">Silence</button>
</div>
<div id="log"></div>
<script>
var h=location.hostname;
function lg(s){var e=document.getElementById('log');e.innerHTML=new Date().toLocaleTimeString()+' '+s+'<br>'+e.innerHTML}
function post(p,d){fetch(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(function(r){return r.text()}).then(function(t){lg(p+' '+t)}).catch(function(e){lg('ERR '+e)})}
function doTap(){var x=+document.getElementById('tx').value,y=+document.getElementById('ty').value;post('/tap',{x:x,y:y})}
function doPress(){var x=+document.getElementById('tx').value,y=+document.getElementById('ty').value;post('/press',{x:x,y:y,ms:600})}
function swipe(d){if(d<0)post('/swipe',{x1:80,y1:240,x2:720,y2:240,steps:20});else post('/swipe',{x1:720,y1:240,x2:80,y2:240,steps:20})}
function scroll_(d){if(d<0)post('/swipe',{x1:400,y1:100,x2:400,y2:380,steps:15});else post('/swipe',{x1:400,y1:380,x2:400,y2:100,steps:15})}
function cmd(c){post('/cmd',{cmd:c})}
function meter(pl,pr,rl,rr,tpl,tpr,lm,ls,li,lr,ph,cl,cr){post('/inject_meter',{peak_l:pl,peak_r:pr,rms_l:rl,rms_r:rr,true_peak_l:tpl,true_peak_r:tpr,lufs_m:lm,lufs_s:ls,lufs_i:li,lufs_r:lr,phase:ph,clip_l:cl,clip_r:cr})}
function rShot(){document.getElementById('shot').src='http://'+h+':8099?t='+Date.now()}
function rStat(){fetch('/status').then(function(r){return r.json()}).then(function(d){document.getElementById('st').textContent='IP:'+d.ip+' FPS:'+d.fps.toFixed(1)+' Heap:'+Math.round(d.heap/1024)+'K PSRAM:'+Math.round(d.psram/1024)+'K WiFi:'+d.wifi+'dBm Conn:'+d.connected+' Play:'+d.playing}).catch(function(){})}
setInterval(rShot,1500);setInterval(rStat,2000);rShot();rStat();
</script></body></html>
)HTML";

/* ── Minimal JSON field extractors (Core 0 only) ── */
static float _rmt_json_f(const String& j, const char* key, float def=0.0f) {
    String k = String('"') + key + "\":"; int p = j.indexOf(k);
    return (p < 0) ? def : j.substring(p + k.length()).toFloat();
}
static int _rmt_json_i(const String& j, const char* key, int def=0) {
    return (int)_rmt_json_f(j, key, (float)def);
}
static String _rmt_json_s(const String& j, const char* key) {
    String k = String('"') + key + "\":\"";
    int p = j.indexOf(k); if (p < 0) return "";
    int s2 = p + k.length(); int e = j.indexOf('"', s2);
    return (e > s2) ? j.substring(s2, e) : "";
}

/* ── LVGL virtual indev callback — Core 1 only, no locking required ── */
static void lvgl_remote_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
    static bool     active     = false;
    static RmtType  cur_type   = RMT_NONE;
    static int16_t  ax=0,ay=0,bx=0,by=0;
    static uint16_t tot_steps=1, cur_step=0;
    static uint32_t hold_ms=0, hold_start=0;

    /* Latch new command when idle */
    if (!active && _rmt_pending) {
        portENTER_CRITICAL(&_rmt_mux);
        cur_type  = _rmt_queue.type;
        ax = _rmt_queue.x1; ay = _rmt_queue.y1;
        bx = _rmt_queue.x2; by = _rmt_queue.y2;
        tot_steps = (_rmt_queue.steps < 1) ? 1 : _rmt_queue.steps;
        hold_ms   = _rmt_queue.hold_ms;
        _rmt_pending = false;
        portEXIT_CRITICAL(&_rmt_mux);
        cur_step   = 0;
        active     = true;
        hold_start = millis();
    }

    if (!active) { data->state = LV_INDEV_STATE_REL; return; }

    switch (cur_type) {
        case RMT_TAP:
            data->point.x = ax; data->point.y = ay;
            if (cur_step < 2) { data->state = LV_INDEV_STATE_PR; cur_step++; }
            else              { data->state = LV_INDEV_STATE_REL; active = false; }
            break;

        case RMT_SWIPE: {
            if (cur_step <= tot_steps) {
                float t = (float)cur_step / (float)tot_steps;
                data->point.x = (int16_t)(ax + (bx - ax) * t);
                data->point.y = (int16_t)(ay + (by - ay) * t);
                data->state   = LV_INDEV_STATE_PR;
                cur_step++;
            } else {
                data->state = LV_INDEV_STATE_REL; active = false;
            }
            break;
        }

        case RMT_PRESS:
            data->point.x = ax; data->point.y = ay;
            if (millis() - hold_start < hold_ms) { data->state = LV_INDEV_STATE_PR; }
            else { data->state = LV_INDEV_STATE_REL; active = false; }
            break;

        default:
            data->state = LV_INDEV_STATE_REL; active = false; break;
    }
}

/* Enqueue a touch command — safe to call from Core 0 */
static void _rmt_enqueue(RmtType type, int16_t x1, int16_t y1,
                          int16_t x2=0, int16_t y2=0,
                          uint16_t steps=1, uint32_t hms=150) {
    portENTER_CRITICAL(&_rmt_mux);
    _rmt_queue.type    = type;
    _rmt_queue.x1      = x1;  _rmt_queue.y1 = y1;
    _rmt_queue.x2      = x2;  _rmt_queue.y2 = y2;
    _rmt_queue.steps   = steps;
    _rmt_queue.hold_ms = hms;
    _rmt_pending = true;
    portEXIT_CRITICAL(&_rmt_mux);
}

/* HTTP response helper */
static void _rmt_send(WiFiClient& c, int code,
                       const char* ct, const String& body) {
    c.printf("HTTP/1.1 %d OK\r\n"
             "Content-Type: %s\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Cache-Control: no-cache\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n\r\n",
             code, ct, (int)body.length());
    c.print(body);
}

/* Core 0 task — Remote Control HTTP server on port 8080 */
static void _remote_ctrl_task(void* /*pv*/) {
    while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));

    WiFiServer srv(8080);
    srv.begin();
    Serial.printf("[RCTRL] Remote control: http://%s:8080\n",
                  WiFi.localIP().toString().c_str());

    for (;;) {
        WiFiClient client = srv.available();
        if (!client) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        /* Read request line + headers */
        String reqLine;
        int    contentLen = 0;
        uint32_t t0 = millis();
        while (client.connected() && millis() - t0 < 2000) {
            if (!client.available()) { vTaskDelay(1); continue; }
            String ln = client.readStringUntil('\n');
            ln.trim();
            if (reqLine.length() == 0)           reqLine = ln;
            else if (ln.startsWith("Content-Length:")) contentLen = ln.substring(15).toInt();
            else if (ln.length() == 0)           break;  /* end of headers */
        }

        /* Read body */
        String body;
        contentLen = (contentLen > 512) ? 512 : contentLen;
        if (contentLen > 0) {
            t0 = millis();
            while ((int)body.length() < contentLen && millis() - t0 < 1000) {
                if (client.available()) body += (char)client.read();
                else vTaskDelay(1);
            }
        }

        bool isPost = reqLine.startsWith("POST");
        String resp = "{\"ok\":true}";
        const char* ct = "application/json";

        /* ── Route ── */
        if (!isPost && (reqLine.indexOf(" / ") >= 0 || reqLine.indexOf(" /\r") >= 0)) {
            /* GET / → embedded HTML panel */
            _rmt_send(client, 200, "text/html; charset=utf-8", String(RCTRL_HTML));
            client.stop(); continue;

        } else if (reqLine.indexOf("/status") >= 0) {
            meter_state_t* s = &udpComm.state;
            char buf[300];
            snprintf(buf, sizeof(buf),
                "{\"ip\":\"%s\",\"fps\":%.1f,\"heap\":%d,\"psram\":%d,"
                "\"wifi\":%d,\"connected\":%d,"
                "\"peak_l\":%.1f,\"peak_r\":%.1f,"
                "\"lufs_m\":%.1f,\"lufs_i\":%.1f,"
                "\"playing\":%d}",
                WiFi.localIP().toString().c_str(),
                (float)g_fps_display,
                (int)ESP.getFreeHeap(), (int)ESP.getFreePsram(),
                (int)WiFi.RSSI(),
                (int)s->connected,
                (float)s->peak_l, (float)s->peak_r,
                (float)s->lufs_momentary, (float)s->lufs_integrated,
                (int)(s->transport_flags & MB_TRANSPORT_PLAYING));
            resp = buf;

        } else if (isPost && reqLine.indexOf("/tap") >= 0) {
            int x = constrain(_rmt_json_i(body,"x",400), 0, 800);
            int y = constrain(_rmt_json_i(body,"y",240), 0, 480);
            _rmt_enqueue(RMT_TAP, (int16_t)x, (int16_t)y);
            resp = "{\"ok\":true,\"action\":\"tap\"}";

        } else if (isPost && reqLine.indexOf("/swipe") >= 0) {
            int x1 = constrain(_rmt_json_i(body,"x1",100), 0, 800);
            int y1 = constrain(_rmt_json_i(body,"y1",240), 0, 480);
            int x2 = constrain(_rmt_json_i(body,"x2",700), 0, 800);
            int y2 = constrain(_rmt_json_i(body,"y2",240), 0, 480);
            int st = constrain(_rmt_json_i(body,"steps",15), 1, 60);
            _rmt_enqueue(RMT_SWIPE,(int16_t)x1,(int16_t)y1,
                         (int16_t)x2,(int16_t)y2,(uint16_t)st);
            resp = "{\"ok\":true,\"action\":\"swipe\"}";

        } else if (isPost && reqLine.indexOf("/press") >= 0) {
            int x  = constrain(_rmt_json_i(body,"x",400), 0, 800);
            int y  = constrain(_rmt_json_i(body,"y",240), 0, 480);
            int ms = constrain(_rmt_json_i(body,"ms",500), 50, 5000);
            _rmt_enqueue(RMT_PRESS,(int16_t)x,(int16_t)y,
                         (int16_t)x,(int16_t)y, 1,(uint32_t)ms);
            resp = "{\"ok\":true,\"action\":\"press\"}";

        } else if (isPost && reqLine.indexOf("/inject_meter") >= 0) {
            meter_state_t* s = &udpComm.state;
            s->peak_l          = _rmt_json_f(body,"peak_l",   -60);
            s->peak_r          = _rmt_json_f(body,"peak_r",   -60);
            s->rms_l           = _rmt_json_f(body,"rms_l",    -60);
            s->rms_r           = _rmt_json_f(body,"rms_r",    -60);
            s->true_peak_l     = _rmt_json_f(body,"true_peak_l",-60);
            s->true_peak_r     = _rmt_json_f(body,"true_peak_r",-60);
            s->lufs_momentary  = _rmt_json_f(body,"lufs_m",   -70);
            s->lufs_short      = _rmt_json_f(body,"lufs_s",   -70);
            s->lufs_integrated = _rmt_json_f(body,"lufs_i",   -70);
            s->lufs_range      = _rmt_json_f(body,"lufs_r",     0);
            s->phase_correlation = _rmt_json_f(body,"phase",    1);
            s->clip_count_l    = (uint16_t)_rmt_json_i(body,"clip_l",0);
            s->clip_count_r    = (uint16_t)_rmt_json_i(body,"clip_r",0);
            s->last_packet_time = millis();
            s->connected = true;
            resp = "{\"ok\":true,\"action\":\"inject_meter\"}";

        } else if (isPost && reqLine.indexOf("/cmd") >= 0) {
            String c2 = _rmt_json_s(body, "cmd"); c2.trim();
            /* Dispatch known safe commands inline */
            if (c2 == "INJECT_PLAY") {
                udpComm.state.transport_flags |=  MB_TRANSPORT_PLAYING;
                udpComm.state.transport_flags &= ~MB_TRANSPORT_STOPPED;
                resp = "{\"ok\":true,\"cmd\":\"INJECT_PLAY\"}";
            } else if (c2 == "INJECT_STOP") {
                udpComm.state.transport_flags &= ~MB_TRANSPORT_PLAYING;
                udpComm.state.transport_flags |=  MB_TRANSPORT_STOPPED;
                resp = "{\"ok\":true,\"cmd\":\"INJECT_STOP\"}";
            } else if (c2 == "PING") {
                resp = "{\"ok\":true,\"pong\":true}";
            } else if (c2 == "REBOOT") {
                _rmt_send(client, 200, ct, "{\"ok\":true,\"rebooting\":true}");
                client.stop(); delay(200); ESP.restart();
            } else {
                resp = "{\"ok\":false,\"err\":\"unknown cmd\"}";
            }
        } else {
            resp = "{\"ok\":false,\"err\":\"not found\"}";
        }

        _rmt_send(client, 200, ct, resp);
        client.stop();
    }
}


/* WiFi connect callback — called from settings screen CONNECT button. */
void wifi_connect_from_settings(lv_event_t* e) {
    (void)e;
    const char* ssid = _settings_get_selected_ssid();
    const char* ui_pass = _ta_pass ? lv_textarea_get_text(_ta_pass) : "";
    
    if (ssid && strlen(ssid) > 0) {
        Serial.printf("[WIFI] Connect button: SSID='%s'\n", ssid);
        
        String passToUse = ui_pass;
        if (passToUse.length() == 0) {
            /* Pull stored password if UI field left blank (e.g. re-connecting known network) */
            String storedPass = wifiMgr.getPass();
            if (storedPass.length() > 0) {
                passToUse = storedPass;
                Serial.println("[WIFI] UI Password blank. Pulled existing password from preferences.");
            }
        }
        
        Serial.printf("[WIFI] Final auth -> SSID: '%s', PassLength: %d\n", ssid, passToUse.length());
        wifiMgr.setCredentials(ssid, passToUse.c_str());
    } else {
        Serial.println("[WIFI] No network selected from dropdown");
    }
}

/* ─── Setup ────────────────────────────────────────────────────── */

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n==============================");
    Serial.println("  MeterBridge v2.0.0");
    Serial.println("==============================\n");

    /* ── Load NVS Preferences ── */
    {
        Preferences prefs;
        prefs.begin("meterbridge", true); /* read-only */
        uint8_t themeId    = prefs.getUChar("theme",      MB_THEME_CODEINE_CRAZY);
        uint8_t rotId      = prefs.getUChar("rotation",   0);
        uint32_t peakMs    = prefs.getULong("peak_hold",  2500);
        uint8_t connMode   = prefs.getUChar("conn_mode",  0);
        uint32_t updateMs  = prefs.getULong("update_ms",  50);

        uint8_t  brightPct = prefs.getUChar("brightness", 80);
        uint8_t  meterMode = prefs.getUChar("meter_mode", 0);
        bool     showFps   = prefs.getBool("show_fps",    true);
        uint8_t  lufsPreset = prefs.getUChar("lufs_preset", 0);
        uint8_t  specMode   = prefs.getUChar("spec_mode",   0);
        uint8_t  mHeight    = prefs.getUChar("mtr_height",  1); /* Default XL */
        bool     autoDim   = prefs.getBool("auto_dim",      false); /* Screensaver off by default */
        prefs.end();

        g_current_theme      = (themeId < MB_THEME_COUNT) ? themeId : MB_THEME_CODEINE_CRAZY;
        g_display_rotation   = (rotId < 4) ? rotId : 0;
        g_conn_mode          = (connMode < 3) ? connMode : 0;
        g_peak_hold_ms       = (peakMs == 0) ? UINT32_MAX : peakMs;
        g_update_interval_ms = (updateMs >= 20 && updateMs <= 2500) ? updateMs : 50;

        g_brightness         = (brightPct <= 100) ? brightPct : 80;
        g_meter_mode         = (meterMode < 4) ? meterMode : 0;
        g_show_fps           = showFps;
        g_loudness_preset    = (lufsPreset < 16) ? lufsPreset : 0;
        g_spec_color_mode    = (specMode < 10) ? specMode : 0;
        g_meter_height_idx   = (mHeight < 6) ? mHeight : 1;
        g_auto_dim_enabled   = autoDim;

        /* Map preset index to target dB — must mirror LOUDNESS_PRESETS[] in screen_settings.h */
        static const float LUFS_TARGETS[] = {
            -14.0f, -16.0f, -23.0f, -27.0f, -18.0f,   /* streaming / broadcast */
            -12.0f, -10.0f,  -9.0f,  -8.0f,  -6.0f,   /* hot / loud            */
             -4.0f,  -3.0f,  -2.0f,  -1.0f,  -0.1f,   /* near-peak             */
              0.0f                                       /* Off                   */
        };
        g_loudness_target = LUFS_TARGETS[g_loudness_preset];

        Serial.printf("[CFG] Theme:%d Rot:%d Peak:%u ConnMode:%d UpdateMs:%u Bright:%d%% MeterMode:%d ShowFPS:%d LUFS:%d\n",
                      themeId, rotId, peakMs, g_conn_mode, g_update_interval_ms, g_brightness, g_meter_mode, (int)g_show_fps, g_loudness_preset);

        /* Apply theme colors into globals */
        mb_theme_load(themeId);
    }

    /* PCA9557 IO Expander init (V3.0 boards) */
    bool hasPCA = pca9557_init();
    Serial.print("[HW] PCA9557: ");
    Serial.println(hasPCA ? "FOUND (V3.0)" : "NOT FOUND (V1.x/V2.x)");

    /* Initialize display */
    Serial.println("[DISP] Initializing LovyanGFX...");
    lcd.init();

    /* Apply hardware rotation from NVS BEFORE LVGL sees any dimensions */
    if (g_display_rotation > 0) {
        lcd.setRotation(g_display_rotation);
        Serial.printf("[DISP] Hardware rotation: %d (%d deg)\n",
                      g_display_rotation, g_display_rotation * 90);
    }
    /* Apply saved brightness (default 80% on first boot) */
    lcd.setBrightness((uint8_t)map(g_brightness, 0, 100, 0, 255));
    Serial.printf("[DISP] %dx%d initialized, brightness:%d%%\n", lcd.width(), lcd.height(), g_brightness);

    lcd.fillScreen(TFT_BLACK);
    delay(100);

    /* Initialize LVGL */
    Serial.println("[LVGL] Initializing...");
    lv_init();

    /* Logical screen size — may swap for portrait orientations */
    uint32_t disp_w = lcd.width();   /* LovyanGFX returns post-rotation size */
    uint32_t disp_h = lcd.height();

    /* ── Draw buffer allocation — graduated fallback ──
     * Target: 200-row PSRAM double buffer = 800*200*2*2 = 640KB of our 7MB+ free PSRAM.
     * Larger buffers = fewer flush() calls per frame = higher FPS.
     * Fall back gracefully if PSRAM is fragmented. */
    size_t buf_size = disp_w * 200;
    buf1 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
    buf2 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));

    if (!buf1 || !buf2) {
        /* 200-row PSRAM failed — try 100 rows */
        if (buf1) { free(buf1); buf1 = nullptr; }
        if (buf2) { free(buf2); buf2 = nullptr; }
        buf_size = disp_w * 100;
        buf1 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
        buf2 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
        if (!buf1 || !buf2) {
            /* 100-row failed — try 40 rows */
            if (buf1) { free(buf1); buf1 = nullptr; }
            if (buf2) { free(buf2); buf2 = nullptr; }
            buf_size = disp_w * 40;
            buf1 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
            buf2 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
            if (!buf1 || !buf2) {
                Serial.println("[WARN] PSRAM full — using 20-row internal RAM buffer");
                if (buf1) { free(buf1); buf1 = nullptr; }
                if (buf2) { free(buf2); buf2 = nullptr; }
                buf_size = disp_w * 20;
                buf1 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_INTERNAL);
                buf2 = nullptr; /* single-buffer mode */
            } else {
                Serial.println("[DISP] 40-row PSRAM draw buffers allocated (fallback)");
            }
        } else {
            Serial.println("[DISP] 100-row PSRAM draw buffers allocated (fallback)");
        }
    } else {
        Serial.println("[DISP] 200-row PSRAM draw buffers allocated (full speed)");
    }

    if (!buf1) {
        Serial.println("[ERROR] All buffer allocations failed — halting");
        while(1) { delay(1000); }
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);


    /* Register display driver with post-rotation dimensions */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = (lv_coord_t)disp_w;
    disp_drv.ver_res  = (lv_coord_t)disp_h;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register touch input */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    /* Register remote control virtual pointer indev (Core 1 read_cb) */
    static lv_indev_drv_t rmt_indev_drv;
    lv_indev_drv_init(&rmt_indev_drv);
    rmt_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    rmt_indev_drv.read_cb = lvgl_remote_cb;
    lv_indev_drv_register(&rmt_indev_drv);

    Serial.printf("[LVGL] Ready. Buffers: %s, Size: %d px\n",
                  buf2 ? "double" : "single", buf_size);

    /* Initialize meter state defaults */
    meter_state_t* st = &udpComm.state;
    st->tempo_bpm = 120.0f;
    st->time_sig_num = 4;
    st->time_sig_den = 4;
    st->measure = 1;
    st->beat_in_bar = 1;
    st->meter_source = MB_SRC_MASTER;
    st->lufs_integrated = -70.0f;

    /* Start WiFi */
    Serial.println("[WIFI] Initializing...");
    wifiMgr.begin();

    /* Build UI */
    Serial.println("[UI] Building interface...");
    ui_init(st, &udpComm);
    Serial.println("[UI] Ready.");

    /* ── OTA Firmware Updates ── */
    ArduinoOTA.setHostname("meterbridge");
    ArduinoOTA.onStart([]() {
        g_ota_in_progress = true;
        g_ota_progress = 0;
        Serial.println("[OTA] Update starting...");
    });
    ArduinoOTA.onEnd([]() {
        g_ota_in_progress = false;
        Serial.println("[OTA] Update complete! Rebooting...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        g_ota_progress = (uint8_t)(progress * 100 / total);
        if (g_ota_progress % 10 == 0)
            Serial.printf("[OTA] Progress: %u%%\n", g_ota_progress);
        /* Feed LVGL during OTA so progress overlay updates */
        lv_timer_handler();
    });
    ArduinoOTA.onError([](ota_error_t error) {
        g_ota_in_progress = false;
        Serial.printf("[OTA] Error[%u]: ", error);
        if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)     Serial.println("End Failed");
    });
    if (wifiMgr.isConnected()) {
        ArduinoOTA.begin();
        Serial.printf("[OTA] Ready. Upload via: pio run -t upload --upload-port %s\n",
                      wifiMgr.getLocalIP().toString().c_str());
    }

    Serial.printf("[MEM] Heap: %d bytes free\n", ESP.getFreeHeap());
    Serial.printf("[MEM] PSRAM: %d bytes free\n", ESP.getFreePsram());
    Serial.println("\n[OK] MeterBridge v2.0.0 ready.\n");
}

/* ─── Serial Command Interface ─────────────────────────────────
 * Commands (newline-terminated):
 *   PING               → PONG
 *   STATUS             → JSON snapshot of current state
 *   REBOOT             → ESP.restart()
 *   NVS_CLEAR          → wipe meterbridge NVS namespace
 *   SET_ROTATION:<0-3> → save + reboot with new rotation
 *   SET_THEME:<0-4>    → save + reboot with new theme
 *   SET_PEAK:<ms>      → set peak hold (0 = forever)
 *   INJECT_METER:<p_l>,<p_r>,<r_l>,<r_r>,<tp_l>,<tp_r>  → fake UDP data
 *   INJECT_PLAY        → set play state
 *   INJECT_STOP        → clear play state
 * ────────────────────────────────────────────────────────────── */

static String _serial_buf;

static void handle_serial_commands() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            _serial_buf.trim();
            if (_serial_buf.length() == 0) { _serial_buf = ""; return; }
            String cmd = _serial_buf;
            _serial_buf = "";

            Serial.printf("[CMD] %s\n", cmd.c_str());

            if (cmd == "PING") {
                Serial.println("PONG");

            } else if (cmd == "STATUS") {
                meter_state_t* s = &udpComm.state;
                Serial.printf("{\"heap\":%d,\"psram\":%d,\"wifi\":%d,"
                              "\"peak_l\":%.1f,\"peak_r\":%.1f,"
                              "\"rms_l\":%.1f,\"rms_r\":%.1f,"
                              "\"lufs_m\":%.1f,\"lufs_s\":%.1f,"
                              "\"lufs_i\":%.1f,\"lufs_r\":%.1f,"
                              "\"phase\":%.2f,\"clip_l\":%d,\"clip_r\":%d,"
                              "\"playing\":%d,"
                              "\"rotation\":%d,\"theme\":%d,\"peak_hold_ms\":%lu,"
                              "\"conn_mode\":%d,\"update_ms\":%u,"
                              "\"brightness\":%d,\"meter_mode\":%d,"
                              "\"show_fps\":%d,\"fps\":%.1f}\n",
                              (int)ESP.getFreeHeap(), (int)ESP.getFreePsram(),
                              (int)wifiMgr.isConnected(),
                              s->peak_l, s->peak_r, s->rms_l, s->rms_r,
                              s->lufs_momentary, s->lufs_short,
                              s->lufs_integrated, s->lufs_range,
                              s->phase_correlation,
                              (int)s->clip_count_l, (int)s->clip_count_r,
                              (int)(s->transport_flags & MB_TRANSPORT_PLAYING),
                              (int)g_display_rotation, (int)g_current_theme,
                              (unsigned long)g_peak_hold_ms,
                              (int)g_conn_mode, (unsigned)g_update_interval_ms,
                              (int)g_brightness, (int)g_meter_mode,
                              (int)g_show_fps, g_fps_display);

            } else if (cmd == "REBOOT") {
                Serial.println("REBOOTING");
                delay(100);
                ESP.restart();

            } else if (cmd == "NVS_CLEAR") {
                Preferences prefs;
                prefs.begin("meterbridge", false);
                prefs.clear();
                prefs.end();
                Serial.println("NVS_CLEARED");
                delay(100);
                ESP.restart();

            } else if (cmd.startsWith("SET_ROTATION:")) {
                uint8_t r = (uint8_t)cmd.substring(13).toInt();
                if (r < 4) {
                    Preferences prefs;
                    prefs.begin("meterbridge", false);
                    prefs.putUChar("rotation", r);
                    prefs.end();
                    Serial.printf("ROTATION_SET:%d\n", r);
                    delay(100);
                    ESP.restart();
                } else {
                    Serial.println("ERR:rotation 0-3");
                }

            } else if (cmd.startsWith("SET_THEME:")) {
                uint8_t t = (uint8_t)cmd.substring(10).toInt();
                if (t < MB_THEME_COUNT) {
                    Preferences prefs;
                    prefs.begin("meterbridge", false);
                    prefs.putUChar("theme", t);
                    prefs.end();
                    Serial.printf("THEME_SET:%d\n", t);
                    delay(100);
                    ESP.restart();
                } else {
                    Serial.printf("ERR:theme 0-%d\n", MB_THEME_COUNT - 1);
                }

            } else if (cmd.startsWith("SET_PEAK:")) {
                uint32_t ms = (uint32_t)cmd.substring(9).toInt();
                g_peak_hold_ms = (ms == 0) ? UINT32_MAX : ms;
                Preferences prefs;
                prefs.begin("meterbridge", false);
                prefs.putULong("peak_hold", ms);
                prefs.end();
                Serial.printf("PEAK_SET:%lu\n", (unsigned long)ms);

            } else if (cmd.startsWith("INJECT_METER:")) {
                /* INJECT_METER:-12.5,-11.0,-18.0,-17.5,-14.0,-13.5 */
                String vals = cmd.substring(13);
                float v[6] = {-60,-60,-60,-60,-60,-60};
                int idx = 0;
                while (vals.length() && idx < 6) {
                    int comma = vals.indexOf(',');
                    String tok = (comma >= 0) ? vals.substring(0, comma) : vals;
                    v[idx++] = tok.toFloat();
                    if (comma < 0) break;
                    vals = vals.substring(comma + 1);
                }
                meter_state_t* s = &udpComm.state;
                s->peak_l = v[0]; s->peak_r = v[1];
                s->rms_l  = v[2]; s->rms_r  = v[3];
                s->true_peak_l = v[4]; s->true_peak_r = v[5];
                s->last_packet_time = millis();
                Serial.printf("INJECTED:%.1f,%.1f\n", v[0], v[1]);

            } else if (cmd == "INJECT_PLAY") {
                udpComm.state.transport_flags |= MB_TRANSPORT_PLAYING;
                udpComm.state.transport_flags &= ~MB_TRANSPORT_STOPPED;
                Serial.println("PLAY_SET");

            } else if (cmd == "INJECT_STOP") {
                udpComm.state.transport_flags &= ~MB_TRANSPORT_PLAYING;
                udpComm.state.transport_flags |= MB_TRANSPORT_STOPPED;
                Serial.println("STOP_SET");

            } else if (cmd.startsWith("SMETER:")) {
                /* SMETER:<pk_l>,<pk_r>,<rms_l>,<rms_r>,<tp_l>,<tp_r>,<lm>,<ls>,<li>,<lr>,<ph>,<cl>,<cr>
                 * Real-time meter data injected by the relay over USB serial (no WiFi needed) */
                String vals = cmd.substring(7);
                float v[13] = {-60,-60,-60,-60,-60,-60,-70,-70,-70,0,1,0,0};
                int idx = 0;
                while (vals.length() && idx < 13) {
                    int comma = vals.indexOf(',');
                    String tok = (comma >= 0) ? vals.substring(0, comma) : vals;
                    v[idx++] = tok.toFloat();
                    if (comma < 0) break;
                    vals = vals.substring(comma + 1);
                }
                meter_state_t* s = &udpComm.state;
                s->peak_l         = v[0];  s->peak_r         = v[1];
                s->rms_l          = v[2];  s->rms_r          = v[3];
                s->true_peak_l    = v[4];  s->true_peak_r    = v[5];
                s->lufs_momentary = v[6];  s->lufs_short     = v[7];
                s->lufs_integrated= v[8];  s->lufs_range     = v[9];
                s->phase_correlation = v[10];
                s->clip_count_l   = (uint16_t)v[11];
                s->clip_count_r   = (uint16_t)v[12];
                s->last_packet_time = millis();
                s->connected = true;
                g_last_serial_data_ms = millis();  /* track serial data arrival */
                Serial.println("SM:OK");

            } else if (cmd.startsWith("STRANSPORT:")) {
                /* STRANSPORT:<flags>,<tempo>,<ts_num>,<ts_den>,<pos_secs>,<measure>,<beat> */
                String vals = cmd.substring(11);
                float v[7] = {0,120,4,4,0,1,1};
                int idx = 0;
                while (vals.length() && idx < 7) {
                    int comma = vals.indexOf(',');
                    String tok = (comma >= 0) ? vals.substring(0, comma) : vals;
                    v[idx++] = tok.toFloat();
                    if (comma < 0) break;
                    vals = vals.substring(comma + 1);
                }
                meter_state_t* s = &udpComm.state;
                s->transport_flags = (uint8_t)v[0];
                s->tempo_bpm       = v[1];
                s->time_sig_num    = (uint8_t)v[2];
                s->time_sig_den    = (uint8_t)v[3];
                s->position_secs   = v[4];
                s->measure         = (uint16_t)v[5];
                s->beat_in_bar     = (uint8_t)v[6];
                Serial.println("ST:OK");

            } else if (cmd.startsWith("STRACK:")) {
                /* STRACK:<idx>,<r>,<g>,<b>,<name>,<muted>,<soloed>,<armed> */
                String vals = cmd.substring(7);
                int parts[4] = {0, 180, 180, 200};
                int pi = 0;
                String rest = vals;
                for (int fi = 0; fi < 4 && rest.length(); fi++) {
                    int comma = rest.indexOf(',');
                    if (comma < 0) break;
                    parts[pi++] = rest.substring(0, comma).toInt();
                    rest = rest.substring(comma + 1);
                }
                // rest now starts with track name (may contain commas), find the last 3 commas for flags
                int last3[3] = {0,0,0};
                int ci = rest.lastIndexOf(',');
                if (ci >= 0) { last3[2] = rest.substring(ci+1).toInt(); rest = rest.substring(0, ci); }
                ci = rest.lastIndexOf(',');
                if (ci >= 0) { last3[1] = rest.substring(ci+1).toInt(); rest = rest.substring(0, ci); }
                ci = rest.lastIndexOf(',');
                if (ci >= 0) { last3[0] = rest.substring(ci+1).toInt(); rest = rest.substring(0, ci); }
                meter_state_t* s = &udpComm.state;
                s->track_index   = (uint8_t)parts[0];
                s->track_color_r = (uint8_t)parts[1];
                s->track_color_g = (uint8_t)parts[2];
                s->track_color_b = (uint8_t)parts[3];
                strncpy(s->track_name, rest.c_str(), MB_MAX_TRACK_NAME_LEN - 1);
                s->track_name[MB_MAX_TRACK_NAME_LEN - 1] = '\0';
                s->track_muted  = (uint8_t)last3[0];
                s->track_soloed = (uint8_t)last3[1];
                s->track_armed  = (uint8_t)last3[2];
                Serial.println("STRK:OK");

            } else if (cmd.startsWith("SPROJECT:")) {
                /* SPROJECT:<project_name>|<section_name> */
                String rest = cmd.substring(9);
                int sep = rest.indexOf('|');
                meter_state_t* s = &udpComm.state;
                if (sep >= 0) {
                    strncpy(s->project_name, rest.substring(0, sep).c_str(), MB_MAX_PROJECT_NAME_LEN - 1);
                    s->project_name[MB_MAX_PROJECT_NAME_LEN - 1] = '\0';
                    strncpy(s->section_name, rest.substring(sep + 1).c_str(), MB_MAX_SECTION_NAME_LEN - 1);
                    s->section_name[MB_MAX_SECTION_NAME_LEN - 1] = '\0';
                } else {
                    strncpy(s->project_name, rest.c_str(), MB_MAX_PROJECT_NAME_LEN - 1);
                    s->project_name[MB_MAX_PROJECT_NAME_LEN - 1] = '\0';
                    s->section_name[0] = '\0';
                }
                Serial.println("SPRJ:OK");

            } else if (cmd.startsWith("SET_BRIGHTNESS:")) {
                /* SET_BRIGHTNESS:<0-100>  Display brightness %, live + NVS */
                uint8_t pct = (uint8_t)constrain(cmd.substring(15).toInt(), 0, 100);
                g_brightness = pct;
                lcd.setBrightness((uint8_t)map(pct, 0, 100, 0, 255));
                Preferences prefs;
                prefs.begin("meterbridge", false);
                prefs.putUChar("brightness", pct);
                prefs.end();
                Serial.printf("BRIGHTNESS_SET:%d\n", (int)pct);

            } else if (cmd.startsWith("SET_METER_MODE:")) {
                /* SET_METER_MODE:<0-3>  0=Classic 1=LR 2=MS 3=LUFS, live + NVS */
                uint8_t mode = (uint8_t)constrain(cmd.substring(15).toInt(), 0, 3);
                g_meter_mode = mode;
                Preferences prefs;
                prefs.begin("meterbridge", false);
                prefs.putUChar("meter_mode", mode);
                prefs.end();
                Serial.printf("METER_MODE_SET:%d\n", (int)mode);

            } else if (cmd.startsWith("SET_UPDATE_MS:")) {
                /* SET_UPDATE_MS:<20-2500>  Relay/Lua tick interval, live + persisted.
                 * Min 20ms = 50 updates/sec max; Min was 40ms before this change.     */
                uint32_t ms = (uint32_t)cmd.substring(14).toInt();
                ms = max(20UL, min(2500UL, (unsigned long)ms));
                g_update_interval_ms = ms;
                Preferences prefs;
                prefs.begin("meterbridge", false);
                prefs.putULong("update_ms", ms);
                prefs.end();
                /* Echo in relay_config.txt format so the PC relay picks it up
                   (relay monitors serial lines starting with "UPDATE_MS_CFG:") */
                Serial.printf("UPDATE_MS_SET:%u\n", (unsigned)ms);
                Serial.printf("[CFG] UpdateMs -> %u ms (%.1f/s)\n",
                              (unsigned)ms, 1000.0f / (float)ms);

            } else if (cmd.startsWith("SET_CONN_MODE:")) {
                /* SET_CONN_MODE:<0-2>  0=UDP, 1=Serial, 2=Both. Applied live + persisted */
                uint8_t m = (uint8_t)cmd.substring(14).toInt();
                if (m < 3) {
                    g_conn_mode = m;
                    Preferences prefs;
                    prefs.begin("meterbridge", false);
                    prefs.putUChar("conn_mode", m);
                    prefs.end();
                    static const char* CMODE_NAMES[] = {"UDP", "Serial", "Both"};
                    Serial.printf("CONN_MODE_SET:%d (%s)\n", m, CMODE_NAMES[m]);
                } else {
                    Serial.println("ERR:conn_mode 0-2");
                }

            } else {
                Serial.printf("ERR:unknown cmd '%s'\n", cmd.c_str());
            }
        } else {
            if (_serial_buf.length() < 1024) {
                _serial_buf += c;
            } else {
                /* Buffer overflow protection */
                _serial_buf = "";
            }
        }
    }
}

/* ─── Main Loop ────────────────────────────────────────────────── */

static uint32_t lastUdpCheck = 0;
static uint32_t lastUiUpdate = 0;

/* ── FPS / perf telemetry (low-overhead) ── */
static uint32_t _perf_frames    = 0;
static uint32_t _perf_worst_ms  = 0;
static uint32_t _perf_report_t  = 0;
static const uint32_t PERF_REPORT_INTERVAL_MS = 5000;

/* ── 1-second FPS counter for UI overlay (ultra-low cost) ── */
static uint32_t _fps1s_frames = 0;
static uint32_t _fps1s_t0     = 0;

void loop() {
    uint32_t _t0 = millis();   /* frame start for worst-frame tracking */

    /* ── OTA handler (must run every loop iteration) ── */
    ArduinoOTA.handle();
    if (g_ota_in_progress) {
        /* During OTA, only update LVGL for progress overlay — skip everything else */
        lv_timer_handler();
        ui_update(&udpComm.state); /* renders OTA overlay */
        return;
    }

    lv_timer_handler();
    handle_serial_commands();

    uint32_t now = millis();

    /* ── 1-second FPS overlay counter ── */
    _fps1s_frames++;
    if (_fps1s_t0 == 0) _fps1s_t0 = now;
    if (now - _fps1s_t0 >= 1000) {
        g_fps_display = _fps1s_frames * 1000.0f / (float)(now - _fps1s_t0);
        _fps1s_frames = 0;
        _fps1s_t0 = now;
    }

    if (now - lastUdpCheck >= 500) {
        lastUdpCheck = now;

        wifiMgr.update();

        static bool udpStarted = false;
        static bool otaStarted = false;
        if (wifiMgr.isConnected() && !udpStarted) {
            udpComm.begin();
            udpStarted = true;
        } else if (!wifiMgr.isConnected() && udpStarted) {
            /* B2 fix: close the AsyncUDP socket so the next begin() binds cleanly */
            udpComm.stop();
            udpStarted = false;
        }
        /* Start OTA once WiFi connects (if it wasn't started in setup) */
        if (wifiMgr.isConnected() && !otaStarted) {
            ArduinoOTA.begin();
            otaStarted = true;
            Serial.printf("[OTA] Started. IP: %s\n", wifiMgr.getLocalIP().toString().c_str());
        } else if (!wifiMgr.isConnected() && otaStarted) {
            otaStarted = false;
        }
        /* Screenshot server: start Core 0 task once; it waits for WiFi internally */
        static bool _snapTaskStarted = false;
        if (wifiMgr.isConnected() && !_snapTaskStarted) {
            _snapTaskStarted = true;
            xTaskCreatePinnedToCore(_screenshot_task, "screenshot", 8192,
                                    nullptr, 1, nullptr, 0 /* Core 0 */);
        }
        /* Remote control HTTP server — Core 0, port 8080 */
        static bool _rmtTaskStarted = false;
        if (wifiMgr.isConnected() && !_rmtTaskStarted) {
            _rmtTaskStarted = true;
            xTaskCreatePinnedToCore(_remote_ctrl_task, "rmt_ctrl", 10240,
                                    nullptr, 1, nullptr, 0 /* Core 0 */);
            Serial.printf("[RCTRL] Task started. Panel: http://%s:8080\n",
                          wifiMgr.getLocalIP().toString().c_str());
        }

        if (udpStarted && udpComm.state.last_packet_time > 0) {
            if (now - udpComm.state.last_packet_time > 3000) {
                /* UDP timed out */
            }
        }

        if (udpStarted) udpComm.update();

        /* ── Connection indicator: mode-aware ── */
        bool wifi_live = udpStarted && wifiMgr.isConnected() &&
                         (udpComm.state.last_packet_time == 0 ||
                          now - udpComm.state.last_packet_time <= 3000);
        bool serial_live = (g_last_serial_data_ms > 0) &&
                           (now - g_last_serial_data_ms <= 3000);
        switch (g_conn_mode) {
            case 0:  /* UDP only */
                udpComm.state.connected = wifi_live; break;
            case 1:  /* Serial only */
                udpComm.state.connected = serial_live; break;
            case 2:  /* Both */
                udpComm.state.connected = wifi_live || serial_live; break;
            default:
                udpComm.state.connected = wifi_live; break;
        }
    }

    /* ── Auto-dim screensaver (opt-in, default OFF) ──────────────────────
     * Triggers when no meter data arrives for >5 min. Dims to 15% at 5 min,
     * blacks at 10 min. Any touch or incoming data restores full brightness.
     * BUG-07 fix: restore brightness when disabled while dimmed.
     * BUG-10 fix: seed idle clock on first enable to prevent instant dim.  */
    {
        static uint8_t  _dim_state        = 0;     /* 0=normal, 1=dim, 2=black */
        static uint32_t _last_sig_ms      = 0;
        static bool     _prev_dim_enabled = false; /* tracks enable/disable edge */

        if (g_auto_dim_enabled) {
            /* Seed idle clock whenever auto-dim is freshly turned on so we
             * don't dim immediately if the board has been running >5 min. */
            if (!_prev_dim_enabled) {
                _last_sig_ms = millis();
                _dim_state   = 0;
            }
            _prev_dim_enabled = true;

            /* Update last-signal timestamp whenever data arrives */
            uint32_t newest_pkt = udpComm.state.last_packet_time;
            if (newest_pkt > _last_sig_ms || g_last_serial_data_ms > _last_sig_ms) {
                _last_sig_ms = max(newest_pkt, g_last_serial_data_ms);
            }
            /* Absolute first-run seed (boot, _last_sig_ms still 0) */
            if (_last_sig_ms == 0) _last_sig_ms = millis();

            /* Wake on touch (remote-control path stamps g_last_touch_ms) */
            if (g_last_touch_ms > _last_sig_ms) {
                _last_sig_ms = g_last_touch_ms;
            }

            uint32_t idle_ms = millis() - _last_sig_ms;
            uint8_t want_dim = (idle_ms > 600000UL) ? 2 :   /* 10 min → black */
                               (idle_ms > 300000UL) ? 1 : 0; /*  5 min → dim  */
            if (want_dim != _dim_state) {
                _dim_state = want_dim;
                uint8_t hw_bright = (want_dim == 2) ? 0 :
                                    (want_dim == 1) ? (uint8_t)map(15, 0, 100, 0, 255) :
                                                      (uint8_t)map(g_brightness, 0, 100, 0, 255);
                lcd.setBrightness(hw_bright);
            }
        } else {
            /* Restore brightness if screen was dimmed when user disabled auto-dim */
            if (_prev_dim_enabled && _dim_state != 0) {
                _dim_state = 0;
                lcd.setBrightness((uint8_t)map(g_brightness, 0, 100, 0, 255));
            }
            _prev_dim_enabled = false;
        }
    }

    /* ── UI update — call every loop; LVGL internal timers self-throttle.
     * Removing the 33ms hard gate lets LVGL push frames as fast as the
     * display can consume them (target: ~60 FPS at panel refresh rate). */
    ui_update(&udpComm.state);

    /* Screenshot: Core 0 task raises flag → we take snapshot here on Core 1
     * (lv_snapshot_take must run in the LVGL task), then signal back.      */
    if (_snap_take_req) {
        _snap_ptr      = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
        _snap_take_req = false;
        _snap_ready    = true;   /* Core 0 task can now send */
    }

    /* ── Perf telemetry ── */
    uint32_t _frame_ms = millis() - _t0;
    _perf_frames++;
    if (_frame_ms > _perf_worst_ms) _perf_worst_ms = _frame_ms;
    if (now - _perf_report_t >= PERF_REPORT_INTERVAL_MS) {
        float fps = _perf_frames * 1000.0f / PERF_REPORT_INTERVAL_MS;
        Serial.printf("PERF:{\"fps\":%.1f,\"worst_ms\":%u,\"heap\":%d}\n",
                      fps, _perf_worst_ms, (int)ESP.getFreeHeap());
        _perf_frames   = 0;
        _perf_worst_ms = 0;
        _perf_report_t = now;
    }

    taskYIELD();   /* let FreeRTOS tick without burning a min-1ms delay */
}
