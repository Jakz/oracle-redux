#include "oracle/gameplay/vasu_interaction.h"

#include <array>

#include "oracle/gameplay/interaction_target.h"

namespace oracle::gameplay {
namespace {

constexpr std::uint8_t vasu_interaction_id = 0x89;
constexpr content::MessageId vasu_welcome_message = 0x3003;
constexpr content::MessageId vasu_no_unappraised_message = 0x3014;
constexpr content::MessageId vasu_no_appraised_message = 0x3015;
constexpr content::MessageId vasu_parting_message = 0x3008;

bool pressed_confirm(const input::InputFrame& frame) noexcept {
    return
        frame.pressed(input::InputAction::a) ||
        frame.pressed(input::InputAction::confirm);
}

bool pressed_cancel(const input::InputFrame& frame) noexcept {
    return
        frame.pressed(input::InputAction::b) ||
        frame.pressed(input::InputAction::cancel);
}

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

void VasuInteractionRuntime::update(
    const input::InputFrame& input,
    const PlayerState& player,
    const core::ActorSlotDomain& actors,
    const content::RomTextDecoder& text) {
    if (!message_.has_value()) {
        if (!pressed_confirm(input)) {
            return;
        }
        const auto target =
            InteractionTargetFinder::find(player, actors);
        if (!target.has_value()) {
            return;
        }
        const auto* actor = actors.get(*target);
        if (
            actor == nullptr ||
            actor->identity.id != vasu_interaction_id ||
            actor->identity.subid != 0) {
            return;
        }
        open(vasu_welcome_message, text);
        return;
    }

    if (page_index_ + 1 < message_->pages.size()) {
        if (pressed_confirm(input)) {
            ++page_index_;
        }
        return;
    }

    if (message_->id == vasu_welcome_message) {
        if (
            input.pressed(input::InputAction::up) ||
            input.pressed(input::InputAction::left)) {
            selected_option_ =
                selected_option_ == 0 ? 2 : selected_option_ - 1;
        }
        if (
            input.pressed(input::InputAction::down) ||
            input.pressed(input::InputAction::right)) {
            selected_option_ = (selected_option_ + 1) % 3;
        }
        if (pressed_cancel(input)) {
            selected_option_ = 2;
            open(vasu_parting_message, text);
        } else if (pressed_confirm(input)) {
            constexpr std::array<content::MessageId, 3> responses{
                vasu_no_unappraised_message,
                vasu_no_appraised_message,
                vasu_parting_message,
            };
            open(responses[selected_option_], text);
        }
        return;
    }

    if (pressed_confirm(input) || pressed_cancel(input)) {
        message_.reset();
        page_index_ = 0;
        selected_option_ = 0;
    }
}

bool VasuInteractionRuntime::captures_input() const noexcept {
    return message_.has_value();
}

ui::DialogueModel VasuInteractionRuntime::model() const {
    if (!message_.has_value()) {
        return ui::DialogueModel{};
    }
    ui::DialogueModel result{
        .message = message_->id,
        .page_text = message_->pages[page_index_],
        .page_index = page_index_,
        .page_count = message_->pages.size(),
        .selected_option = selected_option_,
        .visible = true,
    };
    if (
        message_->id == vasu_welcome_message &&
        page_index_ + 1 == message_->pages.size()) {
        result.options = {
            ui::DialogueOption{0, "Appraise"},
            ui::DialogueOption{1, "List"},
            ui::DialogueOption{2, "Quit"},
        };
    }
    return result;
}

std::uint64_t VasuInteractionRuntime::deterministic_state() const noexcept {
    if (!message_.has_value()) {
        return 0;
    }
    return
        (static_cast<std::uint64_t>(message_->id) << 32u) |
        (static_cast<std::uint64_t>(page_index_) << 16u) |
        static_cast<std::uint64_t>(selected_option_);
}

void VasuInteractionRuntime::open(
    const content::MessageId message,
    const content::RomTextDecoder& text) {
    message_ = text.decode(message);
    page_index_ = 0;
    selected_option_ = 0;
}

}  // namespace oracle::gameplay
