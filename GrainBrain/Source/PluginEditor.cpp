#include "PluginProcessor.h"
#include "PluginEditor.h"

static const juce::Colour sectionBg = juce::Colour(0xFF0A0A0A);
static const juce::Colour sectionBorder = juce::Colour(0xFF222222);

// Shared band-strip layout metrics (used by both BandStrip::resized and the
// static height helpers so the editor's full-width bars stay aligned).
namespace {
    constexpr int kKnobSz = 43;
    constexpr int kValBoxH = 13;
    constexpr int kLblH = 13;
    constexpr int kSliderH = kKnobSz + kValBoxH;   // 56
    constexpr int kCellH = kSliderH + kLblH + 2;   // 71
    constexpr int kLfoRowH = kSliderH + 4;         // 60
    constexpr int kStripInsetTop = 12;             // reduced(6) + removeFromTop(6)
    constexpr int kStripInsetBottom = 6;
}

//==============================================================================
// BandStrip
//==============================================================================
BandStrip::BandStrip(GrainBrainAudioProcessor& p, int bandIndex,
    juce::AudioProcessorValueTreeState& apvts)
    : mBandIndex(bandIndex), mColour(bandColours[bandIndex]),
    mApvts(apvts), mMidiLearn(&p.midiLearn)
{
    juce::String id = juce::String(bandIndex);
    juce::StringArray divNames{ "1/1","1/2","1/4","1/4T","1/8","1/8T","1/16","1/16T" };
    juce::StringArray shapeNames{ "Sine","Saw+","Saw-","Tri","Step","Rand" };

    apvts.addParameterListener("grainFreeze" + id, this);
    apvts.addParameterListener("bandEnabled" + id, this);
    mFrozen = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("grainFreeze" + id))->get();
    mEnabled = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bandEnabled" + id))->get();

    // Band name label
    juce::StringArray names{ "LOW", "LO MID", "HI MID", "HIGH" };
    lBandName.setText(names[bandIndex], juce::dontSendNotification);
    lBandName.setFont(juce::Font(juce::FontOptions("Courier New", 11.f, juce::Font::bold)));
    lBandName.setColour(juce::Label::textColourId, mColour);
    lBandName.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(lBandName);

    setupButton(btnEnabled, "PWR", "bandEnabled" + id, apvts, aEnabled);

    // Filter
    setupKnob(sBandCenter, lBandCenter, "Center", "bandCenter" + id, apvts, aBandCenter);
    setupKnob(sBandWidth, lBandWidth, "BW %", "bandWidth" + id, apvts, aBandWidth);

    // Grain
    setupKnob(sSize, lSize, "Size", "grainSize" + id, apvts, aSize);
    setupKnob(sScatter, lScatter, "Scatter", "grainScatter" + id, apvts, aScatter);
    setupKnob(sPitch, lPitch, "Pitch", "grainPitch" + id, apvts, aPitch);
    setupKnob(sPitchSpread, lPitchSpread, "Spread", "grainPitchSpread" + id, apvts, aPitchSpread);
    setupKnob(sDrive, lDrive, "Drive", "grainDrive" + id, apvts, aDrive);
    setupKnob(sMix, lMix, "Mix", "grainMix" + id, apvts, aMix);
    setupKnob(sGain, lGain, "Gain", "bandGain" + id, apvts, aGain);
    setupKnob(sFeedback, lFeedback, "Bleed", "feedbackAmt" + id, apvts, aFeedback);

    setupButton(btnReverse, "REV", "grainReverse" + id, apvts, aReverse);
    setupButton(btnFreeze, "FRZ", "grainFreeze" + id, apvts, aFreeze);
    setupButton(btnSync, "SYNC", "grainSync" + id, apvts, aSync);

    for (int i = 0; i < divNames.size(); ++i) cbDiv.addItem(divNames[i], i + 1);
    cbDiv.setSelectedId(3, juce::dontSendNotification);
    cbDiv.setColour(juce::ComboBox::backgroundColourId, bgMid);
    cbDiv.setColour(juce::ComboBox::textColourId, mColour);
    cbDiv.setColour(juce::ComboBox::outlineColourId, mColour.withAlpha(0.4f));
    cbDiv.setColour(juce::ComboBox::arrowColourId, mColour);
    addAndMakeVisible(cbDiv);
    aDiv = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "grainDiv" + id, cbDiv);
    lDiv.setText("DIV", juce::dontSendNotification);
    lDiv.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    lDiv.setColour(juce::Label::textColourId, dimText);
    lDiv.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lDiv);

    // Gate section
    lGateSection.setText("--- GATE ---", juce::dontSendNotification);
    lGateSection.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    lGateSection.setColour(juce::Label::textColourId, mColour.withAlpha(0.7f));
    lGateSection.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lGateSection);

    setupButton(btnGate, "GATE", "gateOn" + id, apvts, aGate);

    for (int i = 0; i < divNames.size(); ++i) cbGateDiv.addItem(divNames[i], i + 1);
    cbGateDiv.setSelectedId(3, juce::dontSendNotification);
    cbGateDiv.setColour(juce::ComboBox::backgroundColourId, bgMid);
    cbGateDiv.setColour(juce::ComboBox::textColourId, mColour);
    cbGateDiv.setColour(juce::ComboBox::outlineColourId, mColour.withAlpha(0.4f));
    cbGateDiv.setColour(juce::ComboBox::arrowColourId, mColour);
    addAndMakeVisible(cbGateDiv);
    aGateDiv = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "gateDiv" + id, cbGateDiv);
    lGateDiv.setText("DIV", juce::dontSendNotification);
    lGateDiv.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    lGateDiv.setColour(juce::Label::textColourId, dimText);
    lGateDiv.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lGateDiv);

    setupKnob(sGateDepth, lGateDepth, "Depth", "gateDepth" + id, apvts, aGateDepth);
    setupKnob(sGateAttack, lGateAttack, "Attack", "gateAttack" + id, apvts, aGateAttack);
    setupKnob(sGateRelease, lGateRelease, "Release", "gateRelease" + id, apvts, aGateRelease);
    setupKnob(sGatePhase, lGatePhase, "Phase", "gatePhase" + id, apvts, aGatePhase);

    // LFO section
    lLfoSection.setText("--- LFO ---", juce::dontSendNotification);
    lLfoSection.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    lLfoSection.setColour(juce::Label::textColourId, mColour.withAlpha(0.7f));
    lLfoSection.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lLfoSection);

    mLfoShapeDisplay = std::make_unique<LFOShapeDisplay>(apvts, "lfoShape" + id, mColour);
    addAndMakeVisible(*mLfoShapeDisplay);

    // LFO rate slider — also gets MIDI CC support
    sLfoRate.setSliderStyle(juce::Slider::LinearHorizontal);
    sLfoRate.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
    sLfoRate.setColour(juce::Slider::trackColourId, juce::Colour(0xFF333333));
    sLfoRate.setColour(juce::Slider::thumbColourId, mColour);
    sLfoRate.setColour(juce::Slider::textBoxTextColourId, mColour);
    sLfoRate.setColour(juce::Slider::textBoxBackgroundColourId, bgMid);
    sLfoRate.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    sLfoRate.getProperties().set("accentColour", (int)mColour.getARGB());
    addAndMakeVisible(sLfoRate);
    aLfoRate = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "lfoRate" + id, sLfoRate);
    attachMidiListener(sLfoRate, "lfoRate" + id);

    lLfoRate.setText("HZ", juce::dontSendNotification);
    lLfoRate.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    lLfoRate.setColour(juce::Label::textColourId, dimText);
    lLfoRate.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lLfoRate);

    setupButton(btnLfoOn, "ON", "lfoOn" + id, apvts, aLfoOn);
    setupButton(btnLfoSync, "SYNC", "lfoSync" + id, apvts, aLfoSync);

    for (int i = 0; i < shapeNames.size(); ++i) cbLfoShape.addItem(shapeNames[i], i + 1);
    cbLfoShape.setSelectedId(1, juce::dontSendNotification);
    cbLfoShape.setColour(juce::ComboBox::backgroundColourId, bgMid);
    cbLfoShape.setColour(juce::ComboBox::textColourId, mColour);
    cbLfoShape.setColour(juce::ComboBox::outlineColourId, mColour.withAlpha(0.4f));
    cbLfoShape.setColour(juce::ComboBox::arrowColourId, mColour);
    addAndMakeVisible(cbLfoShape);
    aLfoShape = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "lfoShape" + id, cbLfoShape);

    for (int i = 0; i < divNames.size(); ++i) cbLfoDiv.addItem(divNames[i], i + 1);
    cbLfoDiv.setSelectedId(5, juce::dontSendNotification);
    cbLfoDiv.setColour(juce::ComboBox::backgroundColourId, bgMid);
    cbLfoDiv.setColour(juce::ComboBox::textColourId, mColour);
    cbLfoDiv.setColour(juce::ComboBox::outlineColourId, mColour.withAlpha(0.4f));
    cbLfoDiv.setColour(juce::ComboBox::arrowColourId, mColour);
    addAndMakeVisible(cbLfoDiv);
    aLfoDiv = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "lfoDiv" + id, cbLfoDiv);

    lLfoDiv.setText("DIV", juce::dontSendNotification);
    lLfoDiv.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    lLfoDiv.setColour(juce::Label::textColourId, dimText);
    lLfoDiv.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lLfoDiv);

    lLfoColDepth.setText("DEPTH", juce::dontSendNotification);
    lLfoColDepth.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    lLfoColDepth.setColour(juce::Label::textColourId, dimText);
    lLfoColDepth.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lLfoColDepth);

    lLfoColPhase.setText("PHASE", juce::dontSendNotification);
    lLfoColPhase.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    lLfoColPhase.setColour(juce::Label::textColourId, dimText);
    lLfoColPhase.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lLfoColPhase);

    const juce::String depthIDs[] = { "lfoD_ctr","lfoD_wid","lfoD_mix","lfoD_pit","lfoD_sct","lfoD_siz","lfoD_gph" };
    const juce::String phaseIDs[] = { "lfoP_ctr","lfoP_wid","lfoP_mix","lfoP_pit","lfoP_sct","lfoP_siz","lfoP_gph" };
    const juce::String targetNames[] = { "CENTER","WIDTH","MIX","PITCH","SCATTER","SIZE","GT PHASE" };

    for (int t = 0; t < 7; ++t)
    {
        lLfoTarget[t].setText(targetNames[t], juce::dontSendNotification);
        lLfoTarget[t].setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
        lLfoTarget[t].setColour(juce::Label::textColourId, mColour.withAlpha(0.7f));
        lLfoTarget[t].setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lLfoTarget[t]);

        sLfoDepth[t].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sLfoDepth[t].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 13);
        sLfoDepth[t].setColour(juce::Slider::textBoxTextColourId, mColour);
        sLfoDepth[t].setColour(juce::Slider::textBoxBackgroundColourId, bgMid);
        sLfoDepth[t].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sLfoDepth[t].getProperties().set("accentColour", (int)mColour.getARGB());
        addAndMakeVisible(sLfoDepth[t]);
        aLfoDepth[t] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, depthIDs[t] + id, sLfoDepth[t]);
        attachMidiListener(sLfoDepth[t], depthIDs[t] + id);

        sLfoPhase[t].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sLfoPhase[t].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 13);
        sLfoPhase[t].setColour(juce::Slider::textBoxTextColourId, mColour.withAlpha(0.6f));
        sLfoPhase[t].setColour(juce::Slider::textBoxBackgroundColourId, bgMid);
        sLfoPhase[t].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sLfoPhase[t].getProperties().set("accentColour", (int)mColour.withAlpha(0.5f).getARGB());
        addAndMakeVisible(sLfoPhase[t]);
        aLfoPhase[t] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, phaseIDs[t] + id, sLfoPhase[t]);
        attachMidiListener(sLfoPhase[t], phaseIDs[t] + id);
    }
}

