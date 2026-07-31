#pragma once

#include <cstddef>
#include <cstdint>

namespace oracle::content {

// Oracle backgrounds select the Game Boy Color's signed $8800 tile-data
// region. The returned offset is relative to the start of VRAM at $8000.
[[nodiscard]] std::size_t signed_background_tile_offset(
    std::uint8_t tile) noexcept;

}  // namespace oracle::content
