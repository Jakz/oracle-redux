#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/core/world_id.h"

namespace oracle::content {

inline constexpr std::size_t small_room_columns = 10;
inline constexpr std::size_t small_room_rows = 8;
inline constexpr std::size_t small_room_metatile_count =
    small_room_columns * small_room_rows;
inline constexpr std::int32_t metatile_world_size = 16;
inline constexpr std::int32_t small_room_world_width =
    static_cast<std::int32_t>(small_room_columns) * metatile_world_size;
inline constexpr std::int32_t small_room_world_height =
    static_cast<std::int32_t>(small_room_rows) * metatile_world_size;

struct RoomLayout {
    core::WorldRoomId id;
    std::array<std::uint8_t, small_room_metatile_count> metatiles{};
};

struct RoomPlacement {
    RoomLayout layout;
    std::int32_t world_x{};
    std::int32_t world_y{};
};

class RoomLayoutDecoder {
public:
    explicit RoomLayoutDecoder(const RomSource& rom);

    [[nodiscard]] RoomLayout decode_small_room(
        std::uint8_t layout_group,
        std::uint8_t room) const;

    [[nodiscard]] RoomLayout decode_small_room(
        std::uint8_t world_group,
        std::uint8_t layout_group,
        std::uint8_t room) const;

    [[nodiscard]] std::vector<RoomPlacement> decode_neighborhood(
        std::uint8_t layout_group,
        std::uint8_t center_room,
        std::uint8_t radius) const;

    [[nodiscard]] static std::array<
        std::uint8_t,
        small_room_metatile_count>
    decode_common_byte_layout(
        std::span<const std::uint8_t> encoded,
        std::uint8_t compression_mode);

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
