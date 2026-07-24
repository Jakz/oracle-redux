#include "oracle/core/item_runtime.h"

namespace oracle::core {

void ItemRuntime::set_var3c_to_ff(ItemState& item) noexcept {
    item.var3c = 0xff;
}

ItemDamageResult ItemRuntime::update_damage_to_apply(
    ItemState& item) noexcept {
    const auto damage = item.damage_to_apply;
    item.damage_to_apply = 0;
    item.health = static_cast<std::uint8_t>(item.health + damage);

    return ItemDamageResult{
        .var2a = item.var2a,
        .zero = item.var2a == 0,
        .health_negative = (item.health & 0x80U) != 0,
    };
}

bool ItemRuntime::transfer_knockback_to_link(
    ItemState& item,
    LinkState& link) noexcept {
    const auto counter = item.knockback_counter;
    if (counter == 0) {
        return false;
    }

    item.knockback_counter = 0;
    if (counter >= link.knockback_counter) {
        link.knockback_counter = counter;
    }

    // The original routine updates the angle even when the existing Link
    // counter is stronger.
    link.knockback_angle = item.knockback_angle;
    return true;
}

}  // namespace oracle::core
