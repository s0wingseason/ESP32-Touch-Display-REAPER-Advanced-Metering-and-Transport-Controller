/**
 * StudioBeacon - Govee Service Manager
 * 
 * Manages Govee integration lifecycle, status subscriptions,
 * and user configuration. Supports:
 * - Smart lights (bulbs, strips, light bars)
 * - Smart thermostats
 * - Motion sensors
 * - Other Govee IoT devices
 */

const { GoveeAPI, DEFAULT_STATUS_MAPPINGS, TRANSITION_OPTIONS } = require('./govee-api');
const { RegistryHelper } = require('../utils/registry-helper');
const { safeStorage } = require('electron');
const fs = require('fs');
const path = require('path');

// Device type categories for smart handling
const DEVICE_CATEGORIES = {
    LIGHT: ['H6', 'H7', 'H8', 'H0'], // Light products (bulbs, strips, bars)
    THERMOSTAT: ['H5', 'H4'],
    SENSOR: ['H3'],
    APPLIANCE: ['H2', 'H1']
};

class GoveeService {
    constructor(configDir) {
        this.configDir = configDir;
        this.configFile = path.join(configDir, 'govee-config.json');
        this.api = null;
        this.config = this.getDefaultConfig();
        this.isInitialized = false;
        this.lastStatus = null;
        this.statusTimeout = null;
        this.registryLoaded = false;
    }

    /**
     * Default configuration
     */
    getDefaultConfig() {
        return {
            enabled: false,
            apiKeyEncrypted: null,
            selectedDevices: [], // Array of device IDs to control
            statusMappings: { ...DEFAULT_STATUS_MAPPINGS },
            transitionMs: 3000, // Default 3 second fade
            restoreOnExit: true,
            debounceMs: 500, // Prevent rapid status changes
            brightnessOverride: null, // If set, overrides all brightness values
            groupByZone: false, // Different devices for different zones
            zones: {}, // Zone-specific device assignments
            thermostatSettings: {
                enabled: false,
                adjustOnRecording: false, // Lower fan noise during recording
                recordingTemp: 72, // Target temp during recording
                normalTemp: 70
            },
            sensorTriggersEnabled: false // React to motion sensors
        };
    }

    /**
     * Load configuration from disk with Registry fallback
     */
    async loadConfig() {
        let loaded = false;

        // Try file-based config first
        try {
            if (fs.existsSync(this.configFile)) {
                const data = fs.readFileSync(this.configFile, 'utf-8');
                const parsed = JSON.parse(data);
                this.config = { ...this.getDefaultConfig(), ...parsed };
                console.log('Govee: Configuration loaded from file');
                loaded = true;
            }
        } catch (error) {
            console.error('Govee: Failed to load config from file:', error.message);
        }

        // If file failed, try Registry fallback
        if (!loaded && RegistryHelper.isWindows()) {
            try {
                const regSettings = await RegistryHelper.getSettings();
                if (regSettings) {
                    this.config = { ...this.getDefaultConfig(), ...regSettings };
                    this.registryLoaded = true;
                    console.log('Govee: Configuration loaded from Registry fallback');
                    loaded = true;
                }

                // Also try to get API key from registry
                if (!this.config.apiKeyEncrypted) {
                    const regApiKey = await RegistryHelper.getApiKey();
                    if (regApiKey) {
                        // Store it in the encrypted format
                        this.setApiKey(regApiKey);
                        console.log('Govee: API key recovered from Registry');
                    }
                }
            } catch (error) {
                console.error('Govee: Failed to load from Registry:', error.message);
            }
        }

        return this.config;
    }

    /**
     * Save configuration to disk and Registry backup
     */
    async saveConfig() {
        // Save to file
        try {
            if (!fs.existsSync(this.configDir)) {
                fs.mkdirSync(this.configDir, { recursive: true });
            }
            fs.writeFileSync(this.configFile, JSON.stringify(this.config, null, 2));
            console.log('Govee: Configuration saved to file');
        } catch (error) {
            console.error('Govee: Failed to save config to file:', error.message);
        }

        // Also save to Registry as backup (if on Windows)
        if (RegistryHelper.isWindows()) {
            try {
                // Save settings (without encrypted key, that's stored separately)
                const settingsForRegistry = { ...this.config };
                delete settingsForRegistry.apiKeyEncrypted;
                await RegistryHelper.setSettings(settingsForRegistry);

                // Save selected devices separately for quick access
                if (this.config.selectedDevices.length > 0) {
                    await RegistryHelper.setSelectedDevices(this.config.selectedDevices);
                }
            } catch (error) {
                console.error('Govee: Failed to save to Registry backup:', error.message);
            }
        }
    }

