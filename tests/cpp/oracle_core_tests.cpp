#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "oracle/content/interaction_sprite.h"
#include "oracle/content/link_sprite.h"
#include "oracle/content/part_data.h"
#include "oracle/content/part_sprite.h"
#include "oracle/content/enemy_data.h"
#include "oracle/content/enemy_sprite.h"
#include "oracle/content/rom_source.h"
#include "oracle/content/rom_text.h"
#include "oracle/content/room_objects.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/core/simulation_region.h"
#include "oracle/content/room_collisions.h"
#include "oracle/content/room_layout.h"
#include "oracle/content/room_mutations.h"
#include "oracle/content/room_pixels.h"
#include "oracle/content/room_topology.h"
#include "oracle/content/sword_sprite.h"
#include "oracle/experience_settings.h"
#include "oracle/gameplay/actor_collision.h"
#include "oracle/gameplay/interaction_target.h"
#include "oracle/gameplay/octorok_runtime.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/gameplay/room_actor_loader.h"
#include "oracle/gameplay/sword_runtime.h"
#include "oracle/gameplay/vasu_interaction.h"
#include "oracle/input/input_frame.h"
#include "oracle/core/item_campaign_policy.h"
#include "oracle/core/item_runtime.h"
#include "oracle/presentation/camera.h"
#include "oracle/presentation/frame_timing.h"
#include "oracle/presentation/render_plan.h"
#include "oracle/presentation/world_scene.h"
#include "oracle/script/campaign_script.h"
#include "oracle/script/original_state.h"

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

oracle::input::InputFrame pressed_frame(
    const oracle::input::InputAction action) {
    const auto bit = static_cast<std::uint16_t>(
        1u << static_cast<std::uint8_t>(action));
    return oracle::input::InputFrame{bit, bit, 0};
}

void test_semantic_input_frames() {
    using oracle::input::InputAction;
    using oracle::input::SemanticInputSampler;

    SemanticInputSampler sampler;
    sampler.set(InputAction::right, true);
    auto frame = sampler.sample();
    check(frame.held(InputAction::right), "input frame carries held actions");
    check(
        frame.pressed(InputAction::right),
        "input frame carries pressed edges");
    frame = sampler.sample();
    check(
        frame.held(InputAction::right) &&
            !frame.pressed(InputAction::right),
        "held input does not repeat its pressed edge");

    sampler.set(InputAction::a, true);
    sampler.set(InputAction::a, false);
    frame = sampler.sample();
    check(
        frame.pressed(InputAction::a) &&
            frame.released(InputAction::a) &&
            !frame.held(InputAction::a),
        "between-tick taps retain both semantic edges");

    sampler.release_all();
    frame = sampler.sample();
    check(
        frame.released(InputAction::right) &&
            !frame.held(InputAction::right),
        "backend reset synthesizes held-action releases");
}

void test_actor_slot_domain() {
    using oracle::core::ActorCategory;
    using oracle::core::ActorIdentity;
    using oracle::core::ActorSlotDomain;
    using oracle::core::WorldRoomId;

    ActorSlotDomain actors;
    const auto interaction = actors.allocate_dynamic(
        ActorCategory::interaction,
        ActorIdentity{0x89, 0, 0},
        WorldRoomId{2, 0xee},
        0x50,
        0x18,
        true,
        false,
        0);
    check(
        interaction.has_value() && interaction->slot == 2,
        "dynamic interactions begin after two reserved slots");
    const auto enemy = actors.allocate_dynamic(
        ActorCategory::enemy,
        ActorIdentity{1, 2, 3},
        WorldRoomId{},
        0,
        0,
        false,
        false,
        0);
    check(
        enemy.has_value() && enemy->slot == 0,
        "dynamic enemies begin at their first original slot");
    const auto item = actors.allocate_dynamic(
        ActorCategory::item,
        ActorIdentity{1, 0, 0},
        WorldRoomId{},
        0,
        0,
        false,
        false,
        0);
    check(
        item.has_value() && item->slot == 7,
        "dynamic items retain the original d7-db allocation range");

    const auto stale = *interaction;
    auto* interaction_state = actors.get(stale);
    check(
        interaction_state != nullptr,
        "allocated actor handle resolves before release");
    if (interaction_state != nullptr) {
        interaction_state->collision_radius_y = 18;
        interaction_state->collision_radius_x = 6;
        interaction_state->blocks_player = true;
    }
    check(actors.release(stale), "active actor slot releases");
    const auto replacement = actors.allocate_dynamic(
        ActorCategory::interaction,
        ActorIdentity{0x46, 0, 0},
        WorldRoomId{},
        0,
        0,
        false,
        false,
        1);
    check(
        replacement.has_value() &&
            replacement->slot == stale.slot &&
            replacement->generation != stale.generation &&
            actors.get(stale) == nullptr,
        "slot generations reject stale actor handles");
    const auto* replacement_state = actors.get(*replacement);
    check(
        replacement_state != nullptr &&
            replacement_state->collision_radius_y == 0 &&
            replacement_state->collision_radius_x == 0 &&
            replacement_state->maximum_health == 0 &&
            replacement_state->health == 0 &&
            replacement_state->contact_damage == 0 &&
            !replacement_state->blocks_player,
        "reused actor slots clear authoritative collision and combat state");
}

