#pragma once

#include <cstdint>

#include "oracle/core/item_state.h"

namespace oracle::core {

struct ItemDamageResult {
    std::uint8_t var2a{};
    bool zero{};
    bool health_negative{};
};

class ItemRuntime {
public:
    static void set_var3c_to_ff(ItemState& item) noexcept;

    [[nodiscard]] static ItemDamageResult update_damage_to_apply(
        ItemState& item) noexcept;

    [[nodiscard]] static bool transfer_knockback_to_link(
        ItemState& item,
        LinkState& link) noexcept;
};

}  // namespace oracle::core
