#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>

// 4-point Catmull-Rom interpolation read (shared by voices + preview player).
static inline float bunkaCubic (const juce::AudioBuffer<float>& buf, int ch, double pos, int n) noexcept
{
    const int    i1   = (int) std::floor (pos);
    const float  frac = (float) (pos - i1);
    auto get = [&] (int idx) -> float { return buf.getSample (ch, juce::jlimit (0, n - 1, idx)); };
    const float xm1 = get (i1 - 1), x0 = get (i1), x1 = get (i1 + 1), x2 = get (i1 + 2);
    const float a = -0.5f * xm1 + 1.5f * x0 - 1.5f * x1 + 0.5f * x2;
    const float b =        xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c = -0.5f * xm1            + 0.5f * x1;
    return ((a * frac + b) * frac + c) * frac + x0;
}

//==============================================================================
// Shared sample payload. Reference-counted so the audio thread can keep a
// slice playing from old data while the message thread swaps in new data.
//==============================================================================
struct BunkaSampleData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<BunkaSampleData>;

    juce::AudioBuffer<float> buffer;        // file's native channels
    double            fileSampleRate = 44100.0;
    std::vector<int>  sliceStarts;          // sorted; [0] == 0
    std::vector<float> sliceScores;         // per-slice low-band energy ratio (kick-ish -> high)
    double            loopBpm = 120.0;
    int               numBars = 1;

    int numSlices() const noexcept { return (int) sliceStarts.size(); }

    int sliceStart (int i) const noexcept
    {
        if (sliceStarts.empty()) return 0;
        return sliceStarts[(size_t) juce::jlimit (0, numSlices() - 1, i)];
    }

    int sliceEnd (int i) const noexcept
    {
        if (i + 1 < numSlices()) return sliceStarts[(size_t) (i + 1)];
        return buffer.getNumSamples();
    }
};

//==============================================================================
// Time-stretch configuration shared by all voices (built in prepare()).
//==============================================================================
struct StretchConfig
{
    int grainLen = 1024;     // output samples per grain
    int synthHop = 512;      // fixed output hop between grains (= grainLen/2)
    std::vector<float> window;   // periodic Hann, size grainLen (COLA at 50% overlap)

    void prepare (double sampleRate)
    {
        grainLen = juce::jmax (256, (int) std::lround (0.030 * sampleRate)); // ~30 ms
        grainLen &= ~1;                                  // make even
        synthHop = grainLen / 2;
        window.resize ((size_t) grainLen);
        for (int j = 0; j < grainLen; ++j)
            window[(size_t) j] = 0.5f * (1.f - std::cos (juce::MathConstants<float>::twoPi
                                          * ((float) j + 0.5f) / (float) grainLen));
    }
};

//==============================================================================
// One playing slice. Two render paths:
//   Direct  — cubic-interpolated resample (REPITCH / classic MPC, pitch tracks tempo)
//   Stretch — overlap-add time-stretch (PITCH PRESERVE: tempo-matched, pitch held,
//             pitch independently shiftable). One-shot, drum-style envelope.
//==============================================================================
class SliceVoice
{
public:
    enum class Mode { Direct, Stretch };

    bool isActive() const noexcept { return active; }
    juce::int64 age()  const noexcept { return startStamp; }

    // Pre-allocate OLA buffers off the audio thread (called from prepare()).
    void allocate (int grainLen)
    {
        olaL.assign ((size_t) grainLen, 0.f);
        olaR.assign ((size_t) grainLen, 0.f);
        olaSize = grainLen;
    }

    //-- REPITCH / native resample --------------------------------------------
    void startDirect (BunkaSampleData::Ptr d, int sliceIdx, double rate, float velGain,
                      bool reverse, double attackSamps, double releaseSamps, int offset, juce::int64 stamp)
    {
        commonStart (d, sliceIdx, velGain, reverse, attackSamps, releaseSamps, offset, stamp);
        mode     = Mode::Direct;
        playRate = juce::jmax (1.0e-6, rate);
        pos      = rev ? (double) (sliceEndIdx - 1) : (double) sliceStartIdx;
    }

