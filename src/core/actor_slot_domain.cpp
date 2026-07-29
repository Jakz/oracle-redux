#include "oracle/core/actor_slot_domain.h"

#include <algorithm>
#include <utility>

namespace oracle::core {
namespace {

struct DynamicRange {
    std::uint8_t first{};
    std::uint8_t last{};
};

DynamicRange dynamic_range(const ActorCategory category) noexcept {
    switch (category) {
    case ActorCategory::item:
        return DynamicRange{7, 11};
    case ActorCategory::interaction:
        return DynamicRange{2, 15};
    case ActorCategory::enemy:
    case ActorCategory::part:
        return DynamicRange{0, 15};
    }
    return DynamicRange{};
}

std::size_t category_index(const ActorCategory category) noexcept {
    return static_cast<std::size_t>(category);
}

}  // namespace

std::optional<ActorSlotHandle> ActorSlotDomain::allocate_dynamic(
    const ActorCategory category,
    const ActorIdentity identity,
    const WorldRoomId room,
    const std::int16_t local_x,
    const std::int16_t local_y,
    const bool positioned,
    const bool conditional,
    const std::size_t source_record_index) {
    const auto range = dynamic_range(category);
    auto& actor_band = band(category);
    for (auto slot = range.first; slot <= range.last; ++slot) {
        auto& state = actor_band[slot];
        if (state.active) {
            continue;
        }
        ++state.generation;
        if (state.generation == 0) {
            ++state.generation;
        }
        state.identity = identity;
        state.room = room;
        state.local_x = local_x;
        state.local_y = local_y;
        state.source_record_index = source_record_index;
        state.active = true;
        state.positioned = positioned;
        state.conditional = conditional;
        state.collision_radius_y = 0;
        state.collision_radius_x = 0;
        state.maximum_health = 0;
        state.health = 0;
        state.contact_damage = 0;
        state.blocks_player = false;
        return ActorSlotHandle{
            category,
            slot,
            state.generation,
        };
    }
    return std::nullopt;
}

std::optional<ActorSlotHandle> ActorSlotDomain::allocate_at(
    const ActorCategory category,
    const std::uint8_t slot,
    const ActorIdentity identity,
    const WorldRoomId room) {
    if (slot >= slots_per_category) {
        return std::nullopt;
    }
    auto& state = band(category)[slot];
    if (state.active) {
        return std::nullopt;
    }
    ++state.generation;
    if (state.generation == 0) {
        ++state.generation;
    }
    state.identity = identity;
    state.room = room;
    state.active = true;
    state.positioned = false;
    state.conditional = false;
    state.collision_radius_y = 0;
    state.collision_radius_x = 0;
    state.maximum_health = 0;
    state.health = 0;
    state.contact_damage = 0;
    state.blocks_player = false;
    return ActorSlotHandle{
        category,
        slot,
        state.generation,
    };
}

ActorSlotState* ActorSlotDomain::get(
    const ActorSlotHandle handle) noexcept {
    if (handle.slot >= slots_per_category) {
        return nullptr;
    }
    auto& state = band(handle.category)[handle.slot];
    if (!state.active || state.generation != handle.generation) {
        return nullptr;
    }
    return &state;
}

const ActorSlotState* ActorSlotDomain::get(
    const ActorSlotHandle handle) const noexcept {
    if (handle.slot >= slots_per_category) {
        return nullptr;
    }
    const auto& state = band(handle.category)[handle.slot];
    if (!state.active || state.generation != handle.generation) {
        return nullptr;
    }
    return &state;
}

bool ActorSlotDomain::release(
    const ActorSlotHandle handle) noexcept {
    auto* state = get(handle);
    if (state == nullptr) {
        return false;
    }
    state->active = false;
    return true;
}

void ActorSlotDomain::clear() noexcept {
    for (auto& actor_band : bands_) {
        for (auto& state : actor_band) {
            state.active = false;
        }
    }
}

std::span<const ActorSlotState> ActorSlotDomain::slots(
    const ActorCategory category) const noexcept {
    return band(category);
}

std::size_t ActorSlotDomain::active_count(
    const ActorCategory category) const noexcept {
    const auto actor_band = slots(category);
    return static_cast<std::size_t>(
        std::count_if(
            actor_band.begin(),
            actor_band.end(),
            [](const ActorSlotState& state) {
                return state.active;
            }));
}

ActorSlotDomain::SlotBand& ActorSlotDomain::band(
    const ActorCategory category) noexcept {
    return bands_[category_index(category)];
}

const ActorSlotDomain::SlotBand& ActorSlotDomain::band(
    const ActorCategory category) const noexcept {
    return bands_[category_index(category)];
}

}  // namespace oracle::core