BandStrip::~BandStrip()
{
    juce::String id = juce::String(mBandIndex);
    mApvts.removeParameterListener("grainFreeze" + id, this);
    mApvts.removeParameterListener("bandEnabled" + id, this);
}

void BandStrip::parameterChanged(const juce::String& paramID, float newValue)
{
    if (paramID.contains("grainFreeze"))      mFrozen = newValue > 0.5f;
    else if (paramID.contains("bandEnabled")) mEnabled = newValue > 0.5f;
    juce::MessageManager::callAsync([this] { repaint(); });
}

void BandStrip::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText,
    const juce::String& paramId, juce::AudioProcessorValueTreeState& apvts,
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
    int knobSize)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSize + 6, 13);
    s.getProperties().set("accentColour", (int)mColour.getARGB());
    s.setColour(juce::Slider::textBoxTextColourId, mColour);
    s.setColour(juce::Slider::textBoxBackgroundColourId, bgMid);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(labelText.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, dimText);
    addAndMakeVisible(l);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, s);
    attachMidiListener(s, paramId);
}

void BandStrip::setupButton(juce::TextButton& btn, const juce::String& label,
    const juce::String& paramId, juce::AudioProcessorValueTreeState& apvts,
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att)
{
    btn.setButtonText(label);
    btn.setClickingTogglesState(true);
    btn.setColour(juce::TextButton::buttonColourId, bgPanel);
    btn.setColour(juce::TextButton::buttonOnColourId, mColour.withAlpha(0.8f));
    btn.setColour(juce::TextButton::textColourOffId, dimText);
    btn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible(btn);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramId, btn);
}

