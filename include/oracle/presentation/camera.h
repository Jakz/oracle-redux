#pragma once

#include <cstdint>

namespace oracle::presentation {

enum class CameraMode : std::uint8_t {
    fidelity,
    widescreen,
    world_overview,
};

struct PixelSize {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct WorldPoint {
    double x{};
    double y{};
};

struct WorldRect {
    double left{};
    double top{};
    double width{};
    double height{};
};

class PresentationCamera {
public:
    PresentationCamera(
        CameraMode mode,
        PixelSize output_size,
        WorldPoint center,
        double pixels_per_world_unit);

    [[nodiscard]] static PresentationCamera fidelity();

    [[nodiscard]] CameraMode mode() const noexcept;
    [[nodiscard]] PixelSize output_size() const noexcept;
    [[nodiscard]] WorldPoint center() const noexcept;
    [[nodiscard]] double pixels_per_world_unit() const noexcept;
    [[nodiscard]] WorldRect visible_world_rect() const noexcept;

private:
    CameraMode mode_;
    PixelSize output_size_;
    WorldPoint center_;
    double pixels_per_world_unit_;
};

}  // namespace oracle::presentation
