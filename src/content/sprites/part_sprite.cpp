#include "oracle/content/part_sprite.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "oracle/content/part_data.h"

namespace oracle::content {
namespace {

constexpr std::uint8_t octorok_projectile_part_id = 0x18;
constexpr std::uint8_t item_drop_part_id = 0x01;
constexpr std::uint8_t enemy_destroyed_part_id = 0x02;
constexpr std::size_t part_graphics_size = 0x200;
constexpr std::size_t tile_bytes = 16;
constexpr std::uint8_t common_gameplay_palette_header = 0x0f;
constexpr std::array<std::uint8_t, 16> item_drop_tile_offsets{
    0x00, 0x02, 0x04, 0x06,
    0x10, 0x12, 0x14, 0x16,
    0x18, 0x1a, 0x1c, 0x1e,
    0x0c, 0x0c, 0x0c, 0x08,
};
constexpr std::array<std::uint8_t, 16> item_drop_palettes{
    0x02, 0x05, 0x00, 0x05,
    0x04, 0x02, 0x03, 0x01,
    0x01, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x03, 0x04,
};

using SpritePalettes =
    std::array<std::array<RgbaPixel, 4>, 8>;

struct CampaignOffsets {
    std::size_t object_gfx_header_table{};
    std::size_t part_oam_table{};
    std::uint8_t part_oam_table_bank{};
    std::size_t palette_header_table{};
    std::uint8_t palette_data_bank{};
    std::uint8_t oam_data_bank{};
};

struct OamObject {
    std::int32_t y{};
    std::int32_t x{};
    std::uint8_t tile{};
    std::uint8_t flags{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) noexcept {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            0xfda8a,
            0x5b71e,
            0x16,
            0x632c,
            0x17,
            0x14,
        };
    }
    return CampaignOffsets{
        0xfdafb,
        0x57237,
        0x15,
        0x6290,
        0x16,
        0x13,
    };
}

RgbaPixel decode_color(const std::uint16_t color) {
    const auto expand = [](const std::uint16_t channel) {
        return static_cast<std::uint8_t>(
            (channel * 255u + 15u) / 31u);
    };
    return RgbaPixel{
        expand(color & 0x1f),
        expand((color >> 5u) & 0x1f),
        expand((color >> 10u) & 0x1f),
        255,
    };
}

SpritePalettes load_sprite_palettes(
    const RomSource& rom,
    const CampaignOffsets offsets) {
    SpritePalettes palettes{};
    const auto header_pointer =
        rom.read_little_u16(
            offsets.palette_header_table +
            static_cast<std::size_t>(
                common_gameplay_palette_header) *
                2);
    auto entry = rom.banked_file_offset(1, header_pointer);
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
        if (sprite && pointer < 0x8000) {
            if (start + count > palettes.size()) {
                throw std::runtime_error{
                    "part palette header exceeds sprite palette RAM"};
            }
            const auto data = rom.banked_file_offset(
                offsets.palette_data_bank,
                pointer);
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
    return palettes;
}

std::vector<OamObject> load_oam(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t part_id,
    const std::uint8_t oam_index) {
    const auto pointer_table_address = rom.read_little_u16(
        offsets.part_oam_table +
        static_cast<std::size_t>(part_id) * 2);
    const auto pointer_table = rom.banked_file_offset(
        offsets.part_oam_table_bank,
        pointer_table_address);
    const auto pointer = rom.read_little_u16(
        pointer_table +
        static_cast<std::size_t>(oam_index) * 2);
    auto cursor =
        rom.banked_file_offset(offsets.oam_data_bank, pointer);
    const auto count = rom.read_byte(cursor++);
    std::vector<OamObject> objects;
    objects.reserve(count);
    for (std::uint8_t index = 0; index < count; ++index) {
        objects.push_back(
            OamObject{
                static_cast<std::int8_t>(rom.read_byte(cursor)),
                static_cast<std::int8_t>(rom.read_byte(cursor + 1)),
                static_cast<std::uint8_t>(
                    rom.read_byte(cursor + 2) & 0xfe),
                rom.read_byte(cursor + 3),
            });
        cursor += 4;
    }
    return objects;
}

std::uint8_t tile_color_index(
    const std::vector<std::uint8_t>& graphics,
    const std::size_t tile,
    const std::size_t x,
    const std::size_t y) {
    const auto offset = tile * tile_bytes + y * 2;
    if (offset + 1 >= graphics.size()) {
        throw std::runtime_error{
            "projectile OAM references graphics outside its sheet"};
    }
    const auto bit = static_cast<std::uint8_t>(7u - x);
    return static_cast<std::uint8_t>(
        ((graphics[offset] >> bit) & 1u) |
        (((graphics[offset + 1] >> bit) & 1u) << 1u));
}

}  // namespace

PartSpriteDecoder::PartSpriteDecoder(const RomSource& rom) : rom_{rom} {}

PartSpriteFrame PartSpriteDecoder::decode_octorok_projectile() const {
    const auto definition =
        PartDefinitionDecoder{rom_}.decode(
            octorok_projectile_part_id);
    return decode(
        octorok_projectile_part_id,
        0,
        0,
        static_cast<std::uint8_t>(definition.oam_flags & 0x07));
}

PartSpriteFrame PartSpriteDecoder::decode_enemy_destroyed(
    const std::uint8_t oam_index) const {
    if (oam_index >= 6) {
        throw std::invalid_argument{
            "enemy-destroyed OAM index must be between 0 and 5"};
    }
    const auto definition =
        PartDefinitionDecoder{rom_}.decode(enemy_destroyed_part_id);
    return decode(
        enemy_destroyed_part_id,
        oam_index,
        0,
        static_cast<std::uint8_t>(definition.oam_flags & 0x07));
}

PartSpriteFrame PartSpriteDecoder::decode_item_drop(
    const std::uint8_t subid) const {
    if (subid >= item_drop_tile_offsets.size()) {
        throw std::invalid_argument{
            "item-drop subid must be between 0 and 15"};
    }
    const auto oam_index =
        subid == 0x0f
        ? static_cast<std::uint8_t>(2)
        : (
            subid == 0x01 || (subid >= 0x05 && subid <= 0x0b)
            ? static_cast<std::uint8_t>(1)
            : static_cast<std::uint8_t>(0));
    return decode(
        item_drop_part_id,
        oam_index,
        item_drop_tile_offsets[subid],
        item_drop_palettes[subid]);
}

PartSpriteFrame PartSpriteDecoder::decode(
    const std::uint8_t part_id,
    const std::uint8_t oam_index,
    const std::uint8_t tile_offset,
    const std::uint8_t palette_index) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto definition =
        PartDefinitionDecoder{rom_}.decode(part_id);
    const auto objects =
        load_oam(rom_, offsets, part_id, oam_index);
    if (objects.empty()) {
        throw std::runtime_error{"part frame has no OAM objects"};
    }

