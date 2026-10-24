cmake_minimum_required(VERSION 3.22)
project(TonicRecreation VERSION 0.1.0)
# JUCE can be supplied locally or fetched automatically on GitHub Actions.
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
  add_subdirectory("${JUCE_PATH}" JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
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
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(TonicRecreation)
target_sources(TonicRecreation PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h Source/RollingFrequencyMedian.h Source/FixedTemporalMedian.h Source/KeySilenceGate.h Source/KeyCandidateScore.h Source/StereoAnalysisMix.h Source/TempoOctaveGuard.h Source/TempoEvidenceGate.h Source/TempoContinuity.h Source/RelativeModeEvidence.h Source/TonalPhraseTracker.h Source/BeatAlignedHarmony.h Source/DownbeatChordTranscriber.h Source/TonalCenterModel.h Source/TonalModelWeights.h Source/TemporalTonalFeatures.h Source/TemporalTonalModel.h Source/TemporalModelWeights.h
    Source/PluginEditor.cpp Source/PluginEditor.h)
target_compile_features(TonicRecreation PRIVATE cxx_std_17)
target_link_libraries(TonicRecreation PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
target_compile_definitions(TonicRecreation PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

# JUCE-independent tonal phrase tests. Built in the same Windows GitHub run.
enable_testing()
add_executable(TonicPhraseTests tests/tonal_phrase_tracker_test.cpp Source/TonalPhraseTracker.h)
target_compile_features(TonicPhraseTests PRIVATE cxx_std_17)
add_test(NAME TonicPhraseTests COMMAND TonicPhraseTests)

add_executable(TonicBeatGridTests tests/beat_aligned_harmony_test.cpp Source/BeatAlignedHarmony.h)
target_compile_features(TonicBeatGridTests PRIVATE cxx_std_17)
add_test(NAME TonicBeatGridTests COMMAND TonicBeatGridTests)

add_executable(TonicDownbeatChordTests tests/downbeat_chord_test.cpp Source/DownbeatChordTranscriber.h Source/TonalCenterModel.h Source/TonalModelWeights.h)
target_compile_features(TonicDownbeatChordTests PRIVATE cxx_std_17)
add_test(NAME TonicDownbeatChordTests COMMAND TonicDownbeatChordTests)

add_executable(TonicChordEvidenceTests tests/chord_evidence_benchmark.cpp Source/DownbeatChordTranscriber.h Source/TonalCenterModel.h Source/TonalModelWeights.h)
target_compile_features(TonicChordEvidenceTests PRIVATE cxx_std_17)
add_test(NAME TonicChordEvidenceTests COMMAND TonicChordEvidenceTests)

# Pure C++ stereo cancellation regression; no JUCE dependency.
add_executable(TonicStereoMixTests tests/stereo_analysis_mix_test.cpp Source/StereoAnalysisMix.h)
target_compile_features(TonicStereoMixTests PRIVATE cxx_std_17)
add_test(NAME TonicStereoMixTests COMMAND TonicStereoMixTests)

# V41: portable, deterministic evidence-gating checks.
add_executable(TonicTempoOctaveGuardTests tests/tempo_octave_guard_test.cpp Source/TempoOctaveGuard.h)
target_compile_features(TonicTempoOctaveGuardTests PRIVATE cxx_std_17)
add_test(NAME TonicTempoOctaveGuardTests COMMAND TonicTempoOctaveGuardTests)

# V42: conservative recovery of repeated low-autocorrelation beat-grid evidence.
add_executable(TonicTempoEvidenceGateTests tests/tempo_evidence_gate_test.cpp Source/TempoEvidenceGate.h)
target_compile_features(TonicTempoEvidenceGateTests PRIVATE cxx_std_17)
add_test(NAME TonicTempoEvidenceGateTests COMMAND TonicTempoEvidenceGateTests)

# V43: independent tonic/relative mode evidence and uncertainty gating.
add_executable(TonicRelativeModeTests tests/relative_mode_evidence_test.cpp Source/RelativeModeEvidence.h)
target_compile_features(TonicRelativeModeTests PRIVATE cxx_std_17)
add_test(NAME TonicRelativeModeTests COMMAND TonicRelativeModeTests)

# Pure C++ V45 tempo continuity safeguard.
add_executable(TonicTempoContinuityTests tests/tempo_continuity_test.cpp Source/TempoContinuity.h)
target_compile_features(TonicTempoContinuityTests PRIVATE cxx_std_17)
add_test(NAME TonicTempoContinuityTests COMMAND TonicTempoContinuityTests)

# V47: pure portable model-feature tests; the default model is not certified.
add_executable(TonicTonalModelTests tests/tonal_center_model_test.cpp Source/TonalCenterModel.h Source/TonalModelWeights.h)
target_compile_features(TonicTonalModelTests PRIVATE cxx_std_17)
add_test(NAME TonicTonalModelTests COMMAND TonicTonalModelTests)

# V49: independent temporal features; no trained model is enabled.
add_executable(TonicTemporalTonalTests
    tests/temporal_tonal_features_test.cpp
    Source/TemporalTonalFeatures.h Source/TemporalTonalModel.h
    Source/TemporalModelWeights.h Source/TonalCenterModel.h Source/TonalModelWeights.h)
target_compile_features(TonicTemporalTonalTests PRIVATE cxx_std_17)
add_test(NAME TonicTemporalTonalTests COMMAND TonicTemporalTonalTests)

# V51: same spectral median as std::nth_element, without re-sorting every FFT bin.
add_executable(TonicSpectralMedianTests tests/spectral_median_test.cpp Source/RollingFrequencyMedian.h)
target_compile_features(TonicSpectralMedianTests PRIVATE cxx_std_17)
add_test(NAME TonicSpectralMedianTests COMMAND TonicSpectralMedianTests)

# V52: portable exact seven-frame temporal median tests.
add_executable(TonicTemporalMedianTests tests/temporal_median_test.cpp Source/FixedTemporalMedian.h)
target_compile_features(TonicTemporalMedianTests PRIVATE cxx_std_17)
add_test(NAME TonicTemporalMedianTests COMMAND TonicTemporalMedianTests)

# V53: tonal-context timeout and sample-rate parity (JUCE-independent).
add_executable(TonicKeySilenceGateTests tests/key_silence_gate_test.cpp Source/KeySilenceGate.h)
target_compile_features(TonicKeySilenceGateTests PRIVATE cxx_std_17)
add_test(NAME TonicKeySilenceGateTests COMMAND TonicKeySilenceGateTests)

# V53: identical key scoring in the standalone benchmark and VST3 processor.
add_executable(TonicKeyCandidateTests tests/key_candidate_score_test.cpp Source/KeyCandidateScore.h)
target_compile_features(TonicKeyCandidateTests PRIVATE cxx_std_17)
add_test(NAME TonicKeyCandidateTests COMMAND TonicKeyCandidateTests)

# V54: conservative evidence-fusion key tests, JUCE-independent.
add_executable(TonicKeyTemporalConsensusTests
    tests/key_temporal_consensus_test.cpp
    Source/KeyTemporalConsensus.h Source/KeyCandidateScore.h)
target_compile_features(TonicKeyTemporalConsensusTests PRIVATE cxx_std_17)
add_test(NAME TonicKeyTemporalConsensusTests COMMAND TonicKeyTemporalConsensusTests)