    //-- PITCH PRESERVE / overlap-add time-stretch ----------------------------
    //   baseRate = fileSR/deviceSR ; pitchFactor = 2^(semi/12) ; tempoRatio = host/loop
    void startStretch (BunkaSampleData::Ptr d, int sliceIdx, const StretchConfig* config,
                       double baseRate, double pitchFactor, double tempoRatio, float velGain,
                       bool reverse, double attackSamps, double releaseSamps, int offset, juce::int64 stamp,
                       double attackPreserveSrc)
    {
        commonStart (d, sliceIdx, velGain, reverse, attackSamps, releaseSamps, offset, stamp);
        mode = Mode::Stretch;
        cfg  = config;

        rRate = baseRate * pitchFactor;                              // intra-grain read rate (pitch)
        vLen  = (double) (sliceEndIdx - sliceStartIdx);              // virtual slice length (src samples)
        totalOut = vLen / juce::jmax (1.0e-9, baseRate * tempoRatio);// expected output length (samples)

        // Transient preservation: play the attack region near-native (crisp),
        // then stretch the body harder to make up the time. One OLA, two onset
        // rates selected by read position — no phase switch, no seam.
        haAttack = (double) cfg->synthHop * baseRate;                // stretch ~1 over the attack
        const double attackOut  = attackPreserveSrc / juce::jmax (1.0e-9, baseRate);
        const double bodyOut    = totalOut - attackOut;
        const double bodyLenSrc = vLen - attackPreserveSrc;
        if (reverse || attackPreserveSrc <= 0.0 || bodyLenSrc <= 0.0 || bodyOut < (double) cfg->synthHop)
        {
            attackSrc = 0.0;                                         // disable — uniform stretch
            haBody    = (double) cfg->synthHop * baseRate * tempoRatio;
        }
        else
        {
            attackSrc = attackPreserveSrc;
            haBody    = (double) cfg->synthHop * bodyLenSrc / bodyOut;
        }

        vStart      = 0.0;
        outCount    = 0.0;
        olaRead     = 0;
        countdown   = 0;
        spawning    = true;
        firstGrain  = true;
        std::fill (olaL.begin(), olaL.end(), 0.f);
        std::fill (olaR.begin(), olaR.end(), 0.f);
    }

    void stop() noexcept { active = false; data = nullptr; }

    void render (float& l, float& r) noexcept
    {
        if (! active) return;
        if (startDelay > 0) { --startDelay; return; }   // sample-accurate trigger offset
        if (mode == Mode::Direct) renderDirect (l, r);
        else                      renderStretch (l, r);
    }

private:
    void commonStart (BunkaSampleData::Ptr d, int sliceIdx, float velGain, bool reverse,
                      double attackSamps, double releaseSamps, int offset, juce::int64 stamp)
    {
        data         = d;
        gain         = velGain;
        rev          = reverse;
        atkSamps     = juce::jmax (1.0, attackSamps);
        relSamps     = juce::jmax (1.0, releaseSamps);
        startDelay   = juce::jmax (0, offset);
        startStamp   = stamp;
        atkEnv       = 0.0;
        sliceStartIdx = d->sliceStart (sliceIdx);
        sliceEndIdx   = d->sliceEnd   (sliceIdx);
        active       = (sliceEndIdx > sliceStartIdx);
    }

    //-- Direct (resample) ----------------------------------------------------
    void renderDirect (float& l, float& r) noexcept
    {
        const bool past = rev ? (pos <= (double) sliceStartIdx) : (pos >= (double) sliceEndIdx);
        if (past) { stop(); return; }

        const auto& buf = data->buffer;
        const int n   = buf.getNumSamples();
        const int nCh = buf.getNumChannels();

        const float sl = readCubic (buf, 0, pos, n);
        const float sr = nCh > 1 ? readCubic (buf, 1, pos, n) : sl;

        if (atkEnv < 1.0) { atkEnv += 1.0 / atkSamps; if (atkEnv > 1.0) atkEnv = 1.0; }
        const double distToEnd = rev ? (pos - (double) sliceStartIdx) : ((double) sliceEndIdx - pos);
        const double outRemain = distToEnd / playRate;
        const double relEnv    = outRemain <= relSamps ? juce::jlimit (0.0, 1.0, outRemain / relSamps) : 1.0;
        const float  env       = (float) (atkEnv * relEnv) * gain;

        l += sl * env;
        r += sr * env;
        pos += rev ? -playRate : playRate;
    }

