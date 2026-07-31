#pragma once

#include <cstdint>
#include <optional>

#include "oracle/core/actor_slot_domain.h"
#include "oracle/core/campaign.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/input/input_frame.h"

namespace oracle::gameplay {

struct FeatherStepReport {
    bool started{};
    bool landed{};
    bool rejected{};
    bool in_air{};
    std::optional<std::uint8_t> link_frame;
    std::int32_t z_subpixels{};
    std::int32_t speed_z_subpixels{};
    double visual_elevation{};
};

// Native level-one ITEM_FEATHER ($17) parent path for top-down rooms. The
// runtime preserves wLinkInAir and the original signed 8.8 Z integration.
// Seasons' level-two Roc's Cape continuation and side-scrolling physics are
// deliberately separate future policies.
class FeatherRuntime {
public:
    explicit FeatherRuntime(core::Campaign campaign) noexcept;

    void reset() noexcept;

    [[nodiscard]] FeatherStepReport update(
        const input::InputFrame& input,
        PlayerState& player,
        core::ActorSlotDomain& actors,
        bool side_scrolling = false);

    [[nodiscard]] static double visual_elevation(
        const PlayerState& player) noexcept;
    [[nodiscard]] static bool active(const PlayerState& player) noexcept;
    [[nodiscard]] std::uint64_t deterministic_state(
        const PlayerState& player) const noexcept;

private:
    core::Campaign campaign_;
    std::uint8_t animation_tick_{};
};

}  // namespace oracle::gameplay
