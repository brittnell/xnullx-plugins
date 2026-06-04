#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <utility>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BunkaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "gain", "Gain", juce::NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "pitch", "Pitch", juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack", juce::NormalisableRange<float>(0.f, 200.f, 0.1f, 0.5f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "release", "Release", juce::NormalisableRange<float>(1.f, 500.f, 0.1f, 0.5f), 20.f));

    params.push_back (std::make_unique<juce::AudioParameterBool>("pitchPreserve", "Pitch Preserve", true));
    params.push_back (std::make_unique<juce::AudioParameterBool>("reverse",       "Reverse",        false));
    params.push_back (std::make_unique<juce::AudioParameterBool>("hostSync",      "Host Sync",      true));
    params.push_back (std::make_unique<juce::AudioParameterBool>("seqOn",         "Sequencer",      true));

    params.push_back (std::make_unique<juce::AudioParameterBool>("sliceMode", "Grid Mode", false));  // false=Transient, true=Grid
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "threshold", "Threshold", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterInt>("bars", "Bars", 1, 8, 1));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "gridDiv", "Grid Div", SliceEngine::divisionNames(), 2));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "snapDiv", "Snap Div", SliceEngine::divisionNames(), 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "chance", "Chance", juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));

    return { params.begin(), params.end() };
}

//==============================================================================
BunkaAudioProcessor::BunkaAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();

    pGain          = apvts.getRawParameterValue ("gain");
    pPitch         = apvts.getRawParameterValue ("pitch");
    pAttack        = apvts.getRawParameterValue ("attack");
    pRelease       = apvts.getRawParameterValue ("release");
    pPitchPreserve = apvts.getRawParameterValue ("pitchPreserve");
    pReverse       = apvts.getRawParameterValue ("reverse");
    pHostSync      = apvts.getRawParameterValue ("hostSync");
    pSeqOn         = apvts.getRawParameterValue ("seqOn");
    pChance        = apvts.getRawParameterValue ("chance");
    pSnapDiv       = apvts.getRawParameterValue ("snapDiv");
    pSliceMode     = apvts.getRawParameterValue ("sliceMode");
    pThreshold     = apvts.getRawParameterValue ("threshold");
    pBars          = apvts.getRawParameterValue ("bars");
    pGridDiv       = apvts.getRawParameterValue ("gridDiv");

    for (auto* id : { "sliceMode", "threshold", "bars", "gridDiv" })
        apvts.addParameterListener (id, this);

    startTimerHz (30);
}

BunkaAudioProcessor::~BunkaAudioProcessor()
{
    stopTimer();
    for (auto* id : { "sliceMode", "threshold", "bars", "gridDiv" })
        apvts.removeParameterListener (id, this);
}

//==============================================================================
void BunkaAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    sampler.prepare (sr);
}

