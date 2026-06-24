/*
 * MeterBridge — Thread-Safe Meter Values
 * 
 * Lock-free storage for meter values computed on the audio thread
 * and consumed by the network streaming thread.
 */

#pragma once
#include <atomic>
#include <cmath>

struct MeterValues {
    // Peak
    std::atomic<float> peakL{-100.0f};
    std::atomic<float> peakR{-100.0f};
    
    // True Peak (4x oversampled)
    std::atomic<float> truePeakL{-100.0f};
    std::atomic<float> truePeakR{-100.0f};
    
    // RMS
    std::atomic<float> rmsL{-100.0f};
    std::atomic<float> rmsR{-100.0f};
    
    // LUFS
    std::atomic<float> lufsMomentary{-100.0f};
    std::atomic<float> lufsShortTerm{-100.0f};
    std::atomic<float> lufsIntegrated{-100.0f};
    std::atomic<float> lufsRange{0.0f};
    
    // Phase correlation
    std::atomic<float> phaseCorrelation{1.0f};
    
    // Clip counters
    std::atomic<uint16_t> clipCountL{0};
    std::atomic<uint16_t> clipCountR{0};
    
    void resetClips() {
        clipCountL.store(0);
        clipCountR.store(0);
    }
    
    void resetAll() {
        peakL.store(-100.0f);
        peakR.store(-100.0f);
        truePeakL.store(-100.0f);
        truePeakR.store(-100.0f);
        rmsL.store(-100.0f);
        rmsR.store(-100.0f);
        lufsMomentary.store(-100.0f);
        lufsShortTerm.store(-100.0f);
        lufsIntegrated.store(-100.0f);
        lufsRange.store(0.0f);
        phaseCorrelation.store(1.0f);
        resetClips();
    }
};
