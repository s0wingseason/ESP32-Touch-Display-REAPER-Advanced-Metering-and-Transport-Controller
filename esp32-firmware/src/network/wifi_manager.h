/*
 * MeterBridge — WiFi Manager
 * 
 * Handles WiFi connectivity with AP-mode fallback for initial configuration.
 * On first boot (or when no credentials are stored), broadcasts an AP network
 * for the user to connect and provide their studio WiFi credentials.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <vector>

/* ─── Configuration ──────────────────────────────────────────── */

#define WIFI_AP_SSID       "MeterBridge-Setup"
#define WIFI_AP_PASS       "meterbridge"
#define WIFI_CONNECT_TIMEOUT_MS    15000   /* 15s per connection attempt       */
#define WIFI_RECONNECT_BASE_MS     10000   /* Initial backoff: 10s             */
#define WIFI_RECONNECT_MAX_MS     300000   /* Max backoff cap: 5 minutes       */
#define WIFI_AP_FALLBACK_RETRIES     10   /* Retries before AP fallback mode  */

/* ─── WiFi State ─────────────────────────────────────────────── */

typedef enum {
    WIFI_STATE_IDLE,
    WIFI_STATE_AP_MODE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_SCANNING_FOR_RECONNECT,
    WIFI_STATE_ERROR
} wifi_state_t;

/* ─── WiFi Manager Class ─────────────────────────────────────── */

class WiFiManager {
public:
    WiFiManager() : _state(WIFI_STATE_IDLE), _lastReconnectAttempt(0), _retryCount(0), _reconnectBackoffMs(WIFI_RECONNECT_BASE_MS) {}

