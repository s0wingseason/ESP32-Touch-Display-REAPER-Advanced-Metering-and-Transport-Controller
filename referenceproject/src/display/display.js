/**
 * StudioBeacon - Display Logic
 * Handles state updates, UI rendering, keyboard controls, and particle effects
 */

// State
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
    sampleRate: 48000,
    // New metadata
    currentMarker: '',
    nextMarker: '',
    takeCount: 0,
    clickEnabled: false,
    recordingDuration: 0
};

let settings = {};
let sessionStartTime = Date.now();

// DOM Elements
const elements = {
    statusIndicator: document.getElementById('statusIndicator'),
    statusText: document.getElementById('statusText'),
    statusSubtitle: document.getElementById('statusSubtitle'),
    projectName: document.getElementById('projectName'),
    timecode: document.getElementById('timecode'),
    barsBeats: document.getElementById('barsBeats'),
    regionName: document.getElementById('regionName'),
    tempo: document.getElementById('tempo'),
    studioName: document.getElementById('studioName'),
    sessionTimer: document.getElementById('sessionTimer'),
    datetime: document.getElementById('datetime'),
    particles: document.getElementById('particles'),
    metadataPanel: document.getElementById('metadataPanel'),
    settingsHint: document.getElementById('settingsHint'),
    // Visibility rows
    projectRow: document.getElementById('projectRow'),
    timecodeRow: document.getElementById('timecodeRow'),
    barsBeatsRow: document.getElementById('barsBeatsRow'),
    regionRow: document.getElementById('regionRow'),
    tempoRow: document.getElementById('tempoRow')
};

// Status configurations
const statusConfigs = {
    // DAW Transport States
    recording: {
        text: 'RECORDING',
        subtitle: 'Session in progress',
        class: 'recording'
    },
    playing: {
        text: 'PLAYBACK',
        subtitle: 'Reviewing take',
        class: 'playing'
    },
    paused: {
        text: 'PAUSED',
        subtitle: 'Awaiting input',
        class: 'paused'
    },
    stopped: {
        text: 'STANDBY',
        subtitle: 'Awaiting session',
        class: 'stopped'
    },
    // Extended DAW States
    arming: {
        text: 'ARMING',
        subtitle: 'Preparing to record',
        class: 'arming'
    },
    comping: {
        text: 'COMPING',
        subtitle: 'Editing takes',
        class: 'comping'
    },
    mixing: {
        text: 'MIXING',
        subtitle: 'Balancing levels',
        class: 'mixing'
    },
    mastering: {
        text: 'MASTERING',
        subtitle: 'Final processing',
        class: 'mastering'
    },
    bouncing: {
        text: 'BOUNCING',
        subtitle: 'Exporting audio',
        class: 'bouncing'
    },
    rendering: {
        text: 'RENDERING',
        subtitle: 'Processing...',
        class: 'rendering'
    },
    // Manual Status Overrides
    oncall: {
        text: 'ON A CALL',
        subtitle: 'Do not disturb',
        class: 'oncall'
    },
    away: {
        text: 'AWAY',
        subtitle: 'Taking a break',
        class: 'away'
    },
    lunch: {
        text: 'AT LUNCH',
        subtitle: 'Back soon',
        class: 'lunch'
    },
    meeting: {
        text: 'IN MEETING',
        subtitle: 'Please wait',
        class: 'meeting'
    },
    monitoring: {
        text: 'MONITORING',
        subtitle: 'Listening back',
        class: 'monitoring'
    },
    reviewing: {
        text: 'REVIEWING',
        subtitle: 'Checking takes',
        class: 'reviewing'
    },
    talentready: {
        text: 'TALENT READY',
        subtitle: 'Performer standing by',
        class: 'talentready'
    },
    talentinbooth: {
        text: 'TALENT IN BOOTH',
        subtitle: 'Recording imminent',
        class: 'talentinbooth'
    },
    sessionending: {
        text: 'SESSION ENDING',
        subtitle: 'Wrapping up soon',
        class: 'sessionending'
    },
    sessioncomplete: {
        text: 'SESSION COMPLETE',
        subtitle: 'Thank you!',
        class: 'sessioncomplete'
    },
    // Technical States
    technicalissue: {
        text: 'TECHNICAL ISSUE',
        subtitle: 'Please stand by',
        class: 'technicalissue'
    },
    calibrating: {
        text: 'CALIBRATING',
        subtitle: 'Adjusting levels',
        class: 'calibrating'
    },
    soundcheck: {
        text: 'SOUND CHECK',
        subtitle: 'Testing levels',
        class: 'soundcheck'
    },
    // Privacy/Access
    donotenter: {
        text: 'DO NOT ENTER',
        subtitle: 'Session in progress',
        class: 'donotenter'
    },
    quiet: {
        text: 'QUIET PLEASE',
        subtitle: 'Recording nearby',
        class: 'quiet'
    },
    available: {
        text: 'AVAILABLE',
        subtitle: 'Come on in',
        class: 'available'
    },
    custom: {
        text: 'CUSTOM',
        subtitle: '',
        class: 'custom'
    }
};

