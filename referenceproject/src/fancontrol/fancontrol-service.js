/**
 * StudioBeacon - FanControl Integration
 * 
 * Integrates with the FanControl application (getfancontrol.com) to
 * automatically reduce fan speeds during recording to minimize noise.
 * 
 * This is an OPT-IN feature that requires:
 * 1. FanControl to be installed
 * 2. User to enable this feature in settings
 * 3. Two FanControl configuration files to be created:
 *    - StudioBeacon_Recording.json (low fan speeds for recording)
 *    - StudioBeacon_Normal.json (normal/automatic fan control)
 */

const { exec, spawn } = require('child_process');
const util = require('util');
const execAsync = util.promisify(exec);
const fs = require('fs');
const path = require('path');

// Common FanControl installation paths
const FANCONTROL_PATHS = [
    'C:\\Program Files\\FanControl\\FanControl.exe',
    'C:\\Program Files (x86)\\FanControl\\FanControl.exe',
    path.join(process.env.LOCALAPPDATA || '', 'Programs', 'FanControl', 'FanControl.exe'),
    path.join(process.env.APPDATA || '', 'FanControl', 'FanControl.exe')
];

// FanControl config directory
const FANCONTROL_CONFIG_DIR = path.join(process.env.APPDATA || '', 'FanControl', 'Configurations');

class FanControlService {
    constructor(configDir) {
        this.configDir = configDir;
        this.configFile = path.join(configDir, 'fancontrol-config.json');
        this.config = this.getDefaultConfig();
        this.fanControlPath = null;
        this.isRecordingMode = false;
        this.previousConfig = null; // Store the active config before switching
    }

    /**
     * Default configuration
     */
    getDefaultConfig() {
        return {
            enabled: false,
            fanControlPath: null, // Auto-detected or user-specified
            recordingFanSpeed: 25, // Percentage (0-100)
            useCustomConfigs: false, // Use StudioBeacon_Recording/Normal configs
            recordingConfigName: 'StudioBeacon_Recording',
            normalConfigName: 'StudioBeacon_Normal',
            restoreOnExit: true,
            debounceMs: 1000 // Wait before switching to avoid rapid toggles
        };
    }

    /**
     * Load configuration
     */
    loadConfig() {
        try {
            if (fs.existsSync(this.configFile)) {
                const data = fs.readFileSync(this.configFile, 'utf-8');
                const loaded = JSON.parse(data);
                this.config = { ...this.getDefaultConfig(), ...loaded };
                console.log('FanControl: Configuration loaded');
            }
        } catch (error) {
            console.error('FanControl: Failed to load config:', error.message);
        }
        return this.config;
    }

    /**
     * Save configuration
     */
    saveConfig() {
        try {
            if (!fs.existsSync(this.configDir)) {
                fs.mkdirSync(this.configDir, { recursive: true });
            }
            fs.writeFileSync(this.configFile, JSON.stringify(this.config, null, 2));
            console.log('FanControl: Configuration saved');
        } catch (error) {
            console.error('FanControl: Failed to save config:', error.message);
        }
    }

    /**
     * Find FanControl installation
     */
    async findFanControl() {
        // Check user-specified path first
        if (this.config.fanControlPath && fs.existsSync(this.config.fanControlPath)) {
            this.fanControlPath = this.config.fanControlPath;
            return this.fanControlPath;
        }

        // Search common paths
        for (const testPath of FANCONTROL_PATHS) {
            if (fs.existsSync(testPath)) {
                this.fanControlPath = testPath;
                this.config.fanControlPath = testPath;
                this.saveConfig();
                console.log(`FanControl: Found at ${testPath}`);
                return testPath;
            }
        }

        // Try to find via Windows registry or where command
        try {
            const { stdout } = await execAsync('where FanControl.exe 2>nul');
            if (stdout.trim()) {
                this.fanControlPath = stdout.trim().split('\n')[0];
                console.log(`FanControl: Found via PATH at ${this.fanControlPath}`);
                return this.fanControlPath;
            }
        } catch (e) {
            // Not in PATH
        }

        console.log('FanControl: Application not found');
        return null;
    }

    /**
     * Check if FanControl is running
     */
    async isFanControlRunning() {
        try {
            const { stdout } = await execAsync('tasklist /FI "IMAGENAME eq FanControl.exe" /NH');
            return stdout.toLowerCase().includes('fancontrol.exe');
        } catch (e) {
            return false;
        }
    }

