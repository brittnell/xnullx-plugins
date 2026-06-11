#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr float kRatioValues[] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f,
                                       3.f, 4.f, 5.f, 6.f, 7.f, 8.f };
    constexpr float kDivValues[]   = { 4.f, 2.f, 1.f, 0.6667f, 0.5f,
                                       0.3333f, 0.25f, 0.1667f };  // beats
    constexpr float kCentreSec     = 0.004f;   // PM centre tap (latency)
    constexpr float kFoldDriveMax  = 15.f;     // drive = 1 + p * 15
}

//==============================================================================
FoldspaceAudioProcessor::FoldspaceAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "FoldspaceParams", createLayout())
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout FoldspaceAudioProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using Pc = juce::AudioParameterChoice;
    using Pb = juce::AudioParameterBool;
    using Rng = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto rangeSkew = [](float lo, float hi, float skew) { Rng r(lo, hi); r.setSkewForCentre(skew); return r; };

    // ---- global / IO --------------------------------------------------------
    p.push_back(std::make_unique<P>("inTrim",  "In Trim",  Rng(-24.f, 24.f, 0.1f), 0.f));
    p.push_back(std::make_unique<P>("outTrim", "Out Trim", Rng(-24.f, 24.f, 0.1f), 0.f));
    p.push_back(std::make_unique<P>("dryWet",  "Dry/Wet",  Rng(0.f, 1.f, 0.01f), 1.f));
    p.push_back(std::make_unique<Pc>("oversample", "Oversample",
                                     juce::StringArray { "OFF", "2X", "4X" }, 1));
    p.push_back(std::make_unique<Pb>("masterBypass", "Bypass", false));

    // ---- pitch --------------------------------------------------------------
    p.push_back(std::make_unique<Pc>("pitchMode", "Pitch Mode",
                                     juce::StringArray { "TRACK", "MIDI", "FREE" }, 0));
    p.push_back(std::make_unique<P>("freeHz", "Free Hz", rangeSkew(20.f, 2000.f, 200.f), 110.f));
    p.push_back(std::make_unique<P>("glide",  "Glide",   rangeSkew(0.f, 500.f, 80.f), 20.f));

    // ---- modulator ----------------------------------------------------------
    p.push_back(std::make_unique<Pc>("oscWave", "Osc Wave",
                                     juce::StringArray { "SINE", "TRI", "SAW", "SQUARE" }, 0));
    p.push_back(std::make_unique<Pc>("oscRatio", "Osc Ratio",
                                     juce::StringArray { "0.25", "0.5", "0.75", "1", "1.5", "2",
                                                         "3", "4", "5", "6", "7", "8" }, 3));
    p.push_back(std::make_unique<P>("oscFine", "Osc Fine", Rng(-100.f, 100.f, 1.f), 0.f));
    p.push_back(std::make_unique<Pb>("oscSync", "Osc Sync", false));
    p.push_back(std::make_unique<Pc>("oscDiv", "Osc Div",
                                     juce::StringArray { "1/1", "1/2", "1/4", "1/4T",
                                                         "1/8", "1/8T", "1/16", "1/16T" }, 2));
    p.push_back(std::make_unique<P>("srcOsc",   "Src Osc",   Rng(0.f, 1.f, 0.01f), 1.f));
    p.push_back(std::make_unique<P>("srcInput", "Src Input", Rng(0.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("modFold",  "Mod Fold",  Rng(0.f, 1.f, 0.01f), 0.f));

    // ---- PM -----------------------------------------------------------------
    p.push_back(std::make_unique<P>("pmIndex",    "PM Index",  rangeSkew(0.f, 1.f, 0.35f), 0.3f));
    p.push_back(std::make_unique<P>("carrier",    "Carrier",   Rng(0.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("envToIndex", "Env>Index", Rng(-1.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("envAtk",     "Env Atk",   rangeSkew(0.1f, 100.f, 10.f), 5.f));
    p.push_back(std::make_unique<P>("envRel",     "Env Rel",   rangeSkew(10.f, 1000.f, 200.f), 150.f));
    p.push_back(std::make_unique<P>("stereo",     "Stereo",    Rng(0.f, 180.f, 1.f), 0.f));

    // ---- fold ---------------------------------------------------------------
    p.push_back(std::make_unique<P>("folds",     "Folds",    rangeSkew(0.f, 1.f, 0.35f), 0.25f));
    p.push_back(std::make_unique<P>("shape",     "Shape",    Rng(0.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("symmetry",  "Symmetry", Rng(-1.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("envToFold", "Env>Fold", Rng(-1.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("tone",      "Tone",     rangeSkew(500.f, 20000.f, 4000.f), 20000.f));

    // ---- feedback -----------------------------------------------------------
    p.push_back(std::make_unique<P>("fbAmount", "Feedback", Rng(0.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("fbTone",   "Fb Tone",  rangeSkew(200.f, 18000.f, 2500.f), 4000.f));
    p.push_back(std::make_unique<P>("fbFold",   "Fb Fold",  Rng(0.f, 1.f, 0.01f), 0.f));
    p.push_back(std::make_unique<P>("drift",    "Drift",    Rng(0.f, 1.f, 0.01f), 0.f));

    return { p.begin(), p.end() };
}

//==============================================================================
bool FoldspaceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void FoldspaceAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    numCh    = juce::jmax(1, getTotalNumOutputChannels());

    using OS = juce::dsp::Oversampling<float>;
    os2 = std::make_unique<OS>((size_t)numCh, 1, OS::filterHalfBandPolyphaseIIR, true);
    os4 = std::make_unique<OS>((size_t)numCh, 2, OS::filterHalfBandPolyphaseIIR, true);
    os2->initProcessing((size_t)samplesPerBlock);
    os4->initProcessing((size_t)samplesPerBlock);

    for (auto& ch : channels)
        ch.prepare(sampleRate * 4.0);

    tracker.prepare(sampleRate);
    envFollower.prepare(sampleRate);

    envBuf.assign((size_t)samplesPerBlock, 0.f);
    dryBuf.setSize(numCh, samplesPerBlock);

    const int maxLat = (int)std::ceil(os4->getLatencyInSamples() + kCentreSec * sampleRate) + 8;
    dryDelay.setMaximumDelayInSamples(maxLat);
    dryDelay.prepare({ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)numCh });

    smIn .reset(sampleRate, 0.02); smOut.reset(sampleRate, 0.02); smMix.reset(sampleRate, 0.02);

    curOsIndex = -1;   // force applyOsSetting in the first block
    hzState = 110.f;
    modPhase = carPhase = 0.0;
}

void FoldspaceAudioProcessor::applyOsSetting(int osIndex)
{
    curOsIndex = osIndex;
    const int factor = osIndex == 2 ? 4 : (osIndex == 1 ? 2 : 1);
    procRate = baseRate * factor;

    os2->reset();
    os4->reset();
    for (auto& ch : channels) { ch.reset(); ch.setRate(procRate); }

    for (auto* s : { &smSrcOsc, &smSrcIn, &smFb, &smModFold, &smIdx, &smEnvIdx,
                     &smCar, &smStereo, &smFolds, &smEnvFold, &smShape, &smBias, &smFbFold })
        s->reset(procRate, 0.02);

    auto* curOs = osIndex == 2 ? os4.get() : (osIndex == 1 ? os2.get() : nullptr);
    const float osLat = curOs != nullptr ? curOs->getLatencyInSamples() : 0.f;
    dryDelaySamps = osLat + kCentreSec * (float)baseRate;
    dryDelay.reset();
    setLatencySamples((int)std::lround(dryDelaySamps));
}

//==============================================================================
void FoldspaceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int chs        = juce::jmin(numCh, buffer.getNumChannels());
    if (numSamples == 0 || chs == 0) return;

    auto P = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };

    // ---- MIDI: mono, last-note priority; pitch persists after note-off -----
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            midiNoteHz = (float)juce::MidiMessage::getMidiNoteInHertz(msg.getNoteNumber());
    }

    if (P("masterBypass") > 0.5f)
    {
        float pk = buffer.getMagnitude(0, numSamples);
        uiInPeak.store(pk); uiOutPeak.store(pk);
        return;
    }

    const int osIndex = (int)P("oversample");
    if (osIndex != curOsIndex)
        applyOsSetting(osIndex);

    // defensive: some hosts exceed the prepared block size
    if ((int)envBuf.size() < numSamples)
    {
        envBuf.resize((size_t)numSamples);
        dryBuf.setSize(numCh, numSamples, false, false, true);
    }

    const int osFactor = osIndex == 2 ? 4 : (osIndex == 1 ? 2 : 1);
    const int shift    = osIndex == 2 ? 2 : (osIndex == 1 ? 1 : 0);

    // ---- per-block parameter targets ----------------------------------------
    smIn .setTargetValue(juce::Decibels::decibelsToGain(P("inTrim")));
    smOut.setTargetValue(juce::Decibels::decibelsToGain(P("outTrim")));
    smMix.setTargetValue(P("dryWet"));

    envFollower.setTimes(P("envAtk"), P("envRel"));

    // drift LFO: random rate, random targets, smoothed approach
    {
        driftPhase += (double)numSamples / baseRate * driftRate;
        if (driftPhase >= 1.0)
        {
            driftPhase -= std::floor(driftPhase);
            driftRate = 0.05 + 0.45 * rng.nextDouble();
            dv1T = rng.nextFloat() * 2.f - 1.f;
            dv2T = rng.nextFloat() * 2.f - 1.f;
        }
        dv1 += 0.05f * (dv1T - dv1);
        dv2 += 0.05f * (dv2T - dv2);
    }
    const float drift = P("drift");

    smSrcOsc .setTargetValue(P("srcOsc"));
    smSrcIn  .setTargetValue(P("srcInput"));
    smModFold.setTargetValue(P("modFold"));
    smIdx    .setTargetValue(P("pmIndex"));
    smEnvIdx .setTargetValue(P("envToIndex"));
    smCar    .setTargetValue(P("carrier"));
    smStereo .setTargetValue(P("stereo") / 360.f);             // 0..0.5 phase
    smFolds  .setTargetValue(P("folds"));
    smEnvFold.setTargetValue(P("envToFold"));
    smShape  .setTargetValue(P("shape"));
    smBias   .setTargetValue(P("symmetry") * 0.8f + 0.2f * drift * dv2);
    smFbFold .setTargetValue(P("fbFold"));

    // quadratic (convex) feedback curve: the folded PM loop has huge
    // small-signal gain — unlike O2's saturator loop — so the knob needs
    // convex shaping. Cubic proved too subtle low down; k^2 is the tuned
    // middle between that and the original concave curve.
    const float fbKnob   = P("fbAmount");
    const float fbCurved = fbKnob * fbKnob
                           * juce::jlimit(0.f, 1.3f, 1.f + 0.3f * drift * dv1);
    smFb.setTargetValue(juce::jlimit(0.f, 1.f, fbCurved));

    // one-pole coefficients (per block; >=19.5k = bypass-open)
    const float toneHz = P("tone");
    const float toneG = toneHz >= 19500.f ? 1.f
        : 1.f - std::exp((float)(-juce::MathConstants<double>::twoPi * toneHz / procRate));
    const float fbToneG =
          1.f - std::exp((float)(-juce::MathConstants<double>::twoPi * P("fbTone") / procRate));

    // ---- pitch target --------------------------------------------------------
    const int pitchMode = (int)P("pitchMode");
    float targetHz = P("freeHz");
    if (pitchMode == 1)
        targetHz = midiNoteHz;
    else if (pitchMode == 0)
    {
        float m[5]; std::copy(medBuf, medBuf + 5, m);
        std::sort(m, m + 5);
        targetHz = m[2];
    }
    targetHz = juce::jlimit(20.f, 4000.f, targetHz);

    const float glideMs = P("glide");
    const float kGlide  = glideMs <= 0.01f ? 1.f
        : 1.f - std::exp((float)(-1.0 / (glideMs * 0.001 * procRate)));

    // modulator frequency factors
    const float ratio    = kRatioValues[(int)P("oscRatio")];
    const float fineMult = std::pow(2.f, P("oscFine") / 1200.f);
    const bool  syncMode = P("oscSync") > 0.5f;
    float syncHz = 2.f;
    if (syncMode)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm()) bpm = *b;
        syncHz = (float)(bpm / 60.0) / kDivValues[(int)P("oscDiv")];
    }

    // ---- base-rate pre-pass: in trim, analysis taps, dry copy ---------------
    float inPk = 0.f;
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = smIn.getNextValue();
        float absMax = 0.f, mono = 0.f;
        for (int c = 0; c < chs; ++c)
        {
            float v = buffer.getSample(c, n) * g;
            buffer.setSample(c, n, v);
            dryBuf.setSample(c, n, v);
            absMax = juce::jmax(absMax, std::abs(v));
            mono += v;
        }
        inPk = juce::jmax(inPk, absMax);
        tracker.push(mono / (float)chs);
        envBuf[(size_t)n] = envFollower.process(absMax);
    }
    uiInPeak.store(inPk);
    uiEnv.store(envFollower.env);

    if (tracker.getConfidence() > 0.5f)
    {
        medBuf[medIdx] = tracker.getHz();
        medIdx = (medIdx + 1) % 5;
    }
    uiPitchConf.store(tracker.getConfidence());

    // ---- wet chain at the processing rate ------------------------------------
    juce::dsp::AudioBlock<float> baseBlock(buffer);
    auto sub = baseBlock.getSubBlock(0, (size_t)numSamples);
    auto* curOs = osIndex == 2 ? os4.get() : (osIndex == 1 ? os2.get() : nullptr);
    juce::dsp::AudioBlock<float> proc = curOs != nullptr ? curOs->processSamplesUp(sub) : sub;

    const int osN = (int)proc.getNumSamples();
    float* chPtr[2] = { proc.getChannelPointer(0),
                        chs > 1 ? proc.getChannelPointer(1) : nullptr };

    PMStep sp;
    sp.wave   = (int)P("oscWave");
    sp.centre = kCentreSec * (float)procRate;
    sp.toneG  = toneG;
    sp.fbToneG = fbToneG;

    const float invProcRate = (float)(1.0 / procRate);
    const float modFreqMult = ratio * fineMult;

    for (int n = 0; n < osN; ++n)
    {
        hzState += kGlide * (targetHz - hzState);
        const float modFreq = syncMode ? syncHz : hzState * modFreqMult;

        sp.modInc = modFreq * invProcRate;
        modPhase += (double)sp.modInc;
        if (modPhase >= 1.0) modPhase -= 1.0;
        carPhase += (double)(hzState * invProcRate);
        if (carPhase >= 1.0) carPhase -= 1.0;
        sp.modPhase = (float)modPhase;
        sp.carPhase = (float)carPhase;

        const float env = envBuf[(size_t)juce::jmin(numSamples - 1, n >> shift)];
        sp.env = env;

        sp.srcOsc   = smSrcOsc.getNextValue();
        sp.srcInput = smSrcIn.getNextValue();
        sp.fbAmt    = smFb.getNextValue();
        sp.modDriveG = 1.f + smModFold.getNextValue() * kFoldDriveMax;
        sp.idx      = juce::jlimit(0.f, 1.f, smIdx.getNextValue()
                                             + smEnvIdx.getNextValue() * env);
        sp.carBlend  = smCar.getNextValue();
        sp.stereoOff = smStereo.getNextValue();
        const float foldsEff = juce::jlimit(0.f, 1.f, smFolds.getNextValue()
                                                      + smEnvFold.getNextValue() * env);
        sp.mainDriveG = 1.f + foldsEff * kFoldDriveMax;
        sp.shape   = smShape.getNextValue();
        sp.bias    = smBias.getNextValue();
        sp.fbDriveG = 1.f + smFbFold.getNextValue() * kFoldDriveMax;

        for (int c = 0; c < chs; ++c)
            chPtr[c][n] = channels[c].processSample(chPtr[c][n], (float)c, sp);

        // publish the folded modulator (L) at ~base rate for the scope
        if ((n & (osFactor - 1)) == 0)
        {
            const int w = scopeWrite.load(std::memory_order_relaxed);
            scopeBuf[w % scopeSize] = channels[0].getLastMod();
            scopeWrite.store(w + 1, std::memory_order_release);
        }
    }

    if (curOs != nullptr)
        curOs->processSamplesDown(sub);

    uiPitchHz.store(hzState);   // the glided pitch actually driving the carrier

    // ---- dry/wet (latency-matched), out trim, meters -------------------------
    float outPk = 0.f;
    for (int n = 0; n < numSamples; ++n)
    {
        const float mix = smMix.getNextValue();
        const float og  = smOut.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            dryDelay.pushSample(c, dryBuf.getSample(c, n));
            const float dry = dryDelay.popSample(c, dryDelaySamps, true);
            const float wet = buffer.getSample(c, n);
            const float out = (dry + mix * (wet - dry)) * og;
            buffer.setSample(c, n, out);
            outPk = juce::jmax(outPk, std::abs(out));
        }
    }
    uiOutPeak.store(outPk);

    // clear any extra host channels
    for (int c = chs; c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, numSamples);
}

//==============================================================================
void FoldspaceAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = std::unique_ptr<juce::XmlElement>(state.createXml()))
        copyXmlToBinary(*xml, destData);
}

void FoldspaceAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
juce::AudioProcessorEditor* FoldspaceAudioProcessor::createEditor()
{
    return new FoldspaceAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FoldspaceAudioProcessor();
}
