#include "oracle/gameplay/feather_runtime.h"

#include <algorithm>

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t feather_item_id = 0x17;
constexpr std::uint8_t feather_parent_slot = 0x01;
constexpr std::int32_t top_down_jump_speed = -0x01e0;
constexpr std::int32_t top_down_gravity = 0x0020;
constexpr std::int32_t maximum_fall_speed = 0x0300;
constexpr double subpixels_per_pixel = 256.0;

std::uint8_t direction_index(const PlayerFacing facing) noexcept {
    switch (facing) {
    case PlayerFacing::north:
        return 0;
    case PlayerFacing::east:
        return 1;
    case PlayerFacing::south:
        return 2;
    case PlayerFacing::west:
        return 3;
    }
    return 0;
}

std::uint8_t jump_frame_base(
    const core::Campaign campaign,
    const std::uint8_t tick) noexcept {
    // LINK_ANIM_MODE_JUMP points to animationData19f78 in Ages and
    // animationData19cd6 in Seasons. Their three frames last 9, 9, and 6
    // ticks; the final pose is held until Z reaches the ground plane.
    const auto first = campaign == core::Campaign::ages
        ? std::uint8_t{0xe4}
        : std::uint8_t{0xd8};
    if (tick < 9) {
        return first;
    }
    if (tick < 18) {
        return static_cast<std::uint8_t>(first + 4);
    }
    return static_cast<std::uint8_t>(first + 8);
}

}  // namespace

FeatherRuntime::FeatherRuntime(const core::Campaign campaign) noexcept
    : campaign_{campaign} {}

void FeatherRuntime::reset() noexcept {
    animation_tick_ = 0;
}

FeatherStepReport FeatherRuntime::update(
    const input::InputFrame& input,
    PlayerState& player,
    core::ActorSlotDomain& actors,
    const bool side_scrolling) {
    FeatherStepReport report;
    if (
        input.pressed(input::InputAction::a) &&
        !active(player)) {
        if (side_scrolling) {
            report.rejected = true;
        } else {
            const auto parent = actors.allocate_at(
                core::ActorCategory::item,
                feather_parent_slot,
                core::ActorIdentity{.id = feather_item_id},
                player.room);
            if (!parent.has_value()) {
                report.rejected = true;
            } else {
                player.speed_z_subpixels = top_down_jump_speed;
                player.in_air = 1;
                animation_tick_ = 0;
                report.started = true;

                // Level-one top-down use clears its parent on the same retail
                // update after it has initialized Link's airborne state.
                (void)actors.release(*parent);
            }
        }
    }

    if (player.in_air == 1) {
        player.in_air = 2;
    }
    if (player.in_air == 2) {
        player.z_subpixels += player.speed_z_subpixels;
        if (player.z_subpixels >= 0) {
            player.z_subpixels = 0;
            player.speed_z_subpixels = 0;
            player.in_air = 0;
            animation_tick_ = 0;
            report.landed = true;
        } else {
            player.speed_z_subpixels = std::min(
                maximum_fall_speed,
                player.speed_z_subpixels + top_down_gravity);
            report.link_frame = static_cast<std::uint8_t>(
                jump_frame_base(campaign_, animation_tick_) +
                direction_index(player.facing));
            ++animation_tick_;
        }
    }

    report.in_air = active(player);
    report.z_subpixels = player.z_subpixels;
    report.speed_z_subpixels = player.speed_z_subpixels;
    report.visual_elevation = visual_elevation(player);
    return report;
}

double FeatherRuntime::visual_elevation(
    const PlayerState& player) noexcept {
    return std::max(
        0.0,
        -static_cast<double>(player.z_subpixels) /
            subpixels_per_pixel);
}

bool FeatherRuntime::active(const PlayerState& player) noexcept {
    return player.in_air != 0 || player.z_subpixels < 0;
}

std::uint64_t FeatherRuntime::deterministic_state(
    const PlayerState& player) const noexcept {
    constexpr std::uint64_t basis = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    auto result = basis;
    const auto append = [&](const std::uint64_t value) {
        result ^= value;
        result *= prime;
    };
    append(static_cast<std::uint8_t>(campaign_));
    append(animation_tick_);
    append(static_cast<std::uint32_t>(player.z_subpixels));
    append(static_cast<std::uint32_t>(player.speed_z_subpixels));
    append(player.in_air);
    append(direction_index(player.facing));
    return result;
}

}  // namespace oracle::gameplay