std::vector<std::uint8_t> test_vasu_rom_scenario(
    const std::filesystem::path& path,
    const oracle::core::Campaign campaign) {
    if (!std::filesystem::exists(path)) {
        return {};
    }
    using oracle::content::InteractionSpriteDecoder;
    using oracle::content::RomSource;
    using oracle::content::RomTextDecoder;
    using oracle::content::RoomObjectDecoder;
    using oracle::core::ActorCategory;
    using oracle::core::ActorSlotDomain;
    using oracle::gameplay::PlayerFacing;
    using oracle::gameplay::PlayerTraversal;
    using oracle::gameplay::RoomActorLoader;
    using oracle::gameplay::VasuInteractionRuntime;
    using oracle::gameplay::collect_actor_collision_bodies;
    using oracle::input::InputAction;
    using oracle::script::CampaignScriptProfile;
    using oracle::script::OriginalActorField;
    using oracle::script::OriginalStateKey;

    const auto rom = RomSource::load(path);
    check(
        rom.metadata().campaign == campaign,
        "Vasu scenario ROM campaign matches");
    const auto scenario = oracle::gameplay::vasu_scenario(campaign);
    const RoomObjectDecoder object_decoder{rom};
    const auto catalog = object_decoder.decode(
        static_cast<std::uint8_t>(scenario.room.area),
        static_cast<std::uint8_t>(scenario.room.room));
    check(
        catalog.records.size() == 5,
        "Vasu shop has five original interaction records");

    ActorSlotDomain actors;
    const auto report = RoomActorLoader::load(catalog, actors);
    check(report.failures.empty(), "Vasu shop actors fit original slots");
    check(
        actors.active_count(ActorCategory::interaction) == 5,
        "Vasu shop instantiates all room interactions");
    const auto slots = actors.slots(ActorCategory::interaction);
    check(
        slots[2].identity.id == 0x89 &&
            slots[2].identity.subid == 0 &&
            slots[2].local_x == 0x50 &&
            slots[2].local_y == 0x18,
        "Vasu actor preserves ROM identity and ground anchor");

    const InteractionSpriteDecoder sprite_decoder{rom};
    const auto sprite = sprite_decoder.decode_vasu(0, 0);
    check(
        sprite.width > 16 && sprite.height > 16,
        "Vasu OAM keeps its multi-sprite original size");
    check(
        std::any_of(
            sprite.pixels.begin(),
            sprite.pixels.end(),
            [](const oracle::content::RgbaPixel pixel) {
                return pixel.alpha != 0;
            }),
        "Vasu frame contains decoded opaque ROM pixels");

    const RomTextDecoder text_decoder{rom};
    const auto welcome = text_decoder.decode(0x3003);
    check(
        welcome.pages.size() == 3 && welcome.option_count == 3,
        "Vasu welcome retains original stops and options");
    check(
        welcome.pages.front().find("Vasu") != std::string::npos,
        "Vasu welcome text decodes from the ROM");
    check(
        welcome.option_labels ==
            std::vector<std::string>{"Appraise", "List", "Quit"},
        "Vasu menu labels decode from original text commands");

    auto player = PlayerTraversal::from_packed_room_position(
        scenario.room,
        scenario.player_spawn_yx);
    player.facing = PlayerFacing::north;
    VasuInteractionRuntime runtime{rom};
    runtime.update(
        pressed_frame(InputAction::a),
        player,
        actors);
    const auto collision_bodies =
        collect_actor_collision_bodies(actors);
    check(
        collision_bodies.size() == 1 &&
            collision_bodies.front().radius_y == 0x12 &&
            collision_bodies.front().radius_x == 0x06,
        "Vasu collision radii come from retail opcode 8d");
    oracle::content::RoomCollisionMap clear_room{
        .id = scenario.room,
        .columns = 10,
        .rows = 8,
        .values = std::vector<std::uint8_t>(80, 0),
    };
    const auto clear_lookup =
        [&](const oracle::core::WorldRoomId id)
            -> const oracle::content::RoomCollisionMap* {
            return id == clear_room.id ? &clear_room : nullptr;
        };
    auto collision_player = player;
    const auto contact = PlayerTraversal::step(
        collision_player,
        {},
        0.0,
        clear_lookup,
        collision_bodies.bodies());
    check(
        contact.contacted_actor && contact.blocked,
        "Vasu resolves an existing Link overlap");
    check_close(
        std::abs(
            collision_player.local_x -
            collision_bodies.front().local_x),
        12.0,
        "Vasu contact uses both original X collision radii");
    check(
        oracle::gameplay::InteractionTargetFinder::find(
            collision_player,
            actors,
            24.0,
            12.0) == collision_bodies.front().actor,
        "Link can still address Vasu from the collision boundary");
    runtime.update(
        pressed_frame(InputAction::a),
        player,
        actors);
    runtime.update(
        pressed_frame(InputAction::a),
        player,
        actors);
    runtime.update(
        pressed_frame(InputAction::down),
        player,
        actors);
    runtime.update(
        pressed_frame(InputAction::a),
        player,
        actors);
    runtime.update({}, player, actors);
    check(
        runtime.model().message == 0x3015,
        "Vasu List branch reaches the original no-rings message");
    const auto* instance = runtime.script_instance();
    check(instance != nullptr, "Vasu owns a campaign-script instance");
    if (instance != nullptr) {
        const auto entry = CampaignScriptProfile::vasu_entry(campaign);
        check(
            !runtime.script_trace().empty() &&
                runtime.script_trace().front().source == entry,
            "Vasu execution begins at the retail script address");
        check(
            runtime.original_state().read_actor(
                instance->actor,
                OriginalActorField::var3b) == 3,
            "retail helper records the no-special-ring route in var3b");
        check(
            runtime.original_state().read(
                OriginalStateKey::selected_text_option) == 1,
            "retail option branches use the original selection key");
    }

    VasuInteractionRuntime replay{rom};
    const std::array<InputAction, 5> actions{
        InputAction::a,
        InputAction::a,
        InputAction::a,
        InputAction::down,
        InputAction::a,
    };
    for (const auto action : actions) {
        replay.update(
            pressed_frame(action),
            player,
            actors);
    }
    replay.update({}, player, actors);
    check(
        replay.deterministic_state() ==
            runtime.deterministic_state(),
        "identical semantic input reproduces Vasu state");
    check(
        replay.script_trace() == runtime.script_trace(),
        "identical semantic input reproduces the complete script trace");

    std::vector<std::uint8_t> opcodes;
    for (const auto& event : runtime.script_trace()) {
        opcodes.push_back(event.opcode);
    }
    const std::array expected_opcodes{
        std::uint8_t{0x8d},
        std::uint8_t{0x9b},
        std::uint8_t{0xbe},
        std::uint8_t{0x9e},
        std::uint8_t{0xbd},
        std::uint8_t{0xb5},
        std::uint8_t{0xe0},
        std::uint8_t{0xc6},
        std::uint8_t{0x9a},
        std::uint8_t{0xc3},
        std::uint8_t{0xc3},
        std::uint8_t{0xcc},
        std::uint8_t{0x98},
    };
    check(
        opcodes == std::vector<std::uint8_t>{
            expected_opcodes.begin(),
            expected_opcodes.end()},
        "Vasu List scenario executes the expected retail opcode path");
    return opcodes;
}

