#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/room_layout.h"

namespace oracle::content {

enum class Season : std::uint8_t {
    spring = 0,
    summer = 1,
    autumn = 2,
    winter = 3,
};

struct RgbaPixel {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};
};

struct TilesetDescriptor {
    std::uint8_t assignment{};
    std::uint8_t index{};
    std::uint8_t collision_mode{};
    std::uint8_t dungeon_index{0xff};
    std::uint8_t flags{};
    std::uint8_t unique_graphics{};
    std::uint8_t main_graphics{};
    std::uint8_t palette{};
    std::uint8_t mapping{};
    std::uint8_t layout_group{};
    std::uint8_t animation{};
};

struct RenderedRoom {
    core::WorldRoomId id;
    TilesetDescriptor tileset;
    std::uint64_t animation_signature{};
    std::array<
        RgbaPixel,
        small_room_world_width * small_room_world_height>
        pixels{};
};

class RoomPixelDecoder {
public:
    explicit RoomPixelDecoder(const RomSource& rom);

    [[nodiscard]] TilesetDescriptor describe_tileset(
        std::uint8_t layout_group,
        std::uint8_t room,
        Season season = Season::spring) const;

    [[nodiscard]] RenderedRoom render(
        const RoomLayout& room,
        Season season = Season::spring,
        std::uint64_t animation_tick = 0) const;

    [[nodiscard]] std::uint64_t animation_signature(
        std::uint8_t animation_group,
        std::uint64_t animation_tick) const;

    [[nodiscard]] static std::vector<std::uint8_t> decompress_graphics(
        std::span<const std::uint8_t> source,
        std::size_t output_size,
        std::uint8_t mode);

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
