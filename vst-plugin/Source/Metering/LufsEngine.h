/*
 * MeterBridge — ITU-R BS.1770-4 LUFS Engine
 * 
 * Implements Momentary (400ms), Short-term (3s), and Integrated loudness.
 * Uses K-weighting filter (two biquads: shelf + high-pass).
 */

#pragma once
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <vector>
#include <algorithm>
#include <numeric>

class LufsEngine {
public:
    void prepare(double sampleRate, int blockSize) {
        sr = sampleRate;
        
        /* K-weighting filter coefficients (ITU-R BS.1770-4) */
        computeShelfCoeffs(sampleRate);
        computeHighPassCoeffs(sampleRate);
        
        /* Reset filter states */
        for (auto& s : shelfState) s = {0,0,0,0};
        for (auto& s : hpState) s = {0,0,0,0};
        
        /* Gating block size: 400ms with 75% overlap = 100ms hop */
        blockSamples400 = (int)(sampleRate * 0.4);
        blockSamples3s  = (int)(sampleRate * 3.0);
        hopSamples      = (int)(sampleRate * 0.1);
        
        /* Ring buffer for 3s of filtered power */
        int maxBlocks = blockSamples3s / hopSamples;
        blockPowers.resize(maxBlocks, -100.0);
        blockWritePos = 0;
        
        accL.clear(); accR.clear();
        accL.resize(blockSamples400, 0.0f);
        accR.resize(blockSamples400, 0.0f);
        accPos = 0;
        hopCounter = 0;
        
        /* Integrated loudness gating */
        allBlockPowers.clear();
        integratedLUFS = -100.0;
    }

    void process(const float* left, const float* right, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            /* Apply K-weighting: stage 1 (shelf) then stage 2 (high-pass) */
            float fL = applyBiquad(left[i],  shelfCoeffs, shelfState[0]);
            fL       = applyBiquad(fL,        hpCoeffs,    hpState[0]);
            float fR = applyBiquad(right[i], shelfCoeffs, shelfState[1]);
            fR       = applyBiquad(fR,        hpCoeffs,    hpState[1]);
            
            /* Accumulate into 400ms ring buffer */
            accL[accPos] = fL;
            accR[accPos] = fR;
            accPos = (accPos + 1) % blockSamples400;
            
            hopCounter++;
            if (hopCounter >= hopSamples) {
                hopCounter = 0;
                computeBlockPower();
            }
        }
    }

    float getMomentary() const { return momentaryLUFS; }
    float getShortTerm() const { return shortTermLUFS; }
    float getIntegrated() const { return integratedLUFS; }
    float getRange() const { return lufsRange; }

    void resetIntegrated() {
        allBlockPowers.clear();
        integratedLUFS = -100.0;
        lufsRange = 0.0f;
    }

