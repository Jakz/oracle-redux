#include "oracle/content/interaction_sprite.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace oracle::content {
namespace {

constexpr std::size_t ages_vasu_graphics = 0xaaa16;
constexpr std::size_t seasons_vasu_graphics = 0xa5e3e;
constexpr std::size_t ages_vasu_oam_pointers = 0x5b3cc;
constexpr std::size_t seasons_vasu_oam_pointers = 0x52c7d;
constexpr std::size_t ages_palette_header_table = 0x632c;
constexpr std::size_t seasons_palette_header_table = 0x6290;
constexpr std::uint8_t ages_palette_data_bank = 0x17;
constexpr std::uint8_t seasons_palette_data_bank = 0x16;
constexpr std::uint8_t ages_oam_data_bank = 0x14;
constexpr std::uint8_t seasons_oam_data_bank = 0x13;
constexpr std::uint8_t common_gameplay_palette_header = 0x0f;
constexpr std::size_t vasu_graphics_size = 0x200;
constexpr std::size_t tile_bytes = 16;
constexpr std::uint8_t vasu_interaction_id = 0x89;

using SpritePalettes =
    std::array<std::array<RgbaPixel, 4>, 8>;

struct CampaignOffsets {
    std::size_t graphics{};
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

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            ages_vasu_graphics,
            ages_vasu_oam_pointers,
            ages_palette_header_table,
            ages_palette_data_bank,
            ages_oam_data_bank,
        };
    }
    return CampaignOffsets{
        seasons_vasu_graphics,
        seasons_vasu_oam_pointers,
        seasons_palette_header_table,
        seasons_palette_data_bank,
        seasons_oam_data_bank,
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
                    "interaction palette header exceeds sprite palette RAM"};
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

std::uint8_t select_vasu_oam_index_impl(
    const std::uint8_t subid,
    const std::uint64_t tick) {
    if (subid == 0) {
        constexpr std::array<std::uint8_t, 8> frames{
            0, 2, 0, 2, 4, 6, 4, 6,
        };
        return static_cast<std::uint8_t>(
            frames[(tick / 16) % frames.size()] / 2);
    }
    if (subid == 1) {
        if (tick == 0) {
            return 8 / 2;
        }
        constexpr std::array<std::uint8_t, 2> frames{0x0a, 0x0c};
        return static_cast<std::uint8_t>(
            frames[((tick - 1) / 16) % frames.size()] / 2);
    }
    if (subid == 6) {
        if (tick == 0) {
            return 0x14 / 2;
        }
        constexpr std::array<std::uint8_t, 2> frames{0x16, 0x18};
        return static_cast<std::uint8_t>(
            frames[((tick - 1) / 16) % frames.size()] / 2);
    }
    throw std::invalid_argument{
        "Vasu family supports subids 0, 1, and 6"};
}

std::vector<OamObject> load_oam(
    const RomSource& rom,
    const CampaignOffsets offsets,
    const std::uint8_t oam_index) {
    const auto pointer =
        rom.read_little_u16(
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
            "Vasu OAM references graphics outside its decoded sheet"};
    }
    const auto bit = static_cast<std::uint8_t>(7u - x);
    return static_cast<std::uint8_t>(
        ((graphics[offset] >> bit) & 1u) |
        (((graphics[offset + 1] >> bit) & 1u) << 1u));
}

}  // namespace

InteractionSpriteDecoder::InteractionSpriteDecoder(
    const RomSource& rom)
    : rom_{rom} {}

std::uint8_t InteractionSpriteDecoder::select_vasu_oam_index(
    const std::uint8_t subid,
    const std::uint64_t animation_tick) {
    return select_vasu_oam_index_impl(subid, animation_tick);
}

InteractionSpriteFrame InteractionSpriteDecoder::decode_vasu(
    const std::uint8_t subid,
    const std::uint64_t animation_tick) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto oam_index =
        select_vasu_oam_index_impl(subid, animation_tick);
    const auto objects = load_oam(rom_, offsets, oam_index);
    if (objects.empty()) {
        throw std::runtime_error{"Vasu frame has no OAM objects"};
    }

    auto minimum_x = std::numeric_limits<std::int32_t>::max();
    auto minimum_y = std::numeric_limits<std::int32_t>::max();
    auto maximum_x = std::numeric_limits<std::int32_t>::min();
    auto maximum_y = std::numeric_limits<std::int32_t>::min();
    for (const auto& object : objects) {
        minimum_x = std::min(minimum_x, object.x);
        minimum_y = std::min(minimum_y, object.y - 8);
        maximum_x = std::max(maximum_x, object.x + 8);
        maximum_y = std::max(maximum_y, object.y + 8);
    }

    const auto source = rom_.bytes().subspan(offsets.graphics);
    const auto graphics = RoomPixelDecoder::decompress_graphics(
        source,
        vasu_graphics_size,
        3);
    const auto palettes = load_sprite_palettes(rom_, offsets);
    InteractionSpriteFrame frame{
        vasu_interaction_id,
        subid,
        oam_index,
        minimum_x,
        minimum_y,
        maximum_x - minimum_x,
        maximum_y - minimum_y,
    };
    frame.pixels.assign(
        static_cast<std::size_t>(frame.width * frame.height),
        RgbaPixel{.alpha = 0});

    for (const auto& object : objects) {
        const bool flip_x = (object.flags & 0x20) != 0;
        const bool flip_y = (object.flags & 0x40) != 0;
        const auto palette =
            static_cast<std::size_t>(object.flags & 0x07);
        for (std::size_t destination_y = 0;
             destination_y < 16;
             ++destination_y) {
            const auto source_y =
                flip_y ? 15 - destination_y : destination_y;
            const auto source_tile =
                static_cast<std::size_t>(object.tile) + source_y / 8;
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
                    object.x +
                    static_cast<std::int32_t>(destination_x) -
                    minimum_x;
                const auto frame_y =
                    object.y - 8 +
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
