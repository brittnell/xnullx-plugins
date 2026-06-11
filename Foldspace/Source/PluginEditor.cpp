#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FoldspaceAudioProcessorEditor::FoldspaceAudioProcessorEditor(FoldspaceAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(transfer);
    addAndMakeVisible(scope);

    // ---- top bar -------------------------------------------------------------
    presetBox.setTextWhenNothingSelected("-- no preset --");
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            presetMgr.loadPreset(presetBox.getText());
    };
    addAndMakeVisible(presetBox);

    saveB.setButtonText("SAVE");
    saveB.onClick = [this] { doSavePreset(); };
    addAndMakeVisible(saveB);

    delB.setButtonText("DEL");
    delB.setColour(juce::TextButton::textColourOffId, fsRed);
    delB.onClick = [this]
    {
        if (presetBox.getSelectedId() > 0)
        {
            presetMgr.deletePreset(presetBox.getText());
            refreshPresetList();
            presetBox.setSelectedId(0, juce::dontSendNotification);
        }
    };
    addAndMakeVisible(delB);

    resetB.setButtonText("RESET");
    resetB.onClick = [this] { doReset(); };
    addAndMakeVisible(resetB);

    pitchL.setJustificationType(juce::Justification::centred);
    pitchL.setColour(juce::Label::textColourId, fsBlue);
    addAndMakeVisible(pitchL);

    // ---- toggle styling helper -------------------------------------------------
    auto styleToggle = [this](juce::TextButton& b, const juce::String& text,
                              const juce::String& paramID, juce::Colour on,
                              std::unique_ptr<BA>& att)
    {
        b.setButtonText(text);
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonOnColourId, on.withAlpha(0.85f));
        b.setColour(juce::TextButton::textColourOnId,   juce::Colours::black);
        b.setColour(juce::TextButton::textColourOffId,  fsDim);
        addAndMakeVisible(b);
        att = std::make_unique<BA>(proc.apvts, paramID, b);
    };

    // ---- modulator strip (violet) ---------------------------------------------
    waveSlider.setAccent(fsViolet);
    waveSlider.setLabels({ "SINE", "TRI", "SAW", "SQR" });
    waveSlider.onChange = [this](int i) { setChoiceParam("oscWave", i); };
    addAndMakeVisible(waveSlider);

    styleKnob(ratioK,   "RATIO",  "oscRatio", fsViolet);
    styleKnob(fineK,    "FINE",   "oscFine",  fsViolet);
    styleKnob(srcOscK,  "OSC",    "srcOsc",   fsViolet);
    styleKnob(srcInK,   "INPUT",  "srcInput", fsViolet);
    styleKnob(modFoldK, "FOLD",   "modFold",  fsViolet);

    styleToggle(syncB, "SYNC", "oscSync", fsViolet, aSync);
    divBox.addItemList({ "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T" }, 1);
    addAndMakeVisible(divBox);
    aDiv = std::make_unique<CA>(proc.apvts, "oscDiv", divBox);

    // ---- PM strip (blue) --------------------------------------------------------
    styleKnob(indexK,   "INDEX",   "pmIndex",    fsBlue);
    styleKnob(carrierK, "CARRIER", "carrier",    fsBlue);
    styleKnob(envIdxK,  "ENV>IDX", "envToIndex", fsBlue);
    styleKnob(stereoK,  "STEREO",  "stereo",     fsBlue);
    styleKnob(atkK,     "ATK",     "envAtk",     fsBlue);
    styleKnob(relK,     "REL",     "envRel",     fsBlue);

    // ---- fold strip (spice) -----------------------------------------------------
    styleKnob(foldsK,   "FOLDS",   "folds",     fsSpice);
    styleKnob(shapeK,   "SHAPE",   "shape",     fsSpice);
    styleKnob(symK,     "SYM",     "symmetry",  fsSpice);
    styleKnob(toneK,    "TONE",    "tone",      fsSpice);
    styleKnob(envFoldK, "ENV>FLD", "envToFold", fsSpice);

    // ---- feedback strip (red) ---------------------------------------------------
    styleKnob(fbK,      "AMOUNT", "fbAmount", fsRed);
    styleKnob(fbFoldK,  "FOLD",   "fbFold",   fsRed);
    styleKnob(fbToneK,  "TONE",   "fbTone",   fsRed);
    styleKnob(driftK,   "DRIFT",  "drift",    fsRed);

    // ---- bottom bar ---------------------------------------------------------------
    pitchModeSlider.setAccent(fsBlue);
    pitchModeSlider.setLabels({ "TRACK", "MIDI", "FREE" });
    pitchModeSlider.onChange = [this](int i) { setChoiceParam("pitchMode", i); };
    addAndMakeVisible(pitchModeSlider);
    pitchModeL.setText("PITCH", juce::dontSendNotification);
    pitchModeL.setJustificationType(juce::Justification::centred);
    pitchModeL.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    addAndMakeVisible(pitchModeL);

    styleKnob(freeHzK, "FREE HZ", "freeHz", fsSpice);
    styleKnob(glideK,  "GLIDE",   "glide",  fsSpice);
    styleKnob(inK,     "IN",      "inTrim", fsSpice);
    styleKnob(mixK,    "DRY/WET", "dryWet", fsSpice);
    styleKnob(outK,    "OUT",     "outTrim", fsSpice);

    osSlider.setAccent(fsSpice);
    osSlider.setLabels({ "OFF", "2X", "4X" });
    osSlider.onChange = [this](int i) { setChoiceParam("oversample", i); };
    addAndMakeVisible(osSlider);
    osL.setText("OS", juce::dontSendNotification);
    osL.setJustificationType(juce::Justification::centred);
    osL.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    addAndMakeVisible(osL);

    styleToggle(bypassB, "BYPASS", "masterBypass", fsRed, aBypass);

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    inMeterL.setText("IN", juce::dontSendNotification);
    inMeterL.setJustificationType(juce::Justification::centred);
    inMeterL.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    addAndMakeVisible(inMeterL);
    outMeterL.setText("OUT", juce::dontSendNotification);
    outMeterL.setJustificationType(juce::Justification::centred);
    outMeterL.setFont(juce::Font(juce::FontOptions("Courier New", 8.f, juce::Font::plain)));
    addAndMakeVisible(outMeterL);

    refreshPresetList();
    startTimerHz(30);
    setSize(kW, kH);
}

