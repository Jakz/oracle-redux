#include "oracle/content/room_objects.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>

namespace oracle::content {
namespace {

constexpr std::size_t ages_object_group_table = 0x5432b;
constexpr std::size_t seasons_object_group_table = 0x45b3b;
constexpr std::uint8_t ages_pointer_bank = 0x15;
constexpr std::uint8_t seasons_pointer_bank = 0x11;
constexpr std::uint8_t ages_object_data_bank = 0x12;
constexpr std::uint8_t seasons_object_data_bank = 0x11;
constexpr std::size_t maximum_records = 1024;
constexpr std::size_t maximum_recursion_depth = 32;

struct CampaignOffsets {
    std::size_t group_table{};
    std::uint8_t pointer_bank{};
    std::uint8_t object_data_bank{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            .group_table = ages_object_group_table,
            .pointer_bank = ages_pointer_bank,
            .object_data_bank = ages_object_data_bank,
        };
    }
    return CampaignOffsets{
        .group_table = seasons_object_group_table,
        .pointer_bank = seasons_pointer_bank,
        .object_data_bank = seasons_object_data_bank,
    };
}

void append_record(
    RoomObjectCatalog& catalog,
    const RoomObjectRecord record) {
    if (catalog.records.size() >= maximum_records) {
        throw std::runtime_error{
            "room object catalog exceeds its safety limit"};
    }
    catalog.records.push_back(record);
}

class ObjectBytecodeParser {
public:
    ObjectBytecodeParser(
        const RomSource& rom,
        const CampaignOffsets offsets,
        const std::uint8_t room_flags,
        RoomObjectCatalog& catalog)
        : rom_{rom},
          offsets_{offsets},
          room_flags_{room_flags},
          catalog_{catalog} {}

