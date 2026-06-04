#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout O2AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Global ─────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "inputTrim",   "Input Trim",
        juce::NormalisableRange<float>(-48.f, 15.f, 0.1f), 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lowXover",    "Low Crossover (Hz)",
        juce::NormalisableRange<float>(70.f, 1500.f, 1.f, 0.4f), 300.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "highXover",   "High Crossover (Hz)",
        juce::NormalisableRange<float>(800.f, 15000.f, 1.f, 0.4f), 3000.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outputLevel", "Output Level",
        juce::NormalisableRange<float>(-48.f, 12.f, 0.1f), 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWet",      "Dry/Wet",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("mono",         "Mono",          false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("masterBypass", "Master Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("oversample",   "Oversample",    true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("widen",        "Mono Widen",    false));

    // ── Per band ───────────────────────────────────────────────────────────
    juce::StringArray bandNames { "Low", "Mid", "High" };
    juce::StringArray variantPlaceholders { "V0", "V1", "V2", "V3" };

    for (int i = 0; i < 3; ++i)
    {
        juce::String b  = bandNames[i];
        juce::String id = juce::String(i);

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "satType"    + id, b + " Sat Type",    o2TypeNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "satVariant" + id, b + " Sat Variant", variantPlaceholders, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "drive"      + id, b + " Drive",
            juce::NormalisableRange<float>(0.f, 10.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "width"      + id, b + " Width",
            juce::NormalisableRange<float>(-5.f, 5.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "level"      + id, b + " Level",
            juce::NormalisableRange<float>(-48.f, 6.f, 0.1f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "feedback"   + id, b + " Feedback",
            juce::NormalisableRange<float>(0.f, 0.90f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "hiss"       + id, b + " Hiss",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "drift"      + id, b + " Drift",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "flipPhase"  + id, b + " Flip Phase",  false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "gateFbk"    + id, b + " Gate Fbk",    false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "bandBypass" + id, b + " Bypass",       false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "bandMute"   + id, b + " Mute",         false));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
O2AudioProcessor::O2AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    pInputTrim    = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("inputTrim"));
    pLowXover     = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("lowXover"));
    pHighXover    = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("highXover"));
    pOutputLevel  = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("outputLevel"));
    pDryWet       = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("dryWet"));
    pMono         = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter("mono"));
    pMasterBypass = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter("masterBypass"));
    pOversample   = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter("oversample"));
    pWiden        = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter("widen"));

    for (int i = 0; i < NUM_BANDS; ++i)
    {
        juce::String id = juce::String(i);
        pSatType   [i] = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("satType"    + id));
        pSatVariant[i] = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("satVariant" + id));
        pDrive     [i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("drive"      + id));
        pWidth     [i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("width"      + id));
        pLevel     [i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("level"      + id));
        pFeedback  [i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("feedback"   + id));
        pHiss      [i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("hiss"       + id));
        pDrift     [i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter("drift"      + id));
        pFlipPhase [i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("flipPhase"  + id));
        pGateFbk   [i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("gateFbk"    + id));
        pBandBypass[i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("bandBypass" + id));
        pBandMute  [i] = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter("bandMute"   + id));
    }
}

O2AudioProcessor::~O2AudioProcessor() {}

//==============================================================================
void O2AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        mSatEngine[i].prepare(sampleRate);
        mOversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        mOversamplers[i]->initProcessing((size_t)samplesPerBlock);
        mBandBuf[i].setSize(2, samplesPerBlock * 4);
        mBandBuf[i].clear();

        mFbState    [i][0] = mFbState    [i][1] = 0.f;
        mHissFilter [i][0] = mHissFilter [i][1] = 0.f;
        mGateEnv    [i]    = 0.f;
        mDriftPhase [i]    = 0.f;
        mDriftFreq  [i]    = 0.1f + mDist(mRng) * 0.3f;

        for (int k = 0; k < kWidenMax; ++k) mWidenBuf[i][k] = 0.f;
        mWidenIdx[i] = 0;
    }

    // ~0.8 ms decorrelation delay for the mono-widen side synthesis
    mWidenLen = juce::jlimit(8, kWidenMax - 1, (int)std::round(sampleRate * 0.0008));

    mLP1State[0] = mLP1State[1] = 0.f;
    mLP2State[0] = mLP2State[1] = 0.f;
    mInPeakSmooth  = 0.f;
    mOutPeakSmooth = 0.f;
}

void O2AudioProcessor::releaseResources() {}

