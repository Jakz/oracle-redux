#include "oracle/presentation/frame_timing.h"

#include <cmath>
#include <stdexcept>

namespace oracle::presentation {

FrameTiming::FrameTiming(
    const std::uint64_t previous_tick,
    const std::uint64_t current_tick,
    const double interpolation_alpha)
    : previous_tick_{previous_tick},
      current_tick_{current_tick},
      interpolation_alpha_{interpolation_alpha} {
    if (current_tick_ < previous_tick_) {
        throw std::invalid_argument{
            "current render tick cannot precede previous render tick"};
    }
    if (
        !std::isfinite(interpolation_alpha_) ||
        interpolation_alpha_ < 0.0 ||
        interpolation_alpha_ > 1.0) {
        throw std::invalid_argument{
            "render interpolation alpha must be finite and between 0 and 1"};
    }
}

std::uint64_t FrameTiming::previous_tick() const noexcept {
    return previous_tick_;
}

std::uint64_t FrameTiming::current_tick() const noexcept {
    return current_tick_;
}

double FrameTiming::interpolation_alpha() const noexcept {
    return interpolation_alpha_;
}

double FrameTiming::interpolate(
    const double previous,
    const double current) const noexcept {
    return previous + ((current - previous) * interpolation_alpha_);
}

}  // namespace oracle::presentation
