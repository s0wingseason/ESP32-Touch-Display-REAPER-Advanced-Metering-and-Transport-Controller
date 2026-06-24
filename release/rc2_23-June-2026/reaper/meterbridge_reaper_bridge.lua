--[[
    MeterBridge — REAPER Auto-Bridge
    
    This script runs inside REAPER and monitors transport state, meters,
    and metadata, writing a JSON state file that the MeterBridge relay
    reads and converts to binary UDP for the ESP32 CrowPanel display.
    
    Based on the proven StudioBeacon bridge (v3.4.0) by FalconEYE.
    
    Location: %APPDATA%\REAPER\Scripts\meterbridge_reaper_bridge.lua
    (or run manually via Actions > Show action list > Load...)
    
    Version: 1.0.0
]]--

-- Clear any stuck running state and set fresh
reaper.SetExtState("MeterBridge", "running", "1", false)

-- Show startup notification
reaper.ShowConsoleMsg("MeterBridge: Bridge script starting...\n")

-- Configuration
local UPDATE_INTERVAL = 0.05  -- Default: 20 updates/sec (50ms) — matches ESP32 g_update_interval_ms default

local IS_WINDOWS = reaper.GetOS():find("Win") ~= nil
local TEMP_PATH = IS_WINDOWS and os.getenv("TEMP") .. "\\MeterBridge" or "/tmp/MeterBridge"
local STATE_FILE = TEMP_PATH .. (IS_WINDOWS and "\\live_state.json" or "/live_state.json")
local CMD_FILE = TEMP_PATH .. (IS_WINDOWS and "\\commands.json" or "/commands.json")
local MAX_ARMED_TRACKS = 8

-- Create temp directory
if IS_WINDOWS then
    os.execute('mkdir "' .. TEMP_PATH .. '" 2>nul')
else
    os.execute('mkdir -p "' .. TEMP_PATH .. '"')
end

reaper.ShowConsoleMsg("MeterBridge: Writing to " .. STATE_FILE .. "\n")

-- ═══════════════════════════════════════════════════════════════
-- JSON Encoding (from StudioBeacon, proven working)
-- ═══════════════════════════════════════════════════════════════

-- Forward declare toJSON for mutual recursion
local toJSON

-- JSON encoder for arrays
local function arrayToJSON(arr)
    local parts = {}
    for _, v in ipairs(arr) do
        if type(v) == "table" then
            table.insert(parts, toJSON(v))
        elseif type(v) == "number" then
            table.insert(parts, string.format("%.2f", v))
        elseif type(v) == "string" then
            table.insert(parts, '"' .. v .. '"')
        elseif type(v) == "boolean" then
            table.insert(parts, v and "true" or "false")
        end
    end
    return "[" .. table.concat(parts, ",") .. "]"
end

-- Simple JSON encoder
toJSON = function(tbl)
    local parts = {}
    for k, v in pairs(tbl) do
        local key = '"' .. tostring(k) .. '"'
        local val
        if type(v) == "string" then
            val = '"' .. v:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n') .. '"'
        elseif type(v) == "number" then
            val = string.format("%.2f", v)
        elseif type(v) == "boolean" then
            val = v and "true" or "false"
        elseif type(v) == "table" then
            -- Check if it's an array (has numeric keys starting at 1)
            if #v > 0 or next(v) == nil then
                val = arrayToJSON(v)
            else
                val = toJSON(v)
            end
        else
            val = '"' .. tostring(v) .. '"'
        end
        table.insert(parts, key .. ":" .. val)
    end
    return "{" .. table.concat(parts, ",") .. "}"
end

-- ═══════════════════════════════════════════════════════════════
-- REAPER API Wrappers
-- ═══════════════════════════════════════════════════════════════

