/* MeterBridge — RMS Meter DSP */
#pragma once
#include <cmath>
#include <vector>

class RmsMeter {
public:
    void prepare(double sampleRate, float windowMs = 300.0f) {
        sr = sampleRate;
        windowSize = (int)(sampleRate * windowMs / 1000.0f);
        bufL.resize(windowSize, 0.0f);
        bufR.resize(windowSize, 0.0f);
        writePos = 0;
        sumL = sumR = 0.0;
    }

    void process(const float* left, const float* right, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            /* Remove oldest sample's contribution */
            sumL -= (double)(bufL[writePos] * bufL[writePos]);
            sumR -= (double)(bufR[writePos] * bufR[writePos]);
            /* Add new */
            bufL[writePos] = left[i];
            bufR[writePos] = right[i];
            sumL += (double)(left[i] * left[i]);
            sumR += (double)(right[i] * right[i]);
            writePos = (writePos + 1) % windowSize;
        }
        /* Clamp to prevent negative from floating point drift */
        if (sumL < 0.0) sumL = 0.0;
        if (sumR < 0.0) sumR = 0.0;
    }

    float getRmsDbL() const {
        double mean = sumL / windowSize;
        if (mean <= 0.0) return -100.0f;
        return (float)(10.0 * std::log10(mean));
    }

    float getRmsDbR() const {
        double mean = sumR / windowSize;
        if (mean <= 0.0) return -100.0f;
        return (float)(10.0 * std::log10(mean));
    }

private:
    double sr = 44100.0;
    int windowSize = 13230;
    std::vector<float> bufL, bufR;
    int writePos = 0;
    double sumL = 0.0, sumR = 0.0;
};
