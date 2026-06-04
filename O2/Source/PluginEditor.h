#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "O2LookAndFeel.h"
#include "PresetManager.h"

//==============================================================================
// Peak meter
//==============================================================================
class O2PeakMeter : public juce::Component
{
public:
    void setPeak(float linearPeak)
    {
        float db = juce::Decibels::gainToDecibels(linearPeak, -60.f);
        mLevel = juce::jlimit(0.f, 1.f, (db + 48.f) / 60.f);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(b, 2.f);

        int   segments = 12;
        float segH     = b.getHeight() / (float)segments;
        int   lit      = (int)(mLevel * (float)segments);

        for (int i = 0; i < segments; ++i)
        {
            float y = b.getBottom() - (i + 1) * segH;
            juce::Colour c = (i >= 10) ? juce::Colour(0xFFFF2222)
                           : (i >= 7)  ? juce::Colour(0xFFFFCC00)
                           :             juce::Colour(0xFF00CC66);
            g.setColour(i < lit ? c : c.withAlpha(0.12f));
            g.fillRoundedRectangle(b.getX() + 1.f, y + 1.f,
                                   b.getWidth() - 2.f, segH - 2.f, 1.f);
        }
    }

private:
    float mLevel = 0.f;
};

//==============================================================================
// Band strip
//==============================================================================
class O2BandStrip : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    O2BandStrip(O2AudioProcessor& p, int bandIndex,
                juce::AudioProcessorValueTreeState& apvts);
    ~O2BandStrip() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged(const juce::String& paramID, float newValue) override;
    void repopulateVariantBox(int typeIdx);

    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText,
                   const juce::String& paramId,
                   std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att);

    void setupButton(juce::TextButton& btn, const juce::String& label,
                     const juce::String& paramId,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att);

    int          mBandIndex;
    juce::Colour mColour;
    juce::AudioProcessorValueTreeState& mApvts;
    int          mSeparatorY = 0;

    juce::Label      lBandName;

    juce::ComboBox   cbType, cbVariant;
    juce::Label      lType, lVariant;

    // Main knobs
    juce::Slider     sDrive, sWidth, sLevel;
    juce::Label      lDrive, lWidth, lLevel;

    // New knobs row
    juce::Slider     sFeedback, sHiss, sDrift;
    juce::Label      lFeedback, lHiss, lDrift;

    // Button row 1
    juce::TextButton btnBypass, btnMute;
    // Button row 2
    juce::TextButton btnFlipPhase, btnGateFbk;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aType, aVariant;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        aDrive, aWidth, aLevel, aFeedback, aHiss, aDrift;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        aBypass, aMute, aFlipPhase, aGateFbk;
};

//==============================================================================
// Main editor
//==============================================================================
class O2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit O2AudioProcessorEditor(O2AudioProcessor&);
    ~O2AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText,
                   const juce::String& paramId,
                   std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att);

    void refreshPresetBox();
    void savePreset();
    void deletePreset();
    void resetAllParameters();

    O2AudioProcessor&  audioProcessor;
    O2LookAndFeel      laf;
    O2PresetManager    presetManager;

    std::unique_ptr<O2BandStrip> bandStrips[3];

    // Global knobs
    juce::Slider     sInputTrim, sLowXover, sHighXover, sOutputLevel, sDryWet;
    juce::Label      lInputTrim, lLowXover, lHighXover, lOutputLevel, lDryWet;

    // Global toggles
    juce::TextButton btnMono, btnBypass, btnOversample, btnWiden;

    // Meters
    O2PeakMeter meterIn, meterOut;
    juce::Label lMeterIn, lMeterOut;

    // Top bar
    juce::ComboBox   presetBox;
    juce::TextButton btnSave, btnReset, btnDelete;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        aInputTrim, aLowXover, aHighXover, aOutputLevel, aDryWet;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        aMono, aMasterBypass, aOversample, aWiden;

    std::unique_ptr<juce::AlertWindow> saveDialog;
    std::unique_ptr<juce::AlertWindow> deleteDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(O2AudioProcessorEditor)
};
