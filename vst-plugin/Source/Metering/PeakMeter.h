/* MeterBridge — Peak Meter DSP */
#pragma once
#include <cmath>
#include <algorithm>

class PeakMeter {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        peakL = peakR = 0.0f;
        /* Ballistics: ~300ms release */
        float releaseTimeMs = 300.0f;
        releaseCoeff = std::exp(-1.0f / (float)(sr * releaseTimeMs / 1000.0f));
    }

    void process(const float* left, const float* right, int numSamples) {
        float pL = 0.0f, pR = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float absL = std::fabs(left[i]);
            float absR = std::fabs(right[i]);
            pL = std::max(pL, absL);
            pR = std::max(pR, absR);
            if (absL >= 1.0f) ++clipsL;
            if (absR >= 1.0f) ++clipsR;
        }
        /* Attack: instant. Release: exponential decay */
        peakL = (pL > peakL) ? pL : peakL * releaseCoeff;
        peakR = (pR > peakR) ? pR : peakR * releaseCoeff;
    }

    float getPeakDbL() const { return toDb(peakL); }
    float getPeakDbR() const { return toDb(peakR); }
    uint16_t getClipsL() const { return clipsL; }
    uint16_t getClipsR() const { return clipsR; }
    void resetClips() { clipsL = clipsR = 0; }

private:
    static float toDb(float linear) {
        if (linear <= 0.0f) return -100.0f;
        return 20.0f * std::log10(linear);
    }
    double sr = 44100.0;
    float peakL = 0.0f, peakR = 0.0f;
    float releaseCoeff = 0.999f;
    uint16_t clipsL = 0, clipsR = 0;
};
