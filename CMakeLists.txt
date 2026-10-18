cmake_minimum_required(VERSION 3.22)
project(TonicRecreation VERSION 0.1.0)
# Configure with -DJUCE_PATH=/path/to/JUCE (JUCE 7+ recommended).
if(NOT DEFINED JUCE_PATH)
  message(FATAL_ERROR "Set JUCE_PATH to a local JUCE checkout")
endif()
add_subdirectory("${JUCE_PATH}" JUCE)
juce_add_plugin(TonicRecreation
    COMPANY_NAME "Independent Recreation"
    BUNDLE_ID "audio.independent.tonic-recreation"
    PLUGIN_MANUFACTURER_CODE InRc
    PLUGIN_CODE TnRc
    FORMATS VST3
    PRODUCT_NAME "TONIC-inspired"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)
target_sources(TonicRecreation PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h)
target_compile_features(TonicRecreation PRIVATE cxx_std_17)
target_link_libraries(TonicRecreation PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
target_compile_definitions(TonicRecreation PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
