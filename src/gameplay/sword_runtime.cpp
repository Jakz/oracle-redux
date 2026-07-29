#include "oracle/gameplay/sword_runtime.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t sword_item_id = 0x05;
constexpr std::uint8_t sword_parent_slot = 0x02;
constexpr std::uint8_t sword_weapon_slot = 0x06;
constexpr std::uint8_t swing_duration_ticks = 17;

struct AnimationStep {
    std::uint8_t first_tick{};
    std::uint8_t link_frame_base{};
    std::uint8_t parameter{};
};

// LINK_ANIM_MODE_22 starts at animationData19fef / animationData19d48.
// Its final $86 parameter marks completion and is not a damaging frame.
constexpr std::array swing_animation{
    AnimationStep{0, 0xac, 0x00},
    AnimationStep{3, 0xb0, 0x02},
    AnimationStep{6, 0xb4, 0x64},
    AnimationStep{14, 0xb0, 0x06},
};

struct ArcRecord {
    std::uint8_t radius_y{};
    std::uint8_t radius_x{};
    std::int8_t offset_y{};
    std::int8_t offset_x{};
};

constexpr std::array<ArcRecord, 16> ordinary_sword_arc{{
    {0x09, 0x06, -2, 16},
    {0x06, 0x09, -14, 0},
    {0x09, 0x06, 0, -15},
    {0x06, 0x09, -14, 0},
    {0x07, 0x07, -11, 13},
    {0x07, 0x07, -11, 13},
    {0x07, 0x07, 17, -13},
    {0x07, 0x07, -11, -13},
    {0x09, 0x06, -17, -4},
    {0x06, 0x09, 2, 19},
    {0x09, 0x06, 21, 3},
    {0x06, 0x09, 2, -19},
    {0x09, 0x06, -10, -4},
    {0x04, 0x09, 2, 12},
    {0x09, 0x06, 16, 3},
    {0x06, 0x09, 2, -12},
}};

// updateSwingableItemAnimation's packed high-nibble arc / low-bit
// animation table for parameters below $10.
constexpr std::array<std::uint8_t, 16> swing_mapping{{
    0x02, 0x41, 0x80, 0xc0,
    0x10, 0x51, 0x92, 0xd2,
    0x26, 0x65, 0xa4, 0xe4,
    0x30, 0x77, 0xb6, 0xf6,
}};

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

AnimationStep animation_at(const std::uint8_t tick) noexcept {
    auto result = swing_animation.front();
    for (const auto step : swing_animation) {
        if (tick < step.first_tick) {
            break;
        }
        result = step;
    }
    return result;
}

}  // namespace

void SwordRuntime::reset() noexcept {
    parent_actor_.reset();
    weapon_actor_.reset();
    facing_ = PlayerFacing::south;
    animation_tick_ = 0;
}

SwordStepReport SwordRuntime::update(
    const input::InputFrame& input,
    const PlayerState& player,
    core::ActorSlotDomain& actors) {
    SwordStepReport report;
    if (
        (
            parent_actor_.has_value() &&
            actors.get(*parent_actor_) == nullptr) ||
        (
            weapon_actor_.has_value() &&
            actors.get(*weapon_actor_) == nullptr)) {
        if (parent_actor_.has_value()) {
            (void)actors.release(*parent_actor_);
        }
        if (weapon_actor_.has_value()) {
            (void)actors.release(*weapon_actor_);
        }
        parent_actor_.reset();
        weapon_actor_.reset();
        animation_tick_ = 0;
    }
    if (
        parent_actor_.has_value() &&
        animation_tick_ >= swing_duration_ticks) {
        (void)actors.release(*parent_actor_);
        (void)actors.release(*weapon_actor_);
        parent_actor_.reset();
        weapon_actor_.reset();
        animation_tick_ = 0;
        report.ended = true;
    }

    if (
        !parent_actor_.has_value() &&
        input.pressed(input::InputAction::b)) {
        parent_actor_ = actors.allocate_at(
            core::ActorCategory::item,
            sword_parent_slot,
            core::ActorIdentity{.id = sword_item_id},
            player.room);
        if (parent_actor_.has_value()) {
            weapon_actor_ = actors.allocate_at(
                core::ActorCategory::item,
                sword_weapon_slot,
                core::ActorIdentity{.id = sword_item_id},
                player.room);
        }
        if (parent_actor_.has_value() && weapon_actor_.has_value()) {
            facing_ = player.facing;
            animation_tick_ = 0;
            report.started = true;
        } else {
            if (parent_actor_.has_value()) {
                (void)actors.release(*parent_actor_);
            }
            parent_actor_.reset();
            weapon_actor_.reset();
        }
    }
    if (!parent_actor_.has_value()) {
        return report;
    }

    const auto direction = direction_index(facing_);
    const auto animation = animation_at(animation_tick_);
    const auto phase =
        static_cast<std::uint8_t>(
            (animation.parameter & 0x1f) >> 1u);
    const auto packed =
        swing_mapping[
            static_cast<std::size_t>(direction) * 4 + phase];
    const auto arc_index =
        static_cast<std::uint8_t>(packed >> 4u);
    const auto item_animation =
        static_cast<std::uint8_t>(packed & 0x07);
    const auto arc = ordinary_sword_arc[arc_index];
    const auto center_x =
        player.local_x + static_cast<double>(arc.offset_x);
    const auto center_y =
        player.local_y + static_cast<double>(arc.offset_y);

    auto* actor = actors.get(*weapon_actor_);
    if (actor != nullptr) {
        actor->room = player.room;
        actor->local_x =
            static_cast<std::int16_t>(std::lround(center_x));
        actor->local_y =
            static_cast<std::int16_t>(std::lround(center_y));
        actor->positioned = true;
        actor->collision_radius_y = arc.radius_y;
        actor->collision_radius_x = arc.radius_x;
    }

    report.hitbox = SwordHitbox{
        .room = player.room,
        .center_x = center_x,
        .center_y = center_y,
        .half_width = static_cast<double>(arc.radius_x),
        .half_height = static_cast<double>(arc.radius_y),
    };
    report.pose = SwordPose{
        .actor = *weapon_actor_,
        .room = player.room,
        .local_x = center_x,
        .local_y = center_y,
        .arc_index = arc_index,
        .animation_index = item_animation,
        .link_frame =
            static_cast<std::uint8_t>(
                animation.link_frame_base + direction),
        .animation_parameter = animation.parameter,
    };
    ++animation_tick_;
    return report;
}

bool SwordRuntime::active() const noexcept {
    return parent_actor_.has_value();
}

std::uint64_t SwordRuntime::deterministic_state() const noexcept {
    constexpr std::uint64_t basis = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    auto result = basis;
    const auto append = [&](const std::uint64_t value) {
        result ^= value;
        result *= prime;
    };
    append(parent_actor_.has_value());
    if (parent_actor_.has_value()) {
        append(parent_actor_->slot);
        append(parent_actor_->generation);
        append(weapon_actor_->slot);
        append(weapon_actor_->generation);
    }
    append(direction_index(facing_));
    append(animation_tick_);
    return result;
}

}  // namespace oracle::gameplay
