/**
 * StudioBeacon - Govee Smart Lighting Integration
 * 
 * Provides API client for Govee cloud services to control smart lights
 * based on studio status changes.
 */

const https = require('https');

// Govee API Configuration
const GOVEE_API_BASE = 'openapi.api.govee.com';
const GOVEE_API_VERSION = 'v1';

// Rate limiting
const RATE_LIMIT_PER_MINUTE = 10;
const RATE_LIMIT_DAILY = 10000;

class GoveeAPI {
    constructor(apiKey) {
        this.apiKey = apiKey;
        this.devices = [];
        this.deviceStates = new Map(); // Store previous states for restoration
        this.requestQueue = [];
        this.requestsThisMinute = 0;
        this.requestsToday = 0;
        this.lastMinuteReset = Date.now();
        this.lastDayReset = Date.now();
        this.isProcessingQueue = false;
    }

    /**
     * Make an API request to Govee
     */
    async request(method, endpoint, body = null) {
        return new Promise((resolve, reject) => {
            const options = {
                hostname: GOVEE_API_BASE,
                port: 443,
                path: `/router/api/${GOVEE_API_VERSION}${endpoint}`,
                method: method,
                headers: {
                    'Govee-API-Key': this.apiKey,
                    'Content-Type': 'application/json'
                }
            };

            const req = https.request(options, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    try {
                        const parsed = JSON.parse(data);
                        if (res.statusCode >= 200 && res.statusCode < 300) {
                            resolve(parsed);
                        } else {
                            reject(new Error(parsed.message || `HTTP ${res.statusCode}`));
                        }
                    } catch (e) {
                        reject(new Error(`Failed to parse response: ${data}`));
                    }
                });
            });

            req.on('error', reject);
            req.setTimeout(10000, () => {
                req.destroy();
                reject(new Error('Request timeout'));
            });

