#pragma once
#include <JuceHeader.h>
#include "SliceEngine.h"
#include "SamplerEngine.h"
#include <vector>
#include <array>
#include <random>

//==============================================================================
class BunkaAudioProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::Timer
{
public:
    BunkaAudioProcessor();
    ~BunkaAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Bunka"; }
    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    //== Editor-facing API ====================================================
    void loadFile (const juce::File&);
    juce::String getLoadedFileName() const { return loadedName; }

    int    getWaveformVersion() const noexcept { return waveformVersion.load(); }
    void   getWaveformSnapshot (std::vector<float>& minMaxOut,
                                std::vector<int>&   sliceStartsOut,
                                int& numSamplesOut, double& loopBpmOut);

    double getCurrentBpm() const noexcept { return currentBPM.load(); }
    int    getNumSlices()  const noexcept { return numSlicesAtomic.load(); }
    static int getSliceFloorNote() noexcept { return 48; }   // C3 (MIDI 48)

    void triggerPreview() { previewRequested.store (true); }     // audition raw file
    void setSlicesManual (const std::vector<int>& starts);       // editor drag/add/delete
    int  snapSample (int sample, bool toGrid) const;             // zero-cross / grid snap on drop

    //== Pattern bank =========================================================
    static constexpr int kNumPatterns = 12;
    void launchPattern (int idx) { activePattern.store (juce::jlimit (0, kNumPatterns - 1, idx)); }
    int  getActivePattern() const noexcept { return activePattern.load(); }
    void shufflePatterns();
    void randomizePatterns();
    static constexpr int getNumPatterns() noexcept { return kNumPatterns; }

    bool isPatternLocked (int idx) const { return patternLocked[(size_t) juce::jlimit (0, kNumPatterns - 1, idx)]; }
    void setPatternLocked (int idx, bool b) { patternLocked[(size_t) juce::jlimit (0, kNumPatterns - 1, idx)] = b; }

    std::atomic<int> lastTriggeredSlice { -1 };

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void reslice();                                   // message thread
    void applySlicing (BunkaSampleData&);             // fills sliceStarts + loopBpm
    void rebuildWaveformThumb (BunkaSampleData::Ptr);

    // pattern bank helpers
    void rebuildPatternsForSlices (int n, bool keepIfSameCount);
    void shuffleInto        (std::vector<int>&);    // plain permutation (fallback)
    void musicalShuffleInto (std::vector<int>&);    // bassy slices -> strong beats
    void randomizeInto      (std::vector<int>&);
    juce::String encodePatterns() const;
    void decodePatterns (const juce::String&);

    BunkaSampler             sampler;
    BunkaSampleData::Ptr     sampleData;              // swapped under suspendProcessing
    juce::AudioFormatManager formatManager;
    juce::String             loadedName, loadedPath;
    double                   sampleRate = 44100.0;

    std::atomic<double> currentBPM      { 120.0 };
    std::atomic<int>    numSlicesAtomic { 0 };
    std::atomic<bool>   reslicePending  { false };
    std::atomic<bool>   previewRequested { false };
    std::atomic<int>    waveformVersion { 0 };

    static constexpr int kThumbBins = 1024;
    juce::CriticalSection thumbLock;
    std::vector<float>    thumbMinMax;                // kThumbBins * 2 (min,max)
    std::vector<int>      thumbSlices;
    int                   thumbNumSamples = 1;
    double                thumbLoopBpm    = 120.0;

    // pattern bank
    std::array<std::vector<int>, kNumPatterns> patterns;
    std::array<bool, kNumPatterns>             patternLocked {};   // protected from regen
    std::atomic<int> activePattern   { 0 };
    int              patternSliceCount = 0;
    std::mt19937     rng { (unsigned) juce::Time::currentTimeMillis() };   // message thread (generation)
    juce::Random     audioRng;                                            // audio thread (probability)

    // cached raw parameter pointers
    std::atomic<float>* pSeqOn         = nullptr;
    std::atomic<float>* pChance        = nullptr;
    std::atomic<float>* pSnapDiv       = nullptr;
    std::atomic<float>* pGain          = nullptr;
    std::atomic<float>* pPitch         = nullptr;
    std::atomic<float>* pAttack        = nullptr;
    std::atomic<float>* pRelease       = nullptr;
    std::atomic<float>* pPitchPreserve = nullptr;
    std::atomic<float>* pReverse       = nullptr;
    std::atomic<float>* pHostSync      = nullptr;
    std::atomic<float>* pSliceMode     = nullptr;
    std::atomic<float>* pThreshold     = nullptr;
    std::atomic<float>* pBars          = nullptr;
    std::atomic<float>* pGridDiv       = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BunkaAudioProcessor)
};
