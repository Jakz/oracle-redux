#include "oracle/gameplay/object_contact.h"

namespace oracle::gameplay {
namespace {

std::int64_t signed_high_byte(const std::int32_t value) noexcept {
    constexpr std::int64_t scale = 0x100;
    const auto widened = static_cast<std::int64_t>(value);
    if (value >= 0) {
        return widened / scale;
    }
    return -((-widened + scale - 1) / scale);
}

}  // namespace

bool object_z_contact(
    const std::int32_t left_z_subpixels,
    const std::int32_t right_z_subpixels) noexcept {
    const auto difference =
        signed_high_byte(left_z_subpixels) -
        signed_high_byte(right_z_subpixels);
    return difference > -7 && difference < 7;
}

}  // namespace oracle::gameplay
