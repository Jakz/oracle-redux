#include "oracle/presentation/world_viewport.h"

#include <algorithm>
#include <stdexcept>

namespace oracle::presentation {

WorldViewport calculate_world_viewport(
    const double camera_x,
    const double camera_y,
    const double zoom,
    const std::int32_t output_width,
    const std::int32_t output_height,
    const double margin) {
    if (
        zoom <= 0.0 ||
        output_width < 0 ||
        output_height < 0 ||
        margin < 0.0) {
        throw std::invalid_argument{
            "viewport dimensions, zoom, and margin must be valid"};
    }
    const auto half_width =
        static_cast<double>(output_width) / (2.0 * zoom);
    const auto half_height =
        static_cast<double>(output_height) / (2.0 * zoom);
    return WorldViewport{
        .left = camera_x - half_width - margin,
        .top = camera_y - half_height - margin,
        .right = camera_x + half_width + margin,
        .bottom = camera_y + half_height + margin,
    };
}

bool intersects_world_viewport(
    const WorldViewport viewport,
    const double world_x,
    const double world_y,
    const double width,
    const double height) noexcept {
    return
        world_x + width > viewport.left &&
        world_y + height > viewport.top &&
        world_x < viewport.right &&
        world_y < viewport.bottom;
}

std::optional<WorldTextureCrop> crop_world_texture(
    const double texture_world_x,
    const double texture_world_y,
    const std::int32_t texture_width,
    const std::int32_t texture_height,
    const double camera_x,
    const double camera_y,
    const double zoom,
    const std::int32_t output_width,
    const std::int32_t output_height) {
    if (texture_width < 0 || texture_height < 0) {
        throw std::invalid_argument{
            "world texture dimensions must be nonnegative"};
    }
    const auto viewport = calculate_world_viewport(
        camera_x,
        camera_y,
        zoom,
        output_width,
        output_height);
    const auto source_left = std::clamp(
        viewport.left - texture_world_x,
        0.0,
        static_cast<double>(texture_width));
    const auto source_top = std::clamp(
        viewport.top - texture_world_y,
        0.0,
        static_cast<double>(texture_height));
    const auto source_right = std::clamp(
        viewport.right - texture_world_x,
        0.0,
        static_cast<double>(texture_width));
    const auto source_bottom = std::clamp(
        viewport.bottom - texture_world_y,
        0.0,
        static_cast<double>(texture_height));
    if (
        source_right <= source_left ||
        source_bottom <= source_top) {
        return std::nullopt;
    }
    return WorldTextureCrop{
        .source_x = source_left,
        .source_y = source_top,
        .source_width = source_right - source_left,
        .source_height = source_bottom - source_top,
        .destination_x =
            (
                texture_world_x + source_left - camera_x) *
                zoom +
            static_cast<double>(output_width) * 0.5,
        .destination_y =
            (
                texture_world_y + source_top - camera_y) *
                zoom +
            static_cast<double>(output_height) * 0.5,
        .destination_width =
            (source_right - source_left) * zoom,
        .destination_height =
            (source_bottom - source_top) * zoom,
    };
}

}  // namespace oracle::presentation
