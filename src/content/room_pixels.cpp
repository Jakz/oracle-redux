#include "oracle/content/room_pixels.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

namespace oracle::content {
namespace {

constexpr std::size_t ages_tileset_assignments = 0x112d4;
constexpr std::size_t seasons_tileset_assignments = 0x1133c;
constexpr std::size_t ages_tileset_data = 0x10f9c;
constexpr std::size_t seasons_tileset_data = 0x10c84;
constexpr std::size_t ages_tileset_header_table = 0x787e;
constexpr std::size_t seasons_tileset_header_table = 0x7964;
constexpr std::size_t ages_dictionary_table = 0x7870;
constexpr std::size_t seasons_dictionary_table = 0x794e;
constexpr std::uint8_t ages_tile_mapping_bank = 0x18;
constexpr std::uint8_t seasons_tile_mapping_bank = 0x17;
constexpr std::size_t ages_graphics_header_table = 0x69da;
constexpr std::size_t seasons_graphics_header_table = 0x6926;
constexpr std::size_t ages_unique_graphics_table = 0x11b28;
constexpr std::size_t seasons_unique_graphics_table = 0x1195e;
constexpr std::size_t ages_palette_header_table = 0x632c;
constexpr std::size_t seasons_palette_header_table = 0x6290;
constexpr std::uint8_t ages_palette_data_bank = 0x17;
constexpr std::uint8_t seasons_palette_data_bank = 0x16;
constexpr std::size_t ages_animation_group_table = 0x11b52;
constexpr std::size_t seasons_animation_group_table = 0x119b0;
constexpr std::size_t ages_animation_graphics_headers = 0x11be9;
constexpr std::size_t seasons_animation_graphics_headers = 0x11a48;
constexpr std::size_t tileset_record_size = 8;
constexpr std::size_t graphics_header_entry_size = 6;
constexpr std::size_t vram_size = 0x2000;
constexpr std::size_t tile_bytes = 16;
constexpr std::size_t metatile_count = 256;

struct CampaignOffsets {
    std::size_t tileset_assignments{};
    std::size_t tileset_data{};
    std::size_t tileset_header_table{};
    std::size_t dictionary_table{};
    std::uint8_t tile_mapping_bank{};
    std::size_t graphics_header_table{};
    std::size_t unique_graphics_table{};
    std::size_t palette_header_table{};
    std::uint8_t palette_data_bank{};
    std::size_t animation_group_table{};
    std::size_t animation_graphics_headers{};
};

struct MetatileMapping {
    std::array<std::uint8_t, 4> tile_indices{};
    std::array<std::uint8_t, 4> attributes{};
};

using MetatileMappings = std::array<MetatileMapping, metatile_count>;
using Vram = std::array<std::array<std::uint8_t, vram_size>, 2>;
using BackgroundPalettes =
    std::array<std::array<RgbaPixel, 4>, 8>;

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            .tileset_assignments = ages_tileset_assignments,
            .tileset_data = ages_tileset_data,
            .tileset_header_table = ages_tileset_header_table,
            .dictionary_table = ages_dictionary_table,
            .tile_mapping_bank = ages_tile_mapping_bank,
            .graphics_header_table = ages_graphics_header_table,
            .unique_graphics_table = ages_unique_graphics_table,
            .palette_header_table = ages_palette_header_table,
            .palette_data_bank = ages_palette_data_bank,
            .animation_group_table = ages_animation_group_table,
            .animation_graphics_headers =
                ages_animation_graphics_headers,
        };
    }
    return CampaignOffsets{
        .tileset_assignments = seasons_tileset_assignments,
        .tileset_data = seasons_tileset_data,
        .tileset_header_table = seasons_tileset_header_table,
        .dictionary_table = seasons_dictionary_table,
        .tile_mapping_bank = seasons_tile_mapping_bank,
        .graphics_header_table = seasons_graphics_header_table,
        .unique_graphics_table = seasons_unique_graphics_table,
        .palette_header_table = seasons_palette_header_table,
        .palette_data_bank = seasons_palette_data_bank,
        .animation_group_table = seasons_animation_group_table,
        .animation_graphics_headers =
            seasons_animation_graphics_headers,
    };
}

