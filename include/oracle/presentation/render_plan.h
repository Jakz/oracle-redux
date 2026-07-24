#pragma once

#include <cstdint>
#include <vector>

#include "oracle/experience_settings.h"

namespace oracle::presentation {

enum class RenderPass : std::uint8_t {
    world,
    dynamic_lighting,
    atmospheric_fog,
    color_grade,
    interface,
};

// Produces a backend-independent pass order. SDL3 GPU, another GPU backend,
// and a headless verifier can all consume the same resolved plan.
[[nodiscard]] std::vector<RenderPass> build_render_plan(
    const PresentationSettings& settings);

}  // namespace oracle::presentation