void test_link_sprite_facing(
    const std::filesystem::path& path,
    const oracle::core::Campaign campaign) {
    if (!std::filesystem::exists(path)) {
        return;
    }
    using oracle::content::LinkDirection;
    using oracle::content::LinkSpriteDecoder;
    using oracle::content::RomSource;

    const auto rom = RomSource::load(path);
    check(
        rom.metadata().campaign == campaign,
        "Link sprite test ROM campaign matches");
    const LinkSpriteDecoder decoder{rom};
    constexpr std::array directions{
        LinkDirection::north,
        LinkDirection::east,
        LinkDirection::south,
        LinkDirection::west,
    };
    for (std::size_t index = 0; index < directions.size(); ++index) {
        const auto idle = decoder.decode(directions[index], false, 0);
        const auto walk_a = decoder.decode(directions[index], true, 0);
        const auto walk_b = decoder.decode(directions[index], true, 6);
        check(
            idle.original_frame == 0x54 + index,
            "idle Link retains the directional walk pose");
        check(
            walk_a.original_frame == idle.original_frame,
            "first walking phase matches the directional idle pose");
        check(
            walk_b.original_frame == 0x80 + index,
            "second walking phase preserves Link facing");
        check(
            idle.origin_x == -8 &&
                idle.origin_y == -8 &&
                idle.width == 16 &&
                idle.height == 16,
            "ordinary Link frames retain the original 16x16 anchor");
    }

    const auto north_attack = decoder.decode_original_frame(0xb4);
    check(
        north_attack.origin_y < -8 &&
            north_attack.pixels.size() ==
                static_cast<std::size_t>(
                    north_attack.width * north_attack.height),
        "shifted attack OAM expands above Link without clipping");
}

void test_sword_rom_scenario(
    const std::filesystem::path& path,
    const oracle::core::Campaign campaign) {
    if (!std::filesystem::exists(path)) {
        return;
    }
    using oracle::content::LinkSpriteDecoder;
    using oracle::content::RomSource;
    using oracle::content::SwordSpriteDecoder;
    using oracle::core::ActorCategory;
    using oracle::core::ActorSlotDomain;
    using oracle::gameplay::PlayerFacing;
    using oracle::gameplay::PlayerState;
    using oracle::gameplay::SwordRuntime;
    using oracle::input::InputAction;

    const auto rom = RomSource::load(path);
    check(
        rom.metadata().campaign == campaign,
        "sword test ROM campaign matches");
    const SwordSpriteDecoder sword_decoder{rom};
    for (std::uint8_t animation = 0; animation < 8; ++animation) {
        const auto frame = sword_decoder.decode(animation);
        check(
            frame.original_oam_index == animation &&
                frame.width > 0 &&
                frame.height > 0 &&
                std::any_of(
                    frame.pixels.begin(),
                    frame.pixels.end(),
                    [](const oracle::content::RgbaPixel pixel) {
                        return pixel.alpha != 0;
                    }),
            "sword animation decodes nonempty original OAM pixels");
    }
    const auto attack_link =
        LinkSpriteDecoder{rom}.decode_original_frame(0xac);
    check(
        attack_link.original_frame == 0xac &&
            attack_link.pixels.size() ==
                static_cast<std::size_t>(
                    attack_link.width * attack_link.height),
        "Link decoder preserves the full retail sword attack frame");

    ActorSlotDomain actors;
    SwordRuntime runtime;
    const PlayerState player{
        .room = oracle::core::WorldRoomId{.area = 0, .room = 0x64},
        .local_x = 80.0,
        .local_y = 64.0,
        .facing = PlayerFacing::north,
    };
    const auto first = runtime.update(
        pressed_frame(InputAction::b),
        player,
        actors);
    check(
        first.started &&
            first.pose.has_value() &&
            first.pose->actor.slot == 6 &&
            first.pose->arc_index == 0 &&
            first.pose->animation_index == 2 &&
            first.pose->link_frame == 0xac &&
            first.pose->visual_elevation == 2.0 &&
            first.pose->animation_parameter == 0x00 &&
            actors.slots(ActorCategory::item)[2].active &&
            actors.active_count(ActorCategory::item) == 2,
        "sword press reserves parent slot 2 and weapon slot 6");
    const auto opening_sword = sword_decoder.decode(2);
    check(
        opening_sword.origin_y <= -8 &&
            first.pose->local_x +
                    static_cast<double>(opening_sword.origin_x) >
                player.local_x &&
            first.pose->local_y -
                    first.pose->visual_elevation +
                    static_cast<double>(opening_sword.origin_y) <
                player.local_y,
        "north opening sword frame is anchored above-right of Link");
    const auto first_generation =
        first.pose.has_value() ? first.pose->actor.generation : 0;

    (void)runtime.update({}, player, actors);  // tick 1
    (void)runtime.update({}, player, actors);  // tick 2
    const auto second_pose = runtime.update({}, player, actors);  // tick 3
    check(
        second_pose.pose.has_value() &&
            second_pose.pose->arc_index == 4 &&
            second_pose.pose->animation_index == 1 &&
            second_pose.pose->link_frame == 0xb0 &&
            second_pose.pose->animation_parameter == 0x02,
        "sword tick 3 advances through LINK_ANIM_MODE_22");

    (void)runtime.update({}, player, actors);  // tick 4
    (void)runtime.update({}, player, actors);  // tick 5
    const auto third_pose = runtime.update({}, player, actors);  // tick 6
    check(
        third_pose.pose.has_value() &&
            third_pose.pose->arc_index == 8 &&
            third_pose.pose->animation_index == 0 &&
            third_pose.pose->link_frame == 0xb4 &&
            third_pose.pose->animation_parameter == 0x64,
        "sword tick 6 preserves the tile-break parameter pose");

    for (int tick = 7; tick < 14; ++tick) {
        (void)runtime.update({}, player, actors);
    }
    const auto final_pose = runtime.update({}, player, actors);  // tick 14
    check(
        final_pose.pose.has_value() &&
            final_pose.pose->arc_index == 12 &&
            final_pose.pose->animation_index == 0 &&
            final_pose.pose->link_frame == 0xb0 &&
            final_pose.pose->animation_parameter == 0x06,
        "sword tick 14 selects the final active retail pose");
    (void)runtime.update({}, player, actors);  // tick 15
    (void)runtime.update({}, player, actors);  // tick 16
    const auto ended = runtime.update({}, player, actors);  // end marker
    check(
        ended.ended &&
            !ended.pose.has_value() &&
            actors.active_count(ActorCategory::item) == 0,
        "sword end marker releases the reserved item slot");
    const auto restarted = runtime.update(
        pressed_frame(InputAction::b),
        player,
        actors);
    check(
        restarted.started &&
            restarted.pose.has_value() &&
            restarted.pose->actor.generation != first_generation,
        "a later sword press allocates a fresh item-slot generation");
}

