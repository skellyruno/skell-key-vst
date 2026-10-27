#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <atomic>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "TonalPhraseTracker.h"
#include "BeatAlignedHarmony.h"
#include "TempoEvidenceGate.h"
#include "TempoContinuity.h"
#include "RelativeModeEvidence.h"
#include "DownbeatChordTranscriber.h"
#include "TonalCenterModel.h"
#include "TemporalTonalFeatures.h"
#include "KeyTemporalConsensus.h"

class TonicRecreationAudioProcessor final : public juce::AudioProcessor {
public:
    TonicRecreationAudioProcessor();
    ~TonicRecreationAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Skell Key"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    // Click-to-hear preview of the detected key. Message thread requests, audio thread renders.
    void playKeyPreview(int root,int mode);
    int getPreviewPitchClass() const { return previewNote.load(std::memory_order_relaxed); } // -1 when silent
    float getInputLevel() const { return inputLevel.load(std::memory_order_relaxed); }
    void requestReset() { resetRequested.store(true, std::memory_order_release); analysisReady.signal(); }
    // Called only on the message thread (drag/drop or file chooser).
    bool requestFileAnalysis(const juce::File& file);
    void returnToLive();
    bool hasLiveInput() const { return liveInputDetected.load(std::memory_order_relaxed); }
    bool isOfflineMode() const { return offlineMode.load(std::memory_order_relaxed); }
    bool isFileBusy() const { return fileBusy.load(std::memory_order_relaxed); }
    float getFileProgress() const { return fileProgress.load(std::memory_order_relaxed); }
    float getFileDurationSeconds() const { return fileDurationSeconds.load(std::memory_order_relaxed); }
    float getFileAnalyzedSeconds() const { return fileAnalyzedSeconds.load(std::memory_order_relaxed); }
    juce::String getFileName() const;
    bool fileFailed() const { return fileError.load(std::memory_order_relaxed); }
    void setPaused(bool v) { paused.store(v); }
    bool isPaused() const { return paused.load(); }
    int getDetectedRoot() const { return detectedRoot.load(); }
    int getDetectedMode() const { return detectedMode.load(); }
    float getConfidence() const { return confidence.load(); }
    float getDetectedTuningHz() const { return detectedTuningHz.load(); }
    float getTuningConfidence() const { return tuningConfidence.load(); }
    float getDetectedBpm() const { return detectedBpm.load(std::memory_order_relaxed); }
    float getBpmConfidence() const { return bpmConfidence.load(std::memory_order_relaxed); }
    float getTempoAmbiguity() const { return tempoAmbiguity.load(std::memory_order_relaxed); }
    float getHostBpm() const { return hostBpm.load(std::memory_order_relaxed); }
    bool hasHostBpm() const { return hostBpmValid.load(std::memory_order_relaxed); }
    int getTranscribedChordRoot() const { return transcribedChordRoot.load(); }
    int getTranscribedChordQuality() const { return transcribedChordQuality.load(); }
    int getTranscribedChordBass() const { return transcribedChordBass.load(); }
    float getTranscribedChordEvidence() const { return transcribedChordEvidence.load(); }
    int getChordRoot() const { return chordRoot.load(); }
    int getChordKind() const { return chordKind.load(); } // 0 maj 1 min 2 dim 3 aug
    float getChordConfidence() const { return chordConfidence.load(); }
    int getRankedRoot(int i) const { return candidateRoots[(size_t)juce::jlimit(0,2,i)].load(); }
    int getRankedMode(int i) const { return candidateModes[(size_t)juce::jlimit(0,2,i)].load(); }
    float getRankedScore(int i) const { return candidateScores[(size_t)juce::jlimit(0,2,i)].load(); }
    float getChroma(int i) const { return liveChroma[(size_t)juce::jlimit(0,11,i)].load(); }
    juce::AudioProcessorValueTreeState& getState() { return state; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
private:
    static constexpr int windowSize = 16384, hopSize = 4096, fifoSize = 65536;
    juce::AudioProcessorValueTreeState state;
    std::atomic<bool> paused {false}, resetRequested {false};
    std::atomic<int> detectedRoot {-1}, detectedMode {2}, chordRoot {-1}, chordKind {-1};
    std::atomic<float> confidence {0.0f}, chordConfidence {0.0f}, detectedTuningHz {440.0f}, tuningConfidence {0.0f};
    std::array<std::atomic<float>,12> liveChroma {};
    std::array<std::atomic<int>,3> candidateRoots {}, candidateModes {};
    std::array<std::atomic<float>,3> candidateScores {};
    std::atomic<float> detectedBpm {0.0f}, bpmConfidence {0.0f}, hostBpm {0.0f};
    std::atomic<bool> hostBpmValid {false};
    static constexpr int onsetHistoryLength = 4200; // up to 42 seconds at 100 Hz; worker thread only
    static constexpr int onsetHopRate = 100;
    std::array<float,onsetHistoryLength> onsetHistory {};
    int onsetWrite=0, onsetCount=0, onsetSamples=0, bpmAnalysisFrames=0, tempoSilentHops=0;
    std::atomic<float> tempoAmbiguity {0.0f};
    float onsetEnergySum=0.0f, onsetHighEnergySum=0.0f;
    float tempoHighpassDecay=0.0f; // Calculated on worker at sample-rate changes, never per sample
    float tempoLowpass=0.0f, previousEnergy=0.0f, previousHighEnergy=0.0f, smoothedOnset=0.0f;
    float lastStableBpm=0.0f, loopFastBpm=0.0f; // loopFastBpm: tempo confirmed by loop-length fit
    skell::TempoEvidenceGate weakTempoGate;
    skell::TempoContinuity tempoContinuity;
    void consumeTempoSample(float sample);
    void estimateTempo();
    double currentSampleRate=44100.0;
    // Only touched by the background analysis worker, except before worker starts.
    double analysisSampleRate=44100.0;
    std::atomic<bool> liveInputDetected {false}, offlineMode {false}, fileBusy {false},
                      fileError {false}, offlineCancel {false};
    std::atomic<float> fileProgress {0.0f}, fileDurationSeconds {0.0f}, fileAnalyzedSeconds {0.0f};
    std::atomic<unsigned int> fileRequestGeneration {0};
    int silentAudioSamples=0; // Audio callback only; elapsed-sample based, not block-count based
    mutable juce::CriticalSection fileRequestLock; // Never taken by audio callback
    juce::String pendingFilePath, displayedFileName;
    bool fileRequestPending=false;
    unsigned int pendingFileGeneration=0;
    void analyseAudioSample(float value);
    void analyseFile(const juce::String& path, unsigned int generation);
    void discardLiveQueue();
    juce::AbstractFifo sampleFifo {fifoSize};
    std::array<float,fifoSize> sampleQueue {};
    juce::WaitableEvent analysisReady;
    class AnalysisWorker final : public juce::Thread {
    public:
        explicit AnalysisWorker(TonicRecreationAudioProcessor& p):juce::Thread("Skell Key analysis"), parent(p) {}
        void run() override;
        TonicRecreationAudioProcessor& parent;
    } worker {*this};
    juce::dsp::FFT fft {14};
    std::array<float,windowSize> history {};
    std::vector<float> fftBuffer;
    std::array<float,windowSize/2> spectrumPower {};
    // Causal median time/frequency separation on the analysis worker only.
    static constexpr int harmonicFrames = 7;
    std::array<std::array<float,windowSize/2>,harmonicFrames> spectralHistory {};
    std::array<float,windowSize/2> tonalPower {};
    std::array<float,windowSize/2> harmonicMask {};
    int harmonicWrite = 0, harmonicCount = 0;
    std::array<float,60> noteFrequencies {};
    int tempoHopSamples=441;
    float actualOnsetRate=100.0f;
    void rebuildFrequencyMap(double sampleRate);
    std::array<float,windowSize> hannWindow {};
    skell::TonalPhraseTracker phraseTracker;
    skell::BeatAlignedHarmony beatGrid;
    skell::DownbeatChordTracker downbeatChordTracker;
    skell::RelativeModeEvidence relativeModeEvidence;
    skell::KeyEvidenceConsistency keyConsistency;
    skell::KeyTemporalConsensus temporalKeyConsensus;
    skell::TonalCenterFeatures learnedTonalFeatures;
    skell::TemporalTonalFeatures temporalTonalFeatures;
    std::atomic<int> transcribedChordRoot{-1},transcribedChordQuality{-1},transcribedChordBass{-1};
    std::atomic<float> transcribedChordEvidence{0.0f};
    uint64_t analyzedSamples = 0;
    skell::BassFrame lastBassFrame {};
    std::array<float,12> chordMemory {};
    // Worker-owned sparse chord-change evidence, not frame-duration weighting.
    std::array<float,12> progressionRootEvidence {};
    int lastProgressionRoot=-1, lastProgressionKind=-1;
    int progressionEvents=0;
    int heldChordRoot=-1, heldChordKind=-1, chordHold=0;
    float tuningCentsMemory=0.0f, tuningHistMass=0.0f;
    std::array<float,100> tuningHist {}; // decayed histogram of peak deviations from the 440 Hz grid, 1 cent/bin
    int tuningFrames=0;
    bool tuningApplied=false; // worker-only: tuned note grid in use
    std::array<float,12> chromaMemory {};
    std::array<float,12> keyChroma {}; // long-term (~20 s) pitch-class evidence used for the key ranking
    int keyChromaFrames=0;
    std::array<float,12> startChroma {}, endChroma {}; // file analysis: opening / closing pitch classes
    bool holdResultsOnSilence=false; // worker-only: true while analysing a file
    int historyWrite=0, historyCount=0, samplesSinceWindow=0, silentFrames=0;
    int stableRoot=-1, stableMode=2, pendingRoot=-1, pendingMode=2, pendingCount=0;
    void analyseBlock(juce::AudioBuffer<float>&);
    void renderPreview(juce::AudioBuffer<float>&);
    std::atomic<float> inputLevel {0.0f};
    std::atomic<bool> previewRequest {false};
    std::atomic<int> previewRoot {0}, previewMode {0}, previewNote {-1};
    struct PreviewVoice { bool active=false; int midi=60; double phase=0.0; juce::int64 start=0, length=0; };
    std::array<PreviewVoice,12> voices {};
    juce::int64 previewClock=0, previewTotal=0; bool previewRunning=false; // audio thread only
    void analyseWindow();
    void updateHarmonicMask();
    void resetAnalysis();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonicRecreationAudioProcessor)
};
