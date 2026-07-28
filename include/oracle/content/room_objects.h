#pragma once

#include <cstdint>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/core/world_id.h"

namespace oracle::content {

enum class RoomObjectKind : std::uint8_t {
    interaction,
    enemy,
    part,
    item_drop,
};

struct RoomObjectRecord {
    RoomObjectKind kind{};
    std::uint8_t id{};
    std::uint8_t subid{};
    std::uint8_t parameter{};
    std::uint8_t original_y{};
    std::uint8_t original_x{};
    bool positioned{};
    bool random_position{};
    bool conditional{};
};

struct RoomObjectCatalog {
    core::WorldRoomId room;
    std::vector<RoomObjectRecord> records;
};

// Catalogs the original room object bytecode. This first boundary preserves
// object identity and spawn coordinates; behavior and object graphics remain
// separate later stages.
class RoomObjectDecoder {
public:
    explicit RoomObjectDecoder(const RomSource& rom);

    [[nodiscard]] RoomObjectCatalog decode(
        std::uint8_t group,
        std::uint8_t room,
        std::uint8_t room_flags = 0) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
