#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_pixels.h"

namespace oracle::content {

struct PartSpriteFrame {
    std::uint8_t part_id{};
    std::uint8_t original_oam_index{};
    std::int32_t origin_x{};
    std::int32_t origin_y{};
    std::int32_t width{};
    std::int32_t height{};
    std::vector<RgbaPixel> pixels;
};

// Bounded cartridge decoder for PART_OCTOROK_PROJECTILE ($18). It resolves
// campaign-relocated partData, object graphics, palette, and part OAM tables.
class PartSpriteDecoder {
public:
    explicit PartSpriteDecoder(const RomSource& rom);

    [[nodiscard]] PartSpriteFrame decode_octorok_projectile() const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