bool BunkaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void BunkaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (previewRequested.exchange (false))
        sampler.triggerPreview();

    double hostBpm   = 120.0;
    bool   isPlaying = false;
    double ppq       = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())         hostBpm = *bpm > 0.0 ? *bpm : 120.0;
            isPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition())   ppq = *p;
        }
    currentBPM.store (hostBpm);

    auto data = sampleData;                       // ref-counted local snapshot

    const float  gainDb        = pGain->load();
    const bool   pitchPreserve = pPitchPreserve->load() > 0.5f;
    const bool   hostSync      = pHostSync->load() > 0.5f;
    const bool   seqOn         = pSeqOn->load() > 0.5f;
    const float  chance        = pChance->load();
    const double pitchSemis    = (double) pPitch->load();
    const bool   reverse       = pReverse->load() > 0.5f;
    const double atkMs         = (double) pAttack->load();
    const double relMs         = (double) pRelease->load();

    const double loopBpm    = data != nullptr ? data->loopBpm : 120.0;
    const double tempoRatio = hostSync ? (hostBpm / juce::jmax (1.0, loopBpm)) : 1.0;

    const int floorNote = getSliceFloorNote();
    const int numSamples = buffer.getNumSamples();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            if (note >= floorNote)                       // slice trigger (finger-drum)
            {
                const int sliceIdx = note - floorNote;
                if (data != nullptr && sliceIdx < data->numSlices())
                {
                    sampler.triggerSlice (sliceIdx, m.getFloatVelocity(), tempoRatio,
                                          pitchPreserve, pitchSemis, reverse, atkMs, relMs,
                                          meta.samplePosition);
                    lastTriggeredSlice.store (sliceIdx);
                }
            }
            else if (note >= 36)                          // pattern launch (36..47)
            {
                launchPattern (note - 36);
            }
        }
    }

    // ── Internal sequencer: play the active pattern, phase-locked to the bar ──
    if (seqOn && isPlaying && data != nullptr && data->numSlices() > 0 && hostBpm > 0.0)
    {
        const int    N         = data->numSlices();
        const double L         = juce::jmax (1.0, (double) data->buffer.getNumSamples());
        const double loopBeats = 4.0 * juce::jmax (1, data->numBars);
        const double ppqPerSmp = hostBpm / 60.0 / sampleRate;
        const double ppqEnd    = ppq + numSamples * ppqPerSmp;

        const auto& pat = patterns[(size_t) juce::jlimit (0, kNumPatterns - 1, activePattern.load())];
        const int    M      = (int) pat.size();
        const double avgSeg = loopBeats / (double) juce::jmax (1, N);

        // Each slice occupies its own length in beats; rests take an average slot.
        // Slices play BACK-TO-BACK (cumulative) so uneven transient slices never
        // leave gaps. The total is normalised to the bar so it stays phase-locked
        // (a permutation/shuffle already sums to exactly one bar -> scale == 1).
        auto segBeats = [&] (int raw) -> double
        {
            if (raw < 0) return avgSeg;
            const int s = juce::jlimit (0, N - 1, raw);
            return ((double) (data->sliceEnd (s) - data->sliceStart (s)) / L) * loopBeats;
        };

        double rawTotal = 0.0;
        for (int i = 0; i < M; ++i) rawTotal += segBeats (pat[(size_t) i]);
        const double scale = rawTotal > 1.0e-9 ? loopBeats / rawTotal : 1.0;

        double cum = 0.0;
        for (int i = 0; i < M; ++i)
        {
            const int raw = pat[(size_t) i];
            if (raw >= 0)
            {
                const int    idx      = juce::jlimit (0, N - 1, raw);
                const double fireBeat = cum * scale;
                for (double m = std::ceil ((ppq - fireBeat) / loopBeats); ; ++m)
                {
                    const double cross = fireBeat + m * loopBeats;
                    if (cross >= ppqEnd) break;
                    if (cross >= ppq)
                    {
                        if (chance < 1.0f && audioRng.nextFloat() >= chance) continue;   // per-step probability
                        const int off = juce::jlimit (0, numSamples - 1, (int) ((cross - ppq) / ppqPerSmp));
                        sampler.triggerSlice (idx, 1.0f, tempoRatio, pitchPreserve, pitchSemis,
                                              reverse, atkMs, relMs, off);
                        lastTriggeredSlice.store (idx);   // drive the live spark slice highlight

                    }
                }
            }
            cum += segBeats (raw);
        }
    }

    if (data != nullptr)
        sampler.renderBlock (buffer, 0, buffer.getNumSamples());

    buffer.applyGain (juce::Decibels::decibelsToGain (gainDb));
}

//==============================================================================
void BunkaAudioProcessor::loadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return;

    const int len = (int) reader->lengthInSamples;
    const int ch  = (int) reader->numChannels;
    if (len <= 0 || ch <= 0) return;

    BunkaSampleData::Ptr d = new BunkaSampleData();
    d->buffer.setSize (ch, len);
    reader->read (&d->buffer, 0, len, 0, true, true);
    d->fileSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;

    applySlicing (*d);

    suspendProcessing (true);
    sampleData = d;
    sampler.setSampleData (d);
    rebuildPatternsForSlices (d->numSlices(), false);   // fresh bank for the new file
    suspendProcessing (false);

    activePattern.store (0);
    loadedName = file.getFileName();
    loadedPath = file.getFullPathName();
    numSlicesAtomic.store (d->numSlices());
    rebuildWaveformThumb (d);
    waveformVersion++;
}

void BunkaAudioProcessor::applySlicing (BunkaSampleData& d)
{
    const int mode = (int) pSliceMode->load();        // 0 = Transient, 1 = Grid
    d.numBars  = juce::jmax (1, (int) pBars->load());
    d.loopBpm  = SliceEngine::bpmFromLength (d.buffer.getNumSamples(), d.fileSampleRate, d.numBars);

    if (mode == 1)
        d.sliceStarts = SliceEngine::gridSlice (d.buffer.getNumSamples(), d.fileSampleRate,
                                                d.loopBpm, (int) pGridDiv->load());
    else
        d.sliceStarts = SliceEngine::detectOnsets (d.buffer, d.fileSampleRate, pThreshold->load());

    if (d.sliceStarts.empty())
        d.sliceStarts.push_back (0);

    d.sliceScores = SliceEngine::computeSliceScores (d.buffer, d.sliceStarts, d.fileSampleRate);
}

