cmake_minimum_required(VERSION 3.22)
project(TonicRecreation VERSION 0.1.0 LANGUAGES C CXX)

# Use -DJUCE_PATH=/path/to/JUCE when available; otherwise fetch JUCE 8.
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(TonicRecreation
    COMPANY_NAME "Independent Recreation"
    BUNDLE_ID "audio.independent.tonic-recreation"
    PLUGIN_MANUFACTURER_CODE InRc
    PLUGIN_CODE TnRc
    FORMATS VST3
    PRODUCT_NAME "TONIC-inspired"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Needed by the current source's #include <JuceHeader.h>.
juce_generate_juce_header(TonicRecreation)

file(GLOB TONIC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
target_sources(TonicRecreation PRIVATE ${TONIC_SOURCES})
target_compile_features(TonicRecreation PRIVATE cxx_std_17)
target_link_libraries(TonicRecreation PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra)
target_compile_definitions(TonicRecreation PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
