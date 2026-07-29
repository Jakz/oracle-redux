#include "oracle/gameplay/octorok_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "oracle/content/room_layout.h"

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t octorok_enemy_id = 0x09;
constexpr std::array<std::uint8_t, 8> counter_values{
    30, 45, 60, 75, 45, 60, 75, 90,
};
constexpr std::array<std::uint8_t, 4> walk_counter_values{
    0x19, 0x21, 0x29, 0x31,
};
constexpr std::array<std::uint8_t, 5> decision_masks{
    0x07, 0x07, 0x03, 0x03, 0x01,
};
constexpr std::uint8_t sword_duration_ticks = 8;
constexpr std::uint8_t player_damage_invincibility_ticks = 60;

bool shares_actor_space(
    const core::WorldRoomId left,
    const core::WorldRoomId right) noexcept {
    if (left.area != right.area) {
        return false;
    }
    return left.area < 4 || left.room == right.room;
}

double room_world_x(
    const core::WorldRoomId room,
    const double local_x) noexcept {
    if (room.area >= 4) {
        return local_x;
    }
    return
        static_cast<double>(room.room & 0x0f) *
            content::small_room_world_width +
        local_x;
}

double room_world_y(
    const core::WorldRoomId room,
    const double local_y) noexcept {
    if (room.area >= 4) {
        return local_y;
    }
    return
        static_cast<double>((room.room >> 4u) & 0x0f) *
            content::small_room_world_height +
        local_y;
}

bool overlaps_player(
    const core::ActorSlotState& actor,
    const PlayerState& player) noexcept {
    if (!shares_actor_space(actor.room, player.room)) {
        return false;
    }
    constexpr double link_collision_radius = 6.0;
    const auto delta_x = std::abs(
        room_world_x(actor.room, actor.local_x) -
        room_world_x(player.room, player.local_x));
    const auto delta_y = std::abs(
        room_world_y(actor.room, actor.local_y) -
        room_world_y(player.room, player.local_y));
    return
        delta_x <
            link_collision_radius + actor.collision_radius_x &&
        delta_y <
            link_collision_radius + actor.collision_radius_y;
}

bool overlaps_sword(
    const core::ActorSlotState& actor,
    const SwordHitbox& sword) noexcept {
    if (!shares_actor_space(actor.room, sword.room)) {
        return false;
    }
    const auto delta_x = std::abs(
        room_world_x(actor.room, actor.local_x) -
        room_world_x(sword.room, sword.center_x));
    const auto delta_y = std::abs(
        room_world_y(actor.room, actor.local_y) -
        room_world_y(sword.room, sword.center_y));
    return
        delta_x <
            sword.half_width + actor.collision_radius_x &&
        delta_y <
            sword.half_height + actor.collision_radius_y;
}

bool enemy_position_is_clear(
    const core::ActorSlotState& actor,
    const std::int16_t x,
    const std::int16_t y,
    const EnemyCollisionLookup& collision_lookup) {
    const auto* collisions = collision_lookup(actor.room);
    if (collisions == nullptr) {
        return false;
    }
    constexpr std::array<std::pair<int, int>, 4> samples{
        std::pair{-7, -7},
        std::pair{7, -7},
        std::pair{-7, 7},
        std::pair{7, 7},
    };
    const auto width = static_cast<int>(
        collisions->columns * content::metatile_world_size);
    const auto height = static_cast<int>(
        collisions->rows * content::metatile_world_size);
    for (const auto [offset_x, offset_y] : samples) {
        const auto sample_x = static_cast<int>(x) + offset_x;
        const auto sample_y = static_cast<int>(y) + offset_y;
        if (
            sample_x < 0 ||
            sample_y < 0 ||
            sample_x >= width ||
            sample_y >= height) {
            return false;
        }
        const auto pixel_x = static_cast<std::size_t>(sample_x);
        const auto pixel_y = static_cast<std::size_t>(sample_y);
        const auto column =
            pixel_x /
            static_cast<std::size_t>(content::metatile_world_size);
        const auto row =
            pixel_y /
            static_cast<std::size_t>(content::metatile_world_size);
        const auto local_x = static_cast<std::uint8_t>(
            pixel_x %
            static_cast<std::size_t>(content::metatile_world_size));
        const auto local_y = static_cast<std::uint8_t>(
            pixel_y %
            static_cast<std::size_t>(content::metatile_world_size));
        if (content::RoomCollisionDecoder::is_solid(
                collisions->at(column, row),
                local_x,
                local_y,
                content::CollisionProfile::
                    grounded_actor_without_small_bridges)) {
            return false;
        }
    }
    return true;
}

}  // namespace

