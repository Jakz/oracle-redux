#pragma once

#include <cstdint>
#include <optional>

#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/input/input_frame.h"

namespace oracle::gameplay {

struct SwordHitbox {
    core::WorldRoomId room;
    double center_x{};
    double center_y{};
    double half_width{};
    double half_height{};
};

struct SwordPose {
    core::ActorSlotHandle actor;
    core::WorldRoomId room;
    double local_x{};
    double local_y{};
    std::uint8_t arc_index{};
    std::uint8_t animation_index{};
    std::uint8_t link_frame{};
    std::uint8_t animation_parameter{};
};

struct SwordStepReport {
    std::optional<SwordHitbox> hitbox;
    std::optional<SwordPose> pose;
    bool started{};
    bool ended{};
};

// Native reimplementation of the ordinary ITEM_SWORD parent/child path.
// It reserves original parent slot 2 and weapon slot 6, follows
// LINK_ANIM_MODE_22 timing, and positions the child from swordArcData.
class SwordRuntime {
public:
    void reset() noexcept;

    [[nodiscard]] SwordStepReport update(
        const input::InputFrame& input,
        const PlayerState& player,
        core::ActorSlotDomain& actors);

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;

private:
    std::optional<core::ActorSlotHandle> parent_actor_;
    std::optional<core::ActorSlotHandle> weapon_actor_;
    PlayerFacing facing_{PlayerFacing::south};
    std::uint8_t animation_tick_{};
};

}  // namespace oracle::gameplay
