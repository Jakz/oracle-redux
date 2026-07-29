#include "oracle/gameplay/vasu_interaction.h"

#include <cstddef>

#include "oracle/gameplay/interaction_target.h"

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t vasu_interaction_id = 0x89;
}  // namespace

VasuScenarioDefinition vasu_scenario(
    const core::Campaign campaign) noexcept {
    if (campaign == core::Campaign::ages) {
        return VasuScenarioDefinition{
            core::WorldRoomId{2, 0xee},
            0x35,
        };
    }
    return VasuScenarioDefinition{
        core::WorldRoomId{1, 0x91},
        0x35,
    };
}

VasuInteractionRuntime::VasuInteractionRuntime(
    const content::RomSource& rom)
    : rom_{rom}, script_{rom} {}

void VasuInteractionRuntime::update(
    const input::InputFrame& input,
    const PlayerState& player,
    const core::ActorSlotDomain& actors) {
    if (!actor_.has_value() || actors.get(*actor_) == nullptr) {
        actor_.reset();
        const auto slots =
            actors.slots(core::ActorCategory::interaction);
        for (std::size_t slot = 0; slot < slots.size(); ++slot) {
            const auto& candidate = slots[slot];
            if (
                candidate.active &&
                candidate.identity.id == vasu_interaction_id &&
                candidate.identity.subid == 0) {
                actor_ = core::ActorSlotHandle{
                    core::ActorCategory::interaction,
                    static_cast<std::uint8_t>(slot),
                    candidate.generation,
                };
                break;
            }
        }
        if (!actor_.has_value()) {
            return;
        }
        script_.state().write(
            script::OriginalStateKey::global_obtained_ring_box,
            1);
        script_.start(
            script::CampaignScriptProfile::vasu_entry(
                rom_.metadata().campaign),
            *actor_,
            0x30);
    }

    const auto target =
        InteractionTargetFinder::find(player, actors);
    const bool received_a =
        target.has_value() &&
        *target == *actor_ &&
        (
            input.pressed(input::InputAction::a) ||
            input.pressed(input::InputAction::confirm));
    script_.tick(input, received_a);
}

bool VasuInteractionRuntime::captures_input() const noexcept {
    return script_.captures_input();
}

ui::DialogueModel VasuInteractionRuntime::model() const {
    return script_.dialogue_model();
}

std::uint64_t VasuInteractionRuntime::deterministic_state() const noexcept {
    return script_.deterministic_state();
}

const std::vector<script::ScriptTraceEvent>&
VasuInteractionRuntime::script_trace() const noexcept {
    return script_.trace();
}

const script::ScriptInstance*
VasuInteractionRuntime::script_instance() const noexcept {
    return script_.instance();
}

const script::OriginalStateStore&
VasuInteractionRuntime::original_state() const noexcept {
    return script_.state();
}

}  // namespace oracle::gameplay
