#include "oracle/gameplay/octorok_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "oracle/content/room_layout.h"
#include "oracle/gameplay/object_contact.h"

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t octorok_enemy_id = 0x09;
constexpr std::uint8_t item_drop_part_id = 0x01;
constexpr std::uint8_t enemy_destroyed_part_id = 0x02;
constexpr std::uint8_t octorok_projectile_part_id = 0x18;
constexpr std::array<std::uint8_t, 8> counter_values{
    30, 45, 60, 75, 45, 60, 75, 90,
};
constexpr std::array<std::uint8_t, 4> walk_counter_values{
    0x19, 0x21, 0x29, 0x31,
};
constexpr std::array<std::uint8_t, 5> decision_masks{
    0x07, 0x07, 0x03, 0x03, 0x01,
};
constexpr std::uint8_t player_damage_invincibility_ticks = 60;
constexpr std::uint8_t sword_damage = 2;
constexpr std::uint8_t sword_hit_invincibility_ticks = 0x15;
constexpr std::uint8_t sword_knockback_ticks = 0x0b;
constexpr std::int32_t sword_knockback_speed_subpixels = 0x200;
constexpr std::int32_t projectile_speed_subpixels = 0x200;
constexpr std::int32_t projectile_bounce_speed_subpixels = 0x40;
constexpr std::int32_t projectile_bounce_velocity_subpixels = 0xe0;
constexpr std::int32_t projectile_gravity_subpixels = 0x0e;
constexpr std::uint8_t projectile_bounce_ticks = 0x20;
constexpr std::uint8_t enemy_destroyed_ticks = 20;
constexpr std::int32_t item_drop_initial_velocity_subpixels = 0x160;
constexpr std::int32_t item_drop_gravity_subpixels = 0x20;
constexpr std::uint16_t item_drop_lifetime_ticks = 480;
constexpr std::array<std::uint8_t, 8> octorok_drop_probability{
    0x49, 0x50, 0x49, 0x24, 0x88, 0x99, 0xb2, 0xd2,
};
constexpr std::array<std::uint8_t, 32> octorok_drop_set{
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
};

enum class ProjectileTerrainResult {
    clear,
    solid,
    out_of_bounds,
};

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
    if (
        !player.object_contact_enabled ||
        !shares_actor_space(actor.room, player.room) ||
        !object_z_contact(player.z_subpixels, 0)) {
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

ProjectileTerrainResult projectile_terrain_result(
    const core::ActorSlotState& actor,
    const std::int16_t x,
    const std::int16_t y,
    const EnemyCollisionLookup& collision_lookup) {
    const auto* collisions = collision_lookup(actor.room);
    if (collisions == nullptr) {
        return ProjectileTerrainResult::out_of_bounds;
    }
    const auto width = static_cast<int>(
        collisions->columns * content::metatile_world_size);
    const auto height = static_cast<int>(
        collisions->rows * content::metatile_world_size);
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return ProjectileTerrainResult::out_of_bounds;
    }
    const auto pixel_x = static_cast<std::size_t>(x);
    const auto pixel_y = static_cast<std::size_t>(y);
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
    return content::RoomCollisionDecoder::is_solid(
        collisions->at(column, row),
        local_x,
        local_y,
        content::CollisionProfile::
            grounded_actor_without_small_bridges)
        ? ProjectileTerrainResult::solid
        : ProjectileTerrainResult::clear;
}

std::pair<std::int32_t, std::int32_t> cardinal_velocity(
    const std::uint8_t angle,
    const std::int32_t speed_subpixels) noexcept {
    switch (angle & 0x18) {
    case 0x00:
        return {0, -speed_subpixels};
    case 0x08:
        return {speed_subpixels, 0};
    case 0x10:
        return {0, speed_subpixels};
    case 0x18:
        return {-speed_subpixels, 0};
    }
    return {};
}

