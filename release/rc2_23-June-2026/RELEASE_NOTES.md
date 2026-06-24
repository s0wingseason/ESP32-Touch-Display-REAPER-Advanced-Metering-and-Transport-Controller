# MeterBridge v2.0 — Release Candidate 2
**Date:** June 23, 2026  
**Publisher:** FalconEYE Software Dev  
**Hardware:** [Elecrow CrowPanel 7" ESP32-S3 HMI Display](https://www.amazon.com/ELECROW-ESP32-Display-Compatible-MicroPython/dp/B0G1RYBZNL/)  
**Repository:** https://github.com/s0wingseason/ESP32-Touch-Display-REAPER-Advanced-Metering-and-Transport-Controller  

---

## What is MeterBridge?

MeterBridge turns a $40 ESP32 touch display into a dedicated, always-on hardware metering surface for REAPER DAW. Real-time LUFS, RMS, True Peak, 16-band spectrum analysis, phase correlation, and full transport control — living outside your DAW window on a 7" touchscreen.

---

## What's New in RC2 (since RC1)

### Installer Hardening
- **Fixed:** `build_installer.bat` crashed immediately with `... was unexpected at this time` — caused by a non-ASCII em-dash in the `title` command and illegal `goto` statements inside nested parenthesized blocks. Fully rewritten with proper batch scripting conventions.
- **Fixed:** Installer correctly finds Inno Setup 6 in `%LOCALAPPDATA%\Programs` (user installs without admin rights).
- **Fixed:** `goto :DONE` inside nested `if errorlevel` blocks replaced with errorlevel variable capture (`set "ERR=!errorlevel!"`) for standard-compliant execution flow.
- **Fixed:** esptool bundling now copies `.pyd` and `.dll` dependencies separately (two `for` loops) to avoid wildcard expansion issues.

### Pascal Script (Inno Setup) Hardening
- **Removed:** `TOutputMsgWizardPage` / `SummaryPage` — this page type does not support `Values[0]` or `.RichEditViewer` in all Inno Setup 6 builds. Replaced with a reliable post-install `MsgBox()` summary.
- **Removed:** Non-standard `SummaryPage: TOutputMsgWizardPage` variable declaration.
- **Removed:** `CurPageChanged` procedure that populated the now-removed summary page.
- **Fixed:** All JSON command injection now uses compact (`{"cmd":"INJECT_PLAY"}`) format — the ESP32's minimal parser requires no whitespace in key-value pairs.

### Quality Assurance
- Live field QA performed on physical device (`192.168.1.180`):
  - Signal injection test: Peak L=−3 dB, Peak R=−10 dB — meters responded correctly.
  - Stress test: +6.0 dB overscale, 99 clip count triggered — no crash or visual glitch.
  - Transport state injection (PLAY/STOP) verified via remote API.
  - FPS confirmed stable at ~700–796 FPS during all tests.
  - PSRAM free heap: 6.3–7.1 MB throughout — no memory leak observed.

### Data Sanitization (RC2 Release Folder)
- All intermediate build logs (`iss_out*.txt`, `iss_err*.txt`, `build_installer_log.txt`) excluded from the release package.
- Scratch screenshots (`esp32_*.bmp`, `esp32_*.png`, `qatest_*.png`) excluded.
- JVM crash logs (`hs_err_pid*.log`, `replay_pid*.log`) excluded.
- Temp scripts (`tmp_convert.ps1`) excluded.
- Only production-ready files shipped; see **Files in This Release** below.

### Known Limitations Resolved (vs RC1)
- ✅ `build_installer.bat` now runs reliably from a plain double-click in File Explorer.
- ✅ ISCC compilation is clean (Exit 0) with no Pascal Script errors.

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
1. Run `MeterBridge_v2.0-RC2_Setup.exe`
2. Follow the wizard — select components as needed
3. After install, run **Install REAPER Bridge Scripts** from the Start Menu
4. Run **Run MeterBridge Relay** from the Start Menu or Desktop shortcut
5. Flash the firmware (see below)
6. In REAPER, load the Lua bridge script and add the JSFX plugin to the master bus

### Option B — Standalone (no install required)
1. Extract `MeterBridge_v2.0-RC2_Standalone.zip` anywhere
2. Double-click `run_relay.bat` to start the relay
3. Double-click `install_reaper_bridge.bat` to copy scripts into REAPER

---

## Flashing the Firmware

1. Connect your ESP32 via USB-C
2. Double-click `flash_firmware.bat` (in the install folder or standalone zip)  
   — or use PlatformIO: `pio run -e crowpanel_7inch --target upload`
3. On first boot, the display shows a **MeterBridge-Setup** WiFi AP — connect to it and enter your network credentials

---

## Remote Control API

The ESP32 serves an HTTP REST API on port 8080:

| Endpoint | Method | Action |
|----------|--------|--------|
| `/status` | GET | JSON device status + current meter values |
| `/tap` | POST | Inject a touch tap at `{"x":400,"y":240}` |
| `/swipe` | POST | Inject a swipe gesture |
| `/press` | POST | Inject a long-press |
| `/inject_meter` | POST | Push meter values directly to display |
| `/cmd` | POST | Send transport commands: `INJECT_PLAY`, `INJECT_STOP`, `REBOOT`, `PING` |

Screenshots served on port 8099 (`/screenshot` → BMP).

---

## Known Limitations (RC2)

- OTA firmware update is functional but experimental — USB flash preferred for initial setup
- Auto-dim wake-on-touch: hardware touch resumes automatically on next REAPER data packet; remote-control touch is immediate
- LUFS history graph is portrait-mode only on small sidebar heights (space-conditional)
- ESP32 WiFi supports 2.4 GHz only — 5 GHz networks will not appear
- Vertical meter bar dB value labels (`-inf`) may appear truncated at narrowest bar width setting — cosmetic only, fix targeted for v2.1

---

## Files in This Release

| File | Description |
|------|-------------|
| `MeterBridge_v2.0-RC2_Setup.exe` | Windows installer — full component selection |
| `MeterBridge_v2.0-RC2_Standalone.zip` | Portable/no-install version |
| `firmware/meterbridge_firmware_rc2.bin` | ESP32 firmware binary |
| `firmware/bootloader.bin` | ESP32 bootloader |
| `firmware/partitions.bin` | ESP32 partition table |
| `reaper/meterbridge_reaper_bridge.lua` | REAPER Lua bridge script |
| `reaper/meterbridge_spectrum.jsfx` | REAPER JSFX spectrum analyzer plugin |
| `RELEASE_NOTES.md` | This file |

---

## Upgrading from RC1

No breaking changes. Simply:
1. Uninstall RC1 (via Add/Remove Programs or the RC1 uninstaller)
2. Run the RC2 installer
3. Re-flash the firmware from `firmware/meterbridge_firmware_rc2.bin`

---

## License

Copyright © 2026 FalconEYE Software Dev. All rights reserved.  
Source code available at the repository URL above.

---

*Built with ❤️ on an Elecrow CrowPanel ESP32-S3 7" display, PlatformIO, LVGL 8.3, REAPER + JSFX.*
