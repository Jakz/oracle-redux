#include "oracle/presentation/camera.h"

#include <stdexcept>

namespace oracle::presentation {

PresentationCamera::PresentationCamera(
    const CameraMode mode,
    const PixelSize output_size,
    const WorldPoint center,
    const double pixels_per_world_unit)
    : mode_(mode),
      output_size_(output_size),
      center_(center),
      pixels_per_world_unit_(pixels_per_world_unit) {
    if (output_size.width == 0 || output_size.height == 0) {
        throw std::invalid_argument("camera output size must be nonzero");
    }
    if (!(pixels_per_world_unit > 0.0)) {
        throw std::invalid_argument(
            "camera pixels_per_world_unit must be positive");
    }
}

PresentationCamera PresentationCamera::fidelity() {
    return PresentationCamera{
        CameraMode::fidelity,
        PixelSize{.width = 160, .height = 144},
        WorldPoint{.x = 80.0, .y = 72.0},
        1.0,
    };
}

CameraMode PresentationCamera::mode() const noexcept {
    return mode_;
}

PixelSize PresentationCamera::output_size() const noexcept {
    return output_size_;
}

WorldPoint PresentationCamera::center() const noexcept {
    return center_;
}

double PresentationCamera::pixels_per_world_unit() const noexcept {
    return pixels_per_world_unit_;
}

WorldRect PresentationCamera::visible_world_rect() const noexcept {
    const auto width =
        static_cast<double>(output_size_.width) / pixels_per_world_unit_;
    const auto height =
        static_cast<double>(output_size_.height) / pixels_per_world_unit_;
    return WorldRect{
        .left = center_.x - width / 2.0,
        .top = center_.y - height / 2.0,
        .width = width,
        .height = height,
    };
}

}  // namespace oracle::presentation