OctorokScenarioDefinition octorok_scenario(
    const core::Campaign campaign) noexcept {
    if (campaign == core::Campaign::ages) {
        // group0Map64EnemyObjectData: ENEMY_OCTOROK $00 at $48,$48.
        return OctorokScenarioDefinition{
            core::WorldRoomId{0, 0x64},
            0x64,
        };
    }
    // group0Mapa6EnemyObjectData: ENEMY_OCTOROK $00 at $48,$68.
    return OctorokScenarioDefinition{
        core::WorldRoomId{0, 0xa6},
        0x66,
    };
}

OctorokRuntime::OctorokRuntime(
    const content::RomSource& rom,
    const std::uint16_t rng_seed)
    : rom_{rom}, definitions_{rom} {
    reset(rng_seed);
}

void OctorokRuntime::reset(const std::uint16_t rng_seed) noexcept {
    actor_runtime_ = {};
    rng_low_ = static_cast<std::uint8_t>(rng_seed & 0xff);
    rng_high_ = static_cast<std::uint8_t>(rng_seed >> 8u);
    sword_ticks_ = 0;
}

OctorokRuntime::RandomStep OctorokRuntime::next_random() noexcept {
    // Exact getRandomNumber_noPreserveVars transition:
    // HL=(hRng2:hRng1)*3, hRng2=H, hRng1=H+old hRng1.
    const auto previous_low = rng_low_;
    const auto seed = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(rng_high_) << 8u) |
        rng_low_);
    const auto intermediate =
        static_cast<std::uint16_t>(seed * 3u);
    const auto intermediate_high =
        static_cast<std::uint8_t>(intermediate >> 8u);
    const auto intermediate_low =
        static_cast<std::uint8_t>(intermediate & 0xff);
    rng_high_ = intermediate_high;
    rng_low_ = static_cast<std::uint8_t>(
        intermediate_high + previous_low);
    return RandomStep{
        rng_low_,
        intermediate_high,
        intermediate_low,
    };
}

void OctorokRuntime::initialize_actor(
    const std::size_t slot,
    core::ActorSlotState& actor) {
    const auto definition = definitions_.decode(
        actor.identity.id,
        actor.identity.subid);
    actor.collision_radius_y = definition.collision_radius_y;
    actor.collision_radius_x = definition.collision_radius_x;
    actor.maximum_health = definition.health;
    actor.health = definition.health;
    actor.contact_damage = definition.contact_damage;
    // Enemy contact damage is intentionally not a solid-body collision.
    actor.blocks_player = false;

    auto& runtime = actor_runtime_[slot];
    runtime = ActorRuntime{};
    runtime.generation = actor.generation;
    runtime.initialized = true;
    runtime.phase = OctorokPhase::walking;
    runtime.decision_mask =
        decision_masks[
            std::min<std::size_t>(
                actor.identity.subid,
                decision_masks.size() - 1)];

    const auto random = next_random();
    runtime.counter =
        counter_values[random.value & runtime.decision_mask];
    runtime.angle =
        static_cast<std::uint8_t>(
            random.intermediate_high & 0x18);
    runtime.walk_counter =
        walk_counter_values[random.intermediate_low & 0x03];
}

bool OctorokRuntime::move_actor(
    core::ActorSlotState& actor,
    ActorRuntime& runtime,
    const EnemyCollisionLookup& collision_lookup) {
    const auto speed =
        (actor.identity.subid & 0x02) != 0 ? 192 : 128;
    int velocity_x = 0;
    int velocity_y = 0;
    switch (runtime.angle & 0x18) {
    case 0x00:
        velocity_y = -speed;
        break;
    case 0x08:
        velocity_x = speed;
        break;
    case 0x10:
        velocity_y = speed;
        break;
    case 0x18:
        velocity_x = -speed;
        break;
    }
    runtime.subpixel_x = static_cast<std::int16_t>(
        runtime.subpixel_x + velocity_x);
    runtime.subpixel_y = static_cast<std::int16_t>(
        runtime.subpixel_y + velocity_y);

    int delta_x = 0;
    int delta_y = 0;
    while (runtime.subpixel_x >= 256) {
        ++delta_x;
        runtime.subpixel_x -= 256;
    }
    while (runtime.subpixel_x <= -256) {
        --delta_x;
        runtime.subpixel_x += 256;
    }
    while (runtime.subpixel_y >= 256) {
        ++delta_y;
        runtime.subpixel_y -= 256;
    }
    while (runtime.subpixel_y <= -256) {
        --delta_y;
        runtime.subpixel_y += 256;
    }
    if (delta_x == 0 && delta_y == 0) {
        return true;
    }

    const auto candidate_x = static_cast<std::int16_t>(
        actor.local_x + delta_x);
    const auto candidate_y = static_cast<std::int16_t>(
        actor.local_y + delta_y);
    if (!enemy_position_is_clear(
            actor,
            candidate_x,
            candidate_y,
            collision_lookup)) {
        runtime.subpixel_x = 0;
        runtime.subpixel_y = 0;
        return false;
    }
    actor.local_x = candidate_x;
    actor.local_y = candidate_y;
    return true;
}

