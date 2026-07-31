#include "scenario_catalog.h"

#include <array>
#include <charconv>
#include <istream>
#include <ostream>
#include <string>

namespace oracle::room_slice {
namespace {

constexpr std::array scenarios{
    ScenarioDescriptor{
        ScenarioId::latest,
        "latest",
        "Latest integrated slice",
        "Current recommended playable checkpoint (currently the chest room).",
    },
    ScenarioDescriptor{
        ScenarioId::explore,
        "explore",
        "Widescreen exploration",
        "Free movement, room seams, warps, Feather, sword, and live hazards.",
    },
    ScenarioDescriptor{
        ScenarioId::chest,
        "chest",
        "ROM-defined chest",
        "Open the persistent 30-rupee chest with Z or Enter.",
    },
    ScenarioDescriptor{
        ScenarioId::vasu,
        "vasu",
        "Vasu interaction",
        "Walk to Vasu and exercise native dialogue/script behavior.",
    },
    ScenarioDescriptor{
        ScenarioId::octorok,
        "octorok",
        "Octorok combat",
        "Exercise movement, projectiles, contact, sword, defeat, and drops.",
    },
    ScenarioDescriptor{
        ScenarioId::hole,
        "hole",
        "Ordinary hole lifecycle",
        "Spawn beside a ROM-derived hole and walk into it to test respawn.",
    },
    ScenarioDescriptor{
        ScenarioId::atlas,
        "atlas",
        "Whole-world atlas",
        "Inspect the complete small-room address space with viewport culling.",
    },
};

}  // namespace

std::span<const ScenarioDescriptor> scenario_catalog() noexcept {
    return scenarios;
}

const ScenarioDescriptor& describe_scenario(const ScenarioId id) noexcept {
    for (const auto& scenario : scenarios) {
        if (scenario.id == id) {
            return scenario;
        }
    }
    return scenarios.front();
}

std::optional<ScenarioId> scenario_from_name(
    const std::string_view name) noexcept {
    for (const auto& scenario : scenarios) {
        if (scenario.name == name) {
            return scenario.id;
        }
    }
    return std::nullopt;
}

ScenarioId cycle_scenario(
    const ScenarioId current,
    const int direction) noexcept {
    std::size_t current_index = 0;
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        if (scenarios[index].id == current) {
            current_index = index;
            break;
        }
    }
    const auto count = static_cast<int>(scenarios.size());
    const auto next =
        (static_cast<int>(current_index) + direction % count + count) % count;
    return scenarios[static_cast<std::size_t>(next)].id;
}

void print_scenario_catalog(std::ostream& output) {
    output << "Available Oracle Redux scenarios:\n";
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        const auto& scenario = scenarios[index];
        output
            << "  " << index + 1 << ". " << scenario.name
            << " - " << scenario.title << '\n'
            << "     " << scenario.purpose << '\n';
    }
}

ScenarioId select_scenario(std::istream& input, std::ostream& output) {
    print_scenario_catalog(output);
    output
        << "Select a scenario by number or name "
           "[1/latest]: "
        << std::flush;

    std::string selection;
    if (!std::getline(input, selection) || selection.empty()) {
        output << "Using latest.\n";
        return ScenarioId::latest;
    }
    if (const auto named = scenario_from_name(selection); named.has_value()) {
        return *named;
    }

    std::size_t number = 0;
    const auto result = std::from_chars(
        selection.data(),
        selection.data() + selection.size(),
        number);
    if (
        result.ec == std::errc{} &&
        result.ptr == selection.data() + selection.size() &&
        number >= 1 &&
        number <= scenarios.size()) {
        return scenarios[number - 1].id;
    }

    output << "Unknown selection; using latest.\n";
    return ScenarioId::latest;
}

}  // namespace oracle::room_slice
