#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_pixels.h"

namespace oracle::content {

struct InteractionSpriteFrame {
    std::uint8_t interaction_id{};
    std::uint8_t subid{};
    std::uint8_t original_oam_index{};
    std::int32_t origin_x{};
    std::int32_t origin_y{};
    std::int32_t width{};
    std::int32_t height{};
    std::vector<RgbaPixel> pixels;
};

// First family decoder for the deterministic interaction slice. Vasu ($89)
// and the two shop snakes share behavior and room records in both campaigns,
// while the decoder follows each cartridge's own graphics and OAM addresses.
class InteractionSpriteDecoder {
public:
    explicit InteractionSpriteDecoder(const RomSource& rom);

    [[nodiscard]] InteractionSpriteFrame decode_vasu(
        std::uint8_t subid,
        std::uint64_t animation_tick) const;

    [[nodiscard]] static std::uint8_t select_vasu_oam_index(
        std::uint8_t subid,
        std::uint64_t animation_tick);

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
