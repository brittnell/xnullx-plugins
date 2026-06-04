#pragma once
#include <JuceHeader.h>

class StutterGate
{
public:
    void prepare(double sampleRate)
    {
        mSampleRate = sampleRate;
        mPhase = 0.f;
    }

    // gateCycleSamples: length of one full gate cycle in samples
    // phaseOffset: 0-1, shifts the gate phase relative to other bands
    // depth: 0-1, how much the gate closes (0=no effect, 1=full silence)
    // attack: 0-0.49, fraction of cycle spent opening
    // release: 0-0.49, fraction of cycle spent closing
    float process(float input, float gateCycleSamples, float phaseOffset,
        float depth, float attack, float release, bool enabled)
    {
        if (!enabled || depth < 0.001f)
            return input;

        mPhase += 1.f / gateCycleSamples;
        if (mPhase >= 1.f) mPhase -= 1.f;

        // Apply phase offset
        float p = mPhase + phaseOffset;
        if (p >= 1.f) p -= 1.f;

        // Trapezoid envelope with a real open AND closed phase (≈50% duty):
        //   [0, attack)            rising edge   (0 → 1)
        //   [attack, openHold)     open hold     (1)
        //   [openHold, +release)   falling edge  (1 → 0)
        //   [.., 1)                closed hold   (0)
        // attack / release only shape the edge ramps; Phase shifts the whole cycle.
        const float openHold = 0.5f;            // gate open for the first half of the cycle
        const float closeEnd = openHold + release;

        float env;
        if (p < attack)
            env = attack > 0.f ? p / attack : 1.f;                 // rising edge
        else if (p < openHold)
            env = 1.f;                                             // open hold
        else if (p < closeEnd)
            env = release > 0.f ? 1.f - (p - openHold) / release : 0.f;  // falling edge
        else
            env = 0.f;                                             // closed hold

        // Gate gain: 1 when open, (1-depth) when closed
        float gain = 1.f - depth * (1.f - env);
        return input * gain;
    }

    void resetPhase() { mPhase = 0.f; }

private:
    double mSampleRate = 44100.0;
    float  mPhase = 0.f;
};