void test_vasu_rom_scenarios() {
    using oracle::core::Campaign;
    using oracle::script::OriginalActorField;
    using oracle::script::OriginalStateKey;
    using oracle::script::OriginalStateResolver;

    check(
        OriginalStateResolver::memory_key(
            Campaign::ages,
            0xc615) == OriginalStateKey::obtained_ring_box,
        "original WRAM resolver names the ring-box byte");
    check(
        OriginalStateResolver::memory_key(
            Campaign::seasons,
            0xcba5) == OriginalStateKey::selected_text_option,
        "original WRAM resolver names the text-option byte");
    check(
        OriginalStateResolver::global_flag_key(8) ==
            OriginalStateKey::global_obtained_ring_box,
        "original global flag index resolves independently of WRAM layout");
    check(
        OriginalStateResolver::actor_field(0x7b) ==
            OriginalActorField::var3b,
        "original interaction offset resolves to var3b");
    check(
        !OriginalStateResolver::memory_key(
            Campaign::ages,
            0xffff).has_value() &&
            !OriginalStateResolver::global_flag_key(0xff).has_value() &&
            !OriginalStateResolver::actor_field(0xff).has_value(),
        "unknown original state coordinates remain unmapped");

    const auto ages = test_vasu_rom_scenario(
        "roms/Legend of Zelda, The - Oracle of Ages (USA).gbc",
        Campaign::ages);
    const auto seasons = test_vasu_rom_scenario(
        "roms/Legend of Zelda, The - Oracle of Seasons (USA).gbc",
        Campaign::seasons);
    if (!ages.empty() && !seasons.empty()) {
        check(
            ages == seasons,
            "both relocated Vasu scripts share one native opcode path");
    }
}

