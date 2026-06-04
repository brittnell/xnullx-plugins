#include "PluginProcessor.h"
#include "PluginEditor.h"

static const int knobSz  = 43;
static const int valBoxH = 13;
static const int lblH    = 13;
static const int cellH   = knobSz + valBoxH + lblH + 4;  // 73px per knob row

//==============================================================================
// O2BandStrip
//==============================================================================
O2BandStrip::O2BandStrip(O2AudioProcessor& /*p*/, int bandIndex,
                         juce::AudioProcessorValueTreeState& apvts)
    : mBandIndex(bandIndex), mColour(o2BandColours[bandIndex]), mApvts(apvts)
{
    juce::String id = juce::String(bandIndex);
    apvts.addParameterListener("satType" + id, this);

    // Band name
    juce::StringArray names { "LOW", "MID", "HIGH" };
    lBandName.setText(names[bandIndex], juce::dontSendNotification);
    lBandName.setFont(juce::Font(juce::FontOptions("Courier New", 12.f, juce::Font::bold)));
    lBandName.setColour(juce::Label::textColourId, mColour);
    lBandName.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(lBandName);

    // ── Type ComboBox ─────────────────────────────────────────────────────
    for (int i = 0; i < o2TypeNames.size(); ++i) cbType.addItem(o2TypeNames[i], i + 1);
    cbType.setSelectedId(1, juce::dontSendNotification);
    cbType.setColour(juce::ComboBox::backgroundColourId, o2BgMid);
    cbType.setColour(juce::ComboBox::textColourId,       mColour);
    cbType.setColour(juce::ComboBox::outlineColourId,    mColour.withAlpha(0.4f));
    cbType.setColour(juce::ComboBox::arrowColourId,      mColour);
    addAndMakeVisible(cbType);
    aType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "satType" + id, cbType);

    lType.setText("TYPE", juce::dontSendNotification);
    lType.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    lType.setColour(juce::Label::textColourId, o2Dim);
    lType.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(lType);

    // ── Variant ComboBox ──────────────────────────────────────────────────
    int initialType = dynamic_cast<juce::AudioParameterChoice*>(
        apvts.getParameter("satType" + id))->getIndex();
    for (int i = 0; i < 4; ++i) cbVariant.addItem(o2VariantNames[initialType][i], i + 1);
    cbVariant.setSelectedId(1, juce::dontSendNotification);
    cbVariant.setColour(juce::ComboBox::backgroundColourId, o2BgMid);
    cbVariant.setColour(juce::ComboBox::textColourId,       mColour.withAlpha(0.85f));
    cbVariant.setColour(juce::ComboBox::outlineColourId,    mColour.withAlpha(0.3f));
    cbVariant.setColour(juce::ComboBox::arrowColourId,      mColour);
    addAndMakeVisible(cbVariant);
    aVariant = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "satVariant" + id, cbVariant);

    lVariant.setText("VARIANT", juce::dontSendNotification);
    lVariant.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    lVariant.setColour(juce::Label::textColourId, o2Dim);
    lVariant.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(lVariant);

    // ── Main knobs ────────────────────────────────────────────────────────
    setupKnob(sDrive, lDrive, "Drive",    "drive" + id, aDrive);
    setupKnob(sWidth, lWidth, "Width",    "width" + id, aWidth);
    setupKnob(sLevel, lLevel, "Level",    "level" + id, aLevel);

    // ── Secondary knobs ───────────────────────────────────────────────────
    setupKnob(sFeedback, lFeedback, "Feedback", "feedback" + id, aFeedback);
    setupKnob(sHiss,     lHiss,     "Hiss",     "hiss"     + id, aHiss);
    setupKnob(sDrift,    lDrift,    "Drift",    "drift"    + id, aDrift);

    // ── Buttons row 1 ─────────────────────────────────────────────────────
    setupButton(btnBypass,   "BYPASS", "bandBypass" + id, aBypass);
    setupButton(btnMute,     "MUTE",   "bandMute"   + id, aMute);

    // ── Buttons row 2 ─────────────────────────────────────────────────────
    setupButton(btnFlipPhase, "FLIP \xc3\xb8", "flipPhase" + id, aFlipPhase);
    setupButton(btnGateFbk,   "GATE FBK",      "gateFbk"   + id, aGateFbk);
}

