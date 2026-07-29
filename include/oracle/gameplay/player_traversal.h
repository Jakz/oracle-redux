#pragma once

#include <functional>
#include <span>

#include "oracle/content/room_collisions.h"
#include "oracle/core/world_id.h"
#include "oracle/gameplay/actor_collision.h"

namespace oracle::gameplay {

enum class PlayerFacing {
    north,
    east,
    south,
    west,
};

struct PlayerBody {
    double half_width{4.0};
    double half_height{3.0};
    double actor_collision_radius_x{6.0};
    double actor_collision_radius_y{6.0};
};

struct PlayerState {
    core::WorldRoomId room;
    double local_x{};
    double local_y{};
    PlayerFacing facing{PlayerFacing::south};
};

struct MovementInput {
    double horizontal{};
    double vertical{};
};

struct TraversalStep {
    core::WorldRoomId previous_room;
    bool moved{};
    bool crossed_room_seam{};
    bool blocked{};
    bool contacted_actor{};
};

using CollisionLookup = std::function<
    const content::RoomCollisionMap*(core::WorldRoomId)>;

class PlayerTraversal {
public:
    [[nodiscard]] static TraversalStep step(
        PlayerState& player,
        MovementInput input,
        double elapsed_seconds,
        const CollisionLookup& collision_lookup,
        std::span<const ActorCollisionBody> actor_bodies = {},
        double speed_pixels_per_second = 64.0,
        PlayerBody body = {});

    [[nodiscard]] static bool can_occupy(
        const PlayerState& player,
        const CollisionLookup& collision_lookup,
        std::span<const ActorCollisionBody> actor_bodies = {},
        PlayerBody body = {});

    [[nodiscard]] static bool place_near(
        PlayerState& player,
        double preferred_x,
        double preferred_y,
        const CollisionLookup& collision_lookup,
        std::span<const ActorCollisionBody> actor_bodies = {},
        PlayerBody body = {});

    [[nodiscard]] static double world_x(const PlayerState& player) noexcept;
    [[nodiscard]] static double world_y(const PlayerState& player) noexcept;

    // Converts local rendering coordinates to the original room-layout YX
    // convention, whose first visible metatile row starts at Y=$10.
    [[nodiscard]] static std::uint8_t packed_room_position(
        const PlayerState& player) noexcept;

    [[nodiscard]] static PlayerState from_packed_room_position(
        core::WorldRoomId room,
        std::uint8_t position) noexcept;

    [[nodiscard]] static PlayerState from_transition_destination(
        core::WorldRoomId room,
        std::uint8_t position,
        std::uint8_t parameter,
        std::uint8_t transition,
        double room_width,
        double room_height) noexcept;
};

}  // namespace oracle::gameplay
