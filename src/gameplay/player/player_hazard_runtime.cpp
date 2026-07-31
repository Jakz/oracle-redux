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
constexpr std::uint8_t hidden_ticks = 0x02;
constexpr std::uint8_t recovery_ticks = 0x10;
constexpr std::uint8_t hole_damage = 0x04;
constexpr std::uint8_t hole_invincibility_ticks = 0x3c;
constexpr double metatile_center = 8.0;

bool same_contact(
    const content::LinkTileContact& left,
    const content::LinkTileContact& right) noexcept {
    return left.room == right.room &&
        left.type == right.type &&
        left.column == right.column &&
        left.row == right.row;
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

}  // namespace

void PlayerHazardRuntime::reset(const PlayerState& respawn_point) noexcept {
    set_respawn_point(respawn_point);
    phase_ = PlayerHazardPhase::normal;
    last_contact_.reset();
    standing_counter_ = 0;
    phase_counter_ = 0;
    fall_tick_ = 0;
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

PlayerHazardStepReport PlayerHazardRuntime::update(
    PlayerState& player,
    PlayerCombatState& combat,
    const std::optional<content::LinkTileContact> contact,
    const bool landed_this_tick) noexcept {
    auto result = report();

    switch (phase_) {
    case PlayerHazardPhase::normal:
    case PlayerHazardPhase::hole_pull: {
        if (
            player.in_air != 0 ||
            !contact.has_value() ||
            contact->type != content::LinkTileType::hole) {
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
        result.entered_hole = !was_pulling;

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
            result.began_fall = true;
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
            result.became_hidden = true;
            result.respawned = true;
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
            combat.health = combat.health <= hole_damage
                ? 0
                : static_cast<std::uint8_t>(combat.health - hole_damage);
            combat.invincibility_ticks = hole_invincibility_ticks;
            phase_ = PlayerHazardPhase::recovery;
            phase_counter_ = recovery_ticks;
            visible_ = true;
            result.damaged = true;
            result.fatal_damage = combat.health == 0;
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
            result.recovered = true;
        }
        break;
    }

    auto current = report();
    current.entered_hole = result.entered_hole;
    current.began_fall = result.began_fall;
    current.became_hidden = result.became_hidden;
    current.respawned = result.respawned;
    current.damaged = result.damaged;
    current.fatal_damage = result.fatal_damage;
    current.recovered = result.recovered;
    return current;
}

PlayerHazardPhase PlayerHazardRuntime::phase() const noexcept {
    return phase_;
}

bool PlayerHazardRuntime::captures_input() const noexcept {
    return phase_ == PlayerHazardPhase::hole_fall ||
        phase_ == PlayerHazardPhase::hidden_respawn_delay ||
        phase_ == PlayerHazardPhase::recovery ||
        (phase_ == PlayerHazardPhase::hole_pull &&
         standing_counter_ >= partial_control_ticks);
}

bool PlayerHazardRuntime::visible() const noexcept {
    return visible_;
}

std::uint64_t PlayerHazardRuntime::deterministic_state() const noexcept {
    return static_cast<std::uint64_t>(phase_) |
        (static_cast<std::uint64_t>(standing_counter_) << 8u) |
        (static_cast<std::uint64_t>(phase_counter_) << 16u) |
        (static_cast<std::uint64_t>(fall_tick_) << 24u) |
        (static_cast<std::uint64_t>(visible_) << 32u);
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
    }
    return result;
}

const char* player_hazard_phase_name(
    const PlayerHazardPhase phase) noexcept {
    switch (phase) {
    case PlayerHazardPhase::normal: return "normal";
    case PlayerHazardPhase::hole_pull: return "hole-pull";
    case PlayerHazardPhase::hole_fall: return "hole-fall";
    case PlayerHazardPhase::hidden_respawn_delay: return "hidden-respawn";
    case PlayerHazardPhase::recovery: return "recovery";
    }
    return "unknown";
}

}  // namespace oracle::gameplay
