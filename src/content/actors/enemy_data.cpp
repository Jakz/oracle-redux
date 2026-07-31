#include "oracle/content/enemy_data.h"

#include <cstddef>
#include <stdexcept>

namespace oracle::content {
namespace {

struct CampaignOffsets {
    std::size_t enemy_data{};
    std::size_t extra_enemy_data{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) noexcept {
    if (campaign == core::Campaign::ages) {
        // ages.sym: enemyData 3f:5d4b, extraEnemyData 3f:5fb9.
        return CampaignOffsets{0xfdd4b, 0xfdfb9};
    }
    // seasons.sym: enemyData 3f:5d71, extraEnemyData 3f:5ff3.
    return CampaignOffsets{0xfdd71, 0xfdff3};
}

}  // namespace

EnemyDefinitionDecoder::EnemyDefinitionDecoder(const RomSource& rom)
    : rom_{rom} {}

EnemyDefinition EnemyDefinitionDecoder::decode(
    const std::uint8_t id,
    const std::uint8_t subid) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto entry =
        offsets.enemy_data + static_cast<std::size_t>(id) * 4;
    const auto object_gfx_header = rom_.read_byte(entry);
    const auto raw_collision_mode = rom_.read_byte(entry + 1);
    auto extra_data = rom_.read_byte(entry + 2);
    auto visual = rom_.read_byte(entry + 3);

    if ((extra_data & 0x80) != 0) {
        const auto pointer = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(extra_data & 0x7f) << 8u) |
            visual);
        auto subid_entry = rom_.banked_file_offset(0x3f, pointer);
        std::uint8_t index = 0;
        for (;;) {
            extra_data = rom_.read_byte(subid_entry);
            visual = rom_.read_byte(subid_entry + 1);
            if (index == subid || (extra_data & 0x80) == 0) {
                break;
            }
            ++index;
            subid_entry += 2;
        }
    }

    const auto extra_data_index =
        static_cast<std::uint8_t>(extra_data & 0x7f);
    const auto properties =
        offsets.extra_enemy_data +
        static_cast<std::size_t>(extra_data_index) * 4;
    const auto radius_y = rom_.read_byte(properties);
    const auto radius_x = rom_.read_byte(properties + 1);
    const auto damage = static_cast<std::int8_t>(
        rom_.read_byte(properties + 2));
    const auto health = rom_.read_byte(properties + 3);
    if (radius_y > 0x20 || radius_x > 0x20) {
        throw std::runtime_error{
            "enemy extra-data collision radius is implausible"};
    }

    return EnemyDefinition{
        .id = id,
        .subid = subid,
        .object_gfx_header = object_gfx_header,
        .collision_mode =
            static_cast<std::uint8_t>(raw_collision_mode & 0x7f),
        .extra_data_index = extra_data_index,
        .palette =
            static_cast<std::uint8_t>((visual >> 4u) & 0x07),
        .tile_base =
            static_cast<std::uint8_t>((visual & 0x0f) * 2),
        .collision_radius_y = radius_y,
        .collision_radius_x = radius_x,
        .contact_damage = damage,
        .health = health,
        .collision_enabled = (raw_collision_mode & 0x80) != 0,
    };
}

}  // namespace oracle::content
