#pragma once

#include <optional>

#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_traversal.h"

namespace oracle::gameplay {

class InteractionTargetFinder {
public:
    [[nodiscard]] static std::optional<core::ActorSlotHandle> find(
        const PlayerState& player,
        const core::ActorSlotDomain& actors,
        double maximum_forward_distance = 24.0,
        double maximum_lateral_distance = 10.0);
};

}  // namespace oracle::gameplay
