#pragma once
#include <algorithm>
#include <array>
#include <limits>

namespace skell {
// Exact upper median for 1..7 finite, non-negative spectral magnitudes.
// Sort an eight-lane stack array using a fixed, branch-light comparison network;
// +inf padding sorts AFTER all finite inputs, preserving std::nth_element(n/2).
// Input/outputs are untouched by floating-point arithmetic (only comparisons
// and swaps), so equal input values remain bit-identical.
inline float temporalMedian7(const float* samples, int count) noexcept {
    if (count <= 0 || count > 7) return 0.0f;
    std::array<float,8> x{};
    for (int i=0; i<8; ++i)
        x[static_cast<size_t>(i)] = i<count ? samples[i] : std::numeric_limits<float>::infinity();
    auto swapIfNeeded=[&x](int a, int b) noexcept {
        if(x[static_cast<size_t>(a)]>x[static_cast<size_t>(b)])
            std::swap(x[static_cast<size_t>(a)], x[static_cast<size_t>(b)]);
    };
    swapIfNeeded(0,1); swapIfNeeded(2,3); swapIfNeeded(4,5); swapIfNeeded(6,7);
    swapIfNeeded(0,2); swapIfNeeded(1,3); swapIfNeeded(4,6); swapIfNeeded(5,7);
    swapIfNeeded(1,2); swapIfNeeded(5,6); swapIfNeeded(0,4); swapIfNeeded(3,7);
    swapIfNeeded(1,5); swapIfNeeded(2,6); swapIfNeeded(1,4); swapIfNeeded(3,6);
    swapIfNeeded(2,4); swapIfNeeded(3,5); swapIfNeeded(3,4);
    return x[static_cast<size_t>(count/2)];
}
} // namespace skell