//==============================================================================
void O2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    // Peak-meter release ballistics: instant attack, ~0.3 s fall (was a ~16 s
    // crawl, which made the meters look frozen / unresponsive).
    const float meterDecay = std::exp(-(float)numSamples / ((float)getSampleRate() * 0.30f));

    if (pMasterBypass->get()) return;

    // ── Input gain + peak ──────────────────────────────────────────────────
    buffer.applyGain(juce::Decibels::decibelsToGain(pInputTrim->get()));

    float inPk = 0.f;
    for (int ch = 0; ch < numChannels; ++ch)
        inPk = juce::jmax(inPk, buffer.getMagnitude(ch, 0, numSamples));
    mInPeakSmooth = juce::jmax(inPk, mInPeakSmooth * meterDecay);
    inputPeak.store(mInPeakSmooth);

    // ── Dry copy ───────────────────────────────────────────────────────────
    juce::AudioBuffer<float> dryBuf;
    dryBuf.makeCopyOf(buffer);

    const float* inL = buffer.getReadPointer(0);
    const float* inR = numChannels > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0);

    // ── Crossover ──────────────────────────────────────────────────────────
    float sr = (float)getSampleRate();
    float g1 = std::tan(juce::MathConstants<float>::pi * pLowXover ->get() / sr);
    float g2 = std::tan(juce::MathConstants<float>::pi * pHighXover->get() / sr);

    for (int b = 0; b < NUM_BANDS; ++b)
        mBandBuf[b].setSize(2, numSamples, false, false, true);

    float* lowL  = mBandBuf[0].getWritePointer(0);
    float* lowR  = mBandBuf[0].getWritePointer(1);
    float* midL  = mBandBuf[1].getWritePointer(0);
    float* midR  = mBandBuf[1].getWritePointer(1);
    float* highL = mBandBuf[2].getWritePointer(0);
    float* highR = mBandBuf[2].getWritePointer(1);

    for (int s = 0; s < numSamples; ++s)
    {
        float loL = tpt1p(inL[s], g1, mLP1State[0]);
        float loR = tpt1p(inR[s], g1, mLP1State[1]);
        float remL = inL[s] - loL;
        float remR = inR[s] - loR;
        float miL = tpt1p(remL, g2, mLP2State[0]);
        float miR = tpt1p(remR, g2, mLP2State[1]);
        lowL [s] = loL;   lowR [s] = loR;
        midL [s] = miL;   midR [s] = miR;
        highL[s] = remL - miL;
        highR[s] = remR - miR;
    }

    // ── Per-band processing ────────────────────────────────────────────────
    const float twoPi = juce::MathConstants<float>::twoPi;
    bool doOS = pOversample->get();

    for (int b = 0; b < NUM_BANDS; ++b)
    {
        if (pBandMute[b]->get())
        { mBandBuf[b].clear(); mFbState[b][0] = mFbState[b][1] = 0.f; continue; }

        float drive      = pDrive    [b]->get();
        float widthP     = pWidth    [b]->get();
        float levelGain  = juce::Decibels::decibelsToGain(pLevel[b]->get());
        float fbAmt      = pFeedback [b]->get();
        float hissAmt    = pHiss     [b]->get();
        float drift      = pDrift    [b]->get();
        bool  flipPhase  = pFlipPhase[b]->get();
        bool  gateFbk    = pGateFbk  [b]->get();
        bool  bypass     = pBandBypass[b]->get();
        int   typeIdx    = pSatType   [b]->getIndex();
        int   varIdx     = pSatVariant[b]->getIndex();
        float widthScale = (widthP + 5.f) / 5.f;   // 0=mono, 1=unity, 2=wide

        float* bL = mBandBuf[b].getWritePointer(0);
        float* bR = mBandBuf[b].getWritePointer(1);

        // Drift LFO update (block rate is fine for this slow modulation)
        mDriftPhase[b] += mDriftFreq[b] * (float)numSamples / sr;
        if (mDriftPhase[b] >= 1.f)
        {
            mDriftPhase[b] -= 1.f;
            mDriftFreq[b] = 0.05f + mDist(mRng) * 0.45f;
        }
        float driftMod = std::sin(mDriftPhase[b] * twoPi) * drift;

        // Gate envelope — track input energy for gate fbk
        float blockEnergy = 0.f;
        for (int s = 0; s < numSamples; ++s)
            blockEnergy += std::abs(bL[s]) + std::abs(bR[s]);
        blockEnergy /= (float)(numSamples * 2);
        mGateEnv[b] = mGateEnv[b] * 0.9998f + blockEnergy * 0.0002f;
        bool gated = gateFbk && (mGateEnv[b] < 0.0003f);

        // Effective feedback — reshaped so the knob reaches the audible/resonant
        // region sooner (less dead zone) and the top drives toward self-oscillation
        // for a dramatic effect. drift modulates, gate mutes.
        float fbCurved = std::pow(fbAmt / 0.90f, 0.55f) * 0.97f;
        float effectiveFb = gated ? 0.f
                          : juce::jlimit(0.f, 0.99f, fbCurved * (1.f + driftMod * 0.3f));

        // Effective drive — drift wobbles the saturation amount (audible "analog
        // breathing" even with no feedback); ±~30% of the current drive.
        float effectiveDrive = juce::jlimit(0.f, 10.f, drive * (1.f + driftMod * 0.30f));

        // ── Sample-by-sample processing (required for correct feedback) ──────
        float prevL = mFbState[b][0];
        float prevR = mFbState[b][1];

        if (!bypass && doOS)
        {
            // For oversampling: apply feedback at native rate first,
            // then run saturation through oversampler as a block
            for (int s = 0; s < numSamples; ++s)
            {
                bL[s] = bL[s] + prevL * effectiveFb;
                bR[s] = bR[s] + prevR * effectiveFb;
                prevL = bL[s];
                prevR = bR[s];
            }

            juce::dsp::AudioBlock<float> block(mBandBuf[b].getArrayOfWritePointers(),
                                               2, (size_t)numSamples);
            auto osBlock = mOversamplers[b]->processSamplesUp(block);
            size_t osN = osBlock.getNumSamples();

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = osBlock.getChannelPointer(ch);
                for (size_t s = 0; s < osN; ++s)
                    data[s] = mSatEngine[b].process(data[s], effectiveDrive, typeIdx, varIdx, ch);
            }
            mOversamplers[b]->processSamplesDown(block);
        }
        else
        {
            // Native-rate feedback + saturation, sample by sample
            for (int s = 0; s < numSamples; ++s)
            {
                float xL = bL[s] + prevL * effectiveFb;
                float xR = bR[s] + prevR * effectiveFb;

                if (!bypass)
                {
                    xL = mSatEngine[b].process(xL, effectiveDrive, typeIdx, varIdx, 0);
                    xR = mSatEngine[b].process(xR, effectiveDrive, typeIdx, varIdx, 1);
                }

                prevL = xL;
                prevR = xR;
                bL[s] = xL;
                bR[s] = xR;
            }
        }

        // Store feedback state
        mFbState[b][0] = prevL;
        mFbState[b][1] = prevR;

        // ── Hiss ──────────────────────────────────────────────────────────
        if (hissAmt > 0.001f)
        {
            float hissLevel = hissAmt * 0.018f * (1.f + effectiveDrive * 0.04f);
            for (int s = 0; s < numSamples; ++s)
            {
                float white = mDist(mRng) * 2.f - 1.f;
                // One-pole lowpass ~2.5kHz for colored noise
                mHissFilter[b][0] = mHissFilter[b][0] * 0.85f + white * 0.15f;
                // Mono hiss (same for L and R — preserves mono compatibility)
                float hissSample = mHissFilter[b][0] * hissLevel;
                bL[s] += hissSample;
                bR[s] += hissSample;
            }
        }

        // ── M/S Width + Level ─────────────────────────────────────────────
        const bool  widen  = pWiden->get();
        const float extraW = juce::jmax(0.f, widthScale - 1.f);   // how far past unity
        for (int s = 0; s < numSamples; ++s)
        {
            float mid  = (bL[s] + bR[s]) * 0.5f;
            float side = (bL[s] - bR[s]) * 0.5f * widthScale;

            // Mono-widen: synthesise a side from a short-delayed mid. Cancels in
            // (L+R) so it stays mono-compatible; only adds width as the Width knob
            // is pushed past centre, so a mono source can be widened.
            float delayed = mWidenBuf[b][mWidenIdx[b]];
            mWidenBuf[b][mWidenIdx[b]] = mid;
            if (++mWidenIdx[b] >= mWidenLen) mWidenIdx[b] = 0;
            if (widen) side += delayed * extraW * 0.7f;

            bL[s] = (mid + side) * levelGain;
            bR[s] = (mid - side) * levelGain;
        }

        // ── Flip Phase ────────────────────────────────────────────────────
        if (flipPhase)
            for (int s = 0; s < numSamples; ++s)
            { bL[s] = -bL[s]; bR[s] = -bR[s]; }
    }

    // ── Sum bands ──────────────────────────────────────────────────────────
    buffer.clear();
    for (int b = 0; b < NUM_BANDS; ++b)
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addFrom(ch, 0, mBandBuf[b], ch, 0, numSamples);

    // ── Mono collapse ──────────────────────────────────────────────────────
    if (pMono->get() && numChannels > 1)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int s = 0; s < numSamples; ++s)
        { float m = (L[s] + R[s]) * 0.5f; L[s] = R[s] = m; }
    }

    // ── Output gain + dry/wet ──────────────────────────────────────────────
    buffer.applyGain(juce::Decibels::decibelsToGain(pOutputLevel->get()));

    float wet = pDryWet->get();
    if (wet < 0.999f)
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addFrom(ch, 0, dryBuf, ch, 0, numSamples, 1.f - wet);

    // ── Output peak ────────────────────────────────────────────────────────
    float outPk = 0.f;
    for (int ch = 0; ch < numChannels; ++ch)
        outPk = juce::jmax(outPk, buffer.getMagnitude(ch, 0, numSamples));
    mOutPeakSmooth = juce::jmax(outPk, mOutPeakSmooth * meterDecay);
    outputPeak.store(mOutPeakSmooth);
}

//==============================================================================
void O2AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = std::unique_ptr<juce::XmlElement>(state.createXml()))
        copyXmlToBinary(*xml, destData);
}

void O2AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = std::unique_ptr<juce::XmlElement>(getXmlFromBinary(data, sizeInBytes)))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new O2AudioProcessor();
}
