/*
 * MeterBridge — UDP Communication
 * 
 * Handles receiving meter data from the JUCE VST plugin and
 * sending transport commands back. Uses AsyncUDP for non-blocking
 * performance on Core 0.
 */

#ifndef UDP_COMM_H
#define UDP_COMM_H

#include <Arduino.h>
#include <AsyncUDP.h>
#include "meterbridge_protocol.h"

/* Connection mode — defined in main.cpp. 0=UDP, 1=Serial, 2=Both */
extern uint8_t g_conn_mode;

/* ATK-18 fix: spinlock defined in main.cpp, used here to protect
 * multi-byte string copies written from Core 0 (AsyncUDP callback)
 * and read from Core 1 (LVGL/loop). */
extern portMUX_TYPE stateMux;

/* ─── Callback Types ─────────────────────────────────────────── */

typedef void (*meter_data_cb_t)(const mb_meter_packet_t* data);
typedef void (*transport_data_cb_t)(const mb_transport_packet_t* data);
typedef void (*track_info_cb_t)(const mb_track_info_packet_t* data);

/* ─── Shared Meter State (thread-safe via volatile) ──────────── */

typedef struct {
    /* Meter values (updated from network core) */
    volatile float peak_l;
    volatile float peak_r;
    volatile float true_peak_l;
    volatile float true_peak_r;
    volatile float rms_l;
    volatile float rms_r;
    volatile float lufs_momentary;
    volatile float lufs_short;
    volatile float lufs_integrated;
    volatile float lufs_range;
    volatile float phase_correlation;
    volatile uint16_t clip_count_l;
    volatile uint16_t clip_count_r;
    
    /* Transport state */
    volatile uint8_t transport_flags;
    volatile uint8_t meter_source;
    volatile uint8_t time_sig_num;
    volatile uint8_t time_sig_den;
    volatile float position_beats;
    volatile float position_secs;
    volatile float tempo_bpm;
    volatile uint16_t measure;       /* Current bar number (1-based) */
    volatile uint16_t beat_in_bar;   /* Beat within current bar */
    
    /* Track info */
    volatile uint8_t track_index;
    volatile uint8_t track_color_r;
    volatile uint8_t track_color_g;
    volatile uint8_t track_color_b;
    char track_name[MB_MAX_TRACK_NAME_LEN];
    volatile bool track_muted;
    volatile bool track_soloed;
    volatile bool track_armed;
    
    /* Project info */
    char project_name[MB_MAX_PROJECT_NAME_LEN];
    char section_name[MB_MAX_SECTION_NAME_LEN];

    /* Spectrum (16-band FFT) */
    volatile float spectrum_bands[MB_SPECTRUM_BANDS];
    volatile uint32_t spectrum_last_time;

    /* Master volume (from REAPER) */
    volatile float master_volume_db;

    /* Connection health */
    volatile uint32_t last_packet_time;
    volatile uint16_t last_sequence;
    volatile uint32_t packets_received;
    volatile uint32_t packets_dropped;
    volatile bool connected;
} meter_state_t;

/* ─── UDP Communication Class ─────────────────────────────────── */

class UDPComm {
public:
    UDPComm() : _port(MB_DEFAULT_PORT), _pluginPort(MB_DEFAULT_PORT), _seqOut(0) {
        memset(&state, 0, sizeof(state));
        state.lufs_momentary = -70.0f;
        state.lufs_short = -70.0f;
        state.lufs_integrated = -70.0f;
        state.peak_l = -70.0f;
        state.peak_r = -70.0f;
        state.rms_l = -70.0f;
        state.rms_r = -70.0f;
        state.true_peak_l = -70.0f;
        state.true_peak_r = -70.0f;
        state.phase_correlation = 1.0f;
        state.tempo_bpm = 120.0f;
        state.time_sig_num = 4;
        state.time_sig_den = 4;
        state.measure = 1;
        state.beat_in_bar = 1;
        memset(state.project_name, 0, MB_MAX_PROJECT_NAME_LEN);
        strncpy(state.project_name, "(No Project)", MB_MAX_PROJECT_NAME_LEN - 1);
        memset(state.section_name, 0, MB_MAX_SECTION_NAME_LEN);
        strncpy(state.track_name, "MASTER", MB_MAX_TRACK_NAME_LEN);
        /* Init spectrum bands to minimum (silent) instead of 0dB from memset */
        for (int i = 0; i < MB_SPECTRUM_BANDS; i++)
            state.spectrum_bands[i] = -60.0f;
    }

