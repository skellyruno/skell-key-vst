#pragma once
#include <cmath>

namespace skell {
// Called after an existing tempo scorer has selected a plausible candidate.
// Holding the fast pulse is permitted only while independent autocorrelation
// and phase-folded onset evidence still support it. This is not a BPM prior.
inline bool holdSupportedDoubleTime(float previouslyStableBpm,
                                    float newCandidateBpm,
                                    float previousCorrelation,
                                    float candidateCorrelation,
                                    float previousPhaseStrength,
                                    float candidatePhaseStrength) noexcept {
    if (!std::isfinite(previouslyStableBpm) || !std::isfinite(newCandidateBpm) ||
        !std::isfinite(previousCorrelation) || !std::isfinite(candidateCorrelation) ||
        !std::isfinite(previousPhaseStrength) || !std::isfinite(candidatePhaseStrength) ||
        previouslyStableBpm <= 0.0f || newCandidateBpm <= 0.0f) return false;
    const float ratio = previouslyStableBpm / newCandidateBpm;
    return ratio > 1.90f && ratio < 2.10f &&
           previousCorrelation > 0.22f &&
           previousCorrelation > 0.55f * candidateCorrelation &&
           previousPhaseStrength > 0.63f * candidatePhaseStrength;
}
} // namespace skell
