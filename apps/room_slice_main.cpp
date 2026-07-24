#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
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

#include "oracle/content/rom_source.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_pixels.h"
#include "oracle/core/campaign.h"
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
        throw std::invalid_argument{"room must be a hexadecimal byte"};
    }
    return static_cast<std::uint8_t>(value);
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
             row < oracle::content::small_room_rows;
             ++row) {
            for (std::size_t column = 0;
                 column < oracle::content::small_room_columns;
                 ++column) {
                const auto metatile =
                    placement.layout.metatiles[
                        row * oracle::content::small_room_columns + column];
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
                oracle::content::small_room_world_width * camera.zoom),
            .h = static_cast<float>(
                oracle::content::small_room_world_height * camera.zoom),
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
                oracle::content::small_room_world_width * camera.zoom),
            .h = static_cast<float>(
                oracle::content::small_room_world_height * camera.zoom),
        };
        SDL_RenderRect(renderer, &border);
    }
}

RegionPixels compose_region(
    const oracle::content::RoomPixelDecoder& decoder,
    const std::vector<RoomPlacement>& placements,
    const oracle::content::Season season) {
    if (placements.empty()) {
        throw std::invalid_argument{"cannot compose an empty room region"};
    }
    const auto x_bounds = std::minmax_element(
        placements.begin(),
        placements.end(),
        [](const RoomPlacement& left, const RoomPlacement& right) {
            return left.world_x < right.world_x;
        });
    const auto y_bounds = std::minmax_element(
        placements.begin(),
        placements.end(),
        [](const RoomPlacement& left, const RoomPlacement& right) {
            return left.world_y < right.world_y;
        });
    RegionPixels region{
        .world_x = x_bounds.first->world_x,
        .world_y = y_bounds.first->world_y,
        .width =
            x_bounds.second->world_x - x_bounds.first->world_x +
            oracle::content::small_room_world_width,
        .height =
            y_bounds.second->world_y - y_bounds.first->world_y +
            oracle::content::small_room_world_height,
    };
    region.pixels.resize(
        static_cast<std::size_t>(region.width * region.height));
    region.rooms.reserve(placements.size());

    for (const auto& placement : placements) {
        auto rendered = decoder.render(placement.layout, season);
        const auto local_x = placement.world_x - region.world_x;
        const auto local_y = placement.world_y - region.world_y;
        for (std::int32_t y = 0;
             y < oracle::content::small_room_world_height;
             ++y) {
            const auto source =
                rendered.pixels.begin() +
                static_cast<std::ptrdiff_t>(
                    y * oracle::content::small_room_world_width);
            const auto destination =
                region.pixels.begin() +
                static_cast<std::ptrdiff_t>(
                    (local_y + y) * region.width + local_x);
            std::copy_n(
                source,
                oracle::content::small_room_world_width,
                destination);
        }
        region.rooms.push_back(std::move(rendered));
    }
    return region;
}

std::vector<RoomPlacement> decode_world_neighborhood(
    const oracle::content::RoomLayoutDecoder& layout_decoder,
    const oracle::content::RoomPixelDecoder& pixel_decoder,
    const std::uint8_t world_group,
    const std::uint8_t center_room,
    const std::uint8_t radius,
    const oracle::content::Season season) {
    const auto center_x = static_cast<int>(center_room & 0x0f);
    const auto center_y = static_cast<int>(center_room >> 4u);
    const auto extent = static_cast<int>(radius);
    std::vector<RoomPlacement> rooms;
    rooms.reserve(
        static_cast<std::size_t>((extent * 2 + 1) * (extent * 2 + 1)));
    for (int y = center_y - extent; y <= center_y + extent; ++y) {
        if (y < 0 || y > 15) {
            continue;
        }
        for (int x = center_x - extent; x <= center_x + extent; ++x) {
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
            rooms.push_back(RoomPlacement{
                .layout = layout_decoder.decode_small_room(
                    world_group,
                    tileset.layout_group,
                    room),
                .world_x =
                    x * oracle::content::small_room_world_width,
                .world_y =
                    y * oracle::content::small_room_world_height,
            });
        }
    }
    return rooms;
}

std::string campaign_name(const oracle::core::Campaign campaign) {
    return campaign == oracle::core::Campaign::ages ? "Ages" : "Seasons";
}