std::uint16_t read_big_u16(
    const RomSource& rom,
    const std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(rom.read_byte(offset)) << 8u) |
        rom.read_byte(offset + 1));
}

std::size_t bank_one_pointer(
    const RomSource& rom,
    const std::size_t table,
    const std::size_t index) {
    return rom.banked_file_offset(
        1,
        rom.read_little_u16(table + index * 2));
}

std::uint8_t rotate_right(const std::uint8_t value) {
    return static_cast<std::uint8_t>(
        (value >> 1u) | ((value & 1u) << 7u));
}

std::uint8_t checked_source_byte(
    const std::span<const std::uint8_t> source,
    std::size_t& offset) {
    if (offset >= source.size()) {
        throw std::runtime_error{"compressed graphics are truncated"};
    }
    return source[offset++];
}

std::vector<std::uint8_t> decompress_dictionary_data(
    const RomSource& rom,
    std::size_t source_offset,
    const std::span<const std::uint8_t> dictionary,
    const std::uint8_t mode,
    const std::size_t output_size) {
    std::vector<std::uint8_t> output;
    output.reserve(output_size);
    while (output.size() < output_size) {
        const auto key = rom.read_byte(source_offset++);
        for (std::uint8_t bit = 0;
             bit < 8 && output.size() < output_size;
             ++bit) {
            if ((key & (1u << bit)) == 0) {
                output.push_back(rom.read_byte(source_offset++));
                continue;
            }

            std::size_t length{};
            std::size_t dictionary_offset{};
            if (mode == 0) {
                const auto packed = rom.read_little_u16(source_offset);
                source_offset += 2;
                length = static_cast<std::size_t>(packed >> 12u) + 3;
                dictionary_offset = packed & 0x0fff;
            } else if (mode == 1) {
                length = rom.read_byte(source_offset++);
                dictionary_offset = rom.read_little_u16(source_offset);
                source_offset += 2;
            } else {
                throw std::runtime_error{
                    "unsupported tileset dictionary compression mode"};
            }
            if (dictionary_offset + length > dictionary.size()) {
                throw std::runtime_error{
                    "tileset dictionary reference exceeds ROM data"};
            }
            const auto remaining = output_size - output.size();
            const auto count = std::min(length, remaining);
            output.insert(
                output.end(),
                dictionary.begin() +
                    static_cast<std::ptrdiff_t>(dictionary_offset),
                dictionary.begin() +
                    static_cast<std::ptrdiff_t>(dictionary_offset + count));
        }
    }
    return output;
}

MetatileMappings decode_mappings(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t mapping_index) {
    const auto header =
        bank_one_pointer(
            rom,
            offsets.tileset_header_table,
            mapping_index);
    const auto dictionary_index = rom.read_byte(header);
    const auto dictionary_header =
        bank_one_pointer(
            rom,
            offsets.dictionary_table,
            dictionary_index);
    const auto dictionary_bank =
        static_cast<std::uint8_t>(
            rom.read_byte(dictionary_header) & 0x7f);
    const auto dictionary_mode =
        static_cast<std::uint8_t>(
            rom.read_byte(dictionary_header) >> 7u);
    const auto dictionary_offset =
        rom.banked_file_offset(
            dictionary_bank,
            read_big_u16(rom, dictionary_header + 1));
    const auto dictionary = rom.bytes().subspan(dictionary_offset);

    const auto data_offset =
        rom.banked_file_offset(
            rom.read_byte(header + 1),
            read_big_u16(rom, header + 2));
    const auto data_size =
        static_cast<std::size_t>(
            read_big_u16(rom, header + 6) & 0x7fff);
    const auto mapping_indices =
        decompress_dictionary_data(
            rom,
            data_offset,
            dictionary,
            dictionary_mode,
            data_size);
    if (mapping_indices.size() != metatile_count * 2) {
        throw std::runtime_error{
            "tileset mapping index stream has an unexpected size"};
    }

    const auto mapping_bank_offset =
        static_cast<std::size_t>(offsets.tile_mapping_bank) * 0x4000;
    const auto tile_indices_base =
        rom.banked_file_offset(
            offsets.tile_mapping_bank,
            rom.read_little_u16(mapping_bank_offset));
    const auto attributes_base =
        rom.banked_file_offset(
            offsets.tile_mapping_bank,
            rom.read_little_u16(mapping_bank_offset + 2));
    const auto mapping_table =
        rom.banked_file_offset(offsets.tile_mapping_bank, 0x4004);

    MetatileMappings mappings{};
    for (std::size_t index = 0; index < mappings.size(); ++index) {
        const auto mapping = static_cast<std::uint16_t>(
            mapping_indices[index * 2] |
            (static_cast<std::uint16_t>(
                 mapping_indices[index * 2 + 1])
             << 8u));
        const auto record =
            mapping_table + static_cast<std::size_t>(mapping) * 3;
        const auto middle = rom.read_byte(record + 1);
        const auto tile_offset =
            static_cast<std::size_t>(
                rom.read_byte(record) |
                ((middle & 0xf0u) << 4u)) *
            4;
        const auto attribute_offset =
            static_cast<std::size_t>(
                rom.read_byte(record + 2) |
                ((middle & 0x0fu) << 8u)) *
            4;
        for (std::size_t quadrant = 0; quadrant < 4; ++quadrant) {
            mappings[index].tile_indices[quadrant] =
                rom.read_byte(tile_indices_base + tile_offset + quadrant);
            mappings[index].attributes[quadrant] =
                rom.read_byte(
                    attributes_base + attribute_offset + quadrant);
        }
    }
    return mappings;
}