    /**
     * Initialize the service
     */
    async initialize() {
        if (!this.config.enabled) {
            console.log('FanControl: Integration disabled');
            return { success: false, message: 'FanControl integration is disabled' };
        }

        const fcPath = await this.findFanControl();
        if (!fcPath) {
            return {
                success: false,
                message: 'FanControl not found. Please install from getfancontrol.com or specify the path.'
            };
        }

        const isRunning = await this.isFanControlRunning();
        if (!isRunning) {
            return {
                success: false,
                message: 'FanControl is not running. Please start FanControl first.'
            };
        }

        console.log('FanControl: Service initialized');
        return { success: true, message: 'FanControl integration ready' };
    }

    /**
     * Switch FanControl to a specific configuration
     */
    async switchConfig(configName) {
        if (!this.fanControlPath) {
            await this.findFanControl();
        }

        if (!this.fanControlPath) {
            console.error('FanControl: Cannot switch config - FanControl not found');
            return false;
        }

        try {
            // Use the -c flag to switch configuration
            const cmd = `"${this.fanControlPath}" -c "${configName}"`;
            console.log(`FanControl: Switching to config "${configName}"`);

            await execAsync(cmd);
            return true;
        } catch (error) {
            console.error(`FanControl: Failed to switch config:`, error.message);
            return false;
        }
    }

    /**
     * Create default StudioBeacon configurations if they don't exist
     * Note: This creates placeholder configs - user should customize in FanControl
     */
    async createDefaultConfigs() {
        const configDir = FANCONTROL_CONFIG_DIR;

        if (!fs.existsSync(configDir)) {
            console.log('FanControl: Config directory not found. User needs to create configs in FanControl.');
            return false;
        }

        const recordingConfig = path.join(configDir, `${this.config.recordingConfigName}.json`);
        const normalConfig = path.join(configDir, `${this.config.normalConfigName}.json`);

        const missingConfigs = [];
        if (!fs.existsSync(recordingConfig)) {
            missingConfigs.push(this.config.recordingConfigName);
        }
        if (!fs.existsSync(normalConfig)) {
            missingConfigs.push(this.config.normalConfigName);
        }

        return {
            recordingExists: fs.existsSync(recordingConfig),
            normalExists: fs.existsSync(normalConfig),
            missingConfigs
        };
    }

    /**
     * Enter recording mode (reduce fan speeds)
     */
    async enterRecordingMode() {
        if (!this.config.enabled || this.isRecordingMode) {
            return;
        }

        console.log('FanControl: Entering recording mode (reducing fan speeds)');

        const success = await this.switchConfig(this.config.recordingConfigName);
        if (success) {
            this.isRecordingMode = true;
        }

        return success;
    }

    /**
     * Exit recording mode (restore normal fan speeds)
     */
    async exitRecordingMode() {
        if (!this.config.enabled || !this.isRecordingMode) {
            return;
        }

        console.log('FanControl: Exiting recording mode (restoring fan speeds)');

        const success = await this.switchConfig(this.config.normalConfigName);
        if (success) {
            this.isRecordingMode = false;
        }

        return success;
    }

    /**
     * Handle transport state change
     */
    async onTransportChange(transport) {
        if (!this.config.enabled) return;

        if (transport === 'recording') {
            await this.enterRecordingMode();
        } else if (this.isRecordingMode) {
            // Small delay to avoid rapid switching if user stops/starts quickly
            setTimeout(async () => {
                if (this.isRecordingMode) {
                    await this.exitRecordingMode();
                }
            }, this.config.debounceMs);
        }
    }

    /**
     * Shutdown - restore normal mode
     */
    async shutdown() {
        if (this.isRecordingMode && this.config.restoreOnExit) {
            await this.exitRecordingMode();
        }
    }

    /**
     * Update configuration
     */
    updateConfig(updates) {
        this.config = { ...this.config, ...updates };
        this.saveConfig();
    }

    /**
     * Get configuration for display (safe)
     */
    getConfigForDisplay() {
        return {
            ...this.config,
            isRecordingMode: this.isRecordingMode,
            fanControlInstalled: !!this.fanControlPath
        };
    }

    /**
     * Test the connection to FanControl
     */
    async testConnection() {
        const result = await this.initialize();
        if (result.success) {
            const configStatus = await this.createDefaultConfigs();
            return {
                ...result,
                configStatus,
                fanControlPath: this.fanControlPath
            };
        }
        return result;
    }
}

module.exports = { FanControlService };
