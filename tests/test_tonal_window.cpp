// Portable JUCE-free regression tests for constant DC and DC-biased music.
#include "../Source/TonalWindow.h"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>

int main() {
    constexpr std::size_t N = 16384;
    constexpr double pi = 3.1415926535897932384626433832795;
    constexpr double hz = 220.0, rate = 48000.0;
    std::array<float,N> history{}, hann{}, clean{}, dcMixed{}, baseline{};
    for (std::size_t i = 0; i < N; ++i)
        hann[i] = static_cast<float>(0.5 - 0.5*std::cos(2.0*pi*i/(N-1)));
    const std::size_t write = 397;
    for (std::size_t i = 0; i < N; ++i) {
        const auto phase = static_cast<double>(i) * hz / rate;
        const auto sample = static_cast<float>(0.10 * std::sin(2.0*pi*phase));
        history[(write+i)&(N-1)] = sample;
        clean[(write+i)&(N-1)] = sample;
    }
    const float expected = skell::prepareTonalWindow(clean,write,hann,baseline.data());
    assert(expected > 0.065f && expected < 0.075f);

    // DC-only audio must never fabricate a tonal key, regardless of its level.
    history.fill(0.55f);
    const float dcRms = skell::prepareTonalWindow(history,write,hann,dcMixed.data());
    assert(dcRms < 1.0e-6f);
    for (float sample : dcMixed) assert(std::abs(sample) < 1.0e-6f);

    // A musical note with even large DC bias must yield the SAME key spectrum.
    for (std::size_t i = 0; i < N; ++i)
        history[i] = clean[i] + 0.55f;
    const float acRms = skell::prepareTonalWindow(history,write,hann,dcMixed.data());
    assert(std::abs(acRms - expected) < 1.0e-6f);
    for (std::size_t i = 0; i < N; ++i)
        assert(std::abs(dcMixed[i] - baseline[i]) < 1.0e-6f);

    // Near-DC with tiny AC stays below the configurable key-silence threshold.
    for (std::size_t i = 0; i < N; ++i)
        history[i] = 0.8f + 0.000001f*clean[i];
    const float tiny = skell::prepareTonalWindow(history,write,hann,dcMixed.data());
    assert(tiny < 0.00001f);
    std::puts("PASS: tonal AC gate rejects DC and preserves DC-biased music spectra");
}
