#pragma once

#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>

namespace oracle::room_slice {

enum class ScenarioId {
    latest,
    explore,
    chest,
    vasu,
    octorok,
    hole,
    atlas,
};

struct ScenarioDescriptor {
    ScenarioId id{};
    std::string_view name;
    std::string_view title;
    std::string_view purpose;
};

[[nodiscard]] std::span<const ScenarioDescriptor> scenario_catalog() noexcept;
[[nodiscard]] const ScenarioDescriptor& describe_scenario(
    ScenarioId id) noexcept;
[[nodiscard]] std::optional<ScenarioId> scenario_from_name(
    std::string_view name) noexcept;

void print_scenario_catalog(std::ostream& output);
[[nodiscard]] ScenarioId select_scenario(
    std::istream& input,
    std::ostream& output);

}  // namespace oracle::room_slice
