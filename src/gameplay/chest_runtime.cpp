#include "oracle/gameplay/chest_runtime.h"

#include <algorithm>
#include <cmath>

namespace oracle::gameplay {

ChestScenarioDefinition chest_scenario(
    const core::Campaign campaign) noexcept {
    if (campaign == core::Campaign::ages) {
        return ChestScenarioDefinition{
            .room = core::WorldRoomId{.area = 4, .room = 0x5c},
            .player_spawn_yx = 0x47,
        };
    }
    return ChestScenarioDefinition{
        .room = core::WorldRoomId{.area = 4, .room = 0x05},
        .player_spawn_yx = 0x7d,
    };
}

ChestRuntime::ChestRuntime(const content::RomSource& rom)
    : decoder_{rom} {}

void ChestRuntime::reset() noexcept {
    states_.clear();
}

void ChestRuntime::set_room_flags(
    const core::WorldRoomId room,
    const std::uint8_t flags) {
    auto* room_state = state(room);
    if (room_state == nullptr) {
        states_.push_back(RoomState{.room = room});
        room_state = &states_.back();
    }
    room_state->flags = flags;
}

ChestStepReport ChestRuntime::update(
    const input::InputFrame& frame,
    const PlayerState& player,
    std::uint16_t& rupees) {
    ChestStepReport report{
        .room_flags = room_flags(player.room),
    };
    if (
        !frame.pressed(input::InputAction::a) &&
        !frame.pressed(input::InputAction::confirm)) {
        return report;
    }
    const auto chest = decoder_.find(player.room);
    if (!chest.has_value()) {
        return report;
    }
    if ((report.room_flags & content::room_flag_item) != 0) {
        return report;
    }

    const auto chest_column = static_cast<int>(chest->position & 0x0f);
    const auto chest_row = static_cast<int>(chest->position >> 4u);
    const auto player_column = static_cast<int>(
        std::floor(player.local_x / content::metatile_world_size));
    const auto player_row = static_cast<int>(
        std::floor(player.local_y / content::metatile_world_size));
    auto front_column = player_column;
    auto front_row = player_row;
    switch (player.facing) {
    case PlayerFacing::north:
        --front_row;
        break;
    case PlayerFacing::east:
        ++front_column;
        break;
    case PlayerFacing::south:
        ++front_row;
        break;
    case PlayerFacing::west:
        --front_column;
        break;
    }
    if (front_column != chest_column || front_row != chest_row) {
        return report;
    }
    const bool immediately_below =
        player_column == chest_column && player_row == chest_row + 1;
    const bool horizontally_centered =
        std::abs(
            std::fmod(player.local_x, 16.0) - 8.0) <= 4.0;
    if (
        !immediately_below || !horizontally_centered) {
        report.wrong_side = true;
        return report;
    }

    const auto treasure = decoder_.describe_treasure(*chest);
    if (treasure.treasure_index != content::rupee_treasure_index) {
        report.unsupported_treasure = true;
        return report;
    }
    report.rupees_awarded = decoder_.rupee_value(treasure.parameter);
    rupees = static_cast<std::uint16_t>(
        std::min<unsigned int>(999, rupees + report.rupees_awarded));
    set_room_flags(
        player.room,
        static_cast<std::uint8_t>(
            report.room_flags | content::room_flag_item));
    report.opened = true;
    report.room_flags = room_flags(player.room);
    return report;
}

std::uint8_t ChestRuntime::room_flags(
    const core::WorldRoomId room) const noexcept {
    const auto* found = state(room);
    return found == nullptr ? 0 : found->flags;
}

bool ChestRuntime::apply_room_state(
    content::RoomLayout& room) const {
    const auto chest = decoder_.find(room.id);
    return chest.has_value() &&
        content::ChestDataDecoder::apply_opened_chest(
            room,
            *chest,
            room_flags(room.id));
}

ChestRuntime::RoomState* ChestRuntime::state(
    const core::WorldRoomId room) noexcept {
    const auto found = std::find_if(
        states_.begin(),
        states_.end(),
        [room](const RoomState& candidate) {
            return candidate.room == room;
        });
    return found == states_.end() ? nullptr : &*found;
}

const ChestRuntime::RoomState* ChestRuntime::state(
    const core::WorldRoomId room) const noexcept {
    const auto found = std::find_if(
        states_.begin(),
        states_.end(),
        [room](const RoomState& candidate) {
            return candidate.room == room;
        });
    return found == states_.end() ? nullptr : &*found;
}

}  // namespace oracle::gameplay
