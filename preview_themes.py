"""
MeterBridge — Theme Preview Script
Cycles all 5 themes on the physical CrowPanel display via USB serial.
Each theme boots for ~5 seconds so you can see it, then moves to the next.
Restores theme 0 (Codeine Crazy) at the end.

Usage: python preview_themes.py [COM_PORT]
"""

import sys
import serial
import time

PORT    = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD    = 115200
THEMES  = [
    (0, "Codeine Crazy  — Dark purple, magenta accents"),
    (1, "Pro Broadcast  — Clean dark, professional broadcast look"),
    (2, "Retro LED      — Green phosphor on black, vintage VU"),
    (3, "Neon Synth     — Cyberpunk cyan/pink on dark"),
    (4, "VU Classic     — Warm amber needles, analogue warmth"),
]
HOLD_SECS = 6   # seconds to display each theme

def wait_for_boot(ser, timeout=18):
    """Wait until the device prints its ready banner after a reboot."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            line = ser.readline().decode("utf-8", errors="replace").strip()
        except Exception:
            line = ""
        if line:
            print(f"    > {line}")
        if "[OK]" in line or "MeterBridge" in line and "ready" in line:
            return True
    return False

def set_theme(ser, theme_id):
    cmd = f"SET_THEME:{theme_id}\n".encode()
    ser.write(cmd)
    ser.flush()

print(f"\n{'='*56}")
print(f"  MeterBridge Theme Preview — {PORT}")
print(f"{'='*56}")
print(f"  Each theme will show for {HOLD_SECS} seconds.")
print(f"  Watch your CrowPanel screen!\n")

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(0.5)
    ser.reset_input_buffer()
except Exception as e:
    print(f"[ERROR] Cannot open {PORT}: {e}")
    sys.exit(1)

try:
    for tid, desc in THEMES:
        print(f"\n[Theme {tid}] {desc}")
        print(f"  Sending SET_THEME:{tid} — rebooting...")
        set_theme(ser, tid)
        booted = wait_for_boot(ser, timeout=18)
        if booted:
            print(f"  [OK] Theme {tid} active. Holding for {HOLD_SECS}s...")
        else:
            print(f"  [WARN] Boot confirmation not received. May still be loading.")
        # Hold and drain any serial noise
        deadline = time.time() + HOLD_SECS
        while time.time() < deadline:
            try:
                ser.readline()
            except Exception:
                pass
            time.sleep(0.1)

    # Restore theme 0 (Codeine Crazy) as default
    print(f"\n{'='*56}")
    print(f"  Preview complete. Restoring Theme 0 (Codeine Crazy)...")
    set_theme(ser, 0)
    wait_for_boot(ser, timeout=18)
    print(f"  Done! Device is back on Theme 0.")
    print(f"{'='*56}\n")

finally:
    ser.close()
