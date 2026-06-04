#include "PluginEditor.h"
#include <cmath>

//==============================================================================
BunkaAudioProcessorEditor::BunkaAudioProcessorEditor (BunkaAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // --- waveform / load ---------------------------------------------------
    addAndMakeVisible (waveform);
    waveform.onFileDropped  = [this] (const juce::File& f) { proc.loadFile (f); };
    waveform.onSlicesEdited = [this] (const std::vector<int>& s) { proc.setSlicesManual (s); };

    addAndMakeVisible (speakerB);
    speakerB.onClick = [this] { proc.triggerPreview(); };

    // editing snap: zero-cross by default; grid when SNAP GRID is engaged
    waveform.snapSample = [this] (int s) { return proc.snapSample (s, snapGridB.getToggleState()); };

    loadB.setButtonText ("LOAD");
    loadB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Load a drum loop", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile()) proc.loadFile (f);
            });
    };
    addAndMakeVisible (loadB);

    fileNameL.setJustificationType (juce::Justification::centredLeft);
    fileNameL.setColour (juce::Label::textColourId, bunkaDim);
    fileNameL.setFont (juce::Font (juce::FontOptions ("Courier New", 11.f, juce::Font::plain)));
    addAndMakeVisible (fileNameL);

    // --- top bar: presets --------------------------------------------------
    presetBox.setTextWhenNothingSelected ("-- no preset --");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && presetBox.getSelectedId() > 0)
            presetMgr.loadPreset (name);
    };
    addAndMakeVisible (presetBox);
    refreshPresetList();

    saveB.setButtonText ("SAVE");
    saveB.onClick = [this] { doSavePreset(); };
    addAndMakeVisible (saveB);

    delB.setButtonText ("DEL");
    delB.setColour (juce::TextButton::textColourOffId, bunkaSpark);
    delB.onClick = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty()) { presetMgr.deletePreset (name); refreshPresetList(); }
    };
    addAndMakeVisible (delB);

    bpmL.setJustificationType (juce::Justification::centred);
    bpmL.setColour (juce::Label::textColourId, bunkaSteel);
    bpmL.setFont (juce::Font (juce::FontOptions ("Courier New", 12.f, juce::Font::bold)));
    addAndMakeVisible (bpmL);

    // --- knobs -------------------------------------------------------------
    styleKnob (gainK,    gainL,    "GAIN",      "gain");
    styleKnob (pitchK,   pitchL,   "PITCH",     "pitch");
    styleKnob (attackK,  attackL,  "ATTACK",    "attack");
    styleKnob (releaseK, releaseL, "RELEASE",   "release");
    styleKnob (threshK,  threshL,  "THRESHOLD", "threshold");
    styleKnob (barsK,    barsL,    "BARS",      "bars");
    styleKnob (chanceK,  chanceL,  "CHANCE",    "chance");

    // --- slice mode toggle (Transient / Grid) -----------------------------
    sliceModeB.setClickingTogglesState (true);
    sliceModeB.setColour (juce::TextButton::buttonOnColourId, bunkaSpark.withAlpha (0.85f));
    sliceModeB.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
    sliceModeB.setColour (juce::TextButton::textColourOffId,  bunkaText);
    addAndMakeVisible (sliceModeB);
    aSliceMode = std::make_unique<BA> (proc.apvts, "sliceMode", sliceModeB);
    sliceModeB.setButtonText (sliceModeB.getToggleState() ? "GRID" : "TRANSIENT");

    // --- grid-div & snap-div step sliders ---------------------------------
    auto setupDivSlider = [this] (StepSlider& s, juce::Label& l, const juce::String& label,
                                  const juce::String& paramID)
    {
        s.setLabels (SliceEngine::divisionNames());
        s.onChange = [this, paramID] (int idx) { setChoiceParam (paramID, idx); };
        s.setIndex (getChoiceParam (paramID));
        addAndMakeVisible (s);
        l.setText (label, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, bunkaDim);
        l.setFont (juce::Font (juce::FontOptions ("Courier New", 9.f, juce::Font::plain)));
        addAndMakeVisible (l);
    };
    setupDivSlider (gridDivSlider, gridDivL, "GRID DIV", "gridDiv");
    setupDivSlider (snapDivSlider, snapDivL, "SNAP DIV", "snapDiv");

    // --- mode toggles ------------------------------------------------------
    styleToggle (pitchPreserveB, "PITCH PRESERVE", "pitchPreserve", bunkaSpark);
    styleToggle (reverseB,       "REVERSE",        "reverse",       bunkaSpark);
    styleToggle (hostSyncB,      "HOST SYNC",      "hostSync",      bunkaSpark);

    // editor-only toggle (no APVTS param): snap drags to the loop grid
    snapGridB.setButtonText ("SNAP GRID");
    snapGridB.setClickingTogglesState (true);
    snapGridB.setColour (juce::TextButton::buttonOnColourId, bunkaSpark.withAlpha (0.85f));
    snapGridB.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
    snapGridB.setColour (juce::TextButton::textColourOffId,  bunkaDim);
    addAndMakeVisible (snapGridB);

    // --- pattern bank ------------------------------------------------------
    patternsHeaderL.setText ("PATTERNS", juce::dontSendNotification);
    patternsHeaderL.setJustificationType (juce::Justification::centredLeft);
    patternsHeaderL.setColour (juce::Label::textColourId, bunkaSteel);
    patternsHeaderL.setFont (juce::Font (juce::FontOptions ("Courier New", 11.f, juce::Font::bold)));
    addAndMakeVisible (patternsHeaderL);

    for (int i = 0; i < 12; ++i)
    {
        const juce::Colour pc       = (i % 2 == 0) ? bunkaSpark : bunkaSteel;   // orange / blue / ...
        const juce::Colour lockCol  = (i % 2 == 0) ? bunkaSteel : bunkaSpark;   // opposite of the pad

        auto& p = patternPads[i];
        p.setButtonText (juce::String (i + 1));
        p.setColour (juce::TextButton::buttonOnColourId, pc.withAlpha (0.9f));  // active fill
        p.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        p.setColour (juce::TextButton::textColourOffId,  pc);                   // number in pad colour
        p.onClick = [this, i] { proc.launchPattern (i); };
        addAndMakeVisible (p);

        auto& lk = lockButtons[i];
        lk.setLockColour (lockCol); // lock = opposite colour (stays visible even when the pad is active)
        lk.onClick = [this, i] { proc.setPatternLocked (i, lockButtons[i].getToggleState()); };
        addAndMakeVisible (lk);     // sits on top of the pad's lower-left corner
    }

    shuffleB.setButtonText ("SHUFFLE");
    shuffleB.setColour (juce::TextButton::textColourOffId, bunkaText);
    shuffleB.setFlashColour (bunkaSteel);                       // blue wash
    shuffleB.onClick = [this] { proc.shufflePatterns(); shuffleB.flashNow(); };
    addAndMakeVisible (shuffleB);

    randomizeB.setButtonText ("RANDOMIZE");
    randomizeB.setColour (juce::TextButton::textColourOffId, bunkaText);
    randomizeB.setFlashColour (bunkaSpark);                     // orange wash
    randomizeB.onClick = [this] { proc.randomizePatterns(); randomizeB.flashNow(); };
    addAndMakeVisible (randomizeB);

    styleToggle (seqB, "SEQ", "seqOn", bunkaSteel);

    setSize (820, 620);
    startTimerHz (30);
}