std::optional<std::uint64_t> test_octorok_rom_scenario(
    const std::filesystem::path& path,
    const oracle::core::Campaign campaign) {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    using oracle::content::EnemyDefinitionDecoder;
    using oracle::content::EnemySpriteDecoder;
    using oracle::content::PartDefinitionDecoder;
    using oracle::content::PartSpriteDecoder;
    using oracle::content::RomSource;
    using oracle::content::RoomObjectDecoder;
    using oracle::core::ActorCategory;
    using oracle::core::ActorSlotDomain;
    using oracle::gameplay::OctorokRuntime;
    using oracle::gameplay::PlayerCombatState;
    using oracle::gameplay::PlayerFacing;
    using oracle::gameplay::PlayerState;
    using oracle::gameplay::RoomActorLoader;
    using oracle::input::InputAction;

    const auto rom = RomSource::load(path);
    const auto definition =
        EnemyDefinitionDecoder{rom}.decode(0x09, 0);
    check(
        definition.object_gfx_header ==
            (campaign == oracle::core::Campaign::ages
                 ? 0x8f
                 : 0x74),
        "Octorok uses the campaign-relocated object graphics header");
    check(
        definition.collision_enabled &&
            definition.collision_mode == 0x10 &&
            definition.extra_data_index == 0x08 &&
            definition.collision_radius_y == 6 &&
            definition.collision_radius_x == 6 &&
            definition.contact_damage == -2 &&
            definition.health == 2 &&
            definition.palette == 2 &&
            definition.tile_base == 0,
        "Octorok properties decode from enemyData and extraEnemyData");

    const auto sprite =
        EnemySpriteDecoder{rom}.decode_octorok(0, 0);
    check(
        sprite.original_oam_index == 0 &&
            sprite.width == 16 &&
            sprite.height == 16 &&
            sprite.origin_x == -8 &&
            sprite.origin_y == 8,
        "Octorok north frame follows original OAM and hardware origins");
    check(
        std::any_of(
            sprite.pixels.begin(),
            sprite.pixels.end(),
            [](const oracle::content::RgbaPixel pixel) {
                return pixel.alpha != 0;
            }),
        "Octorok frame contains authentic decoded ROM pixels");

    const auto projectile_definition =
        PartDefinitionDecoder{rom}.decode(0x18);
    check(
        projectile_definition.object_gfx_header ==
            definition.object_gfx_header &&
            projectile_definition.collision_enabled &&
            projectile_definition.collision_mode == 0x07 &&
            projectile_definition.collision_radius_y == 2 &&
            projectile_definition.collision_radius_x == 2 &&
            projectile_definition.contact_damage == -4 &&
            projectile_definition.health == 0x40 &&
            projectile_definition.tile_base == 0x0c &&
            projectile_definition.oam_flags == 0x03,
        "Octorok projectile attributes decode from relocated partData");
    const auto projectile_sprite =
        PartSpriteDecoder{rom}.decode_octorok_projectile();
    check(
        projectile_sprite.part_id == 0x18 &&
            projectile_sprite.original_oam_index == 0 &&
            projectile_sprite.width == 16 &&
            projectile_sprite.height == 16 &&
            projectile_sprite.origin_x == -8 &&
            projectile_sprite.origin_y == 8,
        "Octorok projectile follows its original two-object part OAM");
    check(
        std::any_of(
            projectile_sprite.pixels.begin(),
            projectile_sprite.pixels.end(),
            [](const oracle::content::RgbaPixel pixel) {
                return pixel.alpha != 0;
            }),
        "Octorok projectile frame contains decoded ROM pixels");
    const PartSpriteDecoder part_sprites{rom};
    for (std::uint8_t oam = 0; oam < 6; ++oam) {
        const auto frame =
            part_sprites.decode_enemy_destroyed(oam);
        check(
            frame.part_id == 0x02 &&
                frame.original_oam_index == oam &&
                !frame.pixels.empty() &&
                std::any_of(
                    frame.pixels.begin(),
                    frame.pixels.end(),
                    [](const oracle::content::RgbaPixel pixel) {
                        return pixel.alpha != 0;
                    }),
            "enemy-destroyed animation decodes original part pixels");
    }
    for (
        const std::uint8_t drop :
        std::array<std::uint8_t, 3>{0x01, 0x02, 0x03}) {
        const auto frame = part_sprites.decode_item_drop(drop);
        check(
            frame.part_id == 0x01 &&
                !frame.pixels.empty() &&
                std::any_of(
                    frame.pixels.begin(),
                    frame.pixels.end(),
                    [](const oracle::content::RgbaPixel pixel) {
                        return pixel.alpha != 0;
                    }),
            "Octorok drop-set item decodes original part pixels");
    }

    const auto scenario =
        oracle::gameplay::octorok_scenario(campaign);
    const auto catalog = RoomObjectDecoder{rom}.decode(
        static_cast<std::uint8_t>(scenario.room.area),
        static_cast<std::uint8_t>(scenario.room.room));
    ActorSlotDomain actors;
    const auto load_report = RoomActorLoader::load(catalog, actors);
    check(
        load_report.failures.empty(),
        "Octorok scenario records fit the retail enemy slots");
    const auto enemy_slots = actors.slots(ActorCategory::enemy);
    const auto octorok = std::find_if(
        enemy_slots.begin(),
        enemy_slots.end(),
        [](const oracle::core::ActorSlotState& actor) {
            return
                actor.active &&
                actor.positioned &&
                actor.identity.id == 0x09 &&
                actor.identity.subid == 0;
        });
    check(
        octorok != enemy_slots.end(),
        "Octorok scenario selects a positioned retail room record");
    if (octorok == enemy_slots.end()) {
        return std::nullopt;
    }

    oracle::content::RoomCollisionMap clear_room{
        .id = scenario.room,
        .columns = 10,
        .rows = 8,
        .values = std::vector<std::uint8_t>(80, 0),
    };
    const auto collision_lookup =
        [&](const oracle::core::WorldRoomId id)
            -> const oracle::content::RoomCollisionMap* {
            return id == clear_room.id ? &clear_room : nullptr;
        };
    auto player = PlayerState{
        .room = scenario.room,
        .local_x = static_cast<double>(octorok->local_x),
        .local_y = static_cast<double>(octorok->local_y),
        .facing = PlayerFacing::south,
    };
    PlayerCombatState combat;
    OctorokRuntime runtime{rom, 0x5a17};
    const auto contact = runtime.update(
        player,
        combat,
        actors,
        collision_lookup);
    check(
        contact.contacts == 1 &&
            combat.health == 10 &&
            combat.invincibility_ticks != 0,
        "Octorok contact applies the ROM's two health-unit damage");
    const auto contact_again = runtime.update(
        player,
        combat,
        actors,
        collision_lookup);
    check(
        contact_again.contacts == 0 && combat.health == 10,
        "player damage invincibility suppresses repeated contact damage");
    check(
        oracle::gameplay::collect_actor_collision_bodies(actors).size() == 0,
        "enemy contact remains separate from solid NPC collision");

    ActorSlotDomain impact_actors;
    const auto impact_handle = impact_actors.allocate_dynamic(
        ActorCategory::part,
        oracle::core::ActorIdentity{
            .id = 0x18,
            .parameter = 0x08,
        },
        scenario.room,
        30,
        24,
        true,
        false,
        0);
    check(
        impact_handle.has_value(),
        "Octorok projectile allocates in the original part band");
    OctorokRuntime impact_runtime{rom};
    PlayerCombatState impact_combat;
    const auto distant_player = PlayerState{
        .room = scenario.room,
        .local_x = 120.0,
        .local_y = 96.0,
    };
    clear_room.values[1 * 10 + 2] = 0x0f;
    (void)impact_runtime.update(
        distant_player,
        impact_combat,
        impact_actors,
        collision_lookup);
    const auto impact = impact_runtime.update(
        distant_player,
        impact_combat,
        impact_actors,
        collision_lookup);
    check(
        impact.projectile_impacts == 1 &&
            impact_handle.has_value() &&
            impact_runtime.projectile_phase(*impact_handle) ==
                oracle::gameplay::OctorokProjectilePhase::impact,
        "projectile enters its retail impact state at solid terrain");
    (void)impact_runtime.update(
        distant_player,
        impact_combat,
        impact_actors,
        collision_lookup);
    check(
        impact_handle.has_value() &&
            impact_runtime.projectile_phase(*impact_handle) ==
                oracle::gameplay::OctorokProjectilePhase::bouncing,
        "terrain impact reverses the projectile into a bounded bounce");
    (void)impact_runtime.update(
        distant_player,
        impact_combat,
        impact_actors,
        collision_lookup);
    check(
        impact_handle.has_value() &&
            impact_runtime.projectile_elevation(*impact_handle) > 0.0,
        "bouncing projectile exposes original vertical motion");
    for (int tick = 0; tick < 32; ++tick) {
        (void)impact_runtime.update(
            distant_player,
            impact_combat,
            impact_actors,
            collision_lookup);
    }
    check(
        impact_actors.active_count(ActorCategory::part) == 0,
        "projectile releases its part slot after the 32-tick bounce");
    clear_room.values[1 * 10 + 2] = 0;

    ActorSlotDomain projectile_contact_actors;
    const auto contact_handle =
        projectile_contact_actors.allocate_dynamic(
            ActorCategory::part,
            oracle::core::ActorIdentity{
                .id = 0x18,
                .parameter = 0x08,
            },
            scenario.room,
            40,
            40,
            true,
            false,
            0);
    OctorokRuntime projectile_contact_runtime{rom};
    PlayerCombatState projectile_combat;
    const auto projectile_target = PlayerState{
        .room = scenario.room,
        .local_x = 42.0,
        .local_y = 40.0,
    };
    (void)projectile_contact_runtime.update(
        projectile_target,
        projectile_combat,
        projectile_contact_actors,
        collision_lookup);
    const auto projectile_contact =
        projectile_contact_runtime.update(
            projectile_target,
            projectile_combat,
            projectile_contact_actors,
            collision_lookup);
    check(
        contact_handle.has_value() &&
            projectile_contact.projectile_contacts == 1 &&
            projectile_combat.health == 8 &&
            projectile_combat.invincibility_ticks != 0,
        "projectile contact applies partData's four health-unit damage");
    check(
        oracle::gameplay::collect_actor_collision_bodies(
            projectile_contact_actors).size() == 0,
        "damaging projectile parts do not become solid NPC bodies");

    const auto locate_octorok = [&]()
        -> const oracle::core::ActorSlotState* {
        const auto slots = actors.slots(ActorCategory::enemy);
        const auto found = std::find_if(
            slots.begin(),
            slots.end(),
            [](const oracle::core::ActorSlotState& actor) {
                return actor.active && actor.identity.id == 0x09;
            });
        return found == slots.end() ? nullptr : &*found;
    };
    auto* active_octorok = locate_octorok();
    check(active_octorok != nullptr, "Octorok survives one contact update");
    if (active_octorok == nullptr) {
        return std::nullopt;
    }
    // The north-facing retail swing begins to Link's right before sweeping
    // upward and left; place the target on that first arc record.
    player.local_x = active_octorok->local_x - 16.0;
    player.local_y = active_octorok->local_y + 2.0;
    player.facing = PlayerFacing::north;
    oracle::gameplay::SwordRuntime sword_runtime;
    const auto first_sword = sword_runtime.update(
        pressed_frame(InputAction::b),
        player,
        actors);
    const auto first_strike = runtime.update(
        player,
        combat,
        actors,
        collision_lookup,
        first_sword);
    active_octorok = locate_octorok();
    check(
        first_strike.sword_started &&
            first_strike.enemies_hit == 1 &&
            first_strike.enemies_defeated == 1 &&
            active_octorok != nullptr &&
            active_octorok->health == 0,
        "level-one sword applies retail damage two and defeats red Octorok");
    const auto defeated_slot = std::find_if(
        actors.slots(ActorCategory::enemy).begin(),
        actors.slots(ActorCategory::enemy).end(),
        [](const oracle::core::ActorSlotState& actor) {
            return actor.active && actor.identity.id == 0x09;
        });
    const auto defeated_handle = oracle::core::ActorSlotHandle{
        ActorCategory::enemy,
        static_cast<std::uint8_t>(
            std::distance(
                actors.slots(ActorCategory::enemy).begin(),
                defeated_slot)),
        defeated_slot->generation,
    };
    check(
        runtime.hit_flash(defeated_handle),
        "sword hit exposes the retail invincibility palette flash");
    const auto struck_x = active_octorok->local_x;
    for (int tick = 0; tick < 11; ++tick) {
        (void)runtime.update(
            player,
            combat,
            actors,
            collision_lookup);
    }
    active_octorok = locate_octorok();
    check(
        active_octorok != nullptr &&
            active_octorok->local_x > struck_x,
        "fatal sword hit preserves eleven ticks of knockback");
    const auto death_start = runtime.update(
        player,
        combat,
        actors,
        collision_lookup);
    check(
        death_start.death_puffs_spawned == 1 &&
            locate_octorok() == nullptr,
        "post-knockback enemy becomes PART_ENEMY_DESTROYED");
    const auto find_part = [&](const std::uint8_t id)
        -> std::optional<oracle::core::ActorSlotHandle> {
        const auto slots = actors.slots(ActorCategory::part);
        for (std::size_t slot = 0; slot < slots.size(); ++slot) {
            if (slots[slot].active && slots[slot].identity.id == id) {
                return oracle::core::ActorSlotHandle{
                    ActorCategory::part,
                    static_cast<std::uint8_t>(slot),
                    slots[slot].generation,
                };
            }
        }
        return std::nullopt;
    };
    const auto death_part = find_part(0x02);
    check(
        death_part.has_value() &&
            runtime.aftermath_visual(*death_part).has_value() &&
            runtime.aftermath_visual(*death_part)->oam_index == 0,
        "death puff begins on original OAM frame zero");
    for (int tick = 0; tick < 19; ++tick) {
        (void)runtime.update(
            player,
            combat,
            actors,
            collision_lookup);
    }
    check(
        death_part.has_value() &&
            runtime.aftermath_visual(*death_part).has_value() &&
            runtime.aftermath_visual(*death_part)->oam_index == 5,
        "twenty-tick death puff reaches original final OAM frame");
    (void)runtime.update(
        player,
        combat,
        actors,
        collision_lookup);
    check(
        !find_part(0x02).has_value() &&
            !find_part(0x01).has_value(),
        "seed 5a17 follows Octorok probability table to no drop");

    ActorSlotDomain drop_actors;
    (void)RoomActorLoader::load(catalog, drop_actors);
    const auto drop_enemy = std::find_if(
        drop_actors.slots(ActorCategory::enemy).begin(),
        drop_actors.slots(ActorCategory::enemy).end(),
        [](const oracle::core::ActorSlotState& actor) {
            return actor.active && actor.identity.id == 0x09;
        });
    auto drop_player = PlayerState{
        .room = scenario.room,
        .local_x = drop_enemy->local_x - 16.0,
        .local_y = drop_enemy->local_y + 2.0,
        .facing = PlayerFacing::north,
    };
    OctorokRuntime drop_runtime{rom, 0x0003};
    PlayerCombatState drop_combat{.health = 5};
    oracle::gameplay::SwordRuntime drop_sword_runtime;
    const auto drop_sword = drop_sword_runtime.update(
        pressed_frame(InputAction::b),
        drop_player,
        drop_actors);
    (void)drop_runtime.update(
        drop_player,
        drop_combat,
        drop_actors,
        collision_lookup,
        drop_sword);
    for (int tick = 0; tick < 32; ++tick) {
        (void)drop_runtime.update(
            drop_player,
            drop_combat,
            drop_actors,
            collision_lookup);
    }
    const auto item_drop = [&]()
        -> std::optional<oracle::core::ActorSlotHandle> {
        const auto slots = drop_actors.slots(ActorCategory::part);
        for (std::size_t slot = 0; slot < slots.size(); ++slot) {
            if (
                slots[slot].active &&
                slots[slot].identity.id == 0x01) {
                return oracle::core::ActorSlotHandle{
                    ActorCategory::part,
                    static_cast<std::uint8_t>(slot),
                    slots[slot].generation,
                };
            }
        }
        return std::nullopt;
    }();
    check(
        item_drop.has_value() &&
            drop_actors.get(*item_drop)->identity.subid == 0x01 &&
            drop_runtime.aftermath_visual(*item_drop).has_value(),
        "seed 0003 selects a heart from Octorok drop set E");
    if (item_drop.has_value()) {
        for (int tick = 0; tick < 80; ++tick) {
            (void)drop_runtime.update(
                drop_player,
                drop_combat,
                drop_actors,
                collision_lookup);
        }
        const auto* drop_actor = drop_actors.get(*item_drop);
        if (drop_actor != nullptr) {
            drop_player.local_x = drop_actor->local_x;
            drop_player.local_y = drop_actor->local_y;
        }
        const auto collected = drop_runtime.update(
            drop_player,
            drop_combat,
            drop_actors,
            collision_lookup);
        check(
            collected.item_drops_collected == 1 &&
                drop_combat.health == 9 &&
                drop_actors.get(*item_drop) == nullptr,
            "settled heart drop restores four health and releases its slot");
    }

    ActorSlotDomain replay_actors;
    (void)RoomActorLoader::load(catalog, replay_actors);
    OctorokRuntime replay{rom, 0x5a17};
    PlayerCombatState replay_combat;
    auto replay_player = PlayerState{
        .room = scenario.room,
        .local_x = 8.0,
        .local_y = 8.0,
    };
    std::uint32_t replay_projectiles = 0;
    for (int tick = 0; tick < 1200; ++tick) {
        const auto step = replay.update(
            replay_player,
            replay_combat,
            replay_actors,
            collision_lookup);
        replay_projectiles += step.projectiles_spawned;
    }
    check(
        replay_projectiles != 0,
        "Octorok shooting requests allocate projectile part slots");

    ActorSlotDomain second_replay_actors;
    (void)RoomActorLoader::load(catalog, second_replay_actors);
    OctorokRuntime second_replay{rom, 0x5a17};
    PlayerCombatState second_replay_combat;
    for (int tick = 0; tick < 1200; ++tick) {
        (void)second_replay.update(
            replay_player,
            second_replay_combat,
            second_replay_actors,
            collision_lookup);
    }
    check(
        replay.deterministic_state() ==
            second_replay.deterministic_state(),
        "identical seed and ticks reproduce Octorok native state");
    return replay.deterministic_state();
}

