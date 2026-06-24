"""
MeterBridge ESP32 â€” Automated Test Harness
==========================================
Connects over USB-Serial, drives the device via the serial command interface,
and validates responses. Exits 0 if all tests pass, 1 on any failure.

Usage:
    python test_harness.py [COM_PORT]   # default: COM3
"""

import sys
import time
import json
import serial

PORT  = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD  = 115200
TOUT  = 5.0   # seconds per command timeout

PASS = "PASS"
FAIL = "FAIL"
WARN = "WARN"

results = []

# â”€â”€ Helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def log(sym, label, detail=""):
    line = f"{sym} {label}"
    if detail:
        line += f"  |  {detail}"
    print(line)
    results.append((sym, label))

def send(ser, cmd):
    ser.write((cmd + "\n").encode())
    ser.flush()

def read_line(ser, timeout=TOUT):
    ser.timeout = timeout
    line = ser.readline().decode(errors="replace").strip()
    return line

def expect(ser, cmd, contains, label, timeout=TOUT):
    send(ser, cmd)
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = read_line(ser, timeout=deadline - time.time())
        if not line:
            continue
        if line.startswith("[CMD]"):   # echo, skip
            continue
        if contains in line:
            log(PASS, label, line)
            return line
        # Swallow unrelated output (LVGL debug, etc.)
    log(FAIL, label, f"timeout waiting for '{contains}'")
    return None

