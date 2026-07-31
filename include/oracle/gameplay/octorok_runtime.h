#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

#include "oracle/content/enemy_data.h"
#include "oracle/content/part_data.h"
#include "oracle/content/room_collisions.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_combat_state.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/gameplay/sword_runtime.h"

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

enum class OctorokProjectilePhase : std::uint8_t {
    flying = 0x01,
    impact = 0x02,
    bouncing = 0x03,
};

enum class OctorokAftermathKind : std::uint8_t {
    enemy_destroyed,
    item_drop,
};

struct OctorokAftermathVisual {
    OctorokAftermathKind kind{};
    std::uint8_t oam_index{};
    double elevation{};
    bool visible{true};
};

struct OctorokStepReport {
    std::uint8_t contacts{};
    std::uint8_t enemies_hit{};
    std::uint8_t enemies_defeated{};
    std::uint8_t projectiles_requested{};
    std::uint8_t projectiles_spawned{};
    std::uint8_t projectile_impacts{};
    std::uint8_t projectile_contacts{};
    std::uint8_t projectiles_expired{};
    std::uint8_t death_puffs_spawned{};
    std::uint8_t item_drops_spawned{};
    std::uint8_t item_drops_collected{};
    std::uint8_t item_drops_expired{};
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
        const PlayerState& player,
        PlayerCombatState& combat,
        core::ActorSlotDomain& actors,
        const EnemyCollisionLookup& collision_lookup,
        const SwordStepReport& sword = {});

    [[nodiscard]] std::optional<std::uint8_t> animation_index(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::uint64_t animation_tick(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::optional<OctorokPhase> phase(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::optional<OctorokProjectilePhase>
    projectile_phase(core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] double projectile_elevation(
        core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] std::optional<OctorokAftermathVisual>
    aftermath_visual(core::ActorSlotHandle actor) const noexcept;
    [[nodiscard]] bool hit_flash(
        core::ActorSlotHandle actor) const noexcept;
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
        std::uint8_t knockback_counter{};
        std::uint8_t knockback_angle{};
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

    struct ProjectileRuntime {
        std::uint32_t generation{};
        OctorokProjectilePhase phase{
            OctorokProjectilePhase::flying};
        std::uint8_t counter{};
        std::uint8_t angle{};
        std::int32_t speed_subpixels{};
        std::int32_t subpixel_x{};
        std::int32_t subpixel_y{};
        std::int32_t elevation_subpixels{};
        std::int32_t vertical_velocity_subpixels{};
        bool initialized{};
    };

    enum class AftermathPhase : std::uint8_t {
        enemy_destroyed,
        item_drop_bouncing,
        item_drop_waiting,
    };

    struct AftermathRuntime {
        std::uint32_t generation{};
        AftermathPhase phase{AftermathPhase::enemy_destroyed};
        std::uint16_t counter{};
        std::uint8_t animation_tick{};
        std::int32_t elevation_subpixels{};
        std::int32_t vertical_velocity_subpixels{};
        bool initialized{};
    };

    [[nodiscard]] RandomStep next_random() noexcept;
    void initialize_actor(
        std::size_t slot,
        core::ActorSlotState& actor);
    void initialize_projectile(
        std::size_t slot,
        core::ActorSlotState& actor);
    [[nodiscard]] bool spawn_projectile(
        const core::ActorSlotState& source,
        std::uint8_t angle,
        core::ActorSlotDomain& actors);
    void update_projectiles(
        const PlayerState& player,
        PlayerCombatState& combat,
        core::ActorSlotDomain& actors,
        const EnemyCollisionLookup& collision_lookup,
        OctorokStepReport& report);
    void update_aftermath(
        const PlayerState& player,
        PlayerCombatState& combat,
        core::ActorSlotDomain& actors,
        OctorokStepReport& report);
    [[nodiscard]] bool spawn_death_puff(
        const core::ActorSlotState& source,
        core::ActorSlotDomain& actors);
    [[nodiscard]] std::optional<std::uint8_t>
    decide_octorok_drop() noexcept;
    [[nodiscard]] bool move_actor(
        core::ActorSlotState& actor,
        ActorRuntime& runtime,
        const EnemyCollisionLookup& collision_lookup);
    [[nodiscard]] bool move_knockback(
        core::ActorSlotState& actor,
        ActorRuntime& runtime,
        const EnemyCollisionLookup& collision_lookup);
    [[nodiscard]] std::uint8_t cardinal_angle_to_player(
        const core::ActorSlotState& actor,
        const PlayerState& player) const noexcept;

    const content::RomSource& rom_;
    content::EnemyDefinitionDecoder definitions_;
    content::PartDefinitionDecoder part_definitions_;
    std::array<ActorRuntime, core::ActorSlotDomain::slots_per_category>
        actor_runtime_{};
    std::array<
        ProjectileRuntime,
        core::ActorSlotDomain::slots_per_category>
        projectile_runtime_{};
    std::array<
        AftermathRuntime,
        core::ActorSlotDomain::slots_per_category>
        aftermath_runtime_{};
    std::uint8_t rng_low_{};
    std::uint8_t rng_high_{};
    std::uint64_t frame_counter_{};
};

}  // namespace oracle::gameplay
