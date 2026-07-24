#include "oracle/experience_settings.h"

#include <stdexcept>

namespace oracle {

ExperienceSettings ExperienceSettings::from_preset(
    const ExperiencePreset preset) {
    switch (preset) {
        case ExperiencePreset::classic:
            return ExperienceSettings{};
        case ExperiencePreset::modern:
            return ExperienceSettings{
                .gameplay =
                    GameplaySettings{
                        .simulation_region_mode =
                            SimulationRegionMode::seamless_region,
                        .item_action_slots = 6,
                    },
                .presentation =
                    PresentationSettings{
                        .seamless_world = true,
                        .interpolate_between_logic_ticks = true,
                        .dynamic_lighting = true,
                        .atmospheric_fog = true,
                        .color_grading = true,
                        .pixel_perfect = false,
                    },
            };
    }

    throw std::invalid_argument{"unknown experience preset"};
}

void ExperienceSettings::validate() const {
    constexpr std::uint8_t minimum_item_slots = 2;
    constexpr std::uint8_t maximum_item_slots = 6;
    if (
        gameplay.item_action_slots < minimum_item_slots ||
        gameplay.item_action_slots > maximum_item_slots) {
        throw std::invalid_argument{"item action slots must be between 2 and 6"};
    }
}

}  // namespace oracle
