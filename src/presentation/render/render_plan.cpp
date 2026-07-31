#include "oracle/presentation/render_plan.h"

namespace oracle::presentation {

std::vector<RenderPass> build_render_plan(
    const PresentationSettings& settings) {
    std::vector<RenderPass> passes;
    passes.reserve(5);
    passes.push_back(RenderPass::world);
    if (settings.dynamic_lighting) {
        passes.push_back(RenderPass::dynamic_lighting);
    }
    if (settings.atmospheric_fog) {
        passes.push_back(RenderPass::atmospheric_fog);
    }
    if (settings.color_grading) {
        passes.push_back(RenderPass::color_grade);
    }
    passes.push_back(RenderPass::interface);
    return passes;
}

}  // namespace oracle::presentation
