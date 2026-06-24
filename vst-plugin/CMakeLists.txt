cmake_minimum_required(VERSION 3.22)

project(MeterBridge VERSION 1.0.0 LANGUAGES CXX)

# ─── JUCE ─────────────────────────────────────────────────────
# JUCE is fetched automatically if not found locally
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─── Plugin Target ────────────────────────────────────────────
juce_add_plugin(MeterBridge
    COMPANY_NAME                "FalconEYE Software"
    PLUGIN_MANUFACTURER_CODE    FEYE
    PLUGIN_CODE                 MtBr
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Analyzer Tools
    PRODUCT_NAME                "MeterBridge"
    BUNDLE_ID                   "com.falconeye.meterbridge"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_COPY_DIR               "$ENV{APPDATA}/REAPER/UserPlugins"
)

# ─── Source Files ─────────────────────────────────────────────
target_sources(MeterBridge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Metering/LufsEngine.cpp
    Source/Metering/LufsEngine.h
    Source/Metering/PeakMeter.cpp
    Source/Metering/PeakMeter.h
    Source/Metering/RmsMeter.cpp
    Source/Metering/RmsMeter.h
    Source/Metering/PhaseMeter.cpp
    Source/Metering/PhaseMeter.h
    Source/Metering/MeterValues.h
    Source/Network/UdpStreamer.cpp
    Source/Network/UdpStreamer.h
    Source/Network/UdpListener.cpp
    Source/Network/UdpListener.h
    Source/Network/Protocol.h
    Source/ReaperIntegration/ReaperBridge.cpp
    Source/ReaperIntegration/ReaperBridge.h
)

# ─── Include Paths ────────────────────────────────────────────
target_include_directories(MeterBridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# ─── JUCE Modules ─────────────────────────────────────────────
target_link_libraries(MeterBridge
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ─── Compile Definitions ─────────────────────────────────────
target_compile_definitions(MeterBridge
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ─── Windows Socket Library ──────────────────────────────────
if(WIN32)
    target_link_libraries(MeterBridge PRIVATE ws2_32)
endif()
