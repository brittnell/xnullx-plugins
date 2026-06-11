#pragma once
#include <JuceHeader.h>
#include <cmath>

//==============================================================================
// Peak envelope follower with independent attack/release ballistics.
// Drives the ENV->INDEX / ENV->FOLD sends and gates the internal carrier's
// amplitude so the oscillator never drones over silence.
//==============================================================================
struct EnvelopeFollower
{
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setTimes(atkMs, relMs);
        env = 0.f;
    }

    void setTimes(float attackMs, float releaseMs) noexcept
    {
        atkMs = attackMs; relMs = releaseMs;
        atkG = 1.f - std::exp(-1.f / (float)(sr * juce::jmax(0.0001f, atkMs) * 0.001));
        relG = 1.f - std::exp(-1.f / (float)(sr * juce::jmax(0.001f,  relMs) * 0.001));
    }

    float process(float absIn) noexcept
    {
        env += (absIn > env ? atkG : relG) * (absIn - env);
        return env;
    }

    float env = 0.f;

private:
    double sr = 44100.0;
    float atkMs = 5.f, relMs = 150.f;
    float atkG = 0.1f, relG = 0.01f;
};
