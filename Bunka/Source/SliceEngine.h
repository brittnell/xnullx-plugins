#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

//==============================================================================
// SliceEngine — stateless analysis utilities for Bunka.
//
//  * detectOnsets : energy-flux transient detection with a user sensitivity
//                   threshold. Returns sorted slice START offsets (always
//                   begins with 0). Each slice runs from its start to the next
//                   start (the engine derives ends from the start list + buffer
//                   length), i.e. attack-of-one-hit to attack-of-the-next.
//  * gridSlice    : fallback / "no discernible hits" mode — evenly divides the
//                   buffer by a musical division at a known BPM.
//
//  No hit CLASSIFICATION (kick vs snare) — only onset POSITION, by design.
//==============================================================================
class SliceEngine
{
public:
    // Musical divisions offered for grid slicing (beats per slice).
    // Index order must match the editor's GRID DIV combo box.
    //   0:1/4  1:1/8  2:1/16  3:1/32  4:1/8T  5:1/16T
    static double beatsForDivision (int divisionIndex) noexcept
    {
        static const double beats[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        const int n = (int) (sizeof (beats) / sizeof (double));
        return beats[juce::jlimit (0, n - 1, divisionIndex)];
    }

    static juce::StringArray divisionNames()
    {
        return { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    }

    //==========================================================================
    // Transient detection.
    //   threshold01 : 0..1 user sensitivity. LOW = many slices (sensitive),
    //                 HIGH = few slices (only the strongest transients).
    static std::vector<int> detectOnsets (const juce::AudioBuffer<float>& buffer,
                                          double sampleRate,
                                          float threshold01)
    {
        std::vector<int> starts;
        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();
        if (numSamples <= 0 || numCh <= 0 || sampleRate <= 0.0)
            return starts;

        const int hop = 128;             // ~2.9 ms @ 44.1k — low latency
        const int win = 512;             // analysis window

        // --- short-time RMS energy envelope, one value per hop ---------------
        std::vector<float> energy;
        energy.reserve ((size_t) (numSamples / hop) + 2);
        for (int pos = 0; pos < numSamples; pos += hop)
        {
            const int end = juce::jmin (pos + win, numSamples);
            const int len = end - pos;
            double e = 0.0;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* d = buffer.getReadPointer (ch);
                for (int i = pos; i < end; ++i)
                    e += (double) d[i] * (double) d[i];
            }
            energy.push_back ((float) std::sqrt (e / juce::jmax (1, len * numCh)));
        }

        const int nFrames = (int) energy.size();
        if (nFrames < 3)
        {
            starts.push_back (0);
            return starts;
        }

        // --- rectified energy flux, normalised to 0..1 ----------------------
        std::vector<float> flux ((size_t) nFrames, 0.f);
        float maxFlux = 1.0e-9f;
        for (int i = 1; i < nFrames; ++i)
        {
            const float d = energy[(size_t) i] - energy[(size_t) i - 1];
            flux[(size_t) i] = d > 0.f ? d : 0.f;
            maxFlux = juce::jmax (maxFlux, flux[(size_t) i]);
        }
        for (auto& f : flux) f /= maxFlux;

        // Map sensitivity: higher slider -> higher bar -> fewer onsets.
        const float thresh = juce::jmap (juce::jlimit (0.f, 1.f, threshold01),
                                         0.f, 1.f, 0.04f, 0.60f);

        // Debounce: enforce a minimum gap between onsets (~40 ms).
        const int minGap = juce::jmax (1, (int) std::lround (0.040 * sampleRate / hop));

        std::vector<int> ons;                       // detected transients (no forced 0)
        const int minSpacing = juce::jmax (1, (int) (0.010 * sampleRate));   // 10 ms
        int lastOnset = -minGap;
        for (int i = 1; i < nFrames - 1; ++i)
        {
            const bool isPeak = flux[(size_t) i] >= thresh
                             && flux[(size_t) i] >= flux[(size_t) i - 1]
                             && flux[(size_t) i] >= flux[(size_t) i + 1];

            if (isPeak && (i - lastOnset) >= minGap)
            {
                // The flux peak lags the true attack by ~one window, so the
                // transient sits near the window's leading edge. Refine from
                // there to the precise attack start on the raw signal.
                const int approx   = i * hop + win;
                const int refined  = refineAttack (buffer, approx, sampleRate, numSamples, numCh);
                if (ons.empty() || refined > ons.back() + minSpacing)
                    ons.push_back (refined);
                lastOnset = i;
            }
        }

        if (ons.empty())
        {
            starts.push_back (0);
            return starts;
        }

        // Only keep a slice starting at 0 if the region before the first
        // transient holds real content. A silent lead-in is dropped, so the
        // first slice begins ON the first hit (not a dead micro-slice), and the
        // second slice lands on the next transient.
        auto rangeRms = [&] (int a, int b) -> double
        {
            a = juce::jlimit (0, numSamples, a); b = juce::jlimit (0, numSamples, b);
            if (b <= a) return 0.0;
            double e = 0.0;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* d = buffer.getReadPointer (ch);
                for (int s = a; s < b; ++s) e += (double) d[s] * (double) d[s];
            }
            return std::sqrt (e / juce::jmax (1, (b - a) * numCh));
        };

        const int    firstOn = ons.front();
        const double overall = rangeRms (0, numSamples);
        if (firstOn > 0 && rangeRms (0, firstOn) > overall * 0.30)
            starts.push_back (0);                   // genuine intro before the first hit

        for (int o : ons) starts.push_back (o);
        if (starts.empty()) starts.push_back (0);
        return starts;
    }

