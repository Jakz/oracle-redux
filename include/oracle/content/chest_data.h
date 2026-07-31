#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_layout.h"
#include "oracle/core/world_id.h"

namespace oracle::content {

inline constexpr std::uint8_t room_flag_item = 0x20;
inline constexpr std::uint8_t chest_opened_metatile = 0xf0;
inline constexpr std::uint8_t chest_closed_metatile = 0xf1;
inline constexpr std::uint8_t rupee_treasure_index = 0x28;

struct ChestRecord {
    core::WorldRoomId room;
    std::uint8_t position{};
    std::uint8_t treasure_index{};
    std::uint8_t treasure_subid{};
};

struct TreasureObjectDescriptor {
    std::uint8_t treasure_index{};
    std::uint8_t treasure_subid{};
    std::uint8_t behavior{};
    std::uint8_t parameter{};
    std::uint8_t text_id{};
    std::uint8_t graphics{};
};

// Decodes the retail chestData and treasureObjectData tables directly from
// the player-supplied cartridge. Chest positions retain the original packed
// wRoomLayout Y/X byte; room_layout_index removes its six-byte row padding.
class ChestDataDecoder {
public:
    explicit ChestDataDecoder(const RomSource& rom);

    [[nodiscard]] std::vector<ChestRecord> decode_group(
        std::uint8_t group) const;
    [[nodiscard]] std::optional<ChestRecord> find(
        core::WorldRoomId room) const;
    [[nodiscard]] TreasureObjectDescriptor describe_treasure(
        const ChestRecord& chest) const;
    [[nodiscard]] std::uint16_t rupee_value(
        std::uint8_t parameter) const;

    [[nodiscard]] static std::optional<std::size_t> room_layout_index(
        std::uint8_t position,
        const RoomLayout& room) noexcept;
    [[nodiscard]] static bool apply_opened_chest(
        RoomLayout& room,
        const ChestRecord& chest,
        std::uint8_t room_flags) noexcept;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
