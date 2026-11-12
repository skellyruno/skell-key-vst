#pragma once
// Worker-thread tonal analysis window: remove constant DC bias before
// spectral analysis and use AC-only RMS for the key silence gate.
// This never changes the audible signal, tempo detector, or MIDI output.
#include <array>
#include <cmath>
#include <cstddef>

namespace skell {
template <std::size_t N>
inline float prepareTonalWindow(const std::array<float,N>& history,
                                std::size_t write,
                                const std::array<float,N>& hann,
                                float* fftInput) noexcept {
    static_assert(N > 0 && (N & (N-1)) == 0, "Tonal history must be a power of two");
    // The input samples have already been sanitized by analyseAudioSample.
    double sum = 0.0;
    for (std::size_t i = 0; i < N; ++i)
        sum += history[(write + i) & (N-1)];
    const double dc = sum / static_cast<double>(N);
    double acEnergy = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        const double ac = static_cast<double>(history[(write+i) & (N-1)]) - dc;
        acEnergy += ac * ac;
        fftInput[i] = static_cast<float>(ac * static_cast<double>(hann[i]));
    }
    return static_cast<float>(std::sqrt(acEnergy / static_cast<double>(N)));
}
} // namespace skell