// Status groups for multi-press cycling
const statusGroups = {
    '1': ['recording', 'arming', 'bouncing', 'rendering'],           // Recording family
    '2': ['playing', 'comping', 'mixing', 'mastering', 'reviewing'], // Playback/Edit family
    '3': ['oncall', 'away', 'lunch', 'meeting'],                      // Away family
    '4': ['monitoring', 'soundcheck', 'calibrating'],                 // Monitoring family
    '5': ['talentready', 'talentinbooth'],                           // Talent family
    '6': ['sessionending', 'sessioncomplete'],                       // Session family
    '7': ['donotenter', 'quiet'],                                    // Privacy family
    '8': ['available'],                                               // Access
    '9': ['technicalissue'],                                         // Technical
    '0': ['custom']                                                  // Custom
};

// Track current index per group for cycling
const groupCycleIndex = {};
let lastKeyPressed = null;
let lastKeyTime = 0;
const CYCLE_TIMEOUT = 800; // ms to reset cycle

/**
 * Initialize the display
 */
function init() {
    // Set up event listeners
    setupKeyboardControls();

    // Start timers
    startClockUpdates();
    startSessionTimer();

    // Initialize particle system
    initParticles();

    // Listen for state updates from main process
    window.studioBeacon.onStateUpdate((state) => {
        currentState = { ...currentState, ...state };
        updateDisplay();
    });

    // Listen for settings updates
    window.studioBeacon.onSettingsUpdate((newSettings) => {
        settings = newSettings;
        applySettings();
    });

    // Initial update
    updateDisplay();

    // Window Control Event Listeners
    const btnMinimizeTaskbar = document.getElementById('btn-minimize-taskbar');
    if (btnMinimizeTaskbar) {
        btnMinimizeTaskbar.addEventListener('click', () => {
            window.studioBeacon.minimizeToTaskbar();
        });
    }

    const btnMinimizeTray = document.getElementById('btn-minimize-tray');
    if (btnMinimizeTray) {
        btnMinimizeTray.addEventListener('click', () => {
            window.studioBeacon.minimizeToTray();
        });
    }

    const btnFullscreen = document.getElementById('btn-fullscreen');
    if (btnFullscreen) {
        btnFullscreen.addEventListener('click', () => {
            window.studioBeacon.toggleFullscreen();
        });
    }

    const btnSettings = document.getElementById('btn-settings');
    if (btnSettings) {
        btnSettings.addEventListener('click', () => {
            // Toggle embedded settings panel
            const overlay = document.getElementById('settingsOverlay');
            if (overlay) {
                overlay.classList.toggle('active');
                if (overlay.classList.contains('active')) {
                    initEmbeddedSettings();
                }
            }
        });
    }

    const btnQuit = document.getElementById('btn-quit');
    if (btnQuit) {
        btnQuit.addEventListener('click', () => {
            window.studioBeacon.quitApp();
        });
    }

    // Embedded settings panel handlers
    setupEmbeddedSettings();
}

/**
 * Set up embedded settings panel
 */
