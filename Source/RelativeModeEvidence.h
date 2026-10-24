#pragma once
// Portable C++17, worker-thread-only relative-key tonic evidence.
// Never uses filename, expected BPM/key, or a major/minor preference.
// An observed bass pitch class must agree with a harmonically credible tonic
// triad. Votes are time-gated and saturated so repeated FFT frames and loops
// cannot overwhelm strong long-term key-profile evidence.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace skell {
class RelativeModeEvidence {
public:
    void reset() noexcept { *this = RelativeModeEvidence{}; }

    void observe(const std::array<float,12>& chroma, int bassPc,
                 float bassConfidence, double seconds,
                 int confirmedRoot=-1,int confirmedKind=-1,
                 float confirmedConfidence=0.0f) noexcept {
        // Register transitions in the independent chord detector. These are
        // separate observations from the bass/chroma template used below.
        if(confirmedRoot>=0 && confirmedRoot<12 && confirmedKind>=0 && confirmedKind<=1
           && confirmedConfidence>=0.20f) {
            const int chordId=confirmedRoot*2+confirmedKind;
            if(chordId!=lastConfirmedChord_){
                lastConfirmedChord_=chordId;
                ++distinctChordChanges_;
                if(confirmedRoot==bassPc && bassConfidence>=0.48f) {
                    const auto idx=static_cast<std::size_t>(chordId);
                    if(firstConfirmed_<0.)firstConfirmed_=seconds;
                    lastConfirmed_=seconds;
                    confirmedVotes_[idx]=std::min(8.f, confirmedVotes_[idx]+
                        std::min(1.f,confirmedConfidence));
                    ++confirmedEvents_;
                }
            }
        }
        if (bassPc < 0 || bassPc >= 12 || bassConfidence < 0.48f ||
            !std::isfinite(seconds) || seconds < 0.0) return;
        const float mass = std::max(0.0f, *std::max_element(chroma.begin(), chroma.end()));
        if (mass <= 1.0e-7f) return;
        float total=0.0f;
        for(float v : chroma) total += std::max(0.0f, std::isfinite(v) ? v : 0.0f);
        if (total < 1.0e-7f) return;
        const int root=bassPc;
        const float tonic=std::max(0.f,chroma[static_cast<std::size_t>(root)])/total;
        const float fifth=std::max(0.f,chroma[static_cast<std::size_t>((root+7)%12)])/total;
        const float majorThird=std::max(0.f,chroma[static_cast<std::size_t>((root+4)%12)])/total;
        const float minorThird=std::max(0.f,chroma[static_cast<std::size_t>((root+3)%12)])/total;
        // A clearly pitched bass with the tonic, fifth, and distinguishing
        // third simultaneously present is stronger evidence than a histogram
        // of isolated notes. Do not vote on ambiguous thirds.
        if(tonic<0.080f || fifth<0.075f) return;
        const float distinction=std::abs(majorThird-minorThird);
        if(distinction<0.035f) return;
        const int mode=majorThird>minorThird ? 0 : 1;
        const float third=std::max(majorThird,minorThird);
        if(third<0.085f || tonic+fifth+third<0.36f) return;
        const std::size_t index=static_cast<std::size_t>(root*2+mode);
        // Independent observations are at least 1.25 s apart. A bass ostinato
        // may still contribute, but evidence accumulates with diminishing gain.
        if (seconds-lastVote_[index]<1.25) return;
        lastVote_[index]=seconds;
        if (firstVote_<0.0) firstVote_=seconds;
        const float purity=std::clamp(distinction/0.15f,0.f,1.f);
        const float weight=std::min(1.f,bassConfidence)*purity;
        votes_[index] = std::min(8.f,0.985f*votes_[index]+weight);
        if(totalEvents_<100000) ++totalEvents_;
        lastEvent_=seconds;
    }