std::pair<int, int> apply_subpixel_velocity(
    std::int32_t& subpixel_x,
    std::int32_t& subpixel_y,
    const std::int32_t velocity_x,
    const std::int32_t velocity_y) noexcept {
    subpixel_x += velocity_x;
    subpixel_y += velocity_y;
    int delta_x = 0;
    int delta_y = 0;
    while (subpixel_x >= 256) {
        ++delta_x;
        subpixel_x -= 256;
    }
    while (subpixel_x <= -256) {
        --delta_x;
        subpixel_x += 256;
    }
    while (subpixel_y >= 256) {
        ++delta_y;
        subpixel_y -= 256;
    }
    while (subpixel_y <= -256) {
        --delta_y;
        subpixel_y += 256;
    }
    return {delta_x, delta_y};
}

void apply_contact_damage(
    const core::ActorSlotState& actor,
    PlayerCombatState& combat) noexcept {
    const auto damage = static_cast<std::uint8_t>(
        std::min(
            0x7f,
            std::abs(static_cast<int>(actor.contact_damage))));
    combat.health = damage >= combat.health
        ? 0
        : static_cast<std::uint8_t>(combat.health - damage);
    combat.invincibility_ticks = player_damage_invincibility_ticks;
}

std::uint8_t enemy_destroyed_oam_index(
    const std::uint8_t tick) noexcept {
    if (tick < 2) {
        return 0;
    }
    if (tick < 4) {
        return 1;
    }
    if (tick < 6) {
        return 0;
    }
    if (tick < 10) {
        return 2;
    }
    if (tick < 14) {
        return 3;
    }
    if (tick < 18) {
        return 4;
    }
    return 5;
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
    : rom_{rom}, definitions_{rom}, part_definitions_{rom} {
    reset(rng_seed);
}

void OctorokRuntime::reset(const std::uint16_t rng_seed) noexcept {
    actor_runtime_ = {};
    projectile_runtime_ = {};
    aftermath_runtime_ = {};
    rng_low_ = static_cast<std::uint8_t>(rng_seed & 0xff);
    rng_high_ = static_cast<std::uint8_t>(rng_seed >> 8u);
    frame_counter_ = 0;
}

void OctorokRuntime::initialize_projectile(
    const std::size_t slot,
    core::ActorSlotState& actor) {
    const auto definition =
        part_definitions_.decode(actor.identity.id);
    actor.collision_radius_y = definition.collision_radius_y;
    actor.collision_radius_x = definition.collision_radius_x;
    actor.maximum_health = definition.health;
    actor.health = definition.health;
    actor.contact_damage = definition.contact_damage;
    actor.blocks_player = false;

    auto& runtime = projectile_runtime_[slot];
    runtime = ProjectileRuntime{};
    runtime.generation = actor.generation;
    runtime.initialized = true;
    runtime.phase = OctorokProjectilePhase::flying;
    runtime.angle =
        static_cast<std::uint8_t>(actor.identity.parameter & 0x1f);
    runtime.speed_subpixels = projectile_speed_subpixels;
}

bool OctorokRuntime::spawn_projectile(
    const core::ActorSlotState& source,
    const std::uint8_t angle,
    core::ActorSlotDomain& actors) {
    const auto handle = actors.allocate_dynamic(
        core::ActorCategory::part,
        core::ActorIdentity{
            .id = octorok_projectile_part_id,
            .parameter =
                static_cast<std::uint8_t>(angle & 0x1f),
        },
        source.room,
        source.local_x,
        source.local_y,
        true,
        false,
        source.source_record_index);
    if (!handle.has_value()) {
        return false;
    }
    auto* projectile = actors.get(*handle);
    if (projectile == nullptr) {
        return false;
    }
    initialize_projectile(handle->slot, *projectile);
    return true;
}

void OctorokRuntime::update_projectiles(
    const PlayerState& player,
    PlayerCombatState& combat,
    core::ActorSlotDomain& actors,
    const EnemyCollisionLookup& collision_lookup,
    OctorokStepReport& report) {
    const auto part_slots = actors.slots(core::ActorCategory::part);
    for (std::size_t slot = 0; slot < part_slots.size(); ++slot) {
        const auto& immutable_actor = part_slots[slot];
        if (
            !immutable_actor.active ||
            immutable_actor.identity.id !=
                octorok_projectile_part_id) {
            continue;
        }
        const core::ActorSlotHandle handle{
            core::ActorCategory::part,
            static_cast<std::uint8_t>(slot),
            immutable_actor.generation,
        };
        auto* actor = actors.get(handle);
        if (actor == nullptr) {
            continue;
        }
        auto& runtime = projectile_runtime_[slot];
        if (
            !runtime.initialized ||
            runtime.generation != actor->generation) {
            initialize_projectile(slot, *actor);
            continue;
        }

        switch (runtime.phase) {
        case OctorokProjectilePhase::flying: {
            const auto [velocity_x, velocity_y] =
                cardinal_velocity(
                    runtime.angle,
                    runtime.speed_subpixels);
            auto candidate_subpixel_x = runtime.subpixel_x;
            auto candidate_subpixel_y = runtime.subpixel_y;
            const auto [delta_x, delta_y] =
                apply_subpixel_velocity(
                    candidate_subpixel_x,
                    candidate_subpixel_y,
                    velocity_x,
                    velocity_y);
            const auto candidate_x = static_cast<std::int16_t>(
                actor->local_x + delta_x);
            const auto candidate_y = static_cast<std::int16_t>(
                actor->local_y + delta_y);
            const auto terrain = projectile_terrain_result(
                *actor,
                candidate_x,
                candidate_y,
                collision_lookup);
            if (terrain == ProjectileTerrainResult::out_of_bounds) {
                (void)actors.release(handle);
                ++report.projectiles_expired;
                continue;
            }
            if (terrain == ProjectileTerrainResult::solid) {
                runtime.phase = OctorokProjectilePhase::impact;
                ++report.projectile_impacts;
                continue;
            }
            runtime.subpixel_x = candidate_subpixel_x;
            runtime.subpixel_y = candidate_subpixel_y;
            actor->local_x = candidate_x;
            actor->local_y = candidate_y;
            if (overlaps_player(*actor, player)) {
                if (
                    combat.invincibility_ticks == 0 &&
                    combat.health != 0) {
                    apply_contact_damage(*actor, combat);
                    ++report.projectile_contacts;
                }
                runtime.phase = OctorokProjectilePhase::impact;
                ++report.projectile_impacts;
            }
            break;
        }
        case OctorokProjectilePhase::impact:
            runtime.phase = OctorokProjectilePhase::bouncing;
            runtime.counter = projectile_bounce_ticks;
            runtime.angle =
                static_cast<std::uint8_t>(runtime.angle ^ 0x10);
            runtime.speed_subpixels =
                projectile_bounce_speed_subpixels;
            runtime.subpixel_x = 0;
            runtime.subpixel_y = 0;
            runtime.elevation_subpixels = 0;
            runtime.vertical_velocity_subpixels =
                projectile_bounce_velocity_subpixels;
            break;
        case OctorokProjectilePhase::bouncing: {
            if (runtime.counter != 0) {
                --runtime.counter;
            }
            if (runtime.counter == 0) {
                (void)actors.release(handle);
                ++report.projectiles_expired;
                continue;
            }
            runtime.elevation_subpixels =
                std::max(
                    0,
                    runtime.elevation_subpixels +
                        runtime.vertical_velocity_subpixels);
            runtime.vertical_velocity_subpixels -=
                projectile_gravity_subpixels;
            const auto [velocity_x, velocity_y] =
                cardinal_velocity(
                    runtime.angle,
                    runtime.speed_subpixels);
            const auto [delta_x, delta_y] =
                apply_subpixel_velocity(
                    runtime.subpixel_x,
                    runtime.subpixel_y,
                    velocity_x,
                    velocity_y);
            actor->local_x = static_cast<std::int16_t>(
                actor->local_x + delta_x);
            actor->local_y = static_cast<std::int16_t>(
                actor->local_y + delta_y);
            break;
        }
        }
    }
}

bool OctorokRuntime::spawn_death_puff(
    const core::ActorSlotState& source,
    core::ActorSlotDomain& actors) {
    const auto handle = actors.allocate_dynamic(
        core::ActorCategory::part,
        core::ActorIdentity{
            .id = enemy_destroyed_part_id,
            .subid = source.identity.id,
        },
        source.room,
        source.local_x,
        source.local_y,
        true,
        false,
        source.source_record_index);
    if (!handle.has_value()) {
        return false;
    }
    auto& runtime = aftermath_runtime_[handle->slot];
    runtime = AftermathRuntime{};
    runtime.generation = handle->generation;
    runtime.phase = AftermathPhase::enemy_destroyed;
    runtime.initialized = true;
    return true;
}

std::optional<std::uint8_t>
OctorokRuntime::decide_octorok_drop() noexcept {
    const auto probability_index =
        static_cast<std::uint8_t>(next_random().value & 0x3f);
    const auto probability_byte =
        octorok_drop_probability[probability_index >> 3u];
    const auto probability_bit = static_cast<std::uint8_t>(
        1u << (probability_index & 0x07));
    if ((probability_byte & probability_bit) == 0) {
        return std::nullopt;
    }
    return octorok_drop_set[next_random().value & 0x1f];
}

void OctorokRuntime::update_aftermath(
    const PlayerState& player,
    PlayerCombatState& combat,
    core::ActorSlotDomain& actors,
    OctorokStepReport& report) {
    const auto part_slots = actors.slots(core::ActorCategory::part);
    for (std::size_t slot = 0; slot < part_slots.size(); ++slot) {
        const auto& immutable_actor = part_slots[slot];
        if (
            !immutable_actor.active ||
            (
                immutable_actor.identity.id != enemy_destroyed_part_id &&
                immutable_actor.identity.id != item_drop_part_id)) {
            continue;
        }
        const core::ActorSlotHandle handle{
            core::ActorCategory::part,
            static_cast<std::uint8_t>(slot),
            immutable_actor.generation,
        };
        auto* actor = actors.get(handle);
        if (actor == nullptr) {
            continue;
        }
        auto& runtime = aftermath_runtime_[slot];
        if (
            !runtime.initialized ||
            runtime.generation != actor->generation) {
            runtime = AftermathRuntime{};
            runtime.generation = actor->generation;
            runtime.initialized = true;
            runtime.phase =
                actor->identity.id == enemy_destroyed_part_id
                ? AftermathPhase::enemy_destroyed
                : AftermathPhase::item_drop_bouncing;
            if (runtime.phase == AftermathPhase::item_drop_bouncing) {
                runtime.vertical_velocity_subpixels =
                    item_drop_initial_velocity_subpixels;
            }
        }

        if (runtime.phase == AftermathPhase::enemy_destroyed) {
            ++runtime.animation_tick;
            if (runtime.animation_tick < enemy_destroyed_ticks) {
                continue;
            }
            const auto drop = decide_octorok_drop();
            if (!drop.has_value()) {
                (void)actors.release(handle);
                continue;
            }
            actor->identity = core::ActorIdentity{
                .id = item_drop_part_id,
                .subid = *drop,
            };
            const auto definition =
                part_definitions_.decode(item_drop_part_id);
            actor->collision_radius_y =
                definition.collision_radius_y;
            actor->collision_radius_x =
                definition.collision_radius_x;
            actor->contact_damage = 0;
            actor->blocks_player = false;
            runtime.phase = AftermathPhase::item_drop_bouncing;
            runtime.counter = 0;
            runtime.animation_tick = 0;
            runtime.elevation_subpixels = 0;
            runtime.vertical_velocity_subpixels =
                item_drop_initial_velocity_subpixels;
            ++report.item_drops_spawned;
            continue;
        }

        if (runtime.phase == AftermathPhase::item_drop_bouncing) {
            runtime.elevation_subpixels +=
                runtime.vertical_velocity_subpixels;
            runtime.vertical_velocity_subpixels -=
                item_drop_gravity_subpixels;
            if (
                runtime.elevation_subpixels <= 0 &&
                runtime.vertical_velocity_subpixels < 0) {
                runtime.elevation_subpixels = 0;
                const auto rebound =
                    -runtime.vertical_velocity_subpixels / 2;
                if (rebound < 0x80) {
                    runtime.vertical_velocity_subpixels = 0;
                    runtime.phase = AftermathPhase::item_drop_waiting;
                    runtime.counter = item_drop_lifetime_ticks;
                } else {
                    runtime.vertical_velocity_subpixels = rebound;
                }
            }
            continue;
        }

        if (overlaps_player(*actor, player)) {
            switch (actor->identity.subid) {
            case 0x01:
                combat.health = static_cast<std::uint8_t>(
                    std::min(
                        static_cast<unsigned int>(combat.maximum_health),
                        static_cast<unsigned int>(combat.health) + 4u));
                break;
            case 0x02:
                combat.rupees = static_cast<std::uint16_t>(
                    std::min(999u, combat.rupees + 1u));
                break;
            case 0x03:
                combat.rupees = static_cast<std::uint16_t>(
                    std::min(999u, combat.rupees + 5u));
                break;
            default:
                break;
            }
            (void)actors.release(handle);
            ++report.item_drops_collected;
            continue;
        }
        if (runtime.counter != 0) {
            --runtime.counter;
        }
        if (runtime.counter == 0) {
            (void)actors.release(handle);
            ++report.item_drops_expired;
        }
    }
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

bool OctorokRuntime::move_knockback(
    core::ActorSlotState& actor,
    ActorRuntime& runtime,
    const EnemyCollisionLookup& collision_lookup) {
    const auto [velocity_x, velocity_y] =
        cardinal_velocity(
            runtime.knockback_angle,
            sword_knockback_speed_subpixels);
    auto candidate_subpixel_x =
        static_cast<std::int32_t>(runtime.subpixel_x);
    auto candidate_subpixel_y =
        static_cast<std::int32_t>(runtime.subpixel_y);
    const auto [delta_x, delta_y] =
        apply_subpixel_velocity(
            candidate_subpixel_x,
            candidate_subpixel_y,
            velocity_x,
            velocity_y);
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
    runtime.subpixel_x =
        static_cast<std::int16_t>(candidate_subpixel_x);
    runtime.subpixel_y =
        static_cast<std::int16_t>(candidate_subpixel_y);
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
    const PlayerState& player,
    PlayerCombatState& combat,
    core::ActorSlotDomain& actors,
    const EnemyCollisionLookup& collision_lookup,
    const SwordStepReport& sword_step) {
    OctorokStepReport report;
    if (combat.invincibility_ticks != 0) {
        --combat.invincibility_ticks;
    }
    for (auto& runtime : actor_runtime_) {
        if (runtime.hit_invincibility != 0) {
            --runtime.hit_invincibility;
        }
    }
    update_projectiles(
        player,
        combat,
        actors,
        collision_lookup,
        report);
    update_aftermath(
        player,
        combat,
        actors,
        report);
    if (sword_step.started) {
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
        if (runtime.knockback_counter != 0) {
            --runtime.knockback_counter;
            if (!move_knockback(
                    *actor,
                    runtime,
                    collision_lookup)) {
                runtime.knockback_counter = 0;
            }
            continue;
        }
        if (actor->health == 0) {
            if (spawn_death_puff(*actor, actors)) {
                ++report.death_puffs_spawned;
            }
            (void)actors.release(handle);
            continue;
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
                if (spawn_projectile(
                        *actor,
                        runtime.angle,
                        actors)) {
                    ++report.projectiles_spawned;
                }
            }
            break;
        }
    }

    if (sword_step.hitbox.has_value()) {
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
                !overlaps_sword(*actor, *sword_step.hitbox)) {
                continue;
            }
            runtime.hit_this_swing = true;
            runtime.hit_invincibility =
                sword_hit_invincibility_ticks;
            runtime.knockback_counter = sword_knockback_ticks;
            runtime.knockback_angle = static_cast<std::uint8_t>(
                cardinal_angle_to_player(*actor, player) ^ 0x10);
            runtime.subpixel_x = 0;
            runtime.subpixel_y = 0;
            ++report.enemies_hit;
            actor->health =
                actor->health <= sword_damage
                ? 0
                : static_cast<std::uint8_t>(
                    actor->health - sword_damage);
            if (actor->health == 0) {
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
                actor.health == 0 ||
                !overlaps_player(actor, player)) {
                continue;
            }
            apply_contact_damage(actor, combat);
            ++report.contacts;
            break;
        }
    }
    ++frame_counter_;
    return report;
}

