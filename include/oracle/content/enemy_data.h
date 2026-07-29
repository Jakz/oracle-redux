#pragma once

#include <cstdint>

#include "oracle/content/rom_source.h"

namespace oracle::content {

struct EnemyDefinition {
    std::uint8_t id{};
    std::uint8_t subid{};
    std::uint8_t object_gfx_header{};
    std::uint8_t collision_mode{};
    std::uint8_t extra_data_index{};
    std::uint8_t palette{};
    std::uint8_t tile_base{};
    std::uint8_t collision_radius_y{};
    std::uint8_t collision_radius_x{};
    std::int8_t contact_damage{};
    std::uint8_t health{};
    bool collision_enabled{};
};

// Decodes the retail four-byte enemy record, optional per-subid record, and
// extraEnemyData entry. The first supported consumer is Octorok, but the
// decoder deliberately preserves the shared table format for later families.
class EnemyDefinitionDecoder {
public:
    explicit EnemyDefinitionDecoder(const RomSource& rom);

    [[nodiscard]] EnemyDefinition decode(
        std::uint8_t id,
        std::uint8_t subid) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
