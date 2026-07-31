#include "oracle/content/part_data.h"

#include <cstddef>
#include <stdexcept>

namespace oracle::content {
namespace {

std::size_t part_data_offset(const RomSource& rom) {
    const auto address =
        rom.metadata().campaign == core::Campaign::ages
        ? static_cast<std::uint16_t>(0x60cd)
        : static_cast<std::uint16_t>(0x6103);
    return rom.banked_file_offset(0x3f, address);
}

}  // namespace

PartDefinitionDecoder::PartDefinitionDecoder(const RomSource& rom)
    : rom_{rom} {}

PartDefinition PartDefinitionDecoder::decode(
    const std::uint8_t id) const {
    const auto maximum_id =
        rom_.metadata().campaign == core::Campaign::ages
        ? static_cast<std::uint8_t>(0x5a)
        : static_cast<std::uint8_t>(0x53);
    if (id > maximum_id) {
        throw std::invalid_argument{
            "part id exceeds the campaign partData table"};
    }
    const auto entry =
        part_data_offset(rom_) + static_cast<std::size_t>(id) * 8;
    const auto collision = rom_.read_byte(entry + 1);
    const auto packed_radius = rom_.read_byte(entry + 2);
    const auto radius_y =
        static_cast<std::uint8_t>(packed_radius >> 4u);
    const auto radius_x =
        static_cast<std::uint8_t>(packed_radius & 0x0f);
    if (radius_y > 0x0f || radius_x > 0x0f) {
        throw std::runtime_error{
            "partData contains an invalid packed collision radius"};
    }
    return PartDefinition{
        .id = id,
        .object_gfx_header = rom_.read_byte(entry),
        .collision_mode =
            static_cast<std::uint8_t>(collision & 0x7f),
        .collision_radius_y = radius_y,
        .collision_radius_x = radius_x,
        .contact_damage =
            static_cast<std::int8_t>(rom_.read_byte(entry + 3)),
        .health = rom_.read_byte(entry + 4),
        .tile_base = rom_.read_byte(entry + 5),
        .oam_flags = rom_.read_byte(entry + 6),
        .collision_enabled = (collision & 0x80) != 0,
    };
}

}  // namespace oracle::content