std::optional<OctorokProjectilePhase>
OctorokRuntime::projectile_phase(
    const core::ActorSlotHandle actor) const noexcept {
    if (
        actor.category != core::ActorCategory::part ||
        actor.slot >= projectile_runtime_.size()) {
        return std::nullopt;
    }
    const auto& runtime = projectile_runtime_[actor.slot];
    if (
        !runtime.initialized ||
        runtime.generation != actor.generation) {
        return std::nullopt;
    }
    return runtime.phase;
}

double OctorokRuntime::projectile_elevation(
    const core::ActorSlotHandle actor) const noexcept {
    if (!projectile_phase(actor).has_value()) {
        return 0.0;
    }
    return
        static_cast<double>(
            projectile_runtime_[actor.slot].elevation_subpixels) /
        256.0;
}

std::optional<OctorokAftermathVisual>
OctorokRuntime::aftermath_visual(
    const core::ActorSlotHandle actor) const noexcept {
    if (
        actor.category != core::ActorCategory::part ||
        actor.slot >= aftermath_runtime_.size()) {
        return std::nullopt;
    }
    const auto& runtime = aftermath_runtime_[actor.slot];
    if (
        !runtime.initialized ||
        runtime.generation != actor.generation) {
        return std::nullopt;
    }
    if (runtime.phase == AftermathPhase::enemy_destroyed) {
        return OctorokAftermathVisual{
            .kind = OctorokAftermathKind::enemy_destroyed,
            .oam_index =
                enemy_destroyed_oam_index(runtime.animation_tick),
        };
    }
    return OctorokAftermathVisual{
        .kind = OctorokAftermathKind::item_drop,
        .elevation =
            static_cast<double>(runtime.elevation_subpixels) / 256.0,
        .visible =
            runtime.phase == AftermathPhase::item_drop_bouncing ||
            runtime.counter >= 120 ||
            ((frame_counter_ ^ actor.slot) & 1u) == 0,
    };
}

