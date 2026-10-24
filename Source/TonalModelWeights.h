#pragma once
// V47 default: no independently trained and validated model has been installed.
// DO NOT set certified=true manually. Export a verified model with the trainer.
#include <array>
namespace skell {
struct TonalModelWeights {
    static constexpr bool certified=false;
    static constexpr float temperature=1.0f;
    inline static constexpr std::array<std::array<float,24>,2> weights{};
    inline static constexpr std::array<float,2> bias{};
};
}
