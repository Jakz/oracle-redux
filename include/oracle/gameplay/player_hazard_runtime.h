#pragma once

#include <cstdint>
#include <optional>

#include "oracle/content/room_tile_types.h"
#include "oracle/core/campaign.h"
#include "oracle/gameplay/player_combat_state.h"
#include "oracle/gameplay/player_traversal.h"

namespace oracle::gameplay {

enum class PlayerHazardPhase : std::uint8_t {
    normal,
    hole_pull,
    hole_fall,
    water_entry,
    swimming,
    drowning,
    hidden_respawn_delay,
    recovery,
};

enum class PlayerWaterCapability : std::uint8_t {
    none,
    flippers,
    mermaid_suit,
};

struct PlayerHazardStepReport {
    PlayerHazardPhase phase{PlayerHazardPhase::normal};
    std::optional<std::uint8_t> link_frame;
    std::uint8_t standing_on_tile_counter{};
    bool entered_hole{};
    bool began_fall{};
    bool entered_water{};
    bool began_swimming{};
    bool began_drowning{};
    bool became_hidden{};
    bool respawned{};
    bool damaged{};
    bool fatal_damage{};
    bool recovered{};
    bool captures_input{};
    bool visible{true};
};

// Native recreation of the ordinary-hole and top-down water branches in
// linkApplyTileTypes. Dungeon warp holes, diving, underwater transitions, and
// side-scrolling water intentionally remain owned by separate policies.
class PlayerHazardRuntime {
public:
    explicit PlayerHazardRuntime(
        core::Campaign campaign = core::Campaign::ages) noexcept;

    void reset(const PlayerState& respawn_point) noexcept;
    void set_respawn_point(const PlayerState& player) noexcept;
    void set_water_capability(PlayerWaterCapability capability) noexcept;

    [[nodiscard]] PlayerHazardStepReport update(
        PlayerState& player,
        PlayerCombatState& combat,
        std::optional<content::LinkTileContact> contact,
        bool landed_this_tick = false) noexcept;

    [[nodiscard]] PlayerHazardPhase phase() const noexcept;
    [[nodiscard]] bool captures_input() const noexcept;
    [[nodiscard]] bool swimming() const noexcept;
    [[nodiscard]] bool visible() const noexcept;
    [[nodiscard]] MovementInput movement_input(
        MovementInput requested,
        PlayerFacing facing) const noexcept;
    [[nodiscard]] double movement_speed_pixels_per_second() const noexcept;
    [[nodiscard]] PlayerWaterCapability water_capability() const noexcept;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;

private:
    [[nodiscard]] PlayerHazardStepReport report() const noexcept;

    core::Campaign campaign_{};
    PlayerWaterCapability water_capability_{};
    PlayerState respawn_point_{};
    PlayerHazardPhase phase_{PlayerHazardPhase::normal};
    std::optional<content::LinkTileContact> last_contact_;
    std::uint8_t standing_counter_{};
    std::uint8_t phase_counter_{};
    std::uint8_t fall_tick_{};
    std::uint8_t swim_tick_{};
    std::uint8_t drown_tick_{};
    bool visible_{true};
};

[[nodiscard]] const char* player_hazard_phase_name(
    PlayerHazardPhase phase) noexcept;

}  // namespace oracle::gameplay
