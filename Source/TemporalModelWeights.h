#pragma once
// Experimental temporal model: NOT trained on independently annotated audio.
// Never enable this model without a separately verified holdout report.
#include <array>
namespace skell {
struct TemporalModelWeights {
    static constexpr bool certified=false;
    static constexpr float temperature=1.0f;
    inline static constexpr std::array<std::array<float,48>,2> weights{};
    inline static constexpr std::array<float,2> bias{};
};
}
