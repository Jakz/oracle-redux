#pragma once

#include <cstdint>

namespace oracle::gameplay {

// Recreates _checkCollidedWithLink's signed high-byte comparison. Ordinary
// object contact is accepted only when the two Z coordinates are within seven
// original pixels. Values use the runtime's signed 8.8 convention.
[[nodiscard]] bool object_z_contact(
    std::int32_t left_z_subpixels,
    std::int32_t right_z_subpixels) noexcept;

}  // namespace oracle::gameplay