function setupEmbeddedSettings() {
    const overlay = document.getElementById('settingsOverlay');
    const closeBtn = document.getElementById('settingsClose');
    const saveBtn = document.getElementById('saveSettings');
    const resetBtn = document.getElementById('resetDefaults');
    const applyStatusBtn = document.getElementById('applyStatus');
    const clearStatusBtn = document.getElementById('clearStatus');
    const reinstallBridgeBtn = document.getElementById('reinstallBridge');
    const configureOSCBtn = document.getElementById('configureOSC');

    // Close button
    if (closeBtn) {
        closeBtn.addEventListener('click', () => {
            overlay.classList.remove('active');
        });
    }

    // Close on overlay click
    if (overlay) {
        overlay.addEventListener('click', (e) => {
            if (e.target === overlay) {
                overlay.classList.remove('active');
            }
        });
    }

    // Save button
    if (saveBtn) {
        saveBtn.addEventListener('click', () => {
            saveEmbeddedSettings();
            overlay.classList.remove('active');
        });
    }

    // Apply status
    if (applyStatusBtn) {
        applyStatusBtn.addEventListener('click', () => {
            const statusSelect = document.getElementById('statusSelect');
            const customInput = document.getElementById('customStatusMessage');

            if (customInput && customInput.value.trim()) {
                window.studioBeacon.setCustomMessage(customInput.value.trim());
            } else if (statusSelect && statusSelect.value) {
                window.studioBeacon.setManualStatus(statusSelect.value);
            }
        });
    }

    // Clear status
    if (clearStatusBtn) {
        clearStatusBtn.addEventListener('click', () => {
            window.studioBeacon.setManualStatus(null);
            const statusSelect = document.getElementById('statusSelect');
            if (statusSelect) statusSelect.value = '';
        });
    }

    // Reinstall REAPER bridge
    if (reinstallBridgeBtn) {
        reinstallBridgeBtn.addEventListener('click', async () => {
            const result = await window.studioBeacon.reinstallReaperBridge();
            alert(result.message);
            checkReaperStatus();
        });
    }

    // Configure OSC
    if (configureOSCBtn) {
        configureOSCBtn.addEventListener('click', async () => {
            const result = await window.studioBeacon.configureReaperOSC();
            alert(result.message);
        });
    }

    // Status Size slider - live update
    const statusSizeSlider = document.getElementById('statusSize');
    const statusSizeValue = document.getElementById('statusSizeValue');
    if (statusSizeSlider && statusSizeValue) {
        statusSizeSlider.addEventListener('input', () => {
            statusSizeValue.textContent = `${statusSizeSlider.value}px`;
            // Apply live preview
            document.documentElement.style.setProperty('--status-size', `${statusSizeSlider.value}px`);
        });
    }
}

/**
 * Initialize embedded settings with current values
 */
async function initEmbeddedSettings() {
    // Load monitors
    try {
        const monitors = await window.studioBeacon.getMonitors();
        const monitorSelect = document.getElementById('monitorSelect');
        if (monitorSelect) {
            monitorSelect.innerHTML = monitors.map(m =>
                `<option value="${m.id}">${m.label}${m.primary ? ' (Primary)' : ''}</option>`
            ).join('');
            monitorSelect.value = settings.display?.selectedMonitor || 0;
        }
    } catch (e) {
        console.error('Failed to load monitors:', e);
    }

    // Load current settings
    const displayModeSelect = document.getElementById('displayModeSelect');
    if (displayModeSelect) {
        displayModeSelect.value = settings.display?.displayMode || 'fullscreen';
    }

    const alwaysOnTop = document.getElementById('alwaysOnTopToggle');
    if (alwaysOnTop) {
        alwaysOnTop.checked = settings.display?.alwaysOnTop !== false;
    }

    const studioNameInput = document.getElementById('studioNameInput');
    if (studioNameInput) {
        studioNameInput.value = settings.studio?.name || '';
    }

    // Check REAPER status
    checkReaperStatus();

    // Initialize Govee settings
    initGoveeSettings();

    // Initialize FanControl settings
    initFanControlSettings();
}

/**
 * Initialize Govee smart lighting settings
 */
async function initGoveeSettings() {
    try {
        const goveeConfig = await window.studioBeacon.govee.getConfig();
        if (!goveeConfig) return;

        // Populate current values
        const enabledCheckbox = document.getElementById('goveeEnabled');
        const transitionSelect = document.getElementById('goveeTransition');
        const restoreCheckbox = document.getElementById('goveeRestoreOnExit');

        if (enabledCheckbox) enabledCheckbox.checked = goveeConfig.enabled;
        if (transitionSelect) transitionSelect.value = goveeConfig.transitionMs || 3000;
        if (restoreCheckbox) restoreCheckbox.checked = goveeConfig.restoreOnExit !== false;

        // If has API key, show device list
        if (goveeConfig.hasApiKey) {
            document.getElementById('goveeApiKey').placeholder = '••••••••••••••••';
            await loadGoveeDevices(goveeConfig.selectedDevices || []);
        }
    } catch (e) {
        console.error('Failed to load Govee settings:', e);
    }

    // Set up Govee event handlers
    setupGoveeHandlers();

    // Set up FanControl event handlers
    setupFanControlHandlers();
}