void BandStrip::attachMidiListener(juce::Slider& s, const juce::String& paramID)
{
    if (mMidiLearn == nullptr) return;
    auto listener = std::make_unique<KnobRightClickListener>(paramID, *mMidiLearn);
    s.addMouseListener(listener.get(), false);
    mKnobParams.push_back({ &s, paramID });
    mListeners.push_back(std::move(listener));
}

void BandStrip::repaintKnobs()
{
    if (mMidiLearn == nullptr) return;
    juce::String learningParam = mMidiLearn->getLearningParamID();

    for (auto& [slider, paramID] : mKnobParams)
    {
        int  cc = mMidiLearn->getCCForParam(paramID);
        bool isLearning = (learningParam == paramID);
        slider->getProperties().set("midiCC", cc);
        slider->getProperties().set("midiLearning", isLearning);
        slider->repaint();
    }
}

//==============================================================================
// Collapse layout helpers
//==============================================================================
int BandStrip::coreHeight()
{
    // name(22)+gap(4) + filter(cellH)+gap(4) + grain1(cellH)+gap(4)
    //  + grain2(cellH)+gap(4) + btns(22)+gap(10) + div(18), plus the top inset.
    return kStripInsetTop + (22 + 4 + kCellH + 4 + kCellH + 4 + kCellH + 4 + 22 + 10 + 18);
}

