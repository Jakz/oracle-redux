#pragma once

#include <cstdint>
#include <optional>

namespace oracle::presentation {

struct WorldViewport {
    double left{};
    double top{};
    double right{};
    double bottom{};
};

struct WorldTextureCrop {
    double source_x{};
    double source_y{};
    double source_width{};
    double source_height{};
    double destination_x{};
    double destination_y{};
    double destination_width{};
    double destination_height{};
};

[[nodiscard]] WorldViewport calculate_world_viewport(
    double camera_x,
    double camera_y,
    double zoom,
    std::int32_t output_width,
    std::int32_t output_height,
    double margin = 0.0);

[[nodiscard]] bool intersects_world_viewport(
    WorldViewport viewport,
    double world_x,
    double world_y,
    double width,
    double height) noexcept;

[[nodiscard]] std::optional<WorldTextureCrop> crop_world_texture(
    double texture_world_x,
    double texture_world_y,
    std::int32_t texture_width,
    std::int32_t texture_height,
    double camera_x,
    double camera_y,
    double zoom,
    std::int32_t output_width,
    std::int32_t output_height);

}  // namespace oracle::presentation
