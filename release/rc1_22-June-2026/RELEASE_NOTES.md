# MeterBridge v2.0 — Release Candidate 1
**Date:** June 22, 2026  
**Publisher:** FalconEYE Software Dev  
**Hardware:** [Elecrow CrowPanel 7" ESP32-S3 HMI Display](https://www.amazon.com/ELECROW-ESP32-Display-Compatible-MicroPython/dp/B0G1RYBZNL/)  
**Repository:** https://github.com/s0wingseason/ESP32-Touch-Display-REAPER-Advanced-Metering-and-Transport-Controller  

---

## What is MeterBridge?

MeterBridge turns a $40 ESP32 touch display into a dedicated, always-on hardware metering surface for REAPER DAW. Real-time LUFS, RMS, True Peak, 16-band spectrum analysis, phase correlation, and full transport control — living outside your DAW window on a 7" touchscreen.

---

## What's New in v2.0 RC1

### Mastering-Grade Metering
- **Session MAX True Peak Tracker** — Persistent L+R True Peak maximum across the session, color-coded (green → yellow → red → white). Long-press CLIPS label to reset.
- **Scrolling LUFS-M History Graph** — 30-second scrolling bar graph (120 samples @ 250ms) with target zone visualization in the landscape sidebar.
- **dBFS Reference Marker Lines** — Blue –18 dBFS reference lines rendered on all meter bars in both orientations.
- **Stereo Balance Indicator** — 6px visual balance bar (RMS-based L vs R) below the landscape meters. Color shifts green → amber → red as imbalance grows.
- **Beat Pulse Indicator** — Teal dot flashes 80ms on each new beat-in-bar from REAPER for transport reference.

### Power Management
- **Auto-Dim Screensaver** — Opt-in (non-default). Dims display to 15% after 5 minutes of signal idle, blacks out after 10 minutes. Wakes on incoming REAPER data or remote touch. Persisted in NVS.

### Stability & Performance
- Removed 33ms hard gate from main loop — LVGL now renders at native panel refresh rate (60–120+ FPS).
- All new widgets use dirty-bit optimization — only redraws when values actually change.
- LUFS graph uses PSRAM-allocated canvas buffer, keeping the main LVGL heap free.
- Core isolation maintained: network/remote on Core 0, all UI/LVGL on Core 1.
- Fixed: auto-dim does not immediately trigger on first enable after long uptime.
- Fixed: brightness correctly restored when auto-dim is disabled while screen is dimmed.

### Remote Control
- HTTP REST API on port 8080 for scripted control (Play/Stop/Rec/Rew/Fwd, UI navigation, screenshots).
- Built-in HTML remote panel served from the ESP32 — open in any browser on the same network.

### Documentation
- Full HTML landing page (`docs/index.html`) with features, install guide, and troubleshooting.
- Hardware requirements updated with direct Amazon link to the recommended Elecrow CrowPanel.

---

## Hardware Requirements

| Item | Detail |
|------|--------|
| **Display** | [Elecrow CrowPanel 7" ESP32-S3 HMI](https://www.amazon.com/ELECROW-ESP32-Display-Compatible-MicroPython/dp/B0G1RYBZNL/) — **recommended, tested device** |
| **Compatible** | Any ESP32-S3 + 7" RGB TFT (800×480) with GT911 capacitive touch |
| **Connection** | WiFi 2.4 GHz (UDP, port 4210) or USB Serial |
| **Power** | 5V / 1A via USB-C |
| **Approx. cost** | ~$35–50 USD |

---

## Software Requirements

- **REAPER DAW** (any recent version, v6+ recommended)
- **Python 3.8+** — for the relay bridge
- **Windows 10/11** — primary platform (macOS/Linux with adjustments)
- **SWS Extension for REAPER** — recommended for marker navigation

---

## Installation (Quick Start)

### Option A — Use the Installer (recommended)
1. Run `MeterBridge_v2.0-RC1_Setup.exe`
2. Follow the wizard — select components as needed
3. After install, run **Install REAPER Bridge Scripts** from the Start Menu
4. Run **Run MeterBridge Relay** from the Start Menu or Desktop shortcut
5. Flash the firmware (see below)
6. In REAPER, load the Lua bridge script and add the JSFX plugin to the master bus

### Option B — Standalone (no install required)
1. Extract `MeterBridge_v2.0-RC1_Standalone.zip` anywhere
2. Double-click `run_relay.bat` to start the relay
3. Double-click `install_reaper_bridge.bat` to copy scripts into REAPER

---

## Flashing the Firmware

1. Connect your ESP32 via USB-C
2. Double-click `flash_esp32.bat` (in the install folder or standalone zip)  
   — or use PlatformIO: `pio run -e crowpanel_7inch --target upload`
3. On first boot, the display shows a **MeterBridge-Setup** WiFi AP — connect to it and enter your network credentials

---

## Remote Control API

The ESP32 serves an HTTP REST API on port 8080:

| Endpoint | Action |
|----------|--------|
| `GET /play` | Transport: Play |
| `GET /stop` | Transport: Stop |
| `GET /record` | Transport: Record |
| `GET /rewind` | Transport: Rewind to start |
| `GET /forward` | Transport: Jump forward |
| `GET /status` | JSON status report |
| `GET /screenshot` | Live BMP screenshot (port 8099) |

Open `http://<ESP32-IP>:8080/panel` in any browser for the built-in HTML remote panel.

---

## Known Limitations (RC1)

- OTA firmware update is experimental — use USB flash for reliability
- Auto-dim wake-on-touch: hardware touch resumes automatically on next REAPER data packet; remote-control touch is immediate
- LUFS history graph is portrait-mode only on small sidebar heights (space-conditional)
- ESP32 WiFi supports 2.4 GHz only — 5 GHz networks will not appear

---

## Files in This Release

| File | Description |
|------|-------------|
| `MeterBridge_v2.0-RC1_Setup.exe` | Windows installer — full component selection |
| `MeterBridge_v2.0-RC1_Standalone.zip` | Portable/no-install version |
| `meterbridge_firmware_rc1.bin` | ESP32 firmware binary (flash separately) |
| `bootloader.bin` | ESP32 bootloader |
| `partitions.bin` | ESP32 partition table |
| `meterbridge_reaper_bridge.lua` | REAPER Lua bridge script |
| `meterbridge_spectrum.jsfx` | REAPER JSFX spectrum analyzer plugin |
| `RELEASE_NOTES.md` | This file |

---

## License

Copyright © 2026 FalconEYE Software Dev. All rights reserved.  
Source code available at the repository URL above.

---

*Built with ❤️ on an Elecrow CrowPanel ESP32-S3 7" display, PlatformIO, LVGL 8.3, REAPER + JSFX.*
