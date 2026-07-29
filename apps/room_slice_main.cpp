#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "oracle/content/link_sprite.h"
#include "oracle/content/part_sprite.h"
#include "oracle/content/interaction_sprite.h"
#include "oracle/content/enemy_sprite.h"
#include "oracle/content/rom_source.h"
#include "oracle/content/rom_text.h"
#include "oracle/content/room_collisions.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_mutations.h"
#include "oracle/content/room_objects.h"
#include "oracle/content/room_pixels.h"
#include "oracle/content/room_topology.h"
#include "oracle/content/sword_sprite.h"
#include "oracle/core/campaign.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/gameplay/octorok_runtime.h"
#include "oracle/gameplay/room_actor_loader.h"
#include "oracle/gameplay/sword_runtime.h"
#include "oracle/gameplay/vasu_interaction.h"
#include "oracle/input/input_frame.h"
#include "oracle/presentation/frame_timing.h"

namespace {

using oracle::content::RoomPlacement;

struct CameraState {
    double x{};
    double y{};
    double zoom{3.0};
};

struct Color {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

struct RegionPixels {
    std::int32_t world_x{};
    std::int32_t world_y{};
    std::int32_t width{};
    std::int32_t height{};
    std::uint64_t animation_signature{};
    std::vector<oracle::content::RgbaPixel> pixels;
    std::vector<oracle::content::RenderedRoom> rooms;
};

std::uint8_t parse_hex_byte(const std::string_view text) {
    auto normalized = text;
    if (normalized.starts_with("0x") || normalized.starts_with("0X")) {
        normalized.remove_prefix(2);
    }
    unsigned int value = 0;
    const auto [end, error] = std::from_chars(
        normalized.data(),
        normalized.data() + normalized.size(),
        value,
        16);
    if (error != std::errc{} || end != normalized.data() + normalized.size() ||
        value > 0xff) {
        throw std::invalid_argument{"value must be a hexadecimal byte"};
    }
    return static_cast<std::uint8_t>(value);
}

std::uint64_t parse_unsigned_integer(const std::string_view text) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value,
        10);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument{
            "value must be an unsigned decimal integer"};
    }
    return value;
}

oracle::content::Season parse_season(const std::string_view text) {
    using oracle::content::Season;
    if (text == "spring" || text == "0") {
        return Season::spring;
    }
    if (text == "summer" || text == "1") {
        return Season::summer;
    }
    if (text == "autumn" || text == "fall" || text == "2") {
        return Season::autumn;
    }
    if (text == "winter" || text == "3") {
        return Season::winter;
    }
    throw std::invalid_argument{
        "season must be spring, summer, autumn, or winter"};
}

Color metatile_color(
    const oracle::core::Campaign campaign,
    const std::uint8_t metatile) {
    std::uint32_t hash =
        static_cast<std::uint32_t>(metatile) * 0x45d9f3bu + 0x27100001u;
    hash ^= hash >> 16u;
    const auto accent =
        campaign == oracle::core::Campaign::ages ? 26u : 0u;
    return Color{
        .red = static_cast<std::uint8_t>(48u + ((hash >> 0u) & 0x7fu)),
        .green = static_cast<std::uint8_t>(
            64u + ((hash >> 8u) & 0x7fu)),
        .blue = static_cast<std::uint8_t>(
            48u + (((hash >> 16u) + accent) & 0x7fu)),
    };
}

void set_color(SDL_Renderer* renderer, const Color color) {
    SDL_SetRenderDrawColor(
        renderer,
        color.red,
        color.green,
        color.blue,
        SDL_ALPHA_OPAQUE);
}

void render_diagnostic_region(
    SDL_Renderer* renderer,
    const std::vector<RoomPlacement>& rooms,
    const oracle::core::Campaign campaign,
    const CameraState camera,
    const int output_width,
    const int output_height,
    const std::uint8_t center_room) {
    const auto to_screen_x = [&](const double world_x) {
        return static_cast<float>(
            (world_x - camera.x) * camera.zoom + output_width * 0.5);
    };
    const auto to_screen_y = [&](const double world_y) {
        return static_cast<float>(
            (world_y - camera.y) * camera.zoom + output_height * 0.5);
    };

    for (const auto& placement : rooms) {
        for (std::size_t row = 0;
             row < placement.layout.rows;
             ++row) {
            for (std::size_t column = 0;
                 column < placement.layout.columns;
                 ++column) {
                const auto metatile =
                    placement.layout.metatiles[
                        row * placement.layout.columns + column];
                const auto world_x =
                    placement.world_x +
                    static_cast<double>(
                        column * oracle::content::metatile_world_size);
                const auto world_y =
                    placement.world_y +
                    static_cast<double>(
                        row * oracle::content::metatile_world_size);
                SDL_FRect rectangle{
                    .x = to_screen_x(world_x),
                    .y = to_screen_y(world_y),
                    .w = static_cast<float>(
                        oracle::content::metatile_world_size * camera.zoom +
                        0.5),
                    .h = static_cast<float>(
                        oracle::content::metatile_world_size * camera.zoom +
                        0.5),
                };
                if (
                    rectangle.x + rectangle.w < 0 ||
                    rectangle.y + rectangle.h < 0 ||
                    rectangle.x > output_width ||
                    rectangle.y > output_height) {
                    continue;
                }
                const auto color = metatile_color(campaign, metatile);
                set_color(renderer, color);
                SDL_RenderFillRect(renderer, &rectangle);

                const Color highlight{
                    .red = static_cast<std::uint8_t>(
                        std::min(255, static_cast<int>(color.red) + 22)),
                    .green = static_cast<std::uint8_t>(
                        std::min(255, static_cast<int>(color.green) + 22)),
                    .blue = static_cast<std::uint8_t>(
                        std::min(255, static_cast<int>(color.blue) + 22)),
                };
                set_color(renderer, highlight);
                SDL_FRect top_edge = rectangle;
                top_edge.h = std::max(1.0f, rectangle.h * 0.08f);
                SDL_RenderFillRect(renderer, &top_edge);
            }
        }

        const bool is_center =
            placement.layout.id.room == center_room;
        set_color(
            renderer,
            is_center ? Color{255, 238, 128} : Color{24, 28, 36});
        const SDL_FRect border{
            .x = to_screen_x(placement.world_x),
            .y = to_screen_y(placement.world_y),
            .w = static_cast<float>(
                placement.layout.pixel_width() * camera.zoom),
            .h = static_cast<float>(
                placement.layout.pixel_height() * camera.zoom),
        };
        SDL_RenderRect(renderer, &border);
    }
}

void render_room_borders(
    SDL_Renderer* renderer,
    const std::vector<RoomPlacement>& rooms,
    const CameraState camera,
    const int output_width,
    const int output_height,
    const std::uint8_t center_room) {
    const auto to_screen_x = [&](const double world_x) {
        return static_cast<float>(
            (world_x - camera.x) * camera.zoom + output_width * 0.5);
    };
    const auto to_screen_y = [&](const double world_y) {
        return static_cast<float>(
            (world_y - camera.y) * camera.zoom + output_height * 0.5);
    };
    for (const auto& placement : rooms) {
        const bool is_center = placement.layout.id.room == center_room;
        set_color(
            renderer,
            is_center ? Color{255, 238, 128} : Color{24, 28, 36});
        const SDL_FRect border{
            .x = to_screen_x(placement.world_x),
            .y = to_screen_y(placement.world_y),
            .w = static_cast<float>(
                placement.layout.pixel_width() * camera.zoom),
            .h = static_cast<float>(
                placement.layout.pixel_height() * camera.zoom),
        };
        SDL_RenderRect(renderer, &border);
    }
}

void render_collision_overlay(
    SDL_Renderer* renderer,
    const std::vector<RoomPlacement>& rooms,
    const std::vector<oracle::content::RoomCollisionMap>& collisions,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    if (rooms.size() != collisions.size()) {
        throw std::invalid_argument{
            "room placements and collision maps are not aligned"};
    }
    const auto to_screen_x = [&](const double world_x) {
        return static_cast<float>(
            (world_x - camera.x) * camera.zoom + output_width * 0.5);
    };
    const auto to_screen_y = [&](const double world_y) {
        return static_cast<float>(
            (world_y - camera.y) * camera.zoom + output_height * 0.5);
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (std::size_t room_index = 0;
         room_index < rooms.size();
         ++room_index) {
        const auto& placement = rooms[room_index];
        const auto& map = collisions[room_index];
        for (std::size_t row = 0; row < map.rows; ++row) {
            for (std::size_t column = 0; column < map.columns; ++column) {
                const auto collision = map.at(column, row);
                if (collision == 0) {
                    continue;
                }
                const auto world_x =
                    placement.world_x +
                    static_cast<double>(
                        column * oracle::content::metatile_world_size);
                const auto world_y =
                    placement.world_y +
                    static_cast<double>(
                        row * oracle::content::metatile_world_size);
                const SDL_FRect metatile_rect{
                    .x = to_screen_x(world_x),
                    .y = to_screen_y(world_y),
                    .w = static_cast<float>(16.0 * camera.zoom),
                    .h = static_cast<float>(16.0 * camera.zoom),
                };
                if (
                    metatile_rect.x + metatile_rect.w < 0 ||
                    metatile_rect.y + metatile_rect.h < 0 ||
                    metatile_rect.x > output_width ||
                    metatile_rect.y > output_height) {
                    continue;
                }

                if (collision == 0x10) {
                    SDL_SetRenderDrawColor(renderer, 35, 145, 255, 100);
                    SDL_RenderFillRect(renderer, &metatile_rect);
                } else if (collision < 0x10) {
                    SDL_SetRenderDrawColor(renderer, 255, 52, 52, 105);
                    for (std::uint8_t quadrant_y = 0;
                         quadrant_y < 2;
                         ++quadrant_y) {
                        for (std::uint8_t quadrant_x = 0;
                             quadrant_x < 2;
                             ++quadrant_x) {
                            const auto sample_x = static_cast<std::uint8_t>(
                                quadrant_x * 8 + 4);
                            const auto sample_y = static_cast<std::uint8_t>(
                                quadrant_y * 8 + 4);
                            if (!oracle::content::RoomCollisionDecoder::
                                    is_solid(
                                        collision,
                                        sample_x,
                                        sample_y)) {
                                continue;
                            }
                            const SDL_FRect quadrant{
                                .x = to_screen_x(
                                    world_x + quadrant_x * 8),
                                .y = to_screen_y(
                                    world_y + quadrant_y * 8),
                                .w = static_cast<float>(
                                    8.0 * camera.zoom + 0.25),
                                .h = static_cast<float>(
                                    8.0 * camera.zoom + 0.25),
                            };
                            SDL_RenderFillRect(renderer, &quadrant);
                        }
                    }
                } else {
                    SDL_SetRenderDrawColor(renderer, 255, 115, 40, 125);
                    const auto vertical =
                        (collision & 0x0f) < 8;
                    for (std::uint8_t stripe = 0; stripe < 8; ++stripe) {
                        const auto sample =
                            static_cast<std::uint8_t>(stripe * 2 + 1);
                        if (!oracle::content::RoomCollisionDecoder::is_solid(
                                collision,
                                vertical ? sample : 8,
                                vertical ? 8 : sample)) {
                            continue;
                        }
                        const SDL_FRect stripe_rect{
                            .x = to_screen_x(
                                world_x + (vertical ? stripe * 2 : 0)),
                            .y = to_screen_y(
                                world_y + (vertical ? 0 : stripe * 2)),
                            .w = static_cast<float>(
                                (vertical ? 2.0 : 16.0) *
                                camera.zoom),
                            .h = static_cast<float>(
                                (vertical ? 16.0 : 2.0) *
                                camera.zoom),
                        };
                        SDL_RenderFillRect(renderer, &stripe_rect);
                    }
                }

                if (
                    oracle::content::RoomCollisionDecoder::is_special(
                        collision)) {
                    SDL_SetRenderDrawColor(renderer, 50, 210, 255, 190);
                    SDL_RenderRect(renderer, &metatile_rect);
                }
            }
        }
    }
}

oracle::content::LinkDirection link_direction(
    const oracle::gameplay::PlayerFacing facing) {
    using oracle::content::LinkDirection;
    using oracle::gameplay::PlayerFacing;
    switch (facing) {
    case PlayerFacing::north:
        return LinkDirection::north;
    case PlayerFacing::east:
        return LinkDirection::east;
    case PlayerFacing::south:
        return LinkDirection::south;
    case PlayerFacing::west:
        return LinkDirection::west;
    }
    return LinkDirection::south;
}

void render_player(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const double world_x,
    const double world_y,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    const auto screen_x = static_cast<float>(
        (world_x - camera.x) * camera.zoom + output_width * 0.5);
    const auto screen_y = static_cast<float>(
        (world_y - camera.y) * camera.zoom + output_height * 0.5);
    const auto half_width = static_cast<float>(8.0 * camera.zoom);
    const auto half_height = static_cast<float>(8.0 * camera.zoom);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 4, 10, 14, 145);
    const SDL_FRect shadow{
        .x = screen_x - static_cast<float>(5.0 * camera.zoom),
        .y = screen_y + static_cast<float>(4.0 * camera.zoom),
        .w = static_cast<float>(10.0 * camera.zoom),
        .h = std::max(2.0f, static_cast<float>(3.0 * camera.zoom)),
    };
    SDL_RenderFillRect(renderer, &shadow);

    const SDL_FRect sprite{
        .x = screen_x - half_width,
        .y = screen_y - half_height,
        .w = half_width * 2.0f,
        .h = half_height * 2.0f,
    };
    SDL_RenderTexture(renderer, texture, nullptr, &sprite);
}

