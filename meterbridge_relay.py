#!/usr/bin/env python3
"""
MeterBridge Relay — REAPER JSON → ESP32

Supports two transport modes:
  UDP  (default): Binary packet format over WiFi/LAN.
  Serial: Text-based SMETER/STRANSPORT/STRACK/SPROJECT commands over USB.
  Both: Simultaneously sends UDP and Serial (for hybrid setups / testing).

Zero external dependencies in UDP-only mode.
Serial mode requires pyserial: pip install pyserial

Usage:
    python meterbridge_relay.py [--esp32-ip <ip>] [--port <port>]
    python meterbridge_relay.py --connection-mode serial --serial-port COM3
    python meterbridge_relay.py --connection-mode both --esp32-ip 192.168.1.x --serial-port COM3

Copyright (c) 2026 FalconEYE Software Dev
Version: 2.0.0
"""

import socket
import struct
import json
import time
import os
import sys
import argparse
import ctypes


# ═══════════════════════════════════════════════════════════════
# Windows Console Fix — Disable QuickEdit Mode
# ═══════════════════════════════════════════════════════════════
# When QuickEdit is enabled (cmd.exe default), clicking inside the
# console window puts it into "select" mode, which pauses ALL I/O.
# This causes the relay to silently freeze until the user presses a
# key to cancel the selection. Disabling QuickEdit at startup
# eliminates this entire class of user-facing hangs.

def _disable_quickedit():
    """Disable QuickEdit mode on Windows console to prevent click-to-freeze."""
    if sys.platform != "win32":
        return
    try:
        kernel32 = ctypes.windll.kernel32
        STD_INPUT_HANDLE = ctypes.c_ulong(-10 & 0xFFFFFFFF)
        ENABLE_QUICK_EDIT_MODE = 0x0040
        ENABLE_EXTENDED_FLAGS  = 0x0080
        handle = kernel32.GetStdHandle(STD_INPUT_HANDLE)
        mode = ctypes.c_ulong()
        kernel32.GetConsoleMode(handle, ctypes.byref(mode))
        mode.value &= ~ENABLE_QUICK_EDIT_MODE  # clear QuickEdit
        mode.value |= ENABLE_EXTENDED_FLAGS     # required for the change to take effect
        kernel32.SetConsoleMode(handle, mode)
    except Exception:
        pass  # non-fatal; best-effort

# ═══════════════════════════════════════════════════════════════
# Protocol Constants (must match meterbridge_protocol.h)
# ═══════════════════════════════════════════════════════════════

MB_MAGIC_BYTE           = 0x4D  # 'M'
MB_PROTOCOL_VERSION     = 0x01
MB_DEFAULT_PORT         = 9876
MB_DISCOVERY_PORT       = 9877
MB_MAX_TRACK_NAME_LEN   = 32
MB_MAX_PROJECT_NAME_LEN = 32
MB_MAX_SECTION_NAME_LEN = 32

# Packet types (Relay → ESP32)
MB_PKT_METER_DATA       = 0x01
MB_PKT_TRANSPORT_STATE  = 0x02
MB_PKT_TRACK_INFO       = 0x03
MB_PKT_HEARTBEAT        = 0x04
MB_PKT_PROJECT_INFO     = 0x05
MB_PKT_SPECTRUM         = 0x07
MB_PKT_MARKER_LIST      = 0x08

# Command types (ESP32 → Relay)
MB_CMD_PLAY             = 0x10
MB_CMD_STOP             = 0x11
MB_CMD_RECORD           = 0x12
MB_CMD_REWIND           = 0x13
MB_CMD_FORWARD          = 0x14
MB_CMD_TOGGLE_REPEAT    = 0x15
MB_CMD_TOGGLE_METRONOME = 0x16
MB_CMD_SEEK_POSITION    = 0x17
MB_CMD_PREV_TRACK       = 0x18
MB_CMD_NEXT_TRACK       = 0x19
MB_CMD_TOGGLE_MUTE      = 0x1A
MB_CMD_TOGGLE_SOLO      = 0x1B
MB_CMD_REQUEST_INFO     = 0x20
MB_CMD_SET_METER_SRC    = 0x30
MB_CMD_SET_VOLUME       = 0x31
MB_CMD_HEARTBEAT_RESP   = 0x40
MB_CMD_DISCOVERY        = 0x50
MB_CMD_NEXT_MARKER      = 0x1C
MB_CMD_PREV_MARKER      = 0x1D
MB_CMD_RESET_CLIPS      = 0x1E

