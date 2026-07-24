#pragma once

#include <cstdint>

namespace oracle {

enum class ExperiencePreset : std::uint8_t {
    classic,
    modern,
};

enum class SimulationRegionMode : std::uint8_t {
    classic_room,
    seamless_region,
};

struct GameplaySettings {
    SimulationRegionMode simulation_region_mode{
        SimulationRegionMode::classic_room};
    std::uint8_t item_action_slots{2};
};

struct PresentationSettings {
    bool seamless_world{false};
    bool interpolate_between_logic_ticks{false};
    bool dynamic_lighting{false};
    bool atmospheric_fog{false};
    bool color_grading{false};
    bool pixel_perfect{true};
};

struct ExperienceSettings {
    GameplaySettings gameplay;
    PresentationSettings presentation;

    [[nodiscard]] static ExperienceSettings from_preset(
        ExperiencePreset preset);
    void validate() const;
};

}  // namespace oracle
