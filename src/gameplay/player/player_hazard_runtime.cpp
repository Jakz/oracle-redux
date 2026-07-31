#include "oracle/gameplay/player_hazard_runtime.h"

#include <cmath>

#include "oracle/content/room_layout.h"

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t partial_control_ticks = 0x10;
constexpr std::uint8_t feather_landing_counter = 0x04;
constexpr std::uint8_t changed_hole_counter = 0x0e;
constexpr std::uint8_t fall_frame_1_ticks = 0x10;
constexpr std::uint8_t fall_frame_2_ticks = 0x0a;
constexpr std::uint8_t fall_frame_3_ticks = 0x0a;
constexpr std::uint8_t fall_animation_ticks =
    fall_frame_1_ticks + fall_frame_2_ticks + fall_frame_3_ticks;
constexpr std::uint8_t flippers_entry_ticks = 0x0a;
constexpr std::uint8_t mermaid_suit_entry_ticks = 0x02;
constexpr std::uint8_t drown_splash_ticks = 0x06;
constexpr std::uint8_t drown_body_ticks = 0x10;
constexpr std::uint8_t drown_animation_ticks =
    drown_splash_ticks + drown_body_ticks;
constexpr std::uint8_t hidden_ticks = 0x02;
constexpr std::uint8_t recovery_ticks = 0x10;
constexpr std::uint8_t hazard_damage = 0x04;
constexpr std::uint8_t respawn_invincibility_ticks = 0x3c;
constexpr std::uint8_t water_entry_invincibility_ticks = 0x88;
constexpr double normal_speed_pixels_per_second = 60.0;
constexpr double flippers_speed_pixels_per_second = 30.0;
constexpr double metatile_center = 8.0;

bool same_contact(
    const content::LinkTileContact& left,
    const content::LinkTileContact& right) noexcept {
    return left.room == right.room &&
        left.type == right.type &&
        left.column == right.column &&
        left.row == right.row;
}

bool is_water(const content::LinkTileType type) noexcept {
    return type == content::LinkTileType::water ||
        type == content::LinkTileType::seawater;
}

bool can_swim(
    const content::LinkTileType type,
    const PlayerWaterCapability capability) noexcept {
    if (type == content::LinkTileType::seawater) {
        return capability == PlayerWaterCapability::mermaid_suit;
    }
    return capability != PlayerWaterCapability::none;
}

bool centered(const double coordinate) noexcept {
    auto within_tile = std::fmod(coordinate, 16.0);
    if (within_tile < 0.0) {
        within_tile += 16.0;
    }
    const auto integer_coordinate = static_cast<int>(std::floor(within_tile));
    return integer_coordinate >= 7 && integer_coordinate <= 9;
}

void apply_retail_hole_pull(
    double& coordinate,
    const double target) noexcept {
    // The LR35902 branch increments on an exact zero delta too. The following
    // center window accepts the resulting +1 coordinate.
    coordinate += coordinate > target ? -1.0 : 1.0;
}

std::uint8_t fall_frame(const std::uint8_t tick) noexcept {
    if (tick < fall_frame_1_ticks) {
        return 0x08;
    }
    if (tick < fall_frame_1_ticks + fall_frame_2_ticks) {
        return 0x09;
    }
    return 0x0a;
}

std::uint8_t swim_frame(const std::uint8_t tick) noexcept {
    return static_cast<std::uint8_t>(0x0b + ((tick / 0x10u) & 1u));
}

std::uint8_t drown_frame(
    const core::Campaign campaign,
    const std::uint8_t tick) noexcept {
    if (tick < drown_splash_ticks) {
        return campaign == core::Campaign::ages ? 0xd4 : 0xc8;
    }
    return 0x0b;
}

}  // namespace

PlayerHazardRuntime::PlayerHazardRuntime(
    const core::Campaign campaign) noexcept
    : campaign_{campaign} {}

void PlayerHazardRuntime::reset(const PlayerState& respawn_point) noexcept {
    set_respawn_point(respawn_point);
    phase_ = PlayerHazardPhase::normal;
    last_contact_.reset();
    standing_counter_ = 0;
    phase_counter_ = 0;
    fall_tick_ = 0;
    swim_tick_ = 0;
    drown_tick_ = 0;
    visible_ = true;
}

