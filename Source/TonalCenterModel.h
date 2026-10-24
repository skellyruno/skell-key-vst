#pragma once
// V47: portable, deterministic feature extraction and 24-class inference.
// The untrained default weights live in TonalModelWeights.h. A separately
// validated training run may export replacement weights.
#include "TonalModelWeights.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace skell {
struct TonalCenterFeatures {
    std::array<float, 12> chroma{};
    std::array<float, 12> bass{};
    int frames = 0;
    void reset() noexcept { chroma.fill(0); bass.fill(0); frames=0; }
    void observe(const std::array<float,12>& pitchClass, int bassPc, float bassConfidence) noexcept {
        float mass = 0;
        for (const float v : pitchClass) mass += std::max(0.0f,v);
        if (mass < 1.0e-8f) return;
        for (int k=0; k<12; ++k) chroma[(size_t)k] += std::max(0.0f,pitchClass[(size_t)k]) / mass;
        if (bassPc>=0 && bassPc<12 && bassConfidence>=0.12f)
            bass[(size_t)bassPc] += std::min(1.0f,bassConfidence);
        ++frames;
    }
    std::array<float,24> values() const noexcept {
        std::array<float,24> out{};
        float cs=0,bs=0;
        for(int k=0;k<12;++k){cs+=chroma[(size_t)k];bs+=bass[(size_t)k];}
        for(int k=0;k<12;++k){
            out[(size_t)k]=cs>0?chroma[(size_t)k]/cs:0.0f;
            out[(size_t)(12+k)]=bs>0?bass[(size_t)k]/bs:0.0f;
        }
        return out;
    }
};
struct TonalPrediction {
    int root=-1, mode=2;
    float probability=0, margin=0;
    std::array<float,24> posteriors{};
};
inline TonalPrediction predictTonalCenter(const std::array<float,24>& f) noexcept {
    TonalPrediction prediction;
    // Unvalidated weights are NEVER allowed to affect audio analysis.
    if (!TonalModelWeights::certified) return prediction;
    float logits[24]{};float highest=-1.0e30f;
    for(int root=0;root<12;++root)for(int mode=0;mode<2;++mode){
        float s=TonalModelWeights::bias[(size_t)mode];
        for(int pc=0;pc<12;++pc){
            const int shifted=(root+pc)%12;
            s+=TonalModelWeights::weights[(size_t)mode][(size_t)pc]*f[(size_t)shifted];
            s+=TonalModelWeights::weights[(size_t)mode][(size_t)(12+pc)]*f[(size_t)(12+shifted)];
        }
        const int index=2*root+mode;
        logits[index]=s / std::max(0.1f,TonalModelWeights::temperature);
        highest=std::max(highest,logits[index]);
    }
    float total=0;
    for(int i=0;i<24;++i){
        prediction.posteriors[(size_t)i]=std::exp(logits[i]-highest);
        total+=prediction.posteriors[(size_t)i];
    }
    int best=-1;float first=-1,second=-1;
    if(total<=0)return prediction;
    for(int i=0;i<24;++i){
        auto& p=prediction.posteriors[(size_t)i];p/=total;
        if(p>first){second=first;first=p;best=i;} else if(p>second) second=p;
    }
    prediction.root=best/2;prediction.mode=best%2;
    prediction.probability=first;prediction.margin=first-second;
    return prediction;
}
} // namespace skell