void test_octorok_rom_scenarios() {
    using oracle::core::Campaign;
    const auto ages = test_octorok_rom_scenario(
        "roms/Legend of Zelda, The - Oracle of Ages (USA).gbc",
        Campaign::ages);
    const auto seasons = test_octorok_rom_scenario(
        "roms/Legend of Zelda, The - Oracle of Seasons (USA).gbc",
        Campaign::seasons);
    if (ages.has_value() && seasons.has_value()) {
        check(
            ages == seasons,
            "both cartridges drive one shared Octorok behavior path");
    }
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

void test_spatial_room_seams() {
    using oracle::content::RoomExitKind;
    using oracle::content::RoomTopologyDecoder;

    const auto center = RoomTopologyDecoder::spatial_seams(0, 0x91);
    check(center.size() == 4, "interior overworld rooms have four seams");
    check(
        center[0].kind == RoomExitKind::north_seam &&
            center[0].destination.room == 0x81,
        "north seam decrements the room row");
    check(
        center[1].kind == RoomExitKind::east_seam &&
            center[1].destination.room == 0x92,
        "east seam increments the room column");

    const auto corner = RoomTopologyDecoder::spatial_seams(3, 0x00);
    check(corner.size() == 2, "corner overworld rooms have two seams");
    check(
        corner[0].kind == RoomExitKind::east_seam &&
            corner[1].kind == RoomExitKind::south_seam,
        "top-left room only connects east and south");
    check(
        RoomTopologyDecoder::spatial_seams(4, 0x91).empty(),
        "large-layout groups do not infer grid seams");
}

void test_player_traversal() {
    using oracle::content::RoomCollisionMap;
    using oracle::core::WorldRoomId;
    using oracle::gameplay::MovementInput;
    using oracle::gameplay::ActorCollisionBody;
    using oracle::gameplay::PlayerState;
    using oracle::gameplay::PlayerTraversal;

    std::array<RoomCollisionMap, 2> maps{
        RoomCollisionMap{
            .id = WorldRoomId{.area = 0, .room = 0x91},
            .columns = 10,
            .rows = 8,
            .values = std::vector<std::uint8_t>(80, 0),
        },
        RoomCollisionMap{
            .id = WorldRoomId{.area = 0, .room = 0x92},
            .columns = 10,
            .rows = 8,
            .values = std::vector<std::uint8_t>(80, 0),
        },
    };
    const auto lookup =
        [&](const WorldRoomId id) -> const RoomCollisionMap* {
            for (const auto& map : maps) {
                if (map.id == id) {
                    return &map;
                }
            }
            return nullptr;
        };

    PlayerState player{
        .room = WorldRoomId{.area = 0, .room = 0x91},
        .local_x = 155.0,
        .local_y = 64.0,
    };
    const auto seam_step =
        PlayerTraversal::step(
            player,
            MovementInput{.horizontal = 1.0},
            0.2,
            lookup);
    check(seam_step.moved, "player moves through clear collision data");
    check(
        seam_step.crossed_room_seam &&
            player.room.room == 0x92,
        "player movement crosses an available east room seam");
    check(
        player.local_x < 16.0,
        "seam crossing wraps to destination-local coordinates");

    player =
        PlayerState{
            .room = WorldRoomId{.area = 0, .room = 0x91},
            .local_x = 152.0,
            .local_y = 64.0,
        };
    maps[1].values[4 * 10] = 0x0f;
    const auto blocked_seam =
        PlayerTraversal::step(
            player,
            MovementInput{.horizontal = 1.0},
            0.2,
            lookup);
    check(
        blocked_seam.blocked && player.room.room == 0x91,
        "destination-room collision blocks a seam crossing");

    maps[0].values[2 * 10 + 2] = 0x0f;
    player =
        PlayerState{
            .room = WorldRoomId{.area = 0, .room = 0x91},
            .local_x = 28.0,
            .local_y = 40.0,
        };
    static_cast<void>(
        PlayerTraversal::step(
            player,
            MovementInput{.horizontal = 1.0},
            0.5,
            lookup));
    check(
        player.local_x < 28.1,
        "substepped traversal cannot tunnel through a solid metatile");

    const std::array actor_bodies{
        ActorCollisionBody{
            .room = WorldRoomId{.area = 0, .room = 0x91},
            .local_x = 64.0,
            .local_y = 64.0,
            .radius_y = 18,
            .radius_x = 6,
        },
    };
    player =
        PlayerState{
            .room = WorldRoomId{.area = 0, .room = 0x91},
            .local_x = 60.0,
            .local_y = 60.0,
        };
    const auto overlap_step =
        PlayerTraversal::step(
            player,
            {},
            0.0,
            lookup,
            actor_bodies);
    check(
        overlap_step.contacted_actor &&
            overlap_step.blocked &&
            overlap_step.moved,
        "actor contact reports deterministic overlap correction");
    check_close(
        player.local_x,
        52.0,
        "shallower horizontal actor overlap resolves first");
    check_close(
        player.local_y,
        60.0,
        "actor overlap leaves the non-resolved axis unchanged");

    player =
        PlayerState{
            .room = WorldRoomId{.area = 0, .room = 0x91},
            .local_x = 52.0,
            .local_y = 52.0,
        };
    const auto actor_slide =
        PlayerTraversal::step(
            player,
            MovementInput{
                .horizontal = 1.0,
                .vertical = 1.0,
            },
            0.25,
            lookup,
            actor_bodies);
    check(
        actor_slide.contacted_actor &&
            actor_slide.blocked &&
            actor_slide.moved,
        "diagonal movement reports actor contact while sliding");
    check_close(
        player.local_x,
        52.0,
        "actor body blocks the penetrating movement axis");
    check(
        player.local_y > 52.0,
        "unblocked movement axis slides along the actor body");

    maps[1].values[4 * 10] = 0;
    const std::array adjacent_actor{
        ActorCollisionBody{
            .room = WorldRoomId{.area = 0, .room = 0x92},
            .local_x = 6.0,
            .local_y = 64.0,
            .radius_y = 6,
            .radius_x = 6,
        },
    };
    player =
        PlayerState{
            .room = WorldRoomId{.area = 0, .room = 0x91},
            .local_x = 152.0,
            .local_y = 64.0,
        };
    const auto adjacent_contact =
        PlayerTraversal::step(
            player,
            MovementInput{.horizontal = 1.0},
            0.2,
            lookup,
            adjacent_actor);
    check(
        adjacent_contact.contacted_actor &&
            adjacent_contact.blocked &&
            player.room.room == 0x91 &&
            player.local_x <= 154.0,
        "actor bodies block movement across a small-room seam");

    const auto packed =
        PlayerTraversal::from_packed_room_position(
            WorldRoomId{.area = 2, .room = 0x9f},
            0x44);
    check_close(
        packed.local_x,
        72.0,
        "packed warp X converts to metatile center");
    check_close(
        packed.local_y,
        56.0,
        "packed warp Y accounts for the status-bar row");
    check(
        PlayerTraversal::packed_room_position(packed) == 0x44,
        "packed warp position conversion round-trips");

    const auto enters_from_bottom =
        PlayerTraversal::from_transition_destination(
            WorldRoomId{.area = 4, .room = 0x66},
            0xff,
            0x09,
            0x03,
            240.0,
            176.0);
    check_close(
        enters_from_bottom.local_x,
        120.0,
        "FF enter-screen destination uses horizontal room center");
    check_close(
        enters_from_bottom.local_y,
        168.0,
        "parameter 9 enters from the bottom room edge");
}

}  // namespace

int main() {
    test_semantic_input_frames();
    test_actor_slot_domain();
    test_item_primitives();
    test_campaign_policy();
    test_presentation_camera();
    test_experience_profiles();
    test_room_layout_decompression();
    test_graphics_decompression();
    test_room_tile_replacements();
    test_room_collision_shapes();
    test_spatial_room_seams();
    test_player_traversal();
    test_link_sprite_facing(
        "roms/Legend of Zelda, The - Oracle of Ages (USA).gbc",
        oracle::core::Campaign::ages);
    test_link_sprite_facing(
        "roms/Legend of Zelda, The - Oracle of Seasons (USA).gbc",
        oracle::core::Campaign::seasons);
    test_sword_rom_scenario(
        "roms/Legend of Zelda, The - Oracle of Ages (USA).gbc",
        oracle::core::Campaign::ages);
    test_sword_rom_scenario(
        "roms/Legend of Zelda, The - Oracle of Seasons (USA).gbc",
        oracle::core::Campaign::seasons);
    test_vasu_rom_scenarios();
    test_octorok_rom_scenarios();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "oracle_core_tests: OK\n";
    return EXIT_SUCCESS;
}