void PlayerHazardRuntime::set_respawn_point(
    const PlayerState& player) noexcept {
    respawn_point_ = player;
    respawn_point_.z_subpixels = 0;
    respawn_point_.speed_z_subpixels = 0;
    respawn_point_.in_air = 0;
    respawn_point_.object_contact_enabled = true;
}

void PlayerHazardRuntime::set_water_capability(
    const PlayerWaterCapability capability) noexcept {
    water_capability_ = capability;
}

PlayerHazardStepReport PlayerHazardRuntime::update(
    PlayerState& player,
    PlayerCombatState& combat,
    const std::optional<content::LinkTileContact> contact,
    const bool landed_this_tick) noexcept {
    auto events = report();

    switch (phase_) {
    case PlayerHazardPhase::normal:
    case PlayerHazardPhase::hole_pull: {
        if (
            player.in_air != 0 ||
            !contact.has_value()) {
            phase_ = PlayerHazardPhase::normal;
            standing_counter_ = 0;
            last_contact_ = contact;
            player.object_contact_enabled = true;
            return report();
        }

        if (is_water(contact->type)) {
            const auto contact_changed =
                !last_contact_.has_value() ||
                !same_contact(*last_contact_, *contact);
            standing_counter_ = contact_changed
                ? 1
                : standing_counter_ == 0xff
                ? standing_counter_
                : static_cast<std::uint8_t>(standing_counter_ + 1);
            last_contact_ = contact;
            swim_tick_ = 0;
            events.entered_water = true;
            if (can_swim(contact->type, water_capability_)) {
                phase_ = PlayerHazardPhase::water_entry;
                phase_counter_ =
                    water_capability_ == PlayerWaterCapability::mermaid_suit
                    ? mermaid_suit_entry_ticks
                    : flippers_entry_ticks;
                player.object_contact_enabled = true;
                events.began_swimming = true;
            } else {
                phase_ = PlayerHazardPhase::drowning;
                drown_tick_ = 0;
                player.object_contact_enabled = false;
                combat.invincibility_ticks =
                    water_entry_invincibility_ticks;
                events.began_drowning = true;
            }
            break;
        }

        if (contact->type != content::LinkTileType::hole) {
            phase_ = PlayerHazardPhase::normal;
            standing_counter_ = 0;
            last_contact_ = contact;
            player.object_contact_enabled = true;
            return report();
        }

        const auto was_pulling = phase_ == PlayerHazardPhase::hole_pull;
        const auto contact_changed =
            !last_contact_.has_value() ||
            !same_contact(*last_contact_, *contact);
        if (contact_changed) {
            const auto moved_between_same_type =
                last_contact_.has_value() &&
                last_contact_->type == contact->type;
            standing_counter_ = moved_between_same_type
                ? changed_hole_counter
                : 1;
        } else if (standing_counter_ != 0xff) {
            ++standing_counter_;
        }
        if (landed_this_tick) {
            standing_counter_ = feather_landing_counter;
        }
        last_contact_ = contact;
        phase_ = PlayerHazardPhase::hole_pull;
        events.entered_hole = !was_pulling;

        const auto target_x =
            static_cast<double>(contact->column *
                content::metatile_world_size) + metatile_center;
        const auto target_y =
            static_cast<double>(contact->row *
                content::metatile_world_size) + metatile_center;
        const auto pull_phase = standing_counter_ & 0x03u;
        switch (pull_phase) {
        case 0:
            apply_retail_hole_pull(player.local_y, target_y);
            break;
        case 1:
            apply_retail_hole_pull(player.local_x, target_x);
            break;
        default:
            break;
        }

        if (
            pull_phase <= 1 &&
            centered(player.local_x) &&
            centered(player.local_y)) {
            player.object_contact_enabled = false;
            phase_ = PlayerHazardPhase::hole_fall;
            // linkState02 substate 0 centers and initializes the animation on
            // the following Link update.
            fall_tick_ = 0xff;
            events.began_fall = true;
        }
        break;
    }
    case PlayerHazardPhase::hole_fall:
        player.object_contact_enabled = false;
        if (fall_tick_ == 0xff) {
            if (last_contact_.has_value()) {
                player.local_x =
                    static_cast<double>(last_contact_->column *
                        content::metatile_world_size) + metatile_center;
                player.local_y =
                    static_cast<double>(last_contact_->row *
                        content::metatile_world_size) + metatile_center;
            }
            player.z_subpixels = 0;
            player.speed_z_subpixels = 0;
            player.in_air = 0;
            fall_tick_ = 0;
            break;
        }
        if (fall_tick_ < fall_animation_ticks) {
            ++fall_tick_;
        }
        if (fall_tick_ >= fall_animation_ticks) {
            player = respawn_point_;
            player.object_contact_enabled = false;
            phase_ = PlayerHazardPhase::hidden_respawn_delay;
            phase_counter_ = hidden_ticks;
            visible_ = false;
            events.became_hidden = true;
            events.respawned = true;
        }
        break;
    case PlayerHazardPhase::water_entry:
    case PlayerHazardPhase::swimming:
        if (
            !contact.has_value() ||
            !is_water(contact->type)) {
            phase_ = PlayerHazardPhase::normal;
            standing_counter_ = 0;
            last_contact_ = contact;
            player.object_contact_enabled = true;
            break;
        }
        last_contact_ = contact;
        if (standing_counter_ != 0xff) {
            ++standing_counter_;
        }
        if (!can_swim(contact->type, water_capability_)) {
            phase_ = PlayerHazardPhase::drowning;
            drown_tick_ = 0;
            player.object_contact_enabled = false;
            combat.invincibility_ticks = water_entry_invincibility_ticks;
            events.began_drowning = true;
            break;
        }
        player.object_contact_enabled = true;
        ++swim_tick_;
        if (phase_ == PlayerHazardPhase::water_entry) {
            if (phase_counter_ != 0) {
                --phase_counter_;
            }
            if (phase_counter_ == 0) {
                phase_ = PlayerHazardPhase::swimming;
            }
        }
        break;
    case PlayerHazardPhase::drowning:
        player.object_contact_enabled = false;
        if (drown_tick_ < drown_animation_ticks) {
            ++drown_tick_;
        }
        if (drown_tick_ >= drown_animation_ticks) {
            player = respawn_point_;
            player.object_contact_enabled = false;
            phase_ = PlayerHazardPhase::hidden_respawn_delay;
            phase_counter_ = hidden_ticks;
            visible_ = false;
            events.became_hidden = true;
            events.respawned = true;
        }
        break;
    case PlayerHazardPhase::hidden_respawn_delay:
        player.object_contact_enabled = false;
        if (phase_counter_ != 0) {
            --phase_counter_;
        }
        if (phase_counter_ == 0) {
            player.z_subpixels = 0;
            player.speed_z_subpixels = 0;
            player.in_air = 0;
            combat.health = combat.health <= hazard_damage
                ? 0
                : static_cast<std::uint8_t>(combat.health - hazard_damage);
            combat.invincibility_ticks = respawn_invincibility_ticks;
            phase_ = PlayerHazardPhase::recovery;
            phase_counter_ = recovery_ticks;
            visible_ = true;
            events.damaged = true;
            events.fatal_damage = combat.health == 0;
        }
        break;
    case PlayerHazardPhase::recovery:
        player.object_contact_enabled = false;
        if (phase_counter_ != 0) {
            --phase_counter_;
        }
        if (phase_counter_ == 0) {
            phase_ = PlayerHazardPhase::normal;
            standing_counter_ = 0;
            last_contact_.reset();
            player.object_contact_enabled = true;
            events.recovered = true;
        }
        break;
    }

    auto current = report();
    current.entered_hole = events.entered_hole;
    current.began_fall = events.began_fall;
    current.entered_water = events.entered_water;
    current.began_swimming = events.began_swimming;
    current.began_drowning = events.began_drowning;
    current.became_hidden = events.became_hidden;
    current.respawned = events.respawned;
    current.damaged = events.damaged;
    current.fatal_damage = events.fatal_damage;
    current.recovered = events.recovered;
    return current;
}

