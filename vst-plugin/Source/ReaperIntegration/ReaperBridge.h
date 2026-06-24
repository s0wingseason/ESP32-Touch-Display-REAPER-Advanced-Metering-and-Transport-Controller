/*
 * MeterBridge — REAPER API Bridge
 * 
 * Dynamically loads REAPER extension API functions at runtime when
 * the plugin detects it's running inside REAPER. Provides transport
 * control, track info queries, and playback state monitoring.
 * 
 * This works from within the VST3 plugin — no separate DLL needed
 * for basic transport/track functionality. The companion extension
 * DLL (Phase 2) will provide deeper integration.
 */

#pragma once
#include <JuceHeader.h>

#ifdef _WIN32
#include <windows.h>
#endif

class ReaperBridge {
public:
    ReaperBridge() = default;

    /** Attempt to detect REAPER and load API functions. */
    bool initialize() {
#ifdef _WIN32
        /* Try to get REAPER's module handle */
        HMODULE reaper = GetModuleHandleA("reaper.exe");
        if (!reaper) reaper = GetModuleHandleA("REAPER.exe");
        if (!reaper) {
            DBG("ReaperBridge: Not running in REAPER");
            return false;
        }

        /* Load the plugin_getapi function */
        auto getAPI = (decltype(plugin_getapi)*)GetProcAddress(reaper, "plugin_getapi");
        if (!getAPI) {
            DBG("ReaperBridge: plugin_getapi not found");
            return false;
        }

        /* Load individual API functions */
        fn_GetPlayState = (int(*)())getAPI("GetPlayState");
        fn_GetPlayPosition = (double(*)())getAPI("GetPlayPosition");
        fn_GetPlayPosition2 = (double(*)())getAPI("GetPlayPosition2");
        fn_GetCursorPosition = (double(*)())getAPI("GetCursorPosition");
        fn_Main_OnCommand = (void(*)(int, int))getAPI("Main_OnCommand");
        fn_GetMasterTrack = (void*(*)(void*))getAPI("GetMasterTrack");
        fn_GetSelectedTrack = (void*(*)(void*, int))getAPI("GetSelectedTrack");
        fn_GetMediaTrackInfo_Value = (double(*)(void*, const char*))getAPI("GetMediaTrackInfo_Value");
        fn_GetSetMediaTrackInfo_String = (bool(*)(void*, const char*, char*, bool))getAPI("GetSetMediaTrackInfo_String");
        fn_GetMasterMuteSoloFlags = (int(*)())getAPI("GetMasterMuteSoloFlags");
        fn_Master_GetTempo = (double(*)())getAPI("Master_GetTempo");
        fn_TimeMap_GetTimeSigAtTime = (void(*)(void*, double, int*, int*, double*))getAPI("TimeMap_GetTimeSigAtTime");
        fn_CountTracks = (int(*)(void*))getAPI("CountTracks");
        fn_GetToggleCommandState = (int(*)(int))getAPI("GetToggleCommandState");

        isLoaded = (fn_GetPlayState != nullptr && fn_Main_OnCommand != nullptr);
        if (isLoaded) DBG("ReaperBridge: REAPER API loaded successfully");
        return isLoaded;
#else
        return false;
#endif
    }

    bool isAvailable() const { return isLoaded; }

    /* ── Transport Queries ── */

    int getPlayState() const {
        /* Returns: 0=stopped, 1=playing, 2=paused, 5=recording, 6=record-paused */
        return fn_GetPlayState ? fn_GetPlayState() : 0;
    }

    bool isPlaying() const { return (getPlayState() & 1) != 0; }
    bool isPaused() const { return (getPlayState() & 2) != 0; }
    bool isRecording() const { return (getPlayState() & 4) != 0; }

    double getPlayPosition() const {
        return fn_GetPlayPosition ? fn_GetPlayPosition() : 0.0;
    }

    double getTempo() const {
        return fn_Master_GetTempo ? fn_Master_GetTempo() : 120.0;
    }

    void getTimeSig(int& num, int& den) const {
        if (fn_TimeMap_GetTimeSigAtTime) {
            double tempo;
            fn_TimeMap_GetTimeSigAtTime(nullptr, getPlayPosition(), &num, &den, &tempo);
        } else {
            num = 4; den = 4;
        }
    }

