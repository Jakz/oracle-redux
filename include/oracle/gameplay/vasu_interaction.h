#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "oracle/content/rom_text.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/input/input_frame.h"
#include "oracle/script/campaign_script.h"
#include "oracle/ui/dialogue_model.h"

namespace oracle::gameplay {

struct VasuScenarioDefinition {
    core::WorldRoomId room;
    std::uint8_t player_spawn_yx{};
};

[[nodiscard]] VasuScenarioDefinition vasu_scenario(
    core::Campaign campaign) noexcept;

// A bounded first behavior path for INTERAC_VASU. The scenario begins from the
// documented post-ring-box state with no loose or appraised rings, matching
// vasuScript's normal welcome branch. Text is always decoded from the ROM.
class VasuInteractionRuntime {
public:
    explicit VasuInteractionRuntime(const content::RomSource& rom);

    void update(
        const input::InputFrame& input,
        const PlayerState& player,
        core::ActorSlotDomain& actors);

    [[nodiscard]] bool captures_input() const noexcept;
    [[nodiscard]] ui::DialogueModel model() const;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;
    [[nodiscard]] const std::vector<script::ScriptTraceEvent>&
        script_trace() const noexcept;
    [[nodiscard]] const script::ScriptInstance* script_instance()
        const noexcept;
    [[nodiscard]] const script::OriginalStateStore& original_state()
        const noexcept;

private:
    const content::RomSource& rom_;
    script::CampaignScriptRuntime script_;
    std::optional<core::ActorSlotHandle> actor_;
};

}  // namespace oracle::gameplay