FoldspaceAudioProcessorEditor::~FoldspaceAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void FoldspaceAudioProcessorEditor::styleKnob(Knob& k, const juce::String& label,
                                              const juce::String& paramID, juce::Colour accent)
{
    k.s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
    k.s.getProperties().set("accentColour", (int)accent.getARGB());
    k.s.setColour(juce::Slider::textBoxTextColourId,       accent);
    k.s.setColour(juce::Slider::textBoxBackgroundColourId, fsBgMid);
    k.s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible(k.s);

    k.l.setText(label, juce::dontSendNotification);
    k.l.setJustificationType(juce::Justification::centred);
    k.l.setColour(juce::Label::textColourId, fsDim);
    k.l.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    addAndMakeVisible(k.l);

    k.att = std::make_unique<SA>(proc.apvts, paramID, k.s);
}

void FoldspaceAudioProcessorEditor::placeKnob(Knob& k, int cellCentreX, int y)
{
    k.s.setBounds(cellCentreX - 32, y, 64, 57);
    k.l.setBounds(cellCentreX - 50, y + 57, 100, 13);
}

void FoldspaceAudioProcessorEditor::setChoiceParam(const juce::String& id, int idx)
{
    if (auto* p = proc.apvts.getParameter(id))
    {
        const int n = p->getNumSteps();
        p->beginChangeGesture();
        p->setValueNotifyingHost(n > 1 ? (float)idx / (float)(n - 1) : 0.f);
        p->endChangeGesture();
    }
}

int FoldspaceAudioProcessorEditor::getChoiceParam(const juce::String& id) const
{
    if (auto* p = proc.apvts.getParameter(id))
    {
        const int n = p->getNumSteps();
        return (int)std::lround(p->getValue() * (double)(n > 1 ? n - 1 : 1));
    }
    return 0;
}

//==============================================================================
void FoldspaceAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    int id = 1;
    for (auto& n : presetMgr.getPresetNames())
        presetBox.addItem(n, id++);
}

