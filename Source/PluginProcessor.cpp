#include "FixedTemporalMedian.h"
#include "RollingFrequencyMedian.h"
#include "TempoEvidenceGate.h"
#include "PluginProcessor.h"
#include "KeySilenceGate.h"
#include "KeyCandidateScore.h"
#include "PluginEditor.h"
#include "StereoAnalysisMix.h"
#include "TempoOctaveGuard.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr int N=16384;
constexpr double pi=3.14159265358979323846;
// Average of the Krumhansl-Kessler and Temperley key profiles: less sensitive to the
// quirks of either corpus than a single profile.
constexpr float majorProfile[12]={5.675f,2.115f,3.490f,2.165f,4.440f,4.045f,2.260f,4.845f,2.195f,3.580f,1.895f,3.440f};
constexpr float minorProfile[12]={5.665f,2.340f,3.510f,4.940f,2.300f,3.765f,2.270f,4.625f,3.740f,2.345f,2.420f,3.585f};
float correlation(const std::array<float,12>& c, int root, const float* profile) {
    return skell::keyProfileCorrelation(c,root,profile);
}
}
TonicRecreationAudioProcessor::TonicRecreationAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  state(*this,nullptr,"PARAMETERS",createParameterLayout()),fftBuffer(N*2,0.0f) {
    for(int i=0;i<N;++i) hannWindow[(size_t)i]=0.5f-0.5f*std::cos(float(2*pi*i/(N-1)));
    resetAnalysis();
}
TonicRecreationAudioProcessor::~TonicRecreationAudioProcessor(){releaseResources();}
juce::AudioProcessorValueTreeState::ParameterLayout TonicRecreationAudioProcessor::createParameterLayout(){
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterBool>("preferFlats","Prefer flats",false));
    p.add(std::make_unique<juce::AudioParameterBool>("chords","Chord preview",true));
    p.add(std::make_unique<juce::AudioParameterFloat>("responsiveness","Detection responsiveness",0.03f,0.70f,0.18f));
    p.add(std::make_unique<juce::AudioParameterFloat>("sensitivity","Silence sensitivity",0.00001f,0.02f,0.0005f));
    return p;
}
void TonicRecreationAudioProcessor::resetAnalysis(){
    phraseTracker.reset(); beatGrid.reset(); downbeatChordTracker.reset(); relativeModeEvidence.reset(); keyConsistency.reset(); temporalKeyConsensus.reset(); learnedTonalFeatures.reset(); temporalTonalFeatures.reset(); analyzedSamples = 0; lastBassFrame = {};
    chordMemory.fill(0); progressionRootEvidence.fill(0);
    lastProgressionRoot=lastProgressionKind=-1; progressionEvents=0;
    heldChordRoot=heldChordKind=-1; chordHold=0; tuningCentsMemory=0; tuningHist.fill(0); tuningHistMass=0; tuningFrames=0; tuningApplied=false; tempoAmbiguity=0;
    harmonicWrite=harmonicCount=0; tonalPower.fill(0); harmonicMask.fill(0);
    chromaMemory.fill(0);keyChroma.fill(0);keyChromaFrames=0;startChroma.fill(0);endChroma.fill(0);history.fill(0);historyWrite=historyCount=samplesSinceWindow=silentFrames=0;
    stableRoot=pendingRoot=-1;stableMode=pendingMode=2;pendingCount=0;
    detectedRoot.store(-1);detectedMode.store(2);confidence.store(0);
    chordRoot.store(-1);chordKind.store(-1);chordConfidence.store(0);
    transcribedChordRoot.store(-1);transcribedChordQuality.store(-1);
    transcribedChordBass.store(-1);transcribedChordEvidence.store(0);
    detectedTuningHz.store(440.0f); tuningConfidence.store(0.0f);
    detectedBpm.store(0); bpmConfidence.store(0); lastStableBpm=0; loopFastBpm=0; weakTempoGate.reset(); tempoContinuity.reset();
    onsetHistory.fill(0); onsetWrite=onsetCount=onsetSamples=bpmAnalysisFrames=0;
    onsetEnergySum=onsetHighEnergySum=tempoLowpass=previousEnergy=previousHighEnergy=smoothedOnset=0;
    tempoSilentHops=0;
    for(auto& v:liveChroma)v.store(0);
    for(int i=0;i<3;++i){candidateRoots[(size_t)i].store(-1);candidateModes[(size_t)i].store(2);candidateScores[(size_t)i].store(0);}
}
void TonicRecreationAudioProcessor::prepareToPlay(double rate,int){
    releaseResources();currentSampleRate=rate;analysisSampleRate=rate;sampleFifo.reset();resetAnalysis();
    rebuildFrequencyMap(rate);
    offlineMode.store(false);liveInputDetected.store(false);fileBusy.store(false);
    offlineCancel.store(false);silentAudioSamples=0;
    worker.startThread();
}
// Runs only on the analysis worker, or before the worker is started.
// Offline files can have a different sample rate from the live DAW input.
void TonicRecreationAudioProcessor::rebuildFrequencyMap(double sampleRate){
    analysisSampleRate=std::max(1.0,sampleRate);
    temporalKeyConsensus.prepare(analysisSampleRate,hopSize);
    tempoHighpassDecay=std::exp(-2.0f * 3.14159265f * 150.0f
                                 / static_cast<float>(analysisSampleRate));
    tempoHopSamples=std::max(1,juce::roundToInt(analysisSampleRate/onsetHopRate));
    // Using the measured novelty-envelope rate avoids tempo drift when the
    // sample rate is not an exact multiple of 100 Hz.
    actualOnsetRate=static_cast<float>(analysisSampleRate/tempoHopSamples);
    for(int midi=36;midi<=95;++midi){
        const int index=midi-36;
        const float frequency=440.0f*std::pow(2.0f,(midi-69)/12.0f);
        noteFrequencies[(size_t)index]=frequency;
    }
}
void TonicRecreationAudioProcessor::releaseResources(){
    worker.signalThreadShouldExit();analysisReady.signal();worker.stopThread(5000);
}
bool TonicRecreationAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const{
    const auto a=l.getMainInputChannelSet(),b=l.getMainOutputChannelSet();
    return (a==juce::AudioChannelSet::mono()||a==juce::AudioChannelSet::stereo())&&a==b;
}
void TonicRecreationAudioProcessor::emitScaleMidi(juce::MidiBuffer& midi){
    static const int maj[7]={0,2,4,5,7,9,11}, mnr[7]={0,2,3,5,7,8,10};
    unsigned int want=0;
    const int root=detectedRoot.load(), mode=detectedMode.load();
    if(scaleOutputOn.load() && root>=0 && root<12){
        const int* iv=mode==1?mnr:maj;
        for(int i=0;i<7;++i) want|=1u<<((root+iv[i])%12);
    }
    for(int pc=0;pc<12;++pc){
        const unsigned int bit=1u<<pc;
        if((want&bit)&&!(scaleSentMask&bit)) midi.addEvent(juce::MidiMessage::noteOn(1,60+pc,(juce::uint8)100),0);
        else if(!(want&bit)&&(scaleSentMask&bit)) midi.addEvent(juce::MidiMessage::noteOff(1,60+pc),0);
    }
    scaleSentMask=want;
}
void TonicRecreationAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi){
    midi.clear();
    emitScaleMidi(midi);
    juce::ScopedNoDenormals nd;
    // Input level for the UI meter (before the preview tone is added).
    {
        float peak=0.0f;
        for(int ch=0;ch<buffer.getNumChannels();++ch) peak=std::max(peak,buffer.getMagnitude(ch,0,buffer.getNumSamples()));
        const float prev=inputLevel.load(std::memory_order_relaxed);
        inputLevel.store(peak>prev?peak:prev*std::pow(0.9995f,(float)buffer.getNumSamples()/1.0f*0.1f+1.0f),std::memory_order_relaxed);
    }
    analyseBlock(buffer);
    renderPreview(buffer); // after analysis has copied the input, so the tone is never analysed
}
void TonicRecreationAudioProcessor::analyseBlock(juce::AudioBuffer<float>& buffer){
    // Refresh host tempo on the audio thread; never retain a playhead pointer.
    if (auto* playhead = getPlayHead()) {
        if (auto position = playhead->getPosition()) {
            if (auto bpm = position->getBpm(); bpm && *bpm > 0.0 && std::isfinite(*bpm)) {
                hostBpm.store(static_cast<float>(*bpm),std::memory_order_relaxed);
                hostBpmValid.store(true,std::memory_order_relaxed);
            } else hostBpmValid.store(false,std::memory_order_relaxed);
        } else hostBpmValid.store(false,std::memory_order_relaxed);
    } else hostBpmValid.store(false,std::memory_order_relaxed);
    // No mutex, disk access, or heap allocation in the audio callback.
    // Live-input activity is informational only. Explicit file requests always
    // take priority, regardless of whether the DAW currently has audio.
    const int channels = buffer.getNumChannels(), count = buffer.getNumSamples();
    if(channels > 0 && count > 0) {
        float peak = 0.0f;
        for (int ch=0;ch<channels;++ch)
            peak = std::max(peak, buffer.getMagnitude(ch,0,count));
        if (peak > 0.0003f) {silentAudioSamples=0;liveInputDetected.store(true,std::memory_order_relaxed);}
        else { silentAudioSamples += count;
            if (silentAudioSamples > static_cast<int>(currentSampleRate * 0.5))
                liveInputDetected.store(false,std::memory_order_relaxed); }
    }
    if(offlineMode.load(std::memory_order_relaxed) || paused.load(std::memory_order_relaxed) || channels==0)return;
    int start1=0,size1=0,start2=0,size2=0;
    // Never waits for analysis and never allocates memory on the audio thread.
    sampleFifo.prepareToWrite(count,start1,size1,start2,size2);
    // Fetch channel pointers once per callback, not for every captured sample.
    // Valid channel layouts are mono and stereo (see isBusesLayoutSupported).
    const float* const left = buffer.getReadPointer(0);
    const float* const right = channels > 1 ? buffer.getReadPointer(1) : nullptr;
    // Classify the incoming stereo block once before feeding the FIFO. Normal
    // stereo still averages, but an almost-cancelled anti-phase pair no longer
    // erases pitched evidence. The plugin never writes to the host buffer.
    const auto stereoMix = skell::StereoAnalysisMix::decide(left,right,count);
    auto copy = [&](int destination, int offset, int length) {
        if (right != nullptr) {
            for (int i = 0; i < length; ++i)
                sampleQueue[(size_t)(destination + i)] =
                    stereoMix.sample(left[offset+i],right[offset+i]);
        } else {
            for (int i = 0; i < length; ++i)
                sampleQueue[(size_t)(destination + i)] = left[offset + i];
        }
    };
    copy(start1, 0, size1);
    copy(start2, size1, size2);
    sampleFifo.finishedWrite(size1+size2);
    if(size1+size2>0)analysisReady.signal();
    // Transparent analyzer; audio is left untouched.
}
bool TonicRecreationAudioProcessor::requestFileAnalysis(const juce::File& file) {
    // Explicit file selection overrides active DAW input without touching the audio callback.
    if (!file.existsAsFile()) return false;
    const auto extension = file.getFileExtension().toLowerCase();
    if (!juce::StringArray {".wav", ".aiff", ".aif", ".flac", ".ogg", ".mp3"}.contains(extension))
        return false;
    {
        const juce::ScopedLock lock(fileRequestLock);
        pendingFilePath=file.getFullPathName();
        displayedFileName=file.getFileName();
        pendingFileGeneration=fileRequestGeneration.fetch_add(1,std::memory_order_acq_rel)+1;
        fileRequestPending=true;
    }
    offlineCancel.store(true,std::memory_order_release); // Cancel an earlier file scan.
    offlineMode.store(true,std::memory_order_release);
    fileBusy.store(true);fileError.store(false);fileProgress.store(0);
    fileDurationSeconds.store(0); fileAnalyzedSeconds.store(0);
    paused.store(false);
    analysisReady.signal();
    return true;
}
void TonicRecreationAudioProcessor::returnToLive(){
    fileRequestGeneration.fetch_add(1,std::memory_order_acq_rel);
    offlineCancel.store(true,std::memory_order_release);
    offlineMode.store(false,std::memory_order_release);
    fileBusy.store(false);
    fileProgress.store(0); fileDurationSeconds.store(0); fileAnalyzedSeconds.store(0);
    {
        const juce::ScopedLock lock(fileRequestLock);
        fileRequestPending=false;
        displayedFileName.clear();
        pendingFilePath.clear();
    }
    requestReset();
}
juce::String TonicRecreationAudioProcessor::getFileName() const {
    const juce::ScopedLock lock(fileRequestLock);
    return displayedFileName;
}
void TonicRecreationAudioProcessor::discardLiveQueue(){
    int a=0,b=0,c=0,d=0;
    sampleFifo.prepareToRead(sampleFifo.getNumReady(),a,b,c,d);
    sampleFifo.finishedRead(b+d);
}
void TonicRecreationAudioProcessor::analyseAudioSample(float sample){
    ++analyzedSamples;
    consumeTempoSample(sample);
    history[(size_t)historyWrite]=sample;
    historyWrite=(historyWrite+1)&(windowSize-1);
    historyCount=std::min(windowSize,historyCount+1);
    if(historyCount==windowSize && ++samplesSinceWindow>=hopSize){
        samplesSinceWindow=0;analyseWindow();
    }
}
void TonicRecreationAudioProcessor::analyseFile(const juce::String& path, unsigned int generation){
    // Entire file I/O and DSP stays on the analysis thread, never the audio thread.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(path)));
    if (generation != fileRequestGeneration.load(std::memory_order_acquire)) return;
    if (!reader || reader->sampleRate <= 0.0 || reader->numChannels==0 || reader->lengthInSamples<=0) {
        if (generation == fileRequestGeneration.load()) { fileError.store(true); fileBusy.store(false); }
        return;
    }
    discardLiveQueue();
    resetAnalysis();
    rebuildFrequencyMap(reader->sampleRate);
    holdResultsOnSilence=true;
    constexpr int blockSize=4096;
    juce::AudioBuffer<float> decoded(2,blockSize);
    const auto total=std::min<juce::int64>(reader->lengthInSamples,
                       static_cast<juce::int64>(reader->sampleRate*12.0*60.0));
    fileDurationSeconds.store(static_cast<float>(total / reader->sampleRate));
    for(juce::int64 offset=0;offset<total && !worker.threadShouldExit();offset+=blockSize){
        if(offlineCancel.load(std::memory_order_acquire) || !offlineMode.load() ||
           generation != fileRequestGeneration.load(std::memory_order_acquire))break;
        const int n=static_cast<int>(std::min<juce::int64>(blockSize,total-offset));
        decoded.clear();
        if(!reader->read(&decoded,0,n,offset,true,true)) {
            // Only the active request may publish a read failure.
            if (generation == fileRequestGeneration.load(std::memory_order_acquire))
                fileError.store(true, std::memory_order_release);
            break;
        }
        const float* const left = decoded.getReadPointer(0);
        const float* const right = reader->numChannels > 1 ? decoded.getReadPointer(1) : nullptr;
        const auto stereoMix = skell::StereoAnalysisMix::decide(left,right,n);
        for(int i=0;i<n;++i){
            // Check regularly inside decoded blocks: a new drop or Live Input
            // must preempt the currently running analysis, not wait for 4096 samples.
            if ((i & 255) == 0 && (worker.threadShouldExit() ||
                offlineCancel.load(std::memory_order_acquire) ||
                !offlineMode.load(std::memory_order_acquire) ||
                generation != fileRequestGeneration.load(std::memory_order_acquire)))
                break;
            analyseAudioSample(right != nullptr ? stereoMix.sample(left[i],right[i]) : left[i]);
        }
        if (worker.threadShouldExit() || offlineCancel.load(std::memory_order_acquire) ||
            !offlineMode.load(std::memory_order_acquire) ||
            generation != fileRequestGeneration.load(std::memory_order_acquire))
            break;
        if ((offset/blockSize)%32==0 || offset+n>=total) {
            fileProgress.store(total>0?static_cast<float>(double(offset+n)/double(total)):1.0f);
            fileAnalyzedSeconds.store(static_cast<float>((offset+n)/reader->sampleRate));
        }
        if((offset/blockSize)%128==0)juce::Thread::yield();
    }
    holdResultsOnSilence=false;
    if (generation == fileRequestGeneration.load(std::memory_order_acquire)) {
        if (!offlineCancel.load() && offlineMode.load() && !fileError.load()) {
            fileProgress.store(1.0f);
            fileAnalyzedSeconds.store(fileDurationSeconds.load());
        }
        fileBusy.store(false);
    }
}
void TonicRecreationAudioProcessor::AnalysisWorker::run(){
    std::array<float,4096> batch {};
    while(!threadShouldExit()){
        parent.analysisReady.wait(100);
        if(threadShouldExit())break;
        juce::String nextFile;
        unsigned int nextGeneration=0;
        {
            const juce::ScopedLock lock(parent.fileRequestLock);
            if(parent.fileRequestPending){
                nextFile=parent.pendingFilePath;
                nextGeneration=parent.pendingFileGeneration;
                parent.fileRequestPending=false;
            }
        }
        if(nextFile.isNotEmpty()){
            if (nextGeneration != parent.fileRequestGeneration.load(std::memory_order_acquire)) continue;
            parent.offlineCancel.store(false,std::memory_order_release);
            parent.analyseFile(nextFile, nextGeneration);
            continue;
        }
        if(parent.offlineMode.load(std::memory_order_acquire)){
            parent.discardLiveQueue();
            continue;
        }
        if(parent.resetRequested.exchange(false,std::memory_order_acq_rel)){
            parent.discardLiveQueue();parent.resetAnalysis();
            parent.rebuildFrequencyMap(parent.currentSampleRate);
        }
        while(!threadShouldExit() && !parent.offlineMode.load()){
            int start1=0,n1=0,start2=0,n2=0;
            parent.sampleFifo.prepareToRead((int)batch.size(),start1,n1,start2,n2);
            const int count=n1+n2;
            if(count==0)break;
            for(int i=0;i<n1;++i)batch[(size_t)i]=parent.sampleQueue[(size_t)(start1+i)];
            for(int i=0;i<n2;++i)batch[(size_t)(n1+i)]=parent.sampleQueue[(size_t)(start2+i)];
            parent.sampleFifo.finishedRead(count);
            for(int i=0;i<count;++i) {
                // Prioritize explicit file requests over an already-copied live batch.
                if ((i & 255) == 0 && (threadShouldExit() ||
                    parent.offlineMode.load(std::memory_order_acquire)))
                    break;
                parent.analyseAudioSample(batch[(size_t)i]);
            }
        }
    }
}
// Tempo detection is deliberately independent of the FFT key-analysis window.
// Compute a 100 Hz log-energy novelty envelope, then normalized onset autocorrelation.
// This estimates a repeating beat interval, not a guaranteed downbeat or meter.
void TonicRecreationAudioProcessor::consumeTempoSample(float sample) {
    onsetEnergySum += sample * sample;
    // Second, transient-sensitive energy band: one-pole ~150 Hz high-pass.
    // The tempo detector runs off the audio thread; these states are worker-owned.
    tempoLowpass = tempoHighpassDecay * tempoLowpass
                 + (1.0f-tempoHighpassDecay) * sample;
    const float high = sample - tempoLowpass;
    onsetHighEnergySum += high * high;
    ++onsetSamples;
    if (onsetSamples < tempoHopSamples) return;
    // Do not keep presenting an old BPM after input has stopped. Use the
    // original RMS envelope, rather than novelty, to distinguish silence
    // from a sustained tone that happens to have no transients.
    const float meanSquare = onsetEnergySum / onsetSamples;
    if (meanSquare < 1.0e-8f) {
        ++tempoSilentHops;
        if (tempoSilentHops == juce::roundToInt(actualOnsetRate * 2.0f) && !holdResultsOnSilence) {
            detectedBpm.store(0.0f);
            bpmConfidence.store(0.0f);
            tempoAmbiguity.store(0.0f);
            lastStableBpm = 0.0f; loopFastBpm = 0.0f;
            onsetHistory.fill(0.0f); weakTempoGate.reset(); tempoContinuity.reset();
            onsetWrite = onsetCount = bpmAnalysisFrames = 0;
            previousEnergy = previousHighEnergy = smoothedOnset = tempoLowpass = 0.0f;
        }
        // Do not refill the history with silence or continually re-estimate it.
        if (tempoSilentHops >= juce::roundToInt(actualOnsetRate * 2.0f)) {
            onsetSamples = 0;
            onsetEnergySum = onsetHighEnergySum = 0.0f;
            return;
        }
    } else {
        tempoSilentHops = 0;
    }
    const float energy = std::log1p(1000.0f * meanSquare);
    const float highEnergy = std::log1p(1000.0f * onsetHighEnergySum / onsetSamples);
    const float broadbandRise = std::max(0.0f, energy - previousEnergy);
    const float highRise = std::max(0.0f, highEnergy - previousHighEnergy);
    // Blend onset evidence; make bright snare/hat attacks available without
    // allowing them to completely override a strong low-frequency pulse.
    const float rise = 0.55f * broadbandRise + 0.45f * highRise;
    previousEnergy = 0.7f * previousEnergy + 0.3f * energy;
    previousHighEnergy = 0.7f * previousHighEnergy + 0.3f * highEnergy;
    smoothedOnset = 0.65f * smoothedOnset + 0.35f * rise;
    onsetHistory[static_cast<size_t>(onsetWrite)] = smoothedOnset;
    onsetWrite = (onsetWrite + 1) % onsetHistoryLength;
    onsetCount = std::min(onsetCount + 1, onsetHistoryLength);
    onsetSamples = 0; onsetEnergySum = onsetHighEnergySum = 0.0f;
    // An update every 1.5 elapsed seconds, independent of the source sample rate.
    if (++bpmAnalysisFrames >= std::max(1, juce::roundToInt(1.5f * actualOnsetRate))) {
        bpmAnalysisFrames = 0;
        estimateTempo();
    }
}

