#include "oracle/content/enemy_sprite.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "oracle/content/enemy_data.h"

namespace oracle::content {
namespace {

constexpr std::uint8_t octorok_enemy_id = 0x09;
constexpr std::size_t octorok_graphics_size = 0x200;
constexpr std::size_t tile_bytes = 16;
constexpr std::uint8_t common_gameplay_palette_header = 0x0f;

using SpritePalettes =
    std::array<std::array<RgbaPixel, 4>, 8>;

struct CampaignOffsets {
    std::size_t object_gfx_header_table{};
    std::size_t oam_pointers{};
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
        // objectGfxHeaderTable 3f:5a8a; enemy09Oam... 0d:7b23.
        return CampaignOffsets{
            0xfda8a,
            0x37b23,
            0x632c,
            0x17,
            0x13,
        };
    }
    // objectGfxHeaderTable 3f:5afb; enemy09Oam... 0c:7ad0.
    return CampaignOffsets{
        0xfdafb,
        0x33ad0,
        0x6290,
        0x16,
        0x12,
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
                    "enemy palette header exceeds sprite palette RAM"};
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
    const std::uint8_t oam_index) {
    const auto pointer = rom.read_little_u16(
        offsets.oam_pointers +
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
            "Octorok OAM references graphics outside its decoded sheet"};
    }
    const auto bit = static_cast<std::uint8_t>(7u - x);
    return static_cast<std::uint8_t>(
        ((graphics[offset] >> bit) & 1u) |
        (((graphics[offset + 1] >> bit) & 1u) << 1u));
}

}  // namespace

EnemySpriteDecoder::EnemySpriteDecoder(const RomSource& rom)
    : rom_{rom} {}

EnemySpriteFrame EnemySpriteDecoder::decode_octorok(
    const std::uint8_t animation_index,
    const std::uint64_t animation_tick) const {
    if (animation_index >= 4) {
        throw std::invalid_argument{
            "Octorok animation index must be between 0 and 3"};
    }
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto frame_in_animation =
        static_cast<std::uint8_t>((animation_tick / 8) & 1u);
    const auto oam_index = static_cast<std::uint8_t>(
        animation_index * 2 + frame_in_animation);
    const auto objects = load_oam(rom_, offsets, oam_index);
    if (objects.empty()) {
        throw std::runtime_error{"Octorok frame has no OAM objects"};
    }

    auto minimum_x = std::numeric_limits<std::int32_t>::max();
    auto minimum_y = std::numeric_limits<std::int32_t>::max();
    auto maximum_x = std::numeric_limits<std::int32_t>::min();
    auto maximum_y = std::numeric_limits<std::int32_t>::min();
    for (const auto& object : objects) {
        // Retail adds Object.x directly to OAM X (whose hardware origin is
        // -8) and Object.y+$10 to OAM Y (whose hardware origin is -16).
        minimum_x = std::min(minimum_x, object.x - 8);
        minimum_y = std::min(minimum_y, object.y);
        maximum_x = std::max(maximum_x, object.x);
        maximum_y = std::max(maximum_y, object.y + 16);
    }

    const auto definition =
        EnemyDefinitionDecoder{rom_}.decode(octorok_enemy_id, 0);
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
        octorok_graphics_size,
        compression_mode);
    const auto palettes = load_sprite_palettes(rom_, offsets);
    EnemySpriteFrame frame{
        .enemy_id = octorok_enemy_id,
        .animation_index = animation_index,
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
                    definition.tile_base + object.tile) +
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
                    pixel =
                        palettes[definition.palette][color_index];
                }
            }
        }
    }
    return frame;
}

}  // namespace oracle::content