/**
 * Load and display Govee devices
 */
async function loadGoveeDevices(selectedIds) {
    const result = await window.studioBeacon.govee.discoverDevices();
    const deviceList = document.getElementById('goveeDeviceList');
    const devicesRow = document.getElementById('goveeDevicesRow');
    const statusEl = document.getElementById('goveeStatus');

    if (result.success && result.devices.length > 0) {
        devicesRow.style.display = 'flex';
        deviceList.innerHTML = result.devices.map(device => `
            <label class="device-item">
                <input type="checkbox" class="govee-device-checkbox" 
                    data-device-id="${device.id}" 
                    ${selectedIds.includes(device.id) ? 'checked' : ''}>
                <span>${device.name}</span>
                <small>${device.sku}</small>
            </label>
        `).join('');

        if (statusEl) {
            statusEl.textContent = `${result.devices.length} devices found`;
            statusEl.className = 'status-indicator-small connected';
        }
    } else {
        if (statusEl) {
            statusEl.textContent = result.message || 'No devices found';
            statusEl.className = 'status-indicator-small disconnected';
        }
    }
}

/**
 * Set up Govee event handlers
 */
function setupGoveeHandlers() {
    // Connect button
    const connectBtn = document.getElementById('goveeConnect');
    if (connectBtn) {
        connectBtn.addEventListener('click', async () => {
            const apiKeyInput = document.getElementById('goveeApiKey');
            const statusEl = document.getElementById('goveeStatus');

            if (apiKeyInput && apiKeyInput.value) {
                statusEl.textContent = 'Connecting...';
                const result = await window.studioBeacon.govee.setApiKey(apiKeyInput.value);

                if (result.success) {
                    apiKeyInput.value = '';
                    apiKeyInput.placeholder = '••••••••••••••••';
                    statusEl.textContent = result.message;
                    statusEl.className = 'status-indicator-small connected';
                    await loadGoveeDevices([]);
                } else {
                    statusEl.textContent = result.message || 'Connection failed';
                    statusEl.className = 'status-indicator-small disconnected';
                }
            } else {
                // Just discover devices with existing key
                await loadGoveeDevices([]);
            }
        });
    }

    // Enable checkbox
    const enabledCheckbox = document.getElementById('goveeEnabled');
    if (enabledCheckbox) {
        enabledCheckbox.addEventListener('change', async () => {
            await window.studioBeacon.govee.updateConfig({ enabled: enabledCheckbox.checked });
        });
    }

    // Transition select
    const transitionSelect = document.getElementById('goveeTransition');
    if (transitionSelect) {
        transitionSelect.addEventListener('change', async () => {
            await window.studioBeacon.govee.updateConfig({ transitionMs: parseInt(transitionSelect.value) });
        });
    }

    // Restore on exit checkbox
    const restoreCheckbox = document.getElementById('goveeRestoreOnExit');
    if (restoreCheckbox) {
        restoreCheckbox.addEventListener('change', async () => {
            await window.studioBeacon.govee.updateConfig({ restoreOnExit: restoreCheckbox.checked });
        });
    }

    // Device selection (delegated)
    const deviceList = document.getElementById('goveeDeviceList');
    if (deviceList) {
        deviceList.addEventListener('change', async (e) => {
            if (e.target.classList.contains('govee-device-checkbox')) {
                const checkboxes = deviceList.querySelectorAll('.govee-device-checkbox:checked');
                const selectedDevices = Array.from(checkboxes).map(cb => cb.dataset.deviceId);
                await window.studioBeacon.govee.updateConfig({ selectedDevices });
            }
        });
    }

    // Test buttons
    const testRed = document.getElementById('goveeTestRed');
    const testGreen = document.getElementById('goveeTestGreen');
    const testWhite = document.getElementById('goveeTestWhite');

    if (testRed) {
        testRed.addEventListener('click', () => {
            window.studioBeacon.govee.testLighting({ color: '#FF0000', brightness: 100, power: true });
        });
    }
    if (testGreen) {
        testGreen.addEventListener('click', () => {
            window.studioBeacon.govee.testLighting({ color: '#00FF00', brightness: 100, power: true });
        });
    }
    if (testWhite) {
        testWhite.addEventListener('click', () => {
            window.studioBeacon.govee.testLighting({ color: '#FFFFFF', brightness: 80, power: true });
        });
    }
}

