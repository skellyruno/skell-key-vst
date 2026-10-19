#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr int N=16384;
constexpr double pi=3.14159265358979323846;
constexpr float majorProfile[12]={6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
constexpr float minorProfile[12]={6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};
float correlation(const std::array<float,12>& c, int root, const float* profile) {
    float avgC=0,avgP=0;
    for(int i=0;i<12;++i) {avgC+=c[(i+root)%12];avgP+=profile[i];}
    avgC/=12;avgP/=12;
    float ab=0, aa=0, bb=0;
    for(int i=0;i<12;++i) {float a=c[(i+root)%12]-avgC,b=profile[i]-avgP;ab+=a*b;aa+=a*a;bb+=b*b;}
    return ab/std::sqrt(std::max(1.0e-12f,aa*bb));
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
    chordMemory.fill(0); heldChordRoot=heldChordKind=-1; chordHold=0; tuningCentsMemory=0; tempoAmbiguity=0;
    chromaMemory.fill(0);history.fill(0);historyWrite=historyCount=samplesSinceWindow=silentFrames=0;
    stableRoot=pendingRoot=-1;stableMode=pendingMode=2;pendingCount=0;
    detectedRoot.store(-1);detectedMode.store(2);confidence.store(0);
    chordRoot.store(-1);chordKind.store(-1);chordConfidence.store(0);
    detectedTuningHz.store(440.0f); tuningConfidence.store(0.0f);
    detectedBpm.store(0); bpmConfidence.store(0); lastStableBpm=0;
    onsetHistory.fill(0); onsetWrite=onsetCount=onsetSamples=bpmAnalysisFrames=0;
    onsetEnergySum=previousEnergy=smoothedOnset=0;
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
    tempoHopSamples=std::max(1,juce::roundToInt(analysisSampleRate/onsetHopRate));
    // Using the measured novelty-envelope rate avoids tempo drift when the
    // sample rate is not an exact multiple of 100 Hz.
    actualOnsetRate=static_cast<float>(analysisSampleRate/tempoHopSamples);
    for(int midi=36;midi<=95;++midi){
        const int index=midi-36;
        const float frequency=440.0f*std::pow(2.0f,(midi-69)/12.0f);
        noteFrequencies[(size_t)index]=frequency;
        for(int harmonic=1;harmonic<=3;++harmonic){
            const float hz=frequency*harmonic;
            // Never read outside the FFT spectrum at low sample rates.
            noteBins[(size_t)index*3+(size_t)(harmonic-1)]=
                hz<analysisSampleRate*0.45 ? juce::roundToInt(hz*windowSize/analysisSampleRate) : -1;
        }
    }
}
void TonicRecreationAudioProcessor::releaseResources(){
    worker.signalThreadShouldExit();analysisReady.signal();worker.stopThread(5000);
}
bool TonicRecreationAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const{
    const auto a=l.getMainInputChannelSet(),b=l.getMainOutputChannelSet();
    return (a==juce::AudioChannelSet::mono()||a==juce::AudioChannelSet::stereo())&&a==b;
}
void TonicRecreationAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&){
    juce::ScopedNoDenormals nd;
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
    // A signal is considered present once a block exceeds the threshold; sustained
    // silence clears the flag, making drag-and-drop available again.
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
    auto copy=[&](int dest,int from,int n){for(int i=0;i<n;++i){float s=0;for(int c=0;c<channels;++c)s+=buffer.getReadPointer(c)[from+i];sampleQueue[(size_t)(dest+i)]=s/float(channels);}};
    copy(start1,0,size1);copy(start2,size1,size2);
    sampleFifo.finishedWrite(size1+size2);
    if(size1+size2>0)analysisReady.signal();
    // Transparent analyzer; audio is left untouched.
}
bool TonicRecreationAudioProcessor::requestFileAnalysis(const juce::File& file) {
    if (!file.existsAsFile() || hasLiveInput()) return false;
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
    consumeTempoSample(sample);
    history[(size_t)historyWrite]=sample;
    historyWrite=(historyWrite+1)%windowSize;
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
    if (!reader || reader->sampleRate <= 0.0 || reader->numChannels==0) {
        if (generation == fileRequestGeneration.load()) { fileError.store(true); fileBusy.store(false); }
        return;
    }
    discardLiveQueue();
    resetAnalysis();
    rebuildFrequencyMap(reader->sampleRate);
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
        if(!reader->read(&decoded,0,n,offset,true,true)){fileError.store(true);break;}
        for(int i=0;i<n;++i){
            float value=decoded.getSample(0,i);
            if(reader->numChannels>1)value=0.5f*(value+decoded.getSample(1,i));
            analyseAudioSample(value);
        }
        if ((offset/blockSize)%32==0) {
            fileProgress.store(total>0?static_cast<float>(double(offset+n)/double(total)):1.0f);
            fileAnalyzedSeconds.store(static_cast<float>((offset+n)/reader->sampleRate));
        }
        if((offset/blockSize)%128==0)juce::Thread::yield();
    }
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
            for(int i=0;i<count;++i)parent.analyseAudioSample(batch[(size_t)i]);
        }
    }
}
// Tempo detection is deliberately independent of the FFT key-analysis window.
// Compute a 100 Hz log-energy novelty envelope, then normalized onset autocorrelation.
// This estimates a repeating beat interval, not a guaranteed downbeat or meter.
void TonicRecreationAudioProcessor::consumeTempoSample(float sample) {
    onsetEnergySum += sample * sample;
    ++onsetSamples;
    if (onsetSamples < tempoHopSamples) return;
    const float energy = std::log1p(1000.0f * onsetEnergySum / onsetSamples);
    const float rise = std::max(0.0f, energy - previousEnergy);
    previousEnergy = 0.7f * previousEnergy + 0.3f * energy;
    smoothedOnset = 0.65f * smoothedOnset + 0.35f * rise;
    onsetHistory[static_cast<size_t>(onsetWrite)] = smoothedOnset;
    onsetWrite = (onsetWrite + 1) % onsetHistoryLength;
    onsetCount = std::min(onsetCount + 1, onsetHistoryLength);
    onsetSamples = 0; onsetEnergySum = 0.0f;
    if (++bpmAnalysisFrames >= 150) {
        bpmAnalysisFrames = 0;
        estimateTempo();
    }
}

