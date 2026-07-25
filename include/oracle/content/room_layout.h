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
inline constexpr std::size_t large_room_columns = 15;
inline constexpr std::size_t large_room_rows = 11;
inline constexpr std::size_t large_room_storage_columns = 16;
inline constexpr std::size_t large_room_storage_metatile_count =
    large_room_storage_columns * large_room_rows;
inline constexpr std::int32_t metatile_world_size = 16;
inline constexpr std::int32_t small_room_world_width =
    static_cast<std::int32_t>(small_room_columns) * metatile_world_size;
inline constexpr std::int32_t small_room_world_height =
    static_cast<std::int32_t>(small_room_rows) * metatile_world_size;

struct RoomLayout {
    core::WorldRoomId id;
    std::size_t columns{small_room_columns};
    std::size_t rows{small_room_rows};
    std::vector<std::uint8_t> metatiles;

    [[nodiscard]] std::int32_t pixel_width() const {
        return
            static_cast<std::int32_t>(columns) * metatile_world_size;
    }

    [[nodiscard]] std::int32_t pixel_height() const {
        return
            static_cast<std::int32_t>(rows) * metatile_world_size;
    }
};

struct RoomPlacement {
    RoomLayout layout;
    std::int32_t world_x{};
    std::int32_t world_y{};
};

enum class RoomLayoutKind : std::uint8_t {
    large,
    small,
};

class RoomLayoutDecoder {
public:
    explicit RoomLayoutDecoder(const RomSource& rom);

    [[nodiscard]] std::uint8_t layout_group_count() const;

    [[nodiscard]] RoomLayoutKind layout_kind(
        std::uint8_t layout_group) const;

    [[nodiscard]] RoomLayout decode_small_room(
        std::uint8_t layout_group,
        std::uint8_t room) const;

    [[nodiscard]] RoomLayout decode_small_room(
        std::uint8_t world_group,
        std::uint8_t layout_group,
        std::uint8_t room) const;

    [[nodiscard]] RoomLayout decode_large_room(
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

    [[nodiscard]] static std::vector<std::uint8_t>
    decode_dictionary_layout(
        std::span<const std::uint8_t> encoded,
        std::span<const std::uint8_t> dictionary,
        std::size_t output_size);

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