BunkaAudioProcessorEditor::~BunkaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void BunkaAudioProcessorEditor::styleKnob (juce::Slider& s, juce::Label& lbl,
                                           const juce::String& label, const juce::String& paramID)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
    s.getProperties().set ("accentColour", (int) bunkaSteel.getARGB());
    s.setColour (juce::Slider::textBoxTextColourId,       bunkaSteel);
    s.setColour (juce::Slider::textBoxBackgroundColourId, bunkaBgMid);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    lbl.setText (label, juce::dontSendNotification);
    lbl.setJustificationType (juce::Justification::centred);
    lbl.setColour (juce::Label::textColourId, bunkaDim);
    lbl.setFont (juce::Font (juce::FontOptions ("Courier New", 9.f, juce::Font::plain)));
    addAndMakeVisible (lbl);

    auto att = std::make_unique<SA> (proc.apvts, paramID, s);
    if      (paramID == "gain")      aGain    = std::move (att);
    else if (paramID == "pitch")     aPitch   = std::move (att);
    else if (paramID == "attack")    aAttack  = std::move (att);
    else if (paramID == "release")   aRelease = std::move (att);
    else if (paramID == "threshold") aThresh  = std::move (att);
    else if (paramID == "bars")      aBars    = std::move (att);
    else if (paramID == "chance")    aChance  = std::move (att);

    // steel at default, spark once moved — at-a-glance "what's changed"
    s.onValueChange = [this, &s, paramID] { updateKnobAccent (s, paramID); };
    updateKnobAccent (s, paramID);
}