void print_description(
    const oracle::content::RomSource& rom,
    const std::vector<RoomPlacement>& rooms,
    const std::uint8_t center_room,
    const RegionPixels* authentic_region) {
    std::unordered_set<std::uint8_t> metatiles;
    for (const auto& room : rooms) {
        metatiles.insert(
            room.layout.metatiles.begin(),
            room.layout.metatiles.end());
    }
    std::cout
        << "campaign=" << campaign_name(rom.metadata().campaign) << '\n'
        << "game_code=" << rom.metadata().game_code << '\n'
        << "fingerprint=" << std::hex << std::setw(16)
        << std::setfill('0') << rom.metadata().compatibility_fingerprint
        << '\n'
        << "center_room=" << std::setw(2)
        << static_cast<unsigned int>(center_room) << '\n'
        << std::dec
        << "decoded_rooms=" << rooms.size() << '\n'
        << "unique_metatiles=" << metatiles.size() << '\n'
        << "render_mode="
        << (authentic_region != nullptr ? "authentic-rom" : "diagnostic")
        << '\n';
    if (authentic_region != nullptr) {
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

int run_window(
    const oracle::content::RomSource& rom,
    const std::vector<RoomPlacement>& rooms,
    const RegionPixels* authentic_region,
    const std::uint8_t center_room,
    const bool force_diagnostic,
    std::optional<std::filesystem::path> screenshot_path) {
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

    SDL_Texture* region_texture = nullptr;
    if (authentic_region != nullptr) {
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

    const auto center_x = static_cast<double>(
        (center_room & 0x0f) * oracle::content::small_room_world_width +
        oracle::content::small_room_world_width / 2);
    const auto center_y = static_cast<double>(
        (center_room >> 4u) * oracle::content::small_room_world_height +
        oracle::content::small_room_world_height / 2);
    CameraState previous{.x = center_x, .y = center_y, .zoom = 3.0};
    CameraState current = previous;

    constexpr double logic_step = 1.0 / 60.0;
    double accumulator = 0.0;
    std::uint64_t logic_tick = 0;
    auto last_counter = SDL_GetPerformanceCounter();
    const auto counter_frequency =
        static_cast<double>(SDL_GetPerformanceFrequency());
    bool running = true;
    bool diagnostic =
        force_diagnostic || authentic_region == nullptr;

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_R) {
                previous = CameraState{
                    .x = center_x,
                    .y = center_y,
                    .zoom = 3.0,
                };
                current = previous;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_F1 &&
                authentic_region != nullptr) {
                diagnostic = !diagnostic;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                current.zoom = std::clamp(
                    current.zoom * std::pow(1.15, event.wheel.y),
                    0.35,
                    8.0);
            }
        }

        const auto now = SDL_GetPerformanceCounter();
        const auto elapsed = std::min(
            0.25,
            static_cast<double>(now - last_counter) / counter_frequency);
        last_counter = now;
        accumulator += elapsed;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        while (accumulator >= logic_step) {
            previous = current;
            const auto pan_speed = 150.0 / current.zoom;
            const auto horizontal =
                (keyboard[SDL_SCANCODE_D] ||
                 keyboard[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0) -
                (keyboard[SDL_SCANCODE_A] ||
                 keyboard[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);
            const auto vertical =
                (keyboard[SDL_SCANCODE_S] ||
                 keyboard[SDL_SCANCODE_DOWN] ? 1.0 : 0.0) -
                (keyboard[SDL_SCANCODE_W] ||
                 keyboard[SDL_SCANCODE_UP] ? 1.0 : 0.0);
            current.x += horizontal * pan_speed * logic_step;
            current.y += vertical * pan_speed * logic_step;
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
                center_room);
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
                center_room);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 8, 10, 18, 210);
        const SDL_FRect panel{12.0f, 12.0f, 460.0f, 58.0f};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 232, 238, 248, SDL_ALPHA_OPAQUE);
        const std::string line_one =
            "Authentic ROM pixels | WASD/arrows: pan | wheel: zoom";
        const std::string line_two =
            diagnostic
            ? "F1: authentic view | R: reset | mode: diagnostic"
            : "F1: diagnostic view | R: reset | mode: authentic";
        SDL_RenderDebugText(renderer, 22.0f, 23.0f, line_one.c_str());
        SDL_RenderDebugText(renderer, 22.0f, 43.0f, line_two.c_str());
        if (screenshot_path.has_value()) {
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
                   "[--room HEX] [--season NAME] [--diagnostic] "
                   "[--describe] [--screenshot PATH]\n";
            return EXIT_FAILURE;
        }

        const std::filesystem::path rom_path{argv[1]};
        std::uint8_t center_room = 0x91;
        bool describe_only = false;
        bool force_diagnostic = false;
        auto season = oracle::content::Season::spring;
        std::optional<std::filesystem::path> screenshot_path;
        for (int index = 2; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "--describe") {
                describe_only = true;
            } else if (argument == "--diagnostic") {
                force_diagnostic = true;
            } else if (argument == "--room" && index + 1 < argc) {
                center_room = parse_hex_byte(argv[++index]);
            } else if (argument == "--season" && index + 1 < argc) {
                season = parse_season(argv[++index]);
            } else if (argument == "--screenshot" && index + 1 < argc) {
                screenshot_path = std::filesystem::path{argv[++index]};
            } else {
                throw std::invalid_argument{
                    "unknown or incomplete command-line argument"};
            }
        }

        const auto rom = oracle::content::RomSource::load(rom_path);
        const oracle::content::RoomLayoutDecoder layout_decoder{rom};
        const oracle::content::RoomPixelDecoder pixel_decoder{rom};
        const auto rooms = decode_world_neighborhood(
            layout_decoder,
            pixel_decoder,
            0,
            center_room,
            1,
            season);
        std::optional<RegionPixels> authentic_region;
        if (!force_diagnostic) {
            try {
                authentic_region =
                    compose_region(pixel_decoder, rooms, season);
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
            center_room,
            authentic_region ? &*authentic_region : nullptr);
        if (describe_only) {
            return EXIT_SUCCESS;
        }
        return run_window(
            rom,
            rooms,
            authentic_region ? &*authentic_region : nullptr,
            center_room,
            force_diagnostic,
            screenshot_path);
    } catch (const std::exception& error) {
        std::cerr << "oracle_room_slice: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
