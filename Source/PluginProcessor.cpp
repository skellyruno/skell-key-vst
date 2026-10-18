#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <numeric>

namespace {
constexpr int kWindow = 16384;
constexpr int kHop = 4096;
constexpr float kPi = 3.14159265358979323846f;
const char* noteNames[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
// Krumhansl-Schmuckler profiles; correlation gives a useful first-pass key guess,
// not a claim to reproduce zplane's proprietary detector.
constexpr float majorProfile[12] = {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
constexpr float minorProfile[12] = {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};
float corr(const std::array<float,12>& x, const float* p) {
    float mx=0, mp=0;
    for(int i=0;i<12;++i){mx+=x[i]; mp+=p[i];}
    mx/=12; mp/=12;
    float n=0,a=0,b=0;
    for(int i=0;i<12;++i){float u=x[i]-mx,v=p[i]-mp;n+=u*v;a+=u*u;b+=v*v;}
    return n/std::sqrt(std::max(1e-12f,a*b));
}
}

TonicRecreationAudioProcessor::TonicRecreationAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  state(*this, nullptr, "PARAMETERS", createParameterLayout()), analysisBuffer(kWindow, 0.0f), fftBuffer(kWindow * 2, 0.0f)
{}

juce::AudioProcessorValueTreeState::ParameterLayout TonicRecreationAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>("referenceHz","Reference tuning", 430.0f, 450.0f, 440.0f));
    p.add(std::make_unique<juce::AudioParameterBool>("preferFlats","Prefer flats", false));
    p.add(std::make_unique<juce::AudioParameterBool>("chords","Chord preview", false));
    return p;
}
TonicRecreationAudioProcessor::~TonicRecreationAudioProcessor() { releaseResources(); }
void TonicRecreationAudioProcessor::AnalysisWorker::run() {
    while (!threadShouldExit()) {
        parent.snapshotReady.wait(200);
        if (threadShouldExit()) break;
        if (parent.snapshotPending.exchange(false)) parent.analyseWindow();
    }
}
void TonicRecreationAudioProcessor::releaseResources() {
    worker.signalThreadShouldExit();
    snapshotReady.signal();
    worker.stopThread(3000);
}
void TonicRecreationAudioProcessor::prepareToPlay(double sr, int) {
    releaseResources();
    currentSampleRate=sr; std::fill(analysisBuffer.begin(),analysisBuffer.end(),0.0f);
    writePosition=0; samplesSinceAnalysis=0;
    smoothedChroma.fill(0.0f); stableRoot=-1; stableMode=2; pendingRoot=-1; pendingMode=2; pendingCount=0;
    detectedRoot.store(-1); detectedMode.store(2); confidence.store(0.0f);
    snapshotPending.store(false); worker.startThread();
}
bool TonicRecreationAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto in=l.getMainInputChannelSet(), out=l.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo()) && in==out;
}
void TonicRecreationAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels=buffer.getNumChannels(), n=buffer.getNumSamples();
    if(channels==0 || paused.load()) return;
    for(int i=0;i<n;++i) {
        float mono=0.0f;
        for(int c=0;c<channels;++c) mono+=buffer.getReadPointer(c)[i];
        mono/=static_cast<float>(channels);
        analysisBuffer[writePosition]=mono;
        writePosition=(writePosition+1)%kWindow;
        if(++samplesSinceAnalysis>=kHop) {
            samplesSinceAnalysis=0;
            if (!snapshotPending.load() && snapshotLock.tryEnter()) {
                for (int j=0;j<kWindow;++j)
                    snapshot[(size_t)j]=analysisBuffer[(writePosition+j)%kWindow];
                snapshotPending.store(true);
                snapshotLock.exit();
                snapshotReady.signal();
            }
        }
    }
    // Analysis-only effect: pass audio through unchanged.
}
void TonicRecreationAudioProcessor::analyseWindow() {
    // Called only on the worker thread; FFT and trigonometry never run in processBlock.
    double energy = 0.0;
    for (int i = 0; i < kWindow; ++i) {
        const float v = snapshot[(size_t)i];
        energy += double(v) * v;
        const float window = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (kWindow - 1));
        fftBuffer[i] = v * window;
    }
    if (energy / kWindow < 1.0e-7) {
        confidence.store(0.0f);
        return;
    }
    std::fill(fftBuffer.begin() + kWindow, fftBuffer.end(), 0.0f);
    fft.performRealOnlyForwardTransform(fftBuffer.data());
    const float ref = state.getRawParameterValue("referenceHz")->load();
    std::array<float, 12> chroma {};
    // Harmonic-weighted pitch-class spectrum. The FFT bins are interrogated only
    // near the first three harmonics of each MIDI note, with diminishing weights.
    for (int midi = 36; midi <= 96; ++midi) {
        const double f = ref * std::pow(2.0, (midi - 69) / 12.0);
        float evidence = 0.0f;
        for (int harmonic = 1; harmonic <= 3; ++harmonic) {
            const double hz = f * harmonic;
            if (hz >= currentSampleRate * 0.45) break;
            const int bin = juce::roundToInt(hz * kWindow / currentSampleRate);
            float peak = 0.0f;
            for (int j = juce::jmax(1, bin - 1); j <= juce::jmin(kWindow / 2 - 1, bin + 1); ++j) {
                const float re = fftBuffer[2 * j], im = fftBuffer[2 * j + 1];
                peak = juce::jmax(peak, std::sqrt(re * re + im * im));
            }
            evidence += peak / (float(harmonic) * float(harmonic));
        }
        chroma[midi % 12] += evidence;
    }
    const float total = std::accumulate(chroma.begin(), chroma.end(), 0.0f);
    if (total < 1.0e-7f) return;
    for (int i = 0; i < 12; ++i) {
        const float normalized = chroma[i] / total;
        smoothedChroma[i] = 0.82f * smoothedChroma[i] + 0.18f * normalized;
        liveChroma[(size_t)i].store(smoothedChroma[i]);
    }
    float best = -2.0f, second = -2.0f;
    int root = -1, mode = 2;
    for (int r = 0; r < 12; ++r) for (int m = 0; m < 2; ++m) {
        std::array<float, 12> rotated {};
        for (int i = 0; i < 12; ++i) rotated[i] = smoothedChroma[(i + r) % 12];
        const float score = corr(rotated, m == 0 ? majorProfile : minorProfile);
        if (score > best) { second = best; best = score; root = r; mode = m; }
        else if (score > second) second = score;
    }
    // Hysteresis prevents the display from switching on every analysis frame.
    if (root == pendingRoot && mode == pendingMode) ++pendingCount;
    else { pendingRoot = root; pendingMode = mode; pendingCount = 1; }
    if (pendingCount >= 3 && best > 0.25f) { stableRoot = root; stableMode = mode; }
    detectedRoot.store(stableRoot);
    detectedMode.store(stableMode);
    confidence.store(juce::jlimit(0.0f, 1.0f, (best - second) * 4.0f));
    tuningHz.store(ref);
}
juce::AudioProcessorEditor* TonicRecreationAudioProcessor::createEditor(){ return new TonicRecreationAudioProcessorEditor(*this); }
void TonicRecreationAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto xml=state.copyState().createXml(); copyXmlToBinary(*xml,dest);
}
void TonicRecreationAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size); xml && xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

// Required factory entry point for JUCE plugin wrappers.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TonicRecreationAudioProcessor(); }
