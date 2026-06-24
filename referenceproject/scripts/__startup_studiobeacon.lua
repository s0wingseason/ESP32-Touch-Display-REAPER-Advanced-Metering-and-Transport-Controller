--[[
    StudioBeacon REAPER Auto-Bridge
    
    This script runs AUTOMATICALLY when REAPER starts.
    It monitors transport state, meters, and metadata - writing to a file
    that StudioBeacon reads - requiring zero user configuration.
    
    Location: %APPDATA%\REAPER\Scripts\__startup_studiobeacon.lua
    
    Version: 3.4.0 (Fixed math.log10 - REAPER Lua compatibility)
]]--

-- Clear any stuck running state and set fresh
reaper.SetExtState("StudioBeacon", "running", "1", false)

-- Show startup notification in console
reaper.ShowConsoleMsg("StudioBeacon: Bridge script starting...\n")

-- Configuration
local UPDATE_INTERVAL = 0.016 -- ~60fps default (16ms)
local IS_WINDOWS = reaper.GetOS():find("Win") ~= nil
local TEMP_PATH = IS_WINDOWS and os.getenv("TEMP") .. "\\StudioBeacon" or "/tmp/StudioBeacon"
local STATE_FILE = TEMP_PATH .. (IS_WINDOWS and "\\live_state.json" or "/live_state.json")
local MAX_ARMED_TRACKS = 8

-- Create temp directory
if IS_WINDOWS then
    os.execute('mkdir "' .. TEMP_PATH .. '" 2>nul')
else
    os.execute('mkdir -p "' .. TEMP_PATH .. '"')
end

reaper.ShowConsoleMsg("StudioBeacon: Writing to " .. STATE_FILE .. "\n")

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

-- Get transport state
local function getTransport()
    local state = reaper.GetPlayState()
    if state & 4 ~= 0 then return "recording"
    elseif state & 2 ~= 0 then return "paused"
    elseif state & 1 ~= 0 then return "playing"
    else return "stopped" end
end

-- Get project name
local function getProjectName()
    local _, path = reaper.EnumProjects(-1, "")
    if path == "" then return "Untitled" end
    return path:match("([^/\\]+)%.RPP$") or path:match("([^/\\]+)$") or "Untitled"
end

-- Get timecode
local function getTimecode(pos)
    return reaper.format_timestr_pos(pos, "", 5)
end

-- Get bars/beats
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

-- Get current marker name at or before position
local function getCurrentMarker(pos)
    local markerIdx, _ = reaper.GetLastMarkerAndCurRegion(0, pos)
    if markerIdx >= 0 then
        local _, isrgn, _, _, name = reaper.EnumProjectMarkers(markerIdx)
        if not isrgn and name and name ~= "" then return name end
    end
    return ""
end

-- Get next marker after position
local function getNextMarker(pos)
    local numMarkers = reaper.CountProjectMarkers(0)
    for i = 0, numMarkers - 1 do
        local _, isrgn, markerPos, _, name = reaper.EnumProjectMarkers(i)
        if not isrgn and markerPos > pos then
            return name or "", markerPos - pos
        end
    end
    return "", 0
end

-- Get take count for selected/recording item
local function getTakeCount()
    local item = reaper.GetSelectedMediaItem(0, 0)
    if item then
        return reaper.CountTakes(item)
    end
    return 0
end

-- Get metronome/click enabled status
local function getClickEnabled()
    -- Check if metronome is enabled during playback/recording
    local clickEnabled = reaper.GetToggleCommandState(40364) -- Toggle metronome
    return clickEnabled == 1
end

-- Calculate recording duration (time since recording started)
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

-- Get tempo and time signature
local function getTempoInfo()
    local tempo = reaper.Master_GetTempo()
    local _, num, denom = reaper.TimeMap_GetTimeSigAtTime(0, 0)
    return math.floor(tempo), string.format("%d/%d", num, denom)
end