std::uint8_t OctorokRuntime::cardinal_angle_to_player(
    const core::ActorSlotState& actor,
    const PlayerState& player) const noexcept {
    const auto delta_x =
        room_world_x(player.room, player.local_x) -
        room_world_x(actor.room, actor.local_x);
    const auto delta_y =
        room_world_y(player.room, player.local_y) -
        room_world_y(actor.room, actor.local_y);
    if (std::abs(delta_x) > std::abs(delta_y)) {
        return delta_x >= 0.0 ? 0x08 : 0x18;
    }
    return delta_y >= 0.0 ? 0x10 : 0x00;
}

OctorokStepReport OctorokRuntime::update(
    const input::InputFrame& input,
    const PlayerState& player,
    PlayerCombatState& combat,
    core::ActorSlotDomain& actors,
    const EnemyCollisionLookup& collision_lookup) {
    OctorokStepReport report;
    if (combat.invincibility_ticks != 0) {
        --combat.invincibility_ticks;
    }
    for (auto& runtime : actor_runtime_) {
        if (runtime.hit_invincibility != 0) {
            --runtime.hit_invincibility;
        }
    }
    if (input.pressed(input::InputAction::b)) {
        sword_ticks_ = sword_duration_ticks;
        report.sword_started = true;
        for (auto& runtime : actor_runtime_) {
            runtime.hit_this_swing = false;
        }
    }

    auto enemy_slots = actors.slots(core::ActorCategory::enemy);
    for (std::size_t slot = 0; slot < enemy_slots.size(); ++slot) {
        const auto& immutable_actor = enemy_slots[slot];
        if (
            !immutable_actor.active ||
            !immutable_actor.positioned ||
            immutable_actor.identity.id != octorok_enemy_id) {
            continue;
        }
        const core::ActorSlotHandle handle{
            core::ActorCategory::enemy,
            static_cast<std::uint8_t>(slot),
            immutable_actor.generation,
        };
        auto* actor = actors.get(handle);
        if (actor == nullptr) {
            continue;
        }
        auto& runtime = actor_runtime_[slot];
        if (
            !runtime.initialized ||
            runtime.generation != actor->generation) {
            initialize_actor(slot, *actor);
        }

        switch (runtime.phase) {
        case OctorokPhase::deciding: {
            const auto random = next_random();
            const auto decision =
                static_cast<std::uint8_t>(
                    random.value & runtime.decision_mask);
            if (decision == 0) {
                runtime.phase = OctorokPhase::shooting;
                runtime.counter = 0x10;
                if (actor->identity.subid >= 2) {
                    runtime.angle =
                        cardinal_angle_to_player(*actor, player);
                }
            } else {
                runtime.phase = OctorokPhase::standing;
                runtime.counter = counter_values[decision];
            }
            break;
        }
        case OctorokPhase::standing:
            if (runtime.counter != 0) {
                --runtime.counter;
            }
            if (runtime.counter == 0) {
                runtime.phase = OctorokPhase::walking;
                const auto random = next_random();
                runtime.walk_counter =
                    walk_counter_values[random.value & 0x03];
                runtime.angle = static_cast<std::uint8_t>(
                    random.intermediate_low & 0x18);
                if ((random.intermediate_high & 0x03) == 0) {
                    runtime.angle =
                        cardinal_angle_to_player(*actor, player);
                }
            }
            break;
        case OctorokPhase::walking:
            if (runtime.walk_counter != 0) {
                --runtime.walk_counter;
            }
            if (runtime.walk_counter == 0) {
                runtime.phase = OctorokPhase::deciding;
                break;
            }
            if (!move_actor(*actor, runtime, collision_lookup)) {
                runtime.angle = static_cast<std::uint8_t>(
                    next_random().value & 0x18);
            }
            ++runtime.animation_tick;
            break;
        case OctorokPhase::shooting:
            if (runtime.counter != 0) {
                --runtime.counter;
            }
            if (runtime.counter == 0) {
                runtime.phase = OctorokPhase::standing;
                runtime.counter = 0x20;
                ++report.projectiles_requested;
            }
            break;
        }
    }

    const auto sword = sword_hitbox(player);
    if (sword.has_value()) {
        for (std::size_t slot = 0; slot < enemy_slots.size(); ++slot) {
            const auto& immutable_actor = enemy_slots[slot];
            if (
                !immutable_actor.active ||
                immutable_actor.identity.id != octorok_enemy_id) {
                continue;
            }
            const core::ActorSlotHandle handle{
                core::ActorCategory::enemy,
                static_cast<std::uint8_t>(slot),
                immutable_actor.generation,
            };
            auto* actor = actors.get(handle);
            auto& runtime = actor_runtime_[slot];
            if (
                actor == nullptr ||
                runtime.hit_this_swing ||
                runtime.hit_invincibility != 0 ||
                !overlaps_sword(*actor, *sword)) {
                continue;
            }
            runtime.hit_this_swing = true;
            runtime.hit_invincibility = 12;
            ++report.enemies_hit;
            if (actor->health > 0) {
                --actor->health;
            }
            if (actor->health == 0) {
                (void)actors.release(handle);
                ++report.enemies_defeated;
            }
        }
    }

    enemy_slots = actors.slots(core::ActorCategory::enemy);
    if (combat.invincibility_ticks == 0 && combat.health != 0) {
        for (const auto& actor : enemy_slots) {
            if (
                !actor.active ||
                actor.identity.id != octorok_enemy_id ||
                !overlaps_player(actor, player)) {
                continue;
            }
            const auto damage = static_cast<std::uint8_t>(
                std::min(
                    0x7f,
                    std::abs(static_cast<int>(actor.contact_damage))));
            combat.health = damage >= combat.health
                ? 0
                : static_cast<std::uint8_t>(combat.health - damage);
            combat.invincibility_ticks =
                player_damage_invincibility_ticks;
            ++report.contacts;
            break;
        }
    }
    if (sword_ticks_ != 0) {
        --sword_ticks_;
    }
    return report;
}

