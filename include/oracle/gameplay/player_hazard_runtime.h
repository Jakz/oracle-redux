#pragma once

#include <cstdint>
#include <optional>

#include "oracle/content/room_tile_types.h"
#include "oracle/gameplay/player_combat_state.h"
#include "oracle/gameplay/player_traversal.h"

namespace oracle::gameplay {

enum class PlayerHazardPhase : std::uint8_t {
    normal,
    hole_pull,
    hole_fall,
    hidden_respawn_delay,
    recovery,
};

struct PlayerHazardStepReport {
    PlayerHazardPhase phase{PlayerHazardPhase::normal};
    std::optional<std::uint8_t> link_frame;
    std::uint8_t standing_on_tile_counter{};
    bool entered_hole{};
    bool began_fall{};
    bool became_hidden{};
    bool respawned{};
    bool damaged{};
    bool fatal_damage{};
    bool recovered{};
    bool captures_input{};
    bool visible{true};
};

// Native recreation of the ordinary-hole branch in linkApplyTileTypes and
// LINK_STATE_RESPAWNING parameter 0. Dungeon warp holes intentionally remain
// topology-owned and are not treated as ordinary holes here.
class PlayerHazardRuntime {
public:
    void reset(const PlayerState& respawn_point) noexcept;
    void set_respawn_point(const PlayerState& player) noexcept;

    [[nodiscard]] PlayerHazardStepReport update(
        PlayerState& player,
        PlayerCombatState& combat,
        std::optional<content::LinkTileContact> contact,
        bool landed_this_tick = false) noexcept;

    [[nodiscard]] PlayerHazardPhase phase() const noexcept;
    [[nodiscard]] bool captures_input() const noexcept;
    [[nodiscard]] bool visible() const noexcept;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;

private:
    [[nodiscard]] PlayerHazardStepReport report() const noexcept;

    PlayerState respawn_point_{};
    PlayerHazardPhase phase_{PlayerHazardPhase::normal};
    std::optional<content::LinkTileContact> last_contact_;
    std::uint8_t standing_counter_{};
    std::uint8_t phase_counter_{};
    std::uint8_t fall_tick_{};
    bool visible_{true};
};

[[nodiscard]] const char* player_hazard_phase_name(
    PlayerHazardPhase phase) noexcept;

}  // namespace oracle::gameplay