    auto minimum_x = std::numeric_limits<std::int32_t>::max();
    auto minimum_y = std::numeric_limits<std::int32_t>::max();
    auto maximum_x = std::numeric_limits<std::int32_t>::min();
    auto maximum_y = std::numeric_limits<std::int32_t>::min();
    for (const auto& object : objects) {
        minimum_x = std::min(minimum_x, object.x - 8);
        minimum_y = std::min(minimum_y, object.y);
        maximum_x = std::max(maximum_x, object.x);
        maximum_y = std::max(maximum_y, object.y + 16);
    }

    const auto graphics_header =
        offsets.object_gfx_header_table +
        static_cast<std::size_t>(
            definition.object_gfx_header) *
            3;
    const auto bank_and_mode = rom_.read_byte(graphics_header);
    const auto graphics_bank =
        static_cast<std::uint8_t>(bank_and_mode & 0x3f);
    const auto compression_mode =
        static_cast<std::uint8_t>(bank_and_mode >> 6u);
    const auto graphics_pointer = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(
             rom_.read_byte(graphics_header + 1)) << 8u) |
        rom_.read_byte(graphics_header + 2));
    const auto graphics_offset =
        rom_.banked_file_offset(graphics_bank, graphics_pointer);
    const auto graphics = RoomPixelDecoder::decompress_graphics(
        rom_.bytes().subspan(graphics_offset),
        part_graphics_size,
        compression_mode);
    const auto palettes = load_sprite_palettes(rom_, offsets);
    const auto palette = static_cast<std::size_t>(palette_index);

    PartSpriteFrame frame{
        .part_id = part_id,
        .original_oam_index = oam_index,
        .origin_x = minimum_x,
        .origin_y = minimum_y,
        .width = maximum_x - minimum_x,
        .height = maximum_y - minimum_y,
    };
    frame.pixels.assign(
        static_cast<std::size_t>(frame.width * frame.height),
        RgbaPixel{.alpha = 0});
    for (const auto& object : objects) {
        const bool flip_x = (object.flags & 0x20) != 0;
        const bool flip_y = (object.flags & 0x40) != 0;
        for (std::size_t destination_y = 0;
             destination_y < 16;
             ++destination_y) {
            const auto source_y =
                flip_y ? 15 - destination_y : destination_y;
            const auto source_tile =
                static_cast<std::size_t>(
                    definition.tile_base +
                    tile_offset +
                    object.tile) +
                source_y / 8;
            const auto tile_y = source_y & 7u;
            for (std::size_t destination_x = 0;
                 destination_x < 8;
                 ++destination_x) {
                const auto source_x =
                    flip_x ? 7 - destination_x : destination_x;
                const auto color_index = tile_color_index(
                    graphics,
                    source_tile,
                    source_x,
                    tile_y);
                if (color_index == 0) {
                    continue;
                }
                const auto frame_x =
                    object.x - 8 +
                    static_cast<std::int32_t>(destination_x) -
                    minimum_x;
                const auto frame_y =
                    object.y +
                    static_cast<std::int32_t>(destination_y) -
                    minimum_y;
                auto& pixel =
                    frame.pixels[
                        static_cast<std::size_t>(
                            frame_y * frame.width + frame_x)];
                if (pixel.alpha == 0) {
                    pixel = palettes[palette][color_index];
                }
            }
        }
    }
    return frame;
}

}  // namespace oracle::content
