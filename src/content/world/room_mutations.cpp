#include "oracle/content/room_mutations.h"

#include "oracle/content/chest_data.h"

#include <array>
#include <stdexcept>
#include <vector>

namespace oracle::content {
namespace {

constexpr std::size_t ages_standard_substitutions = 0x120f5;
constexpr std::size_t seasons_standard_substitutions = 0x11e26;
constexpr std::array<std::uint8_t, 5> substitution_flag_bits{
    0,
    1,
    2,
    3,
    7,
};

}  // namespace

RoomMutationDecoder::RoomMutationDecoder(const RomSource& rom) : rom_{rom} {}

RoomLayout RoomMutationDecoder::apply_standard_substitutions(
    RoomLayout room,
    const TilesetDescriptor& tileset,
    const std::uint8_t room_flags) const {
    const bool ages =
        rom_.metadata().campaign == core::Campaign::ages;
    const auto table =
        ages
        ? ages_standard_substitutions
        : seasons_standard_substitutions;
    const std::size_t selector_count = ages ? 6 : 8;
    const auto selector =
        ages
        ? static_cast<std::size_t>(tileset.collision_mode)
        : static_cast<std::size_t>(room.id.area);
    if (selector >= selector_count) {
        throw std::runtime_error{
            "room substitution selector exceeds cartridge table"};
    }

    for (std::size_t flag = 0;
         flag < substitution_flag_bits.size();
         ++flag) {
        const auto bit = substitution_flag_bits[flag];
        if ((room_flags & (1u << bit)) == 0) {
            continue;
        }
        const auto pointer_entry =
            table + (flag * selector_count + selector) * 2;
        auto replacements_offset =
            rom_.banked_file_offset(
                4,
                rom_.read_little_u16(pointer_entry));
        std::vector<TileReplacement> replacements;
        while (true) {
            const auto replacement =
                rom_.read_byte(replacements_offset++);
            if (replacement == 0) {
                break;
            }
            replacements.push_back(TileReplacement{
                .replacement = replacement,
                .target = rom_.read_byte(replacements_offset++),
            });
        }
        apply_replacements(room.metatiles, replacements);
    }
    if ((room_flags & room_flag_item) != 0) {
        const ChestDataDecoder chest_decoder{rom_};
        const auto chest = chest_decoder.find(room.id);
        if (chest.has_value()) {
            static_cast<void>(
                ChestDataDecoder::apply_opened_chest(
                    room,
                    *chest,
                    room_flags));
        }
    }
    return room;
}

void RoomMutationDecoder::apply_replacements(
    const std::span<std::uint8_t> metatiles,
    const std::span<const TileReplacement> replacements) {
    for (const auto replacement : replacements) {
        for (auto& metatile : metatiles) {
            if (metatile == replacement.target) {
                metatile = replacement.replacement;
            }
        }
    }
}

}  // namespace oracle::content
