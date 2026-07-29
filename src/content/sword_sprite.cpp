#include "oracle/content/sword_sprite.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace oracle::content {
namespace {

constexpr std::size_t ages_item05_oam_pointers = 0x1e8ca;
constexpr std::size_t seasons_item05_oam_pointers = 0x1e668;
constexpr std::uint8_t ages_item_oam_data_bank = 0x13;
constexpr std::uint8_t seasons_item_oam_data_bank = 0x12;
constexpr std::size_t ages_item_data = 0xfe3a5;
constexpr std::size_t seasons_item_data = 0xfe3a3;
constexpr std::size_t ages_sword_graphics_header = 0x68d2;
constexpr std::size_t seasons_sword_graphics_header = 0x6854;
constexpr std::uint8_t sword_item_id = 0x05;
constexpr std::size_t tile_bytes = 16;
constexpr std::uint8_t common_gameplay_palette_header = 0x0f;
constexpr std::size_t ages_palette_header_table = 0x632c;
constexpr std::size_t seasons_palette_header_table = 0x6290;
constexpr std::uint8_t ages_palette_data_bank = 0x17;
constexpr std::uint8_t seasons_palette_data_bank = 0x16;

using SpritePalettes =
    std::array<std::array<RgbaPixel, 4>, 8>;

struct OamObject {
    std::int32_t y{};
    std::int32_t x{};
    std::uint8_t tile{};
    std::uint8_t flags{};
};

struct SwordGraphicsSource {
    std::size_t offset{};
    std::size_t size{};
    std::uint8_t palette{};
};

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

SpritePalettes load_sprite_palettes(const RomSource& rom) {
    const auto ages =
        rom.metadata().campaign == core::Campaign::ages;
    const auto table =
        ages ? ages_palette_header_table : seasons_palette_header_table;
    const auto data_bank =
        ages ? ages_palette_data_bank : seasons_palette_data_bank;
    SpritePalettes palettes{};
    const auto header_pointer =
        rom.read_little_u16(
            table +
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
                    "sword palette header exceeds sprite palette RAM"};
            }
            const auto data =
                rom.banked_file_offset(data_bank, pointer);
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
    const std::uint8_t animation_index) {
    const auto table =
        rom.metadata().campaign == core::Campaign::ages
        ? ages_item05_oam_pointers
        : seasons_item05_oam_pointers;
    const auto pointer =
        rom.read_little_u16(
            table +
            static_cast<std::size_t>(animation_index) * 2);
    const auto data_bank =
        rom.metadata().campaign == core::Campaign::ages
        ? ages_item_oam_data_bank
        : seasons_item_oam_data_bank;
    auto cursor =
        rom.banked_file_offset(data_bank, pointer);
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

SwordGraphicsSource load_graphics_source(const RomSource& rom) {
    const auto ages =
        rom.metadata().campaign == core::Campaign::ages;
    const auto header =
        ages
        ? ages_sword_graphics_header
        : seasons_sword_graphics_header;
    const auto bank_and_mode = rom.read_byte(header);
    if ((bank_and_mode >> 6u) != 0) {
        throw std::runtime_error{
            "UNCMP_GFXH_1a unexpectedly requests compressed sword data"};
    }
    const auto source_address = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(rom.read_byte(header + 1)) << 8u) |
        rom.read_byte(header + 2));
    const auto size =
        (static_cast<std::size_t>(rom.read_byte(header + 5) & 0x7f) + 1) *
        tile_bytes;
    const auto item_data =
        (ages ? ages_item_data : seasons_item_data) +
        static_cast<std::size_t>(sword_item_id) * 3;
    return SwordGraphicsSource{
        .offset = rom.banked_file_offset(
            static_cast<std::uint8_t>(bank_and_mode & 0x3f),
            source_address),
        .size = size,
        .palette =
            static_cast<std::uint8_t>(
                rom.read_byte(item_data + 2) & 0x07),
    };
}

std::uint8_t tile_color_index(
    const RomSource& rom,
    const SwordGraphicsSource graphics,
    const std::size_t tile,
    const std::size_t x,
    const std::size_t y) {
    const auto offset =
        graphics.offset + tile * tile_bytes + y * 2;
    if (offset + 1 >= graphics.offset + graphics.size) {
        throw std::runtime_error{
            "sword OAM references graphics outside UNCMP_GFXH_1a"};
    }
    const auto bit = static_cast<std::uint8_t>(7u - x);
    return static_cast<std::uint8_t>(
        ((rom.read_byte(offset) >> bit) & 1u) |
        (((rom.read_byte(offset + 1) >> bit) & 1u) << 1u));
}

}  // namespace

SwordSpriteDecoder::SwordSpriteDecoder(const RomSource& rom) : rom_{rom} {}

SwordSpriteFrame SwordSpriteDecoder::decode(
    const std::uint8_t animation_index) const {
    if (animation_index >= 8) {
        throw std::invalid_argument{
            "sword animation index must be between 0 and 7"};
    }
    const auto objects = load_oam(rom_, animation_index);
    if (objects.empty()) {
        throw std::runtime_error{"sword frame has no OAM objects"};
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

    const auto graphics = load_graphics_source(rom_);
    const auto palettes = load_sprite_palettes(rom_);
    SwordSpriteFrame frame{
        .animation_index = animation_index,
        .original_oam_index = animation_index,
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
                static_cast<std::size_t>(object.tile) +
                source_y / 8;
            const auto tile_y = source_y & 7u;
            for (std::size_t destination_x = 0;
                 destination_x < 8;
                 ++destination_x) {
                const auto source_x =
                    flip_x ? 7 - destination_x : destination_x;
                const auto color_index = tile_color_index(
                    rom_,
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
                    pixel = palettes[graphics.palette][color_index];
                }
            }
        }
    }
    return frame;
}

}  // namespace oracle::content