/**
 * Initialize FanControl settings UI
 */
async function initFanControlSettings() {
    if (!window.studioBeacon.fanControl) return;

    const config = await window.studioBeacon.fanControl.getConfig();
    if (!config) return;

    const enabledCheck = document.getElementById('fanControlEnabled');
    const speedSelect = document.getElementById('fanControlSpeed');
    const restoreCheck = document.getElementById('fanControlRestoreOnExit');

    if (enabledCheck) enabledCheck.checked = config.enabled;
    if (speedSelect) speedSelect.value = config.recordingFanSpeed || 25;
    if (restoreCheck) restoreCheck.checked = config.restoreOnExit !== false;

    // Update status indicator
    const statusEl = document.getElementById('fanControlStatus');
    if (statusEl) {
        if (config.fanControlInstalled) {
            statusEl.textContent = config.isRecordingMode ? 'Recording Mode' : 'Ready';
            statusEl.className = 'status-indicator-small connected';
        } else if (config.enabled) {
            statusEl.textContent = 'FanControl not found';
            statusEl.className = 'status-indicator-small disconnected';
        }
    }
}

/**
 * Set up FanControl event handlers
 */
function setupFanControlHandlers() {
    // Enable checkbox
    const enabledCheck = document.getElementById('fanControlEnabled');
    if (enabledCheck) {
        enabledCheck.addEventListener('change', async () => {
            await window.studioBeacon.fanControl.updateConfig({ enabled: enabledCheck.checked });
            if (enabledCheck.checked) {
                await testFanControlConnection();
            }
        });
    }

    // Speed select
    const speedSelect = document.getElementById('fanControlSpeed');
    if (speedSelect) {
        speedSelect.addEventListener('change', async () => {
            await window.studioBeacon.fanControl.updateConfig({
                recordingFanSpeed: parseInt(speedSelect.value)
            });
        });
    }

    // Restore on exit
    const restoreCheck = document.getElementById('fanControlRestoreOnExit');
    if (restoreCheck) {
        restoreCheck.addEventListener('change', async () => {
            await window.studioBeacon.fanControl.updateConfig({
                restoreOnExit: restoreCheck.checked
            });
        });
    }

    // Test connection button
    const testBtn = document.getElementById('fanControlTest');
    if (testBtn) {
        testBtn.addEventListener('click', testFanControlConnection);
    }
}

/**
 * Test FanControl connection
 */
async function testFanControlConnection() {
    const statusEl = document.getElementById('fanControlStatus');
    const configRow = document.getElementById('fanControlConfigStatus');
    const configMsg = document.getElementById('fanControlConfigMsg');

    if (statusEl) {
        statusEl.textContent = 'Testing...';
        statusEl.className = 'status-indicator-small';
    }

    const result = await window.studioBeacon.fanControl.testConnection();

    if (result.success) {
        if (statusEl) {
            statusEl.textContent = 'Connected';
            statusEl.className = 'status-indicator-small connected';
        }

        // Show config status
        if (result.configStatus && configRow && configMsg) {
            configRow.style.display = 'flex';
            if (result.configStatus.missingConfigs.length > 0) {
                configMsg.textContent = `Missing: ${result.configStatus.missingConfigs.join(', ')}`;
                configMsg.className = 'warning-text';
            } else {
                configMsg.textContent = 'Configs found ✓';
                configMsg.className = 'success-text';
            }
        }
    } else {
        if (statusEl) {
            statusEl.textContent = result.message || 'Not connected';
            statusEl.className = 'status-indicator-small disconnected';
        }
    }
}

/**
 * Check REAPER bridge status
 */
async function checkReaperStatus() {
    const statusEl = document.getElementById('reaperStatus');
    if (!statusEl) return;

    try {
        const result = await window.studioBeacon.checkReaperBridgeStatus();
        if (result.installed) {
            statusEl.textContent = 'Installed';
            statusEl.className = 'status-indicator-small connected';
        } else {
            statusEl.textContent = 'Not Installed';
            statusEl.className = 'status-indicator-small disconnected';
        }
    } catch (e) {
        statusEl.textContent = 'Unknown';
        statusEl.className = 'status-indicator-small';
    }
}

/**
 * Save embedded settings
 */