MB_SPECTRUM_BANDS       = 16

# Transport flags
MB_TRANSPORT_PLAYING    = 0x01
MB_TRANSPORT_PAUSED     = 0x02
MB_TRANSPORT_RECORDING  = 0x04
MB_TRANSPORT_REPEAT     = 0x08
MB_TRANSPORT_METRONOME  = 0x10
MB_TRANSPORT_STOPPED    = 0x20

# Command name → REAPER action mapping
CMD_MAP = {
    MB_CMD_PLAY:             "play",
    MB_CMD_STOP:             "stop",
    MB_CMD_RECORD:           "record",
    MB_CMD_REWIND:           "rewind",
    MB_CMD_FORWARD:          "forward",
    MB_CMD_TOGGLE_REPEAT:    "toggle_repeat",
    MB_CMD_TOGGLE_METRONOME: "toggle_metronome",
    MB_CMD_PREV_TRACK:       "prev_track",
    MB_CMD_NEXT_TRACK:       "next_track",
    MB_CMD_TOGGLE_MUTE:      "toggle_mute",
    MB_CMD_TOGGLE_SOLO:      "toggle_solo",
    MB_CMD_NEXT_MARKER:      "next_marker",
    MB_CMD_PREV_MARKER:      "prev_marker",
    MB_CMD_RESET_CLIPS:      "reset_clips",
}


# ═══════════════════════════════════════════════════════════════
# Packet Packing (matches #pragma pack(push, 1) structs)
# ═══════════════════════════════════════════════════════════════

class PacketBuilder:
    """Builds binary packets matching meterbridge_protocol.h structs."""

    def __init__(self):
        self._seq = 0

    def _header(self, pkt_type: int) -> bytes:
        hdr = struct.pack("<BBH", MB_MAGIC_BYTE, pkt_type, self._seq & 0xFFFF)
        self._seq += 1
        return hdr

    def meter_packet(self, state: dict) -> bytes:
        peak_l = float(state.get("masterMeterL", -60))
        peak_r = float(state.get("masterMeterR", -60))
        true_peak_l = peak_l
        true_peak_r = peak_r
        rms_l = max(-60.0, peak_l - 3.0) if peak_l > -60 else -60.0
        rms_r = max(-60.0, peak_r - 3.0) if peak_r > -60 else -60.0
        # Read real LUFS / phase / clip values from REAPER Lua bridge state
        lufs_m = float(state.get("lufsM", -70.0))
        lufs_s = float(state.get("lufsS", -70.0))
        lufs_i = float(state.get("lufsI", -70.0))
        lufs_r = float(state.get("lufsR", 0.0))
        phase  = float(state.get("phase", 1.0))
        clip_l = int(state.get("clipL", 0))
        clip_r = int(state.get("clipR", 0))
        return self._header(MB_PKT_METER_DATA) + struct.pack(
            "<fffffffffffHH",
            peak_l, peak_r, true_peak_l, true_peak_r,
            rms_l, rms_r,
            lufs_m, lufs_s, lufs_i, lufs_r,
            phase, clip_l, clip_r
        )

    def transport_packet(self, state: dict) -> bytes:
        flags = int(state.get("transportFlags", MB_TRANSPORT_STOPPED))
        meter_src = 0
        ts_num = int(state.get("timeSigNum", 4))
        ts_den = int(state.get("timeSigDen", 4))
        pos_beats = float(state.get("positionBeats", 0))
        pos_secs = float(state.get("positionSecs", 0))
        tempo = float(state.get("tempo", 120))
        bb = state.get("barsBeats", "1.1.000")
        parts = str(bb).split(".")
        measure = int(parts[0]) if len(parts) > 0 else 1
        beat = int(parts[1]) if len(parts) > 1 else 1
        return self._header(MB_PKT_TRANSPORT_STATE) + struct.pack(
            "<BBBB fff HH",
            flags, meter_src, ts_num, ts_den,
            pos_beats, pos_secs, tempo,
            measure, beat
        )

    def project_info_packet(self, state: dict) -> bytes:
        project = state.get("projectName", "Untitled")[:MB_MAX_PROJECT_NAME_LEN - 1]
        section = state.get("regionName", "")[:MB_MAX_SECTION_NAME_LEN - 1]
        return self._header(MB_PKT_PROJECT_INFO) + struct.pack(
            f"<{MB_MAX_PROJECT_NAME_LEN}s{MB_MAX_SECTION_NAME_LEN}s",
            project.encode("utf-8", errors="replace"),
            section.encode("utf-8", errors="replace")
        )

    def track_info_packet(self, state: dict) -> bytes:
        sel = state.get("selectedTrack", {})
        idx = int(sel.get("index", 0))
        r = int(sel.get("colorR", 180))
        g = int(sel.get("colorG", 180))
        b = int(sel.get("colorB", 200))
        name = str(sel.get("name", "MASTER"))[:MB_MAX_TRACK_NAME_LEN - 1]
        muted = 1 if sel.get("muted", False) else 0
        soloed = 1 if sel.get("soloed", False) else 0
        armed = 1 if sel.get("armed", False) else 0
        return self._header(MB_PKT_TRACK_INFO) + struct.pack(
            f"<BBBB{MB_MAX_TRACK_NAME_LEN}sBBBB",
            idx, r, g, b,
            name.encode("utf-8", errors="replace").ljust(MB_MAX_TRACK_NAME_LEN, b'\x00')[:MB_MAX_TRACK_NAME_LEN],
            muted, soloed, armed, 0
        )

    def heartbeat_packet(self) -> bytes:
        uptime = int(time.monotonic() * 1000) & 0xFFFFFFFF
        return self._header(MB_PKT_HEARTBEAT) + struct.pack("<I", uptime)

    def discovery_packet(self, listen_port: int = MB_DEFAULT_PORT) -> bytes:
        name = b"MeterBridge Relay"
        name = name[:24].ljust(24, b'\x00')
        return self._header(MB_CMD_DISCOVERY) + struct.pack(
            "<24sHBBII", name, listen_port, 1, 0, 0, 0
        )

    def spectrum_packet(self, state: dict) -> bytes:
        """Build a 16-band spectrum packet from state['spectrum'] array."""
        bands = state.get("spectrum", [])
        # Pad/trim to exactly 16 bands
        band_values = []
        for i in range(MB_SPECTRUM_BANDS):
            if i < len(bands):
                band_values.append(float(bands[i]))
            else:
                band_values.append(-60.0)
        return self._header(MB_PKT_SPECTRUM) + struct.pack(
            f"<{MB_SPECTRUM_BANDS}f", *band_values
        )