void TonicRecreationAudioProcessor::estimateTempo() {
    // Require 6 seconds of actual source time, not an assumed 100 Hz rate.
    if (onsetCount < juce::roundToInt(6.0f * actualOnsetRate)) return;
    constexpr int length=3600;
    const int count=std::min(onsetCount,length);
    std::array<float,length> envelope {};
    float mean=0.0f;
    for(int i=0;i<count;++i){
        envelope[(size_t)i]=onsetHistory[(size_t)((onsetWrite-count+i+onsetHistoryLength)%onsetHistoryLength)];
        mean+=envelope[(size_t)i];
    }
    mean/=count;
    float energy=0.0f;
    for(int i=0;i<count;++i){
        auto& v=envelope[(size_t)i];v=std::max(0.0f,v-mean*0.65f);energy+=v*v;
    }
    // A weak or unstructured rhythmic signal is not evidence of a stable BPM.
    if(energy<0.001f){detectedBpm.store(0);bpmConfidence.store(0);lastStableBpm=0;tempoAmbiguity=0;weakTempoGate.reset();tempoContinuity.reset();return;}
    // One normalized autocorrelation pass for the entire 40-240 BPM search space.
    // A beat hypothesis receives evidence from its half/double time neighbors;
    // this reduces common octave errors but cannot resolve fundamentally ambiguous rhythms.
    // Prefix energy sums avoid recomputing both autocorrelation denominators
    // for every lag. Only the cross term needs an inner product.
    std::array<float,length+1> energyPrefix {};
    for(int i=0;i<count;++i)
        energyPrefix[(size_t)i+1]=energyPrefix[(size_t)i]+envelope[(size_t)i]*envelope[(size_t)i];
    std::array<float,361> corr {};
    // Autocorrelation lag is computed from the *actual* envelope sample rate.
    const int minLag=juce::jlimit(1, 359, juce::roundToInt(60.0f*actualOnsetRate/240.0f));
    const int maxLag=juce::jlimit(minLag, 359, juce::roundToInt(60.0f*actualOnsetRate/40.0f));
    for(int lag=minLag;lag<=maxLag;++lag){
        float cross=0;
        for(int i=lag;i<count;++i)
            cross+=envelope[(size_t)i]*envelope[(size_t)(i-lag)];
        const float e1=energyPrefix[(size_t)count]-energyPrefix[(size_t)lag];
        const float e2=energyPrefix[(size_t)(count-lag)];
        if(e1>1e-8f && e2>1e-8f) corr[(size_t)lag]=cross/std::sqrt(e1*e2);
    }
    // 3:2 subdivisions are common in trap/hip-hop arrangements. They can
    // produce a plausible but incorrect ~94 BPM for a 141 BPM track.
    // Build octave-family evidence at the inferred beat lag; avoid treating
    // the 3:2 alias as a valid half/double-time alternative.
    auto localCorrelation = [&](int target)->float {
        if(target<minLag || target>maxLag) return 0.0f;
        float result=0.0f;
        for(int offset=-1;offset<=1;++offset) {
            const int index=target+offset;
            if(index>=minLag && index<=maxLag)
                result=std::max(result,corr[(size_t)index]);
        }
        return result;
    };
    float best=-1.0f, second=-1.0f;int bestLag=0;
    const float host=hostBpmValid.load()?hostBpm.load():0.0f;
    for(int lag=minLag;lag<=maxLag;++lag){
        const float bpm=60.0f*actualOnsetRate/lag;
        float score=corr[(size_t)lag];
        if(lag*2<=maxLag)score+=0.30f*corr[(size_t)(lag*2)];
        if(lag/2>=minLag) score+=0.12f*localCorrelation(juce::roundToInt(lag*0.5f));
        // A strong 3:2 relationship is often a subdivision, not the beat.
        // Give the faster interpretation a limited bonus only if its own
        // periodicity is supported. This is intentionally a *small* prior.
        if(lag*3/2<=maxLag && lag%2==0)
            score-=0.07f*localCorrelation(lag*3/2);
        if(lag*2/3>=minLag)
            score+=0.08f*localCorrelation(juce::roundToInt(lag*2.0f/3.0f));
        // Perceptual tempo prior (log-normal around 120 BPM, after Moelants & Parncutt):
        // listeners overwhelmingly tap at 90-150 BPM, and plain autocorrelation of an
        // accented bar (kick / snare alternation) otherwise prefers the 2-beat lag,
        // i.e. exactly half the felt tempo.
        score*=0.60f+0.40f*std::exp(-0.5f*std::pow(std::log2(bpm/120.0f)/0.75f,2));
        if(lastStableBpm>0.0f && std::abs(std::log2(bpm/lastStableBpm))<0.10f)score*=1.06f;
        if(host>0.0f && std::abs(std::log2(bpm/host))<0.04f)score*=1.04f;
        if(score>best){second=best;best=score;bestLag=lag;}
        else if(score>second && std::abs(lag-bestLag)>2)second=score;
    }

    // Fractional beat-grid validation: a 3:2 subdivision can dominate lag
    // autocorrelation even when the true pulse aligns better across 12 seconds.
    // Use a small phase-folded novelty histogram to verify a faster 3:2 pulse.
    // The decision is evidence-driven, not a fixed BPM preference.
    auto phaseStrength = [&](float period)->float {
        constexpr int bins=48;
        std::array<float,bins> summed{};
        std::array<int,bins> hit{};
        float total=0.0f;
        for(int i=0;i<count;++i) {
            const float phase=std::fmod(static_cast<float>(i),period)/period;
            const int bin=juce::jlimit(0,bins-1,static_cast<int>(phase*bins));
            summed[(size_t)bin]+=envelope[(size_t)i];
            ++hit[(size_t)bin];total+=envelope[(size_t)i];
        }
        float peak=0.0f;
        for(int phase=0;phase<bins;++phase) {
            float pulse=0.0f;
            for(int j=0;j<5;++j) {
                const int k=(phase+j)%bins;
                pulse+=summed[(size_t)k]/std::max(1,hit[(size_t)k]);
            }
            peak=std::max(peak,pulse/5.0f);
        }
        const float mean=total/count;
        return std::max(0.0f,(peak-mean)/(mean+1.0e-6f));
    };
    auto bestPhasePeriod = [&](float center)->std::pair<float,float> {
        float peak=-1.0f,period=center;
        for(int step=-8;step<=8;++step) {
            const float trial=center+step*0.125f;
            if(trial<minLag || trial>maxLag)continue;
            const float evidence=phaseStrength(trial);
            if(evidence>peak){peak=evidence;period=trial;}
        }
        return {peak,period};
    };
    float selectedPeriod=static_cast<float>(bestLag);
    float phaseWinConfidence=0.0f; // set when the faster 3:2 grid is confirmed by the loop length
    if(bestLag>0 && onsetCount>=juce::roundToInt(9.0f*actualOnsetRate)) {
        const float fastPeriod=bestLag*(2.0f/3.0f);
        if(fastPeriod>=minLag+1 && fastPeriod<=maxLag-1) {
            const auto slower=bestPhasePeriod(static_cast<float>(bestLag));
            const auto faster=bestPhasePeriod(fastPeriod);
            const float fastCorr=localCorrelation(juce::roundToInt(fastPeriod));
            if ((fastCorr > 0.22f && faster.first > slower.first*1.20f
                 && faster.first > slower.first + 0.23f)
                || (corr[(size_t)bestLag] < 0.18f
                    && skell::TempoEvidenceGate::preferThreeTwoFast(
                        localCorrelation(bestLag), fastCorr, slower.first, faster.first))) {
                selectedPeriod=faster.second;
                bestLag=juce::jlimit(minLag,maxLag,juce::roundToInt(selectedPeriod));
            }
        }
    }

    // Half-time alias (2:1). A trap/hip-hop beat at 156 BPM places kick and snare two beats
    // apart, so the 78 BPM lag autocorrelates best. If the exactly double tempo is itself a
    // clear periodicity and its phase-folded pulse is at least as sharp, the fast grid is the beat.
    bool halfTimePromoted=false;
    if (bestLag>0 && selectedPeriod==static_cast<float>(bestLag)) {
        const float bpmNow=60.0f*actualOnsetRate/bestLag;
        const int halfLag=juce::roundToInt(bestLag*0.5f);
        if (bpmNow<96.0f && halfLag>=minLag && bpmNow*2.0f<=185.0f) {
            const float c1=corr[(size_t)bestLag], c2=localCorrelation(halfLag);
            if (c2>=0.60f*c1 && c2>0.15f) {
                const auto slow=bestPhasePeriod(static_cast<float>(bestLag));
                const auto fast=bestPhasePeriod(bestLag*0.5f);
                if (fast.first>=slow.first*1.35f) {
                    selectedPeriod=fast.second;
                    bestLag=juce::jlimit(minLag,maxLag,juce::roundToInt(selectedPeriod));
                    halfTimePromoted=true; phaseWinConfidence=std::max(phaseWinConfidence,0.25f);
                }
            }
        }
    }

    // Loop-length consistency (3:2 alias). Dotted-rhythm patterns (kicks every 1.5 beats)
    // make a track at 150 BPM autocorrelate most strongly at 100 BPM. The song's
    // dominant long-range repetition (a 2-4 bar loop) must, however, be a whole number
    // of beats of the TRUE tempo. If that loop is a whole number of beats at 2/3 of the
    // current period but not at the current one, the faster grid is the real beat.
    if (bestLag>0 && selectedPeriod==static_cast<float>(bestLag) && count>=juce::roundToInt(14.0f*actualOnsetRate)) {
        const int minLoop=juce::roundToInt(1.5f*actualOnsetRate);
        const int maxLoop=std::min(juce::roundToInt(14.0f*actualOnsetRate), count*45/100);
        float loopCorr=0.0f; int loopLag=0;
        auto acfAt=[&](int lag)->float{
            float cross=0.0f;
            for(int i=lag;i<count;++i) cross+=envelope[(size_t)i]*envelope[(size_t)(i-lag)];
            const float e1=energyPrefix[(size_t)count]-energyPrefix[(size_t)lag], e2=energyPrefix[(size_t)(count-lag)];
            return (e1>1.0e-8f&&e2>1.0e-8f)?cross/std::sqrt(e1*e2):0.0f;
        };
        std::array<float,3600> longAcf {};
        for(int lag=minLoop;lag<=maxLoop && lag<count-100;++lag) longAcf[(size_t)lag]=acfAt(lag);
        for(int lag=minLoop+1;lag<maxLoop-1 && lag<count-100;++lag) {
            const float v=longAcf[(size_t)lag];
            if(v>loopCorr && v>=longAcf[(size_t)lag-1] && v>=longAcf[(size_t)lag+1]){loopCorr=v;loopLag=lag;}
        }
        // Only a strong, machine-exact loop is trusted: a performed piece drifts slightly,
        // so its phrase lengths are never an exact whole number of beats at any tempo.
        if(loopLag>0 && loopCorr>=0.75f) {
            const float fastGuess=bestLag*(2.0f/3.0f);
            if(fastGuess>=minLag+1) {
                const float fastPeriod=bestPhasePeriod(fastGuess).second;
                auto misfit=[&](float period)->float{       // relative error of the best whole-beat fit
                    const float beats=std::max(1.0f,std::round(loopLag/period));
                    return std::abs(loopLag/(beats*period)-1.0f);
                };
                if(misfit(fastPeriod)<0.008f && misfit(static_cast<float>(bestLag))>0.025f
                   && std::round(loopLag/fastPeriod)>=4.0f) {
                    selectedPeriod=fastPeriod;
                    bestLag=juce::jlimit(minLag,maxLag,juce::roundToInt(selectedPeriod));
                    phaseWinConfidence=juce::jlimit(0.0f,0.6f,0.30f+0.6f*(loopCorr-0.75f));
                    loopFastBpm=60.0f*actualOnsetRate/selectedPeriod;
                }
            }
        }
    }

    // Fine refinement: the 100 Hz lag grid is coarse at fast tempi (1 lag = 1.5 BPM at 156), so
    // pick the period within +-1.6 % that makes the onsets of the whole window line up best.
    if (selectedPeriod>0.0f && onsetCount>=juce::roundToInt(9.0f*actualOnsetRate)) {
        float bestEv=-1.0f, bestP=selectedPeriod;
        for(int k=-16;k<=16;++k){
            const float trial=selectedPeriod*(1.0f+0.001f*(float)k);
            if(trial<minLag || trial>maxLag) continue;
            const float ev=phaseStrength(trial)*(1.0f-0.002f*std::abs((float)k)); // prefer the unrefined value on ties
            if(ev>bestEv){bestEv=ev;bestP=trial;}
        }
        selectedPeriod=bestP;
    }

    // A loop-confirmed faster tempo is kept: a single frame that fails to re-confirm it
    // (e.g. a sparse passage) must not drop the answer back to the 3:2 alias.
    if (loopFastBpm>0.0f && selectedPeriod>0.0f) {
        const float nowBpm=60.0f*actualOnsetRate/selectedPeriod;
        if (std::abs(nowBpm*1.5f/loopFastBpm-1.0f)<0.04f) {          // currently proposing the slow alias
            selectedPeriod=60.0f*actualOnsetRate/loopFastBpm;
            bestLag=juce::jlimit(minLag,maxLag,juce::roundToInt(selectedPeriod));
            phaseWinConfidence=std::max(phaseWinConfidence,0.30f);
        } else if (std::abs(nowBpm/loopFastBpm-1.0f)<0.03f) {
            phaseWinConfidence=std::max(phaseWinConfidence,0.30f);
        } else if (std::abs(std::log2(nowBpm/loopFastBpm))>0.4f) {
            loopFastBpm=0.0f;                                           // genuinely different tempo now
        }
    }

    // Tempo-level hysteresis for a 3:2 alias: once a faster pulse has
    // independent grid support, do not flicker back to a subdivision simply
    // because the lag correlation momentarily becomes strongest.
    if (selectedPeriod>0.0f && lastStableBpm>0.0f) {
        const float currentBpm=60.0f*actualOnsetRate/selectedPeriod;
        const float ratio=lastStableBpm/currentBpm;
        if(ratio>1.46f && ratio<1.54f) {
            const float previousPeriod=60.0f*actualOnsetRate/lastStableBpm;
            const float oldEvidence=phaseStrength(previousPeriod);
            const float newEvidence=phaseStrength(selectedPeriod);
            if(oldEvidence>newEvidence*0.65f) {
                selectedPeriod=previousPeriod;
                bestLag=juce::jlimit(minLag,maxLag,juce::roundToInt(previousPeriod));
            }
        }
    }
    // Protect a previously supported double-time pulse from a late half-time
    // autocorrelation peak only when *both* beat-grid and correlation evidence
    // still support the fast interpretation. This does not assume any BPM.
    if (selectedPeriod > 0.0f && lastStableBpm > 0.0f) {
        const float proposedBpm = 60.0f * actualOnsetRate / selectedPeriod;
        const float oldPeriod = 60.0f * actualOnsetRate / lastStableBpm;
        if (lastStableBpm > proposedBpm * 1.90f &&
            lastStableBpm < proposedBpm * 2.10f &&
            skell::holdSupportedDoubleTime(
                lastStableBpm, proposedBpm,
                localCorrelation(juce::roundToInt(oldPeriod)),
                localCorrelation(juce::roundToInt(selectedPeriod)),
                phaseStrength(oldPeriod), phaseStrength(selectedPeriod))) {
            selectedPeriod = oldPeriod;
            bestLag = juce::jlimit(minLag, maxLag, juce::roundToInt(oldPeriod));
        }
    }
    // Weak pulse fallback: require repeated agreement between independently
    // measured autocorrelation and phase-folded onset timing.
    const float chosenCorr = bestLag>0 ? corr[(size_t)bestLag] : 0.0f;
    const float chosenBpm = selectedPeriod>0.0f ? 60.0f*actualOnsetRate/selectedPeriod : 0.0f;
    const bool strongPulse = chosenCorr >= 0.18f || phaseWinConfidence > 0.0f;
    const float chosenPhase = (!strongPulse && bestLag>0) ? phaseStrength(selectedPeriod) : 0.0f;
    const bool recurringWeakPulse = !strongPulse && bestLag>0 && weakTempoGate.observeWeak(
        chosenBpm, chosenCorr, chosenPhase, count/actualOnsetRate);
    if (bestLag==0 || (!strongPulse && !recurringWeakPulse)) {
        const auto held=tempoContinuity.recover(analyzedSamples/analysisSampleRate);
        if (held.held) {
            detectedBpm.store(held.bpm); bpmConfidence.store(held.confidence);
        } else {
            detectedBpm.store(0); bpmConfidence.store(0);
        }
        return;
    }
    if (strongPulse) weakTempoGate.reset();
    const float bpm=chosenBpm;
    const int halfLag=bestLag*2, doubleLag=bestLag/2;
    const float half=halfLag<=maxLag?corr[(size_t)halfLag]:0.0f;
    const float dbl=doubleLag>=minLag?corr[(size_t)doubleLag]:0.0f;
    tempoAmbiguity=juce::jlimit(0.0f,1.0f,std::max(half,dbl)/std::max(0.01f,corr[(size_t)bestLag]));
    const float originalConf=juce::jlimit(0.0f,1.0f,(chosenCorr-0.15f)/0.60f)
        *(1.0f-0.30f*tempoAmbiguity);
    const float conf0=strongPulse ? originalConf
        : skell::TempoEvidenceGate::weakConfidence(chosenCorr, chosenPhase)
          *(1.0f-0.30f*tempoAmbiguity);
    const float conf=std::max(conf0,phaseWinConfidence*(1.0f-0.30f*tempoAmbiguity));
    // Ignore a single sudden octave jump when pulse evidence is ambiguous.
    const bool octaveJump = lastStableBpm > 0.0f &&
        (std::abs(bpm / lastStableBpm - 2.0f) < 0.10f ||
         std::abs(bpm / lastStableBpm - 0.5f) < 0.05f);
    if(octaveJump && !halfTimePromoted && tempoAmbiguity.load() > 0.72f) {
        bpmConfidence.store(conf * 0.5f);
        detectedBpm.store(lastStableBpm);
        return;
    }
    if(lastStableBpm==0.0f || std::abs(bpm-lastStableBpm)>8.0f)lastStableBpm=bpm;
    else lastStableBpm=0.75f*lastStableBpm+0.25f*bpm;
    detectedBpm.store(lastStableBpm);bpmConfidence.store(conf);
    tempoContinuity.observe(lastStableBpm,conf,analyzedSamples/analysisSampleRate);
    // V38: estimate a beat phase from actual transients rather than treating
    // the beginning of a file as bar one. All processing is worker-only.
    if (conf > 0.08f) {
        const auto phase = skell::estimateBeatPhase(onsetHistory, onsetWrite, onsetCount,
                                                    actualOnsetRate, lastStableBpm);
        if (phase.confidence >= 0.09f) {
            beatGrid.configure(lastStableBpm,
                static_cast<float>(analyzedSamples / analysisSampleRate) - phase.ageSeconds,
                phase.confidence);
            const auto accent = skell::estimateDownbeatAccent(onsetHistory,onsetWrite,onsetCount,
                                                              actualOnsetRate,lastStableBpm,phase.ageSeconds);
            downbeatChordTracker.configure(lastStableBpm,
                analyzedSamples / analysisSampleRate - phase.ageSeconds,
                phase.confidence,accent);
        }
    }

}


