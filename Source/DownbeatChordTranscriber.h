#pragma once
// V39: Portable C++17, worker-only chord and bar-context evidence.
// Downbeats are hypotheses, NOT guaranteed bar boundaries. This module abstains
// if the pulse accents cannot distinguish a unique phase. Confidence is an
// uncalibrated evidence score, NOT a posterior probability.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace skell {

enum class ChordQuality : int {
    Major=0, Minor=1, Diminished=2, Augmented=3,
    Dominant7=4, Major7=5, Minor7=6, HalfDiminished7=7
};

constexpr std::array<std::array<int,4>,8> chordIntervals{{
    {{0,4,7,-1}}, {{0,3,7,-1}}, {{0,3,6,-1}}, {{0,4,8,-1}},
    {{0,4,7,10}}, {{0,4,7,11}}, {{0,3,7,10}}, {{0,3,6,10}}
}};

struct ChordEstimate {
    int root=-1, quality=-1, bass=-1, inversion=-1;
    float evidence=0.0f, support=0.0f, margin=0.0f;
    bool valid() const noexcept {return root>=0 && quality>=0;}
};

inline ChordEstimate transcribeChord(const std::array<float,12>& src, int bassPc=-1,
                                    float bassConfidence=0.0f) noexcept {
    std::array<float,12> c{};
    float sum=0.0f;
    for(int i=0;i<12;++i){ c[i]=std::max(0.0f,std::isfinite(src[i])?src[i]:0.0f); sum+=c[i]; }
    if(sum<1.0e-7f) return {};
    for(auto& x:c)x/=sum;
    struct Candidate { float value=-1.0e9f, support=0.0f, weakest=0.0f; int root=-1, quality=-1; } best,second;
    for(int root=0;root<12;++root)for(int q=0;q<8;++q) {
        const auto& degrees=chordIntervals[static_cast<std::size_t>(q)];
        float inside=0.0f,weakest=1.0f;
        const int n=q<4?3:4;
        for(int i=0;i<n;++i) {
            const float v=c[static_cast<std::size_t>((root+degrees[static_cast<std::size_t>(i)])%12)];
            inside+=v;weakest=std::min(weakest,v);
        }
        // Explicit support/weakest-tone requirements prevent 7th extensions
        // from appearing merely because noise happens to fill the fourth note.
        if(inside<(q<4?0.53f:0.61f) || weakest<(q<4?0.055f:0.060f))continue;
        const float outside=std::max(0.0f,1.0f-inside);
        // A seventh adds a fourth template note and therefore has a built-in
        // advantage on dense/polyphonic chroma. Its extension must overcome a
        // complexity cost before the more specific label wins. This reduces
        // the frequent false maj7/min7 labels on triads without disabling real
        // seventh recognition. Calibrated on development synthetics only;
        // NOT evidence of 90% generalization on real songs.
        constexpr float seventhComplexityCost = 0.205f;
        const float score=inside-0.64f*outside+0.26f*weakest
                         -(q<4?0.0f:seventhComplexityCost);
        Candidate curr{score,inside,weakest,root,q};
        if(score>best.value){second=best;best=curr;}
        else if(score>second.value)second=curr;
    }
    if(best.root<0)return {};
    const float margin=best.value-std::max(-0.7f,second.value);
    if(margin<0.022f)return {}; // close alternatives => no chord label
    // Conservative evidence score: fit * separation * smallest-tone support.
    // Not called a calibrated probability until validated on real annotations.
    const float supportFit=std::clamp((best.support-0.48f)/0.38f,0.0f,1.0f);
    const float gap=std::clamp((margin-0.014f)/0.16f,0.0f,1.0f);
    const float weakest=std::clamp(best.weakest/0.13f,0.0f,1.0f);
    const float evidence=std::clamp(supportFit*gap*weakest,0.0f,1.0f);
    if(evidence<0.17f)return {};
    ChordEstimate result;
    result.root=best.root;result.quality=best.quality;
    result.support=best.support;result.margin=margin;result.evidence=evidence;
    // Bass inversion is an independent observation. It must be confidently
    // pitched and belong to the chord template; otherwise do not invent it.
    if(bassPc>=0 && bassPc<12 && bassConfidence>=0.50f){
        const auto& degrees=chordIntervals[static_cast<std::size_t>(best.quality)];
        const int count=best.quality<4?3:4;
        for(int degree=0;degree<count;++degree)
            if((result.root+degrees[static_cast<std::size_t>(degree)])%12==bassPc) {
                result.bass=bassPc;result.inversion=degree;break;
            }
    }
    return result;
}

struct DownbeatAccent {
    int beatsAgoMod4=-1; // Accent at beat (most recent beat index - beatsAgoMod4)
    float evidence=0.0f; // Ambiguous strong backbeats should yield no downbeat
};