# ═══════════════════════════════════════════════════════════════
# Serial Sender — text-based SMETER/STRANSPORT/STRACK/SPROJECT
# ═══════════════════════════════════════════════════════════════

class SerialSender:
    """Sends meter data to ESP32 over USB serial (no WiFi needed)."""

    def __init__(self, port: str, baud: int = 115200, timeout: float = 0.1):
        try:
            import serial as _serial
            self._ser = _serial.Serial(port, baud, timeout=timeout)
            time.sleep(1.5)
            self._ser.reset_input_buffer()
            self._ok = True
            print(f"[SERIAL] Connected to {port} @ {baud}")
        except ImportError:
            print("[SERIAL] ERROR: pyserial not installed. Run: pip install pyserial")
            self._ok = False
        except Exception as e:
            print(f"[SERIAL] ERROR opening {port}: {e}")
            self._ok = False

    @property
    def connected(self):
        return self._ok

    def _send(self, cmd: str) -> bool:
        if not self._ok:
            return False
        try:
            self._ser.write((cmd + "\n").encode())
            self._ser.flush()
            time.sleep(0.002)
            while self._ser.in_waiting:
                self._ser.readline()
            return True
        except Exception as e:
            print(f"[SERIAL] Send error: {e}")
            self._ok = False
            return False

    def send_meter(self, state: dict) -> bool:
        pk_l = float(state.get("masterMeterL", -60))
        pk_r = float(state.get("masterMeterR", -60))
        rms_l = max(-60.0, pk_l - 3.0) if pk_l > -60 else -60.0
        rms_r = max(-60.0, pk_r - 3.0) if pk_r > -60 else -60.0
        lufs_m = float(state.get("lufsM", -70))
        lufs_s = float(state.get("lufsS", -70))
        lufs_i = float(state.get("lufsI", -70))
        lufs_r = float(state.get("lufsR", 0))
        phase  = float(state.get("phase", 1.0))
        cl     = int(state.get("clipL", 0))
        cr     = int(state.get("clipR", 0))
        cmd = (f"SMETER:{pk_l:.2f},{pk_r:.2f},{rms_l:.2f},{rms_r:.2f},"
               f"{pk_l:.2f},{pk_r:.2f},{lufs_m:.2f},{lufs_s:.2f},{lufs_i:.2f},"
               f"{lufs_r:.2f},{phase:.3f},{cl},{cr}")
        return self._send(cmd)

    def send_transport(self, state: dict) -> bool:
        flags = int(state.get("transportFlags", MB_TRANSPORT_STOPPED))
        tempo = float(state.get("tempo", 120.0))
        ts_n  = int(state.get("timeSigNum", 4))
        ts_d  = int(state.get("timeSigDen", 4))
        pos_s = float(state.get("positionSecs", 0))
        bb    = str(state.get("barsBeats", "1.1.000")).split(".")
        meas  = int(bb[0]) if len(bb) > 0 else 1
        beat  = int(bb[1]) if len(bb) > 1 else 1
        cmd = f"STRANSPORT:{flags},{tempo:.2f},{ts_n},{ts_d},{pos_s:.3f},{meas},{beat}"
        return self._send(cmd)

    def send_track(self, state: dict) -> bool:
        sel  = state.get("selectedTrack", {})
        idx  = int(sel.get("index", 0))
        r    = int(sel.get("colorR", 180))
        g    = int(sel.get("colorG", 180))
        b    = int(sel.get("colorB", 200))
        name = str(sel.get("name", "MASTER"))[:31]
        muted   = 1 if sel.get("muted", False) else 0
        soloed  = 1 if sel.get("soloed", False) else 0
        armed   = 1 if sel.get("armed", False) else 0
        cmd = f"STRACK:{idx},{r},{g},{b},{name},{muted},{soloed},{armed}"
        return self._send(cmd)

    def send_project(self, state: dict) -> bool:
        proj = str(state.get("projectName", ""))[:31]
        sec  = str(state.get("regionName",  ""))[:31]
        cmd = f"SPROJECT:{proj}|{sec}"
        return self._send(cmd)

    def close(self):
        if self._ok:
            try:
                self._ser.close()
            except Exception:
                pass


