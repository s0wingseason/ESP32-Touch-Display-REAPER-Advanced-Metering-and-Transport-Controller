--[[
    StudioBeacon REAPER Bridge
    
    This Lua script connects REAPER to the StudioBeacon Display application.
    It monitors transport state, project metadata, and sends updates via:
    1. WebSocket (primary, if available)
    2. File-Based JSON (fallback, for zero-dependency)
    
    INSTALLATION:
    1. Copy this file to: %APPDATA%\REAPER\Scripts\
    2. In REAPER: Actions -> Show Action List -> Load ReaScript
    3. Select this script
    4. Run "StudioBeacon" action
    
    Version: 1.2.0 (Zero-Dependency)
]]--

-- Configuration
local WS_HOST = "127.0.0.1"
local WS_PORT = 9999
-- File fallback
local IS_WINDOWS = reaper.GetOS():find("Win") ~= nil
local TEMP_PATH = IS_WINDOWS and os.getenv("TEMP") .. "\\StudioBeacon" or "/tmp/StudioBeacon"

-- State tracking
local socket = nil
local connected = false
local use_file_fallback = false

-- Create temp directory if needed for file fallback
local function ensure_temp_dir() 
    -- Lua doesn't have mkdir, so we use execute.
    -- This is safe to run repeatedly.
    if IS_WINDOWS then
        os.execute('mkdir "' .. TEMP_PATH .. '" 2>nul')
    else
        os.execute('mkdir -p "' .. TEMP_PATH .. '"')
    end
end

-- Dependency Loader Helper (Optional Sockets)
local function load_dependency()
    local status, lib = pcall(require, "socket")
    if status then return lib end
    
    -- Try adding paths just in case
    local sep = package.config:sub(1,1)
    local resource_path = reaper.GetResourcePath()
    local paths = { resource_path .. "/Scripts/?.lua", resource_path .. "/UserPlugins/?.lua" }
    local cpaths = { resource_path .. "/UserPlugins/?.dll", resource_path .. "/UserPlugins/?.so" }
    
    for _, p in ipairs(paths) do package.path = package.path .. ";" .. p end
    for _, p in ipairs(cpaths) do package.cpath = package.cpath .. ";" .. p end
    
    status, lib = pcall(require, "socket")
    if status then return lib end
    
    return nil
end

local socket_lib = load_dependency()

if not socket_lib then
    -- Fallback mode
    use_file_fallback = true
    ensure_temp_dir()
    -- reaper.ShowConsoleMsg("StudioBeacon: LuaSocket not found. Using File Fallback Mode.\n")
end

-- JSON encoder (simple implementation)
local function json_encode(tbl)
    local parts = {}
    for k, v in pairs(tbl) do
        local key = '"' .. tostring(k) .. '"'
        local val
        if type(v) == "string" then
            val = '"' .. v:gsub('"', '\\"') .. '"'
        elseif type(v) == "number" then
            val = tostring(v)
        elseif type(v) == "boolean" then
            val = v and "true" or "false"
        elseif type(v) == "table" then
            val = json_encode(v)
        else
            val = '"' .. tostring(v) .. '"'
        end
        table.insert(parts, key .. ":" .. val)
    end
    return "{" .. table.concat(parts, ",") .. "}"
end

-- Connect to StudioBeacon Display
local function connect()
    if use_file_fallback then return true end
    if connected then return true end
    if not socket_lib or not socket_lib.tcp then return false end
    
    local tcp = socket_lib.tcp()
    tcp:settimeout(0.5)
    
    local success, err = tcp:connect(WS_HOST, WS_PORT)
    if success then
        socket = tcp
        socket:settimeout(0)  -- Non-blocking
        connected = true
        return true
    else
        socket = nil
        connected = false
        return false
    end
end

-- Send data to display
local function send(data)
    local json = json_encode(data)
    
    if use_file_fallback then
        -- WRITE TO FILE
        local file_path = TEMP_PATH .. (IS_WINDOWS and "\\live_state.json" or "/live_state.json")
        local f = io.open(file_path, "w")
        if f then
            f:write(json)
            f:close()
            return true
        end
        return false
    else
        -- SEND TO SOCKET
        if not connected or not socket then return false end
        local success, err = socket:send(json .. "\n")
        if not success then
            connected = false
            socket = nil
            return false
        end
        return true
    end