void TonicRecreationAudioProcessor::updateHarmonicMask() {
    // Worker-only, no heap allocations: a causal 7-frame median distinguishes
    // sustained harmonic bins from short broadband percussion transients.
    constexpr int bins = windowSize/2;
    auto& current=spectralHistory[(size_t)harmonicWrite];
    for(int k=0;k<bins;++k)
        current[(size_t)k]=std::sqrt(std::max(0.0f,spectrumPower[(size_t)k]));
    harmonicWrite=(harmonicWrite+1)%harmonicFrames;
    harmonicCount=std::min(harmonicFrames,harmonicCount+1);
    harmonicMask.fill(1.0f);
    tonalPower.fill(0.0f);
    // Never process FFT bins above the highest note harmonic used by chroma.
    const int lastBin=std::min(bins-7,juce::roundToInt(6200.0f*windowSize/analysisSampleRate)+8);
    skell::RollingFrequencyMedian13 frequencyMedian;
    if (lastBin > 6) frequencyMedian.reset(current.data());
    for(int k=6;k<lastBin;++k){
        if (k > 6) frequencyMedian.advance(current[(size_t)(k-7)], current[(size_t)(k+6)]);
        std::array<float,harmonicFrames> time{};
        for(int t=0;t<harmonicCount;++t)time[(size_t)t]=spectralHistory[(size_t)t][(size_t)k];
        // V52: exact fixed-size upper median; avoids per-bin general selection.
        const float harmonic=skell::temporalMedian7(time.data(), harmonicCount);
        const float percussive=frequencyMedian.median();
        // Soft mask with a floor; no binary spectral holes or hard gating.
        const float h=harmonic*harmonic;
        const float p=1.5f*percussive;
        harmonicMask[(size_t)k]=juce::jlimit(0.05f,1.0f,h/(h+p*p+1.0e-12f));
        // Subtract the local broadband floor; harmonically concentrated bins
        // remain while drum/noise-dominated bins contribute less to chroma.
        const float tonal = std::max(0.0f,current[(size_t)k] - 1.25f*percussive);
        tonalPower[(size_t)k]=tonal*tonal*harmonicMask[(size_t)k];
    }

}

