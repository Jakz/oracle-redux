#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_pixels.h"
#include "oracle/core/world_id.h"

namespace oracle::content {

enum class CollisionProfile : std::uint8_t {
    link,
    grounded_actor,
    grounded_actor_without_small_bridges,
};

struct RoomCollisionMap {
    core::WorldRoomId id;
    std::size_t columns{};
    std::size_t rows{};
    std::vector<std::uint8_t> values;

    [[nodiscard]] std::uint8_t at(
        std::size_t column,
        std::size_t row) const;
};

class RoomCollisionDecoder {
public:
    explicit RoomCollisionDecoder(const RomSource& rom);

    [[nodiscard]] std::array<std::uint8_t, 256>
    decode_tileset_table(std::uint8_t mapping_index) const;

    [[nodiscard]] RoomCollisionMap decode(
        const RoomLayout& room,
        const TilesetDescriptor& tileset) const;

    // local_x/local_y are pixel coordinates inside one 16x16 metatile.
    [[nodiscard]] static bool is_solid(
        std::uint8_t collision,
        std::uint8_t local_x,
        std::uint8_t local_y,
        CollisionProfile profile = CollisionProfile::link);

    [[nodiscard]] static bool is_special(
        std::uint8_t collision) noexcept;

    [[nodiscard]] static std::uint64_t signature(
        const RoomCollisionMap& collisions) noexcept;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