            if (body) {
                req.write(JSON.stringify(body));
            }
            req.end();
        });
    }

    /**
     * Check rate limits and queue if necessary
     */
    async checkRateLimits() {
        const now = Date.now();

        // Reset minute counter
        if (now - this.lastMinuteReset > 60000) {
            this.requestsThisMinute = 0;
            this.lastMinuteReset = now;
        }

        // Reset daily counter
        if (now - this.lastDayReset > 86400000) {
            this.requestsToday = 0;
            this.lastDayReset = now;
        }

        // Check limits
        if (this.requestsToday >= RATE_LIMIT_DAILY) {
            throw new Error('Daily rate limit reached');
        }

        if (this.requestsThisMinute >= RATE_LIMIT_PER_MINUTE) {
            // Wait until next minute
            const waitTime = 60000 - (now - this.lastMinuteReset);
            await new Promise(resolve => setTimeout(resolve, waitTime));
            this.requestsThisMinute = 0;
            this.lastMinuteReset = Date.now();
        }

        this.requestsThisMinute++;
        this.requestsToday++;
    }

    /**
     * Discover all devices on the account
     */
    async discoverDevices() {
        try {
            await this.checkRateLimits();
            const response = await this.request('GET', '/user/devices');

            if (response.data && Array.isArray(response.data)) {
                this.devices = response.data.map(device => ({
                    id: device.device,
                    sku: device.sku,
                    name: device.deviceName || device.device,
                    type: device.type || 'unknown',
                    capabilities: device.capabilities || [],
                    controllable: device.controllable !== false,
                    retrievable: device.retrievable !== false
                }));

                console.log(`Govee: Discovered ${this.devices.length} devices`);
                return this.devices;
            }

            return [];
        } catch (error) {
            console.error('Govee: Device discovery failed:', error.message);
            throw error;
        }
    }

    /**
     * Get current state of a device (for restoration later)
     */
    async getDeviceState(device) {
        try {
            await this.checkRateLimits();
            const response = await this.request('POST', '/device/state', {
                requestId: this.generateRequestId(),
                payload: {
                    sku: device.sku,
                    device: device.id
                }
            });

            if (response.payload) {
                this.deviceStates.set(device.id, response.payload);
                return response.payload;
            }

            return null;
        } catch (error) {
            console.error(`Govee: Failed to get state for ${device.name}:`, error.message);
            return null;
        }
    }

    /**
     * Store current states of all devices for later restoration
     */
    async captureAllDeviceStates() {
        console.log('Govee: Capturing device states for restoration...');
        for (const device of this.devices) {
            if (device.retrievable) {
                await this.getDeviceState(device);
            }
        }
        console.log(`Govee: Captured ${this.deviceStates.size} device states`);
    }

    /**
     * Restore all devices to their previous states
     */
    async restoreAllDeviceStates() {
        console.log('Govee: Restoring device states...');
        for (const [deviceId, state] of this.deviceStates) {
            const device = this.devices.find(d => d.id === deviceId);
            if (device && state.capabilities) {
                for (const cap of state.capabilities) {
                    try {
                        await this.sendCommand(device, cap.type, cap.instance, cap.state);
                    } catch (e) {
                        console.error(`Govee: Failed to restore ${device.name}:`, e.message);
                    }
                }
            }
        }
        console.log('Govee: Device states restored');
    }

    /**
     * Send a control command to a device
     */
    async sendCommand(device, capabilityType, instance, value) {
        try {
            await this.checkRateLimits();

            const payload = {
                requestId: this.generateRequestId(),
                payload: {
                    sku: device.sku,
                    device: device.id,
                    capability: {
                        type: capabilityType,
                        instance: instance,
                        value: value
                    }
                }
            };

            const response = await this.request('POST', '/device/control', payload);
            return { success: true, response };
        } catch (error) {
            console.error(`Govee: Command failed for ${device.name}:`, error.message);
            return { success: false, error: error.message };
        }
    }

    /**
     * Turn device on/off
     */
    async setPower(device, on) {
        return this.sendCommand(
            device,
            'devices.capabilities.on_off',
            'powerSwitch',
            on ? 1 : 0
        );
    }

    /**
     * Set device color (RGB)
     * @param {object} device - Device object
     * @param {string} hexColor - Hex color string like "#FF0000"
     */
    async setColor(device, hexColor) {
        // Convert hex to RGB integer
        const hex = hexColor.replace('#', '');
        const r = parseInt(hex.substring(0, 2), 16);
        const g = parseInt(hex.substring(2, 4), 16);
        const b = parseInt(hex.substring(4, 6), 16);
        const rgbInt = (r << 16) + (g << 8) + b;

        return this.sendCommand(
            device,
            'devices.capabilities.color_setting',
            'colorRgb',
            rgbInt
        );
    }

    /**
     * Set device brightness (0-100)
     */
    async setBrightness(device, brightness) {
        return this.sendCommand(
            device,
            'devices.capabilities.range',
            'brightness',
            Math.max(0, Math.min(100, brightness))
        );
    }

    /**
     * Set color temperature (in Kelvin, typically 2000-9000)
     */
    async setColorTemperature(device, kelvin) {
        return this.sendCommand(
            device,
            'devices.capabilities.color_setting',
            'colorTemperatureK',
            kelvin
        );
    }

    /**
     * Apply a complete lighting profile to a device
     * @param {object} device - Device object
     * @param {object} profile - { color: "#FF0000", brightness: 100, power: true }
     * @param {number} transitionMs - Transition time in milliseconds (for software fade)
     */
    async applyProfile(device, profile, transitionMs = 3000) {
        const results = [];

        // Turn on first if specified
        if (profile.power !== undefined) {
            results.push(await this.setPower(device, profile.power));
        }

        // Set color if specified
        if (profile.color) {
            results.push(await this.setColor(device, profile.color));
        }

        // Set brightness if specified
        if (profile.brightness !== undefined) {
            results.push(await this.setBrightness(device, profile.brightness));
        }

        // Set color temperature if specified
        if (profile.colorTemp) {
            results.push(await this.setColorTemperature(device, profile.colorTemp));
        }

        return results;
    }

    /**
     * Apply profile to multiple devices
     */
    async applyProfileToDevices(deviceIds, profile, transitionMs = 3000) {
        const results = [];

        for (const deviceId of deviceIds) {
            const device = this.devices.find(d => d.id === deviceId);
            if (device && device.controllable) {
                const result = await this.applyProfile(device, profile, transitionMs);
                results.push({ deviceId, results: result });
            }
        }

        return results;
    }

    /**
     * Generate unique request ID
     */
    generateRequestId() {
        return `sb-${Date.now()}-${Math.random().toString(36).substring(2, 9)}`;
    }

    /**
     * Test connection with API key
     */
    async testConnection() {
        try {
            await this.discoverDevices();
            return {
                success: true,
                message: `Connected! Found ${this.devices.length} devices.`,
                devices: this.devices
            };
        } catch (error) {
            return {
                success: false,
                message: error.message
            };
        }
    }
}