-- Get transport state as string + bitfield
local function getTransport()
    local state = reaper.GetPlayState()
    local flags = 0
    local name = "stopped"
    
    if state & 4 ~= 0 then 
        name = "recording"
        flags = 0x04 + 0x01  -- RECORDING + PLAYING
    elseif state & 2 ~= 0 then
        name = "paused"
        flags = 0x02  -- PAUSED
    elseif state & 1 ~= 0 then
        name = "playing"
        flags = 0x01  -- PLAYING
    else
        flags = 0x20  -- STOPPED
    end
    
    -- Check repeat
    local repeatOn = reaper.GetToggleCommandState(1068) == 1
    if repeatOn then flags = flags + 0x08 end
    
    -- Check metronome
    local metronomeOn = reaper.GetToggleCommandState(40364) == 1
    if metronomeOn then flags = flags + 0x10 end
    
    return name, flags, repeatOn, metronomeOn
end

-- Get project name from path
local function getProjectName()
    local _, path = reaper.EnumProjects(-1, "")
    if path == "" then return "Untitled" end
    return path:match("([^/\\]+)%.RPP$") or path:match("([^/\\]+)$") or "Untitled"
end

-- Get timecode string
local function getTimecode(pos)
    return reaper.format_timestr_pos(pos, "", 5)
end

-- Get bars/beats string
local function getBarsBeats(pos)
    local _, measures, cml, fullbeats = reaper.TimeMap2_timeToBeats(0, pos)
    local beats = fullbeats - math.floor(fullbeats / cml) * cml
    local ticks = math.floor((fullbeats % 1) * 960)
    return string.format("%d.%d.%03d", measures + 1, math.floor(beats) + 1, ticks)
end

-- Get current region name
local function getRegion(pos)
    local _, regionIdx = reaper.GetLastMarkerAndCurRegion(0, pos)
    if regionIdx >= 0 then
        local _, isrgn, _, _, name = reaper.EnumProjectMarkers(regionIdx)
        if isrgn and name then return name end
    end
    return ""
end

-- Get current marker name
local function getCurrentMarker(pos)
    local markerIdx, _ = reaper.GetLastMarkerAndCurRegion(0, pos)
    if markerIdx >= 0 then
        local _, isrgn, _, _, name = reaper.EnumProjectMarkers(markerIdx)
        if not isrgn and name and name ~= "" then return name end
    end
    return ""
end

-- Get take count for selected item
local function getTakeCount()
    local item = reaper.GetSelectedMediaItem(0, 0)
    if item then return reaper.CountTakes(item) end
    return 0
end

-- Get click/metronome enabled
local function getClickEnabled()
    return reaper.GetToggleCommandState(40364) == 1
end

-- Recording duration tracker
local recordingStartTime = nil
local function getRecordingDuration()
    local state = reaper.GetPlayState()
    local isRecording = (state & 4) ~= 0
    
    if isRecording then
        if not recordingStartTime then
            recordingStartTime = reaper.time_precise()
        end
        return reaper.time_precise() - recordingStartTime
    else
        recordingStartTime = nil
        return 0
    end
end

-- Get tempo and time signature at current play position
-- (queries at cursor position so mid-song time sig changes display correctly)
local function getTempoInfo()
    local tempo = reaper.Master_GetTempo()
    local pos = reaper.GetPlayPosition()
    local _, num, denom = reaper.TimeMap_GetTimeSigAtTime(0, pos)
    return math.floor(tempo), num, denom
end

-- Convert amplitude to dB (REAPER Lua has no math.log10)
local function toDB(peak)
    if peak <= 0 then return -60 end
    local db = 20 * (math.log(peak) / math.log(10))
    return math.max(-60, math.min(6, db))
end

-- ═══════════════════════════════════════════════════════════════
-- LUFS Approximation (windowed RMS of peak meters)
-- AES-EBU ITU-R BS.1770-4 simplified (no K-weighting, ~2-3dB approx)
-- ═══════════════════════════════════════════════════════════════

local LUFS_BUF_SIZE   = 180   -- 3 seconds @ 60fps for LUFS-S window
local LUFS_M_FRAMES   = 24    -- ~400ms @ 60fps for LUFS-M window
local LUFS_OFFSET     = -0.691  -- true LUFS vs. linear RMS correction
local lufsRingBuf     = {}    -- circular buffer of mean-square values
local lufsRingHead    = 0
local lufsIMeanSq     = 0.0
local lufsICount      = 0
local lufsRangeMin    = -999.0  -- ATK-11 fix: sentinel -999 not 0 to avoid reset at 0 dBFS
local lufsRangeMax    = -999.0
local lufsRangeFrames = 0