    //-- Stretch (overlap-add) ------------------------------------------------
    void renderStretch (float& l, float& r) noexcept
    {
        if (countdown <= 0)
        {
            if (spawning && vStart < vLen) spawnGrain();
            else                           spawning = false;
            countdown += cfg->synthHop;
        }

        float outL = olaL[(size_t) olaRead];
        float outR = olaR[(size_t) olaRead];
        olaL[(size_t) olaRead] = 0.f;
        olaR[(size_t) olaRead] = 0.f;
        olaRead = (olaRead + 1) % olaSize;
        --countdown;

        if (atkEnv < 1.0) { atkEnv += 1.0 / atkSamps; if (atkEnv > 1.0) atkEnv = 1.0; }
        const double remain = totalOut - outCount;
        const double relEnv = remain <= relSamps ? juce::jlimit (0.0, 1.0, remain / relSamps) : 1.0;
        const float  env    = (float) (atkEnv * relEnv) * gain;

        l += outL * env;
        r += outR * env;

        if (++outCount >= totalOut) stop();
    }

    void spawnGrain() noexcept
    {
        const auto& buf = data->buffer;
        const int n   = buf.getNumSamples();
        const int nCh = buf.getNumChannels();
        const int Lg  = cfg->grainLen;
        const int hop = cfg->synthHop;

        for (int j = 0; j < Lg; ++j)
        {
            const double vpos = vStart + (double) j * rRate;
            if (vpos < 0.0 || vpos >= vLen) continue;

            // map virtual position into the source (handles reverse)
            const double actual = rev ? ((double) sliceEndIdx - 1.0 - vpos)
                                      : ((double) sliceStartIdx + vpos);

            // flatten the first grain's left half so the slice attack isn't ramped
            const float w = (firstGrain && j <= hop) ? 1.f : cfg->window[(size_t) j];

            const int idx = (olaRead + j) % olaSize;
            olaL[(size_t) idx] += w * readCubic (buf, 0, actual, n);
            olaR[(size_t) idx] += w * (nCh > 1 ? readCubic (buf, 1, actual, n) : readCubic (buf, 0, actual, n));
        }

        vStart += (vStart < attackSrc) ? haAttack : haBody;   // near-native over the attack, stretched body
        firstGrain = false;
    }

    static float readCubic (const juce::AudioBuffer<float>& buf, int ch, double pos, int n) noexcept
    {
        const int    i1   = (int) std::floor (pos);
        const float  frac = (float) (pos - i1);
        auto get = [&] (int idx) -> float { return buf.getSample (ch, juce::jlimit (0, n - 1, idx)); };
        const float xm1 = get (i1 - 1), x0 = get (i1), x1 = get (i1 + 1), x2 = get (i1 + 2);
        const float a = -0.5f * xm1 + 1.5f * x0 - 1.5f * x1 + 0.5f * x2;
        const float b =        xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c = -0.5f * xm1            + 0.5f * x1;
        return ((a * frac + b) * frac + c) * frac + x0;
    }

    // shared
    BunkaSampleData::Ptr data;
    Mode   mode = Mode::Direct;
    bool   active = false, rev = false;
    int    sliceStartIdx = 0, sliceEndIdx = 0, startDelay = 0;
    float  gain = 1.f;
    double atkSamps = 1.0, relSamps = 1.0, atkEnv = 0.0;
    juce::int64 startStamp = 0;

    // direct
    double pos = 0.0, playRate = 1.0;

    // stretch
    const StretchConfig* cfg = nullptr;
    std::vector<float> olaL, olaR;
    int    olaSize = 0, olaRead = 0, countdown = 0;
    double vStart = 0.0, vLen = 0.0, rRate = 1.0, outCount = 0.0, totalOut = 0.0;
    double haAttack = 1.0, haBody = 1.0, attackSrc = 0.0;
    bool   spawning = false, firstGrain = true;
};

//==============================================================================
// Fixed voice pool. Triggers slices, mixes voices into the output buffer.
//==============================================================================
class BunkaSampler
{
public:
    static constexpr int kNumVoices = 16;