void BunkaAudioProcessorEditor::updateKnobAccent (juce::Slider& s, const juce::String& paramID)
{
    auto* p = proc.apvts.getParameter (paramID);
    const bool moved = (p != nullptr) && std::abs (p->getValue() - p->getDefaultValue()) > 1.0e-4f;
    const juce::Colour c = moved ? bunkaSpark : bunkaSteel;
    s.getProperties().set ("accentColour", (int) c.getARGB());
    s.setColour (juce::Slider::textBoxTextColourId, c);
    s.repaint();
}

void BunkaAudioProcessorEditor::setChoiceParam (const juce::String& id, int idx)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        const int n = p->getNumSteps();
        p->setValueNotifyingHost (n > 1 ? (float) idx / (float) (n - 1) : 0.f);
    }
}

int BunkaAudioProcessorEditor::getChoiceParam (const juce::String& id) const
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        const int n = p->getNumSteps();
        return (int) std::lround (p->getValue() * (double) (n > 1 ? n - 1 : 1));
    }
    return 0;
}

void BunkaAudioProcessorEditor::styleToggle (juce::TextButton& b, const juce::String& text,
                                             const juce::String& paramID, juce::Colour on)
{
    b.setButtonText (text);
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonOnColourId, on.withAlpha (0.85f));
    b.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
    b.setColour (juce::TextButton::textColourOffId,  bunkaDim);
    addAndMakeVisible (b);

    auto att = std::make_unique<BA> (proc.apvts, paramID, b);
    if      (paramID == "pitchPreserve") aPitchPreserve = std::move (att);
    else if (paramID == "reverse")       aReverse       = std::move (att);
    else if (paramID == "hostSync")      aHostSync      = std::move (att);
    else if (paramID == "seqOn")         aSeqOn         = std::move (att);
}

//==============================================================================
void BunkaAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    for (auto& n : presetMgr.getPresetNames())
        presetBox.addItem (n, id++);
}

void BunkaAudioProcessorEditor::doSavePreset()
{
    auto* aw = new juce::AlertWindow ("Save Preset", "Name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, aw] (int res)
        {
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (res == 1 && name.isNotEmpty())
            {
                presetMgr.savePreset (name);
                refreshPresetList();
                presetBox.setText (name, juce::dontSendNotification);
            }
            delete aw;
        }), false);
}