std::optional<std::uint8_t> OctorokRuntime::animation_index(
    const core::ActorSlotHandle actor) const noexcept {
    if (
        actor.category != core::ActorCategory::enemy ||
        actor.slot >= actor_runtime_.size()) {
        return std::nullopt;
    }
    const auto& runtime = actor_runtime_[actor.slot];
    if (!runtime.initialized || runtime.generation != actor.generation) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>((runtime.angle & 0x18) >> 3u);
}

std::uint64_t OctorokRuntime::animation_tick(
    const core::ActorSlotHandle actor) const noexcept {
    const auto animation = animation_index(actor);
    if (!animation.has_value()) {
        return 0;
    }
    return actor_runtime_[actor.slot].animation_tick;
}

std::optional<OctorokPhase> OctorokRuntime::phase(
    const core::ActorSlotHandle actor) const noexcept {
    if (!animation_index(actor).has_value()) {
        return std::nullopt;
    }
    return actor_runtime_[actor.slot].phase;
}

std::optional<SwordHitbox> OctorokRuntime::sword_hitbox(
    const PlayerState& player) const noexcept {
    if (sword_ticks_ == 0) {
        return std::nullopt;
    }
    SwordHitbox result{
        .room = player.room,
        .center_x = player.local_x,
        .center_y = player.local_y,
        .half_width = 5.0,
        .half_height = 5.0,
    };
    switch (player.facing) {
    case PlayerFacing::north:
        result.center_y -= 12.0;
        result.half_width = 7.0;
        break;
    case PlayerFacing::east:
        result.center_x += 12.0;
        result.half_height = 7.0;
        break;
    case PlayerFacing::south:
        result.center_y += 12.0;
        result.half_width = 7.0;
        break;
    case PlayerFacing::west:
        result.center_x -= 12.0;
        result.half_height = 7.0;
        break;
    }
    return result;
}

std::uint64_t OctorokRuntime::deterministic_state() const noexcept {
    constexpr std::uint64_t basis = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    auto result = basis;
    const auto append = [&](const std::uint64_t value) {
        result ^= value;
        result *= prime;
    };
    append(rng_low_);
    append(rng_high_);
    append(sword_ticks_);
    for (const auto& runtime : actor_runtime_) {
        append(runtime.generation);
        append(static_cast<std::uint8_t>(runtime.phase));
        append(runtime.counter);
        append(runtime.walk_counter);
        append(runtime.angle);
        append(runtime.animation_tick);
        append(runtime.initialized);
    }
    return result;
}

}  // namespace oracle::gameplay
