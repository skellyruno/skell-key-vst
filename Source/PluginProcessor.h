#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include <juce_dsp/juce_dsp.h>

class TonicRecreationAudioProcessor final : public juce::AudioProcessor
{
public:
    TonicRecreationAudioProcessor();
    ~TonicRecreationAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TONIC-inspired"; }
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

    void setPaused(bool b) { paused.store(b); }
    bool isPaused() const { return paused.load(); }
    float getChroma(int i) const { return liveChroma[(size_t)i].load(); }
    int getDetectedRoot() const { return detectedRoot.load(); }
    int getDetectedMode() const { return detectedMode.load(); } // 0 major, 1 minor, 2 uncertain
    float getConfidence() const { return confidence.load(); }
    float getTuningHz() const { return tuningHz.load(); }
    juce::AudioProcessorValueTreeState& getState() { return state; }
    juce::AudioProcessorValueTreeState& parameters() { return state; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState state;
    std::atomic<bool> paused { false };
    std::atomic<int> detectedRoot { -1 }, detectedMode { 2 };
    std::atomic<float> confidence { 0.0f }, tuningHz { 440.0f };
    double currentSampleRate = 44100.0;
    std::vector<float> analysisBuffer;
    int writePosition = 0;
    int samplesSinceAnalysis = 0;
    juce::dsp::FFT fft { 14 };
    std::vector<float> fftBuffer;
    std::array<float, 12> smoothedChroma {};
    int stableRoot = -1, stableMode = 2, pendingRoot = -1, pendingMode = 2, pendingCount = 0;
    class AnalysisWorker final : public juce::Thread {
    public:
        explicit AnalysisWorker(TonicRecreationAudioProcessor& owner)
            : juce::Thread("Tonic key analysis"), parent(owner) {}
        void run() override;
        TonicRecreationAudioProcessor& parent;
    } worker { *this };
    juce::CriticalSection snapshotLock;
    juce::WaitableEvent snapshotReady;
    std::array<float, 16384> snapshot {};
    std::atomic<bool> snapshotPending { false };
    std::array<std::atomic<float>, 12> liveChroma {};
    void analyseWindow();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonicRecreationAudioProcessor)
};