void TonicRecreationAudioProcessor::estimateTempo() {
    if (onsetCount < 600) return;
    constexpr int length=1200;
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
    if(energy<0.001f){detectedBpm.store(0);bpmConfidence.store(0);lastStableBpm=0;tempoAmbiguity=0;return;}
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
    float best=-1.0f, second=-1.0f;int bestLag=0;
    const float host=hostBpmValid.load()?hostBpm.load():0.0f;
    for(int lag=minLag;lag<=maxLag;++lag){
        const float bpm=60.0f*actualOnsetRate/lag;
        float score=corr[(size_t)lag];
        if(lag*2<=maxLag)score+=0.30f*corr[(size_t)(lag*2)];
        if(lag/2>=minLag && lag%2==0)score+=0.12f*corr[(size_t)(lag/2)];
        // A small prior makes pulse interpretations near conventional beat rates more stable.
        score*=0.92f+0.08f*std::exp(-0.5f*std::pow(std::log2(bpm/115.0f)/0.9f,2));
        if(lastStableBpm>0.0f && std::abs(std::log2(bpm/lastStableBpm))<0.10f)score*=1.06f;
        if(host>0.0f && std::abs(std::log2(bpm/host))<0.04f)score*=1.04f;
        if(score>best){second=best;best=score;bestLag=lag;}
        else if(score>second)second=score;
    }
    if(bestLag==0 || corr[(size_t)bestLag]<0.18f){detectedBpm.store(0);bpmConfidence.store(0);return;}
    const float bpm=60.0f*actualOnsetRate/bestLag;
    const int halfLag=bestLag*2, doubleLag=bestLag/2;
    const float half=halfLag<=maxLag?corr[(size_t)halfLag]:0.0f;
    const float dbl=doubleLag>=minLag?corr[(size_t)doubleLag]:0.0f;
    tempoAmbiguity=juce::jlimit(0.0f,1.0f,std::max(half,dbl)/std::max(0.01f,corr[(size_t)bestLag]));
    const float conf=juce::jlimit(0.0f,1.0f,(corr[(size_t)bestLag]-0.15f)/0.60f)*(1.0f-0.30f*tempoAmbiguity);
    // Ignore a single sudden octave jump when pulse evidence is ambiguous.
    const bool octaveJump = lastStableBpm > 0.0f &&
        (std::abs(bpm / lastStableBpm - 2.0f) < 0.10f ||
         std::abs(bpm / lastStableBpm - 0.5f) < 0.05f);
    if(octaveJump && tempoAmbiguity.load() > 0.72f) {
        bpmConfidence.store(conf * 0.5f);
        detectedBpm.store(lastStableBpm);
        return;
    }
    if(lastStableBpm==0.0f || std::abs(bpm-lastStableBpm)>8.0f)lastStableBpm=bpm;
    else lastStableBpm=0.75f*lastStableBpm+0.25f*bpm;
    detectedBpm.store(lastStableBpm);bpmConfidence.store(conf);
}