-- Convert amplitude to dB
local function toDB(peak)
    if peak <= 0 then return -60 end
    -- Note: REAPER Lua doesn't have math.log10, use log(x)/log(10)
    local db = 20 * (math.log(peak) / math.log(10))
    return math.max(-60, math.min(6, db))
end

-- Get master track meters (L/R peak in dB)
local function getMasterMeter()
    local master = reaper.GetMasterTrack(0)
    local peakL = reaper.Track_GetPeakInfo(master, 0) -- Channel 0 = Left
    local peakR = reaper.Track_GetPeakInfo(master, 1) -- Channel 1 = Right
    
    return {
        left = toDB(peakL),
        right = toDB(peakR)
    }
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
    
    return {
        total = trackCount,
        armed = armedCount,
        muted = muteCount,
        soloed = soloCount
    }
end

-- Get project info
local function getProjectInfo()
    local sampleRate = reaper.GetSetProjectInfo(0, "PROJECT_SRATE", 0, false)
    local length = reaper.GetProjectLength(0)
    
    return {
        sampleRate = math.floor(sampleRate),
        length = reaper.format_timestr_pos(length, "", 5),
        lengthSeconds = length
    }
end

-- Get master volume
local function getMasterVolume()
    local master = reaper.GetMasterTrack(0)
    local vol = reaper.GetMediaTrackInfo_Value(master, "D_VOL")
    if vol <= 0 then return -100 end
    -- Note: REAPER Lua doesn't have math.log10, use log(x)/log(10)
    local db = 20 * (math.log(vol) / math.log(10))
    return math.floor(db * 10) / 10
end

-- Get performance metrics (buffer size, timestamp)
local function getPerformanceMetrics()
    -- Get audio device buffer size
    local ret, desc = reaper.GetAudioDeviceInfo("BSIZE", "")
    local bufferSize = tonumber(desc) or 0
    
    -- Get sample rate from audio device
    local ret2, srDesc = reaper.GetAudioDeviceInfo("SRATE", "")
    local deviceSampleRate = tonumber(srDesc) or 48000
    
    return {
        bufferSize = bufferSize,
        deviceSampleRate = deviceSampleRate,
        timestamp = os.time()
    }
end

-- Main update loop
local function update()
    local pos = reaper.GetPlayPosition()
    local tempo, timeSig = getTempoInfo()
    local transport = getTransport()
    local trackCounts = getTrackCounts()
    local projectInfo = getProjectInfo()
    local perfMetrics = getPerformanceMetrics()
    
    local state = {
        type = "full-state",
        state = {
            transport = transport,
            projectName = getProjectName(),
            timecode = getTimecode(pos),
            barsBeats = getBarsBeats(pos),
            regionName = getRegion(pos),
            tempo = tempo,
            timeSignature = timeSig,
            
            -- Additional metadata
            trackCount = trackCounts.total,
            armedCount = trackCounts.armed,
            mutedCount = trackCounts.muted,
            soloedCount = trackCounts.soloed,
            sampleRate = projectInfo.sampleRate,
            projectLength = projectInfo.length,
            masterVolume = getMasterVolume(),
            
            -- Performance metrics
            bufferSize = perfMetrics.bufferSize,
            timestamp = perfMetrics.timestamp,
            
            -- Metering data
            masterMeter = getMasterMeter(),
            armedTrackMeters = getArmedTrackMeters(),
            
            -- Session info (new)
            currentMarker = getCurrentMarker(pos),
            nextMarker = getNextMarker(pos),
            takeCount = getTakeCount(),
            clickEnabled = getClickEnabled(),
            recordingDuration = getRecordingDuration()
        }
    }
    
    -- Write to file
    local f = io.open(STATE_FILE, "w")
    if f then
        f:write(toJSON(state))
        f:close()
    end
    
    reaper.defer(update)
end

-- Cleanup on exit
local function cleanup()
    reaper.SetExtState("StudioBeacon", "running", "0", false)
    os.remove(STATE_FILE)
end

reaper.atexit(cleanup)
update()
