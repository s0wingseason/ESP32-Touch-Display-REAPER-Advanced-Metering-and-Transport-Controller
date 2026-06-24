/*
 * MeterBridge — VST3 Plugin Processor Implementation
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

MeterBridgeProcessor::MeterBridgeProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    /* Try to initialize REAPER API bridge */
    reaper.initialize();
}

MeterBridgeProcessor::~MeterBridgeProcessor() {
    stopStreaming();
    stopTimer();
}

void MeterBridgeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    peakMeter.prepare(sampleRate);
    rmsMeter.prepare(sampleRate, 300.0f);
    lufsEngine.prepare(sampleRate, samplesPerBlock);
    phaseMeter.prepare(sampleRate, 300.0f);
    
    meterValues.resetAll();
}

void MeterBridgeProcessor::releaseResources() {
    /* Nothing to release */
}

void MeterBridgeProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& /*midi*/) {
    /* Pass audio through unchanged (this is a metering plugin, not an effect) */
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels < 2 || numSamples == 0) return;
    
    const float* left  = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);
    
    /* Run all metering DSP */
    peakMeter.process(left, right, numSamples);
    rmsMeter.process(left, right, numSamples);
    lufsEngine.process(left, right, numSamples);
    phaseMeter.process(left, right, numSamples);
    
    /* Update atomic meter values (thread-safe for network thread) */
    meterValues.peakL.store(peakMeter.getPeakDbL());
    meterValues.peakR.store(peakMeter.getPeakDbR());
    meterValues.truePeakL.store(peakMeter.getPeakDbL()); /* TODO: true peak with oversampling */
    meterValues.truePeakR.store(peakMeter.getPeakDbR());
    meterValues.rmsL.store(rmsMeter.getRmsDbL());
    meterValues.rmsR.store(rmsMeter.getRmsDbR());
    meterValues.lufsMomentary.store(lufsEngine.getMomentary());
    meterValues.lufsShortTerm.store(lufsEngine.getShortTerm());
    meterValues.lufsIntegrated.store(lufsEngine.getIntegrated());
    meterValues.lufsRange.store(lufsEngine.getRange());
    meterValues.phaseCorrelation.store(phaseMeter.getCorrelation());
    meterValues.clipCountL.store(peakMeter.getClipsL());
    meterValues.clipCountR.store(peakMeter.getClipsR());
}

void MeterBridgeProcessor::startStreaming() {
    if (streamingEnabled) return;
    
    streamer.setMeterValues(&meterValues);
    streamer.start(targetIP, targetPort);
    
    listener.startListening(targetPort + 1,
        [this](uint8_t cmd, uint8_t p8, float pf) {
            handleESP32Command(cmd, p8, pf);
        });
    
    /* Start polling REAPER state at 30Hz */
    startTimerHz(30);
    streamingEnabled = true;
    
    DBG("MeterBridge: Streaming started to " + targetIP + ":" + juce::String(targetPort));
}

void MeterBridgeProcessor::stopStreaming() {
    if (!streamingEnabled) return;
    
    stopTimer();
    streamer.stop();
    listener.stopListening();
    streamingEnabled = false;
    
    DBG("MeterBridge: Streaming stopped");
}

void MeterBridgeProcessor::timerCallback() {
    /* Poll REAPER transport state and send updates */
    if (!reaper.isAvailable()) return;
    
    uint8_t flags = 0;
    if (reaper.isPlaying())   flags |= MB_TRANSPORT_PLAYING;
    if (reaper.isPaused())    flags |= MB_TRANSPORT_PAUSED;
    if (reaper.isRecording()) flags |= MB_TRANSPORT_RECORDING;
    if (reaper.isRepeatOn())  flags |= MB_TRANSPORT_REPEAT;
    if (reaper.isMetronomeOn()) flags |= MB_TRANSPORT_METRONOME;
    if (!reaper.isPlaying() && !reaper.isPaused()) flags |= MB_TRANSPORT_STOPPED;
    
    int tsNum = 4, tsDen = 4;
    reaper.getTimeSig(tsNum, tsDen);
    
    streamer.setTransportState(
        flags, 0,
        (uint8_t)tsNum, (uint8_t)tsDen,
        (float)reaper.getPlayPosition(),
        (float)reaper.getPlayPosition(),
        (float)reaper.getTempo()
    );
    
    /* Update track info periodically */
    void* track = reaper.getMasterTrack();
    if (track) {
        uint8_t r, g, b;
        reaper.getTrackColor(track, r, g, b);
        streamer.setTrackInfo(
            0, reaper.getTrackName(track),
            r, g, b,
            reaper.isTrackMuted(track),
            reaper.isTrackSoloed(track),
            reaper.isTrackArmed(track)
        );
    }
}

void MeterBridgeProcessor::handleESP32Command(uint8_t cmd, uint8_t param8, float paramFloat) {
    if (!reaper.isAvailable()) return;
    
    switch (cmd) {
        case MB_CMD_PLAY:             reaper.play(); break;
        case MB_CMD_STOP:             reaper.stop(); break;
        case MB_CMD_RECORD:           reaper.record(); break;
        case MB_CMD_REWIND:           reaper.rewind(); break;
        case MB_CMD_FORWARD:          reaper.forward(); break;
        case MB_CMD_TOGGLE_REPEAT:    reaper.toggleRepeat(); break;
        case MB_CMD_TOGGLE_METRONOME: reaper.toggleMetronome(); break;
        case MB_CMD_SET_METER_SRC:
            /* TODO: Switch between master and selected track metering */
            break;
        default: break;
    }
}

void MeterBridgeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::ValueTree state("MeterBridge");
    state.setProperty("targetIP", targetIP, nullptr);
    state.setProperty("targetPort", targetPort, nullptr);
    
    juce::MemoryOutputStream stream(dest, true);
    state.writeToStream(stream);
}

void MeterBridgeProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto tree = juce::ValueTree::readFromData(data, (size_t)sizeInBytes);
    if (tree.isValid()) {
        targetIP = tree.getProperty("targetIP", "192.168.1.100").toString();
        targetPort = (int)tree.getProperty("targetPort", MB_DEFAULT_PORT);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MeterBridgeProcessor();
}
