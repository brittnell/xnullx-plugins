#pragma once
#include <JuceHeader.h>
#include "PMEngine.h"
#include "PitchTracker.h"
#include "EnvelopeFollower.h"

//==============================================================================
// Foldspace — phase modulation x wavefolding effect.
//
// One chain, wavefolding at three blendable insertion points:
//   (1) MOD FOLD  — folds the modulator bus before it drives the PM stage
//   (2) MAIN FOLD — folds the PM output (morphable tri/sine, symmetry, env)
//   (3) FB FOLD   — folds inside the feedback loop (output -> own PM input)
//
// Carrier blend: SIGNAL (input audio PM'd via modulated delay line) <-> OSC
// (internal carrier at tracked/MIDI/free pitch, played by the input).
// Whole chain runs at the oversampled rate; folders use first-order ADAA.
//==============================================================================
class FoldspaceAudioProcessor : public juce::AudioProcessor
{
public:
    FoldspaceAudioProcessor();
    ~FoldspaceAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }

    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                     { return true; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 0.1; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram(int) override                  {}
    const juce::String getProgramName(int) override       { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // UI taps (audio thread writes, editor reads)
    std::atomic<float> uiPitchHz   { 0.f };
    std::atomic<float> uiPitchConf { 0.f };
    std::atomic<float> uiEnv       { 0.f };
    std::atomic<float> uiInPeak    { 0.f };
    std::atomic<float> uiOutPeak   { 0.f };

    static constexpr int scopeSize = 512;
    float scopeBuf[scopeSize] = {};
    std::atomic<int> scopeWrite { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyOsSetting(int osIndex);   // resets rates, smoothers, latency

    //==========================================================================
    PMChannel        channels[2];
    PitchTracker     tracker;
    EnvelopeFollower envFollower;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelay;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float>       envBuf;

    double baseRate = 44100.0;
    double procRate = 44100.0;   // baseRate * current OS factor
    int    numCh    = 2;
    int    curOsIndex = -1;      // forces applyOsSetting on first block
    float  dryDelaySamps = 0.f;

    // pitch state
    float  midiNoteHz = 110.f;
    float  hzState    = 110.f;   // glided carrier base pitch
    float  medBuf[5]  = { 110.f, 110.f, 110.f, 110.f, 110.f };
    int    medIdx     = 0;

    // drift LFO (random-rate, random-target)
    double driftPhase = 0.0;
    double driftRate  = 0.2;
    float  dv1 = 0.f, dv2 = 0.f, dv1T = 0.f, dv2T = 0.f;
    juce::Random rng;

    // oscillator phases (shared across channels; R adds stereo offset)
    double modPhase = 0.0, carPhase = 0.0;

    // smoothers — base rate
    juce::SmoothedValue<float> smIn, smOut, smMix;
    // smoothers — processing (oversampled) rate
    juce::SmoothedValue<float> smSrcOsc, smSrcIn, smFb, smModFold,
                               smIdx, smEnvIdx, smCar, smStereo,
                               smFolds, smEnvFold, smShape, smBias, smFbFold;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoldspaceAudioProcessor)
};
