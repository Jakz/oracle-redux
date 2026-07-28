#pragma once

#include <cstdint>

namespace oracle::input {

enum class InputAction : std::uint8_t {
    up,
    down,
    left,
    right,
    a,
    b,
    start,
    select,
    confirm,
    cancel,
    count,
};

class InputFrame {
public:
    constexpr InputFrame() = default;
    constexpr InputFrame(
        const std::uint16_t held,
        const std::uint16_t pressed,
        const std::uint16_t released) noexcept
        : held_{held}, pressed_{pressed}, released_{released} {}

    [[nodiscard]] constexpr bool held(InputAction action) const noexcept {
        return (held_ & mask(action)) != 0;
    }

    [[nodiscard]] constexpr bool pressed(InputAction action) const noexcept {
        return (pressed_ & mask(action)) != 0;
    }

    [[nodiscard]] constexpr bool released(InputAction action) const noexcept {
        return (released_ & mask(action)) != 0;
    }

    [[nodiscard]] constexpr std::uint16_t held_bits() const noexcept {
        return held_;
    }

    [[nodiscard]] constexpr std::uint16_t pressed_bits() const noexcept {
        return pressed_;
    }

    [[nodiscard]] constexpr std::uint16_t released_bits() const noexcept {
        return released_;
    }

private:
    [[nodiscard]] static constexpr std::uint16_t mask(
        const InputAction action) noexcept {
        return static_cast<std::uint16_t>(
            1u << static_cast<std::uint8_t>(action));
    }

    std::uint16_t held_{};
    std::uint16_t pressed_{};
    std::uint16_t released_{};
};

// Platform adapters feed physical-state changes into this sampler. Exactly one
// frame is consumed at each authoritative logic boundary. Edge bits remain
// pending until sampled, so a short press/release between ticks is not lost.
class SemanticInputSampler {
public:
    void set(InputAction action, bool active) noexcept;
    void release_all() noexcept;
    [[nodiscard]] InputFrame sample() noexcept;

private:
    std::uint16_t held_{};
    std::uint16_t pending_pressed_{};
    std::uint16_t pending_released_{};
};

}  // namespace oracle::input
