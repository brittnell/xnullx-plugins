#pragma once
#include <JuceHeader.h>
#include "WaveFolder.h"
#include <vector>
#include <cmath>

//==============================================================================
// Per-sample DSP helpers + the per-channel Foldspace signal chain.
// The whole chain runs at the (possibly oversampled) processing rate.
//==============================================================================

struct OnePoleLP
{
    float s = 0.f;
    void  reset() noexcept { s = 0.f; }
    float process(float x, float g) noexcept { s += g * (x - s); return s; }
};

struct DCBlocker
{
    float x1 = 0.f, y1 = 0.f, R = 0.995f;
    void setRate(double sr) noexcept
    { R = 1.f - (float)(juce::MathConstants<double>::twoPi * 10.0 / sr); }
    void  reset() noexcept { x1 = y1 = 0.f; }
    float process(float x) noexcept
    { const float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

//==============================================================================
// Band-limited oscillator (PolyBLEP for saw/pulse; sine/tri are tame enough
// at the oversampled rate). phase in [0,1), inc = freq / processRate.
//==============================================================================
inline float polyBlep(float t, float dt) noexcept
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)  { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

inline float oscValue(int wave, float ph, float dt) noexcept
{
    switch (wave)
    {
        case 0:  return std::sin(juce::MathConstants<float>::twoPi * ph);   // sine
        case 1:  return 1.f - 4.f * std::abs(ph - 0.5f);                    // triangle
        case 2:                                                              // saw
        {
            float y = 2.f * ph - 1.f;
            return y - polyBlep(ph, dt);
        }
        default:                                                             // pulse
        {
            float y = ph < 0.5f ? 1.f : -1.f;
            y += polyBlep(ph, dt);
            float t2 = ph + 0.5f; if (t2 >= 1.f) t2 -= 1.f;
            return y - polyBlep(t2, dt);
        }
    }
}

//==============================================================================
// Values shared by both channels for one processing-rate sample frame.
// Computed once per frame by the processor, applied per channel.
//==============================================================================
struct PMStep
{
    float modPhase  = 0.f;   // modulator osc phase 0..1 (channel adds stereo offset)
    float modInc    = 0.f;   // modulator osc phase inc (for PolyBLEP)
    float carPhase  = 0.f;   // carrier osc phase 0..1
    int   wave      = 0;

    float srcOsc    = 1.f;   // modulator bus mix
    float srcInput  = 0.f;
    float fbAmt     = 0.f;   // feedback -> modulator bus (post curve, post drift)

    float modDriveG = 1.f;   // mod folder drive (1 = transparent)
    float idx       = 0.f;   // effective PM index 0..1 (env send applied)
    float carBlend  = 0.f;   // 0 = SIGNAL carrier, 1 = OSC carrier
    float centre    = 100.f; // PM centre tap, samples at processing rate

    float mainDriveG = 1.f;  // main folder drive
    float shape      = 0.f;  // tri..sine morph
    float bias       = 0.f;  // symmetry bias (pre-fold)
    float toneG      = 1.f;  // post-fold LP coefficient

    float fbDriveG  = 1.f;   // feedback folder drive
    float fbToneG   = 0.5f;  // feedback damping LP coefficient

    float env       = 0.f;   // input envelope (gates the carrier osc)
    float stereoOff = 0.f;   // modulator phase offset for the R channel, 0..0.5
};

//==============================================================================
class PMChannel
{
public:
    void prepare(double maxProcessRate)
    {
        // centre tap is 4 ms; modulation swings it 0..2x centre; + interp margin
        const int maxCentre = (int)std::ceil(0.004 * maxProcessRate);
        dlyLen = 2 * maxCentre + 16;
        dly.assign((size_t)dlyLen, 0.f);
        reset();
    }

    void setRate(double processRate) noexcept
    {
        mainDC.setRate(processRate);
        fbDC.setRate(processRate);
    }

    void reset() noexcept
    {
        std::fill(dly.begin(), dly.end(), 0.f);
        wpos = 0;
        fbState = 0.f;
        lastMod = 0.f;
        modFold.reset(); mainFold.reset(); fbFold.reset();
        mainDC.reset(); fbDC.reset();
        toneLP.reset(); fbLP.reset();
    }

    // chOff: 0 for L, 1 for R (R gets the stereo modulator phase offset)
    float processSample(float in, float chOff, const PMStep& p) noexcept
    {
        // ----- modulator bus -------------------------------------------------
        float mph = p.modPhase + chOff * p.stereoOff;
        if (mph >= 1.f) mph -= 1.f;
        const float osc  = oscValue(p.wave, mph, p.modInc);
        const float mraw = p.srcOsc * osc + p.srcInput * in + p.fbAmt * fbState;
        const float m    = modFold.processTri(mraw * p.modDriveG);
        lastMod = m;

        const float mc = juce::jlimit(-1.f, 1.f, m);

        // ----- PM stage ------------------------------------------------------
        dly[(size_t)wpos] = in;
        const float delaySamps = p.centre * (1.f + 0.98f * p.idx * mc);
        const float sig = readCatmullRom(delaySamps);
        wpos = (wpos + 1 == dlyLen) ? 0 : wpos + 1;

        float pm = sig;
        if (p.carBlend > 0.0001f)
        {
            const float carAmp = juce::jmin(1.f, p.env * 1.2f);
            const float car = std::sin(juce::MathConstants<float>::twoPi * p.carPhase
                                       + 4.f * juce::MathConstants<float>::pi * p.idx * m) * carAmp;
            pm += p.carBlend * (car - sig);
        }

        // ----- main fold -> DC block -> tone --------------------------------
        float y = mainFold.processMorph(pm * p.mainDriveG + p.bias, p.shape);
        y = mainDC.process(y);
        y = toneLP.process(y, p.toneG);

        // ----- feedback derivation (used NEXT sample, scaled by fbAmt) ------
        float f = fbFold.processTri(y * p.fbDriveG);
        f = fbLP.process(f, p.fbToneG);
        f = fbDC.process(f);
        fbState = std::tanh(f);

        return y;
    }

    float getLastMod() const noexcept { return lastMod; }

private:
    float readCatmullRom(float delaySamps) const noexcept
    {
        // read relative to the sample just written at wpos
        float pos = (float)wpos - delaySamps;
        while (pos < 0.f) pos += (float)dlyLen;
        const int   i1 = (int)pos;
        const float fr = pos - (float)i1;

        auto tap = [this](int i) noexcept -> float
        {
            i %= dlyLen; if (i < 0) i += dlyLen;
            return dly[(size_t)i];
        };
        const float y0 = tap(i1 - 1), y1 = tap(i1), y2 = tap(i1 + 1), y3 = tap(i1 + 2);

        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 =         y0 - 2.5f * y1 + 2.f  * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0              + 0.5f * y2;
        return ((a0 * fr + a1) * fr + a2) * fr + y1;
    }

    std::vector<float> dly;
    int   dlyLen = 0, wpos = 0;
    float fbState = 0.f, lastMod = 0.f;

    WaveFolder modFold, mainFold, fbFold;
    DCBlocker  mainDC, fbDC;
    OnePoleLP  toneLP, fbLP;
};