    void prepare (double deviceSr) noexcept
    {
        deviceSampleRate = juce::jmax (1.0, deviceSr);
        stretchCfg.prepare (deviceSampleRate);
        for (auto& v : voices) { v.stop(); v.allocate (stretchCfg.grainLen); }
    }

    void setSampleData (BunkaSampleData::Ptr d) noexcept
    {
        data = d;
        previewActive = false;
        for (auto& v : voices) v.stop();   // clean cut on (re)load
    }

    // One-shot playback of the whole loaded file, unaltered (native pitch/speed,
    // no slicing/stretch) — for A/B against the sliced/processed result.
    void triggerPreview() noexcept
    {
        if (data == nullptr) return;
        previewRate   = data->fileSampleRate / deviceSampleRate;
        previewPos    = 0.0;
        previewActive = true;
    }

    // tempoRatio = hostBpm / loopBpm (1.0 when host-sync is off)
    void triggerSlice (int sliceIdx, float velGain, double tempoRatio,
                       bool pitchPreserve, double pitchSemis, bool reverse,
                       double attackMs, double releaseMs, int startOffset = 0)
    {
        if (data == nullptr || sliceIdx < 0 || sliceIdx >= data->numSlices()) return;

        const double baseRate    = data->fileSampleRate / deviceSampleRate;
        const double pitchFactor = std::pow (2.0, pitchSemis / 12.0);
        const double atkS = attackMs  * 0.001 * deviceSampleRate;
        const double relS = releaseMs * 0.001 * deviceSampleRate;

        const double attackPreserveSrc = 0.012 * data->fileSampleRate;   // ~12 ms attack kept crisp

        auto& v = allocateVoice();
        if (pitchPreserve)
            v.startStretch (data, sliceIdx, &stretchCfg, baseRate, pitchFactor, tempoRatio,
                            velGain, reverse, atkS, relS, startOffset, ++stamp, attackPreserveSrc);
        else
            v.startDirect (data, sliceIdx, baseRate * tempoRatio * pitchFactor,
                           velGain, reverse, atkS, relS, startOffset, ++stamp);
    }

    void renderBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) noexcept
    {
        const int numCh = out.getNumChannels();
        if (numCh <= 0) return;

        const int    pn      = (previewActive && data != nullptr) ? data->buffer.getNumSamples() : 0;
        const int    pCh     = pn > 0 ? data->buffer.getNumChannels() : 0;
        const double fadeLen = juce::jmax (1.0, 0.003 * deviceSampleRate);   // 3 ms edge fades

        for (int i = 0; i < numSamples; ++i)
        {
            float sumL = 0.f, sumR = 0.f;
            for (auto& v : voices) v.render (sumL, sumR);

            if (previewActive)
            {
                if (previewPos >= pn) previewActive = false;
                else
                {
                    const float pl = bunkaCubic (data->buffer, 0, previewPos, pn);
                    const float pr = pCh > 1 ? bunkaCubic (data->buffer, 1, previewPos, pn) : pl;
                    float fade = 1.f;
                    if (previewPos < fadeLen)            fade = (float) (previewPos / fadeLen);
                    else if (pn - previewPos < fadeLen)  fade = (float) ((pn - previewPos) / fadeLen);
                    sumL += pl * fade;
                    sumR += pr * fade;
                    previewPos += previewRate;
                }
            }

            const int idx = startSample + i;
            out.addSample (0, idx, sumL);
            if (numCh > 1) out.addSample (1, idx, sumR);
        }
    }

    void allNotesOff() noexcept { for (auto& v : voices) v.stop(); }

private:
    SliceVoice& allocateVoice() noexcept
    {
        for (auto& v : voices) if (! v.isActive()) return v;
        SliceVoice* oldest = &voices[0];
        for (auto& v : voices) if (v.age() < oldest->age()) oldest = &v;
        return *oldest;
    }

    SliceVoice voices[kNumVoices];
    BunkaSampleData::Ptr data;
    StretchConfig stretchCfg;
    double deviceSampleRate = 44100.0;
    juce::int64 stamp = 0;

    // preview / audition player
    bool   previewActive = false;
    double previewPos    = 0.0;
    double previewRate   = 1.0;
};