std::optional<oracle::input::InputAction> semantic_action(
    const SDL_Keycode key) {
    using oracle::input::InputAction;
    switch (key) {
    case SDLK_W:
    case SDLK_UP:
        return InputAction::up;
    case SDLK_S:
    case SDLK_DOWN:
        return InputAction::down;
    case SDLK_A:
    case SDLK_LEFT:
        return InputAction::left;
    case SDLK_D:
    case SDLK_RIGHT:
        return InputAction::right;
    case SDLK_Z:
    case SDLK_SPACE:
        return InputAction::a;
    case SDLK_X:
        return InputAction::b;
    case SDLK_RETURN:
        return InputAction::confirm;
    case SDLK_BACKSPACE:
        return InputAction::cancel;
    default:
        return std::nullopt;
    }
}

std::size_t vasu_texture_index(const std::uint8_t subid) {
    if (subid == 0) {
        return 0;
    }
    if (subid == 1) {
        return 1;
    }
    return 2;
}

void render_vasu_actors(
    SDL_Renderer* renderer,
    const std::array<SDL_Texture*, 3>& textures,
    const oracle::content::InteractionSpriteDecoder& sprite_decoder,
    const oracle::core::ActorSlotDomain& actors,
    const std::vector<RoomPlacement>& rooms,
    const oracle::gameplay::PlayerState& player,
    const bool in_front_of_player,
    const std::uint64_t animation_tick,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    constexpr std::int32_t texture_size = 64;
    const auto player_world_y =
        oracle::gameplay::PlayerTraversal::world_y(player);
    for (const auto& actor :
         actors.slots(oracle::core::ActorCategory::interaction)) {
        if (
            !actor.active ||
            !actor.positioned ||
            actor.identity.id != 0x89 ||
            (
                actor.identity.subid != 0 &&
                actor.identity.subid != 1 &&
                actor.identity.subid != 6)) {
            continue;
        }
        const auto placement = std::find_if(
            rooms.begin(),
            rooms.end(),
            [&](const RoomPlacement& candidate) {
                return candidate.layout.id == actor.room;
            });
        if (placement == rooms.end()) {
            continue;
        }
        const auto world_x =
            placement->world_x + actor.local_x;
        const auto world_y =
            placement->world_y + actor.local_y;
        if ((world_y > player_world_y) != in_front_of_player) {
            continue;
        }
        const auto frame = sprite_decoder.decode_vasu(
            actor.identity.subid,
            animation_tick);
        if (
            frame.width > texture_size ||
            frame.height > texture_size) {
            throw std::runtime_error{
                "Vasu sprite exceeds its diagnostic texture"};
        }
        std::array<oracle::content::RgbaPixel, texture_size * texture_size>
            upload{};
        for (std::int32_t y = 0; y < frame.height; ++y) {
            std::copy_n(
                frame.pixels.begin() +
                    static_cast<std::size_t>(y * frame.width),
                frame.width,
                upload.begin() +
                    static_cast<std::size_t>(y * texture_size));
        }
        auto* texture = textures[vasu_texture_index(actor.identity.subid)];
        if (!SDL_UpdateTexture(
                texture,
                nullptr,
                upload.data(),
                texture_size *
                    static_cast<int>(
                        sizeof(oracle::content::RgbaPixel)))) {
            throw std::runtime_error{
                std::string{"Vasu texture upload failed: "} +
                SDL_GetError()};
        }

        const auto screen_x = static_cast<float>(
            (world_x - camera.x) * camera.zoom +
            output_width * 0.5);
        const auto screen_y = static_cast<float>(
            (world_y - camera.y) * camera.zoom +
            output_height * 0.5);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 4, 10, 14, 130);
        const SDL_FRect shadow{
            screen_x - static_cast<float>(5.0 * camera.zoom),
            screen_y - static_cast<float>(1.0 * camera.zoom),
            static_cast<float>(10.0 * camera.zoom),
            std::max(2.0f, static_cast<float>(3.0 * camera.zoom)),
        };
        SDL_RenderFillRect(renderer, &shadow);
        const SDL_FRect source{
            0.0f,
            0.0f,
            static_cast<float>(frame.width),
            static_cast<float>(frame.height),
        };
        const SDL_FRect destination{
            screen_x +
                static_cast<float>(frame.origin_x * camera.zoom),
            screen_y +
                static_cast<float>(frame.origin_y * camera.zoom),
            static_cast<float>(frame.width * camera.zoom),
            static_cast<float>(frame.height * camera.zoom),
        };
        SDL_RenderTexture(renderer, texture, &source, &destination);
    }
}

void render_octorok_actors(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const oracle::content::EnemySpriteDecoder& sprite_decoder,
    const oracle::gameplay::OctorokRuntime& runtime,
    const oracle::core::ActorSlotDomain& actors,
    const std::vector<RoomPlacement>& rooms,
    const oracle::gameplay::PlayerState& player,
    const bool in_front_of_player,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    constexpr std::int32_t texture_size = 32;
    const auto player_world_y =
        oracle::gameplay::PlayerTraversal::world_y(player);
    const auto enemy_slots =
        actors.slots(oracle::core::ActorCategory::enemy);
    for (std::size_t slot = 0; slot < enemy_slots.size(); ++slot) {
        const auto& actor = enemy_slots[slot];
        if (
            !actor.active ||
            !actor.positioned ||
            actor.identity.id != 0x09) {
            continue;
        }
        const auto placement = std::find_if(
            rooms.begin(),
            rooms.end(),
            [&](const RoomPlacement& candidate) {
                return candidate.layout.id == actor.room;
            });
        if (placement == rooms.end()) {
            continue;
        }
        const auto world_x = placement->world_x + actor.local_x;
        const auto world_y = placement->world_y + actor.local_y;
        if ((world_y > player_world_y) != in_front_of_player) {
            continue;
        }
        const oracle::core::ActorSlotHandle handle{
            oracle::core::ActorCategory::enemy,
            static_cast<std::uint8_t>(slot),
            actor.generation,
        };
        const auto animation = runtime.animation_index(handle);
        if (!animation.has_value()) {
            continue;
        }
        const auto frame = sprite_decoder.decode_octorok(
            *animation,
            runtime.animation_tick(handle));
        if (
            frame.width > texture_size ||
            frame.height > texture_size) {
            throw std::runtime_error{
                "Octorok sprite exceeds its diagnostic texture"};
        }
        std::array<oracle::content::RgbaPixel, texture_size * texture_size>
            upload{};
        for (std::int32_t y = 0; y < frame.height; ++y) {
            std::copy_n(
                frame.pixels.begin() +
                    static_cast<std::size_t>(y * frame.width),
                frame.width,
                upload.begin() +
                    static_cast<std::size_t>(y * texture_size));
        }
        if (!SDL_UpdateTexture(
                texture,
                nullptr,
                upload.data(),
                texture_size *
                    static_cast<int>(
                        sizeof(oracle::content::RgbaPixel)))) {
            throw std::runtime_error{
                std::string{"Octorok texture upload failed: "} +
                SDL_GetError()};
        }

        const auto screen_x = static_cast<float>(
            (world_x - camera.x) * camera.zoom +
            output_width * 0.5);
        const auto screen_y = static_cast<float>(
            (world_y - camera.y) * camera.zoom +
            output_height * 0.5);
        const SDL_FRect source{
            0.0f,
            0.0f,
            static_cast<float>(frame.width),
            static_cast<float>(frame.height),
        };
        const SDL_FRect destination{
            screen_x +
                static_cast<float>(frame.origin_x * camera.zoom),
            screen_y +
                static_cast<float>(frame.origin_y * camera.zoom),
            static_cast<float>(frame.width * camera.zoom),
            static_cast<float>(frame.height * camera.zoom),
        };
        SDL_RenderTexture(renderer, texture, &source, &destination);
    }
}

void render_octorok_projectiles(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const oracle::content::PartSpriteFrame& frame,
    const oracle::gameplay::OctorokRuntime& runtime,
    const oracle::core::ActorSlotDomain& actors,
    const std::vector<RoomPlacement>& rooms,
    const oracle::gameplay::PlayerState& player,
    const bool in_front_of_player,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    const auto player_world_y =
        oracle::gameplay::PlayerTraversal::world_y(player);
    const auto part_slots =
        actors.slots(oracle::core::ActorCategory::part);
    for (std::size_t slot = 0; slot < part_slots.size(); ++slot) {
        const auto& actor = part_slots[slot];
        if (
            !actor.active ||
            !actor.positioned ||
            actor.identity.id != 0x18) {
            continue;
        }
        const auto placement = std::find_if(
            rooms.begin(),
            rooms.end(),
            [&](const RoomPlacement& candidate) {
                return candidate.layout.id == actor.room;
            });
        if (placement == rooms.end()) {
            continue;
        }
        const auto world_x = placement->world_x + actor.local_x;
        const auto ground_y = placement->world_y + actor.local_y;
        if ((ground_y > player_world_y) != in_front_of_player) {
            continue;
        }
        const oracle::core::ActorSlotHandle handle{
            oracle::core::ActorCategory::part,
            static_cast<std::uint8_t>(slot),
            actor.generation,
        };
        if (!runtime.projectile_phase(handle).has_value()) {
            continue;
        }
        const auto screen_x = static_cast<float>(
            (world_x - camera.x) * camera.zoom +
            output_width * 0.5);
        const auto screen_ground_y = static_cast<float>(
            (ground_y - camera.y) * camera.zoom +
            output_height * 0.5);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 4, 10, 14, 105);
        const SDL_FRect shadow{
            screen_x - static_cast<float>(4.0 * camera.zoom),
            screen_ground_y +
                static_cast<float>(8.0 * camera.zoom),
            static_cast<float>(8.0 * camera.zoom),
            std::max(
                1.0f,
                static_cast<float>(2.0 * camera.zoom)),
        };
        SDL_RenderFillRect(renderer, &shadow);

        const auto elevation = runtime.projectile_elevation(handle);
        const SDL_FRect source{
            0.0f,
            0.0f,
            static_cast<float>(frame.width),
            static_cast<float>(frame.height),
        };
        const SDL_FRect destination{
            screen_x +
                static_cast<float>(frame.origin_x * camera.zoom),
            screen_ground_y +
                static_cast<float>(
                    (frame.origin_y - elevation) * camera.zoom),
            static_cast<float>(frame.width * camera.zoom),
            static_cast<float>(frame.height * camera.zoom),
        };
        SDL_RenderTexture(renderer, texture, &source, &destination);
    }
}

