#pragma once
// V49 experimental, worker-owned, allocation-free *temporal* feature extraction.
// The first 24 features match V48 smoothed-chroma/bass histogram values.
// [24..35] measure positive chroma arrivals; [36..47] measure distinct,
// two-frame-confirmed bass arrivals, NOT total bass duration.
// Feature extraction cannot affect the production key/BPM decision.
#include "TonalCenterModel.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace skell {
struct TemporalTonalFeatures {
    TonalCenterFeatures global;
    std::array<float,12> chromaArrivals{}, bassArrivals{}, previous{};
    int previousBass = -1, pendingBass = -1, pendingCount = 0, missingBass = 0;
    bool hasPrevious = false;

    void reset() noexcept {
        global.reset(); chromaArrivals.fill(0); bassArrivals.fill(0);
        previous.fill(0); hasPrevious=false;
        previousBass=pendingBass=-1; pendingCount=missingBass=0;
    }

    void observe(const std::array<float,12>& smoothedChroma,
                 const std::array<float,12>& chroma,
                 int bassPc, float bassConfidence) noexcept {
        float mass=0;
        std::array<float,12> normalized{};
        for(int i=0;i<12;++i){
            const float v=std::isfinite(chroma[(size_t)i])?std::max(0.0f,chroma[(size_t)i]):0.0f;
            normalized[(size_t)i]=v; mass+=v;
        }
        if(!(mass>1.0e-8f))return;
        for(float& v:normalized)v/=mass;
        global.observe(smoothedChroma,bassPc,bassConfidence);
        if(hasPrevious){
            // Noise floor avoids treating infinitesimal spectral jitter as a new note.
            for(int pc=0;pc<12;++pc){
                const float change=normalized[(size_t)pc]-previous[(size_t)pc]-0.015f;
                if(change>0)chromaArrivals[(size_t)pc]+=change;
            }
        }
        previous=normalized; hasPrevious=true;
        // A bass note is an arrival only after two consecutive confident frames.
        // Repeated frames of the same note do not accumulate unlimited evidence.
        const bool trustworthy=bassPc>=0 && bassPc<12 && std::isfinite(bassConfidence)
                              && bassConfidence>=0.22f;
        if(!trustworthy){
            pendingBass=-1;pendingCount=0;
            if(++missingBass>=3) previousBass=-1;
            return;
        }
        missingBass=0;
        if(bassPc!=pendingBass){pendingBass=bassPc;pendingCount=1;}
        else pendingCount=std::min(3,pendingCount+1);
        if(pendingCount==2 && pendingBass!=previousBass){
            bassArrivals[(size_t)pendingBass]+=std::min(1.0f,bassConfidence);
            previousBass=pendingBass;
        }
    }

    // Convenience overload used by isolated tests. The plugin uses smoothed
    // chroma for the legacy 24 channels and unsmoothed chroma for arrivals.
    void observe(const std::array<float,12>& chroma,
                 int bassPc, float bassConfidence) noexcept {
        observe(chroma,chroma,bassPc,bassConfidence);
    }

    std::array<float,48> values() const noexcept {
        std::array<float,48> result{};
        const auto base=global.values();
        for(int i=0;i<24;++i) result[(size_t)i]=base[(size_t)i];
        float chromaTotal=0,bassTotal=0;
        for(int pc=0;pc<12;++pc){chromaTotal+=chromaArrivals[(size_t)pc];bassTotal+=bassArrivals[(size_t)pc];}
        for(int pc=0;pc<12;++pc){
            result[(size_t)(24+pc)]=chromaTotal>0?chromaArrivals[(size_t)pc]/chromaTotal:0.0f;
            result[(size_t)(36+pc)]=bassTotal>0?bassArrivals[(size_t)pc]/bassTotal:0.0f;
        }
        return result;
    }
};
} // namespace skell
