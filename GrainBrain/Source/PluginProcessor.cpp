#include "PluginProcessor.h"
#include "PluginEditor.h"

constexpr float GrainBrainAudioProcessor::divValues[];

juce::AudioProcessorValueTreeState::ParameterLayout GrainBrainAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray bandNames{ "Low", "LowMid", "HiMid", "High" };
    juce::StringArray divNames{ "1/1","1/2","1/4","1/4T","1/8","1/8T","1/16","1/16T" };
    juce::StringArray shapeNames{ "Sine","Saw+","Saw-","Tri","Step","Rand" };

    float defaultCenters[4] = { 200.f, 1000.f, 5000.f, 12000.f };

    for (int i = 0; i < 4; ++i)
    {
        juce::String b = bandNames[i];
        juce::String id = juce::String(i);

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "bandCenter" + id, b + " Center",
            juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.4f), defaultCenters[i]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "bandWidth" + id, b + " Width (%)",
            juce::NormalisableRange<float>(0.1f, 100.f, 0.1f, 0.5f), 50.f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "bandEnabled" + id, b + " Enabled", true));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "bandGain" + id, b + " Gain",
            juce::NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainSize" + id, b + " Grain Size (ms)",
            juce::NormalisableRange<float>(1.f, 10000.f, 0.1f, 0.4f), 80.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainScatter" + id, b + " Scatter",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainPitch" + id, b + " Pitch (st)",
            juce::NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainPitchSpread" + id, b + " Pitch Spread (st)",
            juce::NormalisableRange<float>(0.f, 24.f, 0.1f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainDrive" + id, b + " Drive",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainMix" + id, b + " Grain Mix",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "grainReverse" + id, b + " Reverse", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "grainFreeze" + id, b + " Freeze", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "grainSync" + id, b + " BPM Sync", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "grainDiv" + id, b + " Division", divNames, 2));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "gateOn" + id, b + " Gate On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "gateDepth" + id, b + " Gate Depth",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "gateAttack" + id, b + " Gate Attack",
            juce::NormalisableRange<float>(0.f, 0.49f, 0.01f), 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "gateRelease" + id, b + " Gate Release",
            juce::NormalisableRange<float>(0.f, 0.49f, 0.01f), 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "gatePhase" + id, b + " Gate Phase",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "gateDiv" + id, b + " Gate Division", divNames, 2));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "feedbackAmt" + id, b + " Bleed",
            juce::NormalisableRange<float>(0.f, 0.9f, 0.01f), 0.f));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "lfoOn" + id, b + " LFO On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoRate" + id, b + " LFO Rate (Hz)",
            juce::NormalisableRange<float>(0.01f, 20.f, 0.01f, 0.4f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "lfoShape" + id, b + " LFO Shape", shapeNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "lfoSync" + id, b + " LFO Sync", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "lfoDiv" + id, b + " LFO Division", divNames, 4));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_ctr" + id, b + " LFO Depth Center",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_ctr" + id, b + " LFO Phase Center",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_wid" + id, b + " LFO Depth Width",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_wid" + id, b + " LFO Phase Width",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_mix" + id, b + " LFO Depth Mix",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_mix" + id, b + " LFO Phase Mix",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_pit" + id, b + " LFO Depth Pitch",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_pit" + id, b + " LFO Phase Pitch",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_sct" + id, b + " LFO Depth Scatter",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_sct" + id, b + " LFO Phase Scatter",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_gph" + id, b + " LFO Depth GatePhase",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_gph" + id, b + " LFO Phase GatePhase",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoD_siz" + id, b + " LFO Depth Size",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "lfoP_siz" + id, b + " LFO Phase Size",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryIn", "Dry In",
        juce::NormalisableRange<float>(-48.f, 12.f, 0.1f), 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "wetOut", "Wet Out",
        juce::NormalisableRange<float>(-48.f, 12.f, 0.1f), 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "passthrough", "Passthrough",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));

    return { params.begin(), params.end() };
}