void render_sword(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const oracle::content::SwordSpriteDecoder& sprite_decoder,
    const oracle::gameplay::SwordStepReport& sword_step,
    const oracle::gameplay::PlayerState& player,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    if (!sword_step.pose.has_value()) {
        return;
    }
    constexpr std::int32_t texture_size = 32;
    const auto& pose = *sword_step.pose;
    const auto frame =
        sprite_decoder.decode(pose.animation_index);
    std::array<
        oracle::content::RgbaPixel,
        texture_size * texture_size> upload{};
    for (std::int32_t y = 0; y < frame.height; ++y) {
        std::copy_n(
            frame.pixels.begin() +
                static_cast<std::size_t>(y * frame.width),
            frame.width,
            upload.begin() +
                static_cast<std::size_t>(y * texture_size));
    }
    if (!SDL_UpdateTexture(
            texture,
            nullptr,
            upload.data(),
            texture_size *
                static_cast<int>(
                    sizeof(oracle::content::RgbaPixel)))) {
        throw std::runtime_error{
            std::string{"sword texture upload failed: "} +
            SDL_GetError()};
    }
    const auto world_x =
        oracle::gameplay::PlayerTraversal::world_x(player) +
        pose.local_x - player.local_x;
    const auto world_y =
        oracle::gameplay::PlayerTraversal::world_y(player) +
        pose.local_y - player.local_y;
    const SDL_FRect source{
        0.0f,
        0.0f,
        static_cast<float>(frame.width),
        static_cast<float>(frame.height),
    };
    const SDL_FRect destination{
        static_cast<float>(
            (world_x + frame.origin_x - camera.x) *
                camera.zoom +
            output_width * 0.5),
        static_cast<float>(
            (world_y + frame.origin_y - camera.y) *
                camera.zoom +
            output_height * 0.5),
        static_cast<float>(
            frame.width * camera.zoom),
        static_cast<float>(
            frame.height * camera.zoom),
    };
    SDL_RenderTexture(renderer, texture, &source, &destination);
}

void render_dialogue(
    SDL_Renderer* renderer,
    const oracle::ui::DialogueModel& dialogue,
    const int output_width,
    const int output_height) {
    if (!dialogue.visible) {
        return;
    }
    const auto width = std::min(640.0f, output_width - 32.0f);
    const auto height = std::min(180.0f, output_height - 32.0f);
    const SDL_FRect box{
        (output_width - width) * 0.5f,
        output_height - height - 16.0f,
        width,
        height,
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 12, 24, 240);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 238, 232, 210, 255);
    SDL_RenderRect(renderer, &box);

    std::istringstream lines{dialogue.page_text};
    std::string line;
    float y = box.y + 18.0f;
    while (std::getline(lines, line) && y < box.y + box.h - 34.0f) {
        SDL_RenderDebugText(renderer, box.x + 20.0f, y, line.c_str());
        y += 16.0f;
    }
    if (!dialogue.options.empty()) {
        std::ostringstream selection;
        selection << "Choice: ";
        for (std::size_t index = 0;
             index < dialogue.options.size();
             ++index) {
            selection
                << (index == dialogue.selected_option ? "> " : "  ")
                << dialogue.options[index].label;
            if (index + 1 != dialogue.options.size()) {
                selection << "   ";
            }
        }
        const auto text = selection.str();
        SDL_RenderDebugText(
            renderer,
            box.x + 20.0f,
            box.y + box.h - 28.0f,
            text.c_str());
    } else {
        SDL_RenderDebugText(
            renderer,
            box.x + box.w - 116.0f,
            box.y + box.h - 28.0f,
            "Z/Enter: next");
    }
}

void render_object_anchors(
    SDL_Renderer* renderer,
    const oracle::content::RoomObjectCatalog& catalog,
    const std::vector<RoomPlacement>& rooms,
    const CameraState camera,
    const int output_width,
    const int output_height) {
    const auto placement = std::find_if(
        rooms.begin(),
        rooms.end(),
        [&](const RoomPlacement& candidate) {
            return candidate.layout.id == catalog.room;
        });
    if (placement == rooms.end()) {
        return;
    }
    for (const auto& object : catalog.records) {
        if (!object.positioned) {
            continue;
        }
        Color color{};
        switch (object.kind) {
        case oracle::content::RoomObjectKind::interaction:
            color = Color{45, 220, 255};
            break;
        case oracle::content::RoomObjectKind::enemy:
            color = Color{255, 70, 75};
            break;
        case oracle::content::RoomObjectKind::part:
            color = Color{255, 220, 55};
            break;
        case oracle::content::RoomObjectKind::item_drop:
            color = Color{238, 95, 255};
            break;
        }
        const auto world_x =
            placement->world_x + object.original_x;
        const auto world_y =
            placement->world_y +
            static_cast<double>(object.original_y) - 16.0;
        const auto screen_x = static_cast<float>(
            (world_x - camera.x) * camera.zoom +
            output_width * 0.5);
        const auto screen_y = static_cast<float>(
            (world_y - camera.y) * camera.zoom +
            output_height * 0.5);
        const auto radius =
            std::max(3.0f, static_cast<float>(3.0 * camera.zoom));
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(
            renderer,
            color.red,
            color.green,
            color.blue,
            object.conditional ? 150 : 230);
        const SDL_FRect marker{
            .x = screen_x - radius,
            .y = screen_y - radius,
            .w = radius * 2.0f,
            .h = radius * 2.0f,
        };
        SDL_RenderRect(renderer, &marker);
        SDL_RenderLine(
            renderer,
            screen_x - radius,
            screen_y,
            screen_x + radius,
            screen_y);
        SDL_RenderLine(
            renderer,
            screen_x,
            screen_y - radius,
            screen_x,
            screen_y + radius);
    }
}

void copy_room_pixels(
    RegionPixels& region,
    const RoomPlacement& placement,
    const oracle::content::RenderedRoom& rendered) {
    const auto local_x = placement.world_x - region.world_x;
    const auto local_y = placement.world_y - region.world_y;
    for (std::int32_t y = 0; y < rendered.height; ++y) {
        const auto source =
            rendered.pixels.begin() +
            static_cast<std::ptrdiff_t>(
                y * rendered.width);
        const auto destination =
            region.pixels.begin() +
            static_cast<std::ptrdiff_t>(
                (local_y + y) * region.width + local_x);
        std::copy_n(
            source,
            rendered.width,
            destination);
    }
}

RegionPixels compose_region(
    const oracle::content::RoomPixelDecoder& decoder,
    const std::vector<RoomPlacement>& placements,
    const oracle::content::Season season,
    const std::uint64_t animation_tick) {
    if (placements.empty()) {
        throw std::invalid_argument{"cannot compose an empty room region"};
    }
    const auto minimum_x = std::min_element(
        placements.begin(),
        placements.end(),
        [](const RoomPlacement& left, const RoomPlacement& right) {
            return left.world_x < right.world_x;
        });
    const auto maximum_x = std::max_element(
        placements.begin(),
        placements.end(),
        [](const RoomPlacement& left, const RoomPlacement& right) {
            return
                left.world_x + left.layout.pixel_width() <
                right.world_x + right.layout.pixel_width();
        });
    const auto minimum_y = std::min_element(
        placements.begin(),
        placements.end(),
        [](const RoomPlacement& left, const RoomPlacement& right) {
            return left.world_y < right.world_y;
        });
    const auto maximum_y = std::max_element(
        placements.begin(),
        placements.end(),
        [](const RoomPlacement& left, const RoomPlacement& right) {
            return
                left.world_y + left.layout.pixel_height() <
                right.world_y + right.layout.pixel_height();
        });
    RegionPixels region{
        .world_x = minimum_x->world_x,
        .world_y = minimum_y->world_y,
        .width =
            maximum_x->world_x + maximum_x->layout.pixel_width() -
            minimum_x->world_x,
        .height =
            maximum_y->world_y + maximum_y->layout.pixel_height() -
            minimum_y->world_y,
    };
    region.pixels.resize(
        static_cast<std::size_t>(region.width * region.height));
    region.rooms.reserve(placements.size());

    constexpr std::uint64_t signature_prime = 1099511628211ull;
    region.animation_signature = 14695981039346656037ull;
    for (const auto& placement : placements) {
        auto rendered =
            decoder.render(placement.layout, season, animation_tick);
        region.animation_signature ^=
            static_cast<std::uint8_t>(rendered.id.room);
        region.animation_signature *= signature_prime;
        region.animation_signature ^= rendered.animation_signature;
        region.animation_signature *= signature_prime;
        copy_room_pixels(region, placement, rendered);
        region.rooms.push_back(std::move(rendered));
    }
    return region;
}

std::uint64_t region_animation_signature(
    const oracle::content::RoomPixelDecoder& decoder,
    const std::vector<RoomPlacement>& placements,
    const oracle::content::Season season,
    const std::uint64_t animation_tick) {
    constexpr std::uint64_t signature_prime = 1099511628211ull;
    auto signature = 14695981039346656037ull;
    std::array<std::optional<std::uint64_t>, 256> group_signatures;
    for (const auto& placement : placements) {
        const auto tileset = decoder.describe_tileset(
            static_cast<std::uint8_t>(placement.layout.id.area),
            static_cast<std::uint8_t>(placement.layout.id.room),
            season);
        auto& animation_signature =
            group_signatures[tileset.animation];
        if (!animation_signature.has_value()) {
            animation_signature =
                decoder.animation_signature(
                    tileset.animation,
                    animation_tick);
        }
        signature ^=
            static_cast<std::uint8_t>(placement.layout.id.room);
        signature *= signature_prime;
        signature ^= *animation_signature;
        signature *= signature_prime;
    }
    return signature;
}

std::vector<std::size_t> update_animated_rooms(
    RegionPixels& region,
    const oracle::content::RoomPixelDecoder& decoder,
    const std::vector<RoomPlacement>& placements,
    const oracle::content::Season season,
    const std::uint64_t animation_tick,
    const std::uint64_t target_signature) {
    if (region.rooms.size() != placements.size()) {
        throw std::runtime_error{
            "animated room cache does not match world placements"};
    }
    std::array<std::optional<std::uint64_t>, 256> group_signatures;
    std::vector<std::size_t> changed;
    for (std::size_t index = 0; index < placements.size(); ++index) {
        const auto& placement = placements[index];
        const auto& cached = region.rooms[index];
        const auto group = cached.tileset.animation;
        auto& animation_signature = group_signatures[group];
        if (!animation_signature.has_value()) {
            animation_signature =
                decoder.animation_signature(group, animation_tick);
        }
        if (*animation_signature == cached.animation_signature) {
            continue;
        }
        auto rendered =
            decoder.render(placement.layout, season, animation_tick);
        copy_room_pixels(region, placement, rendered);
        region.rooms[index] = std::move(rendered);
        changed.push_back(index);
    }
    region.animation_signature = target_signature;
    return changed;
}

std::vector<RoomPlacement> decode_world_rectangle(
    const oracle::content::RoomLayoutDecoder& layout_decoder,
    const oracle::content::RoomPixelDecoder& pixel_decoder,
    const oracle::content::RoomMutationDecoder& mutation_decoder,
    const std::uint8_t world_group,
    const int minimum_x,
    const int maximum_x,
    const int minimum_y,
    const int maximum_y,
    const oracle::content::Season season,
    const std::uint8_t room_flags) {
    std::vector<RoomPlacement> rooms;
    rooms.reserve(
        static_cast<std::size_t>(
            (maximum_x - minimum_x + 1) *
            (maximum_y - minimum_y + 1)));
    for (int y = minimum_y; y <= maximum_y; ++y) {
        if (y < 0 || y > 15) {
            continue;
        }
        for (int x = minimum_x; x <= maximum_x; ++x) {
            if (x < 0 || x > 15) {
                continue;
            }
            const auto room =
                static_cast<std::uint8_t>((y << 4) | x);
            const auto tileset =
                pixel_decoder.describe_tileset(
                    world_group,
                    room,
                    season);
            if (
                layout_decoder.layout_kind(tileset.layout_group) !=
                oracle::content::RoomLayoutKind::small) {
                std::ostringstream message;
                message
                    << "world group " << std::hex
                    << static_cast<unsigned int>(world_group)
                    << " room " << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned int>(room)
                    << " selects unsupported large layout group "
                    << static_cast<unsigned int>(tileset.layout_group);
                throw std::invalid_argument{message.str()};
            }
            auto layout = layout_decoder.decode_small_room(
                    world_group,
                    tileset.layout_group,
                    room);
            layout = mutation_decoder.apply_standard_substitutions(
                std::move(layout),
                tileset,
                room_flags);
            rooms.push_back(RoomPlacement{
                .layout = std::move(layout),
                .world_x =
                    x * oracle::content::small_room_world_width,
                .world_y =
                    y * oracle::content::small_room_world_height,
            });
        }
    }
    return rooms;
}

