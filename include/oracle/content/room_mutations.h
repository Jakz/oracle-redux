#pragma once

#include <cstdint>
#include <span>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_pixels.h"

namespace oracle::content {

struct TileReplacement {
    std::uint8_t replacement{};
    std::uint8_t target{};
};

class RoomMutationDecoder {
public:
    explicit RoomMutationDecoder(const RomSource& rom);

    [[nodiscard]] RoomLayout apply_standard_substitutions(
        RoomLayout room,
        const TilesetDescriptor& tileset,
        std::uint8_t room_flags) const;

    static void apply_replacements(
        std::span<std::uint8_t> metatiles,
        std::span<const TileReplacement> replacements);

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
