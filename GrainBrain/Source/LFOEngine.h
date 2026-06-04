#pragma once
#include <JuceHeader.h>
#include <random>

class LFOEngine
{
public:
    enum Shape { Sine = 0, SawUp = 1, SawDown = 2, Triangle = 3, Step = 4, Random = 5 };

    void prepare(double sampleRate)
    {
        mSampleRate = sampleRate;
        mPhase = 0.f;
        mRandomValue = mDist(mRng) * 2.f - 1.f;
    }

    void advance(float rateHz)
    {
        mPhase += rateHz / (float)mSampleRate;
        if (mPhase >= 1.f)
        {
            mPhase -= 1.f;
            mRandomValue = mDist(mRng) * 2.f - 1.f;
        }
    }

    float getValue(float phaseOffset, int shape) const
    {
        float p = mPhase + phaseOffset;
        p -= std::floor(p);

        switch (shape)
        {
        case Sine:     return std::sin(p * juce::MathConstants<float>::twoPi);
        case SawUp:    return p * 2.f - 1.f;
        case SawDown:  return 1.f - p * 2.f;
        case Triangle: return p < 0.5f ? p * 4.f - 1.f : 3.f - p * 4.f;
        case Step:     return p < 0.5f ? 1.f : -1.f;
        case Random:   return mRandomValue;
        default:       return 0.f;
        }
    }

private:
    double mSampleRate = 44100.0;
    float  mPhase = 0.f;
    float  mRandomValue = 0.f;

    std::mt19937 mRng{ std::random_device{}() };
    std::uniform_real_distribution<float> mDist{ 0.f, 1.f };
};