#include "oracle/content/room_collisions.h"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace oracle::content {
namespace {

constexpr std::size_t ages_tileset_header_table = 0x787e;
constexpr std::size_t seasons_tileset_header_table = 0x7964;
constexpr std::size_t ages_dictionary_table = 0x7870;
constexpr std::size_t seasons_dictionary_table = 0x794e;
constexpr std::size_t collision_header_offset = 8;
constexpr std::uint64_t signature_offset_basis = 14695981039346656037ull;
constexpr std::uint64_t signature_prime = 1099511628211ull;

constexpr std::array<std::uint8_t, 16> link_special_collisions{
    0x00, 0xc3, 0x03, 0xc0, 0x00, 0xc3, 0xc3, 0x00,
    0x00, 0xc3, 0x03, 0xc0, 0xc0, 0xc1, 0xff, 0x00,
};

constexpr std::array<std::uint8_t, 16> grounded_special_collisions{
    0xff, 0xc3, 0x03, 0xc0, 0x00, 0xc3, 0xc3, 0x00,
    0x00, 0xc3, 0x03, 0xc0, 0xc1, 0xc1, 0xff, 0xff,
};

constexpr std::array<std::uint8_t, 16>
grounded_without_small_bridges_collisions{
    0x00, 0xff, 0x03, 0xc0, 0xc3, 0xc3, 0xc3, 0x00,
    0x00, 0xff, 0x03, 0xc0, 0xc1, 0xc1, 0xff, 0x00,
};

struct CampaignOffsets {
    std::size_t tileset_header_table{};
    std::size_t dictionary_table{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            .tileset_header_table = ages_tileset_header_table,
            .dictionary_table = ages_dictionary_table,
        };
    }
    return CampaignOffsets{
        .tileset_header_table = seasons_tileset_header_table,
        .dictionary_table = seasons_dictionary_table,
    };
}

std::uint16_t read_big_u16(
    const RomSource& rom,
    const std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(rom.read_byte(offset)) << 8u) |
        rom.read_byte(offset + 1));
}

std::size_t bank_one_pointer(
    const RomSource& rom,
    const std::size_t table,
    const std::size_t index) {
    return rom.banked_file_offset(
        1,
        rom.read_little_u16(table + index * 2));
}

std::vector<std::uint8_t> decompress_dictionary_data(
    const RomSource& rom,
    std::size_t source_offset,
    const std::span<const std::uint8_t> dictionary,
    const std::uint8_t mode,
    const std::size_t output_size) {
    std::vector<std::uint8_t> output;
    output.reserve(output_size);
    while (output.size() < output_size) {
        const auto key = rom.read_byte(source_offset++);
        for (std::uint8_t bit = 0;
             bit < 8 && output.size() < output_size;
             ++bit) {
            if ((key & (1u << bit)) == 0) {
                output.push_back(rom.read_byte(source_offset++));
                continue;
            }

            std::size_t length{};
            std::size_t dictionary_offset{};
            if (mode == 0) {
                const auto packed = rom.read_little_u16(source_offset);
                source_offset += 2;
                length = static_cast<std::size_t>(packed >> 12u) + 3;
                dictionary_offset = packed & 0x0fff;
            } else if (mode == 1) {
                length = rom.read_byte(source_offset++);
                dictionary_offset = rom.read_little_u16(source_offset);
                source_offset += 2;
            } else {
                throw std::runtime_error{
                    "unsupported collision dictionary compression mode"};
            }
            if (dictionary_offset + length > dictionary.size()) {
                throw std::runtime_error{
                    "collision dictionary reference exceeds ROM data"};
            }
            const auto count =
                std::min(length, output_size - output.size());
            output.insert(
                output.end(),
                dictionary.begin() +
                    static_cast<std::ptrdiff_t>(dictionary_offset),
                dictionary.begin() +
                    static_cast<std::ptrdiff_t>(dictionary_offset + count));
        }
    }
    return output;
}

