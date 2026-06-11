#pragma once
#include <JuceHeader.h>
#include <cmath>

//==============================================================================
// Morphable wavefolder with first-order antiderivative antialiasing (ADAA).
//
// Two fold characters, crossfadable:
//   triangle fold — reflect-fold into [-1,1] (Buchla 259 family: hard, buzzy)
//   sine fold     — sin(pi/2 * x)            (Serge multiplier family: round)
//
// Drive and symmetry bias are applied by the CALLER (u = x*g + bias) so the
// ADAA difference operates on the actual fold input sequence — this keeps the
// antialiasing valid while drive/bias move per-sample.
//
// ADAA1: y[n] = (F(u[n]) - F(u[n-1])) / (u[n] - u[n-1]),
// falling back to f(midpoint) when the difference is ill-conditioned.
//==============================================================================
struct WaveFolder
{
    // direct (non-antialiased) shapes — also used by the UI transfer display
    static float foldTri(float x) noexcept
    {
        float v = (x + 1.f) * 0.25f;
        v = (v - std::floor(v)) * 4.f;               // 0..4, slope 1 in x
        return v < 2.f ? v - 1.f : 3.f - v;
    }

    static float foldTriAD(float x) noexcept          // antiderivative of foldTri
    {
        float v = (x + 1.f) * 0.25f;
        v = (v - std::floor(v)) * 4.f;
        return v < 2.f ? 0.5f * v * v - v
                       : 3.f * v - 0.5f * v * v - 4.f;
    }

    static float foldSin(float x) noexcept
    {
        return std::sin(juce::MathConstants<float>::halfPi * x);
    }

    static float foldSinAD(float x) noexcept          // antiderivative of foldSin
    {
        const float k = juce::MathConstants<float>::halfPi;
        return -std::cos(k * x) / k;
    }

    static float foldMorph(float x, float shape) noexcept
    {
        return shape <= 0.f ? foldTri(x)
             : shape >= 1.f ? foldSin(x)
             : foldTri(x) + shape * (foldSin(x) - foldTri(x));
    }

    //==========================================================================
    void reset() noexcept
    {
        uPrev = 0.f;
        fTriPrev = foldTriAD(0.f);
        fSinPrev = foldSinAD(0.f);
    }

    // triangle-only path (modulator / feedback folders — fixed character)
    float processTri(float u) noexcept
    {
        const float du = u - uPrev;
        const float fT = foldTriAD(u);
        float y;
        if (std::abs(du) < 1.0e-4f) y = foldTri(0.5f * (u + uPrev));
        else                        y = (fT - fTriPrev) / du;
        uPrev = u; fTriPrev = fT;
        return y;
    }

    // morphable path (main folder)
    float processMorph(float u, float shape) noexcept
    {
        const float du = u - uPrev;
        const float fT = foldTriAD(u);
        const float fS = foldSinAD(u);
        float yT, yS;
        if (std::abs(du) < 1.0e-4f)
        {
            const float mid = 0.5f * (u + uPrev);
            yT = foldTri(mid); yS = foldSin(mid);
        }
        else
        {
            yT = (fT - fTriPrev) / du;
            yS = (fS - fSinPrev) / du;
        }
        uPrev = u; fTriPrev = fT; fSinPrev = fS;
        return yT + shape * (yS - yT);
    }

private:
    float uPrev = 0.f;
    float fTriPrev = 0.f;
    float fSinPrev = 0.f;
};