-- Phase correlation: circular buffer of L*R cross-products (100 frame window ~1.6s)
local PHASE_BUF_SIZE  = 100
local phaseRingBuf    = {}
local phaseRingHead   = 0
local phaseRingConst  = 1.0
for i = 1, PHASE_BUF_SIZE do phaseRingBuf[i] = 1.0 end

-- Clip counters (cumulative — reset when user clears)
local clipCountL = 0
local clipCountR = 0
local CLIP_THRESHOLD = 0.9999  -- ~0.0 dBFS in linear

-- Initialize LUFS ring buffer
for i = 1, LUFS_BUF_SIZE do lufsRingBuf[i] = 0.0 end

local function msToDB(ms)
    if ms <= 0 then return -70.0 end
    local db = LUFS_OFFSET + 10 * math.log(ms) / math.log(10)
    return math.max(-70.0, math.min(0.0, db))
end

local function updateLUFS(linL, linR)
    -- Mean-square for this frame
    local sq = (linL * linL + linR * linR) * 0.5

    -- Write into ring buffer
    lufsRingHead = (lufsRingHead % LUFS_BUF_SIZE) + 1
    lufsRingBuf[lufsRingHead] = sq

    -- LUFS-S: mean over full 3s ring
    local sumS = 0.0
    for i = 1, LUFS_BUF_SIZE do sumS = sumS + lufsRingBuf[i] end
    local lufsS = msToDB(sumS / LUFS_BUF_SIZE)

    -- LUFS-M: mean over last 24 frames (~400ms)
    local sumM = 0.0
    for i = 0, LUFS_M_FRAMES - 1 do
        local idx = ((lufsRingHead - 1 - i) % LUFS_BUF_SIZE) + 1
        sumM = sumM + lufsRingBuf[idx]
    end
    local lufsM = msToDB(sumM / LUFS_M_FRAMES)

    -- LUFS-I: running gated integration (gate: above -70 dBFS)
    local GATE_SQ = 1e-7  -- approx -70 dBFS
    if sq > GATE_SQ then
        lufsICount = lufsICount + 1
        lufsIMeanSq = lufsIMeanSq + (sq - lufsIMeanSq) / lufsICount
    end
    local lufsI = msToDB(lufsIMeanSq)

    -- LRA: short-term range — give minimum 0.1 so display shows a value, not dashes
    lufsRangeFrames = lufsRangeFrames + 1
    if lufsS > -65 then  -- only track above noise floor
        if lufsRangeMin == -999.0 and lufsRangeMax == -999.0 then
            lufsRangeMin = lufsS ; lufsRangeMax = lufsS
        else
            if lufsS < lufsRangeMin then lufsRangeMin = lufsS end
            if lufsS > lufsRangeMax then lufsRangeMax = lufsS end
        end
        -- Reset range window every 3600 frames (~60s)
        if lufsRangeFrames > 3600 then
            lufsRangeMin = lufsS ; lufsRangeMax = lufsS
            lufsRangeFrames = 0
        end
    end
    local lufsR = math.max(0.0, lufsRangeMax - lufsRangeMin)

    return lufsM, lufsS, lufsI, lufsR
end

local function updatePhase(linL, linR)
    -- Phase correlation = cross(L,R) / max(rms(L), rms(R))
    -- Normalized to [-1, 1]: +1=mono, 0=uncorrelated, -1=anti-phase
    local cross = linL * linR
    local autoL = linL * linL
    local autoR = linR * linR
    local denom = math.sqrt(autoL * autoR)
    local inst = (denom > 1e-10) and (cross / denom) or 1.0
    -- Smooth via ring buffer mean
    phaseRingHead = (phaseRingHead % PHASE_BUF_SIZE) + 1
    phaseRingBuf[phaseRingHead] = inst
    local sum = 0.0
    for i = 1, PHASE_BUF_SIZE do sum = sum + phaseRingBuf[i] end
    return sum / PHASE_BUF_SIZE