bool OctorokRuntime::hit_flash(
    const core::ActorSlotHandle actor) const noexcept {
    if (
        actor.category != core::ActorCategory::enemy ||
        actor.slot >= actor_runtime_.size()) {
        return false;
    }
    const auto& runtime = actor_runtime_[actor.slot];
    return
        runtime.initialized &&
        runtime.generation == actor.generation &&
        runtime.hit_invincibility != 0 &&
        (frame_counter_ & 0x04) == 0;
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
    append(frame_counter_);
    for (const auto& runtime : actor_runtime_) {
        append(runtime.generation);
        append(static_cast<std::uint8_t>(runtime.phase));
        append(runtime.counter);
        append(runtime.walk_counter);
        append(runtime.angle);
        append(runtime.hit_invincibility);
        append(runtime.knockback_counter);
        append(runtime.knockback_angle);
        append(
            static_cast<std::uint16_t>(runtime.subpixel_x));
        append(
            static_cast<std::uint16_t>(runtime.subpixel_y));
        append(runtime.animation_tick);
        append(runtime.initialized);
        append(runtime.hit_this_swing);
    }
    for (const auto& runtime : projectile_runtime_) {
        append(runtime.generation);
        append(static_cast<std::uint8_t>(runtime.phase));
        append(runtime.counter);
        append(runtime.angle);
        append(static_cast<std::uint32_t>(runtime.speed_subpixels));
        append(static_cast<std::uint32_t>(runtime.subpixel_x));
        append(static_cast<std::uint32_t>(runtime.subpixel_y));
        append(
            static_cast<std::uint32_t>(
                runtime.elevation_subpixels));
        append(
            static_cast<std::uint32_t>(
                runtime.vertical_velocity_subpixels));
        append(runtime.initialized);
    }
    for (const auto& runtime : aftermath_runtime_) {
        append(runtime.generation);
        append(static_cast<std::uint8_t>(runtime.phase));
        append(runtime.counter);
        append(runtime.animation_tick);
        append(
            static_cast<std::uint32_t>(
                runtime.elevation_subpixels));
        append(
            static_cast<std::uint32_t>(
                runtime.vertical_velocity_subpixels));
        append(runtime.initialized);
    }
    return result;
}

}  // namespace oracle::gameplay