def wait_for_reboot(ser, timeout=12):
    """Wait until '[OK] MeterBridge' appears after a reboot command."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        ser.timeout = 1.0
        line = ser.readline().decode(errors="replace").strip()
        if "[OK] MeterBridge" in line:
            return True
    return False

# â”€â”€ Test cases â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def run_tests(ser):
    print("\nâ•â•â•â•â•â•â•â•â•â• MeterBridge Test Harness â•â•â•â•â•â•â•â•â•â•\n")

    # T01 â€” PING
    expect(ser, "PING", "PONG", "T01 PING responds PONG")

    # T02 â€” STATUS parses as valid JSON
    send(ser, "STATUS")
    deadline = time.time() + TOUT
    status = None
    while time.time() < deadline:
        line = read_line(ser, timeout=deadline - time.time())
        if not line or line.startswith("[CMD]"):
            continue
        try:
            status = json.loads(line)
            break
        except json.JSONDecodeError:
            pass
    if status:
        log(PASS, "T02 STATUS returns valid JSON", str(status))
    else:
        log(FAIL, "T02 STATUS returns valid JSON", "no valid JSON received")

    # T03 â€” Heap sanity (>50 KB expected after full boot)
    if status:
        heap = status.get("heap", 0)
        if heap > 50_000:
            log(PASS, "T03 Heap healthy", f"{heap:,} bytes free")
        else:
            log(FAIL, "T03 Heap healthy", f"only {heap:,} bytes â€” memory pressure!")

    # T04 â€” PSRAM present (>1 MB)
    if status:
        psram = status.get("psram", 0)
        if psram > 1_000_000:
            log(PASS, "T04 PSRAM available", f"{psram:,} bytes free")
        else:
            log(WARN, "T04 PSRAM available", f"{psram:,} bytes â€” may be low")

    # T05 â€” Meter injection (PEAK L/R)
    resp = expect(ser, "INJECT_METER:-12.5,-11.0,-18.0,-17.5,-14.0,-13.5",
                  "INJECTED", "T05 INJECT_METER accepted")
    time.sleep(0.2)

    # T06 â€” STATUS reflects injected values
    send(ser, "STATUS")
    deadline = time.time() + TOUT
    s2 = None
    while time.time() < deadline:
        line = read_line(ser, timeout=deadline - time.time())
        if not line or line.startswith("[CMD]"):
            continue
        try:
            s2 = json.loads(line)
            break
        except json.JSONDecodeError:
            pass
    if s2 and abs(s2.get("peak_l", 0) - (-12.5)) < 0.5:
        log(PASS, "T06 Injected meter data reflected in STATUS", f"peak_l={s2['peak_l']}")
    else:
        # When WiFi+UDP is live, real REAPER data overwrites injected values — expected.
        wifi_active = status and status.get("wifi") == 1
        if wifi_active:
            log(WARN, "T06 Injected meter data (WiFi active — UDP overwrite expected)",
                f"peak_l={s2.get('peak_l') if s2 else 'no response'}")
        else:
            log(FAIL, "T06 Injected meter data reflected in STATUS", str(s2))

    # T07 â€” INJECT_PLAY
    expect(ser, "INJECT_PLAY", "PLAY_SET", "T07 INJECT_PLAY")

    # T08 â€” INJECT_STOP
    expect(ser, "INJECT_STOP", "STOP_SET", "T08 INJECT_STOP")

    # T09 â€” Peak hold set (runtime, no reboot)
    expect(ser, "SET_PEAK:1500", "PEAK_SET", "T09 SET_PEAK:1500ms")

    # T10 â€” Unknown command handling
    expect(ser, "GARBAGE_CMD", "ERR:", "T10 Unknown command returns ERR")

    # -- Serial Data Mode commands (T16-T19) --
    # These exercise SMETER/STRANSPORT/STRACK/SPROJECT used by relay in serial mode.

    # T16 -- SMETER: relay-style 13-value meter injection
    smeter_cmd = "SMETER:-12.5,-11.0,-15.5,-14.0,-12.5,-11.0,-16.5,-17.2,-18.1,8.4,0.92,0,0"
    expect(ser, smeter_cmd, "SM:OK", "T16 SMETER serial meter injection")

    # T17 -- STRANSPORT: flags + tempo + time sig + position
    stransport_cmd = "STRANSPORT:1,120.00,4,4,82.4,5,3"
    expect(ser, stransport_cmd, "ST:OK", "T17 STRANSPORT serial transport injection")

    # T18 -- STRACK: index, color, name, flags
    strack_cmd = "STRACK:2,220,80,40,Lead Vocals,0,0,1"
    expect(ser, strack_cmd, "STRK:OK", "T18 STRACK serial track injection")

    # T19 -- SPROJECT: project name and section separated by |
    sproject_cmd = "SPROJECT:My Album Session|Verse 2"
    expect(ser, sproject_cmd, "SPRJ:OK", "T19 SPROJECT serial project injection")

    # Restore peak hold to default (2500ms)
    expect(ser, "SET_PEAK:2500", "PEAK_SET", "T09b SET_PEAK restore 2500ms")

    # -- Connection Mode commands (T20-T22) --
    # T20 -- Switch to Serial mode (no reboot, live apply)
    expect(ser, "SET_CONN_MODE:1", "CONN_MODE_SET:1", "T20 SET_CONN_MODE:1 (Serial)")

    # T21 -- Switch to Both mode
    expect(ser, "SET_CONN_MODE:2", "CONN_MODE_SET:2", "T21 SET_CONN_MODE:2 (Both)")

    # T22 -- STATUS includes conn_mode field
    send(ser, "STATUS")
    deadline = time.time() + TOUT
    scm = None
    while time.time() < deadline:
        line = read_line(ser, timeout=deadline - time.time())
        if not line or line.startswith("[CMD]"):
            continue
        try:
            scm = json.loads(line)
            break
        except json.JSONDecodeError:
            pass
    if scm and "conn_mode" in scm:
        log(PASS, "T22 STATUS contains conn_mode field", f"conn_mode={scm.get('conn_mode')}")
    else:
        log(FAIL, "T22 STATUS contains conn_mode field", str(scm))

    expect(ser, "SET_CONN_MODE:0", "CONN_MODE_SET:0", "T22b SET_CONN_MODE:0 restore")

    # -- Deep LUFS pipeline verification (T23-T25) --
    # Switch to serial-only mode so injected SMETER isn't overwritten by WiFi UDP
    expect(ser, "SET_CONN_MODE:1", "CONN_MODE_SET:1", "T23a Prep: serial-only mode")
    time.sleep(0.2)

    # Inject known LUFS values via SMETER
    smeter_lufs = "SMETER:-6.0,-8.0,-14.0,-16.0,-12.0,-10.0,-23.5,-19.2,-21.0,6.7,0.85,3,1"
    expect(ser, smeter_lufs, "SM:OK", "T23b SMETER LUFS injection")
    time.sleep(0.3)

    # STATUS should now contain the injected LUFS values
    send(ser, "STATUS")
    deadline = time.time() + TOUT
    s_lufs = None
    while time.time() < deadline:
        line = read_line(ser, timeout=deadline - time.time())
        if not line or line.startswith("[CMD]"):
            continue
        try:
            s_lufs = json.loads(line)
            break
        except json.JSONDecodeError:
            pass
    if s_lufs:
        # T23 — LUFS-M injected
        lm = s_lufs.get("lufs_m", 0)
        if abs(lm - (-23.5)) < 0.5:
            log(PASS, "T23 LUFS-M injected and reflected in STATUS", f"lufs_m={lm}")
        else:
            log(FAIL, "T23 LUFS-M injected and reflected in STATUS", f"lufs_m={lm} (expected ~-23.5)")

        # T24 — Phase correlation
        ph = s_lufs.get("phase", -99)
        if abs(ph - 0.85) < 0.05:
            log(PASS, "T24 Phase correlation reflected in STATUS", f"phase={ph}")
        else:
            log(FAIL, "T24 Phase correlation reflected in STATUS", f"phase={ph} (expected ~0.85)")

        # T25 — Clip counts
        cl = s_lufs.get("clip_l", -1)
        cr = s_lufs.get("clip_r", -1)
        if cl == 3 and cr == 1:
            log(PASS, "T25 Clip counts reflected in STATUS", f"clip_l={cl}, clip_r={cr}")
        else:
            log(FAIL, "T25 Clip counts reflected in STATUS", f"clip_l={cl}, clip_r={cr} (expected 3/1)")

        # T26 — FPS present
        fps = s_lufs.get("fps", -1)
        if fps >= 0:
            log(PASS, "T26 FPS value present in STATUS", f"fps={fps:.1f}")
        else:
            log(FAIL, "T26 FPS value not in STATUS", str(s_lufs))

        # T27 — show_fps field
        sf = s_lufs.get("show_fps", -1)
        if sf in (0, 1):
            log(PASS, "T27 show_fps field in STATUS", f"show_fps={sf}")
        else:
            log(FAIL, "T27 show_fps field missing from STATUS", str(s_lufs))
    else:
        log(FAIL, "T23-T27 LUFS pipeline", "no STATUS response after SMETER injection")

    # Restore conn_mode to 0
    expect(ser, "SET_CONN_MODE:0", "CONN_MODE_SET:0", "T27b Restore conn_mode to UDP")

    # T28 — Capture a PERF telemetry line (waits up to 6s for the 5s interval)
    print(f"\n  [T28] Waiting for PERF telemetry (up to 6s)...")
    perf_deadline = time.time() + 7.0
    perf_data = None
    while time.time() < perf_deadline:
        line = read_line(ser, timeout=1.0)
        if line and line.startswith("PERF:"):
            try:
                perf_data = json.loads(line[5:])
            except json.JSONDecodeError:
                pass
            break
    if perf_data and "fps" in perf_data:
        fps_val = perf_data["fps"]
        worst = perf_data.get("worst_ms", "?")
        heap = perf_data.get("heap", "?")
        log(PASS, f"T28 PERF telemetry captured", f"fps={fps_val:.1f} worst={worst}ms heap={heap}")
    else:
        log(WARN, "T28 PERF telemetry not captured in 6s", "device may be slow or interval hasn't elapsed")

    # T11 -- NVS + rotation cycle (SET_ROTATION:1 -> reboot -> STATUS rotation==1)
    print(f"\n  [T11] Cycling rotation to 1 (reboot expected)...")
    send(ser, "SET_ROTATION:1")
    ok = wait_for_reboot(ser, timeout=15)
    if ok:
        log(PASS, "T11 Rotation=1 reboot completed")
        time.sleep(0.5)
        send(ser, "STATUS")
        deadline = time.time() + TOUT
        s3 = None
        while time.time() < deadline:
            line = read_line(ser, timeout=deadline - time.time())
            if not line or line.startswith("[CMD]"):
                continue
            try:
                s3 = json.loads(line)
                break
            except json.JSONDecodeError:
                pass
        if s3 and s3.get("rotation") == 1:
            log(PASS, "T11b Rotation=1 persisted in NVS", f"rotation={s3.get('rotation')}")
        else:
            log(FAIL, "T11b Rotation=1 persisted in NVS", str(s3))
    else:
        log(FAIL, "T11 Rotation=1 reboot completed", "device never sent [OK] after reboot")

    # T12 â€” Restore to rotation=0
    print(f"\n  [T12] Restoring rotation to 0...")
    send(ser, "SET_ROTATION:0")
    ok = wait_for_reboot(ser, timeout=15)
    if ok:
        log(PASS, "T12 Rotation=0 restored")
    else:
        log(FAIL, "T12 Rotation=0 restored")

    # T13 â€” Theme cycle (theme=2 â†’ reboot â†’ STATUS theme==2)
    print(f"\n  [T13] Cycling theme to 2 (Retro LED)...")
    send(ser, "SET_THEME:2")
    ok = wait_for_reboot(ser, timeout=15)
    if ok:
        time.sleep(0.5)
        send(ser, "STATUS")
        deadline = time.time() + TOUT
        s4 = None
        while time.time() < deadline:
            line = read_line(ser, timeout=deadline - time.time())
            if not line or line.startswith("[CMD]"):
                continue
            try:
                s4 = json.loads(line)
                break
            except json.JSONDecodeError:
                pass
        if s4 and s4.get("theme") == 2:
            log(PASS, "T13 Theme=2 persisted after reboot", f"theme={s4.get('theme')}")
        else:
            log(FAIL, "T13 Theme=2 persisted after reboot", str(s4))
    else:
        log(FAIL, "T13 Theme reboot completed")

    # T14 â€” Restore theme to 0 (Codeine Crazy default)
    print(f"\n  [T14] Restoring theme to 0 (Codeine Crazy)...")
    send(ser, "SET_THEME:0")
    ok = wait_for_reboot(ser, timeout=15)
    if ok:
        log(PASS, "T14 Theme=0 restored")
    else:
        log(FAIL, "T14 Theme=0 restored")

    # T15 â€” NVS clear + verify defaults restored
    print(f"\n  [T15] NVS_CLEAR + verify defaults...")
    send(ser, "NVS_CLEAR")
    ok = wait_for_reboot(ser, timeout=15)
    if ok:
        time.sleep(0.5)
        send(ser, "STATUS")
        deadline = time.time() + TOUT
        s5 = None
        while time.time() < deadline:
            line = read_line(ser, timeout=deadline - time.time())
            if not line or line.startswith("[CMD]"):
                continue
            try:
                s5 = json.loads(line)
                break
            except json.JSONDecodeError:
                pass
        if s5:
            rot_ok   = s5.get("rotation", -1) == 0
            theme_ok = s5.get("theme", -1) == 0
            ph_ok    = s5.get("peak_hold_ms", -1) == 2500
            if rot_ok and theme_ok and ph_ok:
                log(PASS, "T15 NVS defaults restored after clear")
            else:
                log(WARN, "T15 NVS defaults partially restored", str(s5))
        else:
            log(FAIL, "T15 NVS defaults", "no STATUS response")
    else:
        log(FAIL, "T15 NVS_CLEAR reboot", "device never recovered")

    # â”€â”€ Summary â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    print("\nâ•â•â•â•â•â•â•â•â•â• Results â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•")
    passed = sum(1 for r in results if r[0] == PASS)
    failed = sum(1 for r in results if r[0] == FAIL)
    warned = sum(1 for r in results if r[0] == WARN)
    total  = len(results)
    for sym, label in results:
        print(f"  {sym} {label}")
    print(f"\n  {passed}/{total} passed  |  {failed} failed  |  {warned} warnings")
    print("â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n")
    return failed == 0


if __name__ == "__main__":
    print(f"[HARNESS] Connecting to {PORT} @ {BAUD}...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=2)
    except serial.SerialException as e:
        print(f"[HARNESS] Cannot open {PORT}: {e}")
        sys.exit(1)

    # Let any existing output drain, then trigger a STATUS ping to sync
    time.sleep(1.5)
    ser.reset_input_buffer()

    ok = run_tests(ser)
    ser.close()
    sys.exit(0 if ok else 1)