private:
    /* Biquad filter state */
    struct BiquadState { double x1, x2, y1, y2; };
    struct BiquadCoeffs { double b0, b1, b2, a1, a2; };
    
    BiquadCoeffs shelfCoeffs, hpCoeffs;
    BiquadState shelfState[2], hpState[2]; /* L,R */

    double sr = 48000.0;
    int blockSamples400 = 19200;
    int blockSamples3s = 144000;
    int hopSamples = 4800;
    
    std::vector<float> accL, accR;
    int accPos = 0;
    int hopCounter = 0;
    
    std::vector<double> blockPowers;
    int blockWritePos = 0;
    
    std::vector<double> allBlockPowers;
    
    float momentaryLUFS = -100.0f;
    float shortTermLUFS = -100.0f;
    float integratedLUFS = -100.0f;
    float lufsRange = 0.0f;

    static float applyBiquad(float input, const BiquadCoeffs& c, BiquadState& s) {
        double x = (double)input;
        double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return (float)y;
    }

    void computeShelfCoeffs(double fs) {
        /* Pre-filter (high-shelf, +4dB at high frequencies) */
        double db = 3.999843853973347;
        double f0 = 1681.974450955533;
        double Q  = 0.7071752369554196;
        double K  = std::tan(M_PI * f0 / fs);
        double Vh = std::pow(10.0, db / 20.0);
        double Vb = std::pow(Vh, 0.4996667741545416);
        double a0 = 1.0 + K / Q + K * K;
        shelfCoeffs.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelfCoeffs.b1 = 2.0 * (K * K - Vh) / a0;
        shelfCoeffs.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelfCoeffs.a1 = 2.0 * (K * K - 1.0) / a0;
        shelfCoeffs.a2 = (1.0 - K / Q + K * K) / a0;
    }

    void computeHighPassCoeffs(double fs) {
        /* RLB weighting (high-pass, ~60Hz) */
        double f0 = 38.13547087602444;
        double Q  = 0.5003270373238773;
        double K  = std::tan(M_PI * f0 / fs);
        double a0 = 1.0 + K / Q + K * K;
        hpCoeffs.b0 = 1.0 / a0;
        hpCoeffs.b1 = -2.0 / a0;
        hpCoeffs.b2 = 1.0 / a0;
        hpCoeffs.a1 = 2.0 * (K * K - 1.0) / a0;
        hpCoeffs.a2 = (1.0 - K / Q + K * K) / a0;
    }

    void computeBlockPower() {
        /* Compute mean square of the 400ms block */
        double sumL = 0.0, sumR = 0.0;
        for (int i = 0; i < blockSamples400; ++i) {
            sumL += (double)(accL[i] * accL[i]);
            sumR += (double)(accR[i] * accR[i]);
        }
        double meanL = sumL / blockSamples400;
        double meanR = sumR / blockSamples400;
        
        /* Stereo sum (G_l = G_r = 1.0 for L/R channels) */
        double power = meanL + meanR;
        
        /* Momentary LUFS = -0.691 + 10*log10(power) */
        if (power > 1e-10) {
            momentaryLUFS = (float)(-0.691 + 10.0 * std::log10(power));
        } else {
            momentaryLUFS = -100.0f;
        }
        
        /* Store for short-term and integrated computation */
        blockPowers[blockWritePos] = power;
        blockWritePos = (blockWritePos + 1) % (int)blockPowers.size();
        
        /* Short-term (3s window) */
        double stSum = 0.0;
        int stCount = std::min((int)blockPowers.size(), blockSamples3s / hopSamples);
        for (int i = 0; i < stCount; ++i) stSum += blockPowers[i];
        double stMean = stSum / stCount;
        if (stMean > 1e-10) {
            shortTermLUFS = (float)(-0.691 + 10.0 * std::log10(stMean));
        } else {
            shortTermLUFS = -100.0f;
        }
        
        /* Integrated (gated) */
        allBlockPowers.push_back(power);
        computeIntegrated();
    }

    void computeIntegrated() {
        if (allBlockPowers.empty()) return;
        
        /* Absolute gate: -70 LUFS */
        double absThresh = std::pow(10.0, (-70.0 + 0.691) / 10.0);
        double sum1 = 0.0;
        int count1 = 0;
        for (double p : allBlockPowers) {
            if (p > absThresh) { sum1 += p; count1++; }
        }
        if (count1 == 0) { integratedLUFS = -100.0f; return; }
        
        /* Relative gate: -10 LU below absolute-gated level */
        double absGated = -0.691 + 10.0 * std::log10(sum1 / count1);
        double relThresh = std::pow(10.0, (absGated - 10.0 + 0.691) / 10.0);
        
        double sum2 = 0.0;
        int count2 = 0;
        std::vector<double> gatedPowers;
        for (double p : allBlockPowers) {
            if (p > relThresh) {
                sum2 += p;
                count2++;
                gatedPowers.push_back(p);
            }
        }
        if (count2 == 0) { integratedLUFS = -100.0f; return; }
        
        integratedLUFS = (float)(-0.691 + 10.0 * std::log10(sum2 / count2));
        
        /* LRA (loudness range) — simplified */
        if (gatedPowers.size() >= 2) {
            std::sort(gatedPowers.begin(), gatedPowers.end());
            int lo = (int)(gatedPowers.size() * 0.10);
            int hi = (int)(gatedPowers.size() * 0.95);
            if (hi > lo && gatedPowers[lo] > 1e-10 && gatedPowers[hi] > 1e-10) {
                double loLUFS = -0.691 + 10.0 * std::log10(gatedPowers[lo]);
                double hiLUFS = -0.691 + 10.0 * std::log10(gatedPowers[hi]);
                lufsRange = (float)(hiLUFS - loLUFS);
            }
        }
    }
};
