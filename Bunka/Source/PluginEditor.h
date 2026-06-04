#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "BunkaLookAndFeel.h"
#include "PresetManager.h"
#include "WaveformDisplay.h"
#include <cstring>

//==============================================================================
// Small speaker icon button — auditions the raw, unaltered loaded file.
// Renders the supplied SVG (fills inlined for JUCE's parser), tinted to the
// steel accent at rest, brighter on hover, spark on press.
//==============================================================================
class SpeakerButton : public juce::Button
{
public:
    SpeakerButton() : juce::Button ("audition")
    {
        setTooltip ("Audition original file");
        drawable = juce::Drawable::createFromImageData (kSpeakerSvg, std::strlen (kSpeakerSvg));
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        if (drawable == nullptr) return;
        const juce::Colour target = down ? bunkaSpark
                                         : (over ? bunkaSteel.brighter (0.25f) : bunkaSteel);
        drawable->replaceColour (curColour, target);
        curColour = target;
        auto b = getLocalBounds().toFloat();
        auto half = b.withSizeKeepingCentre (b.getWidth() * 0.5f, b.getHeight() * 0.5f);  // 50% smaller
        drawable->drawWithin (g, half, juce::RectanglePlacement::centred, 1.0f);
    }

private:
    std::unique_ptr<juce::Drawable> drawable;
    juce::Colour curColour { 0xff3e9bb7 };     // the SVG's source fill colour

    static constexpr const char* kSpeakerSvg =
        R"SPK(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 86.66 73.15">
<path fill="#3e9bb7" d="M66.69,7.02c-1.23-.72-2.81-.3-3.52.94-.72,1.23-.3,2.81.94,3.52,9.06,5.25,14.68,15.01,14.68,25.47s-5.63,20.21-14.68,25.47c-1.23.72-1.65,2.29-.94,3.52.48.83,1.34,1.28,2.22,1.28.44,0,.89-.11,1.29-.35,5.15-2.99,9.47-7.27,12.49-12.41,3.11-5.29,4.76-11.34,4.76-17.51s-1.65-12.23-4.76-17.51c-3.02-5.13-7.34-9.42-12.49-12.41h0Z"/>
<path fill="#3e9bb7" d="M69.29,48.64c2.08-3.54,3.18-7.58,3.18-11.7s-1.1-8.17-3.18-11.7c-2.02-3.43-4.91-6.29-8.35-8.29-1.23-.72-2.81-.3-3.52.94-.72,1.23-.3,2.81.94,3.52,5.53,3.2,8.96,9.16,8.96,15.54s-3.43,12.33-8.96,15.54c-1.23.72-1.65,2.29-.94,3.52.48.83,1.34,1.28,2.22,1.28.44,0,.89-.11,1.29-.35,3.44-2,6.33-4.86,8.35-8.29h0Z"/>
<path fill="#3e9bb7" d="M53.41,6.45c0-1.3-.68-2.45-1.83-3.07-1.14-.62-2.48-.57-3.57.14l-24.85,16.13c-1.48.96-3.2,1.47-4.97,1.47H6.74c-2.12,0-3.84,1.72-3.84,3.84v23.97c0,2.12,1.72,3.84,3.84,3.84h11.45c1.77,0,3.49.51,4.97,1.47l24.85,16.13c1.09.71,2.43.76,3.57.14,1.14-.62,1.83-1.77,1.83-3.07V6.45Z"/>
</svg>)SPK";
};