    /**
     * Securely store the API key with Registry backup
     */
    async setApiKey(apiKey) {
        try {
            if (safeStorage.isEncryptionAvailable()) {
                const encrypted = safeStorage.encryptString(apiKey);
                this.config.apiKeyEncrypted = encrypted.toString('base64');
            } else {
                // Fallback - not as secure but functional
                this.config.apiKeyEncrypted = Buffer.from(apiKey).toString('base64');
            }
            await this.saveConfig();

            // Also backup to Windows Registry
            if (RegistryHelper.isWindows()) {
                await RegistryHelper.setApiKey(apiKey);
                console.log('Govee: API key backed up to Registry');
            }

            return true;
        } catch (error) {
            console.error('Govee: Failed to store API key:', error.message);
            return false;
        }
    }

    /**
     * Retrieve the API key (with Registry fallback)
     */
    async getApiKey() {
        // Try encrypted storage first
        if (this.config.apiKeyEncrypted) {
            try {
                const encrypted = Buffer.from(this.config.apiKeyEncrypted, 'base64');
                if (safeStorage.isEncryptionAvailable()) {
                    return safeStorage.decryptString(encrypted);
                } else {
                    return Buffer.from(this.config.apiKeyEncrypted, 'base64').toString('utf-8');
                }
            } catch (error) {
                console.error('Govee: Failed to decrypt API key:', error.message);
            }
        }

        // Fallback to Registry
        if (RegistryHelper.isWindows()) {
            const regApiKey = await RegistryHelper.getApiKey();
            if (regApiKey) {
                console.log('Govee: Retrieved API key from Registry fallback');
                return regApiKey;
            }
        }

        return null;
    }

    /**
     * Clear the stored API key from all locations
     */
    async clearApiKey() {
        this.config.apiKeyEncrypted = null;
        await this.saveConfig();

        // Also clear from Registry
        if (RegistryHelper.isWindows()) {
            await RegistryHelper.deleteValue('GoveeApiKey');
        }

        this.api = null;
        this.isInitialized = false;
    }

    /**
     * Categorize a device by its SKU prefix
     */
    getDeviceCategory(sku) {
        if (!sku) return 'UNKNOWN';
        const prefix = sku.substring(0, 2).toUpperCase();

        for (const [category, prefixes] of Object.entries(DEVICE_CATEGORIES)) {
            if (prefixes.includes(prefix)) {
                return category;
            }
        }
        return 'OTHER';
    }

    /**
     * Check if a device is a light (can change color/brightness)
     */
    isLightDevice(device) {
        return this.getDeviceCategory(device.sku) === 'LIGHT';
    }

    /**
     * Check if a device is a thermostat
     */
    isThermostatDevice(device) {
        return this.getDeviceCategory(device.sku) === 'THERMOSTAT';
    }

    /**
     * Check if a device is a sensor
     */
    isSensorDevice(device) {
        return this.getDeviceCategory(device.sku) === 'SENSOR';
    }

    /**
     * Initialize the Govee service
     */
    async initialize() {
        if (!this.config.enabled) {
            console.log('Govee: Integration disabled');
            return { success: false, message: 'Govee integration is disabled' };
        }

        const apiKey = this.getApiKey();
        if (!apiKey) {
            console.log('Govee: No API key configured');
            return { success: false, message: 'No API key configured' };
        }

        try {
            this.api = new GoveeAPI(apiKey);
            const result = await this.api.testConnection();

            if (result.success) {
                this.isInitialized = true;

                // Capture current device states for restoration
                if (this.config.restoreOnExit) {
                    await this.api.captureAllDeviceStates();
                }

                console.log('Govee: Service initialized successfully');
                return result;
            } else {
                return result;
            }
        } catch (error) {
            console.error('Govee: Initialization failed:', error.message);
            return { success: false, message: error.message };
        }
    }

