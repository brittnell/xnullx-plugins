#pragma once
#include <JuceHeader.h>
#include "GrainBrainLookAndFeel.h"

class GrainBrainAudioProcessor;

class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    SpectrumAnalyzer(GrainBrainAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
        : forwardFFT(fftOrder),
        window(fftSize, juce::dsp::WindowingFunction<float>::hann),
        processor(p), apvts(apvts)
    {
        for (int i = 0; i < 4; ++i)
        {
            juce::String id = juce::String(i);
            centers[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("bandCenter" + id));
            widths[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("bandWidth" + id));
            enabled[i] = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bandEnabled" + id));
        }
        startTimerHz(60);
    }

    ~SpectrumAnalyzer() override { stopTimer(); }

    void timerCallback() override
    {
        int available = processor.abstractFifo.getNumReady();
        while (available >= fftSize)
        {
            int start1, size1, start2, size2;
            processor.abstractFifo.prepareToRead(fftSize, start1, size1, start2, size2);
            if (size1 > 0)
                juce::FloatVectorOperations::copy(fftData, processor.audioFifo.data() + start1, size1);
            if (size2 > 0)
                juce::FloatVectorOperations::copy(fftData + size1, processor.audioFifo.data() + start2, size2);
            processor.abstractFifo.finishedRead(size1 + size2);

            window.multiplyWithWindowingTable(fftData, fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData);

            auto mindB = -80.f, maxdB = 0.f;
            for (int i = 0; i < scopeSize; ++i)
            {
                auto skewed = 1.f - std::exp(std::log(1.f - (float)i / scopeSize) * 0.2f);
                auto idx = juce::jlimit(0, fftSize / 2, (int)(skewed * fftSize * 0.5f));
                auto level = juce::jmap(juce::jlimit(mindB, maxdB,
                    juce::Decibels::gainToDecibels(fftData[idx]) -
                    juce::Decibels::gainToDecibels((float)fftSize)),
                    mindB, maxdB, 0.f, 1.f);
                scopeData[i] = scopeData[i] * 0.6f + level * 0.4f;
            }
            available = processor.abstractFifo.getNumReady();
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();

        g.setColour(juce::Colour(0xFF050505));
        g.fillRect(bounds);

        // Frequency grid
        g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
        float freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (auto freq : freqs)
        {
            float x = getXForFrequency(freq, w);
            g.setColour(juce::Colour(0xFF1E1E1E));
            g.drawVerticalLine((int)x, 0, h);
            juce::String label = freq < 1000 ? juce::String((int)freq)
                : juce::String((int)(freq / 1000)) + "k";
            g.setColour(juce::Colour(0xFF444444));
            g.drawText(label, (int)x - 12, (int)h - 14, 24, 12, juce::Justification::centred);
        }

        // Band regions
        for (int i = 0; i < 4; ++i)
        {
            if (!centers[i] || !widths[i]) continue;

            bool isEnabled = enabled[i] ? enabled[i]->get() : true;
            // Live, LFO-modulated values drive the animated region; the label shows
            // the stable set center so the readout stays legible while it sweeps.
            float center = processor.liveCenter[i].load(std::memory_order_relaxed);
            float width = processor.liveWidth[i].load(std::memory_order_relaxed);
            float setCenter = centers[i]->get();
            float Q = juce::jlimit(0.5f, 100.f, 50.f / width);

            // Visual band edges using octave bandwidth approximation
            float halfOctaves = 1.0f / Q;
            float fLow = juce::jlimit(20.f, 20000.f, center / std::pow(2.f, halfOctaves));
            float fHigh = juce::jlimit(20.f, 20000.f, center * std::pow(2.f, halfOctaves));

            float xLow = getXForFrequency(fLow, w);
            float xHigh = getXForFrequency(fHigh, w);
            float xCentr = getXForFrequency(center, w);

            // Filled band region
            float alpha = isEnabled ? 0.20f : 0.06f;
            g.setColour(bandColours[i].withAlpha(alpha));
            g.fillRect(xLow, 0.f, xHigh - xLow, h - 16.f);

            // Center line
            g.setColour(isEnabled ? bandColours[i].withAlpha(0.9f)
                : bandColours[i].withAlpha(0.3f));
            g.drawVerticalLine((int)xCentr, 0, h - 16);

            // Triangle drag handle at top
            juce::Path handle;
            handle.addTriangle(xCentr - 6, 0, xCentr + 6, 0, xCentr, 10);
            g.setColour(draggedBand == i ? juce::Colours::white
                : (isEnabled ? bandColours[i] : bandColours[i].withAlpha(0.4f)));
            g.fillPath(handle);

            // Frequency label (shows the set center, not the wobbling live value)
            juce::String label = setCenter < 1000.f ? juce::String((int)setCenter) + "Hz"
                : juce::String(setCenter / 1000.f, 1) + "k";
            g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
            g.setColour(isEnabled ? bandColours[i] : bandColours[i].withAlpha(0.4f));
            g.drawText(label, (int)xCentr - 24, 12, 48, 12, juce::Justification::centred);
        }

        // Spectrum curve (drawn on top of band regions). Its baseline sits at the
        // bottom of the coloured band regions (h - 16) so it doesn't spill over the
        // frequency labels along the bottom edge.
        const float baseY = h - 16.f;

        // When the display is condensed, peak-normalise the curve so its loudest
        // point reaches near the top — trading absolute accuracy for contrast so
        // the shape stays legible at the much smaller height.
        const bool condensed = h < 80.f;
        float boost = 1.f;
        {
            float vmax = 0.f;
            for (int i = 0; i < scopeSize; ++i) vmax = juce::jmax(vmax, scopeData[i]);
            mDisplayMax = mDisplayMax * 0.85f + vmax * 0.15f;   // smooth to avoid jitter
            if (condensed && mDisplayMax > 0.05f)
                boost = juce::jlimit(1.f, 5.f, 0.98f / mDisplayMax);
        }

        juce::Path spectrum;
        bool started = false;
        for (int i = 0; i < scopeSize; ++i)
        {
            // Compute the same skewed bin used in timerCallback
            auto skewed = 1.f - std::exp(std::log(1.f - (float)i / scopeSize) * 0.2f);
            float bin = skewed * fftSize * 0.5f;
            float freq = juce::jlimit(20.f, 20000.f, bin * sampleRate / (float)fftSize);
            float x = getXForFrequency(freq, w);
            float y = juce::jmap(juce::jlimit(0.f, 1.f, scopeData[i] * boost), 0.f, 1.f, baseY, 0.f);
            if (!started) { spectrum.startNewSubPath(x, y); started = true; }
            else          spectrum.lineTo(x, y);
        }
        juce::Path filled = spectrum;
        filled.lineTo(w, baseY);
        filled.lineTo(0, baseY);
        filled.closeSubPath();

        g.setColour(juce::Colour(0xFF00FF88).withAlpha(0.12f));
        g.fillPath(filled);
        g.setColour(juce::Colour(0xFF00FF88).withAlpha(0.85f));
        g.strokePath(spectrum, juce::PathStrokeType(1.5f));
    }

    void resized() override {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        float w = (float)getWidth();
        float minDist = 20.f;
        draggedBand = -1;

        for (int i = 0; i < 4; ++i)
        {
            if (!centers[i]) continue;
            float x = getXForFrequency(processor.liveCenter[i].load(std::memory_order_relaxed), w);
            float dist = std::abs(e.position.x - x);
            if (dist < minDist)
            {
                minDist = dist;
                draggedBand = i;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggedBand < 0 || !centers[draggedBand]) return;
        float freq = getFrequencyForX(e.position.x, (float)getWidth());
        freq = juce::jlimit(20.f, 20000.f, freq);
        centers[draggedBand]->setValueNotifyingHost(
            centers[draggedBand]->getNormalisableRange().convertTo0to1(freq));
    }

    void mouseUp(const juce::MouseEvent&) override { draggedBand = -1; }

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopeSize = 512;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fftData[fftSize * 2] = {};
    float scopeData[scopeSize] = {};
    float sampleRate = 44100.f;
    float mDisplayMax = 0.f;   // smoothed peak level, used to normalise the condensed view

    GrainBrainAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::AudioParameterFloat* centers[4] = {};
    juce::AudioParameterFloat* widths[4] = {};
    juce::AudioParameterBool* enabled[4] = {};

    int draggedBand = -1;

    float getXForFrequency(float freq, float width)
    {
        return width * std::log(freq / 20.f) / std::log(20000.f / 20.f);
    }

    float getFrequencyForX(float x, float width)
    {
        return 20.f * std::pow(20000.f / 20.f, x / width);
    }
};