std::vector<RoomPlacement> decode_world_neighborhood(
    const oracle::content::RoomLayoutDecoder& layout_decoder,
    const oracle::content::RoomPixelDecoder& pixel_decoder,
    const oracle::content::RoomMutationDecoder& mutation_decoder,
    const std::uint8_t world_group,
    const std::uint8_t center_room,
    const std::uint8_t radius,
    const oracle::content::Season season,
    const std::uint8_t room_flags) {
    const auto center_x = static_cast<int>(center_room & 0x0f);
    const auto center_y = static_cast<int>(center_room >> 4u);
    const auto extent = static_cast<int>(radius);
    return decode_world_rectangle(
        layout_decoder,
        pixel_decoder,
        mutation_decoder,
        world_group,
        center_x - extent,
        center_x + extent,
        center_y - extent,
        center_y + extent,
        season,
        room_flags);
}

std::vector<RoomPlacement> decode_world_room(
    const oracle::content::RoomLayoutDecoder& layout_decoder,
    const oracle::content::RoomPixelDecoder& pixel_decoder,
    const oracle::content::RoomMutationDecoder& mutation_decoder,
    const std::uint8_t world_group,
    const std::uint8_t room,
    const oracle::content::Season season,
    const std::uint8_t room_flags) {
    const auto tileset =
        pixel_decoder.describe_tileset(world_group, room, season);
    auto layout =
        layout_decoder.layout_kind(tileset.layout_group) ==
                oracle::content::RoomLayoutKind::small
            ? layout_decoder.decode_small_room(
                  world_group,
                  tileset.layout_group,
                  room)
            : layout_decoder.decode_large_room(
                  world_group,
                  tileset.layout_group,
                  room);
    layout = mutation_decoder.apply_standard_substitutions(
        std::move(layout),
        tileset,
        room_flags);
    return std::vector<RoomPlacement>{
        RoomPlacement{
            .layout = std::move(layout),
            .world_x = 0,
            .world_y = 0,
        },
    };
}

struct RuntimeWorldData {
    std::vector<RoomPlacement> rooms;
    std::vector<oracle::content::RoomCollisionMap> collisions;
    std::optional<RegionPixels> authentic_region;
    bool large_room_mode{};
};

RuntimeWorldData load_runtime_world(
    const oracle::content::RoomLayoutDecoder& layout_decoder,
    const oracle::content::RoomPixelDecoder& pixel_decoder,
    const oracle::content::RoomMutationDecoder& mutation_decoder,
    const oracle::content::RoomCollisionDecoder& collision_decoder,
    const std::uint8_t world_group,
    const std::uint8_t center_room,
    const oracle::content::Season season,
    const std::uint8_t room_flags,
    const std::uint64_t animation_tick,
    const bool force_diagnostic) {
    const auto center_tileset =
        pixel_decoder.describe_tileset(
            world_group,
            center_room,
            season);
    const bool large_room_mode =
        layout_decoder.layout_kind(center_tileset.layout_group) ==
        oracle::content::RoomLayoutKind::large;
    auto rooms = large_room_mode
        ? decode_world_room(
              layout_decoder,
              pixel_decoder,
              mutation_decoder,
              world_group,
              center_room,
              season,
              room_flags)
        : decode_world_rectangle(
              layout_decoder,
              pixel_decoder,
              mutation_decoder,
              world_group,
              0,
              15,
              0,
              15,
              season,
              room_flags);
    std::vector<oracle::content::RoomCollisionMap> collisions;
    collisions.reserve(rooms.size());
    for (const auto& placement : rooms) {
        const auto tileset = pixel_decoder.describe_tileset(
            static_cast<std::uint8_t>(placement.layout.id.area),
            static_cast<std::uint8_t>(placement.layout.id.room),
            season);
        collisions.push_back(
            collision_decoder.decode(placement.layout, tileset));
    }
    std::optional<RegionPixels> authentic_region;
    if (!force_diagnostic) {
        authentic_region =
            compose_region(
                pixel_decoder,
                rooms,
                season,
                animation_tick);
    }
    return RuntimeWorldData{
        .rooms = std::move(rooms),
        .collisions = std::move(collisions),
        .authentic_region = std::move(authentic_region),
        .large_room_mode = large_room_mode,
    };
}

std::string campaign_name(const oracle::core::Campaign campaign) {
    return campaign == oracle::core::Campaign::ages ? "Ages" : "Seasons";
}

std::string_view exit_kind_name(
    const oracle::content::RoomExitKind kind) {
    using oracle::content::RoomExitKind;
    switch (kind) {
    case RoomExitKind::north_seam:
        return "north-seam";
    case RoomExitKind::east_seam:
        return "east-seam";
    case RoomExitKind::south_seam:
        return "south-seam";
    case RoomExitKind::west_seam:
        return "west-seam";
    case RoomExitKind::tile_warp:
        return "tile-warp";
    case RoomExitKind::screen_edge_warp:
        return "screen-edge-warp";
    case RoomExitKind::fallback_warp:
        return "fallback-warp";
    }
    return "unknown";
}

void print_room_exits(
    const std::vector<oracle::content::RoomExit>& exits) {
    std::cout << "exit_count=" << exits.size() << '\n';
    for (std::size_t index = 0; index < exits.size(); ++index) {
        const auto& exit = exits[index];
        std::cout
            << "exit[" << index << "]=" << exit_kind_name(exit.kind)
            << " source=" << std::hex << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned int>(exit.source.area)
            << ':' << std::setw(2)
            << static_cast<unsigned int>(exit.source.room)
            << " destination=" << std::setw(2)
            << static_cast<unsigned int>(exit.destination.area)
            << ':' << std::setw(2)
            << static_cast<unsigned int>(exit.destination.room);
        if (exit.has_source_position) {
            std::cout
                << " source_yx=" << std::setw(2)
                << static_cast<unsigned int>(exit.source_position);
        }
        if (
            exit.kind == oracle::content::RoomExitKind::screen_edge_warp) {
            std::cout
                << " edge_mask=" << std::setw(2)
                << static_cast<unsigned int>(exit.source_edge_mask);
        }
        if (
            exit.kind == oracle::content::RoomExitKind::tile_warp ||
            exit.kind == oracle::content::RoomExitKind::screen_edge_warp ||
            exit.kind == oracle::content::RoomExitKind::fallback_warp) {
            std::cout
                << " destination_yx=" << std::setw(2)
                << static_cast<unsigned int>(exit.destination_position)
                << " dest_index=" << std::setw(2)
                << static_cast<unsigned int>(exit.destination_index)
                << " transitions="
                << static_cast<unsigned int>(exit.source_transition)
                << '/' << static_cast<unsigned int>(
                       exit.destination_transition)
                << " parameter="
                << static_cast<unsigned int>(
                       exit.destination_parameter);
        }
        std::cout << std::dec << '\n';
    }
}

void print_topology_catalog(
    const oracle::content::RoomTopologyDecoder& topology,
    const std::uint8_t destination_variant) {
    constexpr std::uint64_t signature_prime = 1099511628211ull;
    auto signature = 14695981039346656037ull;
    std::size_t warp_edges = 0;
    std::size_t tile_edges = 0;
    std::size_t screen_edge_edges = 0;
    std::size_t fallback_edges = 0;
    std::size_t destinations = 0;
    for (std::uint8_t group = 0; group < 8; ++group) {
        destinations += topology.warp_destinations(group).size();
        for (std::uint16_t room = 0; room < 256; ++room) {
            const auto exits = topology.exits(
                group,
                static_cast<std::uint8_t>(room),
                destination_variant,
                false);
            for (const auto& exit : exits) {
                ++warp_edges;
                tile_edges +=
                    exit.kind == oracle::content::RoomExitKind::tile_warp
                    ? 1u
                    : 0u;
                screen_edge_edges +=
                    exit.kind ==
                        oracle::content::RoomExitKind::screen_edge_warp
                    ? 1u
                    : 0u;
                fallback_edges +=
                    exit.kind == oracle::content::RoomExitKind::fallback_warp
                    ? 1u
                    : 0u;
                signature ^= exit.source.area;
                signature *= signature_prime;
                signature ^= exit.source.room;
                signature *= signature_prime;
                signature ^= exit.destination.area;
                signature *= signature_prime;
                signature ^= exit.destination.room;
                signature *= signature_prime;
                signature ^= static_cast<std::uint8_t>(exit.kind);
                signature *= signature_prime;
            }
        }
    }
    std::cout
        << "topology_destinations=" << destinations << '\n'
        << "topology_warp_edges=" << warp_edges << '\n'
        << "topology_tile_edges=" << tile_edges << '\n'
        << "topology_screen_edge_edges=" << screen_edge_edges << '\n'
        << "topology_fallback_edges=" << fallback_edges << '\n'
        << "topology_signature=" << std::hex << std::setw(16)
        << std::setfill('0') << signature << std::dec << '\n';
}

void print_object_catalog(
    const oracle::content::RoomObjectDecoder& decoder,
    const std::uint8_t room_flags) {
    constexpr std::uint64_t signature_prime = 1099511628211ull;
    auto signature = 14695981039346656037ull;
    std::size_t populated_rooms = 0;
    std::size_t records = 0;
    std::size_t positioned = 0;
    std::array<std::size_t, 4> kinds{};
    for (std::uint8_t group = 0; group < 8; ++group) {
        for (std::uint16_t room = 0; room < 256; ++room) {
            oracle::content::RoomObjectCatalog catalog;
            try {
                catalog = decoder.decode(
                    group,
                    static_cast<std::uint8_t>(room),
                    room_flags);
            } catch (const std::exception& error) {
                std::ostringstream message;
                message
                    << "object catalog failed at "
                    << std::hex << std::setw(2)
                    << std::setfill('0')
                    << static_cast<unsigned int>(group)
                    << ':' << std::setw(2) << room
                    << ": " << error.what();
                throw std::runtime_error{message.str()};
            }
            populated_rooms += catalog.records.empty() ? 0u : 1u;
            records += catalog.records.size();
            for (const auto& object : catalog.records) {
                positioned += object.positioned ? 1u : 0u;
                ++kinds[static_cast<std::size_t>(object.kind)];
                signature ^= group;
                signature *= signature_prime;
                signature ^= room;
                signature *= signature_prime;
                signature ^= static_cast<std::uint8_t>(object.kind);
                signature *= signature_prime;
                signature ^= object.id;
                signature *= signature_prime;
                signature ^= object.subid;
                signature *= signature_prime;
                signature ^= object.original_y;
                signature *= signature_prime;
                signature ^= object.original_x;
                signature *= signature_prime;
            }
        }
    }
    std::cout
        << "object_catalog_rooms=2048\n"
        << "object_catalog_populated_rooms=" << populated_rooms << '\n'
        << "object_catalog_records=" << records << '\n'
        << "object_catalog_positioned=" << positioned << '\n'
        << "object_catalog_interactions=" << kinds[0] << '\n'
        << "object_catalog_enemies=" << kinds[1] << '\n'
        << "object_catalog_parts=" << kinds[2] << '\n'
        << "object_catalog_item_drops=" << kinds[3] << '\n'
        << "object_catalog_signature=" << std::hex << std::setw(16)
        << std::setfill('0') << signature << std::dec << '\n';
}