# ═══════════════════════════════════════════════════════════════
# Command Handler (ESP32 → REAPER)
# ═══════════════════════════════════════════════════════════════

def handle_command_packet(data: bytes, addr: tuple, cmd_file: str):
    if len(data) < 4:
        return
    magic, pkt_type, seq = struct.unpack_from('<BBH', data, 0)
    if magic != MB_MAGIC_BYTE:
        return
    if pkt_type == MB_CMD_HEARTBEAT_RESP:
        return
    cmd_name = CMD_MAP.get(pkt_type)
    if cmd_name:
        payload = {"command": cmd_name}
        if pkt_type == MB_CMD_SET_METER_SRC and len(data) >= 5:
            payload["value"] = data[4]
        try:
            with open(cmd_file, 'w') as f:
                f.write(json.dumps(payload))
            print(f"[CMD] {addr[0]} → {cmd_name}")
        except OSError as e:
            print(f"[CMD] Failed to write command: {e}")
    else:
        print(f"[CMD] Unknown packet 0x{pkt_type:02X} from {addr[0]}")


# ═══════════════════════════════════════════════════════════════
# Discovery
# ═══════════════════════════════════════════════════════════════

def discover_esp32(timeout: float = 5.0):
    print("[DISC] Sending discovery broadcast...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(1.0)
    builder = PacketBuilder()
    disc_pkt = builder.discovery_packet()
    start = time.monotonic()
    while time.monotonic() - start < timeout:
        try:
            sock.sendto(disc_pkt, ("<broadcast>", MB_DISCOVERY_PORT))
            try:
                data, addr = sock.recvfrom(256)
                if len(data) >= 4 and data[0] == MB_MAGIC_BYTE:
                    print(f"[DISC] ESP32 found at {addr[0]}:{addr[1]}")
                    sock.close()
                    return addr[0]
            except socket.timeout:
                pass
        except OSError as e:
            print(f"[DISC] Broadcast error: {e}")
            time.sleep(1)
    sock.close()
    return None


# ═══════════════════════════════════════════════════════════════
# Relay Config Helper
# ═══════════════════════════════════════════════════════════════

RELAY_CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "relay_config.txt")

def read_relay_config():
    """Read key=value pairs from relay_config.txt. Returns a dict."""
    cfg = {}
    try:
        with open(RELAY_CONFIG_FILE, "r") as f:
            for line in f:
                line = line.strip()
                if "=" in line and not line.startswith("#"):
                    k, _, v = line.partition("=")
                    cfg[k.strip()] = v.strip()
    except (OSError, IOError):
        pass
    return cfg

