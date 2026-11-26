#pragma once
// Portable, bounded, worker-owned long-term key evidence.
// Does not infer a cadence from repeating loops: it only flags agreement
// between a stable pitch-class profile, bass fundamentals and phrase endings.
// Never overrides an estimate when these sources disagree.
#include "KeyCandidateScore.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace skell {
class KeyTemporalConsensus {
public:
    struct Decision { int root=-1, mode=2; float longFit=0; int phraseHits=0; };
    void prepare(double rate, int hop) noexcept {
        const double fs=std::isfinite(rate)?std::clamp(rate,8000.,384000.):48000.;
        const double framesPerSecond=fs/std::max(hop,1);
        // Aggregate consecutive normalized observations before entering the
        // fixed ring. At 96/192k the old minimum could exceed its capacity.
        group_=std::max(1,static_cast<int>(std::ceil(40.*framesPerSecond/512.)));
        capacity_=std::clamp(static_cast<int>(std::lround(40.*framesPerSecond/group_)),1,512);
        minimum_=std::clamp(static_cast<int>(std::lround(24.*framesPerSecond/group_)),1,capacity_);
        reset();
    }
    void reset() noexcept { sums_.fill(0);pending_.fill(0);write_=count_=pendingCount_=0; }
    void observe(const std::array<float,12>& chroma) noexcept {
        float mass=0;
        for(float v:chroma)mass+=std::isfinite(v)?std::max(0.f,v):0.f;
        if(std::isfinite(mass)&&mass>=1.0e-12f){
            for(int i=0;i<12;++i)pending_[static_cast<std::size_t>(i)]+=std::max(0.f,std::isfinite(chroma[static_cast<std::size_t>(i)])?chroma[static_cast<std::size_t>(i)]:0.f)/mass;
        }else if(!std::isfinite(mass)){
            double wide=0.;for(float v:chroma)if(std::isfinite(v))wide+=std::max(0.f,v);
            for(int i=0;i<12;++i){float v=chroma[static_cast<std::size_t>(i)];if(std::isfinite(v)&&wide>0.)pending_[static_cast<std::size_t>(i)]+=static_cast<float>(std::max(0.f,v)/wide);}
        }
        if(++pendingCount_<group_)return;
        const std::size_t slot=static_cast<std::size_t>(write_);
        if(count_==capacity_)
            for(int i=0;i<12;++i)sums_[static_cast<std::size_t>(i)]-=frames_[slot][static_cast<std::size_t>(i)];
        else ++count_;
        for(int i=0;i<12;++i){const auto j=static_cast<std::size_t>(i);frames_[slot][j]=pending_[j]/static_cast<float>(group_);sums_[j]+=frames_[slot][j];}
        pending_.fill(0);pendingCount_=0;write_=(write_+1)%capacity_;
    }
    template<class PhraseTracker>
    Decision propose(int rawRoot,int rawMode,const PhraseTracker& phrases,
                     const float* major,const float* minor) const noexcept {
        if(count_<minimum_||rawRoot<0||rawRoot>11||(rawMode!=0&&rawMode!=1))return {};
        int topRoot=-1,topMode=2;float top=-2.f;
        for(int r=0;r<12;++r)for(int m=0;m<2;++m){
            const float score=keyProfileCorrelation(sums_,r,m==0?major:minor);
            if(score>top){top=score;topRoot=r;topMode=m;}
        }
        if(topRoot==rawRoot || top<0.60f)return {};
        const float current=keyProfileCorrelation(sums_,rawRoot,rawMode==0?major:minor);
        if(top-current<0.035f)return {};
        const int n=phrases.completedPhrases();
        const int first=std::max(0,n-16);
        int preferredCount=0,rawCount=0;
        for(int i=first;i<n;++i){
            const int root=phrases.phraseRootAt(i);
            if(root==topRoot)++preferredCount;
            if(root==rawRoot)++rawCount;
        }
        if(preferredCount<3 || preferredCount<rawCount+2)return {};
        const auto& bass=phrases.bassHistogram();
        if(bass[static_cast<std::size_t>(topRoot)]<15.0f ||
            bass[static_cast<std::size_t>(topRoot)] <
                0.75f*bass[static_cast<std::size_t>(rawRoot)])return {};
        // The phrase roots distinguish tonic candidates, but not chord quality.
        // Mode still comes entirely from the independently measured chroma fit.
        return {topRoot,topMode,top,preferredCount};
    }
    // Number of completed stored groups, identical to the old count when group=1.
    int observedFrames() const noexcept { return count_; }
private:
    std::array<std::array<float,12>,512> frames_{};
    std::array<float,12> sums_{},pending_{};
    int capacity_=512,minimum_=270,write_=0,count_=0,group_=1,pendingCount_=0;
};
}