template <std::size_t N>
DownbeatAccent estimateDownbeatAccent(const std::array<float,N>& onset,int nextWrite,
                                       int count,float onsetRate,float bpm,float beatAgeSeconds) noexcept {
    if(!(onsetRate>20.0f && bpm>=60.f && bpm<=220.f) || count<onsetRate*14.f)
        return {};
    const float period=60.f*onsetRate/bpm;
    if(period<6.f || period>240.f)return {};
    const int beats=std::min(40,static_cast<int>((count-4-beatAgeSeconds*onsetRate)/period));
    if(beats<20)return {};
    const float age=std::max(0.f,beatAgeSeconds*onsetRate);
    std::array<float,4> sums{},counts{};
    for(int k=0;k<beats;++k){
        const int lag=static_cast<int>(std::lround(age+k*period));
        float pulse=0.f;
        for(int d=-2;d<=2;++d){
            const int index=lag+d;
            if(index<0 || index>=count)continue;
            const std::size_t offset=(static_cast<std::size_t>(nextWrite)+N-1-static_cast<std::size_t>(index))%N;
            pulse+=std::max(0.f,onset[offset])*(d==0?0.4f:0.15f);
        }
        sums[static_cast<std::size_t>(k%4)]+=pulse;
        counts[static_cast<std::size_t>(k%4)]+=1.f;
    }
    std::array<float,4> a{};
    for(int i=0;i<4;++i)a[i]=sums[i]/std::max(1.f,counts[i]);
    float total=std::accumulate(a.begin(),a.end(),0.f);
    if(total<1.e-5f)return {};
    int winner=0;
    for(int i=1;i<4;++i)if(a[i]>a[winner])winner=i;
    float runner=0;
    for(int i=0;i<4;++i)if(i!=winner)runner=std::max(runner,a[i]);
    const float contrast=(a[winner]-runner)/std::max(1.e-6f,a[winner]);
    const float aboveMean=(a[winner]-total*0.25f)/std::max(1.e-6f,a[winner]);
    // Deliberately strict: snare-driven beats 2 and 4 or uniform loops abstain.
    const float evidence=std::clamp((contrast-0.11f)*2.4f,0.f,1.f)
                        *std::clamp((aboveMean-0.17f)*2.5f,0.f,1.f);
    if(evidence<0.22f)return {};
    return {winner,evidence};
}