//==============================================================================
void BunkaAudioProcessorEditor::timerCallback()
{
    // BPM readout: host tempo + the loop's own derived tempo
    bpmL.setText (juce::String (proc.getCurrentBpm(), 1) + " BPM",
                  juce::dontSendNotification);

    // refresh waveform when the processor reports a new version
    if (proc.getWaveformVersion() != lastWaveformVersion)
    {
        lastWaveformVersion = proc.getWaveformVersion();
        std::vector<float> mm; std::vector<int> slices; int nS = 1; double bpm = 120.0;
        proc.getWaveformSnapshot (mm, slices, nS, bpm);
        waveform.setThumbnail (std::move (mm), std::move (slices), nS);
        fileNameL.setText (proc.getLoadedFileName().isEmpty()
                               ? juce::String ("no loop loaded")
                               : proc.getLoadedFileName()
                                   + "   |   " + juce::String (proc.getNumSlices()) + " slices",
                           juce::dontSendNotification);
    }

    waveform.setActiveSlice (proc.lastTriggeredSlice.load());

    const int act = proc.getActivePattern();
    for (int i = 0; i < 12; ++i)
    {
        patternPads[i].setToggleState (i == act, juce::dontSendNotification);
        lockButtons[i].setToggleState (proc.isPatternLocked (i), juce::dontSendNotification);
    }

    sliceModeB.setButtonText (sliceModeB.getToggleState() ? "GRID" : "TRANSIENT");
    gridDivSlider.setIndex (getChoiceParam ("gridDiv"));
    snapDivSlider.setIndex (getChoiceParam ("snapDiv"));
}

//==============================================================================
void BunkaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bunkaBgDark);

    // technical grid
    g.setColour (bunkaBgPanel.withAlpha (0.6f));
    for (int x = 0; x < getWidth();  x += 40) g.drawVerticalLine   (x, 0.f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += 40) g.drawHorizontalLine (y, 0.f, (float) getWidth());

    // top bar
    g.setColour (bunkaBgMid);
    g.fillRect (0, 0, getWidth(), 44);

    // title — "BUNKA" with the K as the spark glint. Courier is monospaced,
    // so a GlyphArrangement gives a stable per-letter advance without relying
    // on the deprecated Font::getStringWidth* helpers.
    juce::Font tf (juce::FontOptions ("Courier New", 20.f, juce::Font::bold));
    g.setFont (tf);
    float chW = 12.f;
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (tf, "BUNKA", 0.f, 0.f);
        chW = ga.getBoundingBox (0, 5, true).getWidth() / 5.f;
    }
    float tx = 16.f;
    const float ty = 0.f, th = 44.f;
    auto drawSeg = [&] (const juce::String& s, juce::Colour c)
    {
        g.setColour (c);
        g.drawText (s, juce::Rectangle<float> (tx, ty, chW * s.length() + 2.f, th),
                    juce::Justification::centredLeft, false);
        tx += chW * s.length();
    };
    drawSeg ("BUN", juce::Colours::white);
    drawSeg ("K",   bunkaSpark);
    drawSeg ("A",   juce::Colours::white);

    // branding, right side
    g.setColour (bunkaDim);
    g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.f, juce::Font::plain)));
    g.drawText ("XNULLX", juce::Rectangle<int> (getWidth() - 90, 0, 80, 44),
                juce::Justification::centredRight, false);

    // signature steel -> spark accent strip
    juce::ColourGradient grad (bunkaSteelDim, 0.f, 0.f, bunkaSpark, (float) getWidth(), 0.f, false);
    grad.addColour (0.7, bunkaSteel);
    g.setGradientFill (grad);
    g.fillRect (0, 44, getWidth(), 3);

    // section divider label above the controls
    g.setColour (bunkaDim);
    g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.f, juce::Font::plain)));
    g.drawText ("PATTERN TRIGGERS  MIDI 36-47   |   SLICE TRIGGERS  MIDI 48+",
                16, getHeight() - 20, getWidth() - 32, 14, juce::Justification::centredLeft, false);
}

