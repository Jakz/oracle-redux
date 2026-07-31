#include "oracle/script/original_state.h"

#include <algorithm>
#include <cstddef>

namespace oracle::script {
namespace {

std::size_t state_index(const OriginalStateKey key) noexcept {
    return static_cast<std::size_t>(key);
}

std::size_t actor_field_index(const OriginalActorField field) noexcept {
    return static_cast<std::size_t>(field);
}

}  // namespace

std::optional<OriginalStateKey> OriginalStateResolver::memory_key(
    const core::Campaign campaign,
    const std::uint16_t original_address) noexcept {
    if (original_address == 0xc615) {
        return OriginalStateKey::obtained_ring_box;
    }
    if (original_address == 0xcba5) {
        return OriginalStateKey::selected_text_option;
    }
    if (original_address == 0xcc01) {
        return OriginalStateKey::is_linked_game;
    }
    static_cast<void>(campaign);
    return std::nullopt;
}

std::optional<OriginalStateKey>
OriginalStateResolver::global_flag_key(
    const std::uint8_t original_flag) noexcept {
    switch (original_flag) {
    case 0x00:
        return OriginalStateKey::global_1000_enemies_killed;
    case 0x01:
        return OriginalStateKey::global_10000_rupees_collected;
    case 0x02:
        return OriginalStateKey::global_beat_ganon;
    case 0x04:
        return OriginalStateKey::global_got_slayers_ring;
    case 0x05:
        return OriginalStateKey::global_got_wealth_ring;
    case 0x06:
        return OriginalStateKey::global_got_victory_ring;
    case 0x08:
        return OriginalStateKey::global_obtained_ring_box;
    case 0x09:
        return OriginalStateKey::global_appraised_hundredth_ring;
    default:
        return std::nullopt;
    }
}

std::optional<OriginalActorField> OriginalStateResolver::actor_field(
    const std::uint8_t original_low_address) noexcept {
    switch (original_low_address) {
    case 0x76:
        return OriginalActorField::var36;
    case 0x77:
        return OriginalActorField::var37;
    case 0x78:
        return OriginalActorField::var38;
    case 0x7a:
        return OriginalActorField::var3a;
    case 0x7b:
        return OriginalActorField::var3b;
    default:
        return std::nullopt;
    }
}

std::uint8_t OriginalStateStore::read(
    const OriginalStateKey key) const noexcept {
    return values_[state_index(key)];
}

void OriginalStateStore::write(
    const OriginalStateKey key,
    const std::uint8_t value) noexcept {
    values_[state_index(key)] = value;
}

std::uint8_t OriginalStateStore::read_actor(
    const core::ActorSlotHandle actor,
    const OriginalActorField field) const noexcept {
    const auto* values = find_actor(actor);
    return values == nullptr
        ? 0
        : values->values[actor_field_index(field)];
}

void OriginalStateStore::write_actor(
    const core::ActorSlotHandle actor,
    const OriginalActorField field,
    const std::uint8_t value) {
    auto* values = find_actor(actor);
    if (values == nullptr) {
        actor_values_.push_back(ActorFields{.actor = actor});
        values = &actor_values_.back();
    }
    values->values[actor_field_index(field)] = value;
}

OriginalStateStore::ActorFields* OriginalStateStore::find_actor(
    const core::ActorSlotHandle actor) noexcept {
    const auto found = std::find_if(
        actor_values_.begin(),
        actor_values_.end(),
        [actor](const ActorFields& fields) {
            return fields.actor == actor;
        });
    return found == actor_values_.end() ? nullptr : &*found;
}

const OriginalStateStore::ActorFields* OriginalStateStore::find_actor(
    const core::ActorSlotHandle actor) const noexcept {
    const auto found = std::find_if(
        actor_values_.begin(),
        actor_values_.end(),
        [actor](const ActorFields& fields) {
            return fields.actor == actor;
        });
    return found == actor_values_.end() ? nullptr : &*found;
}

}  // namespace oracle::script
