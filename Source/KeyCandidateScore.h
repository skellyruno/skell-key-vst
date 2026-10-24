#pragma once
// Shared candidate-ranking mathematics for both the JUCE processor and the
// standalone C++ audio benchmark. Avoids drift between measured and shipped DSP.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace skell {
inline float keyProfileCorrelation(const std::array<float,12>& chroma,
                                   int root, const float* profile) noexcept {
    float avgC=0.f, avgP=0.f;
    for(int i=0;i<12;++i){avgC+=chroma[static_cast<std::size_t>((i+root)%12)];avgP+=profile[i];}
    avgC/=12.f;avgP/=12.f;
    float ab=0.f, aa=0.f, bb=0.f;
    for(int i=0;i<12;++i){
        const float a=chroma[static_cast<std::size_t>((i+root)%12)]-avgC;
        const float b=profile[i]-avgP;
        ab+=a*b;aa+=a*a;bb+=b*b;
    }
    return ab/std::sqrt(std::max(1.0e-12f,aa*bb));
}

template<class Prediction>
inline float keyCandidateScore(const std::array<float,12>& chroma, int root, int mode,
                               const float* profile,
                               const std::array<float,12>& bassEvidence,
                               const std::array<float,12>& progressionBonus,
                               float phraseBonus, float beatBonus,
                               float downbeatBonus, float relativeModeBonus,
                               const Prediction& learned) noexcept {
    float score=keyProfileCorrelation(chroma,root,profile)
        +bassEvidence[static_cast<std::size_t>(root)]
        +progressionBonus[static_cast<std::size_t>(root)]
        +phraseBonus+beatBonus+downbeatBonus+relativeModeBonus;
    // The feature model is disabled unless its weights have been certified.
    if(learned.root>=0 && learned.probability>0.55f && learned.margin>0.15f)
        score+=0.25f*learned.posteriors[static_cast<std::size_t>(2*root+mode)];
    return score;
}
}
