#include "oracle/content/game_boy_tiles.h"

namespace oracle::content {

std::size_t signed_background_tile_offset(
    const std::uint8_t tile) noexcept {
    const auto signed_index = tile < 0x80
        ? static_cast<std::int32_t>(tile)
        : static_cast<std::int32_t>(tile) - 0x100;
    return static_cast<std::size_t>(
        0x1000 + signed_index * 16);
}

}  // namespace oracle::content
