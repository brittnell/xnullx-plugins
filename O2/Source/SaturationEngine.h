#pragma once
#include <JuceHeader.h>
#include <cmath>

//==============================================================================
// Curve and variant name tables
//==============================================================================
static const juce::StringArray o2TypeNames {
    "Diode", "Op-Amp", "Transformer", "Tape", "Tube"
};

static const juce::String o2VariantNames[5][4] = {
    { "Germanium Sunshine", "Silicon Grit",    "Schottky Velvet",  "Fuzz Therapy"    },
    { "741 Sweat",          "Double Espresso", "Discrete Biscuit", "Slew Daemon"     },
    { "Yorkshire Wool",     "Jensen's Creek",  "Sowter Power",     "Iron Curtain"    },
    { "Ferric Daydream",    "Chrome Holiday",  "Fahrenheit 456",   "Studer Weather"  },
    { "Triode Sunrise",     "Octal Molasses",  "Pentode Circus",   "Rectifier Dreams"},
};

//==============================================================================
// Curve parameters
//
// The waveshaper is a NORMALISED tanh drive:  y = tanh(x·s) / tanh(s), with an
// independent bend per polarity for even-harmonic asymmetry. Because it maps
// full-scale -> full-scale, raising drive adds harmonics and RMS (the signal
// gets fuller/louder) WITHOUT crushing peaks or dropping level — so the effect
// is plainly audible instead of vanishing under gain compensation.
//==============================================================================
struct O2CurveParams
{
    float driveAmt;    // tanh "shape" reached at drive = 10 (amount + hardness + density)
    float asymmetry;   // 0 = symmetric (odd harmonics), up to ~0.8 = strong even harmonics
    float preFreq;     // pre/de-emphasis shelf freq, Hz (0 = off)
    float preGainDb;   // shelf gain (+dB low boost for transformer, −dB HF cut for tape)
    bool  hysteresis;  // tape/transformer memory effect
    bool  slewDamp;    // op-amp slew-rate rounding
};

static const O2CurveParams o2CurveTable[5][4] =
{
    // ── DIODE ────────────────────────────────────────────────────────────────
    { {  7.0f, 0.45f,   0.f,  0.f, false, false },   // Germanium Sunshine — warm, even-rich
      { 10.0f, 0.25f,   0.f,  0.f, false, false },   // Silicon Grit       — harder, gritty
      {  4.5f, 0.20f,   0.f,  0.f, false, false },   // Schottky Velvet    — smooth, gentle
      { 14.0f, 0.55f,   0.f,  0.f, false, false } }, // Fuzz Therapy       — extreme

    // ── OP-AMP ─────────────────────────────────────────────────────────────────
    { {  8.0f, 0.08f,   0.f,  0.f, false, false },   // 741 Sweat          — clean then clips
      {  9.0f, 0.12f,   0.f,  0.f, false, false },   // Double Espresso    — fuller
      {  8.5f, 0.20f,   0.f,  0.f, false, false },   // Discrete Biscuit
      {  6.5f, 0.06f,   0.f,  0.f, false, true  } }, // Slew Daemon        — rounded highs

    // ── TRANSFORMER (low-end emphasis + iron) ──────────────────────────────────
    { {  5.5f, 0.35f, 200.f,  4.f, true,  false },   // Yorkshire Wool
      {  4.5f, 0.25f, 150.f,  2.f, false, false },   // Jensen's Creek
      {  7.0f, 0.40f, 120.f,  6.f, true,  false },   // Sowter Power
      {  8.5f, 0.50f, 100.f,  8.f, false, false } }, // Iron Curtain       — heavy

    // ── TAPE (HF roll-off + hysteresis) ─────────────────────────────────────────
    { {  5.5f, 0.20f, 3000.f,-3.f, true,  false },   // Ferric Daydream
      {  6.5f, 0.15f, 5000.f,-2.f, true,  false },   // Chrome Holiday
      {  8.0f, 0.30f, 2000.f,-4.f, true,  false },   // Fahrenheit 456     — driven tape
      {  4.5f, 0.10f, 4000.f,-2.f, true,  false } }, // Studer Weather     — clean

    // ── TUBE (asymmetric, even-harmonic warmth) ────────────────────────────────
    { {  7.0f, 0.55f,   0.f,  0.f, false, false },   // Triode Sunrise
      {  6.0f, 0.70f,   0.f,  0.f, false, false },   // Octal Molasses     — fat, even-rich
      {  9.0f, 0.40f,   0.f,  0.f, false, false },   // Pentode Circus
      {  8.0f, 0.60f,   0.f,  0.f, false, false } }, // Rectifier Dreams
};