function saveEmbeddedSettings() {
    const monitorSelect = document.getElementById('monitorSelect');
    const displayModeSelect = document.getElementById('displayModeSelect');
    const alwaysOnTop = document.getElementById('alwaysOnTopToggle');
    const studioNameInput = document.getElementById('studioNameInput');
    const statusSizeSlider = document.getElementById('statusSize');

    // Theme colors
    const bgColor = document.getElementById('bgColor');
    const accentPrimary = document.getElementById('accentPrimary');
    const accentSecondary = document.getElementById('accentSecondary');

    // Visibility toggles
    const showProjectName = document.getElementById('showProjectName');
    const showTimecode = document.getElementById('showTimecode');
    const showBarsBeats = document.getElementById('showBarsBeats');
    const showRegionName = document.getElementById('showRegionName');
    const showTempo = document.getElementById('showTempo');

    const newSettings = {
        ...settings,
        display: {
            ...settings.display,
            selectedMonitor: parseInt(monitorSelect?.value) || 0,
            displayMode: displayModeSelect?.value || 'fullscreen',
            alwaysOnTop: alwaysOnTop?.checked !== false
        },
        theme: {
            ...settings.theme,
            backgroundColor: bgColor?.value || '#0d0d12',
            accentPrimary: accentPrimary?.value || '#FF4D6D',
            accentSecondary: accentSecondary?.value || '#4DEDAB'
        },
        typography: {
            ...settings.typography,
            statusSize: parseInt(statusSizeSlider?.value) || 140
        },
        visibility: {
            ...settings.visibility,
            projectName: showProjectName?.checked !== false,
            timecode: showTimecode?.checked !== false,
            barsBeats: showBarsBeats?.checked !== false,
            regionName: showRegionName?.checked !== false,
            tempo: showTempo?.checked !== false
        },
        studio: {
            ...settings.studio,
            name: studioNameInput?.value || 'Studio'
        }
    };

    // Update local settings reference
    settings = newSettings;

    // Apply settings immediately
    applySettings(newSettings);

    window.studioBeacon.saveSettings(newSettings);
}

/**
 * Update the display based on current state
 */
function updateDisplay() {
    // Determine which status to show (manual override takes precedence)
    const activeStatus = currentState.manualStatus || currentState.transport;
    const config = statusConfigs[activeStatus] || statusConfigs.stopped;

    // Update status indicator classes
    elements.statusIndicator.className = 'status-indicator ' + config.class;

    // Update status text
    elements.statusText.className = 'status-text ' + config.class;

    if (activeStatus === 'custom' && currentState.customMessage) {
        elements.statusText.textContent = currentState.customMessage.toUpperCase();
        elements.statusSubtitle.textContent = '';
    } else {
        elements.statusText.textContent = config.text;
        elements.statusSubtitle.textContent = config.subtitle;
    }

    // Update metadata
    elements.projectName.textContent = currentState.projectName || 'No Project';
    elements.timecode.textContent = currentState.timecode || '00:00:00:00';
    elements.barsBeats.textContent = currentState.barsBeats || '1.1.0';
    elements.regionName.textContent = currentState.regionName || '—';
    elements.tempo.textContent = `${currentState.tempo || 120} BPM`;

    // Update new metadata fields
    const markerEl = document.getElementById('currentMarker');
    const takeEl = document.getElementById('takeCount');
    const clickEl = document.getElementById('clickStatus');

    if (markerEl) {
        markerEl.textContent = currentState.currentMarker || '—';
    }
    if (takeEl) {
        takeEl.textContent = currentState.takeCount > 0 ? `Take ${currentState.takeCount}` : '—';
    }
    if (clickEl) {
        clickEl.textContent = currentState.clickEnabled ? 'ON' : 'OFF';
        clickEl.className = 'metadata-value ' + (currentState.clickEnabled ? 'click-on' : 'click-off');
    }

    // Update meters
    updateMeters();

    // Update transport class on body for CSS
    document.body.className = 'transport-' + (currentState.transport || 'stopped');
}

/**
 * Apply settings to the display
 */