def clamp_update_ms(ms):
    """Clamp update interval to [16, 2500] ms."""
    return max(16, min(2500, int(ms)))


# ═══════════════════════════════════════════════════════════════
# Main Relay Loop
# ═══════════════════════════════════════════════════════════════

def main():
    _disable_quickedit()
    parser = argparse.ArgumentParser(description="MeterBridge Relay — REAPER to ESP32")
    parser.add_argument("--esp32-ip",   default=None,  help="ESP32 IP address (auto-discover if not set)")
    parser.add_argument("--port",       type=int, default=MB_DEFAULT_PORT)
    parser.add_argument("--state-dir",  default=None,  help="Override state file directory")
    parser.add_argument("--connection-mode", default="udp",
                        choices=["udp", "serial", "both"],
                        help="Transport: udp (default) | serial (USB) | both")
    parser.add_argument("--serial-port", default="COM3", help="Serial port (default: COM3)")
    parser.add_argument("--serial-baud", type=int, default=115200)
    parser.add_argument("--update-rate-ms", type=int, default=None,
                        help="Update interval ms [20-2500]. Overrides relay_config.txt.")
    args = parser.parse_args()

    if args.state_dir:
        state_dir = args.state_dir
    else:
        temp = os.environ.get("TEMP", os.environ.get("TMPDIR", "/tmp"))
        state_dir = os.path.join(temp, "MeterBridge")

    state_file = os.path.join(state_dir, "live_state.json")
    cmd_file   = os.path.join(state_dir, "commands.json")

    use_udp    = args.connection_mode in ("udp", "both")
    use_serial = args.connection_mode in ("serial", "both")

    print("=" * 55)
    print("  MeterBridge Relay v2.0-RC1")
    print("=" * 55)
    print(f"[RELAY] State file  : {state_file}")
    print(f"[RELAY] Command file: {cmd_file}")
    print(f"[RELAY] Mode        : {args.connection_mode.upper()}")

    # ── UDP setup ──────────────────────────────────────────────
    sock = None
    esp32_ip   = None
    esp32_port = args.port

    if use_udp:
        esp32_ip = args.esp32_ip
        if not esp32_ip:
            print("[RELAY] No ESP32 IP — attempting broadcast discovery...")
            esp32_ip = discover_esp32(timeout=10.0)
            if not esp32_ip:
                if use_serial:
                    print("[RELAY] Discovery failed — continuing in serial-only mode.")
                    use_udp = False
                else:
                    print("[RELAY] Discovery failed. Specify --esp32-ip or use --connection-mode serial")
                    sys.exit(1)

        if use_udp and esp32_ip:
            print(f"[RELAY] UDP Target  : {esp32_ip}:{esp32_port}")
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("0.0.0.0", MB_DISCOVERY_PORT))
            sock.setblocking(False)
            print(f"[RELAY] Bound to port {MB_DISCOVERY_PORT} (bidirectional)")

    # ── Serial setup ───────────────────────────────────────────
    serial_sender = None
    if use_serial:
        serial_sender = SerialSender(args.serial_port, args.serial_baud)
        if not serial_sender.connected:
            if use_udp:
                print("[RELAY] Serial failed — falling back to UDP-only mode.")
                use_serial = False
                serial_sender = None
            else:
                print("[RELAY] Serial connection failed. Exiting.")
                sys.exit(1)

    builder = PacketBuilder()

    # ── Initial update rate: CLI arg > relay_config.txt > 250ms default ──
    _init_cfg = read_relay_config()
    _init_ms  = args.update_rate_ms if args.update_rate_ms else int(_init_cfg.get("update_ms", 25))
    update_ms = clamp_update_ms(_init_ms)
    print(f"[RELAY] Update rate  : {update_ms}ms ({1000.0/update_ms:.1f}/s)")

    # State tracking
    last_content        = ""
    last_transport_time = 0.0
    last_project_time   = 0.0
    last_heartbeat_time = 0.0
    last_config_check   = 0.0
    frame_count  = 0
    packets_sent = 0
    last_status_time = time.monotonic()

    def _intervals(ms):
        """Derive all send intervals from the base update_ms."""
        meter_hz     = 1000.0 / ms
        transport_hz = max(1.0, meter_hz / 4)   # 1/4 of meter, min 1Hz
        project_hz   = max(0.4, meter_hz / 8)   # 1/8 of meter, min 0.4Hz
        return 1.0 / meter_hz, 1.0 / transport_hz, 1.0 / project_hz

    METER_INTERVAL, TRANSPORT_INTERVAL, PROJECT_INTERVAL = _intervals(update_ms)
    HEARTBEAT_INTERVAL = 2.0
    STATUS_INTERVAL    = 10.0

    print("[RELAY] Running! (Ctrl+C to stop)")
    print("[RELAY] Waiting for REAPER Lua bridge to start writing state...")

    try:
        while True:
            now = time.monotonic()
            frame_count += 1

            # ── Live config reload (once per second, skip if CLI forced rate) ──
            if args.update_rate_ms is None and (now - last_config_check >= 1.0):
                last_config_check = now
                cfg = read_relay_config()
                new_ms = clamp_update_ms(int(cfg.get("update_ms", 25)))
                if new_ms != update_ms:
                    update_ms = new_ms
                    METER_INTERVAL, TRANSPORT_INTERVAL, PROJECT_INTERVAL = _intervals(update_ms)
                    print(f"[RELAY] Rate change  : {update_ms}ms ({1000.0/update_ms:.1f}/s)")

            try:
                if os.path.exists(state_file):
                    with open(state_file, "r") as f:
                        content = f.read()

                    if content and content != last_content:
                        last_content = content
                        try:
                            state = json.loads(content)
                        except json.JSONDecodeError:
                            time.sleep(METER_INTERVAL)
                            continue

                        # ── Meter data at ~60Hz ──
                        if use_udp and sock:
                            sock.sendto(builder.meter_packet(state), (esp32_ip, esp32_port))
                            packets_sent += 1
                            # Send spectrum if the Lua bridge is exporting band data
                            if "spectrum" in state:
                                sock.sendto(builder.spectrum_packet(state), (esp32_ip, esp32_port))
                                packets_sent += 1
                        if use_serial and serial_sender:
                            serial_sender.send_meter(state)
                            packets_sent += 1

                        # ── Transport + track at ~10Hz ──
                        if now - last_transport_time >= TRANSPORT_INTERVAL:
                            if use_udp and sock:
                                sock.sendto(builder.transport_packet(state), (esp32_ip, esp32_port))
                                sock.sendto(builder.track_info_packet(state), (esp32_ip, esp32_port))
                                packets_sent += 2
                            if use_serial and serial_sender:
                                serial_sender.send_transport(state)
                                serial_sender.send_track(state)
                                packets_sent += 2
                            last_transport_time = now

                        # ── Project info at ~2Hz ──
                        if now - last_project_time >= PROJECT_INTERVAL:
                            if use_udp and sock:
                                sock.sendto(builder.project_info_packet(state), (esp32_ip, esp32_port))
                                packets_sent += 1
                            if use_serial and serial_sender:
                                serial_sender.send_project(state)
                                packets_sent += 1
                            last_project_time = now
                else:
                    if frame_count % 300 == 0:
                        print(f"[RELAY] Waiting for state file: {state_file}")
            except (IOError, OSError):
                pass

            # ── UDP Heartbeat ──
            if use_udp and sock and (now - last_heartbeat_time >= HEARTBEAT_INTERVAL):
                sock.sendto(builder.heartbeat_packet(), (esp32_ip, esp32_port))
                packets_sent += 1
                last_heartbeat_time = now

            # ── Drain incoming commands from ESP32 (UDP only) ──
            if use_udp and sock:
                while True:
                    try:
                        data, addr = sock.recvfrom(256)
                        handle_command_packet(data, addr, cmd_file)
                    except (BlockingIOError, OSError):
                        break

            # ── Status report ──
            if now - last_status_time >= STATUS_INTERVAL:
                fps = frame_count / STATUS_INTERVAL
                pps = packets_sent / STATUS_INTERVAL
                modes = []
                if use_udp:    modes.append(f"UDP→{esp32_ip}:{esp32_port}")
                if use_serial: modes.append(f"Serial→{args.serial_port}")
                print(f"[RELAY] {fps:.0f} fps | {pps:.0f} pkts/s | {' + '.join(modes)}")
                frame_count = 0
                packets_sent = 0
                last_status_time = now

            time.sleep(METER_INTERVAL)

    except KeyboardInterrupt:
        print("\n[RELAY] Shutting down...")
        if sock:           sock.close()
        if serial_sender:  serial_sender.close()
        print("[RELAY] Done.")


if __name__ == "__main__":
    main()