const std::array<std::uint8_t, 16>& special_table(
    const CollisionProfile profile) {
    switch (profile) {
    case CollisionProfile::link:
        return link_special_collisions;
    case CollisionProfile::grounded_actor:
        return grounded_special_collisions;
    case CollisionProfile::grounded_actor_without_small_bridges:
        return grounded_without_small_bridges_collisions;
    }
    throw std::invalid_argument{"unknown collision profile"};
}

}  // namespace

std::uint8_t RoomCollisionMap::at(
    const std::size_t column,
    const std::size_t row) const {
    if (column >= columns || row >= rows) {
        throw std::out_of_range{"room collision coordinate is out of range"};
    }
    return values[row * columns + column];
}

RoomCollisionDecoder::RoomCollisionDecoder(const RomSource& rom) : rom_{rom} {}

std::array<std::uint8_t, 256>
RoomCollisionDecoder::decode_tileset_table(
    const std::uint8_t mapping_index) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto tileset_header =
        bank_one_pointer(
            rom_,
            offsets.tileset_header_table,
            mapping_index);
    const auto collision_header =
        tileset_header + collision_header_offset;
    const auto dictionary_index = rom_.read_byte(collision_header);
    const auto dictionary_header =
        bank_one_pointer(
            rom_,
            offsets.dictionary_table,
            dictionary_index);
    const auto dictionary_bank =
        static_cast<std::uint8_t>(
            rom_.read_byte(dictionary_header) & 0x7f);
    const auto dictionary_mode =
        static_cast<std::uint8_t>(
            rom_.read_byte(dictionary_header) >> 7u);
    const auto dictionary_offset =
        rom_.banked_file_offset(
            dictionary_bank,
            read_big_u16(rom_, dictionary_header + 1));
    const auto data_offset =
        rom_.banked_file_offset(
            rom_.read_byte(collision_header + 1),
            read_big_u16(rom_, collision_header + 2));
    const auto data_size = static_cast<std::size_t>(
        read_big_u16(rom_, collision_header + 6) & 0x7fff);
    if (data_size != 256) {
        throw std::runtime_error{
            "tileset collision table has an unexpected size"};
    }

    const auto bytes =
        decompress_dictionary_data(
            rom_,
            data_offset,
            rom_.bytes().subspan(dictionary_offset),
            dictionary_mode,
            data_size);
    std::array<std::uint8_t, 256> table{};
    std::copy(bytes.begin(), bytes.end(), table.begin());
    return table;
}

RoomCollisionMap RoomCollisionDecoder::decode(
    const RoomLayout& room,
    const TilesetDescriptor& tileset) const {
    if (
        room.columns == 0 ||
        room.rows == 0 ||
        room.metatiles.size() != room.columns * room.rows) {
        throw std::invalid_argument{
            "room dimensions do not match its metatile data"};
    }
    const auto table = decode_tileset_table(tileset.mapping);
    RoomCollisionMap result{
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

bool RoomCollisionDecoder::is_solid(
    const std::uint8_t collision,
    const std::uint8_t local_x,
    const std::uint8_t local_y,
    const CollisionProfile profile) {
    if (local_x >= 16 || local_y >= 16) {
        throw std::out_of_range{
            "local collision coordinate must be inside a metatile"};
    }
    if (collision < 0x10) {
        const auto bit = static_cast<std::uint8_t>(
            (local_y < 8 ? 2u : 0u) +
            (local_x < 8 ? 1u : 0u));
        return (collision & (1u << bit)) != 0;
    }

    const auto collision_class =
        static_cast<std::uint8_t>(collision & 0x0f);
    const auto shape = special_table(profile)[collision_class];
    const auto coordinate =
        collision_class < 8 ? local_x : local_y;
    return (shape & (1u << (coordinate >> 1u))) != 0;
}

bool RoomCollisionDecoder::is_special(
    const std::uint8_t collision) noexcept {
    return collision >= 0x10;
}

std::uint64_t RoomCollisionDecoder::signature(
    const RoomCollisionMap& collisions) noexcept {
    auto result = signature_offset_basis;
    result ^= collisions.id.area;
    result *= signature_prime;
    result ^= collisions.id.room;
    result *= signature_prime;
    for (const auto value : collisions.values) {
        result ^= value;
        result *= signature_prime;
    }
    return result;
}

}  // namespace oracle::content
