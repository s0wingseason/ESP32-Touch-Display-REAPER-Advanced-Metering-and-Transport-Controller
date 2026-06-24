/**
 * StudioBeacon - Windows Registry Helper
 * 
 * Provides fallback storage for settings when file-based storage fails.
 * Uses Windows Registry for persistent, reliable storage.
 */

const { exec } = require('child_process');
const util = require('util');
const execAsync = util.promisify(exec);

const REGISTRY_PATH = 'HKCU\\Software\\StudioBeacon';

class RegistryHelper {
    /**
     * Check if we're on Windows
     */
    static isWindows() {
        return process.platform === 'win32';
    }

    /**
     * Set a registry value
     */
    static async setValue(name, value, type = 'REG_SZ') {
        if (!this.isWindows()) return false;

        try {
            // Escape the value for command line
            const escapedValue = String(value).replace(/"/g, '\\"');
            const cmd = `reg add "${REGISTRY_PATH}" /v "${name}" /t ${type} /d "${escapedValue}" /f`;
            await execAsync(cmd);
            return true;
        } catch (error) {
            console.error(`Registry: Failed to set ${name}:`, error.message);
            return false;
        }
    }

    /**
     * Get a registry value
     */
    static async getValue(name) {
        if (!this.isWindows()) return null;

        try {
            const cmd = `reg query "${REGISTRY_PATH}" /v "${name}"`;
            const { stdout } = await execAsync(cmd);

            // Parse the output to extract the value
            const lines = stdout.split('\n');
            for (const line of lines) {
                if (line.includes(name)) {
                    const parts = line.trim().split(/\s{2,}/);
                    if (parts.length >= 3) {
                        return parts[2];
                    }
                }
            }
            return null;
        } catch (error) {
            // Key doesn't exist or other error
            return null;
        }
    }

    /**
     * Delete a registry value
     */
    static async deleteValue(name) {
        if (!this.isWindows()) return false;

        try {
            const cmd = `reg delete "${REGISTRY_PATH}" /v "${name}" /f`;
            await execAsync(cmd);
            return true;
        } catch (error) {
            return false;
        }
    }

    /**
     * Store encrypted API key in registry (base64 encoded for safety)
     */
    static async setApiKey(apiKey) {
        const encoded = Buffer.from(apiKey).toString('base64');
        return this.setValue('GoveeApiKey', encoded);
    }

    /**
     * Get API key from registry
     */
    static async getApiKey() {
        const encoded = await this.getValue('GoveeApiKey');
        if (encoded) {
            return Buffer.from(encoded, 'base64').toString('utf-8');
        }
        return null;
    }

    /**
     * Store general settings in registry as JSON
     */
    static async setSettings(settings) {
        const json = JSON.stringify(settings);
        const encoded = Buffer.from(json).toString('base64');
        return this.setValue('Settings', encoded);
    }

    /**
     * Get settings from registry
     */
    static async getSettings() {
        const encoded = await this.getValue('Settings');
        if (encoded) {
            try {
                const json = Buffer.from(encoded, 'base64').toString('utf-8');
                return JSON.parse(json);
            } catch (e) {
                return null;
            }
        }
        return null;
    }

    /**
     * Store Govee device selection
     */
    static async setSelectedDevices(deviceIds) {
        return this.setValue('GoveeDevices', deviceIds.join(','));
    }

    /**
     * Get Govee device selection
     */
    static async getSelectedDevices() {
        const value = await this.getValue('GoveeDevices');
        if (value) {
            return value.split(',').filter(id => id.trim());
        }
        return [];
    }
}

module.exports = { RegistryHelper };