int BandStrip::gateHeight()
{
    // label(14)+gap(4) + topRow(22)+gap(4) + knobRow(cellH)+gap(4)
    return 14 + 4 + 22 + 4 + kCellH + 4;
}

int BandStrip::lfoHeight()
{
    // label(14)+gap(4) + shape(30)+gap(4) + row1(20)+gap(3) + row2(18)+gap(6)
    //  + hdr(12)+gap(2) + 7 target rows of (lfoRowH + 1)
    return 14 + 4 + 30 + 4 + 20 + 3 + 18 + 6 + 12 + 2 + 7 * (kLfoRowH + 1);
}

int BandStrip::totalHeight(bool gateExpanded, bool lfoExpanded)
{
    // The LFO bar is always the last element. Only pad below it when the LFO
    // block is expanded; when collapsed the strip ends flush with the bar so no
    // sliver of panel shows beneath it.
    int h = coreHeight()
        + kBarH + (gateExpanded ? gateHeight() : 0)
        + kBarH;
    if (lfoExpanded) h += lfoHeight() + kStripInsetBottom;
    return h;
}

void BandStrip::setGateChildrenVisible(bool v)
{
    lGateSection.setVisible(v);
    btnGate.setVisible(v);
    cbGateDiv.setVisible(v);
    lGateDiv.setVisible(v);
    sGateDepth.setVisible(v);   lGateDepth.setVisible(v);
    sGateAttack.setVisible(v);  lGateAttack.setVisible(v);
    sGateRelease.setVisible(v); lGateRelease.setVisible(v);
    sGatePhase.setVisible(v);   lGatePhase.setVisible(v);
}

void BandStrip::setLfoChildrenVisible(bool v)
{
    lLfoSection.setVisible(v);
    if (mLfoShapeDisplay) mLfoShapeDisplay->setVisible(v);
    sLfoRate.setVisible(v);  lLfoRate.setVisible(v);
    btnLfoOn.setVisible(v);  btnLfoSync.setVisible(v);
    cbLfoShape.setVisible(v); cbLfoDiv.setVisible(v); lLfoDiv.setVisible(v);
    lLfoColDepth.setVisible(v); lLfoColPhase.setVisible(v);
    for (int t = 0; t < 6; ++t)
    {
        lLfoTarget[t].setVisible(v);
        sLfoDepth[t].setVisible(v);
        sLfoPhase[t].setVisible(v);
    }
}

void BandStrip::setGateExpanded(bool e)
{
    mGateExpanded = e;
    setGateChildrenVisible(e);
}

void BandStrip::setLfoExpanded(bool e)
{
    mLfoExpanded = e;
    setLfoChildrenVisible(e);
}