    void parse_pointer(const std::uint16_t pointer) {
        parse(
            rom_.banked_file_offset(
                offsets_.object_data_bank,
                pointer),
            0,
            false);
    }

private:
    void parse(
        std::size_t cursor,
        const std::size_t depth,
        const bool inherited_conditional) {
        if (depth > maximum_recursion_depth) {
            throw std::runtime_error{
                "room object pointers exceed their recursion limit"};
        }
        const auto branch_start = cursor;
        if (!active_pointers_.insert(branch_start).second) {
            throw std::runtime_error{
                "recursive room object pointer cycle"};
        }

        const bool branch_conditional = inherited_conditional;
        bool next_conditional = false;
        while (true) {
            const auto opcode = rom_.read_byte(cursor++);
            if (opcode == 0xff || opcode == 0xfe) {
                break;
            }
            const auto kind =
                static_cast<std::uint8_t>(opcode & 0x0f);
            const bool conditional =
                branch_conditional || next_conditional;
            next_conditional = false;

            switch (kind) {
            case 0:
                static_cast<void>(rom_.read_byte(cursor++));
                next_conditional = true;
                break;
            case 1:
                while (rom_.read_byte(cursor) < 0xf0) {
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = RoomObjectKind::interaction,
                            .id = rom_.read_byte(cursor),
                            .subid = rom_.read_byte(cursor + 1),
                            .conditional = conditional,
                        });
                    cursor += 2;
                }
                break;
            case 2:
                while (rom_.read_byte(cursor) < 0xf0) {
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = RoomObjectKind::interaction,
                            .id = rom_.read_byte(cursor),
                            .subid = rom_.read_byte(cursor + 1),
                            .original_y = rom_.read_byte(cursor + 2),
                            .original_x = rom_.read_byte(cursor + 3),
                            .positioned = true,
                            .conditional = conditional,
                        });
                    cursor += 4;
                }
                break;
            case 3:
            case 4:
            case 5: {
                const auto pointer =
                    rom_.read_little_u16(cursor);
                cursor += 2;
                const bool selected =
                    kind == 3 ||
                    (kind == 4 && (room_flags_ & 0x80) == 0) ||
                    (kind == 5 && (room_flags_ & 0x80) != 0);
                if (selected) {
                    parse(
                        rom_.banked_file_offset(
                            offsets_.object_data_bank,
                            pointer),
                        depth + 1,
                        conditional);
                }
                break;
            }
            case 6: {
                const auto flags = rom_.read_byte(cursor);
                const auto id = rom_.read_byte(cursor + 1);
                const auto subid = rom_.read_byte(cursor + 2);
                cursor += 3;
                const auto count =
                    static_cast<std::size_t>((flags >> 5u) & 0x07);
                for (std::size_t index = 0; index < count; ++index) {
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = RoomObjectKind::enemy,
                            .id = id,
                            .subid = subid,
                            .random_position = true,
                            .conditional = conditional,
                        });
                }
                break;
            }
            case 7:
                static_cast<void>(rom_.read_byte(cursor++));
                while ((rom_.read_byte(cursor) & 0x80) == 0) {
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = RoomObjectKind::enemy,
                            .id = rom_.read_byte(cursor),
                            .subid = rom_.read_byte(cursor + 1),
                            .original_y = rom_.read_byte(cursor + 2),
                            .original_x = rom_.read_byte(cursor + 3),
                            .positioned = true,
                            .conditional = conditional,
                        });
                    cursor += 4;
                }
                break;
            case 8:
                while ((rom_.read_byte(cursor) & 0x80) == 0) {
                    const auto packed = rom_.read_byte(cursor + 2);
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = RoomObjectKind::part,
                            .id = rom_.read_byte(cursor),
                            .subid = rom_.read_byte(cursor + 1),
                            .original_y = static_cast<std::uint8_t>(
                                (packed & 0xf0) + 8),
                            .original_x = static_cast<std::uint8_t>(
                                ((packed & 0x0f) << 4u) + 8),
                            .positioned = true,
                            .conditional = conditional,
                        });
                    cursor += 3;
                }
                break;
            case 9:
                while (rom_.read_byte(cursor) < 0xf0) {
                    const auto object_type = rom_.read_byte(cursor);
                    const auto record_kind =
                        object_type == 0
                        ? RoomObjectKind::interaction
                        : object_type == 1
                        ? RoomObjectKind::enemy
                        : RoomObjectKind::part;
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = record_kind,
                            .id = rom_.read_byte(cursor + 1),
                            .subid = rom_.read_byte(cursor + 2),
                            .parameter = rom_.read_byte(cursor + 3),
                            .original_y = rom_.read_byte(cursor + 4),
                            .original_x = rom_.read_byte(cursor + 5),
                            .positioned = true,
                            .conditional = conditional,
                        });
                    cursor += 6;
                }
                break;
            case 0x0a:
                static_cast<void>(rom_.read_byte(cursor++));
                while ((rom_.read_byte(cursor) & 0x80) == 0) {
                    const auto packed = rom_.read_byte(cursor + 1);
                    append_record(
                        catalog_,
                        RoomObjectRecord{
                            .kind = RoomObjectKind::item_drop,
                            .id = rom_.read_byte(cursor),
                            .original_y = static_cast<std::uint8_t>(
                                (packed & 0xf0) + 8),
                            .original_x = static_cast<std::uint8_t>(
                                ((packed & 0x0f) << 4u) + 8),
                            .positioned = true,
                            .conditional = conditional,
                        });
                    cursor += 2;
                }
                break;
            default:
                throw std::runtime_error{
                    "unsupported room object opcode"};
            }
        }
        active_pointers_.erase(branch_start);
    }

    const RomSource& rom_;
    CampaignOffsets offsets_;
    std::uint8_t room_flags_{};
    RoomObjectCatalog& catalog_;
    std::unordered_set<std::size_t> active_pointers_;
};

}  // namespace

RoomObjectDecoder::RoomObjectDecoder(const RomSource& rom) : rom_{rom} {}

RoomObjectCatalog RoomObjectDecoder::decode(
    const std::uint8_t group,
    const std::uint8_t room,
    const std::uint8_t room_flags) const {
    if (group >= 8) {
        throw std::invalid_argument{
            "room object group must be between 0 and 7"};
    }
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto group_pointer =
        rom_.read_little_u16(
            offsets.group_table +
            static_cast<std::size_t>(group) * 2);
    const auto group_table =
        rom_.banked_file_offset(
            offsets.pointer_bank,
            group_pointer);
    const auto object_pointer =
        rom_.read_little_u16(
            group_table + static_cast<std::size_t>(room) * 2);
    RoomObjectCatalog catalog{
        .room =
            core::WorldRoomId{
                .area = group,
                .room = room,
            },
    };
    ObjectBytecodeParser parser{
        rom_,
        offsets,
        room_flags,
        catalog,
    };
    parser.parse_pointer(object_pointer);
    return catalog;
}

}  // namespace oracle::content
