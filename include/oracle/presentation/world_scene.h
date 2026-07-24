#pragma once

#include <cstdint>
#include <vector>

#include "oracle/core/world_id.h"

namespace oracle::presentation {

using AreaId = core::AreaId;
using RoomId = core::RoomId;
using AssetId = std::uint32_t;

struct WorldTile {
    AreaId area{};
    RoomId room{};
    std::int32_t world_x{};
    std::int32_t world_y{};
    std::int16_t layer{};
    AssetId tile{};
};

struct WorldSprite {
    AreaId area{};
    RoomId room{};
    std::int32_t world_x{};
    std::int32_t world_y{};
    std::int32_t world_z{};
    std::int16_t layer{};
    AssetId sprite{};
    std::uint8_t frame{};
    std::uint8_t palette{};
};

// Immutable presentation input may contain the active room plus any number of
// cached, non-simulated neighboring rooms for widescreen or World Overview.
struct WorldScene {
    std::vector<WorldTile> tiles;
    std::vector<WorldSprite> sprites;
};

}  // namespace oracle::presentation