O2BandStrip::~O2BandStrip()
{
    mApvts.removeParameterListener("satType" + juce::String(mBandIndex), this);
}

void O2BandStrip::parameterChanged(const juce::String&, float newValue)
{
    int typeIdx = (int)std::round(newValue);
    juce::MessageManager::callAsync([this, typeIdx] { repopulateVariantBox(typeIdx); });
}

void O2BandStrip::repopulateVariantBox(int typeIdx)
{
    juce::String id = juce::String(mBandIndex);
    int current = dynamic_cast<juce::AudioParameterChoice*>(
        mApvts.getParameter("satVariant" + id))->getIndex();
    cbVariant.clear(juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        cbVariant.addItem(o2VariantNames[typeIdx][i], i + 1);
    cbVariant.setSelectedId(current + 1, juce::dontSendNotification);
}

void O2BandStrip::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText,
                              const juce::String& paramId,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSz + 6, valBoxH);
    s.getProperties().set("accentColour", (int)mColour.getARGB());
    s.setColour(juce::Slider::textBoxTextColourId,       mColour);
    s.setColour(juce::Slider::textBoxBackgroundColourId, o2BgMid);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(labelText.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    l.setColour(juce::Label::textColourId, o2Dim);
    addAndMakeVisible(l);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(mApvts, paramId, s);
}

void O2BandStrip::setupButton(juce::TextButton& btn, const juce::String& label,
                               const juce::String& paramId,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att)
{
    btn.setButtonText(label);
    btn.setClickingTogglesState(true);
    btn.setColour(juce::TextButton::buttonColourId,   o2BgPanel);
    btn.setColour(juce::TextButton::buttonOnColourId, mColour.withAlpha(0.8f));
    btn.setColour(juce::TextButton::textColourOffId,  o2Dim);
    btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::black);
    addAndMakeVisible(btn);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(mApvts, paramId, btn);
}

void O2BandStrip::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(o2BgPanel);
    g.fillRoundedRectangle(b.reduced(2.f), 5.f);
    g.setColour(mColour);
    g.fillRoundedRectangle(b.reduced(2.f).removeFromTop(3.f), 2.f);
    g.setColour(mColour.withAlpha(0.18f));
    g.drawRoundedRectangle(b.reduced(2.f), 5.f, 0.8f);

    // Separator above secondary controls
    g.setColour(mColour.withAlpha(0.15f));
    g.drawHorizontalLine(mSeparatorY, (float)getLocalBounds().reduced(12).getX(),
                         (float)getLocalBounds().reduced(12).getRight());
}

