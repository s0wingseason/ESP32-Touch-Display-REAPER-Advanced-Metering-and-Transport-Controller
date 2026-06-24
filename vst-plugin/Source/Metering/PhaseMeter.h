/* MeterBridge — Phase Correlation Meter */
#pragma once
#include <cmath>
#include <vector>

class PhaseMeter {
public:
    void prepare(double sampleRate, float windowMs = 300.0f) {
        windowSize = (int)(sampleRate * windowMs / 1000.0f);
        bufL.resize(windowSize, 0.0f);
        bufR.resize(windowSize, 0.0f);
        writePos = 0;
        sumLR = sumLL = sumRR = 0.0;
    }

    void process(const float* left, const float* right, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float oldL = bufL[writePos], oldR = bufR[writePos];
            sumLR -= (double)(oldL * oldR);
            sumLL -= (double)(oldL * oldL);
            sumRR -= (double)(oldR * oldR);
            bufL[writePos] = left[i];
            bufR[writePos] = right[i];
            sumLR += (double)(left[i] * right[i]);
            sumLL += (double)(left[i] * left[i]);
            sumRR += (double)(right[i] * right[i]);
            writePos = (writePos + 1) % windowSize;
        }
    }

    float getCorrelation() const {
        double denom = std::sqrt(std::max(0.0, sumLL) * std::max(0.0, sumRR));
        if (denom <= 1e-10) return 1.0f;
        return (float)std::clamp(sumLR / denom, -1.0, 1.0);
    }

private:
    int windowSize = 13230;
    std::vector<float> bufL, bufR;
    int writePos = 0;
    double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
};
