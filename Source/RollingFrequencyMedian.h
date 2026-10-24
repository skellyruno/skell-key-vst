#pragma once
#include <array>
#include <algorithm>
#include <cassert>

namespace skell {
// Exact median for a sliding 13-bin frequency neighbourhood. The analyser
// traverses FFT bins in order: updating a sorted window costs a handful of
// moves rather than making thousands of fresh nth_element calls per frame.
// Worker thread only; no heap allocations. Input spectra must be finite.
class RollingFrequencyMedian13 final {
public:
    void reset(const float* firstThirteen) noexcept {
        for (int i=0;i<13;++i) values_[static_cast<size_t>(i)]=firstThirteen[i];
        std::sort(values_.begin(),values_.end());
    }
    void advance(float outgoing, float incoming) noexcept {
        int removed=0;
        while (removed<13 && values_[static_cast<size_t>(removed)]<outgoing) ++removed;
        // A sliding window is built from the same input samples, so outgoing
        // must occur in the multiset (including repeated and zero bins).
        assert(removed<13 && values_[static_cast<size_t>(removed)]==outgoing);
        if (removed>=13) return;
        for (int i=removed;i<12;++i) values_[static_cast<size_t>(i)]=values_[static_cast<size_t>(i+1)];
        int slot=11;
        while (slot>=0 && values_[static_cast<size_t>(slot)]>incoming) {
            values_[static_cast<size_t>(slot+1)]=values_[static_cast<size_t>(slot)];
            --slot;
        }
        values_[static_cast<size_t>(slot+1)]=incoming;
    }
    float median() const noexcept {return values_[6];}
private:
    std::array<float,13> values_{};
};
}
