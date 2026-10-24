#pragma once
#include <algorithm>
#include <cmath>

namespace skell {

// All values are evidence measurements, never track-name or host-tempo priors.
struct TempoEvidenceGate {
    float lastCandidateBpm = 0.0f;
    int consecutiveWindows = 0;

    void reset() noexcept { lastCandidateBpm = 0.0f; consecutiveWindows = 0; }

    // A weak autocorrelation result can become usable only if an independent
    // phase-folded onset measurement supports it over multiple updates.
    bool observeWeak(float bpm, float correlation, float phaseStrength,
                     float availableSeconds) noexcept {
        const bool plausible = std::isfinite(bpm) && bpm >= 40.0f && bpm <= 240.0f;
        const bool enoughEvidence = availableSeconds >= 14.0f
            && correlation >= 0.105f && phaseStrength >= 0.48f;
        if (!plausible || !enoughEvidence) { reset(); return false; }
        const bool matches = lastCandidateBpm > 0.0f
            && std::abs(std::log2(bpm / lastCandidateBpm)) <= 0.035f;
        consecutiveWindows = matches ? std::min(10, consecutiveWindows + 1) : 1;
        lastCandidateBpm = bpm;
        return consecutiveWindows >= 3;
    }

    // Confidence of the low-evidence path remains deliberately capped: it is
    // not the same quality of observation as a strong onset autocorrelation.
    static float weakConfidence(float correlation, float phaseStrength) noexcept {
        if (!std::isfinite(correlation) || !std::isfinite(phaseStrength)) return 0.0f;
        const float c = std::clamp((correlation - 0.10f) / 0.15f, 0.0f, 1.0f);
        const float p = std::clamp((phaseStrength - 0.43f) / 0.45f, 0.0f, 1.0f);
        return std::min(0.20f, 0.27f * c * p);
    }

    static bool preferThreeTwoFast(float slowCorr, float fastCorr,
                                   float slowPhase, float fastPhase) noexcept {
        if (!std::isfinite(slowCorr) || !std::isfinite(fastCorr)
            || !std::isfinite(slowPhase) || !std::isfinite(fastPhase)) return false;
        return fastCorr > 0.105f
            && fastCorr > slowCorr * 0.66f
            && fastPhase > 0.46f
            && fastPhase > slowPhase * 1.17f
            && fastPhase > slowPhase + 0.14f;
    }
};

} // namespace skell
