#pragma once

#include <cstdint>

namespace oracle::gameplay {

struct PlayerCombatState {
    // Retail health is stored in quarter-heart units.
    std::uint8_t maximum_health{12};
    std::uint8_t health{12};
    std::uint8_t invincibility_ticks{};
    std::uint16_t rupees{};
};

}  // namespace oracle::gameplay
