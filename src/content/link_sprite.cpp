#include "oracle/content/link_sprite.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace oracle::content {
namespace {

constexpr std::size_t ages_link_graphics_table = 0x199b1;
constexpr std::size_t seasons_link_graphics_table = 0x19739;
constexpr std::size_t ages_link_oam_pointer_table = 0x1a0a7;
constexpr std::size_t seasons_link_oam_pointer_table = 0x19d9e;
constexpr std::size_t ages_palette_header_table = 0x632c;
constexpr std::size_t seasons_palette_header_table = 0x6290;
constexpr std::uint8_t ages_palette_data_bank = 0x17;
constexpr std::uint8_t seasons_palette_data_bank = 0x16;
constexpr std::uint8_t ages_oam_data_bank = 0x13;
constexpr std::uint8_t seasons_oam_data_bank = 0x12;
constexpr std::uint8_t common_link_palette_header = 0x0f;
constexpr std::uint8_t walking_frame_a_base = 0x54;
constexpr std::uint8_t walking_frame_b_base = 0x80;
constexpr std::uint64_t walking_frame_ticks = 6;
constexpr std::size_t tile_bytes = 16;

using SpritePalettes =
    std::array<std::array<RgbaPixel, 4>, 8>;

struct CampaignOffsets {
    std::size_t graphics_table{};
    std::size_t oam_pointer_table{};
    std::size_t palette_header_table{};
    std::uint8_t palette_data_bank{};
    std::uint8_t oam_data_bank{};
};

CampaignOffsets offsets_for(const core::Campaign campaign) {
    if (campaign == core::Campaign::ages) {
        return CampaignOffsets{
            .graphics_table = ages_link_graphics_table,
            .oam_pointer_table = ages_link_oam_pointer_table,
            .palette_header_table = ages_palette_header_table,
            .palette_data_bank = ages_palette_data_bank,
            .oam_data_bank = ages_oam_data_bank,
        };
    }
    return CampaignOffsets{
        .graphics_table = seasons_link_graphics_table,
        .oam_pointer_table = seasons_link_oam_pointer_table,
        .palette_header_table = seasons_palette_header_table,
        .palette_data_bank = seasons_palette_data_bank,
        .oam_data_bank = seasons_oam_data_bank,
    };
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

SpritePalettes load_sprite_palettes(
    const RomSource& rom,
    const CampaignOffsets offsets) {
    SpritePalettes palettes{};
    const auto header_pointer =
        rom.read_little_u16(
            offsets.palette_header_table +
            static_cast<std::size_t>(common_link_palette_header) * 2);
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
                    "Link palette header exceeds sprite palette RAM"};
            }
            const auto data =
                rom.banked_file_offset(
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

std::uint8_t tile_color_index(
    const std::vector<std::uint8_t>& graphics,
    const std::size_t tile,
    const std::size_t x,
    const std::size_t y) {
    const auto offset = tile * tile_bytes + y * 2;
    if (offset + 1 >= graphics.size()) {
        throw std::runtime_error{
            "Link OAM references graphics outside its DMA frame"};
    }
    const auto bit = static_cast<std::uint8_t>(7u - x);
    return static_cast<std::uint8_t>(
        ((graphics[offset] >> bit) & 1u) |
        (((graphics[offset + 1] >> bit) & 1u) << 1u));
}

std::int32_t signed_offset(const std::uint8_t value) {
    return static_cast<std::int8_t>(value);
}

}  // namespace

LinkSpriteDecoder::LinkSpriteDecoder(const RomSource& rom) : rom_{rom} {}

LinkSpriteFrame LinkSpriteDecoder::decode(
    const LinkDirection direction,
    const bool moving,
    const std::uint64_t animation_tick) const {
    const auto direction_index =
        static_cast<std::uint8_t>(direction);
    if (direction_index > 3) {
        throw std::invalid_argument{"invalid Link direction"};
    }

    const auto walking_phase =
        (animation_tick / walking_frame_ticks) & 1u;
    const auto frame_base =
        moving && walking_phase != 0
        ? walking_frame_b_base
        : walking_frame_a_base;
    const auto frame_index =
        static_cast<std::uint8_t>(frame_base + direction_index);
    return decode_original_frame(frame_index);
}

LinkSpriteFrame LinkSpriteDecoder::decode_original_frame(
    const std::uint8_t frame_index) const {
    const auto offsets = offsets_for(rom_.metadata().campaign);
    const auto frame_record =
        offsets.graphics_table +
        static_cast<std::size_t>(frame_index) * 3;
    const auto oam_index = rom_.read_byte(frame_record);
    const auto packed_graphics =
        rom_.read_little_u16(frame_record + 1);
    const auto graphics_size =
        static_cast<std::size_t>(packed_graphics & 0x1e) * 16;
    if (graphics_size == 0) {
        throw std::runtime_error{"Link frame contains no graphics"};
    }
    const auto graphics_bank = static_cast<std::uint8_t>(
        0x1a + (packed_graphics & 1u));
    const auto graphics_address =
        static_cast<std::uint16_t>(packed_graphics & 0xffe0);
    const auto graphics_offset =
        rom_.banked_file_offset(graphics_bank, graphics_address);
    std::vector<std::uint8_t> graphics(graphics_size);
    for (std::size_t index = 0; index < graphics.size(); ++index) {
        graphics[index] = rom_.read_byte(graphics_offset + index);
    }

    const auto oam_pointer =
        rom_.read_little_u16(
            offsets.oam_pointer_table +
            static_cast<std::size_t>(oam_index) * 2);
    auto oam =
        rom_.banked_file_offset(offsets.oam_data_bank, oam_pointer);
    const auto object_count = rom_.read_byte(oam++);
    const auto palettes = load_sprite_palettes(rom_, offsets);

    LinkSpriteFrame frame{
        .original_frame = frame_index,
        .pixels =
            std::vector<RgbaPixel>(
                16 * 16,
                RgbaPixel{.alpha = 0}),
    };
    for (std::uint8_t object = 0; object < object_count; ++object) {
        const auto y_offset = signed_offset(rom_.read_byte(oam));
        const auto x_offset = signed_offset(rom_.read_byte(oam + 1));
        const auto tile =
            static_cast<std::size_t>(rom_.read_byte(oam + 2) & 0xfe);
        const auto flags = rom_.read_byte(oam + 3);
        oam += 4;
        const bool flip_x = (flags & 0x20) != 0;
        const bool flip_y = (flags & 0x40) != 0;
        const auto palette =
            static_cast<std::size_t>(flags & 0x07);

        for (std::size_t destination_y = 0;
             destination_y < 16;
             ++destination_y) {
            const auto source_y =
                flip_y ? 15 - destination_y : destination_y;
            const auto source_tile = tile + source_y / 8;
            const auto tile_y = source_y & 7u;
            for (std::size_t destination_x = 0;
                 destination_x < 8;
                 ++destination_x) {
                const auto source_x =
                    flip_x ? 7 - destination_x : destination_x;
                const auto color_index =
                    tile_color_index(
                        graphics,
                        source_tile,
                        source_x,
                        tile_y);
                if (color_index == 0) {
                    continue;
                }
                const auto frame_x =
                    x_offset +
                    static_cast<std::int32_t>(destination_x);
                const auto frame_y =
                    y_offset - 8 +
                    static_cast<std::int32_t>(destination_y);
                if (
                    frame_x < 0 ||
                    frame_y < 0 ||
                    frame_x >= frame.width ||
                    frame_y >= frame.height) {
                    continue;
                }
                auto& pixel =
                    frame.pixels[
                        static_cast<std::size_t>(frame_y * frame.width) +
                        static_cast<std::size_t>(frame_x)];
                // Earlier OAM entries win when opaque pixels overlap.
                if (pixel.alpha == 0) {
                    pixel = palettes[palette][color_index];
                }
            }
        }
    }
    return frame;
}

}  // namespace oracle::content
