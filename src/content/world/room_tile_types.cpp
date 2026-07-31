#include "oracle/content/room_tile_types.h"

#include <stdexcept>

namespace oracle::content {
namespace {

constexpr std::size_t ages_tile_types_table = 0x17c6e;
constexpr std::size_t seasons_tile_types_table = 0x17bad;
constexpr std::uint8_t tile_types_bank = 5;
constexpr std::uint8_t collision_mode_count = 6;
constexpr double active_tile_y_offset = 5.0;

std::size_t table_offset(const RomSource& rom) noexcept {
    return rom.metadata().campaign == core::Campaign::ages
        ? ages_tile_types_table
        : seasons_tile_types_table;
}

}  // namespace

LinkTileType RoomTileTypeMap::at(
    const std::size_t column,
    const std::size_t row) const {
    if (column >= columns || row >= rows) {
        throw std::out_of_range{"room tile-type coordinate is out of range"};
    }
    return values[row * columns + column];
}

RoomTileTypeDecoder::RoomTileTypeDecoder(const RomSource& rom) : rom_{rom} {}

std::array<LinkTileType, 256>
RoomTileTypeDecoder::decode_collision_mode_table(
    const std::uint8_t collision_mode) const {
    if (collision_mode >= collision_mode_count) {
        throw std::out_of_range{
            "tileset collision mode exceeds the ROM tile-type table"};
    }

    const auto root = table_offset(rom_);
    auto cursor = rom_.banked_file_offset(
        tile_types_bank,
        rom_.read_little_u16(root + collision_mode * 2));
    std::array<LinkTileType, 256> result{};
    while (true) {
        const auto metatile = rom_.read_byte(cursor++);
        if (metatile == 0) {
            break;
        }
        result[metatile] = static_cast<LinkTileType>(
            rom_.read_byte(cursor++));
    }
    return result;
}

RoomTileTypeMap RoomTileTypeDecoder::decode(
    const RoomLayout& room,
    const TilesetDescriptor& tileset) const {
    if (
        room.columns == 0 ||
        room.rows == 0 ||
        room.metatiles.size() != room.columns * room.rows) {
        throw std::invalid_argument{
            "room dimensions do not match its metatile data"};
    }
    const auto table =
        decode_collision_mode_table(tileset.collision_mode);
    RoomTileTypeMap result{
        .id = room.id,
        .columns = room.columns,
        .rows = room.rows,
        .values = {},
    };
    result.values.reserve(room.metatiles.size());
    for (const auto metatile : room.metatiles) {
        result.values.push_back(table[metatile]);
    }
    return result;
}

std::optional<LinkTileType> RoomTileTypeDecoder::sample_link_feet(
    const RoomTileTypeMap& types,
    const double local_x,
    const double local_y) noexcept {
    if (local_x < 0.0 || local_y + active_tile_y_offset < 0.0) {
        return std::nullopt;
    }
    const auto column = static_cast<std::size_t>(
        local_x / static_cast<double>(metatile_world_size));
    const auto row = static_cast<std::size_t>(
        (local_y + active_tile_y_offset) /
        static_cast<double>(metatile_world_size));
    if (column >= types.columns || row >= types.rows) {
        return std::nullopt;
    }
    return types.values[row * types.columns + column];
}

std::string_view link_tile_type_name(const LinkTileType type) noexcept {
    switch (type) {
    case LinkTileType::normal: return "normal";
    case LinkTileType::hole: return "hole";
    case LinkTileType::warp_hole: return "warp-hole";
    case LinkTileType::cracked_floor: return "cracked-floor";
    case LinkTileType::vines: return "vines";
    case LinkTileType::grass: return "grass";
    case LinkTileType::stairs: return "stairs";
    case LinkTileType::water: return "water";
    case LinkTileType::stump: return "stump";
    case LinkTileType::up_conveyor: return "up-conveyor";
    case LinkTileType::right_conveyor: return "right-conveyor";
    case LinkTileType::down_conveyor: return "down-conveyor";
    case LinkTileType::left_conveyor: return "left-conveyor";
    case LinkTileType::spike: return "spike";
    case LinkTileType::cracked_ice: return "cracked-ice";
    case LinkTileType::ice: return "ice";
    case LinkTileType::lava: return "lava";
    case LinkTileType::puddle: return "puddle";
    case LinkTileType::up_current: return "up-current";
    case LinkTileType::right_current: return "right-current";
    case LinkTileType::down_current: return "down-current";
    case LinkTileType::left_current: return "left-current";
    case LinkTileType::raisable_floor: return "raisable-floor";
    case LinkTileType::seawater: return "seawater";
    case LinkTileType::whirlpool: return "whirlpool";
    }
    return "unknown";
}

}  // namespace oracle::content
