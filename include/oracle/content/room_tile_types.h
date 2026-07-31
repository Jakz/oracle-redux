#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_pixels.h"
#include "oracle/core/world_id.h"

namespace oracle::content {

// Values consumed by linkApplyTileTypes in the original top-down runtime.
// Side-scrolling rooms reuse the byte as a bit field and are identified by
// their tileset flag before applying these enum semantics.
enum class LinkTileType : std::uint8_t {
    normal = 0x00,
    hole = 0x01,
    warp_hole = 0x02,
    cracked_floor = 0x03,
    vines = 0x04,
    grass = 0x05,
    stairs = 0x06,
    water = 0x07,
    stump = 0x08,
    up_conveyor = 0x09,
    right_conveyor = 0x0a,
    down_conveyor = 0x0b,
    left_conveyor = 0x0c,
    spike = 0x0d,
    cracked_ice = 0x0e,
    ice = 0x0f,
    lava = 0x10,
    puddle = 0x11,
    up_current = 0x12,
    right_current = 0x13,
    down_current = 0x14,
    left_current = 0x15,
    raisable_floor = 0x16,
    seawater = 0x17,
    whirlpool = 0x18,
};

struct RoomTileTypeMap {
    core::WorldRoomId id;
    std::size_t columns{};
    std::size_t rows{};
    std::vector<LinkTileType> values;

    [[nodiscard]] LinkTileType at(
        std::size_t column,
        std::size_t row) const;
};

class RoomTileTypeDecoder {
public:
    explicit RoomTileTypeDecoder(const RomSource& rom);

    [[nodiscard]] std::array<LinkTileType, 256>
    decode_collision_mode_table(std::uint8_t collision_mode) const;

    [[nodiscard]] RoomTileTypeMap decode(
        const RoomLayout& room,
        const TilesetDescriptor& tileset) const;

    // Recreates @linkGetActiveTileType's objectGetRelativeTile($0500):
    // Link's active metatile is sampled five pixels below his origin.
    [[nodiscard]] static std::optional<LinkTileType> sample_link_feet(
        const RoomTileTypeMap& types,
        double local_x,
        double local_y) noexcept;

private:
    const RomSource& rom_;
};

[[nodiscard]] std::string_view link_tile_type_name(
    LinkTileType type) noexcept;

}  // namespace oracle::content
