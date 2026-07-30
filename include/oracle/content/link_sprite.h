#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_pixels.h"

namespace oracle::content {

enum class LinkDirection : std::uint8_t {
    north = 0,
    east = 1,
    south = 2,
    west = 3,
};

struct LinkSpriteFrame {
    std::uint8_t original_frame{};
    std::int32_t origin_x{-8};
    std::int32_t origin_y{-8};
    std::int32_t width{16};
    std::int32_t height{16};
    std::vector<RgbaPixel> pixels;
};

// Decodes Link's original 8x16 OAM composition, graphics, and palette from a
// player-supplied cartridge image. The returned visual remains independent
// from Link's smaller collision body.
class LinkSpriteDecoder {
public:
    explicit LinkSpriteDecoder(const RomSource& rom);

    [[nodiscard]] LinkSpriteFrame decode(
        LinkDirection direction,
        bool moving,
        std::uint64_t animation_tick) const;

    [[nodiscard]] static std::uint8_t select_original_frame(
        LinkDirection direction,
        bool moving,
        std::uint64_t animation_tick);

    // Decodes a frame index already selected by a retail Link animation.
    [[nodiscard]] LinkSpriteFrame decode_original_frame(
        std::uint8_t frame_index) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