    /**
     * Start listening for UDP packets on the specified port.
     */
    bool begin(uint16_t port = MB_DEFAULT_PORT) {
        _port = port;
        
        if (_udp.listen(port)) {
            Serial.printf("[UDP] Listening on port %d\n", port);
            
            _udp.onPacket([this](AsyncUDPPacket packet) {
                handlePacket(packet);
            });
            
            return true;
        }
        
        Serial.printf("[UDP] ERROR: Failed to bind port %d\n", port);
        return false;
    }

    /**
     * Send a transport command to the VST plugin.
     */
    void sendCommand(uint8_t command, uint8_t param8 = 0, float paramFloat = 0.0f) {
        if (!_pluginKnown) {
            Serial.println("[UDP] Cannot send command — plugin address unknown");
            return;
        }
        
        mb_command_packet_t pkt;
        mb_init_header(&pkt.header, command, _seqOut++);
        pkt.command = command;
        pkt.param8 = param8;
        pkt.reserved = 0;
        pkt.param_float = paramFloat;
        
        _udp.writeTo((uint8_t*)&pkt, sizeof(pkt), _pluginAddr, _pluginPort);
    }

    /**
     * Send a heartbeat response to the plugin.
     */
    void sendHeartbeat() {
        if (!_pluginKnown) return;
        
        mb_heartbeat_packet_t pkt;
        mb_init_header(&pkt.header, MB_CMD_HEARTBEAT_RESP, _seqOut++);
        pkt.uptime_ms = millis();
        
        _udp.writeTo((uint8_t*)&pkt, sizeof(pkt), _pluginAddr, _pluginPort);
    }

    /**
     * Check connection health. Call periodically.
     * Returns true if we've received data recently.
     */
    bool isConnected() const {
        return state.connected && (millis() - state.last_packet_time < 5000);
    }

    /**
     * Stop listening and release the UDP socket.
     * B2 fix: Call before reconnect to prevent double-bind of port 4210
     * when WiFi drops and reconnects. AsyncUDP holds a socket handle
     * internally; closing it allows a fresh bind on the next begin().
     */
    void stop() {
        _udp.close();
        _pluginKnown = false;  /* Re-discover plugin after reconnect */
        Serial.println("[UDP] Socket closed. Will re-bind on next connect.");
    }

    /**
     * Update connection state. Call in loop().
     */
    void update() {
        /* Check for connection timeout */
        if (state.connected && (millis() - state.last_packet_time > 5000)) {
            state.connected = false;
            Serial.println("[UDP] Connection to plugin lost (timeout)");
        }
    }

    /* Shared state — read by UI core, written by network core */
    meter_state_t state;

private:
    void handlePacket(AsyncUDPPacket& packet) {
        if (packet.length() < sizeof(mb_header_t) || packet.length() > 1024) return;
        
        const mb_header_t* hdr = (const mb_header_t*)packet.data();
        if (!mb_validate_header(hdr)) return;
        
        /* Remember the plugin's address for sending commands back */
        if (!_pluginKnown) {
            _pluginAddr = packet.remoteIP();
            _pluginPort = packet.remotePort();
            _pluginKnown = true;
            Serial.printf("[UDP] Plugin discovered at %s:%d\n",
                          _pluginAddr.toString().c_str(), _pluginPort);
        }
        
        /* Update connection state */
        state.last_packet_time = millis();
        state.packets_received++;
        
        /* Check for dropped packets */
        if (state.last_sequence != 0 && hdr->sequence != state.last_sequence + 1) {
            uint16_t gap = hdr->sequence - state.last_sequence - 1;
            state.packets_dropped += gap;
        }
        state.last_sequence = hdr->sequence;
        
        if (!state.connected) {
            state.connected = true;
            Serial.println("[UDP] Connected to plugin!");
        }
        
        /* Dispatch by packet type */
        switch (hdr->packet_type) {
            case MB_PKT_METER_DATA:
                if (g_conn_mode != 1) handleMeterData(packet);  /* Skip UDP data in serial-only mode */
                break;
            case MB_PKT_TRANSPORT_STATE:
                if (g_conn_mode != 1) handleTransportState(packet);
                break;
            case MB_PKT_TRACK_INFO:
                if (g_conn_mode != 1) handleTrackInfo(packet);
                break;
            case MB_PKT_PROJECT_INFO:
                if (g_conn_mode != 1) handleProjectInfo(packet);
                break;
            case MB_PKT_SPECTRUM:
                if (g_conn_mode != 1) handleSpectrumData(packet);
                break;
            case MB_PKT_HEARTBEAT:
                sendHeartbeat();  /* Always respond to heartbeats for discovery */
                break;
        }
    }