void BunkaAudioProcessor::reslice()
{
    if (sampleData == nullptr) return;

    suspendProcessing (true);
    applySlicing (*sampleData);                       // mutate in place (safe while suspended)
    rebuildPatternsForSlices (sampleData->numSlices(), true);
    suspendProcessing (false);

    numSlicesAtomic.store (sampleData->numSlices());
    rebuildWaveformThumb (sampleData);
    waveformVersion++;
}

void BunkaAudioProcessor::rebuildWaveformThumb (BunkaSampleData::Ptr d)
{
    if (d == nullptr) return;

    const int n   = d->buffer.getNumSamples();
    const int nCh = d->buffer.getNumChannels();
    std::vector<float> mm ((size_t) kThumbBins * 2, 0.f);

    if (n > 0)
    {
        const double per = (double) n / kThumbBins;
        for (int b = 0; b < kThumbBins; ++b)
        {
            const int s0 = (int) (b * per);
            const int s1 = juce::jmin (n, (int) ((b + 1) * per));
            float mn = 0.f, mx = 0.f;
            for (int ch = 0; ch < nCh; ++ch)
            {
                const float* p = d->buffer.getReadPointer (ch);
                for (int i = s0; i < s1; ++i) { mn = juce::jmin (mn, p[i]); mx = juce::jmax (mx, p[i]); }
            }
            mm[(size_t) b * 2]     = mn;
            mm[(size_t) b * 2 + 1] = mx;
        }
    }

    const juce::ScopedLock sl (thumbLock);
    thumbMinMax     = std::move (mm);
    thumbSlices     = d->sliceStarts;
    thumbNumSamples = juce::jmax (1, n);
    thumbLoopBpm    = d->loopBpm;
}

void BunkaAudioProcessor::getWaveformSnapshot (std::vector<float>& minMaxOut,
                                               std::vector<int>&   sliceStartsOut,
                                               int& numSamplesOut, double& loopBpmOut)
{
    const juce::ScopedLock sl (thumbLock);
    minMaxOut      = thumbMinMax;
    sliceStartsOut = thumbSlices;
    numSamplesOut  = thumbNumSamples;
    loopBpmOut     = thumbLoopBpm;
}

void BunkaAudioProcessor::setSlicesManual (const std::vector<int>& starts)
{
    if (sampleData == nullptr) return;

    const int n = sampleData->buffer.getNumSamples();
    std::vector<int> s;
    s.reserve (starts.size() + 1);
    for (int v : starts)
        if (v >= 0 && v < n) s.push_back (v);

    std::sort (s.begin(), s.end());
    s.erase (std::unique (s.begin(), s.end()), s.end());
    if (s.empty() || s.front() != 0) s.insert (s.begin(), 0);

    suspendProcessing (true);
    sampleData->sliceStarts = s;
    sampleData->sliceScores = SliceEngine::computeSliceScores (sampleData->buffer, s, sampleData->fileSampleRate);
    rebuildPatternsForSlices (sampleData->numSlices(), true);
    suspendProcessing (false);

    numSlicesAtomic.store (sampleData->numSlices());

    // keep the editor's re-fetch source in sync, but DON'T bump waveformVersion
    // (the open editor already holds these edits — avoids a fetch/repaint fight)
    {
        const juce::ScopedLock sl (thumbLock);
        thumbSlices = s;
    }
}