// Accumulates one chord estimate PER BEAT, rather than per analysis frame.
// A tonic bonus is gated by reliable chord changes, a stable downbeat phase,
// chord identity, and an independent tonic bass match. No allocations.
class DownbeatChordTracker {
public:
    void reset() noexcept { *this=DownbeatChordTracker{}; }
    void configure(float bpm,double lastBeatSeconds,float phaseEvidence,
                   DownbeatAccent downbeat) noexcept {
        if(bpm<60.f||bpm>220.f||phaseEvidence<0.09f)return;
        const double newPeriod=60.0/bpm;
        if(period_<=0.0 || std::abs(period_-newPeriod)/newPeriod>0.025) {
            reset();period_=newPeriod;anchor_=lastBeatSeconds;phaseEvidence_=phaseEvidence;
        }
        if(downbeat.beatsAgoMod4>=0 && downbeat.evidence>=0.22f) {
            const int recent=static_cast<int>(std::lround((lastBeatSeconds-anchor_)/period_));
            const int phase=((recent-downbeat.beatsAgoMod4)%4+4)%4;
            // Never keep changing the selected bar phase every time hats/snare vary.
            if(downbeatIndex_<0) {downbeatIndex_=phase; downbeatEvidence_=downbeat.evidence;}
            else if(phase==downbeatIndex_)downbeatEvidence_=0.85f*downbeatEvidence_+0.15f*downbeat.evidence;
            else downbeatEvidence_*=0.86f;
            if(downbeatEvidence_<0.19f)downbeatIndex_=-1;
        }
    }
    void ingest(const std::array<float,12>& chroma,int bassPc,float bassConfidence,
                double seconds) noexcept {
        if(period_<=0 || !std::isfinite(seconds) || seconds<anchor_)return;
        const int beat=static_cast<int>(std::floor((seconds-anchor_)/period_));
        if(beat<0)return;
        if(activeBeat_<0)activeBeat_=beat;
        if(beat!=activeBeat_){
            if(beat==activeBeat_+1) finishBeat();
            else {resetAccumulator();lastCandidate_={};candidateRun_=0;}
            activeBeat_=beat;
        }
        const float mass=std::accumulate(chroma.begin(),chroma.end(),0.f);
        if(mass>1.e-6f){
            for(int i=0;i<12;++i) tonal_[static_cast<std::size_t>(i)]+=std::max(0.f,chroma[i])/mass;
            ++frames_;
        }
        if(bassPc>=0&&bassPc<12&&bassConfidence>=0.35f)
            bassVotes_[static_cast<std::size_t>(bassPc)]+=bassConfidence;
    }
    ChordEstimate current() const noexcept {
        return (display_.valid() && completedBeats_-lastConfirmedBeat_<=4)?display_:ChordEstimate{};
    }
    int completedBeats() const noexcept {return completedBeats_;}
    int credibleChanges() const noexcept {return credibleChanges_;}
    int credibleCadences() const noexcept {return credibleCadences_;}
    float downbeatConfidence() const noexcept {return downbeatEvidence_;}
    int downbeatPhase() const noexcept {return downbeatIndex_;}
    float tonicBonus(int keyRoot) const noexcept {
        if(keyRoot<0 || keyRoot>=12 || completedBeats_<24 || credibleChanges_<2 || credibleCadences_<2
           || downbeatIndex_<0 || downbeatEvidence_<0.28f)return 0.f;
        const float mass=std::accumulate(tonicVotes_.begin(),tonicVotes_.end(),0.f);
        if(mass<0.2f)return 0.f;
        return std::clamp(0.018f*(tonicVotes_[static_cast<std::size_t>(keyRoot)]/mass-1.f/12.f),
                          -0.003f,0.015f);
    }
private:
    void resetAccumulator() noexcept{tonal_.fill(0.f);bassVotes_.fill(0.f);frames_=0;}
    void finishBeat() noexcept {
        ++completedBeats_;
        if(frames_<2) {resetAccumulator();return;}
        const float sum=std::accumulate(tonal_.begin(),tonal_.end(),0.f);
        if(sum<1.e-6f){resetAccumulator();return;}
        std::array<float,12> normalized{};
        for(int i=0;i<12;++i)normalized[i]=tonal_[i]/sum;
        int bass=-1;float bassMass=std::accumulate(bassVotes_.begin(),bassVotes_.end(),0.f);
        if(bassMass>0.01f){
            bass=static_cast<int>(std::max_element(bassVotes_.begin(),bassVotes_.end())-bassVotes_.begin());
        }
        const float bassConf=bass>=0?bassVotes_[static_cast<std::size_t>(bass)]/bassMass:0.f;
        auto chord=transcribeChord(normalized,bass,bassConf);
        if(!chord.valid()) {
            lastCandidate_={};candidateRun_=0;
            if(completedBeats_-lastConfirmedBeat_>4) display_={};
            resetAccumulator();return;
        }
        if(chord.root==lastCandidate_.root && chord.quality==lastCandidate_.quality)++candidateRun_;
        else {lastCandidate_=chord;candidateRun_=1;}
        if(candidateRun_>=2) {
            // Separate a new chord after a long unreliable passage from an
            // actual continuous harmonic transition (no phantom cadences).
            if(completedBeats_-lastConfirmedBeat_>4) display_={};
            if(!display_.valid() || chord.root!=display_.root || chord.quality!=display_.quality) {
                if(display_.valid()) {
                    ++credibleChanges_;
                    const bool isDominant=display_.root==(chord.root+7)%12 &&
                            (display_.quality==static_cast<int>(ChordQuality::Major) ||
                             display_.quality==static_cast<int>(ChordQuality::Dominant7));
                    const bool isSubdominant=display_.root==(chord.root+5)%12 &&
                            display_.quality==static_cast<int>(ChordQuality::Major);
                    const bool tonicMajor=chord.quality==static_cast<int>(ChordQuality::Major) ||
                                          chord.quality==static_cast<int>(ChordQuality::Major7);
                    const bool barBoundary=downbeatIndex_>=0 && activeBeat_%4==downbeatIndex_;
                    if(tonicMajor && (isDominant||isSubdominant) && bass==chord.root &&
                       bassConf>=0.58f && barBoundary && downbeatEvidence_>=0.28f &&
                       chord.evidence>=0.28f) {
                        ++credibleCadences_;
                        tonicVotes_[static_cast<std::size_t>(chord.root)]+=chord.evidence*downbeatEvidence_;
                    }
                }
            }
            display_=chord;
            lastConfirmedBeat_=completedBeats_;
        }
        resetAccumulator();
    }
    double period_=-1.0,anchor_=0.0;
    float phaseEvidence_=0.f,downbeatEvidence_=0.f;
    int downbeatIndex_=-1,activeBeat_=-1,frames_=0,completedBeats_=0;
    int candidateRun_=0,credibleChanges_=0,credibleCadences_=0,lastConfirmedBeat_=-100;
    std::array<float,12> tonal_{},bassVotes_{},tonicVotes_{};
    ChordEstimate lastCandidate_{},display_{};
};
} // namespace skell