void load_graphics_entry(
    const RomSource& rom,
    const std::size_t entry,
    Vram& vram) {
    const auto bank_and_mode = rom.read_byte(entry);
    const auto source_bank =
        static_cast<std::uint8_t>(bank_and_mode & 0x3f);
    const auto mode =
        static_cast<std::uint8_t>(bank_and_mode >> 6u);
    const auto source_address = read_big_u16(rom, entry + 1);
    const auto encoded_destination = read_big_u16(rom, entry + 3);
    const auto destination_bank =
        static_cast<std::size_t>(encoded_destination & 0x000f);
    const auto destination =
        static_cast<std::uint16_t>(encoded_destination & 0xfff0);
    const auto output_size =
        (static_cast<std::size_t>(rom.read_byte(entry + 5) & 0x7f) + 1) *
        16;

    if (source_address < 0x4000 || source_address >= 0x8000) {
        return;
    }
    if (
        destination < 0x8000 || destination >= 0xa000 ||
        destination_bank >= vram.size()) {
        return;
    }
    const auto destination_offset =
        static_cast<std::size_t>(destination - 0x8000);
    if (destination_offset + output_size > vram_size) {
        throw std::runtime_error{
            "graphics header writes beyond emulated VRAM"};
    }

    const auto source_offset =
        rom.banked_file_offset(source_bank, source_address);
    const auto decoded = RoomPixelDecoder::decompress_graphics(
        rom.bytes().subspan(source_offset),
        output_size,
        mode);
    std::copy(
        decoded.begin(),
        decoded.end(),
        vram[destination_bank].begin() +
            static_cast<std::ptrdiff_t>(destination_offset));
}

void load_graphics_header(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t index,
    Vram& vram) {
    auto entry =
        bank_one_pointer(rom, offsets.graphics_header_table, index);
    bool repeat = false;
    do {
        load_graphics_entry(rom, entry, vram);
        repeat = (rom.read_byte(entry + 5) & 0x80) != 0;
        entry += graphics_header_entry_size;
    } while (repeat);
}

void load_initial_animation_frames(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t group_index,
    Vram& vram) {
    if (group_index == 0xff) {
        return;
    }
    const auto group =
        rom.banked_file_offset(
            4,
            rom.read_little_u16(
                offsets.animation_group_table +
                static_cast<std::size_t>(group_index) * 2));
    const auto state = rom.read_byte(group);
    const std::size_t initial_frame =
        rom.metadata().campaign == core::Campaign::ages ? 2 : 0;
    for (std::size_t slot = 0; slot < 4; ++slot) {
        if ((state & (1u << slot)) == 0) {
            continue;
        }
        const auto animation =
            rom.banked_file_offset(
                4,
                rom.read_little_u16(group + 1 + slot * 2));
        const auto graphics_index =
            rom.read_byte(animation + initial_frame * 2 + 1);
        const auto graphics_entry =
            offsets.animation_graphics_headers +
            static_cast<std::size_t>(graphics_index) *
                graphics_header_entry_size;
        load_graphics_entry(rom, graphics_entry, vram);
    }
}

