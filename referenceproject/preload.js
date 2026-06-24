const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('studioBeacon', {
    // Settings
    getSettings: () => ipcRenderer.invoke('get-settings'),
    saveSettings: (settings) => ipcRenderer.send('save-settings', settings),

    // Monitors
    getMonitors: () => ipcRenderer.invoke('get-monitors'),
    changeMonitor: (monitorId) => ipcRenderer.send('change-monitor', monitorId),

    // State
    getState: () => ipcRenderer.invoke('get-state'),
    setManualStatus: (status) => ipcRenderer.send('set-manual-status', status),
    setCustomMessage: (message) => ipcRenderer.send('set-custom-message', message),

    // Window controls
    minimizeToTray: () => ipcRenderer.send('minimize-to-tray'),
    minimizeToTaskbar: () => ipcRenderer.send('minimize-to-taskbar'),
    toggleFullscreen: () => ipcRenderer.send('toggle-fullscreen'),
    quitApp: () => ipcRenderer.send('quit-app'),
    closeSettingsSave: () => ipcRenderer.send('close-settings-save'),
    closeSettingsCancel: () => ipcRenderer.send('close-settings-cancel'),

    // Event listeners
    onSettingsUpdate: (callback) => {
        ipcRenderer.on('settings-update', (event, settings) => callback(settings));
    },
    onStateUpdate: (callback) => {
        ipcRenderer.on('state-update', (event, state) => callback(state));
    },
    onMonitorsList: (callback) => {
        ipcRenderer.on('monitors-list', (event, monitors) => callback(monitors));
    },

    // REAPER Integration
    reinstallReaperBridge: () => ipcRenderer.invoke('reinstall-reaper-bridge'),
    configureReaperOSC: () => ipcRenderer.invoke('configure-reaper-osc'),
    checkReaperBridgeStatus: () => ipcRenderer.invoke('check-reaper-bridge-status'),

    // Govee Smart Lighting
    govee: {
        getConfig: () => ipcRenderer.invoke('govee-get-config'),
        updateConfig: (updates) => ipcRenderer.invoke('govee-update-config', updates),
        setApiKey: (apiKey) => ipcRenderer.invoke('govee-set-api-key', apiKey),
        clearApiKey: () => ipcRenderer.invoke('govee-clear-api-key'),
        discoverDevices: () => ipcRenderer.invoke('govee-discover-devices'),
        testLighting: (profile) => ipcRenderer.invoke('govee-test-lighting', profile),
        getTransitionOptions: () => ipcRenderer.invoke('govee-get-transition-options'),
        getDefaultMappings: () => ipcRenderer.invoke('govee-get-default-mappings')
    },

    // FanControl Integration (reduce fan noise during recording)
    fanControl: {
        getConfig: () => ipcRenderer.invoke('fancontrol-get-config'),
        updateConfig: (updates) => ipcRenderer.invoke('fancontrol-update-config', updates),
        testConnection: () => ipcRenderer.invoke('fancontrol-test-connection'),
        checkConfigs: () => ipcRenderer.invoke('fancontrol-check-configs'),
        enterRecordingMode: () => ipcRenderer.invoke('fancontrol-enter-recording-mode'),
        exitRecordingMode: () => ipcRenderer.invoke('fancontrol-exit-recording-mode')
    }
});
