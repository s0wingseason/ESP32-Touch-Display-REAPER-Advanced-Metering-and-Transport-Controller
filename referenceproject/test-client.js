/**
 * StudioBeacon Demo Test Client
 * 
 * Run this script to simulate VST plugin connection for testing the display app.
 * Usage: node test-client.js
 */

const WebSocket = require('ws');

const PORT = 9999;
let ws = null;

// Transport states to cycle through
const transportStates = ['stopped', 'playing', 'recording', 'paused', 'comping'];
let currentStateIndex = 0;

// Mock project data
const mockData = {
    projectName: 'StudioBeacon Demo Session',
    timecode: '01:23:45:12',
    barsBeats: '32.3.240',
    regionName: 'Verse 2',
    tempo: 128,
    timeSignature: '4/4'
};

function connect() {
    console.log(`Connecting to StudioBeacon Display on port ${PORT}...`);

    ws = new WebSocket(`ws://localhost:${PORT}`);

    ws.on('open', () => {
        console.log('✅ Connected to StudioBeacon Display!');
        console.log('');
        console.log('Commands:');
        console.log('  1 - Stopped');
        console.log('  2 - Playing');
        console.log('  3 - Recording');
        console.log('  4 - Paused');
        console.log('  5 - Comping');
        console.log('  c - Cycle through states');
        console.log('  q - Quit');
        console.log('');

        // Send initial state
        sendFullState();
    });

    ws.on('message', (data) => {
        try {
            const message = JSON.parse(data.toString());
            console.log('Received:', message.type);
        } catch (e) { }
    });

    ws.on('close', () => {
        console.log('❌ Disconnected');
        setTimeout(connect, 3000);
    });

    ws.on('error', (err) => {
        console.log('⚠️ Connection failed. Is StudioBeacon Display running?');
    });
}

function sendTransport(state) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'transport',
            state: state
        }));
        console.log(`→ Transport: ${state.toUpperCase()}`);
    }
}

function sendFullState() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'full-state',
            state: {
                transport: transportStates[currentStateIndex],
                ...mockData
            }
        }));
    }
}

function updateTimecode() {
    // Simulate timecode advancement
    const parts = mockData.timecode.split(':').map(Number);
    parts[3]++;
    if (parts[3] >= 30) { parts[3] = 0; parts[2]++; }
    if (parts[2] >= 60) { parts[2] = 0; parts[1]++; }
    if (parts[1] >= 60) { parts[1] = 0; parts[0]++; }

    mockData.timecode = parts.map(p => p.toString().padStart(2, '0')).join(':');

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'metadata',
            data: { timecode: mockData.timecode }
        }));
    }
}

// Connect
connect();

// Handle keyboard input
const readline = require('readline');
readline.emitKeypressEvents(process.stdin);
if (process.stdin.isTTY) {
    process.stdin.setRawMode(true);
}

process.stdin.on('keypress', (str, key) => {
    if (key.ctrl && key.name === 'c' || key.name === 'q') {
        console.log('\nGoodbye!');
        process.exit();
    }

    switch (str) {
        case '1':
            currentStateIndex = 0;
            sendTransport('stopped');
            break;
        case '2':
            currentStateIndex = 1;
            sendTransport('playing');
            break;
        case '3':
            currentStateIndex = 2;
            sendTransport('recording');
            break;
        case '4':
            currentStateIndex = 3;
            sendTransport('paused');
            break;
        case '5':
            currentStateIndex = 4;
            sendTransport('comping');
            break;
        case 'c':
            currentStateIndex = (currentStateIndex + 1) % transportStates.length;
            sendTransport(transportStates[currentStateIndex]);
            break;
    }
});

// Update timecode periodically when playing/recording
setInterval(() => {
    const state = transportStates[currentStateIndex];
    if (state === 'playing' || state === 'recording') {
        updateTimecode();
    }
}, 1000);

console.log('Starting StudioBeacon Test Client...\n');
