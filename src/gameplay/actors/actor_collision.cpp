#include "oracle/gameplay/actor_collision.h"

#include <array>
#include <cstddef>

namespace oracle::gameplay {

ActorCollisionSnapshot collect_actor_collision_bodies(
    const core::ActorSlotDomain& actors) {
    constexpr std::array categories{
        core::ActorCategory::item,
        core::ActorCategory::interaction,
        core::ActorCategory::enemy,
        core::ActorCategory::part,
    };
    ActorCollisionSnapshot result;
    for (const auto category : categories) {
        const auto slots = actors.slots(category);
        for (std::size_t slot = 0; slot < slots.size(); ++slot) {
            const auto& state = slots[slot];
            if (
                !state.active ||
                !state.positioned ||
                !state.blocks_player ||
                state.collision_radius_y == 0 ||
                state.collision_radius_x == 0) {
                continue;
            }
            result.storage[result.count++] =
                ActorCollisionBody{
                    .actor =
                        core::ActorSlotHandle{
                            category,
                            static_cast<std::uint8_t>(slot),
                            state.generation,
                        },
                    .room = state.room,
                    .local_x = static_cast<double>(state.local_x),
                    .local_y = static_cast<double>(state.local_y),
                    .radius_y = state.collision_radius_y,
                    .radius_x = state.collision_radius_x,
                };
        }
    }
    return result;
}

}  // namespace oracle::gameplay