//==============================================================================
void BunkaAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (47);                       // top bar + accent strip

    // top bar contents
    {
        const int barY = 8, barH = 28;
        int x = 124;                                       // just past the BUNKA title
        presetBox.setBounds (x, barY, 232, barH);  x += 232 + 6;
        saveB.setBounds     (x, barY, 54,  barH);  x += 54 + 4;
        delB.setBounds      (x, barY, 44,  barH);  x += 44;        // x = DEL right edge
        const int logoLeft = getWidth() - 90;                      // XNULLX paint-box left
        bpmL.setBounds (x, barY, juce::jmax (10, logoLeft - x), barH);  // centred in the gap
    }

    area.reduce (16, 8);

    // waveform + load row
    auto wf = area.removeFromTop (150);
    waveform.setBounds (wf);

    auto loadRow = area.removeFromTop (28);
    loadB.setBounds (loadRow.removeFromLeft (80).reduced (0, 3));
    loadRow.removeFromLeft (6);
    speakerB.setBounds (loadRow.removeFromLeft (28).reduced (0, 1));
    loadRow.removeFromLeft (10);
    fileNameL.setBounds (loadRow);

    area.removeFromTop (16);

    // knob row (7 knobs)
    auto knobRow = area.removeFromTop (92);
    const int kW = knobRow.getWidth() / 7;
    auto place = [&] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> cell)
    {
        auto lblArea = cell.removeFromBottom (14);
        l.setBounds (lblArea);
        s.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 78), cell.getHeight()));
    };
    place (gainK,    gainL,    knobRow.removeFromLeft (kW));
    place (pitchK,   pitchL,   knobRow.removeFromLeft (kW));
    place (attackK,  attackL,  knobRow.removeFromLeft (kW));
    place (releaseK, releaseL, knobRow.removeFromLeft (kW));
    place (threshK,  threshL,  knobRow.removeFromLeft (kW));
    place (barsK,    barsL,    knobRow.removeFromLeft (kW));
    place (chanceK,  chanceL,  knobRow.removeFromLeft (kW));

    area.removeFromTop (14);

    // slice row: SLICE MODE toggle + GRID DIV step-slider
    auto sliceRow = area.removeFromTop (40);
    sliceModeB.setBounds (sliceRow.removeFromLeft (130).reduced (4, 6));
    sliceRow.removeFromLeft (6);
    gridDivL.setBounds (sliceRow.removeFromLeft (66));
    gridDivSlider.setBounds (sliceRow.reduced (2, 0));

    area.removeFromTop (6);

    // snap row: SNAP GRID toggle + SNAP DIV step-slider
    auto snapRow = area.removeFromTop (40);
    snapGridB.setBounds (snapRow.removeFromLeft (130).reduced (4, 6));
    snapRow.removeFromLeft (6);
    snapDivL.setBounds (snapRow.removeFromLeft (66));
    snapDivSlider.setBounds (snapRow.reduced (2, 0));

    area.removeFromTop (8);

    // playback toggles row
    auto togRow = area.removeFromTop (28);
    const int tW = togRow.getWidth() / 3;
    pitchPreserveB.setBounds (togRow.removeFromLeft (tW).reduced (4, 2));
    reverseB.setBounds       (togRow.removeFromLeft (tW).reduced (4, 2));
    hostSyncB.setBounds      (togRow.removeFromLeft (tW).reduced (4, 2));

    area.removeFromTop (16);

    // pattern controls: header (left) + SHUFFLE / RANDOMIZE / SEQ (right)
    auto patCtl = area.removeFromTop (26);
    patternsHeaderL.setBounds (patCtl.removeFromLeft (120));
    seqB.setBounds       (patCtl.removeFromRight (84).reduced (3, 1));
    patCtl.removeFromRight (6);
    randomizeB.setBounds (patCtl.removeFromRight (110).reduced (3, 1));
    patCtl.removeFromRight (6);
    shuffleB.setBounds   (patCtl.removeFromRight (96).reduced (3, 1));

    area.removeFromTop (8);

    // 12 pattern pads (number centred, lock tick-box in the lower-left)
    auto pads = area.removeFromTop (50);
    const int pw = pads.getWidth() / 12;
    for (int i = 0; i < 12; ++i)
    {
        auto cell = pads.removeFromLeft (pw).reduced (3, 2);
        patternPads[i].setBounds (cell);
        lockButtons[i].setBounds (cell.getX() + 4, cell.getBottom() - 14,
                                  juce::jmin (cell.getWidth() - 6, 38), 12);
    }
}
