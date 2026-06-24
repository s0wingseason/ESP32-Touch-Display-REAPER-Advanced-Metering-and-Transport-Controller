# StudioBeacon Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.3.0] - 2026-01-20

### Added
- **🌀 FanControl Integration (Recording Mode)**
  - Automatically reduces PC fan speeds during recording to minimize noise
  - **OPT-IN ONLY** - must be explicitly enabled by user
  - Uses FanControl's CLI (`-c config.json`) to switch between configurations
  - Requires FanControl app from [getfancontrol.com](https://getfancontrol.com)
  
- **Configuration Switching**
  - Switches to `StudioBeacon_Recording` config when recording starts
  - Restores `StudioBeacon_Normal` config when recording stops
  - Configurable fan speed (10% - 50%, default 25%)
  - Debounce to prevent rapid switching (1 second default)
  
- **Settings Panel UI**
  - New "Fan Control (Recording Mode)" section
  - Enable/disable toggle
  - Fan speed selector
  - Restore on exit option
  - Test connection button with config status check

- **Transport State Hooks**
  - FanControl and Govee services now receive transport change notifications
  - Unified event dispatch for all integrations

### New Files
- `src/fancontrol/fancontrol-service.js` - FanControl integration service

### How to Use
1. Install FanControl from [getfancontrol.com](https://getfancontrol.com)
2. Create two configurations in FanControl:
   - `StudioBeacon_Recording` - Set all fans to 25% or desired quiet level
   - `StudioBeacon_Normal` - Your normal/automatic fan settings
3. In StudioBeacon Settings → Fan Control, enable the feature
4. Start recording - fans will automatically reduce speed
5. Stop recording - fans restore to normal

---

## [1.2.1] - 2026-01-17

### Added
- **Windows Registry Fallback Storage**
  - API key and settings now backed up to Windows Registry (`HKCU\Software\StudioBeacon`)
  - Automatic recovery if config files are lost or corrupted
  - User manual configuration updates supersede Registry fallback data

- **Device Type Categorization**
  - Smart detection of device types by SKU prefix
  - Light devices: H6xxx, H7xxx, H8xxx, H0xxx (bulbs, strips, bars)
  - Thermostat devices: H5xxx, H4xxx
  - Sensor devices: H3xxx
  - Appliance devices: H2xxx, H1xxx

- **Expanded Govee Device Support**
  - Support for thermostats with configurable temperature adjustment during recording
  - Motion sensor trigger support (future)
  - Smart appliance detection

### Fixed
- All Govee service methods now properly async/await
- API key storage and retrieval now uses Registry as fallback
- Configuration saving now properly awaits completion

### New Files
- `src/utils/registry-helper.js` - Windows Registry helper for fallback storage

---

## [1.2.0] - 2026-01-17

### Added
- **💡 Govee Smart Lighting Integration**
  - Automatic lighting changes based on studio status (Recording → Red, Playing → Green, etc.)
  - Full Govee API support for all Wi-Fi enabled devices (bulbs, strips, light bars, etc.)
  - Secure encrypted API key storage using Electron's safeStorage
  - Device discovery - automatically detects all Govee devices on your account
  - Per-device selection - choose which lights to control
  - 25+ status-to-color mappings with customizable profiles
  - Rate limiting and request queuing to respect Govee API limits

- **Transition Effects**
  - Configurable fade timing: Instant, 0.25s, 0.5s, 1s, 1.5s, 3s (default), 5s, 10s, 15s
  - Smooth color transitions for professional studio atmosphere

- **State Restoration**
  - Captures device states when StudioBeacon starts
  - Restores previous lighting when StudioBeacon exits
  - Optional toggle to enable/disable restore behavior

- **Settings Panel UI**
  - New "Smart Lighting (Govee)" section in embedded settings
  - API key input (masked for security)
  - Connect & Discover Devices button
  - Device checklist for selecting which lights to control
  - Transition time dropdown
  - Test buttons (Red, Green, White) for immediate preview

### New Files
- `src/govee/govee-api.js` - Govee REST API client with rate limiting
- `src/govee/govee-service.js` - Service manager for lifecycle and config

### Status-to-Color Mappings
| Status Category | Color | Statuses |
|-----------------|-------|----------|
| Recording | 🔴 Red | recording, arming, bouncing, rendering |
| Playback | 🟢 Green | playing, comping, mixing, mastering, reviewing |
| Monitoring | 🔵 Blue | monitoring, soundcheck, calibrating |
| Away | 🟠 Amber | oncall, away, lunch, meeting |
| Talent | 🟣 Purple | talentready, talentinbooth |
| Session | 🔵 Cyan | sessionending, sessioncomplete |
| Warning | 🔴 Magenta | technicalissue, donotenter, quiet |
| Available | ⚪ White | stopped, paused, available |

---

## [1.1.0] - 2026-01-17

### Added
- **Meter Panel (Side Panel)** - Real-time audio metering display
  - Master bus L/R peak meters with dB readout
  - Armed track meters (up to 8 tracks) with track names
  - Sample rate and track count footer
  - 280px width, optimized for small displays
  
- **New Metadata Fields**
  - Current Marker - shows active marker name (Verse, Chorus, etc.)
  - Take Count - shows "Take X" when recording on selected item
  - Click Status - shows metronome ON/OFF state
  - Recording Duration - tracks how long current recording has been going
  
- **REAPER Lua Bridge Enhancements (v3.3.0)**
  - Master bus metering (L/R peak in dB)
  - Armed track metering for up to 8 tracks
  - Track counts (total/armed/muted/soloed)
  - Sample rate and project length
  - Master volume in dB
  - Current/next marker detection
  - Take count for selected items
  - Click/metronome status
  - Recording duration tracking
  - Console diagnostics on startup
  
- **START.bat Enhancements**
  - Now syncs Lua script to `%APPDATA%\REAPER\Scripts\` automatically
  - Also copies installer folder
  - Shows [OK] confirmation for REAPER bridge install
  
- **Installer Improvements**
  - Added `[Dirs]` section to create REAPER Scripts folder if missing
  - Script also copied to `{app}\scripts\` for manual install reference
  - Uses `uninsneveruninstall` flag to preserve REAPER folders on uninstall

### Changed
- **Display Layout Optimized for Small Screens (7-24 inch displays)**
  - Status indicator: 200px → 140px
  - Meter panel width: 350px → 280px
  - Meter height: 350px → 280px
  - Container padding: 60px → 30px
  - Container width accounts for meter panel space
  
- **Window Controls**
  - Increased button size by 200% (44px → 72px)
  - Brighter borders and enhanced hover effects
  - Better visibility on small displays
  
- **Embedded Settings Panel**
  - Fixed S key and gear button to toggle embedded overlay
  - Added mouse cursor visibility toggle
  - Settings now appear within main window, not separate window
  
- **Keyboard Controls**
  - Implemented multi-press cycling for status groups
  - Press `1` multiple times: Recording → Arming → Bouncing → Rendering
  - 800ms timeout to reset cycle

### Fixed
- **Lua Script Errors**
  - Removed invalid `reaper.GetPeaksBitmap` call (doesn't exist in Lua API)
  - Fixed JSON encoder forward-reference issue with `arrayToJSON`/`toJSON`
  - Removed stuck "running" lock that prevented script restart
  
- **Settings Panel**
  - Removed old `openSettings` IPC from preload.js that was opening separate window
  
- **File Watcher**
  - Added change detection to prevent duplicate state updates
  - Added null check for full-state handler
  
- **CSS**
  - Fixed duplicate closing brace lint error in meter panel styles

### Removed
- Separate settings window (replaced with embedded overlay)
- `openSettings` IPC handler (no longer needed)

---

## [1.0.0] - 2025-12-30

### Added
- Initial release
- Status display with transport states (Recording, Playing, Paused, Stopped)
- 25+ manual status override options
- REAPER integration via Lua bridge script
- WebSocket server for VST communication
- File-based state monitoring fallback
- System tray with context menu
- Keyboard shortcuts for quick status changes
- Particle effect background
- Session timer
- Date/time display
- Customizable themes and typography
- Multi-monitor support
- Inno Setup installer

---

## REAPER Lua Bridge Versions

| Version | Date | Changes |
|---------|------|---------|
| 3.3.0 | 2026-01-14 | Removed running lock, added console diagnostics |
| 3.2.0 | 2026-01-14 | Removed invalid GetPeaksBitmap call |
| 3.1.0 | 2026-01-11 | Fixed JSON encoder forward-reference |
| 3.0.0 | 2026-01-11 | Added metering, track counts, new metadata |
| 2.0.0 | 2025-12-30 | Added OSC support, auto-start capability |
| 1.0.0 | 2025-12-29 | Initial version with transport/metadata |
