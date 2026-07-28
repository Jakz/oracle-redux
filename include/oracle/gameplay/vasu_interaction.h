#pragma once

#include <cstdint>
#include <optional>

#include "oracle/content/rom_text.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/gameplay/player_traversal.h"
#include "oracle/input/input_frame.h"
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
    void update(
        const input::InputFrame& input,
        const PlayerState& player,
        const core::ActorSlotDomain& actors,
        const content::RomTextDecoder& text);

    [[nodiscard]] bool captures_input() const noexcept;
    [[nodiscard]] ui::DialogueModel model() const;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;

private:
    void open(
        content::MessageId message,
        const content::RomTextDecoder& text);

    std::optional<content::DecodedMessage> message_;
    std::size_t page_index_{};
    std::size_t selected_option_{};
};

}  // namespace oracle::gameplay
