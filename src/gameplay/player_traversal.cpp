#include "oracle/gameplay/player_traversal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "oracle/content/room_layout.h"

namespace oracle::gameplay {
namespace {

constexpr double collision_epsilon = 0.001;
constexpr double maximum_movement_substep = 1.0;

struct CollisionSample {
    core::WorldRoomId room;
    double x{};
    double y{};
};

bool same_room(
    const core::WorldRoomId left,
    const core::WorldRoomId right) noexcept {
    return left == right;
}

bool resolve_small_room_sample(CollisionSample& sample) {
    if (sample.room.area >= 4) {
        return false;
    }
    auto room = static_cast<std::uint8_t>(sample.room.room);
    while (sample.x < 0.0) {
        if ((room & 0x0f) == 0) {
            return false;
        }
        --room;
        sample.x += content::small_room_world_width;
    }
    while (sample.x >= content::small_room_world_width) {
        if ((room & 0x0f) == 0x0f) {
            return false;
        }
        ++room;
        sample.x -= content::small_room_world_width;
    }
    while (sample.y < 0.0) {
        if ((room >> 4u) == 0) {
            return false;
        }
        room = static_cast<std::uint8_t>(room - 0x10);
        sample.y += content::small_room_world_height;
    }
    while (sample.y >= content::small_room_world_height) {
        if ((room >> 4u) == 0x0f) {
            return false;
        }
        room = static_cast<std::uint8_t>(room + 0x10);
        sample.y -= content::small_room_world_height;
    }
    sample.room.room = room;
    return true;
}

bool sample_is_clear(
    CollisionSample sample,
    const CollisionLookup& collision_lookup) {
    auto* collisions = collision_lookup(sample.room);
    if (collisions == nullptr) {
        return false;
    }
    const auto width =
        static_cast<double>(
            collisions->columns * content::metatile_world_size);
    const auto height =
        static_cast<double>(
            collisions->rows * content::metatile_world_size);
    if (
        sample.x < 0.0 ||
        sample.y < 0.0 ||
        sample.x >= width ||
        sample.y >= height) {
        if (!resolve_small_room_sample(sample)) {
            return false;
        }
        collisions = collision_lookup(sample.room);
        if (collisions == nullptr) {
            return false;
        }
    }

    const auto pixel_x =
        static_cast<std::size_t>(std::floor(sample.x));
    const auto pixel_y =
        static_cast<std::size_t>(std::floor(sample.y));
    const auto column =
        pixel_x /
        static_cast<std::size_t>(content::metatile_world_size);
    const auto row =
        pixel_y /
        static_cast<std::size_t>(content::metatile_world_size);
    if (column >= collisions->columns || row >= collisions->rows) {
        return false;
    }
    const auto local_x = static_cast<std::uint8_t>(
        pixel_x %
        static_cast<std::size_t>(content::metatile_world_size));
    const auto local_y = static_cast<std::uint8_t>(
        pixel_y %
        static_cast<std::size_t>(content::metatile_world_size));
    return !content::RoomCollisionDecoder::is_solid(
        collisions->at(column, row),
        local_x,
        local_y);
}

void canonicalize_player(PlayerState& player) {
    if (player.room.area >= 4) {
        return;
    }
    auto room = static_cast<std::uint8_t>(player.room.room);
    while (player.local_x < 0.0 && (room & 0x0f) != 0) {
        --room;
        player.local_x += content::small_room_world_width;
    }
    while (
        player.local_x >= content::small_room_world_width &&
        (room & 0x0f) != 0x0f) {
        ++room;
        player.local_x -= content::small_room_world_width;
    }
    while (player.local_y < 0.0 && (room >> 4u) != 0) {
        room = static_cast<std::uint8_t>(room - 0x10);
        player.local_y += content::small_room_world_height;
    }
    while (
        player.local_y >= content::small_room_world_height &&
        (room >> 4u) != 0x0f) {
        room = static_cast<std::uint8_t>(room + 0x10);
        player.local_y -= content::small_room_world_height;
    }
    player.room.room = room;
}

bool move_axis(
    PlayerState& player,
    const double amount,
    const bool horizontal,
    const CollisionLookup& collision_lookup,
    const PlayerBody body) {
    if (amount == 0.0) {
        return false;
    }
    auto candidate = player;
    if (horizontal) {
        candidate.local_x += amount;
    } else {
        candidate.local_y += amount;
    }
    if (!PlayerTraversal::can_occupy(
            candidate,
            collision_lookup,
            body)) {
        return false;
    }
    player = candidate;
    canonicalize_player(player);
    return true;
}

}  // namespace

TraversalStep PlayerTraversal::step(
    PlayerState& player,
    MovementInput input,
    const double elapsed_seconds,
    const CollisionLookup& collision_lookup,
    const double speed_pixels_per_second,
    const PlayerBody body) {
    if (
        elapsed_seconds < 0.0 ||
        speed_pixels_per_second < 0.0 ||
        body.half_width <= 0.0 ||
        body.half_height <= 0.0) {
        throw std::invalid_argument{
            "player traversal parameters must be non-negative"};
    }
    input.horizontal = std::clamp(input.horizontal, -1.0, 1.0);
    input.vertical = std::clamp(input.vertical, -1.0, 1.0);
    const auto input_length =
        std::hypot(input.horizontal, input.vertical);
    if (input_length > 1.0) {
        input.horizontal /= input_length;
        input.vertical /= input_length;
    }

    if (
        std::abs(input.horizontal) >= std::abs(input.vertical) &&
        input.horizontal != 0.0) {
        player.facing = input.horizontal > 0.0
            ? PlayerFacing::east
            : PlayerFacing::west;
    } else if (input.vertical != 0.0) {
        player.facing = input.vertical > 0.0
            ? PlayerFacing::south
            : PlayerFacing::north;
    }

    TraversalStep result{.previous_room = player.room};
    const auto distance = speed_pixels_per_second * elapsed_seconds;
    const auto substeps = std::max(
        1,
        static_cast<int>(
            std::ceil(distance / maximum_movement_substep)));
    const auto horizontal_step =
        input.horizontal * distance / substeps;
    const auto vertical_step =
        input.vertical * distance / substeps;
    for (int index = 0; index < substeps; ++index) {
        const auto moved_horizontal =
            move_axis(
                player,
                horizontal_step,
                true,
                collision_lookup,
                body);
        const auto moved_vertical =
            move_axis(
                player,
                vertical_step,
                false,
                collision_lookup,
                body);
        result.moved =
            result.moved || moved_horizontal || moved_vertical;
        result.blocked =
            result.blocked ||
            (horizontal_step != 0.0 && !moved_horizontal) ||
            (vertical_step != 0.0 && !moved_vertical);
    }
    result.crossed_room_seam =
        !same_room(result.previous_room, player.room);
    return result;
}

bool PlayerTraversal::can_occupy(
    const PlayerState& player,
    const CollisionLookup& collision_lookup,
    const PlayerBody body) {
    const std::array<CollisionSample, 8> samples{
        CollisionSample{
            player.room,
            player.local_x - body.half_width + collision_epsilon,
            player.local_y - body.half_height + collision_epsilon,
        },
        CollisionSample{
            player.room,
            player.local_x,
            player.local_y - body.half_height + collision_epsilon,
        },
        CollisionSample{
            player.room,
            player.local_x + body.half_width - collision_epsilon,
            player.local_y - body.half_height + collision_epsilon,
        },
        CollisionSample{
            player.room,
            player.local_x - body.half_width + collision_epsilon,
            player.local_y,
        },
        CollisionSample{
            player.room,
            player.local_x + body.half_width - collision_epsilon,
            player.local_y,
        },
        CollisionSample{
            player.room,
            player.local_x - body.half_width + collision_epsilon,
            player.local_y + body.half_height - collision_epsilon,
        },
        CollisionSample{
            player.room,
            player.local_x,
            player.local_y + body.half_height - collision_epsilon,
        },
        CollisionSample{
            player.room,
            player.local_x + body.half_width - collision_epsilon,
            player.local_y + body.half_height - collision_epsilon,
        },
    };
    return std::all_of(
        samples.begin(),
        samples.end(),
        [&](const CollisionSample sample) {
            return sample_is_clear(sample, collision_lookup);
        });
}

bool PlayerTraversal::place_near(
    PlayerState& player,
    const double preferred_x,
    const double preferred_y,
    const CollisionLookup& collision_lookup,
    const PlayerBody body) {
    constexpr int search_step = 4;
    constexpr int maximum_radius = 160;
    for (int radius = 0; radius <= maximum_radius; radius += search_step) {
        for (int y = -radius; y <= radius; y += search_step) {
            for (int x = -radius; x <= radius; x += search_step) {
                if (
                    radius != 0 &&
                    std::abs(x) != radius &&
                    std::abs(y) != radius) {
                    continue;
                }
                auto candidate = player;
                candidate.local_x = preferred_x + x;
                candidate.local_y = preferred_y + y;
                if (can_occupy(candidate, collision_lookup, body)) {
                    player = candidate;
                    canonicalize_player(player);
                    return true;
                }
            }
        }
    }
    return false;
}

double PlayerTraversal::world_x(const PlayerState& player) noexcept {
    if (player.room.area >= 4) {
        return player.local_x;
    }
    return
        static_cast<double>(
            player.room.room & 0x0f) *
            content::small_room_world_width +
        player.local_x;
}

double PlayerTraversal::world_y(const PlayerState& player) noexcept {
    if (player.room.area >= 4) {
        return player.local_y;
    }
    return
        static_cast<double>(
            (player.room.room >> 4u) & 0x0f) *
            content::small_room_world_height +
        player.local_y;
}

std::uint8_t PlayerTraversal::packed_room_position(
    const PlayerState& player) noexcept {
    const auto tile_x = std::clamp(
        static_cast<int>(player.local_x / 16.0),
        0,
        15);
    const auto tile_y = std::clamp(
        static_cast<int>((player.local_y + 16.0) / 16.0),
        0,
        15);
    return static_cast<std::uint8_t>((tile_y << 4) | tile_x);
}

PlayerState PlayerTraversal::from_packed_room_position(
    const core::WorldRoomId room,
    const std::uint8_t position) noexcept {
    return PlayerState{
        .room = room,
        .local_x =
            static_cast<double>(position & 0x0f) * 16.0 + 8.0,
        .local_y =
            static_cast<double>(position >> 4u) * 16.0 - 8.0,
        .facing = PlayerFacing::south,
    };
}

}  // namespace oracle::gameplay
