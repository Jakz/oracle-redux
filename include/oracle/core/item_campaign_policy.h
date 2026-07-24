#pragma once

#include <cstdint>

#include "oracle/core/campaign.h"

namespace oracle::core {

struct ParentItemContext {
    bool link_is_riding{};
    bool link_is_in_air{};
    bool link_is_swimming{};
    bool link_is_underwater{};
    bool companion_is_raft{};
};

class ItemCampaignPolicy {
public:
    explicit constexpr ItemCampaignPolicy(Campaign campaign) noexcept
        : campaign_(campaign) {}

    [[nodiscard]] bool is_link_considered_grounded(
        const ParentItemContext& context) const noexcept;

    [[nodiscard]] std::uint8_t select_parent_item_animation(
        std::uint8_t base_animation,
        const ParentItemContext& context) const noexcept;

    [[nodiscard]] constexpr Campaign campaign() const noexcept {
        return campaign_;
    }

private:
    Campaign campaign_;
};

}  // namespace oracle::core
