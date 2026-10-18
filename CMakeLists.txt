cmake_minimum_required(VERSION 3.22)
project(DelayPlugin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(DelayPlugin
    COMPANY_NAME "MyAudioDev"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Mydv
    PLUGIN_CODE Dlay
    FORMATS VST3 AU
    PRODUCT_NAME "Custom Delay"
)

juce_generate_juce_header(DelayPlugin)

# FIX: Disable legacy VST2 preset compatibility in the VST3 wrapper
target_compile_definitions(DelayPlugin
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_sources(DelayPlugin
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/SkellLookAndFeel.h
        Source/SkellComponents.h
)

# Add BinaryData for assets
juce_add_binary_data(DelayAssets
    SOURCES
        Source/Assets/skell_bg.png
)

target_link_libraries(DelayPlugin
    PRIVATE
        DelayAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
