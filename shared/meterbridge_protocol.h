/*
 * MeterBridge Protocol v1.0
 * 
 * Shared binary protocol definition for UDP communication between
 * the JUCE VST3 plugin (REAPER) and the ESP32 CrowPanel display.
 * 
 * This file is included by BOTH the ESP32 firmware and the JUCE plugin.
 * Keep it C-compatible (no C++ features beyond what Arduino supports).
 * 
 * Copyright (c) 2026 FalconEYE Software Dev
 */

#ifndef METERBRIDGE_PROTOCOL_H
#define METERBRIDGE_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Protocol Constants ─────────────────────────────────────── */

#define MB_MAGIC_BYTE           0x4D  /* 'M' for MeterBridge */
#define MB_PROTOCOL_VERSION     0x01
#define MB_DEFAULT_PORT         9876
#define MB_DISCOVERY_PORT       9877
#define MB_MAX_TRACK_NAME_LEN   32
#define MB_MAX_PROJECT_NAME_LEN 32
#define MB_MAX_SECTION_NAME_LEN 32
#define MB_PACKET_HEADER_SIZE   4

/* ─── Packet Types (Plugin → ESP32) ──────────────────────────── */

#define MB_PKT_METER_DATA       0x01  /* Real-time meter values */
#define MB_PKT_TRANSPORT_STATE  0x02  /* Transport state update */
#define MB_PKT_TRACK_INFO       0x03  /* Track name/color info */
#define MB_PKT_HEARTBEAT        0x04  /* Keep-alive ping */
#define MB_PKT_PROJECT_INFO     0x05  /* Project name, section, measure */
#define MB_PKT_CONFIG_ACK       0x06  /* Configuration acknowledgement */
#define MB_PKT_SPECTRUM         0x07  /* 16-band FFT spectrum data */
#define MB_PKT_MARKER_LIST      0x08  /* Nearby markers/regions list */

/* ─── Command Types (ESP32 → Plugin) ─────────────────────────── */

#define MB_CMD_PLAY             0x10
#define MB_CMD_STOP             0x11
#define MB_CMD_RECORD           0x12
#define MB_CMD_REWIND           0x13
#define MB_CMD_FORWARD          0x14
#define MB_CMD_TOGGLE_REPEAT    0x15
#define MB_CMD_TOGGLE_METRONOME 0x16
#define MB_CMD_SEEK_POSITION    0x17  /* Payload: float32 position */
#define MB_CMD_PREV_TRACK       0x18
#define MB_CMD_NEXT_TRACK       0x19
#define MB_CMD_TOGGLE_MUTE      0x1A
#define MB_CMD_TOGGLE_SOLO      0x1B
#define MB_CMD_REQUEST_INFO     0x20  /* Request track info refresh */
#define MB_CMD_SET_METER_SRC    0x30  /* 0=Master, 1=Selected Track */
#define MB_CMD_SET_VOLUME       0x31  /* Payload: float32 dB value */
#define MB_CMD_HEARTBEAT_RESP   0x40  /* Heartbeat response */
#define MB_CMD_DISCOVERY        0x50  /* Device discovery broadcast */
#define MB_CMD_NEXT_MARKER      0x1C  /* Jump to next marker */
#define MB_CMD_PREV_MARKER      0x1D  /* Jump to previous marker */
#define MB_CMD_RESET_CLIPS      0x1E  /* Reset clip counters */

/* ─── Transport State Flags ──────────────────────────────────── */

#define MB_TRANSPORT_PLAYING    (1 << 0)
#define MB_TRANSPORT_PAUSED     (1 << 1)
#define MB_TRANSPORT_RECORDING  (1 << 2)
#define MB_TRANSPORT_REPEAT     (1 << 3)
#define MB_TRANSPORT_METRONOME  (1 << 4)
#define MB_TRANSPORT_STOPPED    (1 << 5)

/* ─── Meter Source ───────────────────────────────────────────── */