void TonicRecreationAudioProcessor::analyseWindow(){
    double energy=0;
    for(int i=0;i<N;++i){
        const float v=history[(size_t)((historyWrite+i)&(N-1))];
        energy+=double(v)*v;
        fftBuffer[(size_t)i]=v*hannWindow[(size_t)i];
    }
    const float rms=float(std::sqrt(energy/N));
    const float gate=state.getRawParameterValue("sensitivity")->load();
    if(rms<gate){
        // File analysis: a silent intro/outro must not erase the result already found.
        if(holdResultsOnSilence) return;
        const int resetFrames=skell::keySilenceResetFrames(analysisSampleRate,hopSize);
        // No fresh tonal evidence during the rest. Hide readouts while retaining
        // internal context for quick relock if the SAME song resumes shortly.
        if(++silentFrames==1){
            detectedRoot.store(-1); detectedMode.store(2);
            chordRoot.store(-1); chordKind.store(-1); chordConfidence.store(0);
            for(auto& v:candidateRoots)v.store(-1);
            transcribedChordRoot.store(-1); transcribedChordQuality.store(-1);
            transcribedChordBass.store(-1); transcribedChordEvidence.store(0);
        }
        if(silentFrames==resetFrames){
            phraseTracker.reset(); beatGrid.reset(); downbeatChordTracker.reset(); relativeModeEvidence.reset(); keyConsistency.reset(); temporalKeyConsensus.reset(); learnedTonalFeatures.reset(); temporalTonalFeatures.reset(); lastBassFrame = {};
            // Prevent the previous song's seven-frame harmonic median leaking into the next.
            harmonicCount=harmonicWrite=0;
            chromaMemory.fill(0);keyChroma.fill(0);keyChromaFrames=0;chordMemory.fill(0);
            progressionRootEvidence.fill(0); progressionEvents=0;
            lastProgressionRoot=lastProgressionKind=-1;
            stableRoot=pendingRoot=-1;stableMode=pendingMode=2;pendingCount=0;
            heldChordRoot=heldChordKind=-1;chordHold=0;
            tuningCentsMemory=0.0f;tuningHist.fill(0.0f);tuningHistMass=0.0f;tuningFrames=0;tuningApplied=false;detectedTuningHz.store(440.0f);tuningConfidence.store(0.0f);
            detectedRoot.store(-1);detectedMode.store(2);chordRoot.store(-1);chordKind.store(-1);chordConfidence.store(0);
            transcribedChordRoot.store(-1);transcribedChordQuality.store(-1);
            transcribedChordBass.store(-1);transcribedChordEvidence.store(0);
            for(auto& v:liveChroma)v.store(0);
            for(int i=0;i<3;++i)candidateRoots[(size_t)i].store(-1);
        }
        silentFrames=std::min(silentFrames,resetFrames);
        confidence.store(0);return;
    }
    silentFrames=0;
    std::fill(fftBuffer.begin()+N,fftBuffer.end(),0.0f);
    fft.performRealOnlyForwardTransform(fftBuffer.data());
    constexpr float ref=440.0f;
    for (int k=0;k<N/2;++k){
        const float re=fftBuffer[(size_t)(2*k)],im=fftBuffer[(size_t)(2*k+1)];
        spectrumPower[(size_t)k]=re*re+im*im;
    }
    updateHarmonicMask();
    lastBassFrame = skell::BassFundamentalEstimator::analyze(tonalPower.data(), N/2, analysisSampleRate);
    std::array<float,12> c{}, midChroma{}, bassChroma{};
    // ---- Spectral peak picking (V55) ----------------------------------------
    // Each spectral peak is located with sub-bin interpolation and credited only
    // to the note(s) it actually belongs to. The earlier "max within +-2 bins of
    // the nominal 440 Hz bin" read the same main lobe for several neighbouring
    // semitones in the bass, and missed off-grid peaks entirely in the treble.
    struct SpectralPeak { float hz, amp; };
    constexpr int maxPeaks = 320;
    std::array<SpectralPeak,maxPeaks> peaks;
    int peakCount = 0;
    {
        const int lowBin  = std::max(3,(int)std::ceil(50.0*N/analysisSampleRate));
        const int highBin = std::min(N/2-3,(int)std::floor(std::min(analysisSampleRate*0.45,6400.0)*N/analysisSampleRate));
        float maxTonal = 0.0f;
        for (int k=lowBin;k<=highBin;++k) maxTonal=std::max(maxTonal,tonalPower[(size_t)k]);
        const float floorPower = maxTonal*1.0e-4f; // -40 dB below the strongest tonal peak
        if (maxTonal>1.0e-12f) {
            for (int k=lowBin;k<=highBin;++k) {
                const float p=tonalPower[(size_t)k];
                if (p<=floorPower || p<tonalPower[(size_t)(k-1)] || p<tonalPower[(size_t)(k+1)]) continue;
                if (p==tonalPower[(size_t)(k-1)]) continue; // flat top: count it once
                // Frequency from the raw spectrum (the soft mask may zero a neighbour).
                const float l=0.5f*std::log(spectrumPower[(size_t)(k-1)]+1.0e-18f);
                const float m=0.5f*std::log(spectrumPower[(size_t)k]+1.0e-18f);
                const float r=0.5f*std::log(spectrumPower[(size_t)(k+1)]+1.0e-18f);
                const float denom=l-2.0f*m+r;
                const float delta=(denom<-1.0e-6f)?juce::jlimit(-0.5f,0.5f,0.5f*(l-r)/denom):0.0f;
                const float hz=float((k+delta)*analysisSampleRate/N);
                const float amp=std::sqrt(p);
                if (peakCount<maxPeaks) peaks[(size_t)peakCount++]={hz,amp};
                else { // keep the strongest peaks only
                    int weakest=0;
                    for (int i=1;i<maxPeaks;++i) if (peaks[(size_t)i].amp<peaks[(size_t)weakest].amp) weakest=i;
                    if (amp>peaks[(size_t)weakest].amp) peaks[(size_t)weakest]={hz,amp};
                }
            }
        }
    }
    // ---- Tuning: mode of the peaks' deviation from the 440 Hz semitone grid --------
    // Deviations are periodic (100 cents), so the estimate spans the full +-50 cents
    // (A=432 Hz is -31.8 cents) with no rejection band. A histogram *mode* is used
    // rather than a mean because partials of a harmonic tone are not equal-tempered
    // (the 5th/7th/11th lie +14/-31/-49 cents off the grid) and would bias a mean,
    // whereas grid-aligned partials (1st, 2nd, 3rd, 4th, 6th, 8th) all vote together.
    {
        float maxAmp=0.0f;
        for (int i=0;i<peakCount;++i) maxAmp=std::max(maxAmp,peaks[(size_t)i].amp);
        std::array<float,100> frameHist {};
        float frameMass=0.0f;
        for (int i=0;i<peakCount;++i) {
            const auto& pk=peaks[(size_t)i];
            if (pk.hz<110.0f || pk.hz>3500.0f || pk.amp<0.05f*maxAmp) continue;
            const float midi=69.0f+12.0f*std::log2(pk.hz/ref);
            const float dev=midi-std::floor(midi);                   // 0..1 semitone
            const float w=std::sqrt(pk.amp);
            const float pos=dev*100.0f;
            const int b0=((int)pos)%100, b1=(b0+1)%100;
            const float frac=pos-std::floor(pos);
            frameHist[(size_t)b0]+=w*(1.0f-frac); frameHist[(size_t)b1]+=w*frac;
            frameMass+=w;
        }
        if (peakCount>=4 && frameMass>0.0f) {
            constexpr float decay=0.95f;
            for (int k=0;k<100;++k) tuningHist[(size_t)k]=decay*tuningHist[(size_t)k]+frameHist[(size_t)k]/frameMass;
            tuningHistMass=decay*tuningHistMass+1.0f;
            ++tuningFrames;
        }
        // Circular smoothing (triangular, +-8 cents) and mode pick.
        float bestVal=-1.0f; int bestBin=0;
        std::array<float,100> smooth {};
        for (int k=0;k<100;++k) {
            float v=0.0f;
            for (int j=-8;j<=8;++j) v+=tuningHist[(size_t)((k+j+100)%100)]*(9.0f-std::abs((float)j));
            smooth[(size_t)k]=v;
            if (v>bestVal) {bestVal=v;bestBin=k;}
        }
        // Parabolic refinement of the mode.
        const float yl=smooth[(size_t)((bestBin+99)%100)], ym=smooth[(size_t)bestBin], yr=smooth[(size_t)((bestBin+1)%100)];
        const float den=yl-2.0f*ym+yr;
        const float sub=std::abs(den)>1.0e-9f?juce::jlimit(-0.5f,0.5f,0.5f*(yl-yr)/den):0.0f;
        float cents=float(bestBin)+sub;                               // 0..100
        if (cents>=50.0f) cents-=100.0f;                              // wrap to -50..+50
        // Confidence: how much of all evidence sits within +-12 cents of the mode,
        // against the 24% a flat (untuned / atonal) distribution would give.
        float near=0.0f;
        for (int j=-12;j<=12;++j) near+=tuningHist[(size_t)((bestBin+j+100)%100)];
        const float fraction=tuningHistMass>1.0e-4f?near/tuningHistMass:0.0f;
        const float maturity=juce::jlimit(0.0f,1.0f,tuningHistMass/8.0f);
        const float conf=juce::jlimit(0.0f,1.0f,(fraction-0.32f)/0.45f)*maturity;
        if (tuningFrames>=6 && conf>0.05f) {
            tuningCentsMemory=cents;
            tuningApplied = conf>0.20f;
            detectedTuningHz.store(ref*std::pow(2.0f,tuningCentsMemory/1200.0f));
            tuningConfidence.store(conf);
        } else {
            tuningApplied=false;
            tuningConfidence.store(tuningConfidence.load()*0.90f);
            if (tuningConfidence.load()<0.025f) { detectedTuningHz.store(440.0f); tuningCentsMemory=0.0f; }
        }
    }
    // ---- Note salience by sub-harmonic summation on the tuned grid -------------
    {
        const float appliedRef = ref*std::pow(2.0f,(tuningApplied?tuningCentsMemory:0.0f)/1200.0f);
        std::array<std::array<float,3>,60> best {};
        for (int i=0;i<peakCount;++i) {
            const auto& pk=peaks[(size_t)i];
            const float midi=69.0f+12.0f*std::log2(pk.hz/appliedRef);
            for (int h=1;h<=3;++h) {
                const float note=midi-12.0f*std::log2(float(h)); // the note whose h-th harmonic this is
                const int m=(int)std::lround(note);
                if (m<36 || m>95) continue;
                const float d=std::abs(note-float(m));
                const float kernel=std::cos(float(pi)*d); // 1 at the note centre, 0 at the semitone boundary
                const float v=pk.amp*kernel*kernel;
                auto& slot=best[(size_t)(m-36)][(size_t)(h-1)];
                slot=std::max(slot,v);
            }
        }
        for (int midi=36;midi<=95;++midi) {
            const auto& b=best[(size_t)(midi-36)];
            float score=0.0f;
            for (int h=1;h<=3;++h) score+=b[(size_t)(h-1)]/float(h*h);
            const float fundamental=b[0];
            // Reduce false note evidence from upper partials of lower notes.
            const float noteSalience = std::sqrt(std::max(0.0f,score))*0.75f
                                      + std::sqrt(std::max(0.0f,fundamental))*0.25f;
            c[(size_t)(midi%12)] += noteSalience;
            // Separate mid-register chroma limits bias from low kick/bass energy
            // and high-register overtones without discarding those registers.
            if (midi >= 48 && midi <= 83) midChroma[(size_t)(midi%12)] += noteSalience;
            // Separate low-register tonal evidence.
            if (midi >= 36 && midi <= 59) bassChroma[(size_t)(midi%12)] += noteSalience;
        }
    }
    // Normalize full-range and mid-register evidence independently.
    // A band with more candidate note bins should not win simply by mass.
    const float allMass = std::accumulate(c.begin(),c.end(),0.0f);
    const float midMass = std::accumulate(midChroma.begin(),midChroma.end(),0.0f);
    if (allMass > 1.0e-9f && midMass > 1.0e-9f) {
        for (int pc=0; pc<12; ++pc)
            c[(size_t)pc] = 0.25f*c[(size_t)pc]/allMass
                          + 0.75f*midChroma[(size_t)pc]/midMass;
    }
    const float total=std::accumulate(c.begin(),c.end(),0.0f);
    if(total<1.0e-9f)return;
    temporalKeyConsensus.observe(c);
    const float alpha=state.getRawParameterValue("responsiveness")->load();
    for(int i=0;i<12;++i){
        chromaMemory[(size_t)i]=(1-alpha)*chromaMemory[(size_t)i]+alpha*c[(size_t)i]/total;
        liveChroma[(size_t)i].store(chromaMemory[(size_t)i]);
    }
    // Long-term key evidence. A key is a property of a passage, not of the chord
    // sounding right now, so the ranking uses a ~20 s memory (scaled by the
    // responsiveness parameter) instead of the ~0.5 s chord-scale memory above.
    // Until that much audio has been heard it is a plain running average, so
    // the first estimate still arrives quickly and is not biased by zeros.
    {
        ++keyChromaFrames;
        const float hopSeconds=float(hopSize/analysisSampleRate);
        const float longAlpha=juce::jlimit(1.0e-4f,0.5f,(alpha/0.18f)*hopSeconds/20.0f);
        // Offline files: the key of a file is its whole-file average, not its last 20 s.
        const float a=holdResultsOnSilence ? 1.0f/float(keyChromaFrames)
                                           : std::max(1.0f/float(keyChromaFrames),longAlpha);
        for(int i=0;i<12;++i)
            keyChroma[(size_t)i]=(1.0f-a)*keyChroma[(size_t)i]+a*c[(size_t)i]/total;
    }
    // File analysis only: remember how the piece begins (first 10 s) and how it ends
    // (~6 s memory that holds through a silent tail). Music usually opens and closes on
    // the tonic, which is independent evidence for the key.
    if(holdResultsOnSilence){
        const float secs=float(analyzedSamples/analysisSampleRate);
        const float ea=float(hopSize/analysisSampleRate)/6.0f;
        for(int i=0;i<12;++i){
            const float v=c[(size_t)i]/total;
            if(secs<10.0f) startChroma[(size_t)i]+=v;
            endChroma[(size_t)i]=(1.0f-ea)*endChroma[(size_t)i]+ea*v;
        }
    }
    // Track-level feature extraction uses exactly the features provided to the
    // independently supervised trainer. The untrained model cannot affect rank.
    learnedTonalFeatures.observe(chromaMemory,lastBassFrame.pc,lastBassFrame.confidence);
    temporalTonalFeatures.observe(chromaMemory,c,lastBassFrame.pc,lastBassFrame.confidence);
    const auto learned = (learnedTonalFeatures.frames >= 64)
        ? skell::predictTonalCenter(learnedTonalFeatures.values()) : skell::TonalPrediction{};
    // Tonal-center evidence is independent of the Krumhansl profile.
    // Its contribution is deliberately bounded: bass can indicate a root,
    // but a bass line alone is not sufficient to name the key.
    const float bassMass = std::accumulate(bassChroma.begin(),bassChroma.end(),0.0f);
    std::array<float,12> bassEvidence{};
    if (bassMass > 1.0e-7f) {
        for (int root=0; root<12; ++root) {
            const float tonic = bassChroma[(size_t)root]/bassMass;
            const float fifth = bassChroma[(size_t)((root+7)%12)]/bassMass;
            bassEvidence[(size_t)root] = juce::jlimit(-0.08f,0.08f,
                0.16f*(tonic-1.0f/12.0f) + 0.06f*(fifth-1.0f/12.0f));
        }
    }
    // Phrase-level prior from distinct, credible chord transitions. A repeated
    // ostinato does not accrue an unlimited bonus simply by lasting longer.
    // The small bonus cannot overpower strong pitch-class evidence.
    std::array<float,12> progressionBonus{};
    if (progressionEvents >= 3) {
        const float mass=std::accumulate(progressionRootEvidence.begin(),
                                         progressionRootEvidence.end(),0.0f);
        if (mass > 0.0f) for (int root=0;root<12;++root) {
            const float relative=progressionRootEvidence[(size_t)root]/mass;
            progressionBonus[(size_t)root]=juce::jlimit(-0.025f,0.035f,
                0.10f*(relative-1.0f/12.0f));
        }
    }
    // Tonic-triad template (root, fifth, third) matched against how the file opens and closes.
    auto triadCos=[](const std::array<float,12>& v,int root,int mode){
        float t[12]={}; t[root]=1.0f; t[(root+(mode==0?4:3))%12]=0.7f; t[(root+7)%12]=0.8f;
        float ab=0,aa=0,bb=0; for(int i=0;i<12;++i){ab+=v[(size_t)i]*t[i];aa+=v[(size_t)i]*v[(size_t)i];bb+=t[i]*t[i];}
        return ab/std::sqrt(std::max(1.0e-12f,aa*bb)); };
    auto boundaryEvidence=[&](int root,int mode)->float{
        if(!holdResultsOnSilence) return 0.0f;
        return 1.0f*0.5f*(triadCos(startChroma,root,mode)+triadCos(endChroma,root,mode)); };
    struct Candidate{float score;int root,mode;};
    std::array<Candidate,24> ranking{};int ix=0;
    for(int root=0;root<12;++root)for(int mode=0;mode<2;++mode)
        ranking[(size_t)ix++]={boundaryEvidence(root,mode)+skell::keyCandidateScore(
            keyChroma,root,mode,mode==0?majorProfile:minorProfile,
            bassEvidence,progressionBonus,phraseTracker.bonus(root),
            beatGrid.bonus(root),downbeatChordTracker.tonicBonus(root),
            relativeModeEvidence.bonus(root,mode),learned),root,mode};
    std::partial_sort(ranking.begin(),ranking.begin()+3,ranking.end(),
        [](const auto& a,const auto& b){return a.score>b.score;});
    const auto consensus=temporalKeyConsensus.propose(ranking[0].root,ranking[0].mode,
                                                       phraseTracker,majorProfile,minorProfile);
    if(consensus.root>=0){
        auto it=std::find_if(ranking.begin(),ranking.end(),[&](const auto& candidate){
            return candidate.root==consensus.root && candidate.mode==consensus.mode;
        });
        if(it!=ranking.end()){
            const auto selected=*it;
            for(auto walk=it;walk!=ranking.begin();--walk)*walk=*(walk-1);
            ranking[0]=selected;
        }
    }
    for(int i=0;i<3;++i){candidateRoots[(size_t)i].store(ranking[(size_t)i].root);candidateModes[(size_t)i].store(ranking[(size_t)i].mode);candidateScores[(size_t)i].store(ranking[(size_t)i].score);}
    const auto& best=ranking[0];
    keyConsistency.observe(best.root,best.mode);
    if(best.root==pendingRoot&&best.mode==pendingMode)++pendingCount;
    else{pendingRoot=best.root;pendingMode=best.mode;pendingCount=1;}
    if(pendingCount>=4&&best.score>0.30f){stableRoot=best.root;stableMode=best.mode;}
    detectedRoot.store(stableRoot);detectedMode.store(stableMode);
    // Only report key confidence when the leading candidate is both
    // distinguishable from the runner-up and itself a credible match.
    const float keyMargin = std::max(0.0f, best.score-ranking[1].score);
    confidence.store(keyConsistency.displayEvidence(
        keyMargin, best.score, keyConsistency.agreement(stableRoot,stableMode),
        relativeModeEvidence.confirmedSupport(stableRoot,stableMode)));
    // Short-memory chord analysis; don't use long-term key chroma to label each chord.
    // Penalize out-of-chord notes and require concentrated pitch-class energy.
    const float chordAlpha=0.42f;
    for(int i=0;i<12;++i)chordMemory[(size_t)i]=(1.0f-chordAlpha)*chordMemory[(size_t)i]+chordAlpha*c[(size_t)i]/total;
    constexpr int intervals[4][3]={{0,4,7},{0,3,7},{0,3,6},{0,4,8}};
    struct ChordScore {float value;int root,kind;};
    std::array<ChordScore,48> scores{};
    int index=0;
    // The chroma vector is normalized; an out-of-chord scan is unnecessary.
    // Compute the three chord tones directly, then derive outside energy.
    const float chromaTotal=std::accumulate(chordMemory.begin(),chordMemory.end(),0.0f);
    const auto top=std::max_element(chordMemory.begin(),chordMemory.end());
    const float peak=*top;
    for(int root=0;root<12;++root)for(int kind=0;kind<4;++kind){
        const float a=chordMemory[(size_t)((root+intervals[kind][0])%12)];
        const float b=chordMemory[(size_t)((root+intervals[kind][1])%12)];
        const float c3=chordMemory[(size_t)((root+intervals[kind][2])%12)];
        const float inside=a+b+c3;
        const float minimum=std::min(a,std::min(b,c3));
        const float outside=std::max(0.0f,chromaTotal-inside);
        scores[(size_t)index++]={inside-0.70f*outside+0.35f*minimum,root,kind};
    }
    std::partial_sort(scores.begin(),scores.begin()+2,scores.end(),
        [](const auto& a,const auto& b){return a.value>b.value;});
    const auto& winner=scores[0];
    const float margin=winner.value-scores[1].value;
    const float cc=juce::jlimit(0.0f,1.0f,margin*7.0f);
    // Abstain on dense/polyphonic mixtures and uncertain winner separation.
    // Three-tone support is essential: isolated bass notes and prominent
    // harmonic overtones must not be labelled as full major/minor chords.
    const float tone0=chordMemory[(size_t)winner.root];
    const float tone1=chordMemory[(size_t)((winner.root+intervals[winner.kind][1])%12)];
    const float tone2=chordMemory[(size_t)((winner.root+intervals[winner.kind][2])%12)];
    const float weakestTone=std::min(tone0,std::min(tone1,tone2));
    const float chordSupport=tone0+tone1+tone2;
    const bool credible=winner.value>0.12f && margin>0.018f && peak>0.12f
        && chordSupport > 0.53f * chromaTotal
        && weakestTone > 0.055f * chromaTotal;
    if(credible){
        if(heldChordRoot==winner.root&&heldChordKind==winner.kind)chordHold=std::min(10,chordHold+1);
        else {heldChordRoot=winner.root;heldChordKind=winner.kind;chordHold=1;}
        if(chordHold>=2){
            chordRoot.store(winner.root);chordKind.store(winner.kind);chordConfidence.store(cc);
            // Count a harmonic event only when a credible triad actually changes.
            // Avoid learning the same looped chord on every FFT frame.
            if ((winner.root!=lastProgressionRoot || winner.kind!=lastProgressionKind)
                && cc > 0.12f) {
                for (float& vote:progressionRootEvidence) vote*=0.985f;
                progressionRootEvidence[(size_t)winner.root]+=std::min(cc,1.0f);
                lastProgressionRoot=winner.root;
                lastProgressionKind=winner.kind;
                progressionEvents=std::min(100000,progressionEvents+1);
            }
        }
    } else {
        chordHold=0;heldChordRoot=heldChordKind=-1;
        chordRoot.store(-1);chordKind.store(-1);chordConfidence.store(0);
    }
    // Vote only when a pitched bass and a harmonic tonic triad agree.
    // This evidence is independent of the spectral key-profile correlation.
    relativeModeEvidence.observe(c,lastBassFrame.pc,lastBassFrame.confidence,
                                 analyzedSamples / analysisSampleRate,
                                 chordRoot.load(),chordKind.load(),chordConfidence.load());
    // A new, worker-owned bass/harmonic phrase state. The evidence from the
    // completed phrase is available on the NEXT key-ranking frame; no future audio.
    phraseTracker.ingest(lastBassFrame, c, chordRoot.load(), chordKind.load(),
                         chordConfidence.load(), detectedBpm.load(),
                         static_cast<float>(hopSize / analysisSampleRate));
    beatGrid.ingest(c, lastBassFrame.pc, lastBassFrame.confidence,
                    chordRoot.load(), chordKind.load(), chordConfidence.load(),
                    analyzedSamples / analysisSampleRate);
    // V39: independent per-beat transcription, separate from the established
    // triad/key estimators. No audio-thread changes or additional FFTs.
    downbeatChordTracker.ingest(c,lastBassFrame.pc,lastBassFrame.confidence,
                                analyzedSamples / analysisSampleRate);
    const auto transcribed=downbeatChordTracker.current();
    if (transcribed.valid() && transcribed.evidence>=0.17f) {
        transcribedChordRoot.store(transcribed.root);
        transcribedChordQuality.store(transcribed.quality);
        transcribedChordBass.store(transcribed.inversion>0 ? transcribed.bass : -1);
        transcribedChordEvidence.store(transcribed.evidence);
    } else {
        transcribedChordRoot.store(-1);transcribedChordQuality.store(-1);
        transcribedChordBass.store(-1);transcribedChordEvidence.store(0.0f);
    }
}

