juce_add_plugin(GrainBrain
    COMPANY_NAME            "XNULLX"
    BUNDLE_ID               "ai.xnullx.grainbrain"
    PLUGIN_MANUFACTURER_CODE XNLX
    PLUGIN_CODE             Cl7u            # matches the existing Windows build's ID
    FORMATS                 VST3 AU         # AU is built on macOS only; ignored on Windows
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    VST3_CATEGORIES         Fx
    AU_MAIN_TYPE            kAudioUnitType_Effect
    PRODUCT_NAME            "GrainBrain")

juce_generate_juce_header(GrainBrain)

target_sources(GrainBrain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GrainBrain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_WINRT_MIDI=1)

target_link_libraries(GrainBrain
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
