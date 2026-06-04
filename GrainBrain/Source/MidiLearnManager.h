#pragma once
#include <JuceHeader.h>

//==============================================================================
// MidiLearnManager
//
// Opens MIDI devices directly so CC learn works regardless of host routing.
// Call openMidiDevices() once from PluginProcessor::prepareToPlay().
//==============================================================================
class MidiLearnManager : public juce::MidiInputCallback
{
public:
    MidiLearnManager() = default;

    ~MidiLearnManager() override
    {
        closeMidiDevices();
    }

    //==========================================================================
    // Direct MIDI device connection — bypasses host routing
    //==========================================================================
    void openMidiDevices(juce::AudioProcessorValueTreeState* apvts)
    {
        mApvts = apvts;
        juce::ScopedLock sl(mLock);
        mMidiInputs.clear();
        for (auto& device : juce::MidiInput::getAvailableDevices())
        {
            if (auto input = juce::MidiInput::openDevice(device.identifier, this))
            {
                input->start();
                mMidiInputs.push_back(std::move(input));
            }
        }
    }

    void closeMidiDevices()
    {
        juce::ScopedLock sl(mLock);
        for (auto& input : mMidiInputs) input->stop();
        mMidiInputs.clear();
    }

    // Called on a background thread by JUCE's MIDI device manager
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) override
    {
        if (msg.isController() && mApvts != nullptr)
            handleCC(msg.getControllerNumber(), msg.getControllerValue(), *mApvts);
    }

    //==========================================================================
    // Also handle CC from the host's MIDI buffer (processBlock fallback)
    //==========================================================================
    void handleCC(int ccNumber, int ccValue, juce::AudioProcessorValueTreeState& apvts)
    {
        juce::ScopedLock sl(mLock);

        if (mLearningParamID.isNotEmpty())
        {
            // Remove any existing mapping for this CC number
            auto existing = mCCToParam.find(ccNumber);
            if (existing != mCCToParam.end())
                mParamToCC.erase(existing->second);

            // Remove old CC for this param if it had one
            auto oldCC = mParamToCC.find(mLearningParamID);
            if (oldCC != mParamToCC.end())
                mCCToParam.erase(oldCC->second);

            mCCToParam[ccNumber] = mLearningParamID;
            mParamToCC[mLearningParamID] = ccNumber;
            mLearningParamID = {};
            mDirty = true;
            return;
        }

        // Normal operation — apply mapped CC value to its parameter
        auto it = mCCToParam.find(ccNumber);
        if (it != mCCToParam.end())
            if (auto* param = apvts.getParameter(it->second))
                param->setValueNotifyingHost(ccValue / 127.f);
    }

    //==========================================================================
    // Learn mode
    //==========================================================================
    void startLearning(const juce::String& paramID)
    {
        juce::ScopedLock sl(mLock);
        mLearningParamID = paramID;
    }

    void stopLearning()
    {
        juce::ScopedLock sl(mLock);
        mLearningParamID = {};
    }

    bool isLearning() const
    {
        juce::ScopedLock sl(mLock);
        return mLearningParamID.isNotEmpty();
    }

    juce::String getLearningParamID() const
    {
        juce::ScopedLock sl(mLock);
        return mLearningParamID;
    }

    //==========================================================================
    // Queries
    //==========================================================================
    int getCCForParam(const juce::String& paramID) const
    {
        juce::ScopedLock sl(mLock);
        auto it = mParamToCC.find(paramID);
        return it != mParamToCC.end() ? it->second : -1;
    }

    void clearParam(const juce::String& paramID)
    {
        juce::ScopedLock sl(mLock);
        // If we're currently learning this param, cancel learn too
        if (mLearningParamID == paramID)
            mLearningParamID = {};
        auto it = mParamToCC.find(paramID);
        if (it != mParamToCC.end())
        {
            mCCToParam.erase(it->second);
            mParamToCC.erase(it);
            mDirty = true;
        }
    }

    void clearAll()
    {
        juce::ScopedLock sl(mLock);
        mLearningParamID = {};   // cancel any pending learn
        mCCToParam.clear();
        mParamToCC.clear();
        mDirty = true;
    }

    //==========================================================================
    // Persistence
    //==========================================================================
    bool isDirty() const { return mDirty; }

    void saveToFile(const juce::File& file)
    {
        juce::ScopedLock sl(mLock);
        file.getParentDirectory().createDirectory();
        juce::XmlElement root("MidiMap");
        for (auto& [cc, param] : mCCToParam)
        {
            auto* entry = root.createNewChildElement("Entry");
            entry->setAttribute("cc", cc);
            entry->setAttribute("param", param);
        }
        root.writeTo(file);
        mDirty = false;
    }

    void loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile()) return;
        juce::ScopedLock sl(mLock);
        mCCToParam.clear();
        mParamToCC.clear();
        if (auto xml = juce::XmlDocument::parse(file))
        {
            for (auto* entry : xml->getChildIterator())
            {
                int          cc = entry->getIntAttribute("cc", -1);
                juce::String param = entry->getStringAttribute("param");
                if (cc >= 0 && param.isNotEmpty())
                {
                    mCCToParam[cc] = param;
                    mParamToCC[param] = cc;
                }
            }
        }
    }

private:
    mutable juce::CriticalSection    mLock;
    juce::String                     mLearningParamID;
    std::map<int, juce::String> mCCToParam;
    std::map<juce::String, int>          mParamToCC;
    bool                             mDirty = false;

    juce::AudioProcessorValueTreeState* mApvts = nullptr;
    std::vector<std::unique_ptr<juce::MidiInput>> mMidiInputs;
};