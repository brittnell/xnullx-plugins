juce_add_plugin(Bunka
    COMPANY_NAME            "XNULLX"
    BUNDLE_ID               "ai.xnullx.bunka"
    PLUGIN_MANUFACTURER_CODE XNLX
    PLUGIN_CODE             Bnka
    FORMATS                 VST3 AU         # AU is built on macOS only; ignored on Windows
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    VST3_CATEGORIES         Instrument
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
    PRODUCT_NAME            "Bunka")

juce_generate_juce_header(Bunka)

target_sources(Bunka PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Bunka PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Bunka
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