//==============================================================================
// Per-band saturation engine (stereo, stateful)
//==============================================================================
class SaturationEngine
{
public:
    void prepare(double sampleRate)
    {
        mSR = sampleRate;
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            mPreState [ch] = 0.f;
            mPostState[ch] = 0.f;
            mHystPrev [ch] = 0.f;
            mSlewState[ch] = 0.f;
        }
        mCachedS = -1.f;   // force recompute of the per-block tanh coefficients
        mCachedA = -1.f;
    }

    // Process a single sample.  drive is 0–10, typeIdx 0–4, varIdx 0–3.
    float process(float x, float drive, int typeIdx, int varIdx, int ch)
    {
        const O2CurveParams& p = o2CurveTable[typeIdx][varIdx];

        // ── Pre-emphasis ────────────────────────────────────────────────────
        if (p.preFreq > 0.f)
            x = shelf(x, p.preFreq, p.preGainDb, mPreState[ch]);

        // ── Tape/transformer hysteresis (direction-dependent bias) ───────────
        if (p.hysteresis)
        {
            float delta   = x - mHystPrev[ch];
            mHystPrev[ch] = x;
            x += delta * 0.08f;
        }

        // ── Normalised asymmetric tanh drive ─────────────────────────────────
        const float s = (drive * 0.1f) * p.driveAmt;
        float y;
        if (s < 1.0e-3f)
        {
            y = x;                                   // drive 0 → perfectly clean
        }
        else
        {
            // tanh(s) coefficients are constant across a block — cache them and
            // only recompute when drive or variant changes.
            if (s != mCachedS || p.asymmetry != mCachedA)
            {
                mCachedS = s;
                mCachedA = p.asymmetry;
                mSPos    = s;
                mSNeg    = s * (1.f - p.asymmetry * 0.45f);   // negative side bends less → even harmonics
                mInvTPos = 1.f / std::tanh(mSPos);
                mInvTNeg = 1.f / std::tanh(mSNeg);
            }

            y = (x >= 0.f) ? std::tanh(x * mSPos) * mInvTPos
                           : std::tanh(x * mSNeg) * mInvTNeg;
        }

        // ── De-emphasis (inverse shelf) ──────────────────────────────────────
        if (p.preFreq > 0.f)
            y = shelf(y, p.preFreq, -p.preGainDb, mPostState[ch]);

        // ── Slew Daemon output smoothing ─────────────────────────────────────
        if (p.slewDamp)
            y = slewFilter(y, ch);

        return y;
    }

private:
    double mSR = 44100.0;

    float mPreState [2] = {};
    float mPostState[2] = {};
    float mHystPrev [2] = {};
    float mSlewState[2] = {};

    // cached per-block tanh drive coefficients
    float mCachedS = -1.f, mCachedA = -1.f;
    float mSPos = 1.f, mSNeg = 1.f, mInvTPos = 1.f, mInvTNeg = 1.f;

    //──────────────────────────────────────────────────────────────────────────
    // First-order TPT low-shelf: gainDb > 0 boosts below freq, < 0 cuts above
    float shelf(float x, float freq, float gainDb, float& s)
    {
        float fc  = juce::jlimit(10.f, (float)(mSR * 0.45), freq);
        float g   = std::tan(juce::MathConstants<float>::pi * fc / (float)mSR);
        float A   = std::pow(10.f, gainDb / 40.f);

        float gk  = (gainDb >= 0.f) ? g * A : g / A;
        float v   = (x - s) * gk / (1.f + gk);
        float lp  = v + s;
        s         = lp + v;

        if (gainDb >= 0.f)
            return x + lp * (A * A - 1.f);          // low shelf boost
        else
            return x - lp * (1.f - 1.f / (A * A));  // high shelf cut
    }

    //──────────────────────────────────────────────────────────────────────────
    // Gentle output lowpass for Slew Daemon — blended with dry
    float slewFilter(float x, int ch)
    {
        float g = std::tan(juce::MathConstants<float>::pi * 7000.f / (float)mSR);
        float a = g / (1.f + g);
        float v = a * (x - mSlewState[ch]);
        float y = v + mSlewState[ch];
        mSlewState[ch] = y + v;
        return y * 0.65f + x * 0.35f;
    }
};