PlayerHazardPhase PlayerHazardRuntime::phase() const noexcept {
    return phase_;
}

bool PlayerHazardRuntime::captures_input() const noexcept {
    return phase_ == PlayerHazardPhase::hole_fall ||
        phase_ == PlayerHazardPhase::drowning ||
        phase_ == PlayerHazardPhase::hidden_respawn_delay ||
        phase_ == PlayerHazardPhase::recovery ||
        (phase_ == PlayerHazardPhase::hole_pull &&
         standing_counter_ >= partial_control_ticks);
}

bool PlayerHazardRuntime::swimming() const noexcept {
    return phase_ == PlayerHazardPhase::water_entry ||
        phase_ == PlayerHazardPhase::swimming;
}

bool PlayerHazardRuntime::visible() const noexcept {
    return visible_;
}

MovementInput PlayerHazardRuntime::movement_input(
    const MovementInput requested,
    const PlayerFacing facing) const noexcept {
    if (phase_ != PlayerHazardPhase::water_entry) {
        return requested;
    }
    switch (facing) {
    case PlayerFacing::north: return MovementInput{.vertical = -1.0};
    case PlayerFacing::east: return MovementInput{.horizontal = 1.0};
    case PlayerFacing::south: return MovementInput{.vertical = 1.0};
    case PlayerFacing::west: return MovementInput{.horizontal = -1.0};
    }
    return {};
}

