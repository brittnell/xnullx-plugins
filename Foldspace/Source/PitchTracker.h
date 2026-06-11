#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

//==============================================================================
// Monophonic pitch tracker — YIN (cumulative-mean-normalised difference) on
// input decimated to ~12 kHz. Tuned bass-first: 30 Hz .. 1 kHz.
//
// Runs on the audio thread (push per base-rate sample); the analysis fires
// every HOP decimated samples (~22 ms). On low confidence the last good pitch
// is held, so brief consonants / silence don't yank the carrier around.
//==============================================================================
class PitchTracker
{
public:
    void prepare(double sampleRate)
    {
        decim   = juce::jmax(1, (int)std::lround(sampleRate / 12000.0));
        dsRate  = sampleRate / (double)decim;
        tauMin  = juce::jmax(2,  (int)(dsRate / 1000.0));       // 1 kHz ceiling
        tauMax  = (int)(dsRate / 30.0);                          // 30 Hz floor
        bufLen  = win + tauMax + 1;
        buf.assign((size_t)bufLen, 0.f);
        diff.assign((size_t)(tauMax + 1), 0.f);
        cmnd.assign((size_t)(tauMax + 1), 1.f);

        // gentle pre-decimation lowpass (~4 kHz) — analysis only
        lpG = 1.f - std::exp((float)(-juce::MathConstants<double>::twoPi * 4000.0 / sampleRate));

        writePos = 0; decimCount = 0; sinceHop = 0; filled = 0;
        lpState = 0.f; hz = 0.f; confidence = 0.f;
    }

    void push(float x) noexcept
    {
        lpState += lpG * (x - lpState);
        if (++decimCount < decim) return;
        decimCount = 0;

        buf[(size_t)writePos] = lpState;
        writePos = (writePos + 1 == bufLen) ? 0 : writePos + 1;
        if (filled < bufLen) ++filled;

        if (++sinceHop >= hop && filled >= bufLen)
        {
            sinceHop = 0;
            analyse();
        }
    }

    float getHz()         const noexcept { return hz; }
    float getConfidence() const noexcept { return confidence; }

private:
    void analyse() noexcept
    {
        // unwrap the ring into analysis order: oldest first
        // d(tau) = sum_{i<win} (x[i] - x[i+tau])^2
        auto at = [this](int i) noexcept -> float
        {
            int idx = writePos + i;                  // writePos == oldest sample
            if (idx >= bufLen) idx -= bufLen;
            return buf[(size_t)idx];
        };

        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            float sum = 0.f;
            for (int i = 0; i < win; ++i)
            {
                const float d = at(i) - at(i + tau);
                sum += d * d;
            }
            diff[(size_t)tau] = sum;
        }

        // cumulative mean normalised difference
        float running = 0.f;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            running += diff[(size_t)tau];
            cmnd[(size_t)tau] = running > 1.0e-12f
                ? diff[(size_t)tau] * (float)(tau - tauMin + 1) / running
                : 1.f;
        }

        // first dip under threshold, refined to its local minimum
        const float threshold = 0.15f;
        int best = -1;
        for (int tau = tauMin + 1; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t)tau] < threshold)
            {
                while (tau + 1 < tauMax && cmnd[(size_t)(tau + 1)] < cmnd[(size_t)tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            confidence *= 0.8f;                       // decay, hold last hz
            return;
        }

        // parabolic interpolation around the minimum
        float refined = (float)best;
        if (best > tauMin && best < tauMax)
        {
            const float a = cmnd[(size_t)(best - 1)];
            const float b = cmnd[(size_t)best];
            const float c = cmnd[(size_t)(best + 1)];
            const float den = a - 2.f * b + c;
            if (std::abs(den) > 1.0e-12f)
                refined += 0.5f * (a - c) / den;
        }

        hz = (float)(dsRate / (double)refined);
        confidence = juce::jlimit(0.f, 1.f, 1.f - cmnd[(size_t)best]);
    }

    static constexpr int win = 512;
    static constexpr int hop = 256;

    std::vector<float> buf, diff, cmnd;
    int   decim = 4, bufLen = 0, tauMin = 2, tauMax = 400;
    int   writePos = 0, decimCount = 0, sinceHop = 0, filled = 0;
    double dsRate = 11025.0;
    float lpState = 0.f, lpG = 0.5f;
    float hz = 0.f, confidence = 0.f;
};
