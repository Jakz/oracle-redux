#pragma once

#include <cstdint>

#include "oracle/content/rom_source.h"

namespace oracle::content {

struct PartDefinition {
    std::uint8_t id{};
    std::uint8_t object_gfx_header{};
    std::uint8_t collision_mode{};
    std::uint8_t collision_radius_y{};
    std::uint8_t collision_radius_x{};
    std::int8_t contact_damage{};
    std::uint8_t health{};
    std::uint8_t tile_base{};
    std::uint8_t oam_flags{};
    bool collision_enabled{};
};

// Decodes the retail eight-byte partData record. Parts occupy their own
// original 16-slot actor band and include projectiles, temporary effects, and
// item-adjacent objects.
class PartDefinitionDecoder {
public:
    explicit PartDefinitionDecoder(const RomSource& rom);

    [[nodiscard]] PartDefinition decode(std::uint8_t id) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
