#include "oracle/gameplay/interaction_target.h"

#include <cmath>
#include <limits>

namespace oracle::gameplay {

std::optional<core::ActorSlotHandle> InteractionTargetFinder::find(
    const PlayerState& player,
    const core::ActorSlotDomain& actors,
    const double maximum_forward_distance,
    const double maximum_lateral_distance) {
    std::optional<core::ActorSlotHandle> best;
    auto best_distance = std::numeric_limits<double>::max();
    const auto slots =
        actors.slots(core::ActorCategory::interaction);
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const auto& actor = slots[slot];
        if (
            !actor.active ||
            !actor.positioned ||
            actor.room != player.room) {
            continue;
        }
        const auto delta_x =
            static_cast<double>(actor.local_x) - player.local_x;
        const auto delta_y =
            static_cast<double>(actor.local_y) - player.local_y;
        double forward{};
        double lateral{};
        switch (player.facing) {
        case PlayerFacing::north:
            forward = -delta_y;
            lateral = delta_x;
            break;
        case PlayerFacing::east:
            forward = delta_x;
            lateral = delta_y;
            break;
        case PlayerFacing::south:
            forward = delta_y;
            lateral = delta_x;
            break;
        case PlayerFacing::west:
            forward = -delta_x;
            lateral = delta_y;
            break;
        }
        if (
            forward <= 0.0 ||
            forward > maximum_forward_distance ||
            std::abs(lateral) > maximum_lateral_distance) {
            continue;
        }
        const auto distance =
            delta_x * delta_x + delta_y * delta_y;
        if (distance >= best_distance) {
            continue;
        }
        best_distance = distance;
        best = core::ActorSlotHandle{
            core::ActorCategory::interaction,
            static_cast<std::uint8_t>(slot),
            actor.generation,
        };
    }
    return best;
}

}  // namespace oracle::gameplay
