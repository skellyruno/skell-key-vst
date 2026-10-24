#pragma once
// Shared, sample-rate-independent key-history timeout.
// A break within a musical phrase should not discard established tonal context.
// Longer silence clears history once so a different song starts with clean evidence.
#include <algorithm>
#include <cmath>

namespace skell {
inline int keySilenceResetFrames(double sampleRate, int hopSamples) noexcept {
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 || hopSamples <= 0)
        return 4;
    return std::max(4, static_cast<int>(std::ceil(2.5 * sampleRate / hopSamples)));
}
}