std::vector<std::uint8_t> unique_palette_overrides(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t index,
    Vram& vram) {
    std::vector<std::uint8_t> palettes;
    const auto normalized = static_cast<std::uint8_t>(index & 0x7f);
    if (normalized == 0) {
        return palettes;
    }

    auto entry =
        rom.banked_file_offset(
            4,
            rom.read_little_u16(
                offsets.unique_graphics_table +
                static_cast<std::size_t>(normalized) * 2));
    bool repeat = false;
    do {
        if (rom.read_byte(entry) == 0) {
            palettes.push_back(
                static_cast<std::uint8_t>(
                    rom.read_byte(entry + 1) & 0x7f));
            break;
        }
        load_graphics_entry(rom, entry, vram);
        repeat = (rom.read_byte(entry + 5) & 0x80) != 0;
        entry += graphics_header_entry_size;
    } while (repeat);
    return palettes;
}

RgbaPixel decode_color(const std::uint16_t color) {
    const auto expand = [](const std::uint16_t channel) {
        return static_cast<std::uint8_t>(
            (channel * 255u + 15u) / 31u);
    };
    return RgbaPixel{
        .red = expand(color & 0x1f),
        .green = expand((color >> 5u) & 0x1f),
        .blue = expand((color >> 10u) & 0x1f),
        .alpha = 255,
    };
}

void load_palette_header(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t index,
    BackgroundPalettes& palettes) {
    auto entry =
        bank_one_pointer(rom, offsets.palette_header_table, index);
    bool repeat = false;
    do {
        const auto flags = rom.read_byte(entry);
        const auto count =
            static_cast<std::size_t>(flags & 0x07) + 1;
        const auto start =
            static_cast<std::size_t>((flags >> 3u) & 0x07);
        const bool sprite = (flags & 0x40) != 0;
        repeat = (flags & 0x80) != 0;
        const auto pointer = rom.read_little_u16(entry + 1);
        if (!sprite && pointer < 0x8000) {
            const auto data =
                rom.banked_file_offset(
                    offsets.palette_data_bank,
                    pointer);
            if (start + count > palettes.size()) {
                throw std::runtime_error{
                    "palette header writes beyond background palette RAM"};
            }
            for (std::size_t palette = 0; palette < count; ++palette) {
                for (std::size_t color = 0; color < 4; ++color) {
                    palettes[start + palette][color] =
                        decode_color(
                            rom.read_little_u16(
                                data + palette * 8 + color * 2));
                }
            }
        }
        entry += 3;
    } while (repeat);
}

std::uint8_t tile_color_index(
    const Vram& vram,
    const std::uint8_t tile,
    const std::uint8_t attribute,
    std::size_t x,
    std::size_t y) {
    if ((attribute & 0x20) != 0) {
        x = 7 - x;
    }
    if ((attribute & 0x40) != 0) {
        y = 7 - y;
    }
    const auto bank = static_cast<std::size_t>((attribute >> 3u) & 1u);
    const auto tile_offset =
        static_cast<std::size_t>(tile) * tile_bytes + y * 2;
    const auto low = vram[bank][tile_offset];
    const auto high = vram[bank][tile_offset + 1];
    const auto bit = static_cast<std::uint8_t>(7u - x);
    return static_cast<std::uint8_t>(
        ((low >> bit) & 1u) | (((high >> bit) & 1u) << 1u));
}

}  // namespace

RoomPixelDecoder::RoomPixelDecoder(const RomSource& rom) : rom_{rom} {}

