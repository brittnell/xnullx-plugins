#pragma once
#include <JuceHeader.h>
#include <functional>
#include "PluginProcessor.h"
#include "GrainBrainLookAndFeel.h"
#include "SpectrumAnalyzer.h"
#include "PresetManager.h"

//==============================================================================
// Collapse control — a clickable triangle (▼ expanded / ▶ collapsed).
// With showBar = true it also draws a full-width section title bar + label
// (used for GATE / LFO); with showBar = false it is just the triangle
// (used in the spectrum's top-left corner).
//==============================================================================
class CollapseControl : public juce::Component
{
public:
    std::function<void()> onToggle;
    bool         expanded = true;
    bool         showBar = true;
    juce::String label;

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        if (showBar)
        {
            g.setColour(bgMid);
            g.fillRoundedRectangle(b.reduced(1.f), 3.f);
            g.setColour(juce::Colour(0xFF333333));
            g.drawRoundedRectangle(b.reduced(1.f), 3.f, 0.6f);
        }

        float cx = showBar ? 12.f : getWidth() * 0.5f;
        float cy = getHeight() * 0.5f;
        juce::Path tri;
        if (expanded) tri.addTriangle(cx - 4.f, cy - 3.f, cx + 4.f, cy - 3.f, cx, cy + 4.f); // ▼
        else          tri.addTriangle(cx - 3.f, cy - 4.f, cx - 3.f, cy + 4.f, cx + 4.f, cy); // ▶
        g.setColour(juce::Colour(0xFF999999));
        g.fillPath(tri);

        if (showBar)
        {
            g.setColour(dimText);
            g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::bold)));
            g.drawText(label, juce::Rectangle<int>(24, 0, getWidth() - 28, getHeight()),
                juce::Justification::centredLeft, false);
        }
    }

    void mouseDown(const juce::MouseEvent&) override { if (onToggle) onToggle(); }
};

//==============================================================================
// LFO waveform preview
//==============================================================================
class LFOShapeDisplay : public juce::Component,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    LFOShapeDisplay(juce::AudioProcessorValueTreeState& apvts,
        const juce::String& shapeParamId,
        juce::Colour colour)
        : mApvts(apvts), mParamId(shapeParamId), mColour(colour)
    {
        apvts.addParameterListener(shapeParamId, this);
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(shapeParamId)))
            mShape = p->getIndex();
    }

    ~LFOShapeDisplay() override { mApvts.removeParameterListener(mParamId, this); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.f);
        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(b, 3.f);
        g.setColour(mColour.withAlpha(0.85f));

        juce::Path path;
        int   N = (int)b.getWidth();
        float cx = b.getX();
        float cy = b.getCentreY();
        float amp = b.getHeight() * 0.38f;
        const float rv[] = { 0.7f,-0.5f,0.9f,-0.8f,0.4f,-0.6f,0.75f,-0.3f };

        for (int x = 0; x <= N; ++x)
        {
            float t = (float)x / (float)N;
            float y = 0.f;
            switch (mShape)
            {
            case 0: y = std::sin(t * juce::MathConstants<float>::twoPi); break;
            case 1: y = t * 2.f - 1.f;  break;
            case 2: y = 1.f - t * 2.f;  break;
            case 3: y = t < 0.5f ? t * 4.f - 1.f : 3.f - t * 4.f; break;
            case 4: y = t < 0.5f ? 1.f : -1.f; break;
            case 5: y = rv[juce::jlimit(0, 7, (int)(t * 8.f))]; break;
            }
            float py = cy - y * amp;
            if (x == 0) path.startNewSubPath(cx, py);
            else        path.lineTo(cx + (float)x, py);
        }
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

private:
    void parameterChanged(const juce::String&, float v) override
    {
        mShape = (int)std::round(v);
        juce::MessageManager::callAsync([this] { repaint(); });
    }

    juce::AudioProcessorValueTreeState& mApvts;
    juce::String mParamId;
    juce::Colour mColour;
    int          mShape = 0;
};

//==============================================================================
// Right-click MIDI learn context menu
//==============================================================================
class KnobRightClickListener : public juce::MouseListener
{
public:
    KnobRightClickListener(const juce::String& paramID, MidiLearnManager& mlm)
        : mParamID(paramID), mMidiLearn(mlm) {
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!e.mods.isRightButtonDown()) return;

        int  cc = mMidiLearn.getCCForParam(mParamID);
        bool isLearning = (mMidiLearn.getLearningParamID() == mParamID);

        juce::PopupMenu menu;

        if (isLearning)
        {
            menu.addItem(1, "Waiting for CC input...", false, false);
            menu.addSeparator();
            menu.addItem(5, "Cancel");
        }
        else
        {
            if (cc >= 0)
                menu.addItem(1, "CC " + juce::String(cc) + " assigned", false, false);
            menu.addItem(2, "Assign MIDI CC");
            if (cc >= 0)
                menu.addItem(3, "Clear CC Assignment");
            menu.addSeparator();
            menu.addItem(4, "Clear All MIDI Assignments");
        }

        menu.showMenuAsync(juce::PopupMenu::Options{},
            [this](int result)
            {
                if (result == 2) mMidiLearn.startLearning(mParamID);
                else if (result == 3) mMidiLearn.clearParam(mParamID);
                else if (result == 4) mMidiLearn.clearAll();
                else if (result == 5) mMidiLearn.stopLearning();
            });
    }

private:
    juce::String      mParamID;
    MidiLearnManager& mMidiLearn;
};

