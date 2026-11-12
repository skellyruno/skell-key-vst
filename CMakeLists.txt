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

# SkellCore is the canonical inter-plugin protocol. Pin the revision so
# recompiling old VST3 releases cannot silently change their wire format.
if(DEFINED SKELLCORE_PATH AND EXISTS "${SKELLCORE_PATH}/SkellCore/CMakeLists.txt")
    add_subdirectory("${SKELLCORE_PATH}/SkellCore" "${CMAKE_CURRENT_BINARY_DIR}/SkellCore")
else()
    include(FetchContent)
    FetchContent_Declare(SkellCoreRepo
        GIT_REPOSITORY https://github.com/skellyruno/skellcore-vst-template.git
        GIT_TAG 4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d
        GIT_SHALLOW FALSE)
    FetchContent_GetProperties(SkellCoreRepo)
    if(NOT skellcorerepo_POPULATED)
        FetchContent_Populate(SkellCoreRepo)
        add_subdirectory("${skellcorerepo_SOURCE_DIR}/SkellCore" "${CMAKE_CURRENT_BINARY_DIR}/SkellCore")
    endif()
endif()

juce_add_plugin(TonicRecreation
    COMPANY_NAME "Skell Audio"
    BUNDLE_ID "audio.independent.tonic-recreation"
    PLUGIN_MANUFACTURER_CODE InRc
    PLUGIN_CODE TnRc
    FORMATS VST3
    PRODUCT_NAME "Skell Key"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
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
    juce::juce_gui_extra
    SkellCore::UI)
target_compile_definitions(TonicRecreation PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

# Standalone protocol validation. Does not need JUCE or a Windows machine.
option(SKELL_KEY_BUILD_SYNC_TESTS "Build SkellCore key/scale/A4 protocol tests" OFF)
if(SKELL_KEY_BUILD_SYNC_TESTS)
    enable_testing()
    add_executable(SkellKeyWorldTuningTests tests/test_world_tuning.cpp)
    target_link_libraries(SkellKeyWorldTuningTests PRIVATE SkellCore::DSP)
    add_test(NAME SkellKeyWorldTuningTests COMMAND SkellKeyWorldTuningTests)
    add_executable(SkellKeyTonalWindowTests tests/test_tonal_window.cpp)
    target_compile_features(SkellKeyTonalWindowTests PRIVATE cxx_std_17)
    add_test(NAME SkellKeyTonalWindowTests COMMAND SkellKeyTonalWindowTests)
endif()
