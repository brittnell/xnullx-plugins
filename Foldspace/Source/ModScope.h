#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FoldspaceLookAndFeel.h"

//==============================================================================
// Oscilloscope of the (post-fold) modulator signal — the waveform actually
// driving the PM stage. The processor publishes ~base-rate samples into a
// lock-free ring; this just draws the most recent window.
//==============================================================================
class ModScope : public juce::Component
{
public:
    explicit ModScope(FoldspaceAudioProcessor& p) : proc(p)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(fsBgPanel);
        g.fillRoundedRectangle(b, 4.f);

        auto area = b.reduced(8.f);

        g.setColour(juce::Colour(0xFF262626));
        g.drawLine(area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.f);

        const int N = FoldspaceAudioProcessor::scopeSize;
        const int w = proc.scopeWrite.load(std::memory_order_acquire);

        juce::Path wave;
        for (int i = 0; i < N; ++i)
        {
            const float v  = juce::jlimit(-1.f, 1.f,
                                proc.scopeBuf[(size_t)((w + i) % N)]);
            const float px = area.getX() + area.getWidth() * (float)i / (float)(N - 1);
            const float py = area.getCentreY() - v * area.getHeight() * 0.48f;
            if (i == 0) wave.startNewSubPath(px, py);
            else        wave.lineTo(px, py);
        }
        g.setColour(fsViolet);
        g.strokePath(wave, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved));

        g.setColour(fsDim);
        g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
        g.drawText("MODULATOR", getLocalBounds().reduced(8, 4),
                   juce::Justification::topLeft, false);
    }

private:
    FoldspaceAudioProcessor& proc;
};
