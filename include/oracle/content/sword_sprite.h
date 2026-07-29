#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_pixels.h"

namespace oracle::content {

struct SwordSpriteFrame {
    std::uint8_t animation_index{};
    std::uint8_t original_oam_index{};
    std::int32_t origin_x{};
    std::int32_t origin_y{};
    std::int32_t width{};
    std::int32_t height{};
    std::vector<RgbaPixel> pixels;
};

// Decodes ITEM_SWORD's shared spr_swords graphics and original item OAM
// composition directly from the player-supplied cartridge.
class SwordSpriteDecoder {
public:
    explicit SwordSpriteDecoder(const RomSource& rom);

    [[nodiscard]] SwordSpriteFrame decode(
        std::uint8_t animation_index) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
