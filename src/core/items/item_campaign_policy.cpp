#include "oracle/core/item_campaign_policy.h"

namespace oracle::core {

namespace {

constexpr std::uint8_t kMountedAnimationFirst = 0x20;
constexpr std::uint8_t kMountedAnimationEnd = 0x24;
constexpr std::uint8_t kMountedAnimationOffset = 0x04;
constexpr std::uint8_t kUnderwaterSourceAnimation = 0x22;
constexpr std::uint8_t kUnderwaterAnimation = 0x2d;

}  // namespace

bool ItemCampaignPolicy::is_link_considered_grounded(
    const ParentItemContext& context) const noexcept {
    if (context.link_is_riding
        || context.link_is_in_air
        || context.link_is_swimming) {
        return false;
    }
    return campaign_ == Campaign::seasons || !context.link_is_underwater;
}

std::uint8_t ItemCampaignPolicy::select_parent_item_animation(
    const std::uint8_t base_animation,
    const ParentItemContext& context) const noexcept {
    if (campaign_ == Campaign::ages) {
        if (context.companion_is_raft) {
            return base_animation;
        }
        if (context.link_is_underwater) {
            return base_animation == kUnderwaterSourceAnimation
                ? kUnderwaterAnimation
                : base_animation;
        }
    }

    if (
        context.link_is_riding
        && base_animation >= kMountedAnimationFirst
        && base_animation < kMountedAnimationEnd) {
        return static_cast<std::uint8_t>(
            base_animation + kMountedAnimationOffset);
    }
    return base_animation;
}

}  // namespace oracle::core
