#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "oracle/core/actor_slot_domain.h"
#include "oracle/core/world_id.h"

namespace oracle::gameplay {

struct ActorCollisionBody {
    core::ActorSlotHandle actor;
    core::WorldRoomId room;
    double local_x{};
    double local_y{};
    std::uint8_t radius_y{};
    std::uint8_t radius_x{};

    [[nodiscard]] friend bool operator==(
        const ActorCollisionBody&,
        const ActorCollisionBody&) = default;
};

struct ActorCollisionSnapshot {
    static constexpr std::size_t capacity =
        core::ActorSlotDomain::slots_per_category * 4;

    std::array<ActorCollisionBody, capacity> storage{};
    std::size_t count{};

    [[nodiscard]] std::span<const ActorCollisionBody> bodies()
        const noexcept {
        return {storage.data(), count};
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return count;
    }

    [[nodiscard]] const ActorCollisionBody& front() const noexcept {
        return storage.front();
    }
};

// Produces a stable authoritative collision snapshot in original object-band
// and slot order. Presentation geometry never participates in this list.
[[nodiscard]] ActorCollisionSnapshot
collect_actor_collision_bodies(
    const core::ActorSlotDomain& actors);

}  // namespace oracle::gameplay