void BandStrip::paint(juce::Graphics& g)
{
    auto  bounds = getLocalBounds().toFloat();
    float dim = mEnabled ? 1.f : 0.35f;

    g.setColour(bgPanel.withAlpha(dim));
    g.fillRoundedRectangle(bounds.reduced(2), 4.f);

    if (mFrozen && mEnabled)
    {
        g.setColour(mColour.withAlpha(0.10f));
        g.fillRoundedRectangle(bounds.reduced(2), 4.f);
    }

    g.setColour(mEnabled ? (mFrozen ? mColour.brighter(0.4f) : mColour)
        : mColour.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.reduced(2).removeFromTop(3), 2.f);

    auto drawSection = [&](juce::Rectangle<int> r) {
        if (r.isEmpty()) return;
        g.setColour(sectionBg.withAlpha(dim));
        g.fillRoundedRectangle(r.toFloat(), 4.f);
        g.setColour(sectionBorder.withAlpha(dim));
        g.drawRoundedRectangle(r.toFloat(), 4.f, 0.5f);
        };
    drawSection(mGateBounds);
    drawSection(mLfoBounds);
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced(6);
    area.removeFromTop(6);

    const int knobSz = kKnobSz;
    const int lblH = kLblH;
    const int sliderH = kSliderH;
    const int cellH = kCellH;
    const int lfoRowH = kLfoRowH;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> cell) {
        int cx = cell.getCentreX();
        s.setBounds(cx - knobSz / 2, cell.getY(), knobSz, sliderH);
        l.setBounds(cell.getX(), cell.getY() + sliderH + 2, cell.getWidth(), lblH);
        };

    int w2 = area.getWidth() / 2;
    int w4 = area.getWidth() / 4;

    // Band name + PWR
    auto nameRow = area.removeFromTop(22);
    btnEnabled.setBounds(nameRow.removeFromRight(40).reduced(0, 2));
    lBandName.setBounds(nameRow);
    area.removeFromTop(4);

    // Filter knobs
    auto filterRow = area.removeFromTop(cellH);
    placeKnob(sBandCenter, lBandCenter, filterRow.removeFromLeft(w2));
    placeKnob(sBandWidth, lBandWidth, filterRow);
    area.removeFromTop(4);

    // Grain row 1: Size Scatter Pitch Spread
    auto row1 = area.removeFromTop(cellH);
    placeKnob(sSize, lSize, row1.removeFromLeft(w4));
    placeKnob(sScatter, lScatter, row1.removeFromLeft(w4));
    placeKnob(sPitch, lPitch, row1.removeFromLeft(w4));
    placeKnob(sPitchSpread, lPitchSpread, row1);
    area.removeFromTop(4);

    // Grain row 2: Drive Mix Gain Feedback
    auto row2 = area.removeFromTop(cellH);
    placeKnob(sDrive, lDrive, row2.removeFromLeft(w4));
    placeKnob(sMix, lMix, row2.removeFromLeft(w4));
    placeKnob(sGain, lGain, row2.removeFromLeft(w4));
    placeKnob(sFeedback, lFeedback, row2);
    area.removeFromTop(4);

    // REV FRZ SYNC
    auto btnRow = area.removeFromTop(22);
    int bw = btnRow.getWidth() / 3;
    btnReverse.setBounds(btnRow.removeFromLeft(bw).reduced(2, 0));
    btnFreeze.setBounds(btnRow.removeFromLeft(bw).reduced(2, 0));
    btnSync.setBounds(btnRow.reduced(2, 0));
    area.removeFromTop(10);

    // Grain DIV
    auto divRow = area.removeFromTop(18);
    lDiv.setBounds(divRow.removeFromLeft(28));
    cbDiv.setBounds(divRow);

    // ── GATE bar gap (editor draws the full-width GATE toggle bar here) ──
    area.removeFromTop(kBarH);

    if (mGateExpanded)
    {
        int gateSectionTop = area.getY();
        lGateSection.setBounds(area.removeFromTop(14));
        area.removeFromTop(4);

        auto gateTopRow = area.removeFromTop(22);
        btnGate.setBounds(gateTopRow.removeFromLeft(44).reduced(0, 1));
        gateTopRow.removeFromLeft(4);
        lGateDiv.setBounds(gateTopRow.removeFromLeft(28));
        cbGateDiv.setBounds(gateTopRow);
        area.removeFromTop(4);

        auto gateKnobRow = area.removeFromTop(cellH);
        placeKnob(sGateDepth, lGateDepth, gateKnobRow.removeFromLeft(w4));
        placeKnob(sGateAttack, lGateAttack, gateKnobRow.removeFromLeft(w4));
        placeKnob(sGateRelease, lGateRelease, gateKnobRow.removeFromLeft(w4));
        placeKnob(sGatePhase, lGatePhase, gateKnobRow);
        area.removeFromTop(4);

        mGateBounds = juce::Rectangle<int>(
            getLocalBounds().reduced(6).getX() - 2,
            gateSectionTop - 4,
            getLocalBounds().reduced(6).getWidth() + 4,
            area.getY() - gateSectionTop + 4);
    }
    else
    {
        mGateBounds = {};
    }

    // ── LFO bar gap (editor draws the full-width LFO toggle bar here) ──
    area.removeFromTop(kBarH);

    if (mLfoExpanded)
    {
        int lfoSectionTop = area.getY();
        lLfoSection.setBounds(area.removeFromTop(14));
        area.removeFromTop(4);

        mLfoShapeDisplay->setBounds(area.removeFromTop(30));
        area.removeFromTop(4);

        // ON + SYNC + HZ + rate slider + shape combo
        auto lfoRow1 = area.removeFromTop(20);
        btnLfoOn.setBounds(lfoRow1.removeFromLeft(28).reduced(0, 1));
        lfoRow1.removeFromLeft(2);
        btnLfoSync.setBounds(lfoRow1.removeFromLeft(38).reduced(0, 1));
        lfoRow1.removeFromLeft(3);
        lLfoRate.setBounds(lfoRow1.removeFromLeft(18));
        sLfoRate.setBounds(lfoRow1.removeFromLeft(60));
        lfoRow1.removeFromLeft(3);
        cbLfoShape.setBounds(lfoRow1);
        area.removeFromTop(3);

        // DIV row
        auto lfoRow2 = area.removeFromTop(18);
        lLfoDiv.setBounds(lfoRow2.removeFromLeft(28));
        cbLfoDiv.setBounds(lfoRow2);
        area.removeFromTop(6);

        // Column layout: label | depth knob | phase knob
        const int labelColW = 42;
        const int knobColW = (area.getWidth() - labelColW) / 2;

        auto hdrRow = area.removeFromTop(12);
        hdrRow.removeFromLeft(labelColW);
        lLfoColDepth.setBounds(hdrRow.removeFromLeft(knobColW));
        lLfoColPhase.setBounds(hdrRow);
        area.removeFromTop(2);

        for (int t = 0; t < 7; ++t)
        {
            auto row = area.removeFromTop(lfoRowH);
            auto labelCell = row.removeFromLeft(labelColW);
            lLfoTarget[t].setBounds(labelCell.getX(),
                labelCell.getY() + 32 - (lblH / 2),
                labelColW, lblH);
            auto depthCol = row.removeFromLeft(knobColW);
            sLfoDepth[t].setBounds(depthCol.withSizeKeepingCentre(knobSz, sliderH));
            sLfoPhase[t].setBounds(row.withSizeKeepingCentre(knobSz, sliderH));
            area.removeFromTop(1);
        }

        mLfoBounds = juce::Rectangle<int>(
            getLocalBounds().reduced(6).getX() - 2,
            lfoSectionTop - 4,
            getLocalBounds().reduced(6).getWidth() + 4,
            area.getY() - lfoSectionTop + 4);
    }
    else
    {
        mLfoBounds = {};
    }
}