GrainBrainAudioProcessor::GrainBrainAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        juce::String id = juce::String(i);

        bandCenter[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("bandCenter" + id));
        bandWidth[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("bandWidth" + id));
        bandEnabled[i] = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bandEnabled" + id));
        bandGain[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("bandGain" + id));
        grainSize[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("grainSize" + id));
        grainScatter[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("grainScatter" + id));
        grainPitch[i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("grainPitch" + id));
        grainPitchSpread[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("grainPitchSpread" + id));
        grainDrive[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("grainDrive" + id));
        grainMix[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("grainMix" + id));
        grainReverse[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("grainReverse" + id));
        grainFreeze[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("grainFreeze" + id));
        grainSync[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("grainSync" + id));
        grainDiv[i] = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("grainDiv" + id));
        gateOn[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("gateOn" + id));
        gateDepth[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("gateDepth" + id));
        gateAttack[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("gateAttack" + id));
        gateRelease[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("gateRelease" + id));
        gatePhase[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("gatePhase" + id));
        gateDiv[i] = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("gateDiv" + id));
        feedbackAmt[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("feedbackAmt" + id));
        lfoOn[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("lfoOn" + id));
        lfoRate[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoRate" + id));
        lfoShape[i] = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("lfoShape" + id));
        lfoSync[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("lfoSync" + id));
        lfoDiv[i] = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("lfoDiv" + id));
        lfoDepthCenter[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_ctr" + id));
        lfoPhaseCenter[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_ctr" + id));
        lfoDepthWidth[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_wid" + id));
        lfoPhaseWidth[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_wid" + id));
        lfoDepthMix[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_mix" + id));
        lfoPhaseeMix[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_mix" + id));
        lfoDepthPitch[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_pit" + id));
        lfoPhaseePitch[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_pit" + id));
        lfoDepthScatter[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_sct" + id));
        lfoPhaseScatter[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_sct" + id));
        lfoDepthGatePhase[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_gph" + id));
        lfoPhaseGatePhase[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_gph" + id));
        lfoDepthSize[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoD_siz" + id));
        lfoPhaseSize[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("lfoP_siz" + id));
    }

    dryIn = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("dryIn"));
    wetOut = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("wetOut"));
    passthrough = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("passthrough"));

    // Load MIDI map if it exists
    midiLearn.loadFromFile(
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("GrainBrain").getChildFile("midimap.xml"));

    // Open MIDI devices immediately — don't wait for prepareToPlay
    midiLearn.openMidiDevices(&apvts);
}

GrainBrainAudioProcessor::~GrainBrainAudioProcessor() {}

void GrainBrainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = (juce::uint32)getTotalNumOutputChannels();

    for (int i = 0; i < NUM_BANDS; ++i)
    {
        bandFilter[i].prepare(spec);
        bandFilter[i].setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        granularL[i].prepare(sampleRate);
        granularR[i].prepare(sampleRate);
        gateL[i].prepare(sampleRate);
        gateR[i].prepare(sampleRate);
        lfoEngine[i].prepare(sampleRate);
        feedbackBuf[i].setSize(2, samplesPerBlock);
        feedbackBuf[i].clear();
        liveCenter[i].store(bandCenter[i]->get(), std::memory_order_relaxed);
        liveWidth[i].store(bandWidth[i]->get(), std::memory_order_relaxed);
    }
}

void GrainBrainAudioProcessor::releaseResources()
{
    for (int i = 0; i < NUM_BANDS; ++i)
        feedbackBuf[i].clear();
}

void GrainBrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ── BPM ──────────────────────────────────────────────────────────────────
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                currentBPM = *bpm > 0.0 ? *bpm : 120.0;

    // ── MIDI CC learn / apply ─────────────────────────────────────────────────
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isController())
            midiLearn.handleCC(msg.getControllerNumber(),
                msg.getControllerValue(), apvts);
    }

    // ── Dry-in gain ───────────────────────────────────────────────────────────
    buffer.applyGain(juce::Decibels::decibelsToGain(dryIn->get()));

    // ── Spectrum FIFO ─────────────────────────────────────────────────────────
    {
        const float* channelData = buffer.getReadPointer(0);
        int start1, size1, start2, size2;
        abstractFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
        if (size1 > 0) juce::FloatVectorOperations::copy(audioFifo.data() + start1, channelData, size1);
        if (size2 > 0) juce::FloatVectorOperations::copy(audioFifo.data() + start2, channelData + size1, size2);
        abstractFifo.finishedWrite(size1 + size2);
    }

    // ── Passthrough copy ──────────────────────────────────────────────────────
    juce::AudioBuffer<float> passthroughBuf;
    passthroughBuf.makeCopyOf(buffer);

    const float* inputL = buffer.getReadPointer(0);
    const float* inputR = numChannels > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0);

    juce::AudioBuffer<float> band[4];
    for (auto& b : band)
        b.setSize(numChannels, numSamples);

    for (int b = 0; b < NUM_BANDS; ++b)
    {
        if (!bandEnabled[b]->get())
        {
            band[b].clear();
            feedbackBuf[b].clear();
            liveCenter[b].store(bandCenter[b]->get(), std::memory_order_relaxed);
            liveWidth[b].store(bandWidth[b]->get(), std::memory_order_relaxed);
            continue;
        }

        float* L = band[b].getWritePointer(0);
        float* R = numChannels > 1 ? band[b].getWritePointer(1) : band[b].getWritePointer(0);

        juce::FloatVectorOperations::copy(L, inputL, numSamples);
        if (numChannels > 1) juce::FloatVectorOperations::copy(R, inputR, numSamples);

        // Cross-band feedback from the previous band. Injected post-filter (inside
        // the sample loop below) so the adjacent band's out-of-band content isn't
        // removed by THIS band's bandpass before it can be heard.
        int srcBand = (b - 1 + NUM_BANDS) % NUM_BANDS;
        float fbAmt = feedbackAmt[srcBand]->get();
        const bool fbActive = fbAmt > 0.001f
            && feedbackBuf[srcBand].getNumSamples() >= numSamples;
        const float* fbL = fbActive ? feedbackBuf[srcBand].getReadPointer(0) : nullptr;
        const float* fbR = fbActive ? feedbackBuf[srcBand].getReadPointer(
            feedbackBuf[srcBand].getNumChannels() > 1 ? 1 : 0) : nullptr;

        // Base params
        bool  rev = grainReverse[b]->get();
        bool  freeze = grainFreeze[b]->get();
        bool  sync = grainSync[b]->get();
        float scatter = grainScatter[b]->get();
        float pitch = grainPitch[b]->get();
        float pitchSpread = grainPitchSpread[b]->get();
        float drive = grainDrive[b]->get();
        float wetmix = grainMix[b]->get();
        float gain = juce::Decibels::decibelsToGain(bandGain[b]->get());
        bool  gOn = gateOn[b]->get();
        float gDepth = gateDepth[b]->get();
        float gAtt = gateAttack[b]->get();
        float gRel = gateRelease[b]->get();
        float gPhs = gatePhase[b]->get();

        float beatsPerSample = (float)(currentBPM / 60.0 / getSampleRate());
        float gateCycleSamples = divValues[gateDiv[b]->getIndex()] / beatsPerSample;

        float size;
        if (sync) {
            float beatsPerMs = (float)currentBPM / 60000.f;
            size = divValues[grainDiv[b]->getIndex()] / beatsPerMs;
        }
        else {
            size = grainSize[b]->get();
        }

        // LFO rate (0 if off)
        float lfoRateHz = 0.f;
        if (lfoOn[b]->get())
        {
            if (lfoSync[b]->get()) {
                float bps = (float)currentBPM / 60.f;
                lfoRateHz = bps / divValues[lfoDiv[b]->getIndex()];
            }
            else {
                lfoRateHz = lfoRate[b]->get();
            }
        }

        int   lfoShp = lfoShape[b]->getIndex();
        float baseCtr = bandCenter[b]->get();
        float baseWid = bandWidth[b]->get();

        float depCtr = lfoDepthCenter[b]->get();
        float phsCtr = lfoPhaseCenter[b]->get();
        float depWid = lfoDepthWidth[b]->get();
        float phsWid = lfoPhaseWidth[b]->get();
        float depMix = lfoDepthMix[b]->get();
        float phsMix = lfoPhaseeMix[b]->get();
        float depPit = lfoDepthPitch[b]->get();
        float phsPit = lfoPhaseePitch[b]->get();
        float depSct = lfoDepthScatter[b]->get();
        float phsSct = lfoPhaseScatter[b]->get();
        float depGph = lfoDepthGatePhase[b]->get();
        float phsGph = lfoPhaseGatePhase[b]->get();
        float depSiz = lfoDepthSize[b]->get();
        float phsSiz = lfoPhaseSize[b]->get();

        // Track the final modulated center/width so the spectrum display can mirror it.
        float lastModCenter = baseCtr;
        float lastModWidth = baseWid;

        for (int s = 0; s < numSamples; ++s)
        {
            lfoEngine[b].advance(lfoRateHz);

            float lfoCenter = lfoEngine[b].getValue(phsCtr, lfoShp) * depCtr;
            float modCenter = juce::jlimit(20.f, 20000.f, baseCtr * std::pow(2.f, lfoCenter * 2.f));

            float lfoWidth = lfoEngine[b].getValue(phsWid, lfoShp) * depWid;
            float modWidth = juce::jlimit(0.1f, 100.f, baseWid + lfoWidth * 50.f);
            float modQ = juce::jlimit(0.5f, 100.f, 50.f / modWidth);

            lastModCenter = modCenter;
            lastModWidth = modWidth;

            bandFilter[b].setCutoffFrequency(modCenter);
            bandFilter[b].setResonance(modQ);
            L[s] = bandFilter[b].processSample(0, L[s]);
            if (numChannels > 1) R[s] = bandFilter[b].processSample(1, R[s]);

            // Cross-band feedback injected post-filter (see fbActive above)
            if (fbActive)
            {
                L[s] += fbL[s] * fbAmt;
                if (numChannels > 1) R[s] += fbR[s] * fbAmt;
            }

            float modMix = juce::jlimit(0.f, 1.f, wetmix + lfoEngine[b].getValue(phsMix, lfoShp) * depMix);
            float modPitch = juce::jlimit(-12.f, 12.f, pitch + lfoEngine[b].getValue(phsPit, lfoShp) * depPit * 12.f);
            float modScatter = juce::jlimit(0.f, 1.f, scatter + lfoEngine[b].getValue(phsSct, lfoShp) * depSct);
            float lfoGph = lfoEngine[b].getValue(phsGph, lfoShp) * depGph * 0.5f;
            float modGPhs = std::fmod(gPhs + lfoGph + 1.f, 1.f);

            // SIZE modulation — cubic depth ramp gives fine control at low settings;
            // multiplicative so the swing stays proportional across grain sizes.
            float sizFrac = depSiz * depSiz * depSiz;
            float modSize = juce::jlimit(1.f, 10000.f,
                size * (1.f + lfoEngine[b].getValue(phsSiz, lfoShp) * sizFrac));

            L[s] = granularL[b].process(L[s], modSize, modScatter, modPitch,
                pitchSpread, drive, modMix, rev, freeze) * gain;
            if (numChannels > 1)
                R[s] = granularR[b].process(R[s], modSize, modScatter, modPitch,
                    pitchSpread, drive, modMix, rev, freeze) * gain;

            L[s] = gateL[b].process(L[s], gateCycleSamples, modGPhs, gDepth, gAtt, gRel, gOn);
            if (numChannels > 1)
                R[s] = gateR[b].process(R[s], gateCycleSamples, modGPhs, gDepth, gAtt, gRel, gOn);
        }

        liveCenter[b].store(lastModCenter, std::memory_order_relaxed);
        liveWidth[b].store(lastModWidth, std::memory_order_relaxed);

        feedbackBuf[b].makeCopyOf(band[b]);
    }

    buffer.clear();
    for (int b = 0; b < NUM_BANDS; ++b)
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addFrom(ch, 0, band[b], ch, 0, numSamples);

    float pt = passthrough->get();
    if (pt > 0.001f)
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addFrom(ch, 0, passthroughBuf, ch, 0, numSamples, pt);

    buffer.applyGain(juce::Decibels::decibelsToGain(wetOut->get()));
}

void GrainBrainAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GrainBrainAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainBrainAudioProcessor();
}