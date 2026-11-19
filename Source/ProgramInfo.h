#pragma once

namespace skellkey::host {
inline constexpr int programCount = 1;

inline constexpr const char* programName(int index) noexcept {
    return index == 0 ? "Default" : "";
}
} // namespace skellkey::host
