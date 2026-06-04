#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "GranularEngine.h"
#include "StutterGate.h"
#include "LFOEngine.h"
#include "MidiLearnManager.h"

class GrainBrainAudioProcessor : public juce::AudioProcessor
{
public:
    GrainBrainAudioProcessor();
    ~GrainBrainAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor()       const override { return true; }
    const juce::String getName() const override { return "GrainBrain"; }
    bool acceptsMidi()     const override { return true; }
    bool producesMidi()    const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms()  override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    juce::AbstractFifo abstractFifo{ 4096 };
    std::array<float, 4096> audioFifo;
    double currentBPM = 120.0;

    // Live (LFO-modulated) band center/width, published once per block so the
    // spectrum display can animate the band regions in sync with the audio thread.
    // Seeded with the default band frequencies / 50% width for the window before
    // prepareToPlay runs (C++17 std::atomic is not zero-initialised by default).
    std::atomic<float> liveCenter[4]{ 200.f, 1000.f, 5000.f, 12000.f };
    std::atomic<float> liveWidth[4]{ 50.f, 50.f, 50.f, 50.f };

    MidiLearnManager midiLearn;   // public — editor reads/writes this

private:
    static constexpr int NUM_BANDS = 4;

    juce::AudioParameterFloat* bandCenter[NUM_BANDS];
    juce::AudioParameterFloat* bandWidth[NUM_BANDS];
    juce::AudioParameterBool* bandEnabled[NUM_BANDS];
    juce::AudioParameterFloat* bandGain[NUM_BANDS];

    juce::AudioParameterFloat* grainSize[NUM_BANDS];
    juce::AudioParameterFloat* grainScatter[NUM_BANDS];
    juce::AudioParameterFloat* grainPitch[NUM_BANDS];
    juce::AudioParameterFloat* grainPitchSpread[NUM_BANDS];
    juce::AudioParameterFloat* grainDrive[NUM_BANDS];
    juce::AudioParameterFloat* grainMix[NUM_BANDS];
    juce::AudioParameterBool* grainReverse[NUM_BANDS];
    juce::AudioParameterBool* grainFreeze[NUM_BANDS];
    juce::AudioParameterBool* grainSync[NUM_BANDS];
    juce::AudioParameterChoice* grainDiv[NUM_BANDS];

    juce::AudioParameterBool* gateOn[NUM_BANDS];
    juce::AudioParameterFloat* gateDepth[NUM_BANDS];
    juce::AudioParameterFloat* gateAttack[NUM_BANDS];
    juce::AudioParameterFloat* gateRelease[NUM_BANDS];
    juce::AudioParameterFloat* gatePhase[NUM_BANDS];
    juce::AudioParameterChoice* gateDiv[NUM_BANDS];

    juce::AudioParameterFloat* feedbackAmt[NUM_BANDS];

    juce::AudioParameterBool* lfoOn[NUM_BANDS];
    juce::AudioParameterFloat* lfoRate[NUM_BANDS];
    juce::AudioParameterChoice* lfoShape[NUM_BANDS];
    juce::AudioParameterBool* lfoSync[NUM_BANDS];
    juce::AudioParameterChoice* lfoDiv[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthCenter[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseCenter[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthWidth[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseWidth[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthMix[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseeMix[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthPitch[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseePitch[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthScatter[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseScatter[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthGatePhase[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseGatePhase[NUM_BANDS];
    juce::AudioParameterFloat* lfoDepthSize[NUM_BANDS];
    juce::AudioParameterFloat* lfoPhaseSize[NUM_BANDS];

    juce::AudioParameterFloat* dryIn{ nullptr };
    juce::AudioParameterFloat* wetOut{ nullptr };
    juce::AudioParameterFloat* passthrough{ nullptr };

    juce::dsp::StateVariableTPTFilter<float> bandFilter[NUM_BANDS];

    GranularEngine granularL[NUM_BANDS];
    GranularEngine granularR[NUM_BANDS];
    StutterGate    gateL[NUM_BANDS];
    StutterGate    gateR[NUM_BANDS];
    LFOEngine      lfoEngine[NUM_BANDS];

    juce::AudioBuffer<float> feedbackBuf[NUM_BANDS];

    static constexpr float divValues[] = {
        4.f, 2.f, 1.f, 0.6667f, 0.5f, 0.3333f, 0.25f, 0.1667f
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainBrainAudioProcessor)
};