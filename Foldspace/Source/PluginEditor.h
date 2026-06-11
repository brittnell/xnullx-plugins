#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FoldspaceLookAndFeel.h"
#include "PresetManager.h"
#include "TransferDisplay.h"
#include "ModScope.h"

//==============================================================================
// Horizontal labeled step selector (family pattern from Bunka), with a
// settable accent colour so each stage strip can own its selector.
//==============================================================================
class StepSlider : public juce::Component
{
public:
    std::function<void(int)> onChange;

    void setAccent(juce::Colour c)        { accent = c; repaint(); }
    void setLabels(juce::StringArray l)   { labels = std::move(l); repaint(); }
    void setIndex(int i)
    {
        i = juce::jlimit(0, juce::jmax(0, labels.size() - 1), i);
        if (i != index) { index = i; repaint(); }
    }
    int getIndex() const { return index; }

    void paint(juce::Graphics& g) override
    {
        const int n = labels.size();
        if (n <= 0) return;

        auto b = getLocalBounds().toFloat();
        auto labelArea = b.removeFromTop(b.getHeight() * 0.55f);
        const float trackY = b.getCentreY();
        const float x0 = b.getX() + 12.f, x1 = b.getRight() - 12.f;

        g.setColour(juce::Colour(0xFF3A3A3A));
        g.drawLine(x0, trackY, x1, trackY, 2.f);

        for (int i = 0; i < n; ++i)
        {
            const float fx = x0 + (x1 - x0) * (n == 1 ? 0.f : (float)i / (n - 1));
            const bool sel = (i == index);
            g.setColour(sel ? accent : juce::Colour(0xFF555555));
            g.drawLine(fx, trackY - 5.f, fx, trackY + 5.f, sel ? 2.f : 1.2f);

            g.setColour(sel ? accent : fsDim);
            g.setFont(juce::Font(juce::FontOptions("Courier New", sel ? 10.f : 9.f,
                                                   sel ? juce::Font::bold : juce::Font::plain)));
            g.drawText(labels[i],
                       juce::Rectangle<float>(fx - 24.f, labelArea.getY(), 48.f, labelArea.getHeight()),
                       juce::Justification::centred, false);
        }

        const float px = x0 + (x1 - x0) * (n == 1 ? 0.f : (float)index / (n - 1));
        juce::Path tri;
        tri.addTriangle(px - 6.f, trackY + 11.f, px + 6.f, trackY + 11.f, px, trackY + 3.f);
        g.setColour(accent);
        g.fillPath(tri);
    }

    void mouseDown(const juce::MouseEvent& e) override { pick(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { pick(e.position.x); }

private:
    void pick(float x)
    {
        const int n = labels.size();
        if (n <= 0) return;
        auto b = getLocalBounds().toFloat();
        const float x0 = b.getX() + 12.f, x1 = b.getRight() - 12.f;
        const float t = juce::jlimit(0.f, 1.f, (x - x0) / juce::jmax(1.f, x1 - x0));
        const int i = (int)std::lround(t * (n - 1));
        if (i != index) { index = i; repaint(); if (onChange) onChange(index); }
    }

    juce::StringArray labels;
    juce::Colour accent { fsSpice };
    int index = 0;
};

//==============================================================================
// Segmented peak meter (vertical). Editor timer feeds block peaks; the meter
// applies its own release so it reads naturally at 30 Hz.
//==============================================================================
class SegMeter : public juce::Component
{
public:
    void pushLevel(float peak)
    {
        level = juce::jmax(peak, level * 0.82f);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.f);
        const int  segs = 12;
        const float gap = 2.f;
        const float segH = (b.getHeight() - gap * (segs - 1)) / (float)segs;

        const float db   = juce::Decibels::gainToDecibels(level, -60.f);
        const float norm = juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f);
        const int   lit  = (int)std::round(norm * segs);

        for (int i = 0; i < segs; ++i)
        {
            const float y = b.getBottom() - segH - (float)i * (segH + gap);
            juce::Colour c = i >= 10 ? fsRed
                           : i >= 8  ? fsSpice
                                     : juce::Colour(0xFF22CC66);
            g.setColour(i < lit ? c : juce::Colour(0xFF202020));
            g.fillRect(b.getX(), y, b.getWidth(), segH);
        }
    }

private:
    float level = 0.f;
};

//==============================================================================
class FoldspaceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit FoldspaceAudioProcessorEditor(FoldspaceAudioProcessor&);
    ~FoldspaceAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider s;
        juce::Label  l;
        std::unique_ptr<SA> att;
    };

    void timerCallback() override;
    void styleKnob(Knob&, const juce::String& label, const juce::String& paramID, juce::Colour accent);
    void placeKnob(Knob&, int cellCentreX, int y);
    void setChoiceParam(const juce::String& id, int idx);
    int  getChoiceParam(const juce::String& id) const;
    void refreshPresetList();
    void doSavePreset();
    void doReset();

    // shared geometry (paint + resized must agree)
    static constexpr int kW = 860, kH = 600;
    static constexpr int kTopH = 44, kStripGap = 3;
    static constexpr int kVizY = 53, kVizH = 120;
    static constexpr int kStripY = 181, kStripH = 330, kStripX0 = 8;
    static constexpr int kStripW = 211;
    static constexpr int kBotY = 519;

    FoldspaceAudioProcessor& proc;
    FoldspaceLookAndFeel     lnf;
    FoldspacePresetManager   presetMgr { proc.apvts };

    TransferDisplay transfer { proc };
    ModScope        scope    { proc };

    // top bar
    juce::ComboBox   presetBox;
    juce::TextButton saveB, delB, resetB;
    juce::Label      pitchL;

    // modulator strip
    StepSlider  waveSlider;
    Knob        ratioK, fineK, srcOscK, srcInK, modFoldK;
    juce::TextButton syncB;
    juce::ComboBox   divBox;
    std::unique_ptr<BA> aSync;
    std::unique_ptr<CA> aDiv;

    // PM strip
    Knob indexK, carrierK, envIdxK, stereoK, atkK, relK;

    // fold strip
    Knob foldsK, shapeK, symK, toneK, envFoldK;

    // feedback strip
    Knob fbK, fbFoldK, fbToneK, driftK;

    // bottom bar
    StepSlider  pitchModeSlider, osSlider;
    juce::Label pitchModeL, osL;
    Knob        freeHzK, glideK, inK, mixK, outK;
    juce::TextButton bypassB;
    std::unique_ptr<BA> aBypass;
    SegMeter    inMeter, outMeter;
    juce::Label inMeterL, outMeterL;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoldspaceAudioProcessorEditor)
};