function applySettings() {
    const root = document.documentElement;

    // Apply theme colors
    if (settings.theme) {
        root.style.setProperty('--bg-primary', settings.theme.background);
        root.style.setProperty('--accent-punch', settings.theme.accentPrimary);
        root.style.setProperty('--accent-mint', settings.theme.accentSecondary);
        root.style.setProperty('--text-primary', settings.theme.textPrimary);
        root.style.setProperty('--text-secondary', settings.theme.textSecondary);
    }

    // Apply typography
    if (settings.typography) {
        root.style.setProperty('--font-primary', `'${settings.typography.fontFamily}', sans-serif`);
        root.style.setProperty('--status-size', `${settings.typography.statusSize}px`);
        root.style.setProperty('--metadata-size', `${settings.typography.metadataSize}px`);
    }

    // Apply effects
    if (settings.effects) {
        root.style.setProperty('--glow-intensity', settings.effects.glowIntensity);
        root.style.setProperty('--animation-speed', settings.effects.animationSpeed);
    }

    // Apply visibility settings
    if (settings.visibility) {
        elements.projectRow.style.display = settings.visibility.projectName ? 'flex' : 'none';
        elements.timecodeRow.style.display = settings.visibility.timecode ? 'flex' : 'none';
        elements.barsBeatsRow.style.display = settings.visibility.barsBeats ? 'flex' : 'none';
        elements.regionRow.style.display = settings.visibility.regionName ? 'flex' : 'none';
        elements.tempoRow.style.display = settings.visibility.tempo ? 'flex' : 'none';
    }

    // Apply studio info
    if (settings.studio) {
        elements.studioName.textContent = settings.studio.name || '';
    }
}

/**
 * Keyboard controls
 */
function setupKeyboardControls() {
    document.addEventListener('keydown', (e) => {
        const key = e.key.toLowerCase();

        // Open embedded settings with 'S'
        if (key === 's') {
            const overlay = document.getElementById('settingsOverlay');
            if (overlay) {
                overlay.classList.toggle('active');
                if (overlay.classList.contains('active')) {
                    initEmbeddedSettings();
                }
            }
            return;
        }

        // Clear manual status with Escape
        if (key === 'escape') {
            // Also close settings if open
            const overlay = document.getElementById('settingsOverlay');
            if (overlay && overlay.classList.contains('active')) {
                overlay.classList.remove('active');
                return;
            }
            window.studioBeacon.setManualStatus(null);
            return;
        }

        // Status group cycling with number keys 0-9
        if (statusGroups.hasOwnProperty(key)) {
            const group = statusGroups[key];
            const now = Date.now();

            // Check if same key pressed within timeout
            if (lastKeyPressed === key && (now - lastKeyTime) < CYCLE_TIMEOUT) {
                // Cycle to next item in group
                groupCycleIndex[key] = ((groupCycleIndex[key] || 0) + 1) % group.length;
            } else {
                // Reset to first item
                groupCycleIndex[key] = 0;
            }

            lastKeyPressed = key;
            lastKeyTime = now;

            const status = group[groupCycleIndex[key]];

            if (status === 'custom') {
                // Custom message - prompt user
                const message = prompt('Enter custom status message:');
                if (message) {
                    window.studioBeacon.setCustomMessage(message);
                }
            } else {
                window.studioBeacon.setManualStatus(status);
            }
            return;
        }

        // Toggle fullscreen with F
        if (key === 'f') {
            window.studioBeacon.toggleFullscreen();
        }
    });
}

/**
 * Clock updates
 */
function startClockUpdates() {
    function updateClock() {
        const now = new Date();
        const options = {
            weekday: 'short',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit'
        };
        elements.datetime.textContent = now.toLocaleDateString('en-US', options);
    }

    updateClock();
    setInterval(updateClock, 1000);
}

/**
 * Session timer
 */
