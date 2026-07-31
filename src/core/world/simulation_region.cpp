#include "oracle/core/simulation_region.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace oracle::core {

ActiveSimulationRegion::ActiveSimulationRegion(
    const SimulationRegionMode mode,
    const WorldRoomId primary_room,
    std::vector<WorldRoomId> active_rooms)
    : mode_{mode},
      primary_room_{primary_room},
      active_rooms_{std::move(active_rooms)} {
    if (active_rooms_.empty()) {
        throw std::invalid_argument{
            "an active simulation region must contain a room"};
    }
    if (!contains(primary_room_)) {
        throw std::invalid_argument{
            "the primary room must belong to the active simulation region"};
    }
    for (auto room = active_rooms_.begin(); room != active_rooms_.end(); ++room) {
        if (std::find(std::next(room), active_rooms_.end(), *room) !=
            active_rooms_.end()) {
            throw std::invalid_argument{
                "an active simulation region cannot contain duplicate rooms"};
        }
    }
    if (
        mode_ == SimulationRegionMode::classic_room &&
        active_rooms_.size() != 1) {
        throw std::invalid_argument{
            "classic room simulation must activate exactly one room"};
    }
}

SimulationRegionMode ActiveSimulationRegion::mode() const noexcept {
    return mode_;
}

WorldRoomId ActiveSimulationRegion::primary_room() const noexcept {
    return primary_room_;
}

const std::vector<WorldRoomId>& ActiveSimulationRegion::active_rooms() const
    noexcept {
    return active_rooms_;
}

bool ActiveSimulationRegion::contains(const WorldRoomId room) const noexcept {
    return std::find(active_rooms_.begin(), active_rooms_.end(), room) !=
        active_rooms_.end();
}

}  // namespace oracle::core
