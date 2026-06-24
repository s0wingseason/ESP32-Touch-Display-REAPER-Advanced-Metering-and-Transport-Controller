const { app, BrowserWindow, screen, ipcMain, Menu, Tray } = require('electron');
const path = require('path');
const WebSocket = require('ws');
const fs = require('fs');
const os = require('os');

// Govee Smart Lighting Integration
const { GoveeService } = require('./src/govee/govee-service');

// FanControl Integration (reduce fan noise during recording)
const { FanControlService } = require('./src/fancontrol/fancontrol-service');

// Single instance lock - kill previous instances
const gotTheLock = app.requestSingleInstanceLock();
if (!gotTheLock) {
    // Another instance is running, quit this one
    app.quit();
} else {
    // This is the first instance, handle second instance attempts
    app.on('second-instance', (event, commandLine, workingDirectory) => {
        // Someone tried to run a second instance, focus our window instead
        if (mainWindow) {
            if (mainWindow.isMinimized()) mainWindow.restore();
            mainWindow.focus();
        }
    });
}
// Configuration paths
const CONFIG_DIR = path.join(app.getPath('userData'), 'config');
const CONFIG_FILE = path.join(CONFIG_DIR, 'settings.json');

let mainWindow = null;
let settingsWindow = null;
let tray = null;
let wss = null;
let goveeService = null;
let fanControlService = null;
let currentState = {
    transport: 'stopped',
    projectName: 'No Project',
    timecode: '00:00:00:00',
    barsBeats: '1.1.0',
    regionName: '',
    tempo: 120,
    timeSignature: '4/4',
    manualStatus: null,
    customMessage: '',
    // Meter data
    masterMeter: { left: -60, right: -60 },
    armedTrackMeters: [],
    // Additional metadata
    trackCount: 0,
    armedCount: 0,
    sampleRate: 48000
};

// Default settings
const defaultSettings = {
    display: {
        selectedMonitor: 0,
        displayMode: 'fullscreen', // 'fullscreen', 'fullscreen-windowed', 'windowed'
        alwaysOnTop: true
    },
    theme: {
        preset: 'newfront-dark',
        background: '#0d0d12',
        accentPrimary: '#FF4D6D',
        accentSecondary: '#4DEDAB',
        textPrimary: '#FFFFFF',
        textSecondary: '#8B8BA3'
    },
    typography: {
        fontFamily: 'Inter',
        statusSize: 140,
        metadataSize: 28,
        fontWeight: 600
    },
    effects: {
        glowIntensity: 0.8,
        animationSpeed: 1.0,
        particlesEnabled: true
    },
    visibility: {
        // Time/Position
        timecode: true,
        barsBeats: true,
        tempo: false,
        timeSignature: false,
        // Project Info
        projectName: true,
        regionName: true,
        markerName: false,
        trackName: false,
        // Session Info
        dateTime: true,
        sessionTimer: true,
        sampleRate: false,
        bitDepth: false,
        // Studio Info
        studioName: true,
        engineerName: false
    },
    studio: {
        name: 'Falcon Studios',
        address: '',
        phone: '',
        website: '',
        email: '',
        logoPath: '',
        logoAlignment: 'bottom-right',
        logoOpacity: 0.5,
        engineer: ''
    },
    network: {
        port: 9999
    }
};

let settings = { ...defaultSettings };

// Load settings from file
function loadSettings() {
    try {
        if (!fs.existsSync(CONFIG_DIR)) {
            fs.mkdirSync(CONFIG_DIR, { recursive: true });
        }
        if (fs.existsSync(CONFIG_FILE)) {
            const data = fs.readFileSync(CONFIG_FILE, 'utf-8');
            settings = { ...defaultSettings, ...JSON.parse(data) };
        } else {
            saveSettings();
        }
    } catch (err) {
        console.error('Error loading settings:', err);
        settings = { ...defaultSettings };
    }
}

// Save settings to file
function saveSettings() {
    try {
        if (!fs.existsSync(CONFIG_DIR)) {
            fs.mkdirSync(CONFIG_DIR, { recursive: true });
        }
        fs.writeFileSync(CONFIG_FILE, JSON.stringify(settings, null, 2));
    } catch (err) {
        console.error('Error saving settings:', err);
    }
}

// Get available monitors
function getMonitors() {
    return screen.getAllDisplays().map((display, index) => ({
        id: index,
        label: `Monitor ${index + 1} (${display.size.width}x${display.size.height})`,
        bounds: display.bounds,
        primary: display.id === screen.getPrimaryDisplay().id
    }));
}

