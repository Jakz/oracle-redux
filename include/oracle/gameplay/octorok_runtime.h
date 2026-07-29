#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

#include "oracle/content/enemy_data.h"
#include "oracle/content/room_collisions.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/input/input_frame.h"

namespace oracle::gameplay {

struct OctorokScenarioDefinition {
    core::WorldRoomId room;
    std::uint8_t player_spawn_yx{};
};

[[nodiscard]] OctorokScenarioDefinition octorok_scenario(
    core::Campaign campaign) noexcept;

enum class OctorokPhase : std::uint8_t {
    deciding = 0x08,
    standing = 0x09,
    walking = 0x0a,
    shooting = 0x0b,
};

struct PlayerCombatState {
    std::uint8_t maximum_health{12};
    std::uint8_t health{12};
    std::uint8_t invincibility_ticks{};
};

struct SwordHitbox {
    core::WorldRoomId room;
    double center_x{};
    double center_y{};
    double half_width{};
    double half_height{};
};

struct OctorokStepReport {
    std::uint8_t contacts{};
    std::uint8_t enemies_hit{};
    std::uint8_t enemies_defeated{};
    std::uint8_t projectiles_requested{};
    bool sword_started{};
};

using EnemyCollisionLookup = std::function<
    const content::RoomCollisionMap*(core::WorldRoomId)>;

// Native, bounded reimplementation of the shared ENEMY_OCTOROK state machine.
// It preserves the retail RNG transition and state counters. Projectile
// requests are emitted but not allocated until the projectile slice.
class OctorokRuntime {
public:
    explicit OctorokRuntime(
        const content::RomSource& rom,
        std::uint16_t rng_seed = 0x5a17);

    void reset(std::uint16_t rng_seed = 0x5a17) noexcept;

    [[nodiscard]] OctorokStepReport update(
        const input::InputFrame& input,
        const PlayerState& player,
        PlayerCombatState& combat,
        core::ActorSlotDomain& actors,
        const EnemyCollisionLookup& collision_lookup);

    [[nodiscard]] std::optional<std::uint8_t> animation_index(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::uint64_t animation_tick(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::optional<OctorokPhase> phase(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::optional<SwordHitbox> sword_hitbox(
        const PlayerState& player) const noexcept;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;

private:
    struct ActorRuntime {
        std::uint32_t generation{};
        OctorokPhase phase{OctorokPhase::deciding};
        std::uint8_t counter{};
        std::uint8_t walk_counter{};
        std::uint8_t decision_mask{};
        std::uint8_t angle{};
        std::uint8_t hit_invincibility{};
        std::int16_t subpixel_x{};
        std::int16_t subpixel_y{};
        std::uint64_t animation_tick{};
        bool initialized{};
        bool hit_this_swing{};
    };

    struct RandomStep {
        std::uint8_t value{};
        std::uint8_t intermediate_high{};
        std::uint8_t intermediate_low{};
    };

    [[nodiscard]] RandomStep next_random() noexcept;
    void initialize_actor(
        std::size_t slot,
        core::ActorSlotState& actor);
    [[nodiscard]] bool move_actor(
        core::ActorSlotState& actor,
        ActorRuntime& runtime,
        const EnemyCollisionLookup& collision_lookup);
    [[nodiscard]] std::uint8_t cardinal_angle_to_player(
        const core::ActorSlotState& actor,
        const PlayerState& player) const noexcept;

    const content::RomSource& rom_;
    content::EnemyDefinitionDecoder definitions_;
    std::array<ActorRuntime, core::ActorSlotDomain::slots_per_category>
        actor_runtime_{};
    std::uint8_t rng_low_{};
    std::uint8_t rng_high_{};
    std::uint8_t sword_ticks_{};
};

}  // namespace oracle::gameplay