void O2BandStrip::resized()
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(4);

    const int w = area.getWidth();

    // Helper: place 3 knobs side by side
    auto place3Knobs = [&](juce::Slider& s1, juce::Label& l1,
                           juce::Slider& s2, juce::Label& l2,
                           juce::Slider& s3, juce::Label& l3)
    {
        int colW = w / 3;
        auto row = area.removeFromTop(cellH);
        auto place = [&](juce::Rectangle<int> cell, juce::Slider& s, juce::Label& l) {
            int cx = cell.getCentreX();
            s.setBounds(cx - knobSz / 2, cell.getY(), knobSz, knobSz + valBoxH);
            l.setBounds(cell.getX(), cell.getY() + knobSz + valBoxH + 2, cell.getWidth(), lblH);
        };
        place(row.removeFromLeft(colW), s1, l1);
        place(row.removeFromLeft(colW), s2, l2);
        place(row, s3, l3);
    };

    // Band name (left-aligned)
    lBandName.setBounds(area.removeFromTop(18));
    area.removeFromTop(2);

    // Type
    lType.setBounds(area.removeFromTop(11));
    cbType.setBounds(area.removeFromTop(20));
    area.removeFromTop(3);

    // Variant
    lVariant.setBounds(area.removeFromTop(11));
    cbVariant.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    // Main knobs row: DRIVE | WIDTH | LEVEL
    place3Knobs(sDrive, lDrive, sWidth, lWidth, sLevel, lLevel);
    area.removeFromTop(6);

    // Separator
    mSeparatorY = area.getY();
    area.removeFromTop(8);

    // Secondary knobs row: FEEDBACK | HISS | DRIFT
    place3Knobs(sFeedback, lFeedback, sHiss, lHiss, sDrift, lDrift);
    area.removeFromTop(6);

    // Button row 1: BYPASS | MUTE
    auto btnRow1 = area.removeFromTop(22);
    int bw = (btnRow1.getWidth() - 4) / 2;
    btnBypass.setBounds(btnRow1.removeFromLeft(bw));
    btnRow1.removeFromLeft(4);
    btnMute.setBounds(btnRow1);
    area.removeFromTop(4);

    // Button row 2: FLIP PHASE | GATE FBK
    auto btnRow2 = area.removeFromTop(22);
    btnFlipPhase.setBounds(btnRow2.removeFromLeft(bw));
    btnRow2.removeFromLeft(4);
    btnGateFbk.setBounds(btnRow2);
}

//==============================================================================
// O2AudioProcessorEditor
//==============================================================================
O2AudioProcessorEditor::O2AudioProcessorEditor(O2AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), presetManager(p.apvts)
{
    setLookAndFeel(&laf);

    // ── Presets ───────────────────────────────────────────────────────────
    presetBox.setColour(juce::ComboBox::backgroundColourId, o2BgMid);
    presetBox.setColour(juce::ComboBox::textColourId,       o2Text);
    presetBox.setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xFF444444));
    presetBox.setColour(juce::ComboBox::arrowColourId,      o2Text);
    presetBox.setTextWhenNothingSelected("-- no preset --");
    presetBox.onChange = [this] {
        auto name = presetBox.getText();
        if (name.isNotEmpty()) presetManager.loadPreset(name);
    };
    addAndMakeVisible(presetBox);

    btnSave.setButtonText("SAVE");
    btnSave.setColour(juce::TextButton::buttonColourId,  o2BgPanel);
    btnSave.setColour(juce::TextButton::textColourOffId, o2Text);
    btnSave.onClick = [this] { savePreset(); };
    addAndMakeVisible(btnSave);

    btnReset.setButtonText("RESET");
    btnReset.setColour(juce::TextButton::buttonColourId,  o2BgPanel);
    btnReset.setColour(juce::TextButton::textColourOffId, o2Text);
    btnReset.onClick = [this] { resetAllParameters(); };
    addAndMakeVisible(btnReset);

    btnDelete.setButtonText("DEL");
    btnDelete.setColour(juce::TextButton::buttonColourId,  o2BgPanel);
    btnDelete.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF4444));
    btnDelete.onClick = [this] { deletePreset(); };
    addAndMakeVisible(btnDelete);

    refreshPresetBox();

    // ── Global knobs ──────────────────────────────────────────────────────
    setupKnob(sInputTrim,   lInputTrim,   "In Trim",  "inputTrim",   aInputTrim);
    setupKnob(sLowXover,    lLowXover,    "Lo X-Over","lowXover",    aLowXover);
    setupKnob(sHighXover,   lHighXover,   "Hi X-Over","highXover",   aHighXover);
    setupKnob(sOutputLevel, lOutputLevel, "Out Trim", "outputLevel", aOutputLevel);
    setupKnob(sDryWet,      lDryWet,      "Dry/Wet",  "dryWet",      aDryWet);

    // ── Global toggles ────────────────────────────────────────────────────
    auto setupToggle = [&](juce::TextButton& btn, const juce::String& label,
                           const juce::String& paramId,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att)
    {
        btn.setButtonText(label);
        btn.setClickingTogglesState(true);
        btn.setColour(juce::TextButton::buttonColourId,   o2BgPanel);
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00CCFF).withAlpha(0.8f));
        btn.setColour(juce::TextButton::textColourOffId,  o2Dim);
        btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::black);
        addAndMakeVisible(btn);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, paramId, btn);
    };

    setupToggle(btnMono,       "MONO",   "mono",         aMono);
    setupToggle(btnBypass,     "BYPASS", "masterBypass", aMasterBypass);
    setupToggle(btnOversample, "4x OS",  "oversample",   aOversample);
    setupToggle(btnWiden,      "WIDEN",  "widen",        aWiden);

    // ── Meters ────────────────────────────────────────────────────────────
    addAndMakeVisible(meterIn);
    addAndMakeVisible(meterOut);

    auto setupMeterLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
        l.setColour(juce::Label::textColourId, o2Dim);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };
    setupMeterLabel(lMeterIn,  "IN");
    setupMeterLabel(lMeterOut, "OUT");

    // ── Band strips ───────────────────────────────────────────────────────
    for (int i = 0; i < 3; ++i)
    {
        bandStrips[i] = std::make_unique<O2BandStrip>(audioProcessor, i, audioProcessor.apvts);
        addAndMakeVisible(*bandStrips[i]);
    }

    startTimerHz(30);
    setSize(680, 485);
}