void print_description(
    const oracle::content::RomSource& rom,
    const std::vector<RoomPlacement>& rooms,
    const std::uint8_t world_group,
    const std::uint8_t center_room,
    const bool atlas_mode,
    const bool large_room_mode,
    const bool player_mode,
    const std::vector<oracle::content::RoomCollisionMap>& collisions,
    const RegionPixels* authentic_region,
    const std::uint64_t animation_tick,
    const std::uint8_t room_flags) {
    constexpr std::uint64_t signature_prime = 1099511628211ull;
    auto layout_signature = 14695981039346656037ull;
    std::unordered_set<std::uint8_t> metatiles;
    auto collision_signature = 14695981039346656037ull;
    std::size_t special_collisions = 0;
    std::size_t collision_cells = 0;
    for (const auto& room : rooms) {
        layout_signature ^= room.layout.id.area;
        layout_signature *= signature_prime;
        layout_signature ^= room.layout.id.room;
        layout_signature *= signature_prime;
        for (const auto metatile : room.layout.metatiles) {
            layout_signature ^= metatile;
            layout_signature *= signature_prime;
        }
        metatiles.insert(
            room.layout.metatiles.begin(),
            room.layout.metatiles.end());
    }
    for (const auto& map : collisions) {
        const auto map_signature =
            oracle::content::RoomCollisionDecoder::signature(map);
        collision_signature ^= map_signature;
        collision_signature *= signature_prime;
        for (const auto value : map.values) {
            collision_cells += value != 0 ? 1u : 0u;
            special_collisions +=
                oracle::content::RoomCollisionDecoder::is_special(value)
                ? 1u
                : 0u;
        }
    }
    std::cout
        << "campaign=" << campaign_name(rom.metadata().campaign) << '\n'
        << "game_code=" << rom.metadata().game_code << '\n'
        << "fingerprint=" << std::hex << std::setw(16)
        << std::setfill('0') << rom.metadata().compatibility_fingerprint
        << '\n'
        << "world_group=" << std::setw(2)
        << static_cast<unsigned int>(world_group) << '\n'
        << "center_room=" << std::setw(2)
        << static_cast<unsigned int>(center_room) << '\n'
        << std::dec
        << "region="
        << (
            atlas_mode
            ? "atlas"
            : large_room_mode
            ? "large-room"
            : player_mode ? "traversable-world" : "neighborhood")
        << '\n'
        << "decoded_rooms=" << rooms.size() << '\n'
        << "room_columns=" << rooms.front().layout.columns << '\n'
        << "room_rows=" << rooms.front().layout.rows << '\n'
        << "unique_metatiles=" << metatiles.size() << '\n'
        << "layout_signature=" << std::hex << std::setw(16)
        << std::setfill('0') << layout_signature << std::dec << '\n'
        << "collision_signature=" << std::hex << std::setw(16)
        << std::setfill('0') << collision_signature << std::dec << '\n'
        << "collision_cells=" << collision_cells << '\n'
        << "special_collision_cells=" << special_collisions << '\n'
        << "render_mode="
        << (authentic_region != nullptr ? "authentic-rom" : "diagnostic")
        << '\n'
        << "animation_tick=" << animation_tick << '\n'
        << "preview_room_flags=" << std::hex << std::setw(2)
        << static_cast<unsigned int>(room_flags) << std::dec << '\n';
    if (authentic_region != nullptr) {
        std::cout
            << "region_width=" << authentic_region->width << '\n'
            << "region_height=" << authentic_region->height << '\n'
            << "animation_signature=" << std::hex << std::setw(16)
            << std::setfill('0')
            << authentic_region->animation_signature << std::dec << '\n';
        const auto center = std::find_if(
            authentic_region->rooms.begin(),
            authentic_region->rooms.end(),
            [center_room](const oracle::content::RenderedRoom& room) {
                return room.id.room == center_room;
            });
        if (center != authentic_region->rooms.end()) {
            std::cout
                << "tileset_index=" << std::hex << std::setw(2)
                << static_cast<unsigned int>(center->tileset.index) << '\n'
                << "mapping_index=" << std::setw(2)
                << static_cast<unsigned int>(center->tileset.mapping) << '\n'
                << "main_gfx_header=" << std::setw(2)
                << static_cast<unsigned int>(
                       center->tileset.main_graphics)
                << '\n'
                << "unique_gfx_header=" << std::setw(2)
                << static_cast<unsigned int>(
                       center->tileset.unique_graphics)
                << '\n'
                << "palette_header=" << std::setw(2)
                << static_cast<unsigned int>(center->tileset.palette)
                << std::dec << '\n';
        }
    }
}

void save_region_bmp(
    RegionPixels& region,
    const std::filesystem::path& output_path) {
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        region.width,
        region.height,
        SDL_PIXELFORMAT_RGBA32,
        region.pixels.data(),
        region.width *
            static_cast<int>(sizeof(oracle::content::RgbaPixel)));
    if (surface == nullptr) {
        throw std::runtime_error{
            std::string{"atlas surface creation failed: "} +
            SDL_GetError()};
    }
    const auto output = output_path.string();
    const bool saved = SDL_SaveBMP(surface, output.c_str());
    SDL_DestroySurface(surface);
    if (!saved) {
        throw std::runtime_error{
            std::string{"atlas export failed: "} + SDL_GetError()};
    }
    std::cout
        << "region_export=" << output << '\n'
        << "export_width=" << region.width << '\n'
        << "export_height=" << region.height << '\n';
}