//==============================================================================
// Main Editor
//==============================================================================
juce::File GrainBrainAudioProcessorEditor::getMidiMapFile()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("GrainBrain").getChildFile("midimap.xml");
}

GrainBrainAudioProcessorEditor::GrainBrainAudioProcessorEditor(GrainBrainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), presetManager(p.apvts)
{
    setLookAndFeel(&laf);

    lBpm.setFont(juce::Font(juce::FontOptions("Courier New", 12.f, juce::Font::bold)));
    lBpm.setColour(juce::Label::textColourId, juce::Colour(0xFF00FF88));
    lBpm.setJustificationType(juce::Justification::centred);
    lBpm.setText("-- BPM", juce::dontSendNotification);
    addAndMakeVisible(lBpm);

    presetBox.setColour(juce::ComboBox::backgroundColourId, bgMid);
    presetBox.setColour(juce::ComboBox::textColourId, textColour);
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF444444));
    presetBox.setColour(juce::ComboBox::arrowColourId, textColour);
    presetBox.setTextWhenNothingSelected("-- no preset --");
    presetBox.onChange = [this] {
        auto name = presetBox.getText();
        if (name.isNotEmpty()) { presetManager.loadPreset(name); reloadCollapseStateFromTree(); }
        };
    addAndMakeVisible(presetBox);

    btnSave.setButtonText("SAVE");
    btnSave.setColour(juce::TextButton::buttonColourId, bgPanel);
    btnSave.setColour(juce::TextButton::textColourOffId, textColour);
    btnSave.onClick = [this] { savePreset(); };
    addAndMakeVisible(btnSave);

    btnReset.setButtonText("RESET");
    btnReset.setColour(juce::TextButton::buttonColourId, bgPanel);
    btnReset.setColour(juce::TextButton::textColourOffId, textColour);
    btnReset.onClick = [this] { resetAllParameters(); };
    addAndMakeVisible(btnReset);

    btnDelete.setButtonText("DEL");
    btnDelete.setColour(juce::TextButton::buttonColourId, bgPanel);
    btnDelete.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF4444));
    btnDelete.onClick = [this] { deletePreset(); };
    addAndMakeVisible(btnDelete);

    refreshPresetBox();

    setupHSlider(sDryIn, lDryIn, "DRY IN", "dryIn", aDryIn);
    setupHSlider(sPassthrough, lPassthrough, "PASS", "passthrough", aPassthrough);
    setupHSlider(sWetOut, lWetOut, "WET OUT", "wetOut", aWetOut);

    spectrumAnalyzer = std::make_unique<SpectrumAnalyzer>(audioProcessor, audioProcessor.apvts);
    addAndMakeVisible(*spectrumAnalyzer);

    for (int i = 0; i < 4; ++i)
    {
        bandStrips[i] = std::make_unique<BandStrip>(audioProcessor, i, audioProcessor.apvts);
        addAndMakeVisible(*bandStrips[i]);
    }

    // ── Collapse state (persisted in the APVTS state tree) ──
    auto& st = audioProcessor.apvts.state;
    mSpectrumExpanded = (int)st.getProperty("uiSpectrumExpanded", 1) != 0;
    mGateExpanded = (int)st.getProperty("uiGateExpanded", 1) != 0;
    mLfoExpanded = (int)st.getProperty("uiLfoExpanded", 1) != 0;

    spectrumToggle.showBar = false;
    spectrumToggle.expanded = mSpectrumExpanded;
    spectrumToggle.onToggle = [this] { mSpectrumExpanded = !mSpectrumExpanded; applyCollapseLayout(); };
    addAndMakeVisible(spectrumToggle);

    gateBar.label = "--- GATE ---";
    gateBar.expanded = mGateExpanded;
    gateBar.onToggle = [this] { mGateExpanded = !mGateExpanded; applyCollapseLayout(); };
    addAndMakeVisible(gateBar);

    lfoBar.label = "--- LFO ---";
    lfoBar.expanded = mLfoExpanded;
    lfoBar.onToggle = [this] { mLfoExpanded = !mLfoExpanded; applyCollapseLayout(); };
    addAndMakeVisible(lfoBar);

    for (auto& s : bandStrips)
    {
        s->setGateExpanded(mGateExpanded);
        s->setLfoExpanded(mLfoExpanded);
    }

    startTimerHz(30);   // 30Hz for smooth learn pulse animation
    setSize(900, computeWindowHeight());
}

