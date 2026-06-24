# StudioBeacon

**A modern "Recording In Progress" display for studio external monitors**

![Status: Development](https://img.shields.io/badge/Status-Development-orange)

---

## Quick Start

### Prerequisites
- [Node.js](https://nodejs.org/) (v18 or later)
- Windows 10/11
- REAPER (for DAW integration)

### Run the App

1. **Double-click `START.bat`**
2. Wait for dependencies to install (first run only)
3. The display will open on your selected monitor

That's it!

---

## Features

- **Live DAW Status** - Shows Recording/Playing/Paused/Stopped status
- **Manual Overrides** - On Call, Away, Monitoring, Talent Ready, Session Ending
- **Metadata Display** - Project name, timecode, bars/beats, tempo, regions
- **Modern UI** - Dark theme with glow effects and particle animations
- **Keyboard Shortcuts** - Press 1-6 for quick status changes, S for settings

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `1` | On Call |
| `2` | Away |
| `3` | Monitoring |
| `4` | Talent Ready |
| `5` | Session Ending |
| `6` | Custom Message |
| `S` | Open Settings |
| `F` | Toggle Fullscreen |
| `ESC` | Clear Status |

---

## REAPER Integration

1. Copy `scripts/StudioBeacon_REAPER_Bridge.lua` to your REAPER Scripts folder
2. In REAPER: Actions → Load ReaScript → Select the file
3. Run the "StudioBeacon" action
4. The display will now show live transport state

---

## File Structure

```
StudioBeacon/
├── START.bat          ← Double-click to run
├── main.js            ← Electron main process
├── preload.js         ← IPC bridge
├── package.json       ← Dependencies
├── src/
│   ├── display/       ← Main display UI
│   └── settings/      ← Settings panel
├── scripts/           ← REAPER Lua scripts
└── assets/            ← Icons and images
```

---

## Troubleshooting

**"npm is not recognized"**
→ Install Node.js from https://nodejs.org

**Display won't start**
→ Delete the `C:\StudioBeacon` folder and try again

**REAPER script not connecting**
→ Make sure StudioBeacon app is running first

---

## License

MIT License - Falcon Studios