    // Land a cut exactly on a transient's attack: find the local peak near
    // `approx`, then walk back to where the smoothed envelope first rises above
    // ~20% of that peak (the attack start), bounded so it can't drift into the
    // preceding gap. A small pre-roll keeps the very onset from being clipped.
    static int refineAttack (const juce::AudioBuffer<float>& buffer, int approx,
                             double sampleRate, int n, int numCh)
    {
        const int back = juce::jmax (1, (int) (0.020 * sampleRate));   // search 20 ms before
        const int fwd  = juce::jmax (1, (int) (0.010 * sampleRate));   //    and 10 ms after
        const int lo   = juce::jlimit (0, n - 1, approx - back);
        const int hi   = juce::jlimit (0, n - 1, approx + fwd);
        const int len  = hi - lo + 1;
        if (len <= 2) return juce::jlimit (0, n - 1, approx);

        // smoothed rectified envelope over the local range
        const int r = 16;
        std::vector<float> env ((size_t) len, 0.f);
        for (int idx = 0; idx < len; ++idx)
        {
            double s = 0.0; int c = 0;
            for (int k = idx - r; k <= idx + r; ++k)
            {
                const int g = lo + k;
                if (g >= 0 && g < n)
                {
                    float m = 0.f;
                    for (int ch = 0; ch < numCh; ++ch) m += buffer.getSample (ch, g);
                    s += std::abs (m); ++c;
                }
            }
            env[(size_t) idx] = c > 0 ? (float) (s / c) : 0.f;
        }

        int   peakI = 0;
        float peak  = 0.f;
        for (int idx = 0; idx < len; ++idx)
            if (env[(size_t) idx] > peak) { peak = env[(size_t) idx]; peakI = idx; }
        if (peak <= 0.f) return juce::jlimit (0, n - 1, approx);

        const float trig = peak * 0.20f;
        int a = peakI;
        while (a > 0 && env[(size_t) a] > trig) --a;

        const int preRoll = (int) (0.0015 * sampleRate);               // ~1.5 ms safety
        return juce::jlimit (0, n - 1, lo + a - preRoll);
    }

    //==========================================================================
    // Per-slice "lowness" score: low-band (~200 Hz) energy / total energy.
    // Kick/bass-like slices score high, hats/noise low — used to place bassy
    // slices on strong beats in the musically-aware shuffle.
    static std::vector<float> computeSliceScores (const juce::AudioBuffer<float>& buffer,
                                                  const std::vector<int>& starts, double sampleRate)
    {
        std::vector<float> scores;
        const int n   = buffer.getNumSamples();
        const int nCh = juce::jmax (1, buffer.getNumChannels());
        if (n <= 0 || starts.empty()) return scores;

        const float a = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi
                                        * 200.f / (float) juce::jmax (1.0, sampleRate));
        scores.reserve (starts.size());
        for (size_t i = 0; i < starts.size(); ++i)
        {
            const int s0 = juce::jlimit (0, n, starts[i]);
            const int s1 = juce::jlimit (0, n, (i + 1 < starts.size()) ? starts[i + 1] : n);
            double lowE = 0.0, totE = 0.0; float lp = 0.f;
            for (int s = s0; s < s1; ++s)
            {
                float m = 0.f;
                for (int c = 0; c < nCh; ++c) m += buffer.getSample (c, s);
                m /= (float) nCh;
                lp  += a * (m - lp);
                lowE += (double) lp * lp;
                totE += (double) m * m;
            }
            scores.push_back (totE > 1.0e-9 ? juce::jlimit (0.f, 1.f, (float) (lowE / totE)) : 0.5f);
        }
        return scores;
    }

    //==========================================================================
    // Grid slicing — evenly divides [0, numSamples) by a musical division.
    static std::vector<int> gridSlice (int numSamples, double sampleRate,
                                       double bpm, int divisionIndex)
    {
        std::vector<int> starts;
        if (numSamples <= 0 || sampleRate <= 0.0 || bpm <= 0.0)
            return starts;

        const double beats     = beatsForDivision (divisionIndex);
        const double sliceLen  = (60.0 / bpm) * beats * sampleRate;
        if (sliceLen < 1.0)
        {
            starts.push_back (0);
            return starts;
        }

        for (double p = 0.0; p < (double) numSamples - 0.5; p += sliceLen)
            starts.push_back ((int) std::lround (p));

        if (starts.empty())
            starts.push_back (0);

        return starts;
    }

    //==========================================================================
    // Estimate BPM from a loop assumed to be `bars` bars of 4/4. This is the
    // reliable, user-overridable path (vs. fragile blind auto-detect).
    static double bpmFromLength (int numSamples, double sampleRate, int bars)
    {
        if (numSamples <= 0 || sampleRate <= 0.0 || bars <= 0)
            return 120.0;

        const double seconds = (double) numSamples / sampleRate;
        const double beats   = 4.0 * (double) bars;          // 4/4
        return (beats / seconds) * 60.0;
    }
};