//==============================================================================
// Band strip
//==============================================================================
class BandStrip : public juce::Component,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    BandStrip(GrainBrainAudioProcessor& p, int bandIndex,
        juce::AudioProcessorValueTreeState& apvts);
    ~BandStrip() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void repaintKnobs();

    // Collapse support (driven globally by the editor)
    void setGateExpanded(bool e);
    void setLfoExpanded(bool e);

    static constexpr int kBarH = 20;            // full-width section toggle bar height
    static int coreHeight();                    // strip-relative y where the GATE bar sits
    static int gateHeight();                    // height of the gate control block
    static int lfoHeight();                     // height of the lfo control block
    static int totalHeight(bool gateExpanded, bool lfoExpanded);

private:
    void parameterChanged(const juce::String& paramID, float newValue) override;
    void setGateChildrenVisible(bool v);
    void setLfoChildrenVisible(bool v);

    bool mGateExpanded = true;
    bool mLfoExpanded = true;

    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText,
        const juce::String& paramId, juce::AudioProcessorValueTreeState& apvts,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
        int knobSize = 43);

    void setupButton(juce::TextButton& btn, const juce::String& label,
        const juce::String& paramId, juce::AudioProcessorValueTreeState& apvts,
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att);

    void attachMidiListener(juce::Slider& s, const juce::String& paramID);

    int          mBandIndex;
    juce::Colour mColour;
    bool         mFrozen = false;
    bool         mEnabled = true;
    juce::AudioProcessorValueTreeState& mApvts;
    MidiLearnManager* mMidiLearn = nullptr;

    std::vector<std::pair<juce::Slider*, juce::String>>      mKnobParams;
    std::vector<std::unique_ptr<KnobRightClickListener>>     mListeners;

    juce::Rectangle<int> mGateBounds, mLfoBounds;

    juce::Label      lBandName;
    juce::TextButton btnEnabled;
    juce::Slider     sBandCenter, sBandWidth;
    juce::Label      lBandCenter, lBandWidth;

    juce::Slider     sSize, sScatter, sPitch, sPitchSpread, sDrive, sMix, sGain, sFeedback;
    juce::Label      lSize, lScatter, lPitch, lPitchSpread, lDrive, lMix, lGain, lFeedback;
    juce::TextButton btnReverse, btnFreeze, btnSync;
    juce::ComboBox   cbDiv;
    juce::Label      lDiv;

    juce::Label      lGateSection;
    juce::TextButton btnGate;
    juce::Slider     sGateDepth, sGateAttack, sGateRelease, sGatePhase;
    juce::Label      lGateDepth, lGateAttack, lGateRelease, lGatePhase;
    juce::ComboBox   cbGateDiv;
    juce::Label      lGateDiv;

    juce::Label      lLfoSection;
    std::unique_ptr<LFOShapeDisplay> mLfoShapeDisplay;
    juce::Slider     sLfoRate;
    juce::Label      lLfoRate;
    juce::TextButton btnLfoOn, btnLfoSync;
    juce::ComboBox   cbLfoShape, cbLfoDiv;
    juce::Label      lLfoDiv;
    juce::Label      lLfoColDepth, lLfoColPhase;
    juce::Slider     sLfoDepth[7];
    juce::Slider     sLfoPhase[7];
    juce::Label      lLfoTarget[7];

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aEnabled;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        aBandCenter, aBandWidth,
        aSize, aScatter, aPitch, aPitchSpread, aDrive, aMix, aGain, aFeedback,
        aGateDepth, aGateAttack, aGateRelease, aGatePhase,
        aLfoRate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        aReverse, aFreeze, aSync, aGate, aLfoOn, aLfoSync;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        aDiv, aGateDiv, aLfoShape, aLfoDiv;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        aLfoDepth[7], aLfoPhase[7];
};

//==============================================================================
// Main editor
//==============================================================================
class GrainBrainAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    GrainBrainAudioProcessorEditor(GrainBrainAudioProcessor&);
    ~GrainBrainAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static juce::File getMidiMapFile();

    GrainBrainAudioProcessor& audioProcessor;
    GrainBrainLookAndFeel laf;
    PresetManager presetManager;

    std::unique_ptr<BandStrip>        bandStrips[4];
    std::unique_ptr<SpectrumAnalyzer> spectrumAnalyzer;

    CollapseControl spectrumToggle, gateBar, lfoBar;
    bool mSpectrumExpanded = true;
    bool mGateExpanded = true;
    bool mLfoExpanded = true;

    juce::Slider sDryIn, sWetOut, sPassthrough;
    juce::Label  lDryIn, lWetOut, lPassthrough;
    juce::Label  lBpm;

    juce::ComboBox   presetBox;
    juce::TextButton btnSave, btnReset, btnDelete;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        aDryIn, aWetOut, aPassthrough;

    std::unique_ptr<juce::AlertWindow> saveDialog;
    std::unique_ptr<juce::AlertWindow> deleteDialog;

    void setupHSlider(juce::Slider& s, juce::Label& l, const juce::String& labelText,
        const juce::String& paramId,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att);
    void refreshPresetBox();
    void savePreset();
    void deletePreset();
    void resetAllParameters();

    void applyCollapseLayout();
    void persistCollapseState();
    void reloadCollapseStateFromTree();
    int  computeWindowHeight() const;
    static constexpr int kSpectrumExpandedH = 150;        // default / full height
    static constexpr int kSpectrumCondensedH = 38;        // collapsed ≈ 1/4 of full

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainBrainAudioProcessorEditor)
};