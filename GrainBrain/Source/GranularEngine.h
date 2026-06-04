#pragma once
#include <JuceHeader.h>
#include <random>

class GranularEngine
{
public:
    static constexpr int MAX_GRAINS = 16;
    static constexpr int BUFFER_SIZE = 960000;

    struct Grain {
        float readPos = 0.f;
        float pitchRatio = 1.f;
        float envPhase = 0.f;
        float envInc = 0.f;
        float size = 0.f;
        bool  active = false;
        bool  reverse = false;
    };

    void prepare(double sampleRate)
    {
        mSampleRate = sampleRate;
        mBuffer.assign(BUFFER_SIZE, 0.f);
        mWritePos = 0;
        mGrainTimer = 0.f;
        mFrozen = false;
        for (auto& g : mGrains) g.active = false;
    }

    float process(float input, float grainSizeMs, float scatter,
        float pitchSemitones, float pitchSpread, float drive, float mix,
        bool reverse, bool freeze)
    {
        if (!freeze)
        {
            mBuffer[mWritePos] = input;
            mWritePos = (mWritePos + 1) % BUFFER_SIZE;
        }

        // Grain size floor lowered to 16 samples (~0.4ms at 44.1kHz)
        float grainSizeSamples = juce::jmax((grainSizeMs / 1000.f) * (float)mSampleRate, 16.f);
        float interval = juce::jmax(grainSizeSamples * (1.f - scatter * 0.9f), 16.f);

        mGrainTimer += 1.f;
        if (mGrainTimer >= interval)
        {
            mGrainTimer = 0.f;
            triggerGrain(grainSizeSamples, pitchSemitones, pitchSpread, scatter, reverse);
        }

        float wet = 0.f;
        for (auto& g : mGrains)
        {
            if (!g.active) continue;

            float env = 0.5f * (1.f - std::cos(
                juce::MathConstants<float>::twoPi * g.envPhase));

            int idx = (int)g.readPos % BUFFER_SIZE;
            if (idx < 0) idx += BUFFER_SIZE;
            float s = mBuffer[idx] * env;

            // Logarithmic drive curve
            if (drive > 0.01f)
            {
                float gain = 1.f + (std::exp(drive * 5.f) - 1.f) * 8.f;
                s = std::tanh(s * gain);
                if (drive > 0.5f)
                    s = s - 0.15f * std::sin(s * juce::MathConstants<float>::twoPi
                        * (drive - 0.5f) * 4.f);
            }

            wet += s;

            g.readPos += g.reverse ? -g.pitchRatio : g.pitchRatio;
            g.envPhase += g.envInc;
            if (g.envPhase >= 1.f) g.active = false;
        }

        wet *= 0.35f;
        return input * (1.f - mix) + wet * mix;
    }

private:
    void triggerGrain(float sizeSamples, float pitchSemitones, float pitchSpread,
        float scatter, bool reverse)
    {
        for (auto& g : mGrains)
        {
            if (g.active) continue;

            float randomOffset = (mDist(mRng) * 2.f - 1.f) * pitchSpread;
            g.pitchRatio = std::pow(2.f, (pitchSemitones + randomOffset) / 12.f);
            g.size = sizeSamples;
            g.envPhase = 0.f;
            g.envInc = 1.f / sizeSamples;
            g.reverse = reverse;

            float scatterOffset = scatter * sizeSamples * mDist(mRng);
            int startPos = (int)(mWritePos - sizeSamples - scatterOffset);
            while (startPos < 0) startPos += BUFFER_SIZE;
            startPos = startPos % BUFFER_SIZE;

            g.readPos = reverse ? (float)((startPos + (int)sizeSamples) % BUFFER_SIZE)
                : (float)startPos;
            g.active = true;
            return;
        }
    }

    std::vector<float> mBuffer;
    int    mWritePos = 0;
    float  mGrainTimer = 0.f;
    double mSampleRate = 44100.0;
    bool   mFrozen = false;
    Grain  mGrains[MAX_GRAINS];

    std::mt19937 mRng{ std::random_device{}() };
    std::uniform_real_distribution<float> mDist{ 0.f, 1.f };
};