    /**
     * Initialize WiFi. Attempts to connect with stored credentials.
     * Falls back to AP mode if no credentials or connection fails.
     */
     void begin() {
        Serial.println("[WiFi] Initializing...");
        
        /* Standard STA mode configuration */
        WiFi.persistent(false);
        WiFi.setAutoConnect(false);
        WiFi.setAutoReconnect(false);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        
        delay(500);
        Serial.printf("[WiFi] Mode: %d  Status: %d\n", (int)WiFi.getMode(), (int)WiFi.status());
        
        _prefs.begin("wifi_mgr", false);
        _state = WIFI_STATE_IDLE;
        _retryCount = 0;
        _lastReconnectAttempt = 0;
        _lastDisconnectReason = 0;

        /* Register Wi-Fi events */
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            switch(event) {
                case ARDUINO_EVENT_WIFI_STA_START:
                    Serial.println("[WiFi] EVT: STA Started");
                    break;
                case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                    Serial.println("[WiFi] EVT: STA Connected (L2 handshake OK!)");
                    break;
                case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                    _localIP = info.got_ip.ip_info.ip.addr;
                    _state = WIFI_STATE_CONNECTED;
                    Serial.printf("[WiFi] EVT: Got IP: %s\n", _localIP.toString().c_str());
                    break;
                case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                    _lastDisconnectReason = info.wifi_sta_disconnected.reason;
                    Serial.printf("[WiFi] EVT: STA Disconnected. Reason: %d\n", _lastDisconnectReason);
                    this->_lastReconnectAttempt = millis();
                    break;
                case ARDUINO_EVENT_WIFI_SCAN_DONE:
                    Serial.printf("[WiFi] EVT: Scan Done. Results: %d\n", info.wifi_scan_done.number);
                    break;
                default:
                    Serial.printf("[WiFi] EVT: %d\n", event);
                    break;
            }
        });

        String ssid = _prefs.getString("wifi_ssid", "");
        String pass = _prefs.getString("wifi_pass", "");
        
        if (ssid.length() == 0) {
            /* No credentials stored — start AP mode so user can configure */
            Serial.println("[WiFi] No credentials stored. Starting AP setup mode.");
            startAPMode();
            return;
        }
        
        /* Connect with stored credentials */
        Serial.printf("[WiFi] Connecting to '%s'...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        /* Wait up to 15s for connection */
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.printf(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            _state = WIFI_STATE_CONNECTED;
            _localIP = WiFi.localIP();
            Serial.printf("[WiFi] Connected! IP: %s\n", _localIP.toString().c_str());
        } else {
            Serial.printf("[WiFi] Connection failed. Status: %d\n", (int)WiFi.status());
            _state = WIFI_STATE_DISCONNECTED;
            _connectStartTime = millis();
        }
    }

    /**
     * Call in loop() to handle reconnection and state management.
     */
    void update() {
        if (_state == WIFI_STATE_CONNECTING) {
            static uint32_t lastStatusLog = 0;
            if (millis() - lastStatusLog > 2000) {
                lastStatusLog = millis();
                Serial.printf("[WiFi] Status check: %d\n", (int)WiFi.status());
            }
            if (WiFi.status() == WL_CONNECTED) {
                _state = WIFI_STATE_CONNECTED;
                _retryCount = 0;
                _reconnectBackoffMs = WIFI_RECONNECT_BASE_MS;  /* reset backoff on success */
                _localIP = WiFi.localIP();
                Serial.printf("[WiFi] Connected! IP: %s\n", _localIP.toString().c_str());
            } else if (millis() - _connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
                /* Timeout: transition to disconnected mode for a retry */
                _state = WIFI_STATE_DISCONNECTED;
                _lastReconnectAttempt = millis();
                Serial.println("[WiFi] Connection attempt timed out.");
            }
        }
        else if (_state == WIFI_STATE_CONNECTED) {
            if (WiFi.status() != WL_CONNECTED) {
                _state = WIFI_STATE_DISCONNECTED;
                Serial.println("[WiFi] Connection lost.");
                _lastReconnectAttempt = millis();
            }
        }
        else if (_state == WIFI_STATE_DISCONNECTED) {
            /* Exponential backoff reconnect — unlimited retries in STA-only studio mode.
             * Backoff doubles each attempt: 10s → 20s → 40s … capped at 5 minutes.
             * Only falls back to AP mode after WIFI_AP_FALLBACK_RETRIES consecutive failures. */
            if (millis() - _lastReconnectAttempt > _reconnectBackoffMs) {
                _retryCount++;
                if (_retryCount > WIFI_AP_FALLBACK_RETRIES) {
                    /* Too many failures — enter AP mode so user can reconfigure */
                    Serial.printf("[WiFi] %d consecutive failures — AP fallback.\n", _retryCount);
                    startAPMode();
                } else {
                    Serial.printf("[WiFi] Reconnect attempt %d (backoff %lu ms)...\n",
                                  _retryCount, _reconnectBackoffMs);
                    if (_retryCount >= 2) {
                        Serial.println("[WiFi] Strategy: Async Scan + BSSID/Channel Lock");
                        WiFi.scanNetworks(true, true); // true = async
                        _state = WIFI_STATE_SCANNING_FOR_RECONNECT;
                    } else {
                        String ssid = _prefs.getString("wifi_ssid", "");
                        String pass = _prefs.getString("wifi_pass", "");
                        connectSTA(ssid.c_str(), pass.c_str());
                    }
                    /* Double backoff, cap at max */
                    _reconnectBackoffMs = min(
                        (uint32_t)(_reconnectBackoffMs * 2u),
                        (uint32_t)WIFI_RECONNECT_MAX_MS);
                }
                _lastReconnectAttempt = millis();
            }
        }
        else if (_state == WIFI_STATE_SCANNING_FOR_RECONNECT) {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                Serial.printf("[WiFi] Async Scan Done. Results: %d\n", n);
                String ssid = _prefs.getString("wifi_ssid", "");
                String pass = _prefs.getString("wifi_pass", "");
                
                int foundIdx = -1;
                for (int i = 0; i < n; ++i) {
                    if (WiFi.SSID(i) == ssid) {
                        foundIdx = i;
                        break;
                    }
                }
                
                /* Initiate the connection with or without BSSID lock based on scan results */
                _wifi_connect_internal(ssid.c_str(), pass.c_str(), foundIdx);
                WiFi.scanDelete();
                _state = WIFI_STATE_CONNECTING;
                _connectStartTime = millis();
            } else if (n == WIFI_SCAN_FAILED) {
                Serial.println("[WiFi] Async scan failed. Falling back to direct connect.");
                String ssid = _prefs.getString("wifi_ssid", "");
                String pass = _prefs.getString("wifi_pass", "");
                connectSTA(ssid.c_str(), pass.c_str());
            }
        }
    }

    /**
     * Save new WiFi credentials and attempt connection.
     */
    void setCredentials(const char* ssid, const char* password) {
        _prefs.putString("wifi_ssid", ssid);
        _prefs.putString("wifi_pass", password);
        Serial.printf("[WiFi] Credentials saved. Connecting to: %s\n", ssid);
        
        _retryCount = 0;
        /* Stop AP mode if running */
        WiFi.softAPdisconnect(true);
        connectSTA(ssid, password);
    }

    /**
     * Clear stored credentials and restart in AP mode.
     */
    void resetCredentials() {
        _prefs.remove("wifi_ssid");
        _prefs.remove("wifi_pass");
        _retryCount = 0;
        startAPMode();
    }

    /* ── Getters ── */
    wifi_state_t getState() const { return _state; }
    bool isConnected() const { return _state == WIFI_STATE_CONNECTED; }
    bool isAPMode() const { return _state == WIFI_STATE_AP_MODE; }
    IPAddress getLocalIP() const { return _localIP; }
    String getSSID() const { return _prefs.getString("wifi_ssid", ""); }
    String getPass() const { return _prefs.getString("wifi_pass", ""); }
    int8_t getRSSI() const { return WiFi.RSSI(); }

    /**
     * Get a human-readable status string.
     */
    bool isConnecting() const { return _state == WIFI_STATE_CONNECTING; }
    bool isScanning() const   { return _isScanning; }

    int16_t scanNetworks(bool force = false) {
        if (!force && (_state == WIFI_STATE_CONNECTING || _isScanning)) {
            return -2; // Busy
        }
        _isScanning = true;
        Serial.println("[WiFi] Starting async scan...");
        return WiFi.scanNetworks(true, true); // Async
    }

    String getStatusText() const {
        char buf[64];
        const char* st;
        switch(_state) {
            case WIFI_STATE_IDLE: st = "IDLE"; break;
            case WIFI_STATE_AP_MODE: st = "AP MODE"; break;
            case WIFI_STATE_CONNECTING: st = "CONNECTING..."; break;
            case WIFI_STATE_CONNECTED: st = "CONNECTED"; break;
            case WIFI_STATE_DISCONNECTED: st = "RETRYING..."; break;
            default: st = "UNKNOWN"; break;
        }
        
        if (_state == WIFI_STATE_CONNECTED) {
            snprintf(buf, sizeof(buf), "v5.0 | %s | %s", st, _localIP.toString().c_str());
        } else if (_state == WIFI_STATE_DISCONNECTED && _lastDisconnectReason != 0) {
            snprintf(buf, sizeof(buf), "v5.0 | %s (S%d ERR %d)", st, (int)WiFi.status(), _lastDisconnectReason);
        } else {
            snprintf(buf, sizeof(buf), "v5.0 | %s (S%d)", st, (int)WiFi.status());
        }
        return String(buf);
    }

    void connectSTA(const char* ssid, const char* password) {
        _wifi_connect_internal(ssid, password, -1);
        _state = WIFI_STATE_CONNECTING;
        _connectStartTime = millis();
    }

    void _wifi_connect_internal(const char* ssid, const char* password, int scan_found_idx) {
        _ssid = String(ssid);
        _ssid.trim();
        _pass = String(password);
        _pass.trim();
        
        Serial.printf("[WiFi] connectSTA/internal: SSID='%s' passLen=%d BSSID_lock=%d\n", 
                      _ssid.c_str(), _pass.length(), (scan_found_idx != -1));
        
        // Gentle disconnect - keep STA mode alive
        WiFi.disconnect(false);
        delay(200);
        
        // Confirm STA is still alive
        wifi_mode_t currentMode = WiFi.getMode();
        
        if (currentMode != WIFI_STA) {
            Serial.println("[WiFi] WARNING: STA mode lost! Re-initializing...");
            WiFi.mode(WIFI_STA);
            delay(500);
        }
        
        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config_t));
        
        strncpy((char*)wifi_config.sta.ssid, _ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, _pass.c_str(), sizeof(wifi_config.sta.password) - 1);
        
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.sta.pmf_cfg.capable = false;
        wifi_config.sta.pmf_cfg.required = false;
        
        if (scan_found_idx != -1) {
            wifi_config.sta.channel = WiFi.channel(scan_found_idx);
            memcpy(wifi_config.sta.bssid, WiFi.BSSID(scan_found_idx), 6);
            wifi_config.sta.bssid_set = true;
            Serial.printf("[WiFi] BSSID Lock Applied: %s CH=%d\n", 
                          WiFi.BSSIDstr(scan_found_idx).c_str(), WiFi.channel(scan_found_idx));
        } else {
            Serial.println("[WiFi] Direct IDF Connect (WPA2 forced, PMF off)");
        }
        
        esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        err = esp_wifi_connect();
    }

    void startAPMode() {
        /* Use AP_STA mode so we can still scan for networks as a client while hosting the AP */
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
        _state = WIFI_STATE_AP_MODE;
        _localIP = WiFi.softAPIP();
        _retryCount = 0;
        Serial.printf("[WiFi] AP Mode active (AP+STA). SSID: %s  IP: %s\n",
                      WIFI_AP_SSID, _localIP.toString().c_str());
    }

    mutable Preferences _prefs;
    wifi_state_t _state;
    IPAddress _localIP;
    String _ssid;
    String _pass;
    bool _isScanning = false;
    uint32_t _connectStartTime = 0;
    uint32_t _lastReconnectAttempt = 0;
    uint8_t  _retryCount = 0;
    uint8_t  _lastDisconnectReason = 0;
    uint32_t _reconnectBackoffMs = WIFI_RECONNECT_BASE_MS; /* Current backoff interval */
};

#endif /* WIFI_MANAGER_H */