    /**
     * Shutdown the Govee service
     */
    async shutdown() {
        if (!this.isInitialized || !this.api) return;

        console.log('Govee: Shutting down...');

        // Restore devices to previous state
        if (this.config.restoreOnExit) {
            try {
                await this.api.restoreAllDeviceStates();
            } catch (error) {
                console.error('Govee: Failed to restore device states:', error.message);
            }
        }

        this.isInitialized = false;
        console.log('Govee: Shutdown complete');
    }

    /**
     * Handle status change from StudioBeacon
     */
    async onStatusChange(status) {
        if (!this.config.enabled || !this.isInitialized || !this.api) {
            return;
        }

        // Debounce rapid changes
        if (this.statusTimeout) {
            clearTimeout(this.statusTimeout);
        }

        this.statusTimeout = setTimeout(async () => {
            await this.applyStatusLighting(status);
        }, this.config.debounceMs);
    }

    /**
     * Apply lighting for a given status
     */
    async applyStatusLighting(status) {
        if (!this.api || !status) return;

        // Get the lighting profile for this status
        let profile = this.config.statusMappings[status];

        if (!profile) {
            // Fallback to stopped/default
            profile = this.config.statusMappings['stopped'] || DEFAULT_STATUS_MAPPINGS['stopped'];
        }

        // Apply brightness override if set
        if (this.config.brightnessOverride !== null) {
            profile = { ...profile, brightness: this.config.brightnessOverride };
        }

        // Get devices to control
        const deviceIds = this.config.selectedDevices.length > 0
            ? this.config.selectedDevices
            : this.api.devices.map(d => d.id);

        if (deviceIds.length === 0) {
            console.log('Govee: No devices selected for control');
            return;
        }

        console.log(`Govee: Applying ${status} lighting to ${deviceIds.length} device(s)`);

        try {
            await this.api.applyProfileToDevices(
                deviceIds,
                profile,
                this.config.transitionMs
            );
            this.lastStatus = status;
        } catch (error) {
            console.error('Govee: Failed to apply lighting:', error.message);
        }
    }

    /**
     * Test lighting on selected devices
     */
    async testLighting(profile) {
        if (!this.api) {
            await this.initialize();
        }

        if (!this.api) {
            return { success: false, message: 'Govee not initialized' };
        }

        const deviceIds = this.config.selectedDevices.length > 0
            ? this.config.selectedDevices
            : this.api.devices.map(d => d.id);

        try {
            await this.api.applyProfileToDevices(deviceIds, profile, 0);
            return { success: true, message: 'Test lighting applied' };
        } catch (error) {
            return { success: false, message: error.message };
        }
    }

    /**
     * Get list of discovered devices
     */
    async getDevices() {
        if (!this.api) {
            const apiKey = this.getApiKey();
            if (!apiKey) {
                return { success: false, devices: [], message: 'No API key' };
            }
            this.api = new GoveeAPI(apiKey);
        }

        try {
            const devices = await this.api.discoverDevices();
            return { success: true, devices };
        } catch (error) {
            return { success: false, devices: [], message: error.message };
        }
    }

    /**
     * Update configuration
     */
    async updateConfig(updates) {
        this.config = { ...this.config, ...updates };
        await this.saveConfig();

        // Re-initialize if enabled status changed
        if (updates.enabled !== undefined) {
            if (updates.enabled && !this.isInitialized) {
                await this.initialize();
            } else if (!updates.enabled) {
                await this.shutdown();
            }
        }
    }

    /**
     * Get current configuration (safe for display - no API key)
     */
    getConfigForDisplay() {
        return {
            ...this.config,
            apiKeyEncrypted: undefined,
            hasApiKey: !!this.config.apiKeyEncrypted
        };
    }

    /**
     * Get available transition options
     */
    getTransitionOptions() {
        return TRANSITION_OPTIONS;
    }

    /**
     * Get default status mappings
     */
    getDefaultMappings() {
        return DEFAULT_STATUS_MAPPINGS;
    }
}

module.exports = { GoveeService };
