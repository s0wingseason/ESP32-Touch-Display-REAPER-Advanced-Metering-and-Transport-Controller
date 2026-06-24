/*
 * MeterBridge — VST3 Plugin Editor Implementation
 * Minimal configuration GUI with connection settings and meter preview.
 */

#include "PluginEditor.h"

MeterBridgeEditor::MeterBridgeEditor(MeterBridgeProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 300);

    /* Title */
    titleLabel.setText("MeterBridge", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFE040FB));
    addAndMakeVisible(titleLabel);

    /* IP Address */
    ipLabel.setText("ESP32 IP:", juce::dontSendNotification);
    ipLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(ipLabel);

    ipInput.setText(processor.targetIP);
    ipInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A24));
    ipInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(ipInput);

    /* Port */
    portLabel.setText("Port:", juce::dontSendNotification);
    portLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(portLabel);

    portInput.setText(juce::String(processor.targetPort));
    portInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A24));
    portInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(portInput);

    /* Buttons */
    connectButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00E676));
    connectButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    connectButton.onClick = [this] { onConnect(); };
    addAndMakeVisible(connectButton);

    disconnectButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF1744));
    disconnectButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    disconnectButton.onClick = [this] { onDisconnect(); };
    addAndMakeVisible(disconnectButton);

    /* Status */
    statusLabel.setText("Disconnected", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(statusLabel);

    /* Meter preview labels */
    auto setupMeterLabel = [this](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font("Consolas", 13.0f, juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(lbl);
    };
    setupMeterLabel(peakLLabel, "Peak L: ---");
    setupMeterLabel(peakRLabel, "Peak R: ---");
    setupMeterLabel(rmsLLabel,  "RMS  L: ---");
    setupMeterLabel(rmsRLabel,  "RMS  R: ---");
    setupMeterLabel(lufsLabel,  "LUFS-I: ---");
    setupMeterLabel(phaseLabel, "Phase:  ---");

    startTimerHz(15);
}

MeterBridgeEditor::~MeterBridgeEditor() {
    stopTimer();
}

void MeterBridgeEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF0A0A0F));

    /* Bottom accent line */
    g.setColour(juce::Colour(0xFF7C4DFF));
    g.fillRect(0, getHeight() - 3, getWidth(), 3);

    /* Divider line */
    g.setColour(juce::Colour(0xFF2A2A38));
    g.fillRect(20, 150, getWidth() - 40, 1);
}

void MeterBridgeEditor::resized() {
    int y = 10;
    titleLabel.setBounds(20, y, 200, 30); y += 40;

    ipLabel.setBounds(20, y, 70, 24);
    ipInput.setBounds(90, y, 160, 24);
    portLabel.setBounds(260, y, 40, 24);
    portInput.setBounds(300, y, 80, 24);
    y += 32;

    connectButton.setBounds(90, y, 100, 28);
    disconnectButton.setBounds(200, y, 100, 28);
    y += 32;

    statusLabel.setBounds(90, y, 200, 20);
    y += 10;

    /* Meter preview section */
    y = 160;
    peakLLabel.setBounds(20, y, 170, 18);
    peakRLabel.setBounds(200, y, 170, 18); y += 20;
    rmsLLabel.setBounds(20, y, 170, 18);
    rmsRLabel.setBounds(200, y, 170, 18); y += 20;
    lufsLabel.setBounds(20, y, 170, 18);
    phaseLabel.setBounds(200, y, 170, 18);
}

void MeterBridgeEditor::timerCallback() {
    auto& mv = processor.meterValues;

    auto fmt = [](float db) -> juce::String {
        if (db <= -70.0f) return "---";
        return juce::String(db, 1) + " dB";
    };

    peakLLabel.setText("Peak L: " + fmt(mv.peakL.load()), juce::dontSendNotification);
    peakRLabel.setText("Peak R: " + fmt(mv.peakR.load()), juce::dontSendNotification);
    rmsLLabel.setText("RMS  L: " + fmt(mv.rmsL.load()), juce::dontSendNotification);
    rmsRLabel.setText("RMS  R: " + fmt(mv.rmsR.load()), juce::dontSendNotification);
    lufsLabel.setText("LUFS-I: " + fmt(mv.lufsIntegrated.load()), juce::dontSendNotification);

    float ph = mv.phaseCorrelation.load();
    phaseLabel.setText("Phase:  " + juce::String(ph, 2), juce::dontSendNotification);

    statusLabel.setText(processor.streamingEnabled ? "Streaming..." : "Disconnected",
                        juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          processor.streamingEnabled ? juce::Colour(0xFF00E676) : juce::Colour(0xFF888888));
}

void MeterBridgeEditor::onConnect() {
    processor.targetIP = ipInput.getText();
    processor.targetPort = portInput.getText().getIntValue();
    processor.startStreaming();
}

void MeterBridgeEditor::onDisconnect() {
    processor.stopStreaming();
}

juce::AudioProcessorEditor* MeterBridgeProcessor::createEditor() {
    return new MeterBridgeEditor(*this);
}