int BunkaAudioProcessor::snapSample (int sample, bool toGrid) const
{
    if (sampleData == nullptr) return sample;
    const int n = sampleData->buffer.getNumSamples();
    if (n <= 1) return 0;
    sample = juce::jlimit (0, n - 1, sample);

    if (toGrid)
    {
        const double beats   = SliceEngine::beatsForDivision ((int) pSnapDiv->load());  // independent of slice grid
        const double spacing = (60.0 / juce::jmax (1.0, sampleData->loopBpm))
                               * beats * sampleData->fileSampleRate;
        if (spacing >= 1.0)
            return juce::jlimit (0, n - 1, (int) std::llround (std::round ((double) sample / spacing) * spacing));
        return sample;
    }

    // nearest zero crossing (sign change on the mono sum) within ~5 ms
    const int W   = juce::jmax (1, (int) (0.005 * sampleData->fileSampleRate));
    const int nCh = sampleData->buffer.getNumChannels();
    auto mono = [&] (int i) -> float
    {
        float s = 0.f;
        for (int c = 0; c < nCh; ++c) s += sampleData->buffer.getSample (c, juce::jlimit (0, n - 1, i));
        return s;
    };

    int best = -1, bestDist = W + 1;
    const int lo = juce::jmax (1, sample - W), hi = juce::jmin (n - 1, sample + W);
    for (int i = lo; i <= hi; ++i)
    {
        const float a = mono (i - 1), b = mono (i);
        if ((a <= 0.f && b > 0.f) || (a >= 0.f && b < 0.f))
        {
            const int d = std::abs (i - sample);
            if (d < bestDist) { bestDist = d; best = i; }
        }
    }
    return best >= 0 ? best : sample;
}

//==============================================================================
void BunkaAudioProcessor::shuffleInto (std::vector<int>& v)
{
    const int N = (int) v.size();
    for (int k = 0; k < N; ++k) v[(size_t) k] = k;            // identity, then Fisher-Yates
    for (int k = N - 1; k > 0; --k)
    {
        std::uniform_int_distribution<int> d (0, k);
        std::swap (v[(size_t) k], v[(size_t) d (rng)]);
    }
}

void BunkaAudioProcessor::musicalShuffleInto (std::vector<int>& v)
{
    const int N = (int) v.size();
    if (N <= 0 || sampleData == nullptr || (int) sampleData->sliceScores.size() < N)
    {
        shuffleInto (v);                         // no analysis available -> plain permutation
        return;
    }

    const double L         = juce::jmax (1.0, (double) sampleData->buffer.getNumSamples());
    const double loopBeats = 4.0 * juce::jmax (1, sampleData->numBars);
    std::uniform_real_distribution<float> jit (0.f, 0.35f);

    // metric strength per slot (on-beat / downbeat = strong) + jitter
    std::vector<std::pair<float,int>> slot (N), slice (N);
    for (int k = 0; k < N; ++k)
    {
        const double posBeats = (sampleData->sliceStart (k) / L) * loopBeats;
        const double frac     = posBeats - std::floor (posBeats);
        float strength = 1.f - 2.f * (float) juce::jmin (frac, 1.0 - frac);          // beat phase
        const double barFrac = std::fmod (posBeats, loopBeats);
        if (juce::jmin (barFrac, loopBeats - barFrac) < 0.1) strength += 0.5f;        // downbeat bonus
        slot[(size_t) k]  = { strength + jit (rng), k };
        slice[(size_t) k] = { sampleData->sliceScores[(size_t) k] + jit (rng), k };   // lowness + jitter
    }

    std::sort (slot.begin(),  slot.end(),  [] (auto& a, auto& b) { return a.first > b.first; });
    std::sort (slice.begin(), slice.end(), [] (auto& a, auto& b) { return a.first > b.first; });

    for (int i = 0; i < N; ++i)                  // bassiest slice -> strongest slot
        v[(size_t) slot[(size_t) i].second] = slice[(size_t) i].second;
}

void BunkaAudioProcessor::randomizeInto (std::vector<int>& v)
{
    // Wilder than shuffle: rests (-1 = silence) + stutters (repeat previous) +
    // random jumps, so it reads as a glitchier re-imagining, not just a reorder.
    const int N = (int) v.size();
    if (N <= 0) return;
    std::uniform_real_distribution<float> u (0.f, 1.f);
    std::uniform_int_distribution<int>    pick (0, N - 1);
    int prev = pick (rng);
    for (int k = 0; k < N; ++k)
    {
        const float r = u (rng);
        if      (r < 0.18f)          v[(size_t) k] = -1;            // rest
        else if (r < 0.50f && k > 0) v[(size_t) k] = prev;          // stutter / roll
        else                         v[(size_t) k] = pick (rng);    // jump
        if (v[(size_t) k] >= 0) prev = v[(size_t) k];
    }
}

