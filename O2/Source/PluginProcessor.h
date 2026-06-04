#pragma once
#include <JuceHeader.h>
#include "SaturationEngine.h"
#include <random>

class O2AudioProcessor : public juce::AudioProcessor
{
public:
    O2AudioProcessor();
    ~O2AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor()       const override { return true; }
    const juce::String getName() const override { return "O2"; }
    bool acceptsMidi()     const override { return false; }
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

    // Peak metering — read by editor timer
    std::atomic<float> inputPeak  { 0.f };
    std::atomic<float> outputPeak { 0.f };

private:
    static constexpr int NUM_BANDS = 3;

    // ── Global parameters ────────────────────────────────────────────────
    juce::AudioParameterFloat*  pInputTrim    { nullptr };
    juce::AudioParameterFloat*  pLowXover     { nullptr };
    juce::AudioParameterFloat*  pHighXover    { nullptr };
    juce::AudioParameterFloat*  pOutputLevel  { nullptr };
    juce::AudioParameterFloat*  pDryWet       { nullptr };
    juce::AudioParameterBool*   pMono         { nullptr };
    juce::AudioParameterBool*   pMasterBypass { nullptr };
    juce::AudioParameterBool*   pOversample   { nullptr };
    juce::AudioParameterBool*   pWiden        { nullptr };

    // ── Per-band parameters ──────────────────────────────────────────────
    juce::AudioParameterChoice* pSatType   [NUM_BANDS];
    juce::AudioParameterChoice* pSatVariant[NUM_BANDS];
    juce::AudioParameterFloat*  pDrive     [NUM_BANDS];
    juce::AudioParameterFloat*  pWidth     [NUM_BANDS];
    juce::AudioParameterFloat*  pLevel     [NUM_BANDS];
    juce::AudioParameterFloat*  pFeedback  [NUM_BANDS];
    juce::AudioParameterFloat*  pHiss      [NUM_BANDS];
    juce::AudioParameterFloat*  pDrift     [NUM_BANDS];
    juce::AudioParameterBool*   pFlipPhase [NUM_BANDS];
    juce::AudioParameterBool*   pGateFbk   [NUM_BANDS];
    juce::AudioParameterBool*   pBandBypass[NUM_BANDS];
    juce::AudioParameterBool*   pBandMute  [NUM_BANDS];

    // ── DSP ──────────────────────────────────────────────────────────────
    SaturationEngine mSatEngine[NUM_BANDS];

    // Crossover TPT state (stereo)
    float mLP1State[2] = {};
    float mLP2State[2] = {};

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversamplers[NUM_BANDS];

    // Band buffers
    juce::AudioBuffer<float> mBandBuf[NUM_BANDS];

    // Feedback state — last processed sample per band per channel
    float mFbState[NUM_BANDS][2] = {};

    // Drift LFO per band
    float mDriftPhase[NUM_BANDS] = {};
    float mDriftFreq [NUM_BANDS] = {};

    // Mono-widen decorrelation delay line per band (delayed mid → synthetic side)
    static constexpr int kWidenMax = 256;
    float mWidenBuf[NUM_BANDS][kWidenMax] = {};
    int   mWidenIdx[NUM_BANDS] = {};
    int   mWidenLen = 40;

    // Hiss filter state (one-pole LP per band per channel)
    float mHissFilter[NUM_BANDS][2] = {};

    // Gate envelope follower per band
    float mGateEnv[NUM_BANDS] = {};

    // Peak smoothing
    float mInPeakSmooth  = 0.f;
    float mOutPeakSmooth = 0.f;

    // RNG for hiss and drift
    std::mt19937 mRng { std::random_device{}() };
    std::uniform_real_distribution<float> mDist { 0.f, 1.f };

    // Helper: first-order TPT lowpass
    static float tpt1p(float x, float g, float& s)
    {
        float v = g * (x - s) / (1.f + g);
        float y = v + s;
        s = y + v;
        return y;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(O2AudioProcessor)
};