    // Small signed correction only between relative major/minor tonic pairs;
    // the strong spectral, beat, and phrase evidence remains authoritative.
    float bonus(int root,int mode) const noexcept {
        if(root<0||root>=12||(mode!=0&&mode!=1)||totalEvents_<4 ||
           firstVote_<0 || lastEvent_-firstVote_<8.0 ||
           confirmedEvents_<3 || distinctChordChanges_<4 ||
           firstConfirmed_<0.0 || lastConfirmed_-firstConfirmed_<8.0)return 0.0f;
        const int competitorRoot=(root+(mode==0?9:3))%12;
        const int competitorMode=1-mode;
        const float own=votes_[static_cast<std::size_t>(root*2+mode)];
        const float opposite=votes_[static_cast<std::size_t>(competitorRoot*2+competitorMode)];
        const float ownConfirmed=confirmedVotes_[static_cast<std::size_t>(root*2+mode)];
        const float otherConfirmed=confirmedVotes_[static_cast<std::size_t>(competitorRoot*2+competitorMode)];
        if(own+opposite<1.2f || ownConfirmed+otherConfirmed<0.5f)return 0.0f;
        const float confirmedDirection=(ownConfirmed-otherConfirmed)/
            (ownConfirmed+otherConfirmed+1.f);
        const float melodicDirection=(own-opposite)/(own+opposite+2.f);
        return std::clamp(0.045f*(0.7f*confirmedDirection+0.3f*melodicDirection),
                          -0.032f,0.032f);
    }
    float support(int root,int mode) const noexcept {
        if(root<0||root>=12||(mode!=0&&mode!=1))return 0.0f;
        return votes_[static_cast<std::size_t>(root*2+mode)];
    }
    float confirmedSupport(int root,int mode) const noexcept {
        if(root<0||root>=12||(mode!=0&&mode!=1))return 0.f;
        return confirmedVotes_[static_cast<std::size_t>(root*2+mode)];
    }
    int eventCount() const noexcept {return totalEvents_;}
    int confirmedEventCount() const noexcept {return confirmedEvents_;}
private:
    std::array<float,24> votes_{};
    std::array<float,24> confirmedVotes_{};
    int lastConfirmedChord_=-1;
    int confirmedEvents_=0,distinctChordChanges_=0;
    double firstConfirmed_=-1.0,lastConfirmed_=-1.0;
    std::array<double,24> lastVote_= []{
        std::array<double,24> result{};
        result.fill(-1.e6);
        return result;
    }();
    int totalEvents_=0;
    double firstVote_=-1.0, lastEvent_=-1.0;
};

// Evidence consistency is not a calibrated probability. The UI must label it
// "key evidence" rather than claiming a % probability of musical correctness.
class KeyEvidenceConsistency {
public:
    void reset() noexcept { winners_.fill(-1);next_=count_=0; }
    void observe(int root,int mode) noexcept {
        winners_[static_cast<std::size_t>(next_)]=
            (root>=0&&root<12&&mode>=0&&mode<=1)?root*2+mode:-1;
        next_=(next_+1)%static_cast<int>(winners_.size());
        count_=std::min(static_cast<int>(winners_.size()),count_+1);
    }
    float agreement(int root,int mode) const noexcept {
        if(root<0||root>=12||mode<0||mode>1||count_<12)return 0.0f;
        const int candidate=root*2+mode;
        int wins=0,valid=0;
        for(int i=0;i<count_;++i){
            const int value=winners_[static_cast<std::size_t>(i)];
            if(value>=0){++valid;if(value==candidate)++wins;}
        }
        return valid>0?static_cast<float>(wins)/valid:0.f;
    }
    // Conservative evidence index, capped when a hypothesis is unstable.
    float displayEvidence(float spectralMargin,float profileFit,float consistency,
                          float tonicSupport) const noexcept {
        const float separation=std::clamp(spectralMargin/0.20f,0.f,1.f);
        const float fit=std::clamp((profileFit-0.25f)/0.55f,0.f,1.f);
        const float stability=std::clamp((consistency-0.33f)/0.67f,0.f,1.f);
        float value=separation*fit*(0.35f+0.65f*stability);
        // High confidence without any independent harmonic/tonic corroboration
        // is not well justified on dense mixes.
        const float cap=tonicSupport>=1.2f?0.82f:0.62f;
        return std::clamp(value,0.0f,cap);
    }
private:
    std::array<int,96> winners_{};
    int next_=0,count_=0;
};
}