    bool isRepeatOn() const {
        /* Command 1068 = Toggle repeat */
        return fn_GetToggleCommandState ? (fn_GetToggleCommandState(1068) == 1) : false;
    }

    bool isMetronomeOn() const {
        /* Command 40364 = Toggle metronome */
        return fn_GetToggleCommandState ? (fn_GetToggleCommandState(40364) == 1) : false;
    }

    /* ── Transport Commands ── */

    void play()     { sendCommand(1007); }  /* Transport: Play */
    void stop()     { sendCommand(1016); }  /* Transport: Stop */
    void record()   { sendCommand(1013); }  /* Transport: Record */
    void rewind()   { sendCommand(40042); } /* Transport: Go to start */
    void forward()  { sendCommand(40043); } /* Transport: Go to end */
    void toggleRepeat()    { sendCommand(1068); }
    void toggleMetronome() { sendCommand(40364); }

    /* ── Track Info ── */

    juce::String getTrackName(void* track) const {
        if (!fn_GetSetMediaTrackInfo_String || !track) return "MASTER";
        char buf[256] = {0};
        fn_GetSetMediaTrackInfo_String(track, "P_NAME", buf, false);
        return juce::String(buf);
    }

    void getTrackColor(void* track, uint8_t& r, uint8_t& g, uint8_t& b) const {
        if (!fn_GetMediaTrackInfo_Value || !track) { r=255; g=0; b=255; return; }
        int color = (int)fn_GetMediaTrackInfo_Value(track, "I_CUSTOMCOLOR");
        if (color & 0x01000000) { /* Custom color flag */
            r = (color >> 0) & 0xFF;
            g = (color >> 8) & 0xFF;
            b = (color >> 16) & 0xFF;
        } else {
            r = 180; g = 180; b = 200; /* Default gray */
        }
    }

    void* getMasterTrack() const {
        return fn_GetMasterTrack ? fn_GetMasterTrack(nullptr) : nullptr;
    }

    void* getSelectedTrack() const {
        return fn_GetSelectedTrack ? fn_GetSelectedTrack(nullptr, 0) : nullptr;
    }

    bool isTrackMuted(void* track) const {
        if (!fn_GetMediaTrackInfo_Value || !track) return false;
        return fn_GetMediaTrackInfo_Value(track, "B_MUTE") != 0.0;
    }

    bool isTrackSoloed(void* track) const {
        if (!fn_GetMediaTrackInfo_Value || !track) return false;
        return fn_GetMediaTrackInfo_Value(track, "I_SOLO") != 0.0;
    }

    bool isTrackArmed(void* track) const {
        if (!fn_GetMediaTrackInfo_Value || !track) return false;
        return fn_GetMediaTrackInfo_Value(track, "I_RECARM") != 0.0;
    }

    int getTrackIndex(void* track) const {
        if (!fn_GetMediaTrackInfo_Value || !track) return 0;
        return (int)fn_GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
    }

private:
    void sendCommand(int cmdId) {
        if (fn_Main_OnCommand) fn_Main_OnCommand(cmdId, 0);
    }

    bool isLoaded = false;

    /* REAPER API function pointers */
    typedef void* (*plugin_getapi_t)(const char*);
    void* (*plugin_getapi)(const char*) = nullptr;

    int (*fn_GetPlayState)() = nullptr;
    double (*fn_GetPlayPosition)() = nullptr;
    double (*fn_GetPlayPosition2)() = nullptr;
    double (*fn_GetCursorPosition)() = nullptr;
    void (*fn_Main_OnCommand)(int, int) = nullptr;
    void* (*fn_GetMasterTrack)(void*) = nullptr;
    void* (*fn_GetSelectedTrack)(void*, int) = nullptr;
    double (*fn_GetMediaTrackInfo_Value)(void*, const char*) = nullptr;
    bool (*fn_GetSetMediaTrackInfo_String)(void*, const char*, char*, bool) = nullptr;
    int (*fn_GetMasterMuteSoloFlags)() = nullptr;
    double (*fn_Master_GetTempo)() = nullptr;
    void (*fn_TimeMap_GetTimeSigAtTime)(void*, double, int*, int*, double*) = nullptr;
    int (*fn_CountTracks)(void*) = nullptr;
    int (*fn_GetToggleCommandState)(int) = nullptr;
};
