#include "oracle/content/room_topology.h"

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace oracle::content {
namespace {

constexpr std::uint8_t warp_bank = 4;
constexpr std::size_t ages_warp_destination_table = 0x12f5b;
constexpr std::size_t ages_warp_source_table = 0x1359e;
constexpr std::size_t seasons_warp_destination_table = 0x12d4e;
constexpr std::size_t seasons_warp_source_table = 0x13457;
constexpr std::size_t maximum_warp_records = 4096;

struct CampaignOffsets {
    std::size_t destination_table{};
    std::size_t source_table{};
};

struct SourceRecord {
    std::uint8_t opcode{};
    std::uint8_t map_or_position{};
    std::uint8_t destination_index{};
    std::uint8_t destination_group{};
    std::uint8_t source_transition{};
    std::uint16_t pointer{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            .destination_table = ages_warp_destination_table,
            .source_table = ages_warp_source_table,
        };
    }
    return CampaignOffsets{
        .destination_table = seasons_warp_destination_table,
        .source_table = seasons_warp_source_table,
    };
}

SourceRecord read_source_record(
    const RomSource& rom,
    const std::size_t offset) {
    const auto opcode = rom.read_byte(offset);
    const auto map_or_position = rom.read_byte(offset + 1);
    if ((opcode & 0x40) != 0) {
        return SourceRecord{
            .opcode = opcode,
            .map_or_position = map_or_position,
            .pointer = rom.read_little_u16(offset + 2),
        };
    }
    const auto group_and_transition = rom.read_byte(offset + 3);
    return SourceRecord{
        .opcode = opcode,
        .map_or_position = map_or_position,
        .destination_index = rom.read_byte(offset + 2),
        .destination_group = static_cast<std::uint8_t>(
            group_and_transition >> 4u),
        .source_transition = static_cast<std::uint8_t>(
            group_and_transition & 0x0f),
        .pointer = 0,
    };
}

WarpDestination resolve_destination(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t group,
    const std::uint8_t base_index,
    const std::uint8_t destination_variant) {
    if (group >= 8) {
        throw std::runtime_error{
            "warp source refers to an invalid destination group"};
    }
    auto index = base_index;
    if (
        rom.metadata().campaign == core::Campaign::seasons &&
        group == 2) {
        index = static_cast<std::uint8_t>(
            index + (destination_variant & 0x03));
    }
    const auto table =
        rom.banked_file_offset(
            warp_bank,
            rom.read_little_u16(
                offsets.destination_table +
                static_cast<std::size_t>(group) * 2));
    const auto table_end = group < 7
        ? rom.banked_file_offset(
              warp_bank,
              rom.read_little_u16(
                  offsets.destination_table +
                  static_cast<std::size_t>(group + 1) * 2))
        : offsets.source_table;
    const auto record =
        table + static_cast<std::size_t>(index) * 3;
    if (record + 3 > table_end) {
        throw std::runtime_error{
            "warp destination index exceeds its group table"};
    }
    const auto parameter_and_transition = rom.read_byte(record + 2);
    return WarpDestination{
        .group = group,
        .index = index,
        .room = rom.read_byte(record),
        .position = rom.read_byte(record + 1),
        .parameter = static_cast<std::uint8_t>(
            parameter_and_transition >> 4u),
        .transition = static_cast<std::uint8_t>(
            parameter_and_transition & 0x0f),
    };
}

RoomExit make_warp_exit(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t source_group,
    const std::uint8_t source_room,
    const SourceRecord& source,
    const RoomExitKind kind,
    const bool has_source_position,
    const bool fallback,
    const std::uint8_t destination_variant) {
    const auto destination =
        resolve_destination(
            rom,
            offsets,
            source.destination_group,
            source.destination_index,
            destination_variant);
    return RoomExit{
        .kind = kind,
        .source =
            core::WorldRoomId{
                .area = source_group,
                .room = source_room,
            },
        .destination =
            core::WorldRoomId{
                .area = destination.group,
                .room = destination.room,
            },
        .has_source_position = has_source_position,
        .source_position =
            has_source_position
            ? source.map_or_position
            : static_cast<std::uint8_t>(0),
        .source_edge_mask = kind == RoomExitKind::screen_edge_warp
            ? static_cast<std::uint8_t>(source.opcode & 0x0f)
            : static_cast<std::uint8_t>(0),
        .source_transition = source.source_transition,
        .destination_position = destination.position,
        .destination_parameter = destination.parameter,
        .destination_transition = destination.transition,
        .destination_index = destination.index,
        .fallback = fallback,
    };
}

std::vector<RoomExit> pointed_exits(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t source_group,
    const std::uint8_t source_room,
    const std::uint16_t pointer,
    const std::size_t group_end,
    const std::uint8_t destination_variant) {
    std::vector<RoomExit> result;
    auto offset = rom.banked_file_offset(warp_bank, pointer);
    for (std::size_t count = 0; count < maximum_warp_records; ++count) {
        if (offset >= group_end) {
            return result;
        }
        const auto source = read_source_record(rom, offset);
        if (source.opcode == 0xff) {
            return result;
        }
        if ((source.opcode & 0x40) != 0) {
            std::ostringstream message;
            message
                << "pointed warp list has an invalid record at bank-4 "
                << "pointer 0x" << std::hex << pointer
                << ", file offset 0x" << offset
                << ", opcode 0x"
                << static_cast<unsigned int>(source.opcode);
            throw std::runtime_error{
                message.str()};
        }
        const bool fallback = (source.opcode & 0x80) != 0;
        result.push_back(
            make_warp_exit(
                rom,
                offsets,
                source_group,
                source_room,
                source,
                fallback
                    ? RoomExitKind::fallback_warp
                    : RoomExitKind::tile_warp,
                true,
                fallback,
                destination_variant));
        offset += 4;
        if (fallback) {
            return result;
        }
    }
    throw std::runtime_error{"pointed warp list does not terminate"};
}

}  // namespace

