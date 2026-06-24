/**
 * StudioBeacon Settings Panel Logic
 */

let settings = {};
let monitors = [];

// DOM Elements
const elements = {
    // Display
    monitorSelect: document.getElementById('monitorSelect'),
    fullscreenToggle: document.getElementById('fullscreenToggle'),
    alwaysOnTopToggle: document.getElementById('alwaysOnTopToggle'),

    // Theme
    bgColor: document.getElementById('bgColor'),
    accentPrimary: document.getElementById('accentPrimary'),
    accentSecondary: document.getElementById('accentSecondary'),
    textPrimary: document.getElementById('textPrimary'),
    textSecondary: document.getElementById('textSecondary'),

    // Typography
    fontFamily: document.getElementById('fontFamily'),
    statusSize: document.getElementById('statusSize'),
    statusSizeValue: document.getElementById('statusSizeValue'),
    metadataSize: document.getElementById('metadataSize'),
    metadataSizeValue: document.getElementById('metadataSizeValue'),

    // Effects
    glowIntensity: document.getElementById('glowIntensity'),
    glowIntensityValue: document.getElementById('glowIntensityValue'),
    particlesToggle: document.getElementById('particlesToggle'),

    // Visibility
    showProjectName: document.getElementById('showProjectName'),
    showTimecode: document.getElementById('showTimecode'),
    showBarsBeats: document.getElementById('showBarsBeats'),
    showRegionName: document.getElementById('showRegionName'),
    showTempo: document.getElementById('showTempo'),
    showDateTime: document.getElementById('showDateTime'),
    showSessionTimer: document.getElementById('showSessionTimer'),

    // Studio
    studioName: document.getElementById('studioName'),
    engineerName: document.getElementById('engineerName'),

    // Network
    wsPort: document.getElementById('wsPort'),
    connectionStatus: document.getElementById('connectionStatus'),

    // Actions
    resetDefaults: document.getElementById('resetDefaults'),
    saveSettings: document.getElementById('saveSettings')
};

// Default settings (must match main.js)
const defaultSettings = {
    display: {
        selectedMonitor: 0,
        fullscreen: true,
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
        timecode: true,
        barsBeats: true,
        projectName: true,
        regionName: true,
        tempo: false,
        dateTime: true,
        sessionTimer: true
    },
    studio: {
        name: 'Falcon Studios',
        engineer: ''
    },
    network: {
        port: 9999
    }
};

/**
 * Initialize settings panel
 */
async function init() {
    // Load current settings
    settings = await window.studioBeacon.getSettings();

    // Listen for updates
    window.studioBeacon.onSettingsUpdate((newSettings) => {
        settings = newSettings;
        populateForm();
    });

    window.studioBeacon.onMonitorsList((monitorList) => {
        monitors = monitorList;
        populateMonitors();
    });

    // Set up event listeners
    setupEventListeners();

    // Populate form with current settings
    populateForm();
}

/**
 * Populate monitor dropdown
 */
function populateMonitors() {
    elements.monitorSelect.innerHTML = '';
    monitors.forEach((monitor, index) => {
        const option = document.createElement('option');
        option.value = index;
        option.textContent = monitor.label + (monitor.primary ? ' (Primary)' : '');
        option.selected = index === settings.display.selectedMonitor;
        elements.monitorSelect.appendChild(option);
    });
}

/**
 * Populate form with settings
 */
