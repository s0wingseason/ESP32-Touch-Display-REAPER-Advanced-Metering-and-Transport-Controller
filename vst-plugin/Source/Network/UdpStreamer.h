/*
 * MeterBridge — UDP Streamer
 * Background thread that reads atomic meter values and sends
 * them to the ESP32 at ~60Hz via UDP.
 */

#pragma once
#include <JuceHeader.h>
#include "Protocol.h"
#include "../Metering/MeterValues.h"

class UdpStreamer : public juce::Timer {
public:
    UdpStreamer() = default;
    ~UdpStreamer() override { stopTimer(); }

    void start(const juce::String& targetIP, int targetPort) {
        ip = targetIP;
        port = targetPort;
        socket.bindToPort(0); /* Bind to any available port */
        seq = 0;
        startTimerHz(60); /* 60 Hz streaming rate */
    }

    void stop() {
        stopTimer();
        socket.shutdown();
    }

    void setMeterValues(MeterValues* mv) { meters = mv; }

    void setTransportState(uint8_t flags, uint8_t src, uint8_t tsNum, uint8_t tsDen,
                           float posBeat, float posSec, float bpm) {
        transportFlags.store(flags);
        meterSource.store(src);
        timeSigNum.store(tsNum);
        timeSigDen.store(tsDen);
        posBeats.store(posBeat);
        posSecs.store(posSec);
        tempoBpm.store(bpm);
        transportDirty.store(true);
    }

    void setTrackInfo(uint8_t idx, const juce::String& name,
                      uint8_t r, uint8_t g, uint8_t b,
                      bool muted, bool soloed, bool armed) {
        trackIndex.store(idx);
        trackColorR.store(r); trackColorG.store(g); trackColorB.store(b);
        trackMuted.store(muted); trackSoloed.store(soloed); trackArmed.store(armed);
        trackName = name;
        trackDirty.store(true);
    }

    bool isConnected() const { return connected.load(); }

private:
    void timerCallback() override {
        if (!meters || ip.isEmpty()) return;

        /* Send meter data every frame */
        sendMeterPacket();

        /* Send transport state when changed or every ~0.5s */
        if (transportDirty.load() || (seq % 30 == 0)) {
            sendTransportPacket();
            transportDirty.store(false);
        }

        /* Send track info when changed */
        if (trackDirty.load()) {
            sendTrackInfoPacket();
            trackDirty.store(false);
        }

        /* Heartbeat every ~2s (120 frames at 60Hz) */
        if (seq % 120 == 0) sendHeartbeat();
    }

    void sendMeterPacket() {
        mb_meter_packet_t pkt;
        mb_init_header(&pkt.header, MB_PKT_METER_DATA, seq++);
        pkt.peak_l = meters->peakL.load();
        pkt.peak_r = meters->peakR.load();
        pkt.true_peak_l = meters->truePeakL.load();
        pkt.true_peak_r = meters->truePeakR.load();
        pkt.rms_l = meters->rmsL.load();
        pkt.rms_r = meters->rmsR.load();
        pkt.lufs_momentary = meters->lufsMomentary.load();
        pkt.lufs_short = meters->lufsShortTerm.load();
        pkt.lufs_integrated = meters->lufsIntegrated.load();
        pkt.lufs_range = meters->lufsRange.load();
        pkt.phase_correlation = meters->phaseCorrelation.load();
        pkt.clip_count_l = meters->clipCountL.load();
        pkt.clip_count_r = meters->clipCountR.load();

        socket.write(ip, port, &pkt, sizeof(pkt));
    }

    void sendTransportPacket() {
        mb_transport_packet_t pkt;
        mb_init_header(&pkt.header, MB_PKT_TRANSPORT_STATE, seq++);
        pkt.state_flags = transportFlags.load();
        pkt.meter_source = meterSource.load();
        pkt.time_sig_num = timeSigNum.load();
        pkt.time_sig_den = timeSigDen.load();
        pkt.position_beats = posBeats.load();
        pkt.position_secs = posSecs.load();
        pkt.tempo_bpm = tempoBpm.load();

        socket.write(ip, port, &pkt, sizeof(pkt));
    }

    void sendTrackInfoPacket() {
        mb_track_info_packet_t pkt;
        mb_init_header(&pkt.header, MB_PKT_TRACK_INFO, seq++);
        pkt.track_index = trackIndex.load();
        pkt.track_color_r = trackColorR.load();
        pkt.track_color_g = trackColorG.load();
        pkt.track_color_b = trackColorB.load();
        memset(pkt.track_name, 0, MB_MAX_TRACK_NAME_LEN);
        trackName.copyToUTF8(pkt.track_name, MB_MAX_TRACK_NAME_LEN - 1);
        pkt.is_muted = trackMuted.load() ? 1 : 0;
        pkt.is_soloed = trackSoloed.load() ? 1 : 0;
        pkt.is_armed = trackArmed.load() ? 1 : 0;
        pkt.reserved = 0;

        socket.write(ip, port, &pkt, sizeof(pkt));
    }

    void sendHeartbeat() {
        mb_heartbeat_packet_t pkt;
        mb_init_header(&pkt.header, MB_PKT_HEARTBEAT, seq++);
        pkt.uptime_ms = (uint32_t)juce::Time::getMillisecondCounter();
        socket.write(ip, port, &pkt, sizeof(pkt));
    }

    juce::DatagramSocket socket{false};
    juce::String ip;
    int port = MB_DEFAULT_PORT;
    uint16_t seq = 0;
    MeterValues* meters = nullptr;
    std::atomic<bool> connected{false};

    /* Transport state (atomic for thread safety) */
    std::atomic<uint8_t> transportFlags{0};
    std::atomic<uint8_t> meterSource{0};
    std::atomic<uint8_t> timeSigNum{4};
    std::atomic<uint8_t> timeSigDen{4};
    std::atomic<float> posBeats{0.0f};
    std::atomic<float> posSecs{0.0f};
    std::atomic<float> tempoBpm{120.0f};
    std::atomic<bool> transportDirty{true};

    /* Track info */
    std::atomic<uint8_t> trackIndex{0};
    std::atomic<uint8_t> trackColorR{255}, trackColorG{0}, trackColorB{255};
    std::atomic<bool> trackMuted{false}, trackSoloed{false}, trackArmed{false};
    juce::String trackName{"MASTER"};
    std::atomic<bool> trackDirty{true};
};
