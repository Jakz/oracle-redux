#pragma once

#include <cstdint>

namespace oracle::core {

struct ItemState {
    std::uint8_t id{};
    std::uint8_t state{};
    std::uint8_t health{};
    std::uint8_t damage_to_apply{};
    std::uint8_t var2a{};
    std::uint8_t var3c{};
    std::uint8_t knockback_angle{};
    std::uint8_t knockback_counter{};
};

struct LinkState {
    std::uint8_t knockback_angle{};
    std::uint8_t knockback_counter{};
};

}  // namespace oracle::core