function populateForm() {
    // Display
    elements.fullscreenToggle.checked = settings.display?.fullscreen ?? true;
    elements.alwaysOnTopToggle.checked = settings.display?.alwaysOnTop ?? true;

    // Theme
    elements.bgColor.value = settings.theme?.background || '#0d0d12';
    elements.accentPrimary.value = settings.theme?.accentPrimary || '#FF4D6D';
    elements.accentSecondary.value = settings.theme?.accentSecondary || '#4DEDAB';
    elements.textPrimary.value = settings.theme?.textPrimary || '#FFFFFF';
    elements.textSecondary.value = settings.theme?.textSecondary || '#8B8BA3';

    // Typography
    elements.fontFamily.value = settings.typography?.fontFamily || 'Inter';
    elements.statusSize.value = settings.typography?.statusSize || 140;
    elements.statusSizeValue.textContent = `${elements.statusSize.value}px`;
    elements.metadataSize.value = settings.typography?.metadataSize || 28;
    elements.metadataSizeValue.textContent = `${elements.metadataSize.value}px`;

    // Effects
    const glowPercent = Math.round((settings.effects?.glowIntensity || 0.8) * 100);
    elements.glowIntensity.value = glowPercent;
    elements.glowIntensityValue.textContent = `${glowPercent}%`;
    elements.particlesToggle.checked = settings.effects?.particlesEnabled ?? true;

    // Visibility
    elements.showProjectName.checked = settings.visibility?.projectName ?? true;
    elements.showTimecode.checked = settings.visibility?.timecode ?? true;
    elements.showBarsBeats.checked = settings.visibility?.barsBeats ?? true;
    elements.showRegionName.checked = settings.visibility?.regionName ?? true;
    elements.showTempo.checked = settings.visibility?.tempo ?? false;
    elements.showDateTime.checked = settings.visibility?.dateTime ?? true;
    elements.showSessionTimer.checked = settings.visibility?.sessionTimer ?? true;

    // Studio
    elements.studioName.value = settings.studio?.name || '';
    elements.engineerName.value = settings.studio?.engineer || '';

    // Network
    elements.wsPort.value = settings.network?.port || 9999;
}

/**
 * Gather settings from form
 */
function gatherSettings() {
    return {
        display: {
            selectedMonitor: parseInt(elements.monitorSelect.value) || 0,
            fullscreen: elements.fullscreenToggle.checked,
            alwaysOnTop: elements.alwaysOnTopToggle.checked
        },
        theme: {
            background: elements.bgColor.value,
            accentPrimary: elements.accentPrimary.value,
            accentSecondary: elements.accentSecondary.value,
            textPrimary: elements.textPrimary.value,
            textSecondary: elements.textSecondary.value
        },
        typography: {
            fontFamily: elements.fontFamily.value,
            statusSize: parseInt(elements.statusSize.value),
            metadataSize: parseInt(elements.metadataSize.value),
            fontWeight: 600
        },
        effects: {
            glowIntensity: parseInt(elements.glowIntensity.value) / 100,
            animationSpeed: 1.0,
            particlesEnabled: elements.particlesToggle.checked
        },
        visibility: {
            projectName: elements.showProjectName.checked,
            timecode: elements.showTimecode.checked,
            barsBeats: elements.showBarsBeats.checked,
            regionName: elements.showRegionName.checked,
            tempo: elements.showTempo.checked,
            dateTime: elements.showDateTime.checked,
            sessionTimer: elements.showSessionTimer.checked
        },
        studio: {
            name: elements.studioName.value,
            engineer: elements.engineerName.value
        },
        network: {
            port: parseInt(elements.wsPort.value) || 9999
        }
    };
}

/**
 * Set up event listeners
 */
function setupEventListeners() {
    // Range sliders - update value display
    elements.statusSize.addEventListener('input', () => {
        elements.statusSizeValue.textContent = `${elements.statusSize.value}px`;
    });

    elements.metadataSize.addEventListener('input', () => {
        elements.metadataSizeValue.textContent = `${elements.metadataSize.value}px`;
    });

    elements.glowIntensity.addEventListener('input', () => {
        elements.glowIntensityValue.textContent = `${elements.glowIntensity.value}%`;
    });

    // Monitor change
    elements.monitorSelect.addEventListener('change', () => {
        const monitorId = parseInt(elements.monitorSelect.value);
        window.studioBeacon.changeMonitor(monitorId);
    });

    // Save button
    elements.saveSettings.addEventListener('click', () => {
        const newSettings = gatherSettings();
        window.studioBeacon.saveSettings(newSettings);

        // Visual feedback
        elements.saveSettings.textContent = 'Saved!';
        elements.saveSettings.style.background = '#4DEDAB';

        // Close window after brief delay to show feedback
        setTimeout(() => {
            window.studioBeacon.closeSettingsSave();
        }, 500);
    });

    // Reset defaults button
    elements.resetDefaults.addEventListener('click', () => {
        if (confirm('Reset all settings to defaults?')) {
            settings = { ...defaultSettings };
            populateForm();
            window.studioBeacon.saveSettings(settings);
        }
    });
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', init);
