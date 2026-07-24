#pragma once

#include <cstdint>

namespace oracle::presentation {

// A render frame may sample between two completed deterministic logic ticks.
// The renderer can therefore run at 120 Hz or faster without changing rules.
class FrameTiming {
public:
    FrameTiming(
        std::uint64_t previous_tick,
        std::uint64_t current_tick,
        double interpolation_alpha);

    [[nodiscard]] std::uint64_t previous_tick() const noexcept;
    [[nodiscard]] std::uint64_t current_tick() const noexcept;
    [[nodiscard]] double interpolation_alpha() const noexcept;
    [[nodiscard]] double interpolate(double previous, double current) const
        noexcept;

private:
    std::uint64_t previous_tick_;
    std::uint64_t current_tick_;
    double interpolation_alpha_;
};

}  // namespace oracle::presentation