int run_window(
    const oracle::content::RomSource& rom,
    std::vector<RoomPlacement> rooms,
    std::vector<oracle::content::RoomCollisionMap> collisions,
    std::optional<RegionPixels> authentic_region,
    const oracle::content::RoomLayoutDecoder& layout_decoder,
    const oracle::content::RoomPixelDecoder& pixel_decoder,
    const oracle::content::RoomMutationDecoder& mutation_decoder,
    const oracle::content::RoomCollisionDecoder& collision_decoder,
    const oracle::content::RoomObjectDecoder& object_decoder,
    const oracle::content::RoomTopologyDecoder& topology_decoder,
    const oracle::content::Season season,
    const std::uint8_t center_room,
    const bool atlas_mode,
    bool large_room_mode,
    const bool player_mode,
    const bool force_diagnostic,
    const bool force_collision_overlay,
    const bool force_object_overlay,
    const std::uint8_t room_flags,
    const std::uint64_t starting_animation_tick,
    std::optional<std::filesystem::path> screenshot_path,
    const std::optional<std::uint8_t> spawn_position) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        throw std::runtime_error{
            std::string{"SDL initialization failed: "} + SDL_GetError()};
    }

    SDL_Window* window = SDL_CreateWindow(
        "Oracle ROM Room Slice",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        const std::string message = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error{"window creation failed: " + message};
    }

    SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV,
        false,
        nullptr);
    if (gpu_device == nullptr) {
        const std::string message = SDL_GetError();
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error{
            "SDL3 GPU device creation failed: " + message};
    }

    SDL_Renderer* renderer = SDL_CreateGPURenderer(gpu_device, window);
    if (renderer == nullptr) {
        const std::string message = SDL_GetError();
        SDL_DestroyGPUDevice(gpu_device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error{
            "SDL3 GPU renderer creation failed: " + message};
    }
    SDL_SetRenderVSync(renderer, 1);

    const oracle::content::LinkSpriteDecoder link_sprite_decoder{rom};
    const oracle::content::InteractionSpriteDecoder
        interaction_sprite_decoder{rom};
    const oracle::content::EnemySpriteDecoder enemy_sprite_decoder{rom};
    const oracle::content::PartSpriteDecoder part_sprite_decoder{rom};
    const oracle::content::SwordSpriteDecoder sword_sprite_decoder{rom};
    const auto octorok_projectile_frame =
        part_sprite_decoder.decode_octorok_projectile();
    SDL_Texture* link_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        16,
        16);
    if (link_texture == nullptr) {
        throw std::runtime_error{
            std::string{"Link texture creation failed: "} +
            SDL_GetError()};
    }
    SDL_SetTextureScaleMode(link_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(link_texture, SDL_BLENDMODE_BLEND);
    std::optional<std::uint8_t> uploaded_link_frame;
    SDL_Texture* sword_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        32,
        32);
    if (sword_texture == nullptr) {
        throw std::runtime_error{
            std::string{"sword texture creation failed: "} +
            SDL_GetError()};
    }
    SDL_SetTextureScaleMode(sword_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(sword_texture, SDL_BLENDMODE_BLEND);
    SDL_Texture* octorok_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        32,
        32);
    if (octorok_texture == nullptr) {
        throw std::runtime_error{
            std::string{"Octorok texture creation failed: "} +
            SDL_GetError()};
    }
    SDL_SetTextureScaleMode(octorok_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(octorok_texture, SDL_BLENDMODE_BLEND);
    SDL_Texture* projectile_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        octorok_projectile_frame.width,
        octorok_projectile_frame.height);
    if (
        projectile_texture == nullptr ||
        !SDL_UpdateTexture(
            projectile_texture,
            nullptr,
            octorok_projectile_frame.pixels.data(),
            octorok_projectile_frame.width *
                static_cast<int>(
                    sizeof(oracle::content::RgbaPixel)))) {
        throw std::runtime_error{
            std::string{"projectile texture creation failed: "} +
            SDL_GetError()};
    }
    SDL_SetTextureScaleMode(
        projectile_texture,
        SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(
        projectile_texture,
        SDL_BLENDMODE_BLEND);
    std::array<SDL_Texture*, 3> vasu_textures{};
    for (auto*& texture : vasu_textures) {
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            64,
            64);
        if (texture == nullptr) {
            throw std::runtime_error{
                std::string{"Vasu texture creation failed: "} +
                SDL_GetError()};
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    SDL_Texture* region_texture = nullptr;
    if (authentic_region.has_value()) {
        region_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC,
            authentic_region->width,
            authentic_region->height);
        if (region_texture == nullptr) {
            throw std::runtime_error{
                std::string{"room texture creation failed: "} +
                SDL_GetError()};
        }
        if (!SDL_UpdateTexture(
                region_texture,
                nullptr,
                authentic_region->pixels.data(),
                authentic_region->width *
                    static_cast<int>(
                        sizeof(oracle::content::RgbaPixel)))) {
            throw std::runtime_error{
                std::string{"room texture upload failed: "} +
                SDL_GetError()};
        }
        SDL_SetTextureScaleMode(region_texture, SDL_SCALEMODE_NEAREST);
    }

    std::ostringstream title;
    title << "Oracle of " << campaign_name(rom.metadata().campaign)
          << " - ROM Room Slice - SDL3 GPU";
    SDL_SetWindowTitle(window, title.str().c_str());

    const auto player_group =
        static_cast<std::uint8_t>(
            std::find_if(
                rooms.begin(),
                rooms.end(),
                [center_room](const RoomPlacement& placement) {
                    return placement.layout.id.room == center_room;
                })->layout.id.area);
    const auto collision_lookup =
        [&](const oracle::core::WorldRoomId id)
            -> const oracle::content::RoomCollisionMap* {
            const auto found = std::find_if(
                collisions.begin(),
                collisions.end(),
                [id](const oracle::content::RoomCollisionMap& map) {
                    return map.id == id;
                });
            return found == collisions.end() ? nullptr : &*found;
        };
    oracle::gameplay::PlayerState initial_player = spawn_position.has_value()
        ? oracle::gameplay::PlayerTraversal::from_packed_room_position(
              oracle::core::WorldRoomId{
                  .area = player_group,
                  .room = center_room,
              },
              *spawn_position)
        : oracle::gameplay::PlayerState{
        .room =
            oracle::core::WorldRoomId{
                .area = player_group,
                .room = center_room,
            },
        .local_x = large_room_mode
            ? rooms.front().layout.pixel_width() * 0.5
            : oracle::content::small_room_world_width * 0.5,
        .local_y = large_room_mode
            ? rooms.front().layout.pixel_height() * 0.5
            : oracle::content::small_room_world_height * 0.5,
    };
    const auto selected_vasu_scenario =
        oracle::gameplay::vasu_scenario(rom.metadata().campaign);
    const auto selected_octorok_scenario =
        oracle::gameplay::octorok_scenario(rom.metadata().campaign);
    if (
        (
            spawn_position == selected_vasu_scenario.player_spawn_yx &&
            initial_player.room == selected_vasu_scenario.room) ||
        (
            spawn_position == selected_octorok_scenario.player_spawn_yx &&
            initial_player.room == selected_octorok_scenario.room)) {
        initial_player.facing =
            oracle::gameplay::PlayerFacing::north;
    }
    if (
        player_mode &&
        !spawn_position.has_value() &&
        !oracle::gameplay::PlayerTraversal::place_near(
            initial_player,
            initial_player.local_x,
            initial_player.local_y,
            collision_lookup)) {
        throw std::runtime_error{
            "no traversable player start was found near the room center"};
    }
    auto previous_player = initial_player;
    auto current_player = initial_player;

    const auto center_x = player_mode
        ? oracle::gameplay::PlayerTraversal::world_x(initial_player)
        : large_room_mode
        ? rooms.front().layout.pixel_width() * 0.5
        : atlas_mode
        ? oracle::content::small_room_world_width * 8.0
        : static_cast<double>(
              (center_room & 0x0f) *
                  oracle::content::small_room_world_width +
              oracle::content::small_room_world_width / 2);
    const auto center_y = player_mode
        ? oracle::gameplay::PlayerTraversal::world_y(initial_player)
        : large_room_mode
        ? rooms.front().layout.pixel_height() * 0.5
        : atlas_mode
        ? oracle::content::small_room_world_height * 8.0
        : static_cast<double>(
              (center_room >> 4u) *
                  oracle::content::small_room_world_height +
              oracle::content::small_room_world_height / 2);
    const auto initial_zoom = atlas_mode
        ? std::min(
              1180.0 /
                  (16.0 * oracle::content::small_room_world_width),
              620.0 /
                  (16.0 * oracle::content::small_room_world_height))
        : 3.0;
    CameraState previous{
        .x = center_x,
        .y = center_y,
        .zoom = initial_zoom,
    };
    CameraState current = previous;
    std::optional<oracle::content::RoomExit> last_warp;
    std::optional<oracle::core::WorldRoomId> deactivated_warp_room;
    std::uint8_t deactivated_warp_position{};
    double warp_cooldown = 0.0;
    std::uint64_t logic_tick = starting_animation_tick;
    bool diagnostic =
        force_diagnostic || !authentic_region.has_value();
    bool player_moving = false;
    auto current_objects = object_decoder.decode(
        player_group,
        center_room,
        room_flags);
    oracle::core::ActorSlotDomain current_actors;
    auto actor_load_report =
        oracle::gameplay::RoomActorLoader::load(
            current_objects,
            current_actors);
    oracle::gameplay::VasuInteractionRuntime vasu_runtime{rom};
    oracle::gameplay::OctorokRuntime octorok_runtime{rom};
    oracle::gameplay::SwordRuntime sword_runtime;
    oracle::gameplay::PlayerCombatState player_combat;
    oracle::gameplay::OctorokStepReport last_combat_step;
    oracle::gameplay::SwordStepReport last_sword_step;
    oracle::input::SemanticInputSampler semantic_input;
    const auto reload_current_objects = [&]() {
        current_objects = object_decoder.decode(
            static_cast<std::uint8_t>(current_player.room.area),
            static_cast<std::uint8_t>(current_player.room.room),
            room_flags);
        current_actors.clear();
        actor_load_report =
            oracle::gameplay::RoomActorLoader::load(
                current_objects,
                current_actors);
    };

    const auto rebuild_region_texture = [&]() {
        SDL_DestroyTexture(region_texture);
        region_texture = nullptr;
        if (!authentic_region.has_value()) {
            return;
        }
        region_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC,
            authentic_region->width,
            authentic_region->height);
        if (
            region_texture == nullptr ||
            !SDL_UpdateTexture(
                region_texture,
                nullptr,
                authentic_region->pixels.data(),
                authentic_region->width *
                    static_cast<int>(
                        sizeof(oracle::content::RgbaPixel)))) {
            throw std::runtime_error{
                std::string{"warped room texture upload failed: "} +
                SDL_GetError()};
        }
        SDL_SetTextureScaleMode(region_texture, SDL_SCALEMODE_NEAREST);
    };
    const auto execute_warp =
        [&](const oracle::content::RoomExit& warp) {
            const auto preserve_diagnostic_view = diagnostic;
            auto loaded = load_runtime_world(
                layout_decoder,
                pixel_decoder,
                mutation_decoder,
                collision_decoder,
                static_cast<std::uint8_t>(warp.destination.area),
                static_cast<std::uint8_t>(warp.destination.room),
                season,
                room_flags,
                logic_tick,
                force_diagnostic);
            rooms = std::move(loaded.rooms);
            collisions = std::move(loaded.collisions);
            authentic_region = std::move(loaded.authentic_region);
            large_room_mode = loaded.large_room_mode;
            rebuild_region_texture();

            const auto destination_placement = std::find_if(
                rooms.begin(),
                rooms.end(),
                [&](const RoomPlacement& placement) {
                    return placement.layout.id == warp.destination;
                });
            if (destination_placement == rooms.end()) {
                throw std::runtime_error{
                    "warped destination room was not loaded"};
            }
            current_player =
                oracle::gameplay::PlayerTraversal::
                    from_transition_destination(
                        warp.destination,
                        warp.destination_position,
                        warp.destination_parameter,
                        warp.destination_transition,
                        destination_placement->layout.pixel_width(),
                        destination_placement->layout.pixel_height());
            if (
                !oracle::gameplay::PlayerTraversal::can_occupy(
                    current_player,
                    collision_lookup)) {
                static_cast<void>(
                    oracle::gameplay::PlayerTraversal::place_near(
                        current_player,
                        current_player.local_x,
                        current_player.local_y,
                        collision_lookup));
            }
            previous_player = current_player;
            initial_player = current_player;
            reload_current_objects();
            current.x =
                oracle::gameplay::PlayerTraversal::world_x(
                    current_player);
            current.y =
                oracle::gameplay::PlayerTraversal::world_y(
                    current_player);
            previous = current;
            last_warp = warp;
            warp_cooldown = 0.35;
            deactivated_warp_room = current_player.room;
            deactivated_warp_position =
                oracle::gameplay::PlayerTraversal::
                    packed_room_position(current_player);
            diagnostic =
                preserve_diagnostic_view ||
                force_diagnostic ||
                !authentic_region.has_value();
        };

    constexpr double logic_step = 1.0 / 60.0;
    double accumulator = 0.0;
    auto last_counter = SDL_GetPerformanceCounter();
    const auto counter_frequency =
        static_cast<double>(SDL_GetPerformanceFrequency());
    bool running = true;
    bool collision_overlay = force_collision_overlay;
    bool object_overlay = force_object_overlay;
    const auto screenshot_ready_tick =
        starting_animation_tick + 2;

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (
                event.type == SDL_EVENT_KEY_DOWN ||
                event.type == SDL_EVENT_KEY_UP) {
                const auto action = semantic_action(event.key.key);
                if (action.has_value()) {
                    semantic_input.set(
                        *action,
                        event.type == SDL_EVENT_KEY_DOWN);
                }
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                semantic_input.release_all();
            }
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_R) {
                if (
                    player_mode &&
                    current_player.room.area !=
                        initial_player.room.area) {
                    // initial_player is updated after every warp, so this is
                    // only defensive if a future reset target spans groups.
                    current_player = initial_player;
                }
                previous = CameraState{
                    .x = player_mode
                        ? oracle::gameplay::PlayerTraversal::world_x(
                              initial_player)
                        : center_x,
                    .y = player_mode
                        ? oracle::gameplay::PlayerTraversal::world_y(
                              initial_player)
                        : center_y,
                    .zoom = initial_zoom,
                };
                current = previous;
                previous_player = initial_player;
                current_player = initial_player;
                reload_current_objects();
                octorok_runtime.reset();
                sword_runtime.reset();
                player_combat =
                    oracle::gameplay::PlayerCombatState{};
                last_combat_step =
                    oracle::gameplay::OctorokStepReport{};
                last_sword_step =
                    oracle::gameplay::SwordStepReport{};
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_F1 &&
                authentic_region.has_value()) {
                diagnostic = !diagnostic;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_F2) {
                collision_overlay = !collision_overlay;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_F3) {
                object_overlay = !object_overlay;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                current.zoom = std::clamp(
                    current.zoom * std::pow(1.15, event.wheel.y),
                    0.1,
                    8.0);
            }
        }

        const auto now = SDL_GetPerformanceCounter();
        const auto elapsed = std::min(
            0.25,
            static_cast<double>(now - last_counter) / counter_frequency);
        last_counter = now;
        accumulator += elapsed;

        while (accumulator >= logic_step) {
            previous = current;
            previous_player = current_player;
            const auto input_frame = semantic_input.sample();
            const auto pan_speed = 150.0 / current.zoom;
            const auto horizontal =
                (input_frame.held(
                     oracle::input::InputAction::right) ? 1.0 : 0.0) -
                (input_frame.held(
                     oracle::input::InputAction::left) ? 1.0 : 0.0);
            const auto vertical =
                (input_frame.held(
                     oracle::input::InputAction::down) ? 1.0 : 0.0) -
                (input_frame.held(
                     oracle::input::InputAction::up) ? 1.0 : 0.0);
            if (player_mode) {
                vasu_runtime.update(
                    input_frame,
                    current_player,
                    current_actors);
                const auto gameplay_input =
                    vasu_runtime.captures_input()
                    ? oracle::input::InputFrame{}
                    : input_frame;
                last_sword_step = sword_runtime.update(
                    gameplay_input,
                    current_player,
                    current_actors);
                last_combat_step = octorok_runtime.update(
                    current_player,
                    player_combat,
                    current_actors,
                    collision_lookup,
                    last_sword_step);
                const auto actor_collision_bodies =
                    oracle::gameplay::collect_actor_collision_bodies(
                        current_actors);
                const auto traversal =
                    oracle::gameplay::PlayerTraversal::step(
                        current_player,
                        oracle::gameplay::MovementInput{
                            .horizontal =
                                (
                                    vasu_runtime.captures_input() ||
                                    last_sword_step.pose.has_value())
                                ? 0.0
                                : horizontal,
                            .vertical =
                                (
                                    vasu_runtime.captures_input() ||
                                    last_sword_step.pose.has_value())
                                ? 0.0
                                : vertical,
                        },
                        logic_step,
                        collision_lookup,
                        actor_collision_bodies.bodies());
                player_moving = traversal.moved;
                if (traversal.crossed_room_seam) {
                    reload_current_objects();
                }
                warp_cooldown =
                    std::max(0.0, warp_cooldown - logic_step);

                std::optional<oracle::content::RoomExit> warp;
                if (traversal.crossed_room_seam) {
                    std::uint8_t edge_mask = 0;
                    const auto source_room = static_cast<std::uint8_t>(
                        traversal.previous_room.room);
                    const auto destination_room =
                        static_cast<std::uint8_t>(
                            current_player.room.room);
                    const auto right_half =
                        previous_player.local_x >=
                        oracle::content::small_room_world_width * 0.5;
                    if (
                        destination_room ==
                        static_cast<std::uint8_t>(
                            source_room - 0x10)) {
                        edge_mask = right_half ? 0x02 : 0x01;
                    } else if (
                        destination_room ==
                        static_cast<std::uint8_t>(
                            source_room + 0x10)) {
                        edge_mask = right_half ? 0x08 : 0x04;
                    }
                    if (edge_mask != 0) {
                        warp =
                            topology_decoder.resolve_screen_edge_warp(
                                static_cast<std::uint8_t>(
                                    traversal.previous_room.area),
                                source_room,
                                edge_mask,
                                static_cast<std::uint8_t>(season));
                    }
                }
                const auto current_packed_position =
                    oracle::gameplay::PlayerTraversal::
                        packed_room_position(current_player);
                if (
                    deactivated_warp_room.has_value() &&
                    (
                        *deactivated_warp_room != current_player.room ||
                        deactivated_warp_position !=
                            current_packed_position)) {
                    deactivated_warp_room.reset();
                }
                const bool standing_on_deactivated_warp =
                    deactivated_warp_room.has_value() &&
                    *deactivated_warp_room == current_player.room &&
                    deactivated_warp_position ==
                        current_packed_position;
                if (
                    !warp.has_value() &&
                    warp_cooldown == 0.0 &&
                    !standing_on_deactivated_warp) {
                    const auto local_column = static_cast<std::size_t>(
                        std::max(0.0, current_player.local_x) /
                        oracle::content::metatile_world_size);
                    const auto local_row = static_cast<std::size_t>(
                        std::max(0.0, current_player.local_y) /
                        oracle::content::metatile_world_size);
                    const auto placement = std::find_if(
                        rooms.begin(),
                        rooms.end(),
                        [&](const RoomPlacement& candidate) {
                            return
                                candidate.layout.id ==
                                current_player.room;
                        });
                    const auto centered_x = std::abs(
                        std::fmod(current_player.local_x, 16.0) - 8.0) <=
                        3.0;
                    const auto centered_y = std::abs(
                        std::fmod(current_player.local_y, 16.0) - 8.0) <=
                        3.0;
                    if (
                        placement != rooms.end() &&
                        local_column < placement->layout.columns &&
                        local_row < placement->layout.rows &&
                        centered_x &&
                        centered_y) {
                        const auto metatile =
                            placement->layout.metatiles[
                                local_row * placement->layout.columns +
                                local_column];
                        const auto tileset =
                            pixel_decoder.describe_tileset(
                                static_cast<std::uint8_t>(
                                    current_player.room.area),
                                static_cast<std::uint8_t>(
                                    current_player.room.room),
                                season);
                        warp = topology_decoder.resolve_tile_warp(
                            static_cast<std::uint8_t>(
                                current_player.room.area),
                            static_cast<std::uint8_t>(
                                current_player.room.room),
                            current_packed_position,
                            metatile,
                            tileset.collision_mode,
                            static_cast<std::uint8_t>(season));
                    }
                }
                if (warp.has_value()) {
                    execute_warp(*warp);
                }
                current.x =
                    oracle::gameplay::PlayerTraversal::world_x(
                        current_player);
                current.y =
                    oracle::gameplay::PlayerTraversal::world_y(
                        current_player);
            } else {
                current.x += horizontal * pan_speed * logic_step;
                current.y += vertical * pan_speed * logic_step;
            }
            accumulator -= logic_step;
            ++logic_tick;
        }

        const oracle::presentation::FrameTiming timing{
            logic_tick == 0 ? 0 : logic_tick - 1,
            logic_tick,
            accumulator / logic_step,
        };
        const CameraState render_camera{
            .x = timing.interpolate(previous.x, current.x),
            .y = timing.interpolate(previous.y, current.y),
            .zoom = timing.interpolate(previous.zoom, current.zoom),
        };

        int output_width = 0;
        int output_height = 0;
        SDL_GetRenderOutputSize(renderer, &output_width, &output_height);
        if (authentic_region.has_value()) {
            const auto signature =
                region_animation_signature(
                    pixel_decoder,
                    rooms,
                    season,
                    logic_tick);
            if (signature != authentic_region->animation_signature) {
                const auto changed = update_animated_rooms(
                    *authentic_region,
                    pixel_decoder,
                    rooms,
                    season,
                    logic_tick,
                    signature);
                for (const auto index : changed) {
                    const auto& placement = rooms[index];
                    const auto& rendered =
                        authentic_region->rooms[index];
                    const SDL_Rect rectangle{
                        .x =
                            placement.world_x -
                            authentic_region->world_x,
                        .y =
                            placement.world_y -
                            authentic_region->world_y,
                        .w = rendered.width,
                        .h = rendered.height,
                    };
                    if (!SDL_UpdateTexture(
                            region_texture,
                            &rectangle,
                            rendered.pixels.data(),
                            rendered.width *
                                static_cast<int>(
                                    sizeof(
                                        oracle::content::RgbaPixel)))) {
                        throw std::runtime_error{
                            std::string{
                                "animated room texture upload failed: "} +
                            SDL_GetError()};
                    }
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 12, 16, 24, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        if (diagnostic) {
            render_diagnostic_region(
                renderer,
                rooms,
                rom.metadata().campaign,
                render_camera,
                output_width,
                output_height,
                player_mode
                    ? static_cast<std::uint8_t>(
                          current_player.room.room)
                    : center_room);
        } else {
            const SDL_FRect destination{
                .x = static_cast<float>(
                    (authentic_region->world_x - render_camera.x) *
                        render_camera.zoom +
                    output_width * 0.5),
                .y = static_cast<float>(
                    (authentic_region->world_y - render_camera.y) *
                        render_camera.zoom +
                    output_height * 0.5),
                .w = static_cast<float>(
                    authentic_region->width * render_camera.zoom),
                .h = static_cast<float>(
                    authentic_region->height * render_camera.zoom),
            };
            SDL_RenderTexture(
                renderer,
                region_texture,
                nullptr,
                &destination);
            render_room_borders(
                renderer,
                rooms,
                render_camera,
                output_width,
                output_height,
                player_mode
                    ? static_cast<std::uint8_t>(
                          current_player.room.room)
                    : center_room);
        }
        if (collision_overlay) {
            render_collision_overlay(
                renderer,
                rooms,
                collisions,
                render_camera,
                output_width,
                output_height);
        }
        if (object_overlay && player_mode) {
            render_object_anchors(
                renderer,
                current_objects,
                rooms,
                render_camera,
                output_width,
                output_height);
        }
        if (player_mode) {
            render_octorok_actors(
                renderer,
                octorok_texture,
                enemy_sprite_decoder,
                octorok_runtime,
                current_actors,
                rooms,
                current_player,
                false,
                render_camera,
                output_width,
                output_height);
            render_octorok_projectiles(
                renderer,
                projectile_texture,
                octorok_projectile_frame,
                octorok_runtime,
                current_actors,
                rooms,
                current_player,
                false,
                render_camera,
                output_width,
                output_height);
            render_vasu_actors(
                renderer,
                vasu_textures,
                interaction_sprite_decoder,
                current_actors,
                rooms,
                current_player,
                false,
                logic_tick,
                render_camera,
                output_width,
                output_height);
            const auto link_frame =
                last_sword_step.pose.has_value()
                ? link_sprite_decoder.decode_original_frame(
                      last_sword_step.pose->link_frame)
                : link_sprite_decoder.decode(
                      link_direction(current_player.facing),
                      player_moving,
                      logic_tick);
            if (
                !uploaded_link_frame.has_value() ||
                *uploaded_link_frame != link_frame.original_frame) {
                if (!SDL_UpdateTexture(
                        link_texture,
                        nullptr,
                        link_frame.pixels.data(),
                        link_frame.width *
                            static_cast<int>(
                                sizeof(oracle::content::RgbaPixel)))) {
                    throw std::runtime_error{
                        std::string{"Link texture upload failed: "} +
                        SDL_GetError()};
                }
                uploaded_link_frame = link_frame.original_frame;
            }
            render_player(
                renderer,
                link_texture,
                timing.interpolate(
                    oracle::gameplay::PlayerTraversal::world_x(
                        previous_player),
                    oracle::gameplay::PlayerTraversal::world_x(
                        current_player)),
                timing.interpolate(
                    oracle::gameplay::PlayerTraversal::world_y(
                        previous_player),
                    oracle::gameplay::PlayerTraversal::world_y(
                        current_player)),
                render_camera,
                output_width,
                output_height);
            render_sword(
                renderer,
                sword_texture,
                sword_sprite_decoder,
                last_sword_step,
                current_player,
                render_camera,
                output_width,
                output_height);
            render_octorok_actors(
                renderer,
                octorok_texture,
                enemy_sprite_decoder,
                octorok_runtime,
                current_actors,
                rooms,
                current_player,
                true,
                render_camera,
                output_width,
                output_height);
            render_octorok_projectiles(
                renderer,
                projectile_texture,
                octorok_projectile_frame,
                octorok_runtime,
                current_actors,
                rooms,
                current_player,
                true,
                render_camera,
                output_width,
                output_height);
            render_vasu_actors(
                renderer,
                vasu_textures,
                interaction_sprite_decoder,
                current_actors,
                rooms,
                current_player,
                true,
                logic_tick,
                render_camera,
                output_width,
                output_height);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 8, 10, 18, 210);
        const SDL_FRect panel{12.0f, 12.0f, 620.0f, 98.0f};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 232, 238, 248, SDL_ALPHA_OPAQUE);
        const std::string line_one = player_mode
            ? "ROM runtime | WASD/arrows: move | Z/Enter: talk | X: sword/back"
            : "Authentic ROM pixels | WASD/arrows: pan | wheel: zoom";
        const std::string line_two =
            diagnostic
            ? "F1: authentic view | R: reset | mode: diagnostic"
            : "F1: diagnostic view | R: reset | mode: authentic";
        std::ostringstream diagnostic_line;
        diagnostic_line
            << (collision_overlay
                    ? "F2: hide collisions"
                    : "F2: show collisions")
            << " | "
            << (object_overlay
                    ? "F3: hide object anchors"
                    : "F3: show object anchors")
            << " | actors " << actor_load_report.spawned.size()
            << " parts "
            << current_actors.active_count(
                   oracle::core::ActorCategory::part)
            << " | HP "
            << static_cast<unsigned int>(player_combat.health)
            << '/'
            << static_cast<unsigned int>(player_combat.maximum_health);
        if (
            last_combat_step.enemies_hit != 0 ||
            last_combat_step.enemies_defeated != 0 ||
            last_combat_step.projectiles_requested != 0) {
            diagnostic_line
                << " | combat "
                << static_cast<unsigned int>(
                       last_combat_step.enemies_hit)
                << " hit, "
                << static_cast<unsigned int>(
                       last_combat_step.enemies_defeated)
                << " defeated, "
                << static_cast<unsigned int>(
                       last_combat_step.projectiles_requested)
                << " shot";
        }
        if (!actor_load_report.failures.empty()) {
            diagnostic_line
                << " (" << actor_load_report.failures.size()
                << " slot failures)";
        }
        std::ostringstream status_line;
        if (player_mode) {
            status_line
                << "room " << std::hex << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned int>(current_player.room.area)
                << ':' << std::setw(2)
                << static_cast<unsigned int>(current_player.room.room)
                << "  yx " << std::setw(2)
                << static_cast<unsigned int>(
                       oracle::gameplay::PlayerTraversal::
                           packed_room_position(current_player));
            if (last_warp.has_value()) {
                status_line
                    << "  last warp "
                    << exit_kind_name(last_warp->kind)
                    << " -> " << std::setw(2)
                    << static_cast<unsigned int>(
                           last_warp->destination.area)
                    << ':' << std::setw(2)
                    << static_cast<unsigned int>(
                           last_warp->destination.room);
            }
        }
        SDL_RenderDebugText(renderer, 22.0f, 23.0f, line_one.c_str());
        SDL_RenderDebugText(renderer, 22.0f, 43.0f, line_two.c_str());
        const auto line_three = diagnostic_line.str();
        SDL_RenderDebugText(renderer, 22.0f, 63.0f, line_three.c_str());
        if (player_mode) {
            const auto text = status_line.str();
            SDL_RenderDebugText(renderer, 22.0f, 83.0f, text.c_str());
        }
        render_dialogue(
            renderer,
            vasu_runtime.model(),
            output_width,
            output_height);
        if (
            screenshot_path.has_value() &&
            logic_tick >= screenshot_ready_tick) {
            SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
            if (pixels == nullptr) {
                throw std::runtime_error{
                    std::string{"screenshot readback failed: "} +
                    SDL_GetError()};
            }
            const auto path_string = screenshot_path->string();
            const bool saved = SDL_SaveBMP(pixels, path_string.c_str());
            SDL_DestroySurface(pixels);
            if (!saved) {
                throw std::runtime_error{
                    std::string{"screenshot save failed: "} + SDL_GetError()};
            }
            std::cout << "screenshot=" << path_string << '\n';
            screenshot_path.reset();
            running = false;
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(link_texture);
    SDL_DestroyTexture(sword_texture);
    SDL_DestroyTexture(octorok_texture);
    SDL_DestroyTexture(projectile_texture);
    for (auto* texture : vasu_textures) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyTexture(region_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr
                << "Usage: oracle_room_slice <US ROM path> "
                   "[--group HEX] [--room HEX] [--season NAME] "
                   "[--atlas] [--export-atlas PATH] "
                   "[--export-region PATH] [--diagnostic] "
                   "[--collisions] [--objects] "
                   "[--list-exits] [--follow-exit N] "
                   "[--catalog-topology] [--catalog-objects] "
                   "[--explore] [--vasu-scenario] "
                   "[--octorok-scenario] "
                   "[--spawn-yx HEX] "
                   "[--tick N] [--room-flags HEX] "
                   "[--describe] [--screenshot PATH]\n";
            return EXIT_FAILURE;
        }

        const std::filesystem::path rom_path{argv[1]};
        std::uint8_t world_group = 0;
        std::uint8_t center_room = 0x91;
        bool describe_only = false;
        bool force_diagnostic = false;
        bool force_collision_overlay = false;
        bool force_object_overlay = false;
        bool atlas_mode = false;
        bool catalog_topology = false;
        bool catalog_objects = false;
        bool vasu_scenario_mode = false;
        bool octorok_scenario_mode = false;
        bool manual_world_selection = false;
        std::optional<std::size_t> follow_exit_index;
        std::optional<std::uint8_t> spawn_position;
        auto season = oracle::content::Season::spring;
        std::uint64_t animation_tick = 0;
        std::uint8_t room_flags = 0;
        std::optional<std::filesystem::path> region_output_path;
        std::optional<std::filesystem::path> screenshot_path;
        for (int index = 2; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "--describe") {
                describe_only = true;
                manual_world_selection = true;
            } else if (argument == "--atlas") {
                atlas_mode = true;
                manual_world_selection = true;
            } else if (argument == "--explore") {
                manual_world_selection = true;
            } else if (argument == "--diagnostic") {
                force_diagnostic = true;
            } else if (argument == "--collisions") {
                force_collision_overlay = true;
            } else if (argument == "--objects") {
                force_object_overlay = true;
            } else if (argument == "--list-exits") {
                describe_only = true;
                manual_world_selection = true;
            } else if (argument == "--catalog-topology") {
                catalog_topology = true;
                describe_only = true;
                manual_world_selection = true;
            } else if (argument == "--catalog-objects") {
                catalog_objects = true;
                describe_only = true;
                manual_world_selection = true;
            } else if (argument == "--vasu-scenario") {
                vasu_scenario_mode = true;
            } else if (argument == "--octorok-scenario") {
                octorok_scenario_mode = true;
            } else if (
                argument == "--follow-exit" &&
                index + 1 < argc) {
                manual_world_selection = true;
                follow_exit_index = static_cast<std::size_t>(
                    parse_unsigned_integer(argv[++index]));
            } else if (
                argument == "--spawn-yx" &&
                index + 1 < argc) {
                manual_world_selection = true;
                spawn_position = parse_hex_byte(argv[++index]);
            } else if (argument == "--group" && index + 1 < argc) {
                manual_world_selection = true;
                world_group = parse_hex_byte(argv[++index]);
            } else if (argument == "--room" && index + 1 < argc) {
                manual_world_selection = true;
                center_room = parse_hex_byte(argv[++index]);
            } else if (argument == "--season" && index + 1 < argc) {
                manual_world_selection = true;
                season = parse_season(argv[++index]);
            } else if (argument == "--tick" && index + 1 < argc) {
                animation_tick = parse_unsigned_integer(argv[++index]);
            } else if (
                argument == "--room-flags" &&
                index + 1 < argc) {
                room_flags = parse_hex_byte(argv[++index]);
            } else if (
                argument == "--export-atlas" &&
                index + 1 < argc) {
                manual_world_selection = true;
                atlas_mode = true;
                region_output_path =
                    std::filesystem::path{argv[++index]};
            } else if (
                argument == "--export-region" &&
                index + 1 < argc) {
                manual_world_selection = true;
                region_output_path =
                    std::filesystem::path{argv[++index]};
            } else if (argument == "--screenshot" && index + 1 < argc) {
                screenshot_path = std::filesystem::path{argv[++index]};
            } else {
                throw std::invalid_argument{
                    "unknown or incomplete command-line argument"};
            }
        }
        if (
            !manual_world_selection &&
            !vasu_scenario_mode &&
            !octorok_scenario_mode) {
            octorok_scenario_mode = true;
        }
        if (world_group >= 8) {
            throw std::invalid_argument{
                "world group must be between 0 and 7"};
        }
        if (region_output_path.has_value() && force_diagnostic) {
            throw std::invalid_argument{
                "region export requires authentic ROM rendering"};
        }

        const auto rom = oracle::content::RomSource::load(rom_path);
        if (vasu_scenario_mode) {
            const auto scenario =
                oracle::gameplay::vasu_scenario(
                    rom.metadata().campaign);
            world_group =
                static_cast<std::uint8_t>(scenario.room.area);
            center_room =
                static_cast<std::uint8_t>(scenario.room.room);
            spawn_position = scenario.player_spawn_yx;
            force_object_overlay = false;
        }
        if (octorok_scenario_mode) {
            const auto scenario =
                oracle::gameplay::octorok_scenario(
                    rom.metadata().campaign);
            world_group =
                static_cast<std::uint8_t>(scenario.room.area);
            center_room =
                static_cast<std::uint8_t>(scenario.room.room);
            spawn_position = scenario.player_spawn_yx;
            force_object_overlay = false;
        }
        const oracle::content::RoomLayoutDecoder layout_decoder{rom};
        const oracle::content::RoomPixelDecoder pixel_decoder{rom};
        const oracle::content::RoomCollisionDecoder collision_decoder{rom};
        const oracle::content::RoomMutationDecoder mutation_decoder{rom};
        const oracle::content::RoomObjectDecoder object_decoder{rom};
        const oracle::content::RoomTopologyDecoder topology_decoder{rom};
        const auto destination_variant =
            static_cast<std::uint8_t>(season);
        if (catalog_topology) {
            print_topology_catalog(
                topology_decoder,
                destination_variant);
        }
        if (catalog_objects) {
            print_object_catalog(object_decoder, room_flags);
        }
        if (follow_exit_index.has_value()) {
            const auto source_exits =
                topology_decoder.exits(
                    world_group,
                    center_room,
                    destination_variant);
            if (*follow_exit_index >= source_exits.size()) {
                throw std::out_of_range{
                    "follow-exit index exceeds the room exit list"};
            }
            const auto& followed = source_exits[*follow_exit_index];
            std::cout
                << "followed_exit=" << *follow_exit_index << '\n'
                << "followed_kind=" << exit_kind_name(followed.kind) << '\n'
                << "followed_source=" << std::hex << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned int>(world_group)
                << ':' << std::setw(2)
                << static_cast<unsigned int>(center_room) << '\n'
                << "followed_destination=" << std::setw(2)
                << static_cast<unsigned int>(followed.destination.area)
                << ':' << std::setw(2)
                << static_cast<unsigned int>(followed.destination.room)
                << std::dec << '\n';
            world_group =
                static_cast<std::uint8_t>(followed.destination.area);
            center_room =
                static_cast<std::uint8_t>(followed.destination.room);
        }
        const auto room_exits =
            topology_decoder.exits(
                world_group,
                center_room,
                destination_variant);
        const auto center_tileset =
            pixel_decoder.describe_tileset(
                world_group,
                center_room,
                season);
        const bool large_room_mode =
            layout_decoder.layout_kind(center_tileset.layout_group) ==
            oracle::content::RoomLayoutKind::large;
        if (atlas_mode && large_room_mode) {
            throw std::invalid_argument{
                "large-layout groups use individual rooms, not an atlas"};
        }
        const bool player_mode =
            !atlas_mode &&
            !describe_only &&
            !region_output_path.has_value();
        auto rooms = large_room_mode
            ? decode_world_room(
                  layout_decoder,
                  pixel_decoder,
                  mutation_decoder,
                  world_group,
                  center_room,
                  season,
                  room_flags)
            : atlas_mode || player_mode
            ? decode_world_rectangle(
                  layout_decoder,
                  pixel_decoder,
                  mutation_decoder,
                  world_group,
                  0,
                  15,
                  0,
                  15,
                  season,
                  room_flags)
            : decode_world_neighborhood(
                  layout_decoder,
                  pixel_decoder,
                  mutation_decoder,
                  world_group,
                  center_room,
                  1,
                  season,
                  room_flags);
        std::vector<oracle::content::RoomCollisionMap> collisions;
        collisions.reserve(rooms.size());
        for (const auto& placement : rooms) {
            const auto tileset = pixel_decoder.describe_tileset(
                static_cast<std::uint8_t>(placement.layout.id.area),
                static_cast<std::uint8_t>(placement.layout.id.room),
                season);
            collisions.push_back(
                collision_decoder.decode(placement.layout, tileset));
        }
        std::optional<RegionPixels> authentic_region;
        if (!force_diagnostic) {
            try {
                authentic_region =
                    compose_region(
                        pixel_decoder,
                        rooms,
                        season,
                        animation_tick);
            } catch (const std::exception& error) {
                std::cerr
                    << "authentic renderer unavailable; using diagnostic "
                       "fallback: "
                    << error.what() << '\n';
            }
        }
        print_description(
            rom,
            rooms,
            world_group,
            center_room,
            atlas_mode,
            large_room_mode,
            player_mode,
            collisions,
            authentic_region ? &*authentic_region : nullptr,
            animation_tick,
            room_flags);
        print_room_exits(room_exits);
        const auto room_objects =
            object_decoder.decode(
                world_group,
                center_room,
                room_flags);
        const auto positioned_object_count =
            std::count_if(
                room_objects.records.begin(),
                room_objects.records.end(),
                [](const oracle::content::RoomObjectRecord& object) {
                    return object.positioned;
                });
        std::cout
            << "room_object_count="
            << room_objects.records.size() << '\n'
            << "positioned_object_count="
            << positioned_object_count << '\n';
        if (spawn_position.has_value()) {
            const auto room = std::find_if(
                rooms.begin(),
                rooms.end(),
                [center_room](const RoomPlacement& placement) {
                    return placement.layout.id.room == center_room;
                });
            const auto collision = std::find_if(
                collisions.begin(),
                collisions.end(),
                [center_room](
                    const oracle::content::RoomCollisionMap& map) {
                    return map.id.room == center_room;
                });
            const auto column =
                static_cast<std::size_t>(*spawn_position & 0x0f);
            const auto packed_row =
                static_cast<std::size_t>(*spawn_position >> 4u);
            const auto row = packed_row == 0 ? 0 : packed_row - 1;
            if (
                room != rooms.end() &&
                collision != collisions.end() &&
                column < room->layout.columns &&
                row < room->layout.rows) {
                const auto metatile =
                    room->layout.metatiles[
                        row * room->layout.columns + column];
                const auto tileset = pixel_decoder.describe_tileset(
                    world_group,
                    center_room,
                    season);
                const auto warp_property =
                    topology_decoder.warp_tile_property(
                        tileset.collision_mode,
                        metatile);
                std::cout
                    << "spawn_metatile=" << std::hex << std::setw(2)
                    << std::setfill('0')
                    << static_cast<unsigned int>(metatile) << '\n'
                    << "spawn_collision=" << std::setw(2)
                    << static_cast<unsigned int>(
                           collision->at(column, row))
                    << '\n'
                    << "spawn_warp_property=";
                if (warp_property.has_value()) {
                    std::cout
                        << std::setw(2)
                        << static_cast<unsigned int>(*warp_property);
                } else {
                    std::cout << "none";
                }
                std::cout << std::dec << '\n';
                std::cout << "room_warp_tiles=";
                bool first = true;
                for (std::size_t tile_row = 0;
                     tile_row < room->layout.rows;
                     ++tile_row) {
                    for (std::size_t tile_column = 0;
                         tile_column < room->layout.columns;
                         ++tile_column) {
                        const auto candidate =
                            room->layout.metatiles[
                                tile_row * room->layout.columns +
                                tile_column];
                        if (
                            !topology_decoder.warp_tile_property(
                                 tileset.collision_mode,
                                 candidate)
                                 .has_value()) {
                            continue;
                        }
                        if (!first) {
                            std::cout << ',';
                        }
                        first = false;
                        std::cout
                            << std::hex << std::setw(2)
                            << std::setfill('0')
                            << static_cast<unsigned int>(
                                   ((tile_row + 1) << 4u) |
                                   tile_column);
                    }
                }
                if (first) {
                    std::cout << "none";
                }
                std::cout << std::dec << '\n';
                const auto resolved =
                    topology_decoder.resolve_tile_warp(
                        world_group,
                        center_room,
                        *spawn_position,
                        metatile,
                        tileset.collision_mode,
                        destination_variant);
                std::cout << "spawn_resolved_warp=";
                if (resolved.has_value()) {
                    std::cout
                        << std::hex << std::setw(2)
                        << std::setfill('0')
                        << static_cast<unsigned int>(
                               resolved->destination.area)
                        << ':' << std::setw(2)
                        << static_cast<unsigned int>(
                               resolved->destination.room);
                } else {
                    std::cout << "none";
                }
                std::cout << std::dec << '\n';
            }
        }
        if (region_output_path.has_value()) {
            if (!authentic_region.has_value()) {
                throw std::runtime_error{
                    "region export has no authentic rendered pixels"};
            }
            save_region_bmp(*authentic_region, *region_output_path);
            return EXIT_SUCCESS;
        }
        if (describe_only) {
            return EXIT_SUCCESS;
        }
        return run_window(
            rom,
            std::move(rooms),
            std::move(collisions),
            std::move(authentic_region),
            layout_decoder,
            pixel_decoder,
            mutation_decoder,
            collision_decoder,
            object_decoder,
            topology_decoder,
            season,
            center_room,
            atlas_mode,
            large_room_mode,
            player_mode,
            force_diagnostic,
            force_collision_overlay,
            force_object_overlay,
            room_flags,
            animation_tick,
            screenshot_path,
            spawn_position);
    } catch (const std::exception& error) {
        std::cerr << "oracle_room_slice: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