//==============================================================================
// Text button that flashes a colour wash (fading out) when triggered — used to
// confirm SHUFFLE / RANDOMIZE fired.
//==============================================================================
class FlashButton : public juce::TextButton,
                    private juce::Timer
{
public:
    void setFlashColour (juce::Colour c) { flashColour = c; }
    void flashNow() { level = 1.f; startTimerHz (30); repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        juce::TextButton::paintButton (g, over, down);
        if (level > 0.001f)
        {
            g.setColour (flashColour.withAlpha (level * 0.85f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.f);
        }
    }

private:
    void timerCallback() override
    {
        level -= 0.05f;                       // ~0.66 s fade (twice as long)
        if (level <= 0.f) { level = 0.f; stopTimer(); }
        repaint();
    }

    float        level = 0.f;
    juce::Colour flashColour { bunkaSteel };
};

//==============================================================================
// Horizontal labeled step selector: labels above, ticked track below, steel
// triangle pointer at the selected step. Click/drag snaps to the nearest step.
//==============================================================================
class StepSlider : public juce::Component
{
public:
    std::function<void (int)> onChange;

    void setLabels (juce::StringArray l) { labels = std::move (l); repaint(); }
    void setIndex  (int i)
    {
        i = juce::jlimit (0, juce::jmax (0, labels.size() - 1), i);
        if (i != index) { index = i; repaint(); }
    }
    int getIndex() const { return index; }

    void paint (juce::Graphics& g) override
    {
        const int n = labels.size();
        if (n <= 0) return;

        auto b = getLocalBounds().toFloat();
        auto labelArea = b.removeFromTop (b.getHeight() * 0.55f);
        const float trackY = b.getCentreY();
        const float x0 = b.getX() + 14.f, x1 = b.getRight() - 14.f;

        g.setColour (juce::Colour (0xFF3A3A3A));
        g.drawLine (x0, trackY, x1, trackY, 2.f);

        for (int i = 0; i < n; ++i)
        {
            const float fx = x0 + (x1 - x0) * (n == 1 ? 0.f : (float) i / (n - 1));
            const bool sel = (i == index);
            g.setColour (sel ? bunkaSteel : juce::Colour (0xFF555555));
            g.drawLine (fx, trackY - 5.f, fx, trackY + 5.f, sel ? 2.f : 1.2f);

            g.setColour (sel ? bunkaSteel : bunkaDim);
            g.setFont (juce::Font (juce::FontOptions ("Courier New", sel ? 10.f : 9.f,
                                                      sel ? juce::Font::bold : juce::Font::plain)));
            g.drawText (labels[i], juce::Rectangle<float> (fx - 24.f, labelArea.getY(), 48.f, labelArea.getHeight()),
                        juce::Justification::centred, false);
        }

        const float px = x0 + (x1 - x0) * (n == 1 ? 0.f : (float) index / (n - 1));
        juce::Path tri;
        tri.addTriangle (px - 6.f, trackY + 11.f, px + 6.f, trackY + 11.f, px, trackY + 3.f);
        g.setColour (bunkaSteel);
        g.fillPath (tri);
    }

    void mouseDown (const juce::MouseEvent& e) override { pick (e.position.x); }
    void mouseDrag (const juce::MouseEvent& e) override { pick (e.position.x); }

private:
    void pick (float x)
    {
        const int n = labels.size();
        if (n <= 0) return;
        auto b = getLocalBounds().toFloat();
        const float x0 = b.getX() + 14.f, x1 = b.getRight() - 14.f;
        const float t = juce::jlimit (0.f, 1.f, (x - x0) / juce::jmax (1.f, x1 - x0));
        const int i = (int) std::lround (t * (n - 1));
        if (i != index) { index = i; repaint(); if (onChange) onChange (index); }
    }

    juce::StringArray labels;
    int index = 0;
};

//==============================================================================
// Tiny per-pad lock: a tick-box + small "lock" label; lights spark when on.
//==============================================================================
class LockButton : public juce::Button
{
public:
    LockButton() : juce::Button ("lock") { setClickingTogglesState (true); }
    void setLockColour (juce::Colour c) { lockColour = c; }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat();
        const bool  on = getToggleState();
        const float bs = juce::jmin (b.getHeight() - 1.f, 9.f);
        juce::Rectangle<float> box (b.getX(), b.getCentreY() - bs * 0.5f, bs, bs);

        g.setColour (on ? lockColour : juce::Colour (over ? 0xFFAAAAAA : 0xFF666666));
        if (on) g.fillRoundedRectangle (box, 1.5f);
        else    g.drawRoundedRectangle (box.reduced (0.5f), 1.5f, 1.f);

        g.setColour (on ? lockColour : bunkaDim);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 7.f, juce::Font::plain)));
        g.drawText ("lock", b.withTrimmedLeft (bs + 3.f), juce::Justification::centredLeft, false);
    }

private:
    juce::Colour lockColour { bunkaSpark };
};

//==============================================================================
class BunkaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit BunkaAudioProcessorEditor (BunkaAudioProcessor&);
    ~BunkaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void styleKnob   (juce::Slider&, juce::Label&, const juce::String& label, const juce::String& paramID);
    void updateKnobAccent (juce::Slider&, const juce::String& paramID);   // blue default / orange if moved
    void styleToggle (juce::TextButton&, const juce::String& text, const juce::String& paramID, juce::Colour on);
    void refreshPresetList();
    void doSavePreset();
    void setChoiceParam (const juce::String& id, int idx);    // for StepSlider <-> choice param
    int  getChoiceParam (const juce::String& id) const;

    BunkaAudioProcessor&  proc;
    BunkaLookAndFeel      lnf;
    BunkaPresetManager    presetMgr { proc.apvts };

    WaveformDisplay   waveform;
    SpeakerButton     speakerB;
    juce::TextButton  loadB, saveB, delB;
    juce::ComboBox    presetBox;
    juce::Label       fileNameL, bpmL;

    juce::Slider gainK, pitchK, attackK, releaseK, threshK, barsK, chanceK;
    juce::Label  gainL, pitchL, attackL, releaseL, threshL, barsL, chanceL;

    juce::TextButton sliceModeB;                  // Transient / Grid toggle
    StepSlider       gridDivSlider, snapDivSlider;
    juce::Label      gridDivL, snapDivL;

    juce::TextButton pitchPreserveB, reverseB, hostSyncB, snapGridB;

    // pattern bank
    juce::Label      patternsHeaderL;
    juce::TextButton patternPads[12];
    LockButton       lockButtons[12];
    FlashButton      shuffleB, randomizeB;
    juce::TextButton seqB;

    std::unique_ptr<SA> aGain, aPitch, aAttack, aRelease, aThresh, aBars, aChance;
    std::unique_ptr<BA> aPitchPreserve, aReverse, aHostSync, aSeqOn, aSliceMode;

    std::unique_ptr<juce::FileChooser> chooser;
    int lastWaveformVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BunkaAudioProcessorEditor)
};
