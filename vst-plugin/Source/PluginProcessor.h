/*
 * MeterBridge — VST3 Plugin Processor
 * 
 * Core audio processor that performs all metering DSP and
 * coordinates network streaming to the ESP32 display.
 */

#pragma once
#include <JuceHeader.h>
#include "Metering/MeterValues.h"
#include "Metering/PeakMeter.h"
#include "Metering/RmsMeter.h"
#include "Metering/LufsEngine.h"
#include "Metering/PhaseMeter.h"
#include "Network/UdpStreamer.h"
#include "Network/UdpListener.h"
#include "ReaperIntegration/ReaperBridge.h"

class MeterBridgeProcessor : public juce::AudioProcessor,
                              public juce::Timer {
public:
    MeterBridgeProcessor();
    ~MeterBridgeProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MeterBridge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /* Public access to meter values for the editor */
    MeterValues meterValues;
    
    /* Connection settings */
    juce::String targetIP{"192.168.1.100"};
    int targetPort{MB_DEFAULT_PORT};
    bool streamingEnabled{false};

    /* REAPER bridge */
    ReaperBridge reaper;

    void startStreaming();
    void stopStreaming();

private:
    void timerCallback() override;
    void handleESP32Command(uint8_t cmd, uint8_t param8, float paramFloat);

    /* DSP engines */
    PeakMeter peakMeter;
    RmsMeter rmsMeter;
    LufsEngine lufsEngine;
    PhaseMeter phaseMeter;

    /* Network */
    UdpStreamer streamer;
    UdpListener listener;

    /* Transport polling state */
    uint8_t lastTransportFlags{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterBridgeProcessor)
};