TilesetDescriptor RoomPixelDecoder::describe_tileset(
    const std::uint8_t layout_group,
    const std::uint8_t room,
    const Season season) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto group_pointer =
        rom_.read_little_u16(
            offsets.tileset_assignments +
            static_cast<std::size_t>(layout_group) * 2);
    const auto group =
        rom_.banked_file_offset(4, group_pointer);
    const auto assignment = rom_.read_byte(group + room);
    const auto index = static_cast<std::uint8_t>(assignment & 0x7f);
    auto record =
        offsets.tileset_data +
        static_cast<std::size_t>(index) * tileset_record_size;
    if (
        rom_.metadata().campaign == core::Campaign::seasons &&
        rom_.read_byte(record) == 0xff) {
        const auto season_table =
            rom_.banked_file_offset(
                4,
                rom_.read_little_u16(record + 1));
        record =
            season_table +
            static_cast<std::size_t>(season) * tileset_record_size;
    }

    return TilesetDescriptor{
        .assignment = assignment,
        .index = index,
        .flags = rom_.read_byte(record + 1),
        .unique_graphics = static_cast<std::uint8_t>(
            rom_.read_byte(record + 2) | (assignment & 0x80)),
        .main_graphics = rom_.read_byte(record + 3),
        .palette = rom_.read_byte(record + 4),
        .mapping = rom_.read_byte(record + 5),
        .layout_group = rom_.read_byte(record + 6),
        .animation = rom_.read_byte(record + 7),
    };
}

RenderedRoom RoomPixelDecoder::render(
    const RoomLayout& room,
    const Season season) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto descriptor =
        describe_tileset(
            static_cast<std::uint8_t>(room.id.area),
            static_cast<std::uint8_t>(room.id.room),
            season);
    auto mappings =
        decode_mappings(rom_, offsets, descriptor.mapping);
    Vram vram{};
    load_graphics_header(
        rom_,
        offsets,
        descriptor.main_graphics,
        vram);
    const auto palette_overrides =
        unique_palette_overrides(
            rom_,
            offsets,
            descriptor.unique_graphics,
            vram);
    load_initial_animation_frames(
        rom_,
        offsets,
        descriptor.animation,
        vram);
    BackgroundPalettes palettes{};
    load_palette_header(rom_, offsets, 0x0f, palettes);
    load_palette_header(rom_, offsets, descriptor.palette, palettes);
    for (const auto palette : palette_overrides) {
        load_palette_header(rom_, offsets, palette, palettes);
    }

    if (
        rom_.metadata().campaign == core::Campaign::ages &&
        (descriptor.flags & 0x80) != 0 &&
        room.id.room != 0x38) {
        for (std::size_t metatile = 0x40; metatile < 0x80; ++metatile) {
            for (auto& attribute : mappings[metatile].attributes) {
                if ((attribute & 0x07) == 6) {
                    attribute =
                        static_cast<std::uint8_t>(attribute & 0xf8);
                }
            }
        }
    }

    RenderedRoom rendered{
        .id = room.id,
        .tileset = descriptor,
    };
    for (std::size_t metatile_y = 0;
         metatile_y < small_room_rows;
         ++metatile_y) {
        for (std::size_t metatile_x = 0;
             metatile_x < small_room_columns;
             ++metatile_x) {
            const auto metatile =
                room.metatiles[
                    metatile_y * small_room_columns + metatile_x];
            const auto& mapping = mappings[metatile];
            for (std::size_t quadrant = 0; quadrant < 4; ++quadrant) {
                const auto quadrant_x = quadrant & 1u;
                const auto quadrant_y = quadrant >> 1u;
                const auto attribute = mapping.attributes[quadrant];
                const auto palette =
                    static_cast<std::size_t>(attribute & 0x07);
                for (std::size_t y = 0; y < 8; ++y) {
                    for (std::size_t x = 0; x < 8; ++x) {
                        const auto destination_x =
                            metatile_x * 16 + quadrant_x * 8 + x;
                        const auto destination_y =
                            metatile_y * 16 + quadrant_y * 8 + y;
                        const auto color = tile_color_index(
                            vram,
                            mapping.tile_indices[quadrant],
                            attribute,
                            x,
                            y);
                        rendered.pixels[
                            destination_y * small_room_world_width +
                            destination_x] = palettes[palette][color];
                    }
                }
            }
        }
    }
    return rendered;
}