// Create main display window
function createMainWindow() {
    const monitors = getMonitors();
    const selectedMonitor = monitors[settings.display.selectedMonitor] || monitors[0];
    const displayMode = settings.display.displayMode || 'fullscreen';

    // Configure window based on display mode
    const windowConfig = {
        x: selectedMonitor.bounds.x,
        y: selectedMonitor.bounds.y,
        width: selectedMonitor.bounds.width,
        height: selectedMonitor.bounds.height,
        minWidth: selectedMonitor.bounds.width,
        minHeight: selectedMonitor.bounds.height,
        alwaysOnTop: settings.display.alwaysOnTop,
        frame: false,
        transparent: false,
        backgroundColor: settings.theme.background,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false
        }
    };

    // Apply display mode settings
    if (displayMode === 'fullscreen') {
        // True fullscreen - exclusive, no borders, covers taskbar
        windowConfig.fullscreen = true;
        windowConfig.simpleFullscreen = false;
    } else if (displayMode === 'fullscreen-windowed') {
        // Fullscreen windowed - covers screen but allows switching
        windowConfig.fullscreen = false;
        windowConfig.fullscreenable = true;
    } else {
        // Windowed mode - resizable with frame
        windowConfig.fullscreen = false;
        windowConfig.frame = true;
        windowConfig.minWidth = 800;
        windowConfig.minHeight = 600;
    }

    mainWindow = new BrowserWindow(windowConfig);

    mainWindow.loadFile(path.join(__dirname, 'src', 'display', 'index.html'));

    // Send initial state and settings
    mainWindow.webContents.on('did-finish-load', () => {
        if (mainWindow && mainWindow.webContents) {
            mainWindow.webContents.send('settings-update', settings);
            mainWindow.webContents.send('state-update', currentState);
        }
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
}

// Create settings window
function createSettingsWindow() {
    if (settingsWindow) {
        settingsWindow.focus();
        return;
    }

    settingsWindow = new BrowserWindow({
        width: 600,
        height: 700,
        resizable: false,
        frame: true,
        backgroundColor: '#1a1a2e',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false
        }
    });

    settingsWindow.loadFile(path.join(__dirname, 'src', 'settings', 'settings.html'));

    settingsWindow.webContents.on('did-finish-load', () => {
        if (settingsWindow && settingsWindow.webContents) {
            settingsWindow.webContents.send('settings-update', settings);
            settingsWindow.webContents.send('monitors-list', getMonitors());
        }
    });

    settingsWindow.on('closed', () => {
        settingsWindow = null;
    });
}

// Create system tray
function createTray() {
    // Using a simple icon path - will create a placeholder
    tray = new Tray(path.join(__dirname, 'assets', 'tray-icon.png'));

    const contextMenu = Menu.buildFromTemplate([
        { label: 'Show Display', click: () => mainWindow?.show() },
        { label: 'Settings', click: createSettingsWindow },
        { type: 'separator' },
        {
            label: 'Set Status', submenu: [
                { label: 'Clear Status', click: () => setManualStatus(null) },
                { type: 'separator' },
                { label: '📞 On a Call', click: () => setManualStatus('oncall') },
                { label: '🚶 Away / Break', click: () => setManualStatus('away') },
                { label: '🎧 Monitoring', click: () => setManualStatus('monitoring') },
                { label: '🎤 Talent Ready', click: () => setManualStatus('talentready') },
                { label: '⏰ Session Ending', click: () => setManualStatus('sessionending') }
            ]
        },
        { type: 'separator' },
        { label: 'Quit', click: () => app.quit() }
    ]);

    tray.setToolTip('StudioBeacon');
    tray.setContextMenu(contextMenu);

    tray.on('double-click', () => {
        mainWindow?.show();
    });
}

// Set manual status override
function setManualStatus(status) {
    currentState.manualStatus = status;
    broadcastState();

    // Trigger Govee lighting change
    if (goveeService) {
        const activeStatus = status || currentState.transport;
        goveeService.onStatusChange(activeStatus);
    }
}

// Broadcast state to all windows
function broadcastState() {
    if (mainWindow && mainWindow.webContents) {
        mainWindow.webContents.send('state-update', currentState);
    }
}