function startSessionTimer() {
    function updateTimer() {
        const elapsed = Date.now() - sessionStartTime;
        const hours = Math.floor(elapsed / 3600000);
        const minutes = Math.floor((elapsed % 3600000) / 60000);
        const seconds = Math.floor((elapsed % 60000) / 1000);

        elements.sessionTimer.textContent =
            `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
    }

    updateTimer();
    setInterval(updateTimer, 1000);
}

/**
 * Particle System - Ambient floating particles
 */
function initParticles() {
    const canvas = elements.particles;
    const ctx = canvas.getContext('2d');

    // Resize canvas to window
    function resizeCanvas() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    }
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);

    // Particle class
    class Particle {
        constructor() {
            this.reset();
        }

        reset() {
            this.x = Math.random() * canvas.width;
            this.y = Math.random() * canvas.height;
            this.size = Math.random() * 2 + 0.5;
            this.speedX = (Math.random() - 0.5) * 0.3;
            this.speedY = (Math.random() - 0.5) * 0.3;
            this.opacity = Math.random() * 0.3 + 0.1;
            this.fadeSpeed = (Math.random() - 0.5) * 0.005;
        }

        update() {
            this.x += this.speedX;
            this.y += this.speedY;
            this.opacity += this.fadeSpeed;

            // Fade in/out
            if (this.opacity <= 0.05 || this.opacity >= 0.4) {
                this.fadeSpeed *= -1;
            }

            // Wrap around edges
            if (this.x < 0) this.x = canvas.width;
            if (this.x > canvas.width) this.x = 0;
            if (this.y < 0) this.y = canvas.height;
            if (this.y > canvas.height) this.y = 0;
        }

        draw() {
            ctx.beginPath();
            ctx.arc(this.x, this.y, this.size, 0, Math.PI * 2);
            ctx.fillStyle = `rgba(255, 255, 255, ${this.opacity})`;
            ctx.fill();
        }
    }

    // Create particles
    const particleCount = 50;
    const particles = [];
    for (let i = 0; i < particleCount; i++) {
        particles.push(new Particle());
    }

    // Animation loop
    function animate() {
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        particles.forEach(particle => {
            particle.update();
            particle.draw();
        });

        requestAnimationFrame(animate);
    }

    animate();
}

/**
 * Update meter displays
 */
function updateMeters() {
    // Master meter
    const masterMeter = currentState.masterMeter;
    if (masterMeter) {
        const masterMeterL = document.getElementById('masterMeterL');
        const masterMeterR = document.getElementById('masterMeterR');
        const masterDbL = document.getElementById('masterDbL');
        const masterDbR = document.getElementById('masterDbR');

        if (masterMeterL && masterMeterR) {
            // Convert dB to percentage (0-100%)
            const dbToPercent = (db) => {
                if (db <= -60) return 0;
                if (db >= 0) return 100;
                return ((db + 60) / 60) * 100;
            };

            masterMeterL.style.height = dbToPercent(masterMeter.left) + '%';
            masterMeterR.style.height = dbToPercent(masterMeter.right) + '%';

            masterDbL.textContent = masterMeter.left <= -60 ? '-∞' : masterMeter.left.toFixed(1);
            masterDbR.textContent = masterMeter.right <= -60 ? '-∞' : masterMeter.right.toFixed(1);
        }
    }

    // Armed track meters
    const armedTracks = currentState.armedTrackMeters || [];
    const container = document.getElementById('armedTracksContainer');
    const noArmedMsg = document.getElementById('noArmedTracks');

    if (container) {
        // Show/hide "no armed tracks" message
        if (noArmedMsg) {
            noArmedMsg.style.display = armedTracks.length === 0 ? 'block' : 'none';
        }

        // Get or create track meter elements
        let existingMeters = container.querySelectorAll('.armed-track-meter');

        armedTracks.forEach((track, index) => {
            let meterEl = existingMeters[index];

            if (!meterEl) {
                // Create new meter element
                meterEl = document.createElement('div');
                meterEl.className = 'armed-track-meter';
                meterEl.innerHTML = `
                    <div class="armed-track-name"></div>
                    <div class="armed-track-bars">
                        <div class="armed-track-bar"><div class="meter-fill meter-fill-l"></div></div>
                        <div class="armed-track-bar"><div class="meter-fill meter-fill-r"></div></div>
                    </div>
                `;
                container.appendChild(meterEl);
            }

            // Update values
            const dbToPercent = (db) => {
                if (db <= -60) return 0;
                if (db >= 0) return 100;
                return ((db + 60) / 60) * 100;
            };

            meterEl.querySelector('.armed-track-name').textContent = track.name;
            meterEl.querySelector('.meter-fill-l').style.width = dbToPercent(track.left) + '%';
            meterEl.querySelector('.meter-fill-r').style.width = dbToPercent(track.right) + '%';
        });

        // Remove excess meter elements
        existingMeters.forEach((el, index) => {
            if (index >= armedTracks.length) {
                el.remove();
            }
        });
    }

    // Update footer info
    const sampleRateInfo = document.getElementById('sampleRateInfo');
    const trackCountInfo = document.getElementById('trackCountInfo');

    if (sampleRateInfo && currentState.sampleRate) {
        sampleRateInfo.textContent = (currentState.sampleRate / 1000).toFixed(0) + 'kHz';
    }
    if (trackCountInfo && currentState.trackCount !== undefined) {
        trackCountInfo.textContent = currentState.trackCount + ' tracks';
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', init);
