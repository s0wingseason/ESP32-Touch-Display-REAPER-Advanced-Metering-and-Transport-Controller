/*
 * MeterBridge — VST3 Plugin Editor (Configuration GUI)
 */

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MeterBridgeEditor : public juce::AudioProcessorEditor,
                           public juce::Timer {
public:
    MeterBridgeEditor(MeterBridgeProcessor&);
    ~MeterBridgeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MeterBridgeProcessor& processor;

    /* UI Components */
    juce::Label titleLabel;
    juce::Label ipLabel, portLabel, statusLabel;
    juce::TextEditor ipInput, portInput;
    juce::TextButton connectButton{"Connect"};
    juce::TextButton disconnectButton{"Disconnect"};

    /* Meter display (mini preview) */
    juce::Label peakLLabel, peakRLabel, rmsLLabel, rmsRLabel;
    juce::Label lufsLabel, phaseLabel;

    void onConnect();
    void onDisconnect();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterBridgeEditor)
};