// Start WebSocket server
function startWebSocketServer() {
    const port = settings.network.port;

    wss = new WebSocket.Server({ port });

    console.log(`WebSocket server started on port ${port}`);

    wss.on('connection', (ws) => {
        console.log('VST plugin connected');

        // Send current settings to VST
        ws.send(JSON.stringify({ type: 'connected', settings }));

        ws.on('message', (data) => {
            try {
                const message = JSON.parse(data.toString());
                handleVSTMessage(message);
            } catch (err) {
                console.error('Error parsing message:', err);
            }
        });

        ws.on('close', () => {
            console.log('VST plugin disconnected');
        });
    });

    wss.on('error', (err) => {
        console.error('WebSocket server error:', err);
    });
}

// File-based state monitoring (Fallback for missing Sockets)
let lastFileContent = '';
let fileChecks = 0;
function startFileWatcher() {
    // Path to temp file: %TEMP%/StudioBeacon/live_state.json
    const tempDir = path.join(os.tmpdir(), 'StudioBeacon');
    const stateFile = path.join(tempDir, 'live_state.json');

    console.log(`[StudioBeacon] Monitoring state file: ${stateFile}`);

    // Poll every 100ms (watching is unreliable on some network shares/temp folders)
    setInterval(() => {
        fileChecks++;

        if (fs.existsSync(stateFile)) {
            try {
                const data = fs.readFileSync(stateFile, 'utf-8');
                if (!data || data === lastFileContent) return;

                lastFileContent = data;
                const message = JSON.parse(data);

                // Debug logging - meter data
                if (message.state?.masterMeter) {
                    console.log(`[StudioBeacon] Meter: L=${message.state.masterMeter.left?.toFixed(1) || -60} dB, R=${message.state.masterMeter.right?.toFixed(1) || -60} dB`);
                }

                handleVSTMessage(message);
            } catch (err) {
                // Ignore read errors (race conditions)
            }
        } else if (fileChecks % 100 === 0) {
            // Log every 10 seconds if file doesn't exist
            console.log(`[StudioBeacon] State file not found: ${stateFile}`);
        }
    }, 100);
}

// Handle messages from VST plugin
function handleVSTMessage(message) {
    const previousTransport = currentState.transport;

    switch (message.type) {
        case 'transport':
            currentState.transport = message.state; // recording, playing, paused, stopped
            break;
        case 'metadata':
            Object.assign(currentState, message.data);
            break;
        case 'full-state':
            // Merge the incoming state with current state
            if (message.state) {
                Object.assign(currentState, message.state);
            }
            break;
    }
    broadcastState();

    // Notify services of transport change
    if (currentState.transport !== previousTransport) {
        // FanControl - reduce fan speed during recording
        if (fanControlService) {
            fanControlService.onTransportChange(currentState.transport);
        }

        // Govee - change lighting based on status
        if (goveeService) {
            goveeService.onStatusChange(currentState.transport);
        }
    }
}

// IPC Handlers
ipcMain.handle('get-settings', () => settings);
ipcMain.handle('get-monitors', () => getMonitors());
ipcMain.handle('get-state', () => currentState);

ipcMain.on('save-settings', (event, newSettings) => {
    // Deep merge settings to preserve nested objects
    settings = {
        ...settings,
        display: { ...settings.display, ...newSettings.display },
        theme: { ...settings.theme, ...newSettings.theme },
        typography: { ...settings.typography, ...newSettings.typography },
        effects: { ...settings.effects, ...newSettings.effects },
        visibility: { ...settings.visibility, ...newSettings.visibility },
        studio: { ...settings.studio, ...newSettings.studio },
        network: { ...settings.network, ...newSettings.network }
    };
    saveSettings();

    // Apply settings to main window immediately
    if (mainWindow && mainWindow.webContents) {
        mainWindow.webContents.send('settings-update', settings);
    }
});

ipcMain.on('set-manual-status', (event, status) => {
    setManualStatus(status);
});

ipcMain.on('set-custom-message', (event, message) => {
    currentState.customMessage = message;
    currentState.manualStatus = message ? 'custom' : null;
    broadcastState();
});

ipcMain.on('open-settings', () => {
    createSettingsWindow();
});

ipcMain.on('change-monitor', (event, monitorId) => {
    settings.display.selectedMonitor = monitorId;
    saveSettings();

    // Recreate main window on new monitor
    if (mainWindow) {
        // Store reference and destroy completely
        const oldWindow = mainWindow;
        mainWindow = null;
        oldWindow.destroy();

        // Small delay to ensure cleanup, then create new window
        setTimeout(() => {
            createMainWindow();
        }, 100);
    }
});