O2AudioProcessorEditor::~O2AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void O2AudioProcessorEditor::timerCallback()
{
    meterIn .setPeak(audioProcessor.inputPeak .load());
    meterOut.setPeak(audioProcessor.outputPeak.load());
}

//==============================================================================
void O2AudioProcessorEditor::setupKnob(juce::Slider& s, juce::Label& l,
                                        const juce::String& labelText,
                                        const juce::String& paramId,
                                        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSz + 6, valBoxH);
    s.getProperties().set("accentColour", (int)juce::Colour(0xFF00CCFF).getARGB());
    s.setColour(juce::Slider::textBoxTextColourId,       o2Text);
    s.setColour(juce::Slider::textBoxBackgroundColourId, o2BgMid);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(labelText.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    l.setColour(juce::Label::textColourId, o2Dim);
    addAndMakeVisible(l);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, s);
}

//==============================================================================
void O2AudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear(juce::dontSendNotification);
    auto names = presetManager.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem(names[i], i + 1);
}

void O2AudioProcessorEditor::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow>(
        "Save Preset", "Enter preset name:", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor("name", "", "Preset name:");
    saveDialog->addButton("Save",   1);
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

void O2AudioProcessorEditor::deletePreset()
{
    auto name = presetBox.getText();
    if (name.isEmpty()) return;
    deleteDialog = std::make_unique<juce::AlertWindow>(
        "Delete Preset", "Delete \"" + name + "\"?", juce::MessageBoxIconType::WarningIcon);
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

//==============================================================================
void O2AudioProcessorEditor::resetAllParameters()
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

//==============================================================================
void O2AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(o2BgDark);

    g.setColour(o2BgPanel.withAlpha(0.6f));
    for (int x = 0; x < getWidth();  x += 40) g.drawVerticalLine  (x, 0.f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 40) g.drawHorizontalLine(y, 0.f, (float)getWidth());

    g.setColour(o2BgMid);
    g.fillRect(0, 0, getWidth(), 44);
    g.fillRect(0, getHeight() - 90, getWidth(), 90);

    // Three-colour accent line under top bar
    float w3 = (float)getWidth() / 3.f;
    for (int i = 0; i < 3; ++i)
    {
        g.setColour(o2BandColours[i].withAlpha(0.7f));
        g.fillRect((int)(i * w3), 43, (int)w3 + 1, 2);
    }

    // Title logo — "O2" with the 2 as the mint-green glint (family glyph draw)
    juce::Font tf(juce::FontOptions("Courier New", 20.f, juce::Font::bold));
    g.setFont(tf);
    float chW = 12.f;
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(tf, "O2", 0.f, 0.f);
        chW = ga.getBoundingBox(0, 2, true).getWidth() / 2.f;
    }
    float tx = 16.f;
    const float ty = 0.f, th = 44.f;
    auto drawSeg = [&](const juce::String& s, juce::Colour c) {
        g.setColour(c);
        g.drawText(s, juce::Rectangle<float>(tx, ty, chW * s.length() + 2.f, th),
                   juce::Justification::centredLeft, false);
        tx += chW * s.length();
    };
    drawSeg("O", juce::Colours::white);
    drawSeg("2", o2BandColours[1]);   // mint-green glint

    // Branding, right side
    g.setColour(o2Dim);
    g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    g.drawText("XNULLX", juce::Rectangle<int>(getWidth() - 90, 0, 80, 44),
               juce::Justification::centredRight, false);
}

