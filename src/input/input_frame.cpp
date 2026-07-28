#include "oracle/input/input_frame.h"

namespace oracle::input {
namespace {

std::uint16_t action_mask(const InputAction action) noexcept {
    return static_cast<std::uint16_t>(
        1u << static_cast<std::uint8_t>(action));
}

}  // namespace

void SemanticInputSampler::set(
    const InputAction action,
    const bool active) noexcept {
    const auto bit = action_mask(action);
    const bool was_active = (held_ & bit) != 0;
    if (active == was_active) {
        return;
    }
    if (active) {
        held_ = static_cast<std::uint16_t>(held_ | bit);
        pending_pressed_ =
            static_cast<std::uint16_t>(pending_pressed_ | bit);
    } else {
        held_ = static_cast<std::uint16_t>(held_ & ~bit);
        pending_released_ =
            static_cast<std::uint16_t>(pending_released_ | bit);
    }
}

void SemanticInputSampler::release_all() noexcept {
    pending_released_ =
        static_cast<std::uint16_t>(pending_released_ | held_);
    held_ = 0;
}

InputFrame SemanticInputSampler::sample() noexcept {
    const InputFrame frame{
        held_,
        pending_pressed_,
        pending_released_,
    };
    pending_pressed_ = 0;
    pending_released_ = 0;
    return frame;
}

}  // namespace oracle::input
