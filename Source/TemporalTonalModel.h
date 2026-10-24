#pragma once
// Experimental 48-feature model output is strictly disabled until certified.
#include "TemporalModelWeights.h"
#include <array>
#include <algorithm>
#include <cmath>
namespace skell {
struct TemporalTonalPrediction {
    int root=-1, mode=2;
    float probability=0,margin=0;
    std::array<float,24> posteriors{};
};
inline TemporalTonalPrediction predictTemporalTonalCenter(const std::array<float,48>& f) noexcept {
    TemporalTonalPrediction result;
    if(!TemporalModelWeights::certified)return result;
    std::array<float,24> scores{};
    float top=-1.0e30f;
    for(int root=0;root<12;++root)for(int mode=0;mode<2;++mode){
        float s=TemporalModelWeights::bias[(size_t)mode];
        for(int channel=0;channel<4;++channel)for(int pc=0;pc<12;++pc){
            const int idx=12*channel+pc;
            s+=TemporalModelWeights::weights[(size_t)mode][(size_t)idx]*f[(size_t)(12*channel+(root+pc)%12)];
        }
        scores[(size_t)(root*2+mode)]=s/std::max(0.1f,TemporalModelWeights::temperature);
        top=std::max(top,scores[(size_t)(root*2+mode)]);
    }
    float denom=0;
    for(int i=0;i<24;++i){auto& p=result.posteriors[(size_t)i];p=std::exp(scores[(size_t)i]-top);denom+=p;}
    if(!(denom>0.0f))return result;
    float first=-1,second=-1;int best=-1;
    for(int i=0;i<24;++i){
        float& p=result.posteriors[(size_t)i];p/=denom;
        if(p>first){second=first;first=p;best=i;}
        else if(p>second)second=p;
    }
    result.root=best/2;result.mode=best%2;result.probability=first;result.margin=first-second;
    return result;
}
}