//==============================================================================
void O2AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // ── Top bar (logo + XNULLX drawn in paint()) ──────────────────────────
    area.removeFromTop(44);
    {
        const int barY = 8, barH = 28;
        int x = 64;                                  // just past the O2 logo
        presetBox.setBounds(x, barY, 232, barH); x += 232 + 6;
        btnSave.setBounds(x, barY, 54, barH);    x += 54 + 4;
        btnDelete.setBounds(x, barY, 44, barH);

        // RESET on the right, just left of the XNULLX branding
        const int xnullxLeft = getWidth() - 90;
        const int resetW = 60;
        btnReset.setBounds(xnullxLeft - resetW - 6, barY, resetW, barH);
    }

    // ── Bottom bar (90px — tall enough for knobs + labels) ────────────────
    auto bottomBar = area.removeFromBottom(90);
    bottomBar.reduce(12, 8);

    // Toggle buttons — left column, 2×2 grid
    auto btnArea = bottomBar.removeFromLeft(160);
    btnArea.removeFromTop(4);
    int bh = (btnArea.getHeight() - 4) / 2;
    auto topRow = btnArea.removeFromTop(bh);
    btnArea.removeFromTop(4);
    auto botRow = btnArea.removeFromTop(bh);
    int bw = topRow.getWidth() / 2;
    btnMono      .setBounds(topRow.removeFromLeft(bw).reduced(2, 1));
    btnBypass    .setBounds(topRow.reduced(2, 1));
    btnOversample.setBounds(botRow.removeFromLeft(bw).reduced(2, 1));
    btnWiden     .setBounds(botRow.reduced(2, 1));

    // Meters — right column
    auto meterArea = bottomBar.removeFromRight(80);
    auto mInArea   = meterArea.removeFromLeft(36);
    auto mOutArea  = meterArea;
    lMeterIn .setBounds(mInArea .removeFromBottom(12));
    meterIn  .setBounds(mInArea .reduced(4, 0));
    lMeterOut.setBounds(mOutArea.removeFromBottom(12));
    meterOut .setBounds(mOutArea.reduced(4, 0));

    // Global knobs — centre of bottom bar
    int globalKnobW = bottomBar.getWidth() / 5;

    auto placeGlobalKnob = [&](juce::Slider& s, juce::Label& l)
    {
        auto cell = bottomBar.removeFromLeft(globalKnobW);
        int  cx   = cell.getCentreX();
        int  topY = cell.getY() + (cell.getHeight() - (knobSz + valBoxH + lblH)) / 2;
        s.setBounds(cx - knobSz / 2, topY, knobSz, knobSz + valBoxH);
        l.setBounds(cell.getX(), topY + knobSz + valBoxH + 2, cell.getWidth(), lblH);
    };

    placeGlobalKnob(sInputTrim,   lInputTrim);
    placeGlobalKnob(sLowXover,    lLowXover);
    placeGlobalKnob(sHighXover,   lHighXover);
    placeGlobalKnob(sOutputLevel, lOutputLevel);
    placeGlobalKnob(sDryWet,      lDryWet);

    // ── Band strips ───────────────────────────────────────────────────────
    area.reduce(8, 8);
    int stripW = area.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        bandStrips[i]->setBounds(area.removeFromLeft(stripW).reduced(4, 0));
}

//==============================================================================
juce::AudioProcessorEditor* O2AudioProcessor::createEditor()
{
    return new O2AudioProcessorEditor(*this);
}
