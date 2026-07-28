#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/core/world_id.h"

namespace oracle::content {

enum class RoomExitKind : std::uint8_t {
    north_seam,
    east_seam,
    south_seam,
    west_seam,
    tile_warp,
    screen_edge_warp,
    fallback_warp,
};

struct WarpDestination {
    std::uint8_t group{};
    std::uint8_t index{};
    std::uint8_t room{};
    std::uint8_t position{};
    std::uint8_t parameter{};
    std::uint8_t transition{};
};

struct RoomExit {
    RoomExitKind kind{};
    core::WorldRoomId source;
    core::WorldRoomId destination;
    bool has_source_position{};
    std::uint8_t source_position{};
    std::uint8_t source_edge_mask{};
    std::uint8_t source_transition{};
    std::uint8_t destination_position{};
    std::uint8_t destination_parameter{};
    std::uint8_t destination_transition{};
    std::uint8_t destination_index{};
    bool fallback{};
};

class RoomTopologyDecoder {
public:
    explicit RoomTopologyDecoder(const RomSource& rom);

    [[nodiscard]] std::vector<WarpDestination> warp_destinations(
        std::uint8_t group) const;

    // destination_variant is the active season (0-3) for Seasons. It only
    // affects the original engine's group-2 destination lookup.
    [[nodiscard]] std::vector<RoomExit> exits(
        std::uint8_t group,
        std::uint8_t room,
        std::uint8_t destination_variant = 0,
        bool include_spatial_seams = true) const;

    [[nodiscard]] std::optional<std::uint8_t> warp_tile_property(
        std::uint8_t collision_mode,
        std::uint8_t metatile) const;

    [[nodiscard]] std::optional<RoomExit> resolve_tile_warp(
        std::uint8_t group,
        std::uint8_t room,
        std::uint8_t source_position,
        std::uint8_t metatile,
        std::uint8_t collision_mode,
        std::uint8_t destination_variant = 0) const;

    [[nodiscard]] std::optional<RoomExit> resolve_screen_edge_warp(
        std::uint8_t group,
        std::uint8_t room,
        std::uint8_t edge_quadrant_mask,
        std::uint8_t destination_variant = 0) const;

    [[nodiscard]] static std::vector<RoomExit> spatial_seams(
        std::uint8_t group,
        std::uint8_t room);

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
