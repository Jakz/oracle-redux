#pragma once

#include <vector>

#include "oracle/core/world_id.h"
#include "oracle/experience_settings.h"

namespace oracle::core {

// Declares the rooms whose authoritative gameplay advances during a logic
// tick. The renderer may display additional rooms without adding them here.
class ActiveSimulationRegion {
public:
    ActiveSimulationRegion(
        SimulationRegionMode mode,
        WorldRoomId primary_room,
        std::vector<WorldRoomId> active_rooms);

    [[nodiscard]] SimulationRegionMode mode() const noexcept;
    [[nodiscard]] WorldRoomId primary_room() const noexcept;
    [[nodiscard]] const std::vector<WorldRoomId>& active_rooms() const noexcept;
    [[nodiscard]] bool contains(WorldRoomId room) const noexcept;

private:
    SimulationRegionMode mode_;
    WorldRoomId primary_room_;
    std::vector<WorldRoomId> active_rooms_;
};

}  // namespace oracle::core