end

-- Get master track meters (returns dBFS + raw linear for LUFS + phase + clips)
local function getMasterMeter()
    local master = reaper.GetMasterTrack(0)
    local linL = reaper.Track_GetPeakInfo(master, 0)
    local linR = reaper.Track_GetPeakInfo(master, 1)
    local lufsM, lufsS, lufsI, lufsR = updateLUFS(linL, linR)
    local phase = updatePhase(linL, linR)
    -- Accumulate clips
    if linL >= CLIP_THRESHOLD then clipCountL = clipCountL + 1 end
    if linR >= CLIP_THRESHOLD then clipCountR = clipCountR + 1 end
    return toDB(linL), toDB(linR), lufsM, lufsS, lufsI, lufsR, phase, clipCountL, clipCountR
end

-- Get armed track meters
local function getArmedTrackMeters()
    local armedTracks = {}
    local trackCount = reaper.CountTracks(0)
    
    for i = 0, trackCount - 1 do
        if #armedTracks >= MAX_ARMED_TRACKS then break end
        
        local track = reaper.GetTrack(0, i)
        local armed = reaper.GetMediaTrackInfo_Value(track, "I_RECARM")
        
        if armed == 1 then
            local _, name = reaper.GetTrackName(track)
            local peakL = reaper.Track_GetPeakInfo(track, 0)
            local peakR = reaper.Track_GetPeakInfo(track, 1)
            
            table.insert(armedTracks, {
                index = i + 1,
                name = name or ("Track " .. (i + 1)),
                left = toDB(peakL),
                right = toDB(peakR)
            })
        end
    end
    
    return armedTracks
end

-- Get track counts
local function getTrackCounts()
    local trackCount = reaper.CountTracks(0)
    local armedCount = 0
    local muteCount = 0
    local soloCount = 0
    
    for i = 0, trackCount - 1 do
        local track = reaper.GetTrack(0, i)
        if reaper.GetMediaTrackInfo_Value(track, "I_RECARM") == 1 then
            armedCount = armedCount + 1
        end
        if reaper.GetMediaTrackInfo_Value(track, "B_MUTE") == 1 then
            muteCount = muteCount + 1
        end
        if reaper.GetMediaTrackInfo_Value(track, "I_SOLO") > 0 then
            soloCount = soloCount + 1
        end
    end
    
    return trackCount, armedCount, muteCount, soloCount
end

-- Get master volume in dB
local function getMasterVolume()
    local master = reaper.GetMasterTrack(0)
    local vol = reaper.GetMediaTrackInfo_Value(master, "D_VOL")
    if vol <= 0 then return -100 end
    local db = 20 * (math.log(vol) / math.log(10))
    return math.floor(db * 10) / 10
end

-- Get sample rate
local function getSampleRate()
    return math.floor(reaper.GetSetProjectInfo(0, "PROJECT_SRATE", 0, false))
end