void FoldspaceAudioProcessorEditor::doSavePreset()
{
    auto* aw = new juce::AlertWindow("Save Preset", "Name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "");
    aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, aw](int res)
        {
            const juce::String name = aw->getTextEditorContents("name").trim();
            if (res == 1 && name.isNotEmpty())
            {
                presetMgr.savePreset(name);
                refreshPresetList();
                presetBox.setText(name, juce::dontSendNotification);
            }
            delete aw;
        }), false);
}

void FoldspaceAudioProcessorEditor::doReset()
{
    for (auto* p : proc.getParameters())
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->getDefaultValue());
        p->endChangeGesture();
    }
}

//==============================================================================
void FoldspaceAudioProcessorEditor::timerCallback()
{
    inMeter.pushLevel(proc.uiInPeak.load());
    outMeter.pushLevel(proc.uiOutPeak.load());
    transfer.repaint();
    scope.repaint();

    // step selectors follow the params (preset loads, automation)
    waveSlider.setIndex(getChoiceParam("oscWave"));
    pitchModeSlider.setIndex(getChoiceParam("pitchMode"));
    osSlider.setIndex(getChoiceParam("oversample"));

    // pitch readout
    const int   mode = getChoiceParam("pitchMode");
    const float hz   = proc.uiPitchHz.load();
    juce::String txt;
    if (mode == 0)
    {
        txt = "TRACK  " + juce::String(hz, 1) + " Hz";
        if (proc.uiPitchConf.load() < 0.4f) txt += "  ~";
    }
    else if (mode == 1)
    {
        const int note = (int)std::lround(69.0 + 12.0 * std::log2(juce::jmax(20.f, hz) / 440.0));
        txt = "MIDI  " + juce::MidiMessage::getMidiNoteName(note, true, true, 3)
            + "  " + juce::String(hz, 1) + " Hz";
    }
    else
        txt = "FREE  " + juce::String(hz, 1) + " Hz";
    pitchL.setText(txt, juce::dontSendNotification);
}