void TonicRecreationAudioProcessor::analyseWindow(){
    double energy=0;
    for(int i=0;i<N;++i){
        const float v=history[(size_t)((historyWrite+i)%N)];
        energy+=double(v)*v;
        fftBuffer[(size_t)i]=v*hannWindow[(size_t)i];
    }
    const float rms=float(std::sqrt(energy/N));
    const float gate=state.getRawParameterValue("sensitivity")->load();
    if(rms<gate){
        if(++silentFrames>=4){
            chromaMemory.fill(0);stableRoot=pendingRoot=-1;stableMode=pendingMode=2;pendingCount=0;
            detectedRoot.store(-1);detectedMode.store(2);chordRoot.store(-1);chordKind.store(-1);chordConfidence.store(0);
            for(auto& v:liveChroma)v.store(0);
            for(int i=0;i<3;++i)candidateRoots[(size_t)i].store(-1);
        }
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
    std::array<float,12> c{};
    float tuningWeighted=0,tuningWeight=0,tuningWeightSquared=0;
    int tuningPeakCount=0;
    // Multiple note hypotheses can land on the same FFT peak. Count each
    // spectral peak only once to avoid biased A-reference estimates.
    std::array<unsigned char,N/2> visitedTuningBins {};
    for(int midi=36;midi<=95;++midi){
        const float f=noteFrequencies[(size_t)(midi-36)];
        float fundamental=0,score=0;
        for(int h=1;h<=3;++h){
            const float hz=f*h;
            if(hz>analysisSampleRate*0.45)break;
            const int bin=noteBins[(size_t)(midi-36)*3+(size_t)(h-1)];
            if(bin<1 || bin>=N/2-2) continue;
            float best=0;int bestBin=bin;
            for(int j=std::max(1,bin-2);j<=std::min(N/2-1,bin+2);++j){
                const float amplitude=spectrumPower[(size_t)j];
                if(amplitude>best){best=amplitude;bestBin=j;}
            }
            const float magnitude=std::sqrt(best);
            if(h==1){fundamental=magnitude;
                // Only high-resolution notes make useful FFT-bin tuning measurements.
                if(bin>70 && bestBin>1 && bestBin<N/2-2 && magnitude>0.01f
                   && !visitedTuningBins[(size_t)bestBin]
                   && spectrumPower[(size_t)bestBin] > spectrumPower[(size_t)(bestBin-1)]
                   && spectrumPower[(size_t)bestBin] >= spectrumPower[(size_t)(bestBin+1)]){
                    // Three-point parabolic interpolation of LOG spectral magnitudes.
                    // Sub-bin frequency estimation is much less quantized than bestBin alone.
                    auto logMag=[&](int k){
                        return 0.5f*std::log(spectrumPower[(size_t)k]+1.0e-18f);
                    };
                    const float left=logMag(bestBin-1),mid=logMag(bestBin),right=logMag(bestBin+1);
                    const float denom=left-2.0f*mid+right;
                    const float delta=std::abs(denom)>1.0e-6f?juce::jlimit(-0.5f,0.5f,0.5f*(left-right)/denom):0.0f;
                    const float cents=1200.0f*std::log2(float((bestBin+delta)*analysisSampleRate/N)/f);
                    if(std::abs(cents)<30){
                        visitedTuningBins[(size_t)bestBin]=1;
                        // Avoid counting weak leakage bins as tuning observations.
                        const float weight=std::min(magnitude,100.0f);
                        tuningWeighted+=cents*weight;tuningWeight+=weight;
                        tuningWeightSquared+=weight*weight;
                        ++tuningPeakCount;
                    }
                }
            }
            score+=magnitude/float(h*h);
        }
        // Reduce false note evidence from upper partials of lower notes.
        c[(size_t)(midi%12)]+=std::sqrt(std::max(0.0f,score))*0.75f+std::sqrt(std::max(0.0f,fundamental))*0.25f;
    }
    const float effectivePeaks=tuningWeightSquared>0.0f
        ?(tuningWeight*tuningWeight/tuningWeightSquared):0.0f;
    if(tuningWeight>5.0f && tuningPeakCount>=3 && effectivePeaks>=2.0f){
        const float cents=juce::jlimit(-35.0f,35.0f,tuningWeighted/tuningWeight);
        tuningCentsMemory=0.94f*tuningCentsMemory+0.06f*cents;
        detectedTuningHz.store(ref*std::pow(2.0f,tuningCentsMemory/1200.0f));
        tuningConfidence.store(juce::jlimit(0.0f,1.0f,(effectivePeaks-1.0f)/8.0f));
    } else { tuningConfidence.store(tuningConfidence.load()*0.93f); }
    const float total=std::accumulate(c.begin(),c.end(),0.0f);
    if(total<1.0e-9f)return;
    const float alpha=state.getRawParameterValue("responsiveness")->load();
    for(int i=0;i<12;++i){
        chromaMemory[(size_t)i]=(1-alpha)*chromaMemory[(size_t)i]+alpha*c[(size_t)i]/total;
        liveChroma[(size_t)i].store(chromaMemory[(size_t)i]);
    }
    struct Candidate{float score;int root,mode;};
    std::array<Candidate,24> ranking{};int ix=0;
    for(int root=0;root<12;++root)for(int mode=0;mode<2;++mode)
        ranking[(size_t)ix++]={correlation(chromaMemory,root,mode==0?majorProfile:minorProfile),root,mode};
    std::partial_sort(ranking.begin(),ranking.begin()+3,ranking.end(),
        [](const auto& a,const auto& b){return a.score>b.score;});
    for(int i=0;i<3;++i){candidateRoots[(size_t)i].store(ranking[(size_t)i].root);candidateModes[(size_t)i].store(ranking[(size_t)i].mode);candidateScores[(size_t)i].store(ranking[(size_t)i].score);}
    const auto& best=ranking[0];
    if(best.root==pendingRoot&&best.mode==pendingMode)++pendingCount;
    else{pendingRoot=best.root;pendingMode=best.mode;pendingCount=1;}
    if(pendingCount>=4&&best.score>0.30f){stableRoot=best.root;stableMode=best.mode;}
    detectedRoot.store(stableRoot);detectedMode.store(stableMode);
    confidence.store(juce::jlimit(0.0f,1.0f,(best.score-ranking[1].score)*3.0f));
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
    const bool credible=winner.value>0.12f && margin>0.018f && peak>0.12f;
    if(credible){
        if(heldChordRoot==winner.root&&heldChordKind==winner.kind)chordHold=std::min(10,chordHold+1);
        else {heldChordRoot=winner.root;heldChordKind=winner.kind;chordHold=1;}
        if(chordHold>=2){chordRoot.store(winner.root);chordKind.store(winner.kind);chordConfidence.store(cc);}
    } else {
        chordHold=0;heldChordRoot=heldChordKind=-1;
        chordRoot.store(-1);chordKind.store(-1);chordConfidence.store(0);
    }
}
juce::AudioProcessorEditor* TonicRecreationAudioProcessor::createEditor(){return new TonicRecreationAudioProcessorEditor(*this);}
void TonicRecreationAudioProcessor::getStateInformation(juce::MemoryBlock& dest){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,dest);}
void TonicRecreationAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size);xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TonicRecreationAudioProcessor();}