// ---- Click-to-hear key preview ------------------------------------------------
// Plays the detected scale upwards and then its tonic triad. Runs entirely on the
// audio thread from a request flag; no allocation or locking.
void TonicRecreationAudioProcessor::playKeyPreview(int root,int mode){
    if(root<0||root>11||(mode!=0&&mode!=1))return;
    previewRoot.store(root,std::memory_order_relaxed);previewMode.store(mode,std::memory_order_relaxed);
    previewRequest.store(true,std::memory_order_release);
}
void TonicRecreationAudioProcessor::renderPreview(juce::AudioBuffer<float>& buffer){
    const int n=buffer.getNumSamples(),chs=buffer.getNumChannels();
    if(previewRequest.exchange(false,std::memory_order_acq_rel)){
        const int root=previewRoot.load(),mode=previewMode.load();
        static const int major[8]={0,2,4,5,7,9,11,12}, minor[8]={0,2,3,5,7,8,10,12};
        const int* sc=mode==0?major:minor;
        for(auto& v:voices)v.active=false;
        const double step=currentSampleRate*0.26; // one note every 260 ms
        int vi=0;
        for(int i=0;i<8;++i){ // ascending scale from C4-ish tonic
            auto& v=voices[(size_t)vi++];
            v={true,60+root+sc[i],0.0,(juce::int64)(step*i),(juce::int64)(step*0.95)};
        }
        const juce::int64 chordStart=(juce::int64)(step*8.4);
        const int tri[3]={0,mode==0?4:3,7};
        for(int i=0;i<3;++i){ // triad, held
            auto& v=voices[(size_t)vi++];
            v={true,48+root+tri[i]+12*(i==0?0:0),0.0,chordStart,(juce::int64)(currentSampleRate*1.8)};
        }
        previewClock=0;previewTotal=chordStart+(juce::int64)(currentSampleRate*1.9);
        previewRunning=true;
    }
    if(!previewRunning){previewNote.store(-1,std::memory_order_relaxed);return;}
    int shown=-1;
    for(int i=0;i<n;++i){
        float mix=0.0f;
        const juce::int64 now=previewClock+i;
        for(auto& v:voices){
            if(!v.active)continue;
            const juce::int64 t=now-v.start;
            if(t<0)continue;
            if(t>v.length){v.active=false;continue;}
            const double f=440.0*std::pow(2.0,(v.midi-69)/12.0);
            const float age=(float)t/(float)currentSampleRate;
            float s=0.0f; double ph=v.phase;
            // Soft piano-like pluck: a few decaying harmonics.
            for(int h=1;h<=5;++h)
                s+=std::sin((float)(2.0*pi*f*h*(double)t/currentSampleRate))*std::exp(-age*(2.0f+1.6f*(float)h))/(float)h;
            (void)ph;
            const float atk=std::min(1.0f,age*400.0f), rel=std::min(1.0f,(float)(v.length-t)/(float)(currentSampleRate*0.04));
            mix+=s*atk*rel*0.16f;
            if(v.start<=now && now-v.start<(juce::int64)(currentSampleRate*0.2)) shown=v.midi;
        }
        for(int ch=0;ch<chs;++ch)buffer.getWritePointer(ch)[i]+=mix;
    }
    previewNote.store(shown>=0?shown%12:previewNote.load(),std::memory_order_relaxed);
    previewClock+=n;
    if(previewClock>=previewTotal){previewRunning=false;for(auto& v:voices)v.active=false;previewNote.store(-1,std::memory_order_relaxed);}
}
juce::AudioProcessorEditor* TonicRecreationAudioProcessor::createEditor(){return new TonicRecreationAudioProcessorEditor(*this);}
void TonicRecreationAudioProcessor::getStateInformation(juce::MemoryBlock& dest){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,dest);}
void TonicRecreationAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size);xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TonicRecreationAudioProcessor();}