// Default status-to-lighting mappings
const DEFAULT_STATUS_MAPPINGS = {
    // Recording family - RED (Do not enter)
    recording: { color: '#FF0000', brightness: 100, power: true },
    arming: { color: '#FF3300', brightness: 80, power: true },
    bouncing: { color: '#FF4444', brightness: 90, power: true },
    rendering: { color: '#FF6600', brightness: 85, power: true },

    // Playback family - GREEN (In session)
    playing: { color: '#00FF00', brightness: 80, power: true },
    comping: { color: '#00CC00', brightness: 75, power: true },
    mixing: { color: '#33FF33', brightness: 70, power: true },
    mastering: { color: '#66FF66', brightness: 75, power: true },
    reviewing: { color: '#00FF66', brightness: 70, power: true },

    // Monitoring family - BLUE (Focused work)
    monitoring: { color: '#0066FF', brightness: 70, power: true },
    soundcheck: { color: '#0099FF', brightness: 75, power: true },
    calibrating: { color: '#00CCFF', brightness: 80, power: true },

    // Away family - AMBER (Unavailable)
    oncall: { color: '#FFAA00', brightness: 90, power: true },
    away: { color: '#FF8800', brightness: 60, power: true },
    lunch: { color: '#FFCC00', brightness: 50, power: true },
    meeting: { color: '#FF9900', brightness: 80, power: true },

    // Talent family - PURPLE (Special attention)
    talentready: { color: '#FF00FF', brightness: 85, power: true },
    talentinbooth: { color: '#CC00FF', brightness: 90, power: true },

    // Session family - CYAN
    sessionending: { color: '#00FFFF', brightness: 70, power: true },
    sessioncomplete: { color: '#66FFFF', brightness: 60, power: true },

    // Warning family - MAGENTA/RED
    technicalissue: { color: '#FF0066', brightness: 100, power: true },
    donotenter: { color: '#FF0000', brightness: 100, power: true },
    quiet: { color: '#FF3366', brightness: 80, power: true },

    // Available/Stopped - WHITE (Come in)
    stopped: { color: '#FFFFFF', brightness: 50, power: true },
    paused: { color: '#CCCCFF', brightness: 60, power: true },
    available: { color: '#FFFFFF', brightness: 70, power: true },

    // Custom - use last custom color or default white
    custom: { color: '#FFFFFF', brightness: 80, power: true }
};

// Transition time options (in milliseconds)
const TRANSITION_OPTIONS = [
    { label: 'Instant', value: 0 },
    { label: '0.25 seconds', value: 250 },
    { label: '0.5 seconds', value: 500 },
    { label: '1 second', value: 1000 },
    { label: '1.5 seconds', value: 1500 },
    { label: '3 seconds (Default)', value: 3000 },
    { label: '5 seconds', value: 5000 },
    { label: '10 seconds', value: 10000 },
    { label: '15 seconds', value: 15000 }
];

module.exports = {
    GoveeAPI,
    DEFAULT_STATUS_MAPPINGS,
    TRANSITION_OPTIONS
};
