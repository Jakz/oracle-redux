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

void render_region(
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

std::string campaign_name(const oracle::core::Campaign campaign) {
    return campaign == oracle::core::Campaign::ages ? "Ages" : "Seasons";
}

void print_description(
    const oracle::content::RomSource& rom,
    const std::vector<RoomPlacement>& rooms,
    const std::uint8_t center_room) {
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
        << "unique_metatiles=" << metatiles.size() << '\n';
}

int run_window(
    const oracle::content::RomSource& rom,
    const std::vector<RoomPlacement>& rooms,
    const std::uint8_t center_room,
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
        render_region(
            renderer,
            rooms,
            rom.metadata().campaign,
            render_camera,
            output_width,
            output_height,
            center_room);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 8, 10, 18, 210);
        const SDL_FRect panel{12.0f, 12.0f, 460.0f, 58.0f};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 232, 238, 248, SDL_ALPHA_OPAQUE);
        const std::string line_one =
            "ROM-derived 3x3 room region | WASD/arrows: pan | wheel: zoom";
        const std::string line_two =
            "R: reset | gold border: selected room | renderer: SDL3 GPU";
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
                   "[--room HEX] [--describe] [--screenshot PATH]\n";
            return EXIT_FAILURE;
        }

        const std::filesystem::path rom_path{argv[1]};
        std::uint8_t center_room = 0x91;
        bool describe_only = false;
        std::optional<std::filesystem::path> screenshot_path;
        for (int index = 2; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "--describe") {
                describe_only = true;
            } else if (argument == "--room" && index + 1 < argc) {
                center_room = parse_hex_byte(argv[++index]);
            } else if (argument == "--screenshot" && index + 1 < argc) {
                screenshot_path = std::filesystem::path{argv[++index]};
            } else {
                throw std::invalid_argument{
                    "unknown or incomplete command-line argument"};
            }
        }

        const auto rom = oracle::content::RomSource::load(rom_path);
        const oracle::content::RoomLayoutDecoder decoder{rom};
        const auto rooms = decoder.decode_neighborhood(0, center_room, 1);
        print_description(rom, rooms, center_room);
        if (describe_only) {
            return EXIT_SUCCESS;
        }
        return run_window(rom, rooms, center_room, screenshot_path);
    } catch (const std::exception& error) {
        std::cerr << "oracle_room_slice: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
