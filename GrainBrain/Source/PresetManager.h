#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
    {
        presetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("GrainBrain/Presets");
        presetDir.createDirectory();
    }

    void savePreset(const juce::String& name)
    {
        juce::MemoryBlock data;
        apvts.state.writeToStream(juce::MemoryOutputStream(data, false));
        auto xml = apvts.copyState().createXml();
        xml->setAttribute("presetName", name);
        xml->writeTo(presetDir.getChildFile(name + ".xml"));
    }

    void loadPreset(const juce::String& name)
    {
        auto file = presetDir.getChildFile(name + ".xml");
        if (!file.existsAsFile()) return;
        auto xml = juce::XmlDocument::parse(file);
        if (xml && xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    void deletePreset(const juce::String& name)
    {
        presetDir.getChildFile(name + ".xml").deleteFile();
    }

    juce::StringArray getPresetNames()
    {
        juce::StringArray names;
        for (auto& f : presetDir.findChildFiles(juce::File::findFiles, false, "*.xml"))
            names.add(f.getFileNameWithoutExtension());
        names.sort(true);
        return names;
    }

    juce::File getPresetDir() const { return presetDir; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::File presetDir;
};