RoomTopologyDecoder::RoomTopologyDecoder(const RomSource& rom) : rom_{rom} {}

std::vector<WarpDestination>
RoomTopologyDecoder::warp_destinations(const std::uint8_t group) const {
    if (group >= 8) {
        throw std::invalid_argument{
            "warp destination group must be between 0 and 7"};
    }
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto start =
        rom_.banked_file_offset(
            warp_bank,
            rom_.read_little_u16(
                offsets.destination_table +
                static_cast<std::size_t>(group) * 2));
    const auto end = group < 7
        ? rom_.banked_file_offset(
              warp_bank,
              rom_.read_little_u16(
                  offsets.destination_table +
                  static_cast<std::size_t>(group + 1) * 2))
        : offsets.source_table;
    if (end < start || (end - start) % 3 != 0) {
        throw std::runtime_error{
            "warp destination table boundaries are invalid"};
    }

    std::vector<WarpDestination> result;
    const auto count = (end - start) / 3;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto record = start + index * 3;
        const auto parameter_and_transition = rom_.read_byte(record + 2);
        result.push_back(
            WarpDestination{
                .group = group,
                .index = static_cast<std::uint8_t>(index),
                .room = rom_.read_byte(record),
                .position = rom_.read_byte(record + 1),
                .parameter = static_cast<std::uint8_t>(
                    parameter_and_transition >> 4u),
                .transition = static_cast<std::uint8_t>(
                    parameter_and_transition & 0x0f),
            });
    }
    return result;
}

std::vector<RoomExit> RoomTopologyDecoder::exits(
    const std::uint8_t group,
    const std::uint8_t room,
    const std::uint8_t destination_variant,
    const bool include_spatial_seams) const {
    if (group >= 8) {
        throw std::invalid_argument{
            "warp source group must be between 0 and 7"};
    }
    auto result = include_spatial_seams
        ? spatial_seams(group, room)
        : std::vector<RoomExit>{};
    const auto offsets = offsets_for(rom_.metadata().campaign);
    auto offset =
        rom_.banked_file_offset(
            warp_bank,
            rom_.read_little_u16(
                offsets.source_table +
                static_cast<std::size_t>(group) * 2));
    const auto group_end = group < 7
        ? rom_.banked_file_offset(
              warp_bank,
              rom_.read_little_u16(
                  offsets.source_table +
                  static_cast<std::size_t>(group + 1) * 2))
        : rom_.bytes().size();
    std::optional<SourceRecord> group_fallback;
    bool found_tile_warp = false;

    for (std::size_t count = 0; count < maximum_warp_records; ++count) {
        const auto source = read_source_record(rom_, offset);
        if (source.opcode == 0xff) {
            break;
        }
        const bool fallback = (source.opcode & 0x80) != 0;
        if (fallback) {
            group_fallback = source;
            break;
        }
        if ((source.opcode & 0x40) != 0) {
            if (source.map_or_position == room) {
                const auto pointed =
                    pointed_exits(
                        rom_,
                        offsets,
                        group,
                        room,
                        source.pointer,
                        group_end,
                        destination_variant);
                result.insert(
                    result.end(),
                    pointed.begin(),
                    pointed.end());
                found_tile_warp = true;
            }
        } else {
            const auto edge_mask =
                static_cast<std::uint8_t>(source.opcode & 0x0f);
            if (source.map_or_position == room) {
                result.push_back(
                    make_warp_exit(
                        rom_,
                        offsets,
                        group,
                        room,
                        source,
                        edge_mask == 0
                            ? RoomExitKind::tile_warp
                            : RoomExitKind::screen_edge_warp,
                        false,
                        false,
                        destination_variant));
                found_tile_warp = found_tile_warp || edge_mask == 0;
            }
        }
        offset += 4;
    }

    if (!found_tile_warp && group_fallback.has_value()) {
        result.push_back(
            make_warp_exit(
                rom_,
                offsets,
                group,
                room,
                *group_fallback,
                RoomExitKind::fallback_warp,
                false,
                true,
                destination_variant));
    }
    return result;
}

std::vector<RoomExit> RoomTopologyDecoder::spatial_seams(
    const std::uint8_t group,
    const std::uint8_t room) {
    std::vector<RoomExit> result;
    if (group >= 4) {
        return result;
    }
    const auto source =
        core::WorldRoomId{.area = group, .room = room};
    const auto append =
        [&](const RoomExitKind kind, const std::uint8_t destination_room) {
            result.push_back(
                RoomExit{
                    .kind = kind,
                    .source = source,
                    .destination =
                        core::WorldRoomId{
                            .area = group,
                            .room = destination_room,
                        },
                });
        };
    if ((room >> 4u) != 0) {
        append(
            RoomExitKind::north_seam,
            static_cast<std::uint8_t>(room - 0x10));
    }
    if ((room & 0x0f) != 0x0f) {
        append(
            RoomExitKind::east_seam,
            static_cast<std::uint8_t>(room + 1));
    }
    if ((room >> 4u) != 0x0f) {
        append(
            RoomExitKind::south_seam,
            static_cast<std::uint8_t>(room + 0x10));
    }
    if ((room & 0x0f) != 0) {
        append(
            RoomExitKind::west_seam,
            static_cast<std::uint8_t>(room - 1));
    }
    return result;
}

}  // namespace oracle::content