// Window control IPC handlers
ipcMain.on('minimize-to-tray', () => {
    if (mainWindow) {
        mainWindow.hide();
    }
});

ipcMain.on('minimize-to-taskbar', () => {
    if (mainWindow) {
        mainWindow.minimize();
    }
});

ipcMain.on('toggle-fullscreen', () => {
    if (mainWindow) {
        mainWindow.setFullScreen(!mainWindow.isFullScreen());
    }
});

ipcMain.on('quit-app', () => {
    app.quit();
});

ipcMain.on('close-settings-save', () => {
    if (settingsWindow) {
        settingsWindow.close();
    }
});

ipcMain.on('close-settings-cancel', () => {
    // Reload settings from disk (discard changes)
    loadSettings();
    if (settingsWindow) {
        settingsWindow.close();
    }
});

// REAPER Integration Handlers
const REAPER_SCRIPTS_PATH = path.join(process.env.APPDATA || '', 'REAPER', 'Scripts');
const REAPER_INI_PATH = path.join(process.env.APPDATA || '', 'REAPER', 'reaper.ini');
const BRIDGE_SCRIPT_NAME = '__startup_studiobeacon.lua';

// Get the embedded bridge script path (bundled with app)
function getBridgeScriptSource() {
    // In production, scripts are in the app directory
    const appPath = app.isPackaged ? path.dirname(app.getPath('exe')) : __dirname;
    return path.join(appPath, 'scripts', BRIDGE_SCRIPT_NAME);
}

// Check if REAPER bridge is installed
ipcMain.handle('check-reaper-bridge-status', async () => {
    try {
        const bridgePath = path.join(REAPER_SCRIPTS_PATH, BRIDGE_SCRIPT_NAME);
        const exists = fs.existsSync(bridgePath);
        return { installed: exists, path: bridgePath };
    } catch (err) {
        return { installed: false, error: err.message };
    }
});

// Reinstall the REAPER bridge script
ipcMain.handle('reinstall-reaper-bridge', async () => {
    try {
        // Ensure REAPER Scripts directory exists
        if (!fs.existsSync(REAPER_SCRIPTS_PATH)) {
            fs.mkdirSync(REAPER_SCRIPTS_PATH, { recursive: true });
        }

        const sourcePath = getBridgeScriptSource();
        const destPath = path.join(REAPER_SCRIPTS_PATH, BRIDGE_SCRIPT_NAME);

        // Copy the script
        if (fs.existsSync(sourcePath)) {
            fs.copyFileSync(sourcePath, destPath);
            return { success: true, message: 'Bridge script installed. Restart REAPER to activate.' };
        } else {
            return { success: false, message: 'Source script not found.' };
        }
    } catch (err) {
        return { success: false, message: err.message };
    }
});

// Configure REAPER OSC settings
ipcMain.handle('configure-reaper-osc', async () => {
    try {
        if (!fs.existsSync(REAPER_INI_PATH)) {
            return { success: false, message: 'REAPER not found. Please install REAPER first.' };
        }

        // Read current reaper.ini
        let iniContent = fs.readFileSync(REAPER_INI_PATH, 'utf-8');

        // OSC settings to add/modify
        // REAPER's OSC section format: [osc]
        const oscSettings = {
            'oscsend': '127.0.0.1:9999',     // Send OSC to localhost:9999
            'oscrecv': '8000',               // Receive on port 8000
            'oscsendbpm': '1',               // Send BPM
            'oscsendplay': '1',              // Send play state
            'oscsendrecord': '1',            // Send record state
            'oscsendtime': '1'               // Send time position
        };

        // Check if [osc] section exists
        if (!iniContent.includes('[osc]')) {
            // Add OSC section
            iniContent += '\n[osc]\n';
            for (const [key, value] of Object.entries(oscSettings)) {
                iniContent += `${key}=${value}\n`;
            }
        } else {
            // Modify existing section - just update the send address
            iniContent = iniContent.replace(/oscsend=.*/g, 'oscsend=127.0.0.1:9999');
        }

        // Backup original
        const backupPath = REAPER_INI_PATH + '.studiobeacon.bak';
        if (!fs.existsSync(backupPath)) {
            fs.copyFileSync(REAPER_INI_PATH, backupPath);
        }

        // Write updated ini
        fs.writeFileSync(REAPER_INI_PATH, iniContent);

        return {
            success: true,
            message: 'OSC configured. Restart REAPER for changes to take effect.\nBackup saved to reaper.ini.studiobeacon.bak'
        };
    } catch (err) {
        return { success: false, message: err.message };
    }
});

