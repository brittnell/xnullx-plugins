#pragma once
#include <JuceHeader.h>

class O2PresetManager
{
public:
    explicit O2PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a) {}

    juce::File getPresetsDir() const
    {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("O2").getChildFile("Presets");
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        for (auto& f : getPresetsDir().findChildFiles(juce::File::findFiles, false, "*.xml"))
            names.add(f.getFileNameWithoutExtension());
        names.sort(false);
        return names;
    }

    void savePreset(const juce::String& name)
    {
        getPresetsDir().createDirectory();
        auto state = apvts.copyState();
        if (auto xml = std::unique_ptr<juce::XmlElement>(state.createXml()))
            xml->writeTo(getPresetsDir().getChildFile(name + ".xml"));
    }

    void loadPreset(const juce::String& name)
    {
        auto f = getPresetsDir().getChildFile(name + ".xml");
        if (!f.existsAsFile()) return;
        if (auto xml = juce::XmlDocument::parse(f))
            if (xml->hasTagName(apvts.state.getType()))
                apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    void deletePreset(const juce::String& name)
    {
        getPresetsDir().getChildFile(name + ".xml").deleteFile();
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};