double PlayerHazardRuntime::movement_speed_pixels_per_second() const noexcept {
    return swimming()
        ? flippers_speed_pixels_per_second
        : normal_speed_pixels_per_second;
}

PlayerWaterCapability PlayerHazardRuntime::water_capability() const noexcept {
    return water_capability_;
}

std::uint64_t PlayerHazardRuntime::deterministic_state() const noexcept {
    return static_cast<std::uint64_t>(phase_) |
        (static_cast<std::uint64_t>(standing_counter_) << 8u) |
        (static_cast<std::uint64_t>(phase_counter_) << 16u) |
        (static_cast<std::uint64_t>(fall_tick_) << 24u) |
        (static_cast<std::uint64_t>(visible_) << 32u) |
        (static_cast<std::uint64_t>(swim_tick_) << 33u) |
        (static_cast<std::uint64_t>(drown_tick_) << 41u) |
        (static_cast<std::uint64_t>(water_capability_) << 49u) |
        (static_cast<std::uint64_t>(campaign_) << 51u);
}

PlayerHazardStepReport PlayerHazardRuntime::report() const noexcept {
    PlayerHazardStepReport result{
        .phase = phase_,
        .standing_on_tile_counter = standing_counter_,
        .captures_input = captures_input(),
        .visible = visible_,
    };
    if (
        phase_ == PlayerHazardPhase::hole_fall &&
        fall_tick_ != 0xff) {
        result.link_frame = fall_frame(fall_tick_);
    } else if (swimming()) {
        result.link_frame = swim_frame(swim_tick_);
    } else if (phase_ == PlayerHazardPhase::drowning) {
        result.link_frame = drown_frame(campaign_, drown_tick_);
    }
    return result;
}

const char* player_hazard_phase_name(
    const PlayerHazardPhase phase) noexcept {
    switch (phase) {
    case PlayerHazardPhase::normal: return "normal";
    case PlayerHazardPhase::hole_pull: return "hole-pull";
    case PlayerHazardPhase::hole_fall: return "hole-fall";
    case PlayerHazardPhase::water_entry: return "water-entry";
    case PlayerHazardPhase::swimming: return "swimming";
    case PlayerHazardPhase::drowning: return "drowning";
    case PlayerHazardPhase::hidden_respawn_delay: return "hidden-respawn";
    case PlayerHazardPhase::recovery: return "recovery";
    }
    return "unknown";
}

}  // namespace oracle::gameplay
