#pragma once
#include <algorithm>
#include <cmath>

namespace skell {

// A bounded-cost stereo analysis policy. This does not alter the host audio.
// Ordinary stereo remains (L+R)/2; only strongly anti-correlated channels
// whose *combined* energy largely cancels choose one original channel.
struct StereoAnalysisMix {
    enum class Mode { mono, average, left, right };
    Mode mode = Mode::mono;
    float correlation = 1.0f;
    float cancellation = 1.0f;

    static StereoAnalysisMix decide(const float* left, const float* right, int count) noexcept {
        StereoAnalysisMix result;
        if (left == nullptr || count <= 0) return result;
        if (right == nullptr) return result;
        result.mode = Mode::average;
        double ll = 0.0, rr = 0.0, lr = 0.0, mixEnergy = 0.0;
        // For a typical 64-4096 sample callback this costs ~5 operations / sample.
        // Use double sums to protect the confidence decision from cancellation.
        for (int i = 0; i < count; ++i) {
            const double a = left[i], b = right[i];
            if (!std::isfinite(a) || !std::isfinite(b)) return result;
            ll += a*a; rr += b*b; lr += a*b;
            const double mid = 0.5 * (a+b);
            mixEnergy += mid*mid;
        }
        const double power = ll+rr;
        if (count < 32 || power < 1.0e-14) return result;
        result.correlation = static_cast<float>(lr / std::sqrt(std::max(1.0e-24, ll*rr)));
        // 0: perfect cancellation, 1: equal-power uncorrelated channels.
        // For identical mono channels this is 2; that's harmless.
        result.cancellation = static_cast<float>(4.0*mixEnergy / power);
        if (result.correlation < -0.85f && result.cancellation < 0.15f)
            result.mode = (rr > ll*1.05) ? Mode::right : Mode::left;
        return result;
    }
    float sample(float left, float right) const noexcept {
        switch (mode) {
            case Mode::left: return left;
            case Mode::right: return right;
            case Mode::average: return 0.5f*(left+right);
            case Mode::mono: return left;
        }
        return left;
    }
};
}