void BunkaAudioProcessor::rebuildPatternsForSlices (int n, bool keepIfSameCount)
{
    n = juce::jmax (1, n);
    if (keepIfSameCount && n == patternSliceCount) return;
    patternSliceCount = n;

    for (int p = 0; p < kNumPatterns; ++p)
    {
        if (patternLocked[(size_t) p]) continue;          // locked pads keep their groove
        patterns[(size_t) p].assign ((size_t) n, 0);
        for (int k = 0; k < n; ++k) patterns[(size_t) p][(size_t) k] = k;   // identity baseline
    }
    for (int p = 1; p < kNumPatterns; ++p)
        if (! patternLocked[(size_t) p]) musicalShuffleInto (patterns[(size_t) p]);  // 2..12 vary
}

void BunkaAudioProcessor::shufflePatterns()      // active pad only
{
    const int p = juce::jlimit (0, kNumPatterns - 1, activePattern.load());
    if (patternSliceCount <= 0 || patternLocked[(size_t) p]) return;
    suspendProcessing (true);
    musicalShuffleInto (patterns[(size_t) p]);
    suspendProcessing (false);
}

void BunkaAudioProcessor::randomizePatterns()    // active pad only
{
    const int p = juce::jlimit (0, kNumPatterns - 1, activePattern.load());
    if (patternSliceCount <= 0 || patternLocked[(size_t) p]) return;
    suspendProcessing (true);
    randomizeInto (patterns[(size_t) p]);
    suspendProcessing (false);
}

juce::String BunkaAudioProcessor::encodePatterns() const
{
    juce::String enc;
    for (int p = 0; p < kNumPatterns; ++p)
    {
        if (p) enc << ';';
        for (size_t k = 0; k < patterns[(size_t) p].size(); ++k)
        {
            if (k) enc << ',';
            enc << patterns[(size_t) p][k];
        }
    }
    return enc;
}

void BunkaAudioProcessor::decodePatterns (const juce::String& s)
{
    auto rows = juce::StringArray::fromTokens (s, ";", "");
    suspendProcessing (true);
    for (int p = 0; p < kNumPatterns && p < rows.size(); ++p)
    {
        auto toks = juce::StringArray::fromTokens (rows[p], ",", "");
        auto& vec = patterns[(size_t) p];
        vec.assign ((size_t) juce::jmax (1, toks.size()), 0);   // keep saved length (locked pads may differ)
        for (int k = 0; k < toks.size(); ++k)
        {
            const int val = toks[k].getIntValue();
            vec[(size_t) k] = val < 0 ? -1 : val;               // rests preserved; playback clamps to slice count
        }
    }
    suspendProcessing (false);
}

//==============================================================================
void BunkaAudioProcessor::parameterChanged (const juce::String&, float)
{
    reslicePending.store (true);
}

void BunkaAudioProcessor::timerCallback()
{
    if (reslicePending.exchange (false))
        reslice();
}

//==============================================================================
void BunkaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("bunkaFile", loadedPath, nullptr);
    state.setProperty ("bunkaPatterns", encodePatterns(), nullptr);
    state.setProperty ("bunkaActivePattern", activePattern.load(), nullptr);
    {
        juce::String locks;
        for (int p = 0; p < kNumPatterns; ++p) { if (p) locks << ','; locks << (patternLocked[(size_t) p] ? 1 : 0); }
        state.setProperty ("bunkaLocks", locks, nullptr);
    }
    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
        copyXmlToBinary (*xml, destData);
}

void BunkaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);

            const juce::String path = tree.getProperty ("bunkaFile", "").toString();
            if (path.isNotEmpty())
            {
                juce::File f (path);
                if (f.existsAsFile()) loadFile (f);     // rebuilds a fresh bank for the file
            }

            // restore the saved bank on top of the fresh one (sizes permitting)
            const juce::String pe = tree.getProperty ("bunkaPatterns", "").toString();
            if (pe.isNotEmpty()) decodePatterns (pe);
            activePattern.store (juce::jlimit (0, kNumPatterns - 1,
                                               (int) tree.getProperty ("bunkaActivePattern", 0)));

            const juce::String lk = tree.getProperty ("bunkaLocks", "").toString();
            if (lk.isNotEmpty())
            {
                auto t = juce::StringArray::fromTokens (lk, ",", "");
                for (int p = 0; p < kNumPatterns && p < t.size(); ++p)
                    patternLocked[(size_t) p] = t[p].getIntValue() != 0;
            }
        }
}

//==============================================================================
juce::AudioProcessorEditor* BunkaAudioProcessor::createEditor()
{
    return new BunkaAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BunkaAudioProcessor();
}
