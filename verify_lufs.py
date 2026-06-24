"""Quick LUFS verification — reads STATUS and prints LUFS fields."""
import serial, time, json
s = serial.Serial('COM3', 115200, timeout=2)
time.sleep(2)
s.reset_input_buffer()
s.write(b'STATUS\n')
s.flush()
deadline = time.time() + 5
while time.time() < deadline:
    line = s.readline().decode(errors='replace').strip()
    if not line or line.startswith('[CMD]'):
        continue
    try:
        d = json.loads(line)
        print(f'=== LUFS END-TO-END VERIFICATION ===')
        print(f'  lufs_m  = {d.get("lufs_m")}')
        print(f'  lufs_s  = {d.get("lufs_s")}')
        print(f'  lufs_i  = {d.get("lufs_i")}')
        print(f'  lufs_r  = {d.get("lufs_r")}')
        print(f'  phase   = {d.get("phase")}')
        print(f'  clip_l  = {d.get("clip_l")}')
        print(f'  clip_r  = {d.get("clip_r")}')
        print(f'  fps     = {d.get("fps")}')
        print(f'  peak_l  = {d.get("peak_l")}')
        print(f'  playing = {d.get("playing")}')
        lm = d.get("lufs_m", -70)
        if lm > -69:
            print(f'\n  ✅ LUFS-M is LIVE data: {lm} (not default -70)')
        else:
            print(f'\n  ❌ LUFS-M is still default: {lm}')
        break
    except json.JSONDecodeError:
        pass
s.close()