#define MB_SRC_MASTER           0x00
#define MB_SRC_SELECTED_TRACK   0x01

/* ─── Packet Structures ─────────────────────────────────────── */

/*
 * Common packet header (4 bytes)
 */
#pragma pack(push, 1)

typedef struct {
    uint8_t  magic;         /* MB_MAGIC_BYTE */
    uint8_t  packet_type;   /* MB_PKT_* or MB_CMD_* */
    uint16_t sequence;      /* Rolling sequence number */
} mb_header_t;

/*
 * Meter data packet (Plugin → ESP32)
 * Sent at ~60 Hz. All values in dBFS unless noted.
 * Total: 4 (header) + 48 (payload) = 52 bytes
 */
typedef struct {
    mb_header_t header;
    float peak_l;           /* Peak level, left channel (dBFS) */
    float peak_r;           /* Peak level, right channel (dBFS) */
    float true_peak_l;      /* Inter-sample true peak, left (dBFS) */
    float true_peak_r;      /* Inter-sample true peak, right (dBFS) */
    float rms_l;            /* RMS level, left channel (dBFS) */
    float rms_r;            /* RMS level, right channel (dBFS) */
    float lufs_momentary;   /* LUFS Momentary (400ms window) */
    float lufs_short;       /* LUFS Short-term (3s window) */
    float lufs_integrated;  /* LUFS Integrated (session) */
    float lufs_range;       /* Loudness Range (LRA) in LU */
    float phase_correlation;/* Stereo phase correlation (-1.0 to +1.0) */
    uint16_t clip_count_l;  /* Cumulative clip count, left */
    uint16_t clip_count_r;  /* Cumulative clip count, right */
} mb_meter_packet_t;

/*
 * Transport state packet (Plugin → ESP32)
 * Sent on state change + periodic refresh
 * Total: 4 (header) + 16 (payload) = 20 bytes
 */
typedef struct {
    mb_header_t header;
    uint8_t  state_flags;   /* Bitfield: MB_TRANSPORT_* */
    uint8_t  meter_source;  /* MB_SRC_MASTER or MB_SRC_SELECTED_TRACK */
    uint8_t  time_sig_num;  /* Time signature numerator */
    uint8_t  time_sig_den;  /* Time signature denominator */
    float    position_beats;/* Current position in beats */
    float    position_secs; /* Current position in seconds */
    float    tempo_bpm;     /* Current tempo in BPM */
    uint16_t measure;       /* Current measure/bar number (1-based) */
    uint16_t beat_in_bar;   /* Current beat within measure (1-based) */
} mb_transport_packet_t;

/*
 * Project info packet (Plugin → ESP32)
 * Sent on change + periodic refresh (~2Hz)
 * Carries project name, current section/region name
 */
typedef struct {
    mb_header_t header;
    char     project_name[MB_MAX_PROJECT_NAME_LEN]; /* Null-terminated */
    char     section_name[MB_MAX_SECTION_NAME_LEN]; /* Current region/marker name */
} mb_project_info_packet_t;

/*
 * Track info packet (Plugin → ESP32)
 * Sent on track selection change or on request
 * Total: 4 (header) + 40 (payload) = 44 bytes
 */
typedef struct {
    mb_header_t header;
    uint8_t  track_index;   /* Track number (0 = master) */
    uint8_t  track_color_r; /* Track color, red component */
    uint8_t  track_color_g; /* Track color, green component */
    uint8_t  track_color_b; /* Track color, blue component */
    char     track_name[MB_MAX_TRACK_NAME_LEN]; /* Null-terminated UTF-8 */
    uint8_t  is_muted;      /* 1 if track is muted */
    uint8_t  is_soloed;     /* 1 if track is soloed */
    uint8_t  is_armed;      /* 1 if track is armed for recording */
    uint8_t  reserved;      /* Padding for alignment */
} mb_track_info_packet_t;

