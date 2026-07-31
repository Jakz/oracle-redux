#include "oracle/content/chest_data.h"

#include <stdexcept>

namespace oracle::content {
namespace {

constexpr std::uint8_t chest_group_count = 8;
constexpr std::size_t chest_record_size = 4;
constexpr std::size_t treasure_record_size = 4;
constexpr std::uint8_t rupee_value_count = 0x15;

struct CampaignOffsets {
    std::uint8_t data_bank{};
    std::uint16_t chest_group_table{};
    std::uint16_t treasure_object_table{};
    std::size_t rupee_values{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            .data_bank = 0x16,
            .chest_group_table = 0x5108,
            .treasure_object_table = 0x5332,
            .rupee_values = 0x1791,
        };
    }
    return CampaignOffsets{
        .data_bank = 0x15,
        .chest_group_table = 0x4f6c,
        .treasure_object_table = 0x5129,
        .rupee_values = 0x176a,
    };
}

std::uint16_t decode_packed_bcd(const std::uint16_t value) {
    std::uint16_t result = 0;
    std::uint16_t multiplier = 1;
    auto remaining = value;
    for (int digit = 0; digit < 4; ++digit) {
        const auto nibble = static_cast<std::uint16_t>(remaining & 0x0f);
        if (nibble > 9) {
            throw std::runtime_error{"ROM rupee value is not packed BCD"};
        }
        result = static_cast<std::uint16_t>(
            result + nibble * multiplier);
        multiplier = static_cast<std::uint16_t>(multiplier * 10);
        remaining = static_cast<std::uint16_t>(remaining >> 4u);
    }
    return result;
}

}  // namespace

ChestDataDecoder::ChestDataDecoder(const RomSource& rom) : rom_{rom} {}

std::vector<ChestRecord> ChestDataDecoder::decode_group(
    const std::uint8_t group) const {
    if (group >= chest_group_count) {
        throw std::out_of_range{"chest group must be between 0 and 7"};
    }
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto table = rom_.banked_file_offset(
        offsets.data_bank,
        offsets.chest_group_table);
    const auto pointer = rom_.read_little_u16(
        table + static_cast<std::size_t>(group) * 2);
    auto cursor = rom_.banked_file_offset(offsets.data_bank, pointer);
    std::vector<ChestRecord> records;
    while (true) {
        const auto position = rom_.read_byte(cursor);
        if (position == 0xff) {
            break;
        }
        records.push_back(ChestRecord{
            .room =
                core::WorldRoomId{
                    .area = group,
                    .room = rom_.read_byte(cursor + 1),
                },
            .position = position,
            .treasure_index = rom_.read_byte(cursor + 2),
            .treasure_subid = rom_.read_byte(cursor + 3),
        });
        cursor += chest_record_size;
    }
    return records;
}

std::optional<ChestRecord> ChestDataDecoder::find(
    const core::WorldRoomId room) const {
    if (room.area >= chest_group_count || room.room > 0xff) {
        return std::nullopt;
    }
    const auto records = decode_group(static_cast<std::uint8_t>(room.area));
    for (const auto& record : records) {
        if (record.room == room) {
            return record;
        }
    }
    return std::nullopt;
}

TreasureObjectDescriptor ChestDataDecoder::describe_treasure(
    const ChestRecord& chest) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto table = rom_.banked_file_offset(
        offsets.data_bank,
        offsets.treasure_object_table);
    auto record = table +
        static_cast<std::size_t>(chest.treasure_index) *
            treasure_record_size;
    const auto first = rom_.read_byte(record);
    if ((first & 0x80) != 0) {
        const auto pointer = rom_.read_little_u16(record + 1);
        record =
            rom_.banked_file_offset(offsets.data_bank, pointer) +
            static_cast<std::size_t>(chest.treasure_subid) *
                treasure_record_size;
    } else if (chest.treasure_subid != 0) {
        throw std::runtime_error{
            "chest references a subid on an inline treasure descriptor"};
    }
    return TreasureObjectDescriptor{
        .treasure_index = chest.treasure_index,
        .treasure_subid = chest.treasure_subid,
        .behavior = rom_.read_byte(record),
        .parameter = rom_.read_byte(record + 1),
        .text_id = rom_.read_byte(record + 2),
        .graphics = rom_.read_byte(record + 3),
    };
}

std::uint16_t ChestDataDecoder::rupee_value(
    std::uint8_t parameter) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    if (parameter >= rupee_value_count) {
        parameter = static_cast<std::uint8_t>(rupee_value_count - 1);
    }
    return decode_packed_bcd(
        rom_.read_little_u16(
            offsets.rupee_values +
            static_cast<std::size_t>(parameter) * 2));
}

std::optional<std::size_t> ChestDataDecoder::room_layout_index(
    const std::uint8_t position,
    const RoomLayout& room) noexcept {
    const auto row = static_cast<std::size_t>(position >> 4u);
    const auto column = static_cast<std::size_t>(position & 0x0f);
    if (row >= room.rows || column >= room.columns) {
        return std::nullopt;
    }
    return row * room.columns + column;
}

bool ChestDataDecoder::apply_opened_chest(
    RoomLayout& room,
    const ChestRecord& chest,
    const std::uint8_t room_flags) noexcept {
    if (room.id != chest.room || (room_flags & room_flag_item) == 0) {
        return false;
    }
    const auto index = room_layout_index(chest.position, room);
    if (!index.has_value() || *index >= room.metatiles.size()) {
        return false;
    }
    room.metatiles[*index] = chest_opened_metatile;
    return true;
}

}  // namespace oracle::content