//==============================================================================
void FoldspaceAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(fsBgDark);

    // technical grid
    g.setColour(fsBgPanel.withAlpha(0.6f));
    for (int x = 0; x < getWidth();  x += 40) g.drawVerticalLine(x, 0.f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 40) g.drawHorizontalLine(y, 0.f, (float)getWidth());

    // top + bottom bars
    g.setColour(fsBgMid);
    g.fillRect(0, 0, getWidth(), kTopH);
    g.fillRect(0, kBotY - 4, getWidth(), getHeight() - (kBotY - 4));

    // title — FOLDSPACE with the S as the spice hinge
    juce::Font tf(juce::FontOptions("Courier New", 20.f, juce::Font::bold));
    g.setFont(tf);
    float chW = 12.f;
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(tf, "FOLDSPACE", 0.f, 0.f);
        chW = ga.getBoundingBox(0, 9, true).getWidth() / 9.f;
    }
    float tx = 16.f;
    auto drawSeg = [&](const juce::String& s, juce::Colour c)
    {
        g.setColour(c);
        g.drawText(s, juce::Rectangle<float>(tx, 0.f, chW * (float)s.length() + 2.f, (float)kTopH),
                   juce::Justification::centredLeft, false);
        tx += chW * (float)s.length();
    };
    drawSeg("FOLD", juce::Colours::white);
    drawSeg("S",    fsSpice);
    drawSeg("PACE", juce::Colours::white);

    // branding
    g.setColour(fsDim);
    g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::plain)));
    g.drawText("XNULLX", juce::Rectangle<int>(getWidth() - 90, 0, 80, kTopH),
               juce::Justification::centredRight, false);

    // 4-segment stage accent strip
    const juce::Colour stageCols[4] = { fsViolet, fsBlue, fsSpice, fsRed };
    const float w4 = (float)getWidth() / 4.f;
    for (int i = 0; i < 4; ++i)
    {
        g.setColour(stageCols[i].withAlpha(0.85f));
        g.fillRect((int)(i * w4), kTopH, (int)w4 + 1, kStripGap);
    }

    // stage strips
    const char* names[4] = { "MODULATOR", "PM", "FOLD", "FEEDBACK" };
    for (int i = 0; i < 4; ++i)
    {
        const int x0 = kStripX0 + i * kStripW;
        auto panel = juce::Rectangle<float>((float)x0, (float)kStripY,
                                            (float)kStripW, (float)kStripH).reduced(3.f);
        g.setColour(fsBgPanel);
        g.fillRoundedRectangle(panel, 4.f);
        g.setColour(stageCols[i]);
        g.fillRect(panel.getX(), panel.getY(), panel.getWidth(), 3.f);

        g.setFont(juce::Font(juce::FontOptions("Courier New", 11.f, juce::Font::bold)));
        g.drawText(names[i], (int)panel.getX() + 8, kStripY + 10, kStripW - 16, 14,
                   juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void FoldspaceAudioProcessorEditor::resized()
{
    // top bar
    presetBox.setBounds(140, 8, 200, 28);
    saveB.setBounds(346, 8, 54, 28);
    delB.setBounds(404, 8, 44, 28);
    resetB.setBounds(704, 8, 60, 28);
    pitchL.setBounds(456, 8, 240, 28);

    // viz strip
    transfer.setBounds(8, kVizY, 418, kVizH);
    scope.setBounds(434, kVizY, 418, kVizH);

    // strip rows (aligned across strips)
    const int rowY[3] = { kStripY + 66, kStripY + 139, kStripY + 212 };
    auto cellL = [](int x0) { return x0 + 54;  };
    auto cellR = [](int x0) { return x0 + 156; };

    // MODULATOR
    {
        const int x0 = kStripX0;
        waveSlider.setBounds(x0 + 8, kStripY + 26, kStripW - 16, 36);
        placeKnob(ratioK,   cellL(x0), rowY[0]);
        placeKnob(fineK,    cellR(x0), rowY[0]);
        placeKnob(srcOscK,  cellL(x0), rowY[1]);
        placeKnob(srcInK,   cellR(x0), rowY[1]);
        placeKnob(modFoldK, cellL(x0), rowY[2]);
        syncB.setBounds(x0 + 114, rowY[2] + 7, 84, 22);
        divBox.setBounds(x0 + 114, rowY[2] + 35, 84, 22);
    }

    // PM
    {
        const int x0 = kStripX0 + kStripW;
        placeKnob(indexK,   cellL(x0), rowY[0]);
        placeKnob(carrierK, cellR(x0), rowY[0]);
        placeKnob(envIdxK,  cellL(x0), rowY[1]);
        placeKnob(stereoK,  cellR(x0), rowY[1]);
        placeKnob(atkK,     cellL(x0), rowY[2]);
        placeKnob(relK,     cellR(x0), rowY[2]);
    }

    // FOLD
    {
        const int x0 = kStripX0 + 2 * kStripW;
        placeKnob(foldsK,   cellL(x0), rowY[0]);
        placeKnob(shapeK,   cellR(x0), rowY[0]);
        placeKnob(symK,     cellL(x0), rowY[1]);
        placeKnob(toneK,    cellR(x0), rowY[1]);
        placeKnob(envFoldK, cellL(x0), rowY[2]);
    }

    // FEEDBACK
    {
        const int x0 = kStripX0 + 3 * kStripW;
        placeKnob(fbK,     cellL(x0), rowY[0]);
        placeKnob(fbFoldK, cellR(x0), rowY[0]);
        placeKnob(fbToneK, cellL(x0), rowY[1]);
        placeKnob(driftK,  cellR(x0), rowY[1]);
    }

    // bottom bar
    pitchModeSlider.setBounds(12, kBotY + 8, 140, 40);
    pitchModeL.setBounds(12, kBotY + 50, 140, 13);
    placeKnob(freeHzK, 192, kBotY + 4);
    placeKnob(glideK,  262, kBotY + 4);
    placeKnob(inK,     332, kBotY + 4);
    placeKnob(mixK,    402, kBotY + 4);
    placeKnob(outK,    472, kBotY + 4);
    osSlider.setBounds(512, kBotY + 8, 100, 40);
    osL.setBounds(512, kBotY + 50, 100, 13);
    bypassB.setBounds(628, kBotY + 24, 64, 24);
    inMeter.setBounds(712, kBotY + 6, 12, 56);
    inMeterL.setBounds(700, kBotY + 63, 36, 12);
    outMeter.setBounds(752, kBotY + 6, 12, 56);
    outMeterL.setBounds(740, kBotY + 63, 36, 12);
}