-- Get selected track info for MeterBridge display
local function getSelectedTrackInfo()
    local track = reaper.GetSelectedTrack(0, 0)
    if not track then
        return 0, "MASTER", 180, 180, 200, false, false, false
    end
    
    local _, name = reaper.GetTrackName(track)
    local idx = math.floor(reaper.GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER"))
    
    -- Get track color
    local color = math.floor(reaper.GetMediaTrackInfo_Value(track, "I_CUSTOMCOLOR"))
    local r, g, b = 180, 180, 200
    if color ~= 0 then
        -- REAPER color format: 0x01BBGGRR
        r = color & 0xFF
        g = (color >> 8) & 0xFF
        b = (color >> 16) & 0xFF
    end
    
    local muted = reaper.GetMediaTrackInfo_Value(track, "B_MUTE") ~= 0
    local soloed = reaper.GetMediaTrackInfo_Value(track, "I_SOLO") > 0
    local armed = reaper.GetMediaTrackInfo_Value(track, "I_RECARM") == 1
    
    return idx, name or ("Track " .. idx), r, g, b, muted, soloed, armed
end


-- ═══════════════════════════════════════════════════════════════
-- Main Update Loop
-- ═══════════════════════════════════════════════════════════════

local frameCount = 0
local lastUpdateTime = 0   -- timer gate: when we last wrote state

local function update()
    -- Rate gate — only write when interval has elapsed
    local now = reaper.time_precise()
    if now - lastUpdateTime < UPDATE_INTERVAL then
        reaper.defer(update)
        return
    end
    lastUpdateTime = now

    -- Allow relay to push a new rate via REAPER ExtState
    local rateStr = reaper.GetExtState("MeterBridge", "update_ms")
    if rateStr ~= "" then
        local ms = tonumber(rateStr) or 250
        ms = math.max(16, math.min(2500, ms))
        UPDATE_INTERVAL = ms / 1000.0
    end

    local pos = reaper.GetPlayPosition()
    local transportName, transportFlags, repeatOn, metronomeOn = getTransport()
    local tempo, tsNum, tsDen = getTempoInfo()
    local masterL, masterR, lufsM, lufsS, lufsI, lufsR, phase, clipL, clipR = getMasterMeter()
    local trackCount, armedCount, muteCount, soloCount = getTrackCounts()
    local trkIdx, trkName, trkR, trkG, trkB, trkMuted, trkSoloed, trkArmed = getSelectedTrackInfo()
    
    -- Build the full state object
    local state = {
        -- Transport
        transport = transportName,
        transportFlags = transportFlags,
        
        -- Position
        timecode = getTimecode(pos),
        barsBeats = getBarsBeats(pos),
        positionBeats = pos * (tempo / 60.0),
        positionSecs = pos,
        
        -- Tempo / Time Sig
        tempo = tempo,
        timeSigNum = tsNum,
        timeSigDen = tsDen,
        
        -- Project
        projectName = getProjectName(),
        regionName = getRegion(pos),
        currentMarker = getCurrentMarker(pos),
        sampleRate = getSampleRate(),
        masterVolume = getMasterVolume(),
        
        -- Track counts
        trackCount = trackCount,
        armedCount = armedCount,
        mutedCount = muteCount,
        soloedCount = soloCount,
        
        -- Metering (dBFS)
        masterMeterL = masterL,
        masterMeterR = masterR,

        -- LUFS (windowed RMS approximation, ~2-3dB of BS.1770-4)
        lufsM = lufsM,
        lufsS = lufsS,
        lufsI = lufsI,
        lufsR = lufsR,

        -- Phase correlation [-1=anti-phase, 0=uncorrelated, +1=mono]
        phase = phase,

        -- Clip counts (cumulative since script start)
        clipL = clipL,
        clipR = clipR,

        -- Selected track
        selectedTrack = {
            index = trkIdx,
            name = trkName,
            colorR = trkR,
            colorG = trkG,
            colorB = trkB,
            muted = trkMuted,
            soloed = trkSoloed,
            armed = trkArmed
        },
        
        -- Armed track meters
        armedTrackMeters = getArmedTrackMeters(),
        
        -- Session info
        takeCount = getTakeCount(),
        clickEnabled = getClickEnabled(),
        recordingDuration = getRecordingDuration(),
        repeatEnabled = repeatOn,
        
        -- Timing
        timestamp = os.time(),
        frame = frameCount
    }

    -- Read spectrum data from JSFX gmem[] shared memory
    -- JSFX layout: gmem[0]=magic 0x4D425350, gmem[1]=band_count, gmem[2..17]=bands
    reaper.gmem_attach("meterbridge_spectrum")
    local magic = reaper.gmem_read(0)
    if magic == 0x4D425350 then  -- "MBSP" sentinel
        local band_count = math.floor(reaper.gmem_read(1))
        if band_count > 0 and band_count <= 16 then
            local bands = {}
            for bi = 1, band_count do
                local db = reaper.gmem_read(1 + bi)  -- gmem[2..17]
                -- Clamp to valid range
                if db < -60 then db = -60 end
                if db > 0 then db = 0 end
                bands[#bands + 1] = db
            end
            state.spectrum = bands
        end
    end

    -- Read markers near current position
    local markerList = {}
    local numMarkers = reaper.CountProjectMarkers(0)
    local cursorPos = reaper.GetPlayPosition()
    local i = 0
    while i < numMarkers and #markerList < 5 do
        local retval, isrgn, markerPos, rgnEnd, mName, markrgnIdx = reaper.EnumProjectMarkers(i)
        if retval > 0 then
            -- Include markers within +/- 30 seconds of current position
            if math.abs(markerPos - cursorPos) < 30 then
                markerList[#markerList + 1] = {
                    name = mName or "",
                    position = markerPos,
                    isRegion = isrgn and 1 or 0
                }
            end
        end
        i = i + 1
    end
    if #markerList > 0 then
        state.markers = markerList
    end
    
    -- Write state to file
    local f = io.open(STATE_FILE, "w")
    if f then
        f:write(toJSON(state))
        f:close()
    end
    
    -- Check for commands every ~10 frames
    if frameCount % 10 == 0 then
        processCommands()
    end
    
    reaper.defer(update)
end

-- ═══════════════════════════════════════════════════════════════
-- Command Processing — reads commands.json written by the relay
-- ═══════════════════════════════════════════════════════════════

local COMMAND_ACTIONS = {
    play             = 40073,  -- Transport: Play
    stop             = 40074,  -- Transport: Stop
    record           = 40013,  -- Transport: Record
    rewind           = 40084,  -- Transport: Go to previous measure
    forward          = 40085,  -- Transport: Go to next measure
    toggle_repeat    = 1068,   -- Transport: Toggle repeat
    toggle_metronome = 40364,  -- Toggle metronome
    prev_track       = 40286,  -- Track: Select previous track
    next_track       = 40285,  -- Track: Select next track
    toggle_mute      = 40281,  -- Track: Toggle mute for selected tracks
    toggle_solo      = 40282,  -- Track: Toggle solo for selected tracks
    next_marker      = 40173,  -- Markers: Go to next marker/project end
    prev_marker      = 40172,  -- Markers: Go to previous marker/project start
    reset_clips      = nil,    -- Custom: reset clip counters (handled in code)
}

local function processCommands()
    local cf = io.open(CMD_FILE, "r")
    if not cf then return end
    local content = cf:read("*all")
    cf:close()
    
    -- Delete the file immediately so we don't re-process
    os.remove(CMD_FILE)
    
    if content == "" then return end
    
    -- Simple JSON parsing for {"command": "xxx"}
    local cmd = content:match('"command"%s*:%s*"([^"]+)"')
    if not cmd then return end
    
    if cmd == "reset_clips" then
        -- Reset clip counters (lua-side global)
        clipCountL = 0
        clipCountR = 0
        reaper.ShowConsoleMsg("MeterBridge: Clip counters reset\n")
    elseif cmd:match("^set_volume:") then
        local db = tonumber(cmd:match("set_volume:(.+)"))
        if db then
            local master = reaper.GetMasterTrack(0)
            -- Convert dB to volume factor
            local vol = 10 ^ (db / 20)
            reaper.SetMediaTrackInfo_Value(master, "D_VOL", vol)
        end
    else
        local actionId = COMMAND_ACTIONS[cmd]
        if actionId then
            reaper.Main_OnCommand(actionId, 0)
        end
    end
end

-- ═══════════════════════════════════════════════════════════════
-- Lifecycle
-- ═══════════════════════════════════════════════════════════════

local function cleanup()
    reaper.SetExtState("MeterBridge", "running", "0", false)
    os.remove(STATE_FILE)
    os.remove(CMD_FILE)
    reaper.ShowConsoleMsg("MeterBridge: Bridge script stopped.\n")
end

reaper.atexit(cleanup)
reaper.ShowConsoleMsg("MeterBridge: Bridge active. Close this script to stop.\n")
update()