std::vector<std::uint8_t> RoomPixelDecoder::decompress_graphics(
    const std::span<const std::uint8_t> source,
    const std::size_t output_size,
    const std::uint8_t mode) {
    if (mode > 3) {
        throw std::invalid_argument{
            "unsupported graphics compression mode"};
    }
    if (mode == 0) {
        if (source.size() < output_size) {
            throw std::runtime_error{"uncompressed graphics are truncated"};
        }
        return std::vector<std::uint8_t>{
            source.begin(),
            source.begin() + static_cast<std::ptrdiff_t>(output_size),
        };
    }

    std::vector<std::uint8_t> output;
    output.reserve(output_size);
    std::size_t source_offset = 0;
    if (mode == 2) {
        while (output.size() < output_size) {
            auto first_mask =
                checked_source_byte(source, source_offset);
            auto second_mask =
                checked_source_byte(source, source_offset);
            std::uint8_t carry = 0;
            if ((first_mask | second_mask) == 0) {
                for (std::size_t index = 0; index < 16; ++index) {
                    output.push_back(
                        checked_source_byte(source, source_offset));
                }
                continue;
            }
            const auto repeated =
                checked_source_byte(source, source_offset);
            for (std::size_t index = 0; index < 8; ++index) {
                const auto next_carry =
                    static_cast<std::uint8_t>(
                        (first_mask & 0x80) >> 7u);
                first_mask = static_cast<std::uint8_t>(
                    (first_mask << 1u) | carry);
                carry = next_carry;
                output.push_back(
                    carry == 0
                    ? checked_source_byte(source, source_offset)
                    : repeated);
            }
            for (std::size_t index = 0; index < 8; ++index) {
                const auto next_carry =
                    static_cast<std::uint8_t>(
                        (second_mask & 0x80) >> 7u);
                second_mask = static_cast<std::uint8_t>(
                    (second_mask << 1u) | carry);
                carry = next_carry;
                output.push_back(
                    carry == 0
                    ? checked_source_byte(source, source_offset)
                    : repeated);
            }
        }
        if (output.size() > output_size) {
            output.resize(output_size);
        }
        return output;
    }

    std::uint8_t control = 0;
    std::uint8_t control_bits = 0;
    while (output.size() < output_size) {
        if (control_bits == 0) {
            control = checked_source_byte(source, source_offset);
            control_bits = 8;
        }
        const bool back_reference = (control & 0x80) != 0;
        control = static_cast<std::uint8_t>(control << 1u);
        --control_bits;
        if (!back_reference) {
            output.push_back(
                checked_source_byte(source, source_offset));
            continue;
        }

        std::uint16_t distance_minus_one{};
        std::size_t length{};
        if (mode == 1) {
            const auto packed =
                checked_source_byte(source, source_offset);
            distance_minus_one = packed & 0x1f;
            auto encoded_length =
                static_cast<std::uint8_t>(packed ^ distance_minus_one);
            if (encoded_length == 0) {
                length =
                    checked_source_byte(source, source_offset);
            } else {
                encoded_length = static_cast<std::uint8_t>(
                    (encoded_length << 4u) |
                    (encoded_length >> 4u));
                length =
                    static_cast<std::size_t>(
                        rotate_right(encoded_length)) +
                    1;
            }
        } else {
            const auto low =
                checked_source_byte(source, source_offset);
            const auto packed =
                checked_source_byte(source, source_offset);
            const auto high =
                static_cast<std::uint8_t>(packed & 0x07);
            distance_minus_one = static_cast<std::uint16_t>(
                low | (static_cast<std::uint16_t>(high) << 8u));
            auto encoded_length =
                static_cast<std::uint8_t>(packed ^ high);
            if (encoded_length == 0) {
                length =
                    checked_source_byte(source, source_offset);
            } else {
                encoded_length = rotate_right(encoded_length);
                encoded_length = rotate_right(encoded_length);
                encoded_length = rotate_right(encoded_length);
                length =
                    static_cast<std::size_t>(encoded_length) + 2;
            }
        }
        if (length == 0) {
            length = 256;
        }
        const auto distance =
            static_cast<std::size_t>(distance_minus_one) + 1;
        for (std::size_t index = 0;
             index < length && output.size() < output_size;
             ++index) {
            if (distance > output.size()) {
                output.push_back(0);
            } else {
                output.push_back(output[output.size() - distance]);
            }
        }
    }
    return output;
}

}  // namespace oracle::content
