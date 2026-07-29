#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_pixels.h"

namespace oracle::content {

struct EnemySpriteFrame {
    std::uint8_t enemy_id{};
    std::uint8_t animation_index{};
    std::uint8_t original_oam_index{};
    std::int32_t origin_x{};
    std::int32_t origin_y{};
    std::int32_t width{};
    std::int32_t height{};
    std::vector<RgbaPixel> pixels;
};

// Bounded cartridge decoder for ENEMY_OCTOROK ($09). Directional animation
// indices are the original 0=north, 1=east, 2=south, 3=west values.
class EnemySpriteDecoder {
public:
    explicit EnemySpriteDecoder(const RomSource& rom);

    [[nodiscard]] EnemySpriteFrame decode_octorok(
        std::uint8_t animation_index,
        std::uint64_t animation_tick) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
