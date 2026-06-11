#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FoldspaceLookAndFeel.h"
#include "WaveFolder.h"

//==============================================================================
// Live transfer-curve display of the MAIN fold stage: in (-1..1) -> out.
// Reflects FOLDS drive, SHAPE morph, SYMMETRY bias and the live ENV->FOLD
// contribution, so the curve breathes with the programme material.
// Repainted by the editor's timer.
//==============================================================================
class TransferDisplay : public juce::Component
{
public:
    explicit TransferDisplay(FoldspaceAudioProcessor& p) : proc(p)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(fsBgPanel);
        g.fillRoundedRectangle(b, 4.f);

        auto area = b.reduced(8.f);

        // grid
        g.setColour(juce::Colour(0xFF262626));
        g.drawLine(area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.f);
        g.drawLine(area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom(), 1.f);

        // unity reference
        g.setColour(juce::Colour(0xFF333333));
        g.drawLine(area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.f);

        // effective fold parameters (env send applied, same math as the DSP)
        auto P = [this](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
        const float env      = proc.uiEnv.load();
        const float foldsEff = juce::jlimit(0.f, 1.f, P("folds") + P("envToFold") * env);
        const float driveG   = 1.f + foldsEff * 15.f;
        const float shape    = P("shape");
        const float bias     = P("symmetry") * 0.8f;

        auto makeCurve = [&](float x0, float x1, int steps)
        {
            juce::Path path;
            for (int i = 0; i <= steps; ++i)
            {
                const float x = x0 + (x1 - x0) * (float)i / (float)steps;
                const float y = juce::jlimit(-1.f, 1.f,
                                    WaveFolder::foldMorph(x * driveG + bias, shape));
                const float px = area.getX() + (x * 0.5f + 0.5f) * area.getWidth();
                const float py = area.getCentreY() - y * area.getHeight() * 0.5f;
                if (i == 0) path.startNewSubPath(px, py);
                else        path.lineTo(px, py);
            }
            return path;
        };

        // full curve, dimmed — the math plot
        g.setColour(fsSpice.withAlpha(0.30f));
        g.strokePath(makeCurve(-1.f, 1.f, 192),
                     juce::PathStrokeType(1.4f, juce::PathStrokeType::curved));

        // live drive region — the part of the curve the input is actually
        // traversing right now (input envelope = fold-stage input amplitude)
        const float xLim = juce::jlimit(0.02f, 1.f, env);
        g.setColour(fsSpice);
        g.strokePath(makeCurve(-xLim, xLim, 128),
                     juce::PathStrokeType(2.f, juce::PathStrokeType::curved));

        g.setColour(fsDim);
        g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
        g.drawText("TRANSFER", getLocalBounds().reduced(8, 4),
                   juce::Justification::topLeft, false);
        g.drawText(juce::String(driveG, 1) + "x", getLocalBounds().reduced(8, 4),
                   juce::Justification::bottomRight, false);
    }

private:
    FoldspaceAudioProcessor& proc;
};