end

-- Gathers Transport State
local function getTransportState()
    local state = reaper.GetPlayState()
    if state & 4 ~= 0 then return "recording"
    elseif state & 2 ~= 0 then return "paused"
    elseif state & 1 ~= 0 then return "playing"
    else return "stopped" end
end

local function getTimecode(pos)
    return reaper.format_timestr_pos(pos, "", 5) -- 5 = Timecode format
end

local function getBarsBeats(pos)
    local _, measures, cml, fullbeats, _ = reaper.TimeMap2_timeToBeats(0, pos)
    local beats = fullbeats - math.floor(fullbeats / cml) * cml
    local ticks = math.floor((fullbeats % 1) * 960)
    return string.format("%d.%d.%03d", measures + 1, math.floor(beats) + 1, ticks)
end

local function getCurrentRegion(pos)
    local markerIdx, regionIdx = reaper.GetLastMarkerAndCurRegion(0, pos)
    if regionIdx >= 0 then
        local _, isrgn, _, _, name, _ = reaper.EnumProjectMarkers(regionIdx)
        if isrgn and name then return name end
    end
    return ""
end

local function getProjectInfo()
    local _, projectPath = reaper.EnumProjects(-1, "")
    local projectName = "Untitled"
    if projectPath ~= "" then
        projectName = projectPath:match("([^/\\]+)%.RPP$") or projectPath:match("([^/\\]+)$") or projectName
    end
    local tempo = reaper.Master_GetTempo()
    local _, num, denom = reaper.TimeMap_GetTimeSigAtTime(0, 0)
    return { projectName = projectName, tempo = math.floor(tempo), timeSignature = string.format("%d/%d", num, denom) }
end

-- Extended Metadata Functions

local function getArmedTracks()
    local tracks = {}
    local count = reaper.CountTracks(0)
    for i = 0, count - 1 do
        local track = reaper.GetTrack(0, i)
        if reaper.GetMediaTrackInfo_Value(track, "I_RECARM") == 1 then
            local _, name = reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "", false)
            if name == "" then name = "Track " .. tostring(i + 1) end
            table.insert(tracks, name)
        end
    end
    return table.concat(tracks, ", ")
end

local function getRecordingTakes()
    -- Heuristic: If recording, check armed tracks for existing items or just return generic
    -- Accurate take counts are complex during record. We'll return the number of items on armed tracks.
    -- Better: Just return "Take " .. counter if we tracked it, but for now we'll stick to basic connectivity.
    -- Alternative: Count items on selected track?
    local item = reaper.GetSelectedMediaItem(0, 0)
    if item then
        local take = reaper.GetActiveTake(item)
        if take then
            local _, name = reaper.GetSetMediaItemTakeInfo_String(take, "P_NAME", "", false)
            return name
        end
    end
    return ""
end

local function getMixStatus()
    local solo = reaper.AnyTrackSolo(0)
    -- Check for muted master?
    local master = reaper.GetMasterTrack(0)
    local masterMute = reaper.GetMediaTrackInfo_Value(master, "B_MUTE") == 1
    
    local status = {}
    if solo then table.insert(status, "SOLO ACTIVE") end
    if masterMute then table.insert(status, "MASTER MUTED") end
    
    return table.concat(status, " • ")
end

-- Main update loop
local function update()
    if not use_file_fallback and not connected then connect() end
    
    local pos = reaper.GetPlayPosition()
    local projectInfo = getProjectInfo()
    local armed = getArmedTracks()
    
    local state = {
        type = "full-state",
        state = {
            transport = getTransportState(),
            projectName = projectInfo.projectName,
            timecode = getTimecode(pos),
            barsBeats = getBarsBeats(pos),
            regionName = getCurrentRegion(pos),
            tempo = projectInfo.tempo,
            timeSignature = projectInfo.timeSignature,
            -- New Fields
            armedTracks = armed,
            currentTake = getRecordingTakes(),
            mixStatus = getMixStatus()
        }
    }
    
    send(state)
    reaper.defer(update)
end

local function cleanup()
    if socket then socket:close() end
end

reaper.atexit(cleanup)
connect()
update()