/*
 * Heartbeat packet (bidirectional)
 * Sent every ~2 seconds to maintain connection awareness
 * Total: 4 (header) + 4 (payload) = 8 bytes
 */
typedef struct {
    mb_header_t header;
    uint32_t uptime_ms;     /* Sender uptime in milliseconds */
} mb_heartbeat_packet_t;

/*
 * Command packet (ESP32 → Plugin)
 * Sent on touch events / user interaction
 * Total: 4 (header) + 8 (payload) = 12 bytes
 */
typedef struct {
    mb_header_t header;
    uint8_t  command;       /* MB_CMD_* */
    uint8_t  param8;        /* 8-bit parameter (e.g., meter source) */
    uint16_t reserved;      /* Reserved / alignment */
    float    param_float;   /* Float parameter (e.g., seek position) */
} mb_command_packet_t;

/*
 * Discovery packet (ESP32 → broadcast)
 * Sent on MB_DISCOVERY_PORT to find the plugin
 * Total: 4 (header) + 36 (payload) = 40 bytes
 */
typedef struct {
    mb_header_t header;
    char     device_name[24];  /* Device name, null-terminated */
    uint16_t listen_port;      /* ESP32's listening port */
    uint8_t  firmware_major;   /* Firmware version major */
    uint8_t  firmware_minor;   /* Firmware version minor */
    uint32_t display_width;    /* Display width in pixels */
    uint32_t display_height;   /* Display height in pixels */
} mb_discovery_packet_t;

/*
 * Spectrum data packet (Plugin → ESP32)
 * 16-band frequency analysis, sent at meter rate.
 * Band centers (Hz): 31, 44, 63, 88, 125, 177, 250, 354, 500, 707, 1k, 1.4k, 2k, 4k, 8k, 16k
 * Total: 4 (header) + 64 (16 floats) = 68 bytes
 */
#define MB_SPECTRUM_BANDS 16

typedef struct {
    mb_header_t header;
    float bands[MB_SPECTRUM_BANDS]; /* Band levels in dBFS (-60..0) */
} mb_spectrum_packet_t;

/*
 * Marker list packet (Plugin → ESP32)
 * Reports nearest markers around playback position.
 * Total: 4 (header) + 1 + 1 + 5*34 = 176 bytes
 */
#define MB_MAX_MARKERS 5
#define MB_MARKER_NAME_LEN 32

typedef struct {
    uint8_t is_region;              /* 0=marker, 1=region */
    uint8_t reserved;
    float   position_secs;          /* Position in seconds */
    char    name[MB_MARKER_NAME_LEN]; /* Null-terminated */
} mb_marker_entry_t;

typedef struct {
    mb_header_t header;
    uint8_t marker_count;           /* How many valid entries (0-5) */
    int8_t  current_idx;            /* Index of "current" marker (-1 = none) */
    mb_marker_entry_t markers[MB_MAX_MARKERS];
} mb_marker_list_packet_t;

#pragma pack(pop)

/* ─── Utility Functions ──────────────────────────────────────── */

static inline void mb_init_header(mb_header_t* hdr, uint8_t type, uint16_t seq) {
    hdr->magic = MB_MAGIC_BYTE;
    hdr->packet_type = type;
    hdr->sequence = seq;
}

static inline int mb_validate_header(const mb_header_t* hdr) {
    return (hdr->magic == MB_MAGIC_BYTE) ? 1 : 0;
}

/* Convert dBFS float to 0-255 uint8 for compact representation */
static inline uint8_t mb_db_to_byte(float db) {
    /* Map -60dB..0dB to 0..255 */
    if (db <= -60.0f) return 0;
    if (db >= 0.0f) return 255;
    return (uint8_t)((db + 60.0f) * (255.0f / 60.0f));
}

/* Convert 0-255 uint8 back to dBFS float */
static inline float mb_byte_to_db(uint8_t val) {
    return ((float)val * (60.0f / 255.0f)) - 60.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* METERBRIDGE_PROTOCOL_H */
