#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/chest_data.h"
#include "oracle/core/world_id.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/input/input_frame.h"

namespace oracle::gameplay {

struct ChestScenarioDefinition {
    core::WorldRoomId room;
    std::uint8_t player_spawn_yx{};
};

[[nodiscard]] ChestScenarioDefinition chest_scenario(
    core::Campaign campaign) noexcept;

struct ChestStepReport {
    bool opened{};
    bool wrong_side{};
    bool unsupported_treasure{};
    std::uint16_t rupees_awarded{};
    std::uint8_t room_flags{};
};

// Native equivalent of nextToChestTile for the first supported treasure
// family. State is stored as the retail per-room ROOMFLAG_ITEM bit, so a newly
// decoded room can reapply the opened metatile without keeping its old pixels.
class ChestRuntime {
public:
    explicit ChestRuntime(const content::RomSource& rom);

    void reset() noexcept;
    void set_room_flags(
        core::WorldRoomId room,
        std::uint8_t flags);
    [[nodiscard]] ChestStepReport update(
        const input::InputFrame& input,
        const PlayerState& player,
        std::uint16_t& rupees);
    [[nodiscard]] std::uint8_t room_flags(
        core::WorldRoomId room) const noexcept;
    [[nodiscard]] bool apply_room_state(
        content::RoomLayout& room) const;

private:
    struct RoomState {
        core::WorldRoomId room;
        std::uint8_t flags{};
    };

    [[nodiscard]] RoomState* state(core::WorldRoomId room) noexcept;
    [[nodiscard]] const RoomState* state(
        core::WorldRoomId room) const noexcept;

    content::ChestDataDecoder decoder_;
    std::vector<RoomState> states_;
};

}  // namespace oracle::gameplay