    void handleMeterData(AsyncUDPPacket& packet) {
        if (packet.length() < sizeof(mb_meter_packet_t)) return;
        
        const mb_meter_packet_t* data = (const mb_meter_packet_t*)packet.data();
        
        state.peak_l = data->peak_l;
        state.peak_r = data->peak_r;
        state.true_peak_l = data->true_peak_l;
        state.true_peak_r = data->true_peak_r;
        state.rms_l = data->rms_l;
        state.rms_r = data->rms_r;
        state.lufs_momentary = data->lufs_momentary;
        state.lufs_short = data->lufs_short;
        state.lufs_integrated = data->lufs_integrated;
        state.lufs_range = data->lufs_range;
        state.phase_correlation = data->phase_correlation;
        state.clip_count_l = data->clip_count_l;
        state.clip_count_r = data->clip_count_r;

        /* Diagnostic: print LUFS values once per second */
        static uint32_t _last_lufs_print = 0;
        uint32_t now = millis();
        if (now - _last_lufs_print >= 2000) {
            _last_lufs_print = now;
            Serial.printf("[UDP-LUFS] M=%.1f S=%.1f I=%.1f R=%.1f ph=%.2f cl=%d/%d pktLen=%d\n",
                data->lufs_momentary, data->lufs_short,
                data->lufs_integrated, data->lufs_range,
                data->phase_correlation,
                data->clip_count_l, data->clip_count_r,
                packet.length());
        }
    }

    void handleTransportState(AsyncUDPPacket& packet) {
        if (packet.length() < sizeof(mb_transport_packet_t)) return;
        
        const mb_transport_packet_t* data = (const mb_transport_packet_t*)packet.data();
        
        state.transport_flags = data->state_flags;
        state.meter_source = data->meter_source;
        state.time_sig_num = data->time_sig_num;
        state.time_sig_den = data->time_sig_den;
        state.position_beats = data->position_beats;
        state.position_secs = data->position_secs;
        state.tempo_bpm = data->tempo_bpm;
        state.measure = data->measure;
        state.beat_in_bar = data->beat_in_bar;
    }

    void handleTrackInfo(AsyncUDPPacket& packet) {
        if (packet.length() < sizeof(mb_track_info_packet_t)) return;
        
        const mb_track_info_packet_t* data = (const mb_track_info_packet_t*)packet.data();
        
        state.track_index = data->track_index;
        state.track_color_r = data->track_color_r;
        state.track_color_g = data->track_color_g;
        state.track_color_b = data->track_color_b;
        /* ATK-08 fix: guard multi-byte string copy from Core 0 against Core 1 reads */
        taskENTER_CRITICAL(&stateMux);
        strncpy(state.track_name, data->track_name, MB_MAX_TRACK_NAME_LEN - 1);
        state.track_name[MB_MAX_TRACK_NAME_LEN - 1] = '\0';
        taskEXIT_CRITICAL(&stateMux);
        state.track_muted = data->is_muted;
        state.track_soloed = data->is_soloed;
        state.track_armed = data->is_armed;
    }

    void handleProjectInfo(AsyncUDPPacket& packet) {
        if (packet.length() < sizeof(mb_project_info_packet_t)) return;
        
        const mb_project_info_packet_t* data = (const mb_project_info_packet_t*)packet.data();
        
        /* ATK-08 fix: guard multi-byte string copies */
        taskENTER_CRITICAL(&stateMux);
        strncpy(state.project_name, data->project_name, MB_MAX_PROJECT_NAME_LEN - 1);
        state.project_name[MB_MAX_PROJECT_NAME_LEN - 1] = '\0';
        strncpy(state.section_name, data->section_name, MB_MAX_SECTION_NAME_LEN - 1);
        state.section_name[MB_MAX_SECTION_NAME_LEN - 1] = '\0';
        taskEXIT_CRITICAL(&stateMux);
    }

    void handleSpectrumData(AsyncUDPPacket& packet) {
        if (packet.length() < sizeof(mb_spectrum_packet_t)) return;
        const mb_spectrum_packet_t* data = (const mb_spectrum_packet_t*)packet.data();
        for (int i = 0; i < MB_SPECTRUM_BANDS; i++) {
            state.spectrum_bands[i] = data->bands[i];
        }
        state.spectrum_last_time = millis();
    }

    AsyncUDP _udp;
    uint16_t _port;
    IPAddress _pluginAddr;
    uint16_t _pluginPort;
    bool _pluginKnown = false;
    uint16_t _seqOut;
};

#endif /* UDP_COMM_H */