// App lifecycle
app.whenReady().then(async () => {
    loadSettings();
    createMainWindow();
    createTray();
    startWebSocketServer();
    startFileWatcher();

    // Initialize Govee Smart Lighting
    goveeService = new GoveeService(CONFIG_DIR);
    await goveeService.loadConfig();
    if (goveeService.config.enabled) {
        console.log('Govee: Initializing smart lighting...');
        await goveeService.initialize();
    }

    // Initialize FanControl Integration (opt-in only)
    fanControlService = new FanControlService(CONFIG_DIR);
    fanControlService.loadConfig();
    if (fanControlService.config.enabled) {
        console.log('FanControl: Initializing fan control...');
        await fanControlService.initialize();
    }
});

app.on('window-all-closed', () => {
    // Keep running in tray on Windows
    if (process.platform !== 'darwin') {
        // Don't quit, keep in tray
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createMainWindow();
    }
});

app.on('before-quit', async () => {
    // Shutdown Govee (restore previous lighting state)
    if (goveeService) {
        await goveeService.shutdown();
    }

    // Shutdown FanControl (restore normal fan speeds)
    if (fanControlService) {
        await fanControlService.shutdown();
    }

    if (wss) {
        wss.close();
    }
});

// ============================================
// Govee Smart Lighting IPC Handlers
// ============================================

// Get Govee configuration
ipcMain.handle('govee-get-config', () => {
    if (!goveeService) return null;
    return goveeService.getConfigForDisplay();
});

// Update Govee configuration
ipcMain.handle('govee-update-config', async (event, updates) => {
    if (!goveeService) return { success: false, message: 'Service not initialized' };
    await goveeService.updateConfig(updates);
    return { success: true };
});

// Set Govee API key
ipcMain.handle('govee-set-api-key', async (event, apiKey) => {
    if (!goveeService) return { success: false, message: 'Service not initialized' };
    const success = await goveeService.setApiKey(apiKey);
    if (success) {
        return await goveeService.initialize();
    }
    return { success: false, message: 'Failed to store API key' };
});

// Clear Govee API key
ipcMain.handle('govee-clear-api-key', async () => {
    if (!goveeService) return { success: false };
    await goveeService.clearApiKey();
    return { success: true };
});

// Discover Govee devices
ipcMain.handle('govee-discover-devices', async () => {
    if (!goveeService) return { success: false, devices: [] };
    return await goveeService.getDevices();
});

// Test Govee lighting
ipcMain.handle('govee-test-lighting', async (event, profile) => {
    if (!goveeService) return { success: false, message: 'Service not initialized' };
    return await goveeService.testLighting(profile);
});

// Get transition options
ipcMain.handle('govee-get-transition-options', () => {
    if (!goveeService) return [];
    return goveeService.getTransitionOptions();
});

// Get default status mappings
ipcMain.handle('govee-get-default-mappings', () => {
    if (!goveeService) return {};
    return goveeService.getDefaultMappings();
});

// ============================================
// FanControl Integration IPC Handlers
// ============================================

// Get FanControl configuration
ipcMain.handle('fancontrol-get-config', () => {
    if (!fanControlService) return null;
    return fanControlService.getConfigForDisplay();
});

// Update FanControl configuration
ipcMain.handle('fancontrol-update-config', async (event, updates) => {
    if (!fanControlService) return { success: false, message: 'Service not initialized' };
    fanControlService.updateConfig(updates);
    return { success: true };
});

// Test FanControl connection
ipcMain.handle('fancontrol-test-connection', async () => {
    if (!fanControlService) return { success: false, message: 'Service not initialized' };
    return await fanControlService.testConnection();
});

// Check FanControl config status (if recording/normal configs exist)
ipcMain.handle('fancontrol-check-configs', async () => {
    if (!fanControlService) return { success: false };
    return await fanControlService.createDefaultConfigs();
});

// Manually trigger recording mode (for testing)
ipcMain.handle('fancontrol-enter-recording-mode', async () => {
    if (!fanControlService) return { success: false };
    const result = await fanControlService.enterRecordingMode();
    return { success: result };
});

// Manually exit recording mode (for testing)
ipcMain.handle('fancontrol-exit-recording-mode', async () => {
    if (!fanControlService) return { success: false };
    const result = await fanControlService.exitRecordingMode();
    return { success: result };
});
