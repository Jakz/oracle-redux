#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "oracle/core/simulation_region.h"
#include "oracle/content/room_collisions.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_mutations.h"
#include "oracle/content/room_pixels.h"
#include "oracle/experience_settings.h"
#include "oracle/core/item_campaign_policy.h"
#include "oracle/core/item_runtime.h"
#include "oracle/presentation/camera.h"
#include "oracle/presentation/frame_timing.h"
#include "oracle/presentation/render_plan.h"
#include "oracle/presentation/world_scene.h"

namespace {

int failures = 0;

void check(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void check_close(
    const double actual,
    const double expected,
    const std::string_view message) {
    check(std::abs(actual - expected) < 0.000001, message);
}

void test_item_primitives() {
    using oracle::core::ItemRuntime;
    using oracle::core::ItemState;
    using oracle::core::LinkState;

    ItemState item{
        .health = 5,
        .damage_to_apply = 0xfd,
        .var2a = 0,
        .var3c = 0x12,
        .knockback_angle = 7,
        .knockback_counter = 3,
    };
    ItemRuntime::set_var3c_to_ff(item);
    check(item.var3c == 0xff, "itemSetVar3cToFF");

    const auto damage_result = ItemRuntime::update_damage_to_apply(item);
    check(item.damage_to_apply == 0, "damageToApply clears");
    check(item.health == 2, "item health wraps as an 8-bit value");
    check(damage_result.zero, "var2a zero flag");
    check(!damage_result.health_negative, "positive resulting health");

    item.health = 1;
    item.damage_to_apply = 0xfe;
    item.var2a = 3;
    const auto underflow_result = ItemRuntime::update_damage_to_apply(item);
    check(item.health == 0xff, "negative item health wraps to FF");
    check(
        underflow_result.health_negative,
        "negative resulting health is reported");
    check(!underflow_result.zero, "nonzero var2a clears zero result");

    LinkState link{.knockback_angle = 1, .knockback_counter = 4};
    check(
        ItemRuntime::transfer_knockback_to_link(item, link),
        "nonzero item knockback transfers");
    check(item.knockback_counter == 0, "item knockback counter clears");
    check(link.knockback_counter == 4, "stronger Link counter is retained");
    check(link.knockback_angle == 7, "item angle always transfers");

    check(
        !ItemRuntime::transfer_knockback_to_link(item, link),
        "zero item knockback does nothing");
}

void test_campaign_policy() {
    using oracle::core::Campaign;
    using oracle::core::ItemCampaignPolicy;
    using oracle::core::ParentItemContext;

    const ItemCampaignPolicy ages{Campaign::ages};
    const ItemCampaignPolicy seasons{Campaign::seasons};
    const ParentItemContext underwater{.link_is_underwater = true};

    check(
        !ages.is_link_considered_grounded(underwater),
        "Ages underwater Link is not grounded");
    check(
        seasons.is_link_considered_grounded(underwater),
        "Seasons has no underwater-map grounding exception");
    check(
        ages.select_parent_item_animation(0x22, underwater) == 0x2d,
        "Ages underwater parent animation replacement");
    check(
        seasons.select_parent_item_animation(0x22, underwater) == 0x22,
        "Seasons retains the base parent animation");

    const ParentItemContext raft{
        .link_is_riding = true,
        .companion_is_raft = true,
    };
    check(
        ages.select_parent_item_animation(0x22, raft) == 0x22,
        "Ages raft bypasses mounted animation offset");
    check(
        seasons.select_parent_item_animation(0x22, raft) == 0x26,
        "Seasons applies ordinary mounted animation offset");
}

void test_presentation_camera() {
    using oracle::presentation::CameraMode;
    using oracle::presentation::PixelSize;
    using oracle::presentation::PresentationCamera;
    using oracle::presentation::WorldPoint;
    using oracle::presentation::WorldScene;
    using oracle::presentation::WorldTile;

    const auto fidelity = PresentationCamera::fidelity();
    const auto original = fidelity.visible_world_rect();
    check_close(original.width, 160.0, "fidelity camera width");
    check_close(original.height, 144.0, "fidelity camera height");

    const PresentationCamera widescreen{
        CameraMode::widescreen,
        PixelSize{.width = 1280, .height = 720},
        WorldPoint{.x = 500.0, .y = 300.0},
        4.0,
    };
    const auto wide_rect = widescreen.visible_world_rect();
    check_close(wide_rect.width, 320.0, "widescreen world width");
    check_close(wide_rect.height, 180.0, "widescreen world height");

    const PresentationCamera overview{
        CameraMode::world_overview,
        PixelSize{.width = 1280, .height = 720},
        WorldPoint{.x = 500.0, .y = 300.0},
        0.5,
    };
    check(
        overview.visible_world_rect().width > wide_rect.width,
        "World Overview exposes more world without changing simulation");

    bool rejected_zero_zoom = false;
    try {
        static_cast<void>(PresentationCamera{
            CameraMode::widescreen,
            PixelSize{.width = 1280, .height = 720},
            WorldPoint{},
            0.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_zero_zoom = true;
    }
    check(rejected_zero_zoom, "camera rejects zero zoom");

    WorldScene scene;
    scene.tiles.push_back(WorldTile{
        .area = 1,
        .room = 2,
        .world_x = 160,
        .world_y = 128,
        .layer = 0,
        .tile = 42,
    });
    scene.tiles.push_back(WorldTile{
        .area = 1,
        .room = 3,
        .world_x = 320,
        .world_y = 128,
        .layer = 0,
        .tile = 43,
    });
    check(
        scene.tiles.size() == 2,
        "presentation scene can contain multiple cached rooms");
}

void test_experience_profiles() {
    using oracle::ExperiencePreset;
    using oracle::ExperienceSettings;
    using oracle::SimulationRegionMode;
    using oracle::core::ActiveSimulationRegion;
    using oracle::core::WorldRoomId;
    using oracle::presentation::FrameTiming;
    using oracle::presentation::RenderPass;
    using oracle::presentation::build_render_plan;

    const auto classic =
        ExperienceSettings::from_preset(ExperiencePreset::classic);
    classic.validate();
    check(
        classic.gameplay.simulation_region_mode ==
            SimulationRegionMode::classic_room,
        "classic preset retains one-room simulation");
    check(
        classic.gameplay.item_action_slots == 2,
        "classic preset retains two item action slots");
    check(
        !classic.presentation.seamless_world,
        "classic preset retains visible room boundaries");
    const auto classic_passes = build_render_plan(classic.presentation);
    check(
        classic_passes.size() == 2 &&
            classic_passes.front() == RenderPass::world &&
            classic_passes.back() == RenderPass::interface,
        "classic render plan contains only world and interface passes");

    auto modern = ExperienceSettings::from_preset(ExperiencePreset::modern);
    modern.validate();
    check(
        modern.gameplay.simulation_region_mode ==
            SimulationRegionMode::seamless_region,
        "modern preset enables an explicit seamless simulation region");
    check(
        modern.gameplay.item_action_slots == 6,
        "modern preset supports six item action slots");
    check(
        modern.presentation.interpolate_between_logic_ticks,
        "modern preset enables smooth render interpolation");
    const auto modern_passes = build_render_plan(modern.presentation);
    check(
        modern_passes.size() == 5,
        "modern render plan includes optional visual effect passes");

    modern.presentation.atmospheric_fog = false;
    check(
        build_render_plan(modern.presentation).size() == 4,
        "modern preset capabilities remain independently configurable");

    const WorldRoomId room_a{.area = 1, .room = 10};
    const WorldRoomId room_b{.area = 1, .room = 11};
    const ActiveSimulationRegion classic_region{
        SimulationRegionMode::classic_room,
        room_a,
        {room_a},
    };
    check(
        !classic_region.contains(room_b),
        "classic simulation does not activate a visible neighbor");

    const ActiveSimulationRegion modern_region{
        SimulationRegionMode::seamless_region,
        room_a,
        {room_a, room_b},
    };
    check(
        modern_region.contains(room_b),
        "seamless simulation declares neighboring active rooms");

    const FrameTiming halfway{100, 101, 0.5};
    check_close(
        halfway.interpolate(20.0, 24.0),
        22.0,
        "render frames interpolate without advancing simulation");
}

void test_room_layout_decompression() {
    using oracle::content::RoomLayoutDecoder;
    using oracle::content::small_room_metatile_count;

    std::array<std::uint8_t, small_room_metatile_count> raw{};
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = static_cast<std::uint8_t>(index);
    }
    const auto uncompressed =
        RoomLayoutDecoder::decode_common_byte_layout(raw, 0);
    check(
        uncompressed == raw,
        "uncompressed ROM room layouts decode exactly");

    std::vector<std::uint8_t> mode_one;
    for (std::size_t chunk = 0; chunk < raw.size() / 8; ++chunk) {
        mode_one.push_back(0);
        for (std::size_t index = 0; index < 8; ++index) {
            mode_one.push_back(raw[chunk * 8 + index]);
        }
    }
    const auto literal_mode_one =
        RoomLayoutDecoder::decode_common_byte_layout(mode_one, 1);
    check(
        literal_mode_one == raw,
        "mode-one ROM room literals decode exactly");

    std::vector<std::uint8_t> repeated_mode_two;
    for (std::uint8_t chunk = 0; chunk < 5; ++chunk) {
        repeated_mode_two.push_back(0xff);
        repeated_mode_two.push_back(0xff);
        repeated_mode_two.push_back(chunk);
    }
    const auto repeated =
        RoomLayoutDecoder::decode_common_byte_layout(repeated_mode_two, 2);
    bool repetition_matches = true;
    for (std::size_t index = 0; index < repeated.size(); ++index) {
        repetition_matches =
            repetition_matches &&
            repeated[index] == static_cast<std::uint8_t>(index / 16);
    }
    check(
        repetition_matches,
        "mode-two ROM room repetitions decode exactly");

    const std::array<std::uint8_t, 8> dictionary{
        0x10,
        0x11,
        0x20,
        0x21,
        0x22,
        0x23,
        0x24,
        0x25,
    };
    const std::array<std::uint8_t, 4> dictionary_stream{
        0x02,
        0x99,
        0x02,
        0x20,
    };
    check(
        RoomLayoutDecoder::decode_dictionary_layout(
            dictionary_stream,
            dictionary,
            6) ==
            std::vector<std::uint8_t>{
                0x99,
                0x20,
                0x21,
                0x22,
                0x23,
                0x24,
            },
        "large-room dictionary references decode their packed length");
}

void test_graphics_decompression() {
    using oracle::content::RoomPixelDecoder;

    const std::array<std::uint8_t, 5> raw{1, 2, 3, 4, 5};
    check(
        RoomPixelDecoder::decompress_graphics(raw, raw.size(), 0) ==
            std::vector<std::uint8_t>{raw.begin(), raw.end()},
        "graphics mode zero copies raw cartridge bytes");

    const std::array<std::uint8_t, 5> mode_one_literals{
        0x00,
        0x11,
        0x22,
        0x33,
        0x44,
    };
    check(
        RoomPixelDecoder::decompress_graphics(
            mode_one_literals,
            4,
            1) ==
            std::vector<std::uint8_t>{0x11, 0x22, 0x33, 0x44},
        "graphics mode one decodes literal control bits");

    const std::array<std::uint8_t, 4> mode_one_reference{
        0x20,
        static_cast<std::uint8_t>('A'),
        static_cast<std::uint8_t>('B'),
        0x61,
    };
    check(
        RoomPixelDecoder::decompress_graphics(
            mode_one_reference,
            6,
            1) ==
            std::vector<std::uint8_t>{'A', 'B', 'A', 'B', 'A', 'B'},
        "graphics mode one expands overlapping back references");

    const std::array<std::uint8_t, 3> mode_two_repeated{
        0xff,
        0xff,
        0x7a,
    };
    check(
        RoomPixelDecoder::decompress_graphics(
            mode_two_repeated,
            16,
            2) ==
            std::vector<std::uint8_t>(16, 0x7a),
        "graphics mode two expands its repeated-byte mask");
}

void test_room_tile_replacements() {
    using oracle::content::RoomMutationDecoder;
    using oracle::content::TileReplacement;

    std::array<std::uint8_t, 6> metatiles{1, 2, 1, 3, 4, 1};
    const std::array<TileReplacement, 2> replacements{
        TileReplacement{.replacement = 4, .target = 1},
        TileReplacement{.replacement = 5, .target = 4},
    };
    RoomMutationDecoder::apply_replacements(
        metatiles,
        replacements);
    check(
        metatiles ==
            std::array<std::uint8_t, 6>{5, 2, 5, 3, 5, 5},
        "room substitutions replace every matching metatile in table order");
}

void test_room_collision_shapes() {
    using oracle::content::CollisionProfile;
    using oracle::content::RoomCollisionDecoder;

    check(
        RoomCollisionDecoder::is_solid(0x08, 1, 1),
        "simple collision bit 3 covers the top-left quadrant");
    check(
        !RoomCollisionDecoder::is_solid(0x08, 9, 1),
        "simple collision bit 3 excludes the top-right quadrant");
    check(
        RoomCollisionDecoder::is_solid(0x01, 15, 15),
        "simple collision bit 0 covers the bottom-right quadrant");

    check(
        !RoomCollisionDecoder::is_solid(
            0x10,
            8,
            8,
            CollisionProfile::link),
        "Link can enter a hole so its separate fall behavior can run");
    check(
        RoomCollisionDecoder::is_solid(
            0x10,
            8,
            8,
            CollisionProfile::grounded_actor),
        "grounded actors treat holes as fully solid");

    check(
        RoomCollisionDecoder::is_solid(0x11, 1, 8),
        "vertical bridge blocks its far-left two-pixel column");
    check(
        !RoomCollisionDecoder::is_solid(0x11, 7, 8),
        "vertical bridge leaves its center open");
    check(
        RoomCollisionDecoder::is_solid(0x19, 8, 1),
        "horizontal bridge blocks its top two-pixel row");
    check(
        !RoomCollisionDecoder::is_solid(0x19, 8, 7),
        "horizontal bridge leaves its center open");
}

}  // namespace

int main() {
    test_item_primitives();
    test_campaign_policy();
    test_presentation_camera();
    test_experience_profiles();
    test_room_layout_decompression();
    test_graphics_decompression();
    test_room_tile_replacements();
    test_room_collision_shapes();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "oracle_core_tests: OK\n";
    return EXIT_SUCCESS;
}
