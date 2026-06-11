juce_add_plugin(Foldspace
    COMPANY_NAME            "XNULLX"
    BUNDLE_ID               "ai.xnullx.foldspace"
    PLUGIN_MANUFACTURER_CODE XNLX
    PLUGIN_CODE             Flds
    FORMATS                 VST3 AU         # AU is built on macOS only; ignored on Windows
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE            # FX with MIDI input (carrier pitch control)
    VST3_CATEGORIES         Fx Distortion
    AU_MAIN_TYPE            kAudioUnitType_MusicEffect   # 'aumf' — effect that accepts MIDI
    PRODUCT_NAME            "Foldspace")

juce_generate_juce_header(Foldspace)

target_sources(Foldspace PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Foldspace PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Foldspace
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
