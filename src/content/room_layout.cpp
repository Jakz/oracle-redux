#include "oracle/content/room_layout.h"

#include <algorithm>
#include <stdexcept>

namespace oracle::content {
namespace {

constexpr std::size_t ages_room_layout_group_table = 0x10f6c;
constexpr std::size_t seasons_room_layout_group_table = 0x10c4c;
constexpr std::size_t layout_group_record_size = 8;
constexpr std::uint8_t ages_layout_group_count = 6;
constexpr std::uint8_t seasons_layout_group_count = 7;

std::size_t read_three_byte_pointer(
    const RomSource& rom,
    const std::size_t offset) {
    return rom.banked_file_offset(
        rom.read_byte(offset),
        rom.read_little_u16(offset + 1));
}

}  // namespace

RoomLayoutDecoder::RoomLayoutDecoder(const RomSource& rom) : rom_{rom} {}

std::uint8_t RoomLayoutDecoder::layout_group_count() const {
    return rom_.metadata().campaign == core::Campaign::ages
        ? ages_layout_group_count
        : seasons_layout_group_count;
}

RoomLayoutKind RoomLayoutDecoder::layout_kind(
    const std::uint8_t layout_group) const {
    if (layout_group >= layout_group_count()) {
        throw std::out_of_range{"room layout group is outside cartridge table"};
    }
    const auto group_table =
        rom_.metadata().campaign == core::Campaign::ages
        ? ages_room_layout_group_table
        : seasons_room_layout_group_table;
    const auto group_record =
        group_table +
        static_cast<std::size_t>(layout_group) * layout_group_record_size;
    return rom_.read_byte(group_record) == 1
        ? RoomLayoutKind::small
        : RoomLayoutKind::large;
}

RoomLayout RoomLayoutDecoder::decode_small_room(
    const std::uint8_t layout_group,
    const std::uint8_t room) const {
    return decode_small_room(layout_group, layout_group, room);
}

RoomLayout RoomLayoutDecoder::decode_small_room(
    const std::uint8_t world_group,
    const std::uint8_t layout_group,
    const std::uint8_t room) const {
    const auto group_table =
        rom_.metadata().campaign == core::Campaign::ages
        ? ages_room_layout_group_table
        : seasons_room_layout_group_table;
    const auto group_record =
        group_table +
        static_cast<std::size_t>(layout_group) * layout_group_record_size;

    if (layout_kind(layout_group) != RoomLayoutKind::small) {
        throw std::invalid_argument{
            "the requested layout group contains large dungeon rooms"};
    }

    const auto pointer_table =
        read_three_byte_pointer(rom_, group_record + 1);
    const auto data_base =
        read_three_byte_pointer(rom_, group_record + 4);
    const auto relative =
        rom_.read_little_u16(
            pointer_table + static_cast<std::size_t>(room) * 2);
    const auto compression_mode =
        static_cast<std::uint8_t>(relative >> 14u);
    const auto data_offset =
        data_base + static_cast<std::size_t>(relative & 0x3fff);
    const auto source = rom_.bytes().subspan(data_offset);

    return RoomLayout{
        .id =
            core::WorldRoomId{
                .area = world_group,
                .room = room,
            },
        .metatiles =
            decode_common_byte_layout(source, compression_mode),
    };
}

std::vector<RoomPlacement> RoomLayoutDecoder::decode_neighborhood(
    const std::uint8_t layout_group,
    const std::uint8_t center_room,
    const std::uint8_t radius) const {
    const auto center_x = static_cast<int>(center_room & 0x0f);
    const auto center_y = static_cast<int>(center_room >> 4u);
    const auto extent = static_cast<int>(radius);
    std::vector<RoomPlacement> rooms;
    rooms.reserve(
        static_cast<std::size_t>((extent * 2 + 1) * (extent * 2 + 1)));

    for (int y = center_y - extent; y <= center_y + extent; ++y) {
        if (y < 0 || y > 15) {
            continue;
        }
        for (int x = center_x - extent; x <= center_x + extent; ++x) {
            if (x < 0 || x > 15) {
                continue;
            }
            const auto room =
                static_cast<std::uint8_t>((y << 4) | x);
            rooms.push_back(RoomPlacement{
                .layout = decode_small_room(layout_group, room),
                .world_x = x * small_room_world_width,
                .world_y = y * small_room_world_height,
            });
        }
    }
    return rooms;
}

std::array<std::uint8_t, small_room_metatile_count>
RoomLayoutDecoder::decode_common_byte_layout(
    const std::span<const std::uint8_t> encoded,
    const std::uint8_t compression_mode) {
    std::array<std::uint8_t, small_room_metatile_count> output{};
    if (compression_mode == 0) {
        if (encoded.size() < output.size()) {
            throw std::runtime_error{"uncompressed room layout is truncated"};
        }
        std::copy_n(encoded.begin(), output.size(), output.begin());
        return output;
    }
    if (compression_mode != 1 && compression_mode != 2) {
        throw std::runtime_error{"unsupported small-room compression mode"};
    }

    const auto mask_bytes = static_cast<std::size_t>(compression_mode);
    const auto values_per_chunk = mask_bytes * 8;
    std::size_t source_offset = 0;
    std::size_t output_offset = 0;
    while (output_offset < output.size()) {
        if (source_offset + mask_bytes > encoded.size()) {
            throw std::runtime_error{"compressed room mask is truncated"};
        }
        std::uint16_t mask = 0;
        for (std::size_t byte = 0; byte < mask_bytes; ++byte) {
            mask |= static_cast<std::uint16_t>(
                encoded[source_offset++]) << (byte * 8);
        }

        if (mask == 0) {
            if (source_offset + values_per_chunk > encoded.size()) {
                throw std::runtime_error{"compressed room literals are truncated"};
            }
            for (std::size_t index = 0; index < values_per_chunk; ++index) {
                output[output_offset++] = encoded[source_offset++];
            }
            continue;
        }

        if (source_offset >= encoded.size()) {
            throw std::runtime_error{"compressed room repeat byte is missing"};
        }
        const auto repeated = encoded[source_offset++];
        for (std::size_t bit = 0; bit < values_per_chunk; ++bit) {
            if (output_offset >= output.size()) {
                break;
            }
            if ((mask & (1u << bit)) != 0) {
                output[output_offset++] = repeated;
            } else {
                if (source_offset >= encoded.size()) {
                    throw std::runtime_error{
                        "compressed room literal is missing"};
                }
                output[output_offset++] = encoded[source_offset++];
            }
        }
    }
    return output;
}

}  // namespace oracle::content