GrainBrainAudioProcessorEditor::~GrainBrainAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GrainBrainAudioProcessorEditor::timerCallback()
{
    lBpm.setText(juce::String(audioProcessor.currentBPM, 1) + " BPM",
        juce::dontSendNotification);

    for (auto& strip : bandStrips)
        strip->repaintKnobs();

    if (audioProcessor.midiLearn.isDirty())
        audioProcessor.midiLearn.saveToFile(getMidiMapFile());
}

void GrainBrainAudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear(juce::dontSendNotification);
    auto names = presetManager.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem(names[i], i + 1);
}

void GrainBrainAudioProcessorEditor::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow>(
        "Save Preset", "Enter preset name:", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor("name", "", "Preset name:");
    saveDialog->addButton("Save", 1);
    saveDialog->addButton("Cancel", 0);
    saveDialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this](int result) {
            if (result == 1) {
                auto name = saveDialog->getTextEditorContents("name").trim();
                if (name.isNotEmpty()) {
                    presetManager.savePreset(name);
                    refreshPresetBox();
                    auto names = presetManager.getPresetNames();
                    int idx = names.indexOf(name);
                    if (idx >= 0) presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
                }
            }
            saveDialog.reset();
        }), true);
}

void GrainBrainAudioProcessorEditor::deletePreset()
{
    auto name = presetBox.getText();
    if (name.isEmpty()) return;
    deleteDialog = std::make_unique<juce::AlertWindow>(
        "Delete Preset", "Delete \"" + name + "\"?",
        juce::MessageBoxIconType::WarningIcon);
    deleteDialog->addButton("Delete", 1);
    deleteDialog->addButton("Cancel", 0);
    deleteDialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, name](int result) {
            if (result == 1) {
                presetManager.deletePreset(name);
                refreshPresetBox();
                presetBox.setSelectedId(0, juce::dontSendNotification);
            }
            deleteDialog.reset();
        }), true);
}

void GrainBrainAudioProcessorEditor::resetAllParameters()
{
    // Return every parameter to its registered default (normalised value).
    // APVTS attachments propagate the change back to all sliders/buttons/combos.
    for (auto* param : audioProcessor.getParameters())
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getDefaultValue());
        param->endChangeGesture();
    }
}

int GrainBrainAudioProcessorEditor::computeWindowHeight() const
{
    const int spectrumH = mSpectrumExpanded ? kSpectrumExpandedH : kSpectrumCondensedH;
    const int stripH = BandStrip::totalHeight(mGateExpanded, mLfoExpanded);
    // top bar (44) + bottom bar (44) + spectrum region + strip-area inset (8+8) + strips
    return 44 + 44 + spectrumH + 16 + stripH;
}

void GrainBrainAudioProcessorEditor::persistCollapseState()
{
    auto& st = audioProcessor.apvts.state;
    st.setProperty("uiSpectrumExpanded", mSpectrumExpanded ? 1 : 0, nullptr);
    st.setProperty("uiGateExpanded", mGateExpanded ? 1 : 0, nullptr);
    st.setProperty("uiLfoExpanded", mLfoExpanded ? 1 : 0, nullptr);
}

void GrainBrainAudioProcessorEditor::reloadCollapseStateFromTree()
{
    auto& st = audioProcessor.apvts.state;
    mSpectrumExpanded = (int)st.getProperty("uiSpectrumExpanded", mSpectrumExpanded ? 1 : 0) != 0;
    mGateExpanded = (int)st.getProperty("uiGateExpanded", mGateExpanded ? 1 : 0) != 0;
    mLfoExpanded = (int)st.getProperty("uiLfoExpanded", mLfoExpanded ? 1 : 0) != 0;
    applyCollapseLayout();
}

void GrainBrainAudioProcessorEditor::applyCollapseLayout()
{
    spectrumToggle.expanded = mSpectrumExpanded;
    gateBar.expanded = mGateExpanded;
    lfoBar.expanded = mLfoExpanded;
    spectrumToggle.repaint();
    gateBar.repaint();
    lfoBar.repaint();

    for (auto& s : bandStrips)
    {
        s->setGateExpanded(mGateExpanded);
        s->setLfoExpanded(mLfoExpanded);
    }

    persistCollapseState();

    const int h = computeWindowHeight();
    if (h != getHeight()) setSize(getWidth(), h);   // triggers resized()
    else                  resized();                // size unchanged → force relayout
    repaint();
}

void GrainBrainAudioProcessorEditor::setupHSlider(
    juce::Slider& s, juce::Label& l, const juce::String& labelText,
    const juce::String& paramId,
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    s.setColour(juce::Slider::trackColourId, juce::Colour(0xFF444444));
    s.setColour(juce::Slider::thumbColourId, textColour);
    s.setColour(juce::Slider::textBoxTextColourId, textColour);
    s.setColour(juce::Slider::textBoxBackgroundColourId, bgMid);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(labelText, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions("Courier New", 10.f, juce::Font::plain)));
    l.setColour(juce::Label::textColourId, dimText);
    l.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(l);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, s);
}

void GrainBrainAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bgDark);
    g.setColour(bgPanel);
    for (int x = 0; x < getWidth(); x += 40)
        g.drawVerticalLine(x, 0.f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 40)
        g.drawHorizontalLine(y, 0.f, (float)getHeight());
    g.setColour(bgMid);
    g.fillRect(0, 0, getWidth(), 44);
    g.fillRect(0, getHeight() - 44, getWidth(), 44);

    // Title logo — "GRAINBRAIN" with the B as the gold glint (Bunka-style glyph draw).
    // Courier is monospaced, so a GlyphArrangement gives a stable per-letter advance.
    juce::Font tf(juce::FontOptions("Courier New", 20.f, juce::Font::bold));
    g.setFont(tf);
    float chW = 12.f;
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(tf, "GRAINBRAIN", 0.f, 0.f);
        chW = ga.getBoundingBox(0, 10, true).getWidth() / 10.f;
    }
    float tx = 16.f;
    const float ty = 0.f, th = 44.f;
    auto drawSeg = [&](const juce::String& s, juce::Colour c) {
        g.setColour(c);
        g.drawText(s, juce::Rectangle<float>(tx, ty, chW * s.length() + 2.f, th),
            juce::Justification::centredLeft, false);
        tx += chW * s.length();
        };
    drawSeg("GRAIN", juce::Colours::white);
    drawSeg("B", bandColours[0]);   // gold glint
    drawSeg("RAIN", juce::Colours::white);

    // Branding, right side
    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    g.drawText("XNULLX", juce::Rectangle<int>(getWidth() - 90, 0, 80, 44),
        juce::Justification::centredRight, false);
}

void GrainBrainAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    area.removeFromTop(44);   // top bar (logo + XNULLX drawn in paint())
    {
        const int barY = 8, barH = 28;
        int x = 146;                                   // just past the GRAINBRAIN logo
        presetBox.setBounds(x, barY, 232, barH); x += 232 + 6;
        btnSave.setBounds(x, barY, 54, barH);    x += 54 + 4;
        btnDelete.setBounds(x, barY, 44, barH);  x += 44;

        // RESET on the right, just left of the XNULLX branding
        const int xnullxLeft = getWidth() - 90;
        const int resetW = 60;
        const int resetX = xnullxLeft - resetW - 6;
        btnReset.setBounds(resetX, barY, resetW, barH);

        // BPM centred in the gap between DEL and RESET
        lBpm.setBounds(x, barY, juce::jmax(10, resetX - 6 - x), barH);
    }

    auto bottomBar = area.removeFromBottom(44);
    bottomBar.reduce(8, 8);
    const int labelW = 52;
    const int sliderW = 210;
    const int gap = 12;

    auto dryArea = bottomBar.removeFromLeft(labelW + sliderW);
    lDryIn.setBounds(dryArea.removeFromLeft(labelW));
    sDryIn.setBounds(dryArea);
    bottomBar.removeFromLeft(gap);

    auto ptArea = bottomBar.removeFromLeft(labelW + sliderW);
    lPassthrough.setBounds(ptArea.removeFromLeft(labelW));
    sPassthrough.setBounds(ptArea);

    auto wetArea = bottomBar.removeFromRight(labelW + sliderW);
    lWetOut.setBounds(wetArea.removeFromLeft(labelW));
    sWetOut.setBounds(wetArea);

    int spectrumH = mSpectrumExpanded ? kSpectrumExpandedH : kSpectrumCondensedH;
    auto specArea = area.removeFromTop(spectrumH).reduced(8, 4);
    spectrumAnalyzer->setBounds(specArea);
    spectrumToggle.setBounds(specArea.getX() + 2, specArea.getY() + 2, 18, 16);

    area.reduce(8, 8);
    const int stripTopY = area.getY();
    const int stripW = area.getWidth() / 4;
    const int barLeft = area.getX();
    const int barWidth = area.getWidth();
    const int stripH = BandStrip::totalHeight(mGateExpanded, mLfoExpanded);

    auto stripsRow = area;
    for (int i = 0; i < 4; ++i)
    {
        auto c = stripsRow.removeFromLeft(stripW).reduced(3, 0);
        bandStrips[i]->setBounds(c.getX(), stripTopY, c.getWidth(), stripH);
    }

    // Full-width section toggle bars sit in the gaps the strips reserved.
    // Match the module panels' visible extent exactly: each strip is inset 3px
    // from its column and its panel is drawn at reduced(2), while the bar draws
    // its own outline at reduced(1) — so the bar bounds inset is 3 + 2 - 1 = 4.
    const int barInset = 4;
    const int gateBarY = stripTopY + BandStrip::coreHeight();
    const int lfoBarY = gateBarY + BandStrip::kBarH + (mGateExpanded ? BandStrip::gateHeight() : 0);
    gateBar.setBounds(barLeft + barInset, gateBarY, barWidth - 2 * barInset, BandStrip::kBarH);
    lfoBar.setBounds(barLeft + barInset, lfoBarY, barWidth - 2 * barInset, BandStrip::kBarH);
}

juce::AudioProcessorEditor* GrainBrainAudioProcessor::createEditor()
{
    return new GrainBrainAudioProcessorEditor(*this);
}