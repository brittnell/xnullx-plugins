juce_add_plugin(O2
    COMPANY_NAME            "XNULLX"
    BUNDLE_ID               "ai.xnullx.o2"
    PLUGIN_MANUFACTURER_CODE XNLX
    PLUGIN_CODE             O2XN
    FORMATS                 VST3 AU         # AU is built on macOS only; ignored on Windows
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    VST3_CATEGORIES         Fx
    AU_MAIN_TYPE            kAudioUnitType_Effect
    PRODUCT_NAME            "O2")

juce_generate_juce_header(O2)

target_sources(O2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(O2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(O2
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
