#pragma once

#include <cstdint>

namespace oracle::core {

using AreaId = std::uint16_t;
using RoomId = std::uint16_t;

struct WorldRoomId {
    AreaId area{};
    RoomId room{};

    [[nodiscard]] friend bool operator==(
        const WorldRoomId&,
        const WorldRoomId&) = default;
};

}  // namespace oracle::core
