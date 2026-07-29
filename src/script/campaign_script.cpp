#include "oracle/script/campaign_script.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace oracle::script {
namespace {

constexpr std::size_t maximum_immediate_instructions = 128;
constexpr std::size_t maximum_trace_events = 8192;
constexpr std::uint8_t primary_helper_bank = 0x15;

std::size_t operand_count(const std::uint8_t opcode) {
    switch (opcode) {
    case 0x00:
    case 0x9b:
    case 0x9e:
    case 0xbd:
    case 0xbe:
        return 0;
    case 0x98:
    case 0x9a:
    case 0xc6:
        return 1;
    case 0x8d:
    case 0xe0:
        return 2;
    case 0xb5:
        return 3;
    case 0xc3:
        return 3;
    case 0xcc:
        return 4;
    default:
        throw std::runtime_error{"unsupported campaign-script opcode"};
    }
}

RomScriptAddress advance(
    const RomScriptAddress source,
    const std::size_t bytes) {
    const auto address =
        static_cast<std::size_t>(source.address) + bytes;
    if (address >= 0x8000) {
        throw std::runtime_error{
            "campaign script crosses its bank boundary"};
    }
    return RomScriptAddress{
        source.bank,
        static_cast<std::uint16_t>(address),
    };
}

std::uint16_t little_u16(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset) {
    return static_cast<std::uint16_t>(
        bytes[offset] |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8u));
}

bool pressed_confirm(const input::InputFrame& input) noexcept {
    return
        input.pressed(input::InputAction::a) ||
        input.pressed(input::InputAction::confirm);
}

bool pressed_cancel(const input::InputFrame& input) noexcept {
    return
        input.pressed(input::InputAction::b) ||
        input.pressed(input::InputAction::cancel);
}

std::runtime_error script_error(
    const RomScriptAddress address,
    const std::string& message) {
    std::ostringstream output;
    output
        << message << " at "
        << std::hex
        << static_cast<unsigned int>(address.bank)
        << ':' << address.address;
    return std::runtime_error{output.str()};
}

}  // namespace

CampaignScriptDecoder::CampaignScriptDecoder(
    const content::RomSource& rom)
    : rom_{rom} {}

ScriptInstruction CampaignScriptDecoder::decode(
    const RomScriptAddress source) const {
    const auto file_offset =
        rom_.banked_file_offset(source.bank, source.address);
    const auto first = rom_.read_byte(file_offset);
    if (first > 0 && first < 0x80) {
        const auto low = rom_.read_byte(file_offset + 1);
        return ScriptInstruction{
            .source = source,
            .next = advance(source, 2),
            .opcode = first,
            .compact_jump = true,
            .raw_bytes = {first, low},
        };
    }
    if (first >= 0xfd) {
        throw script_error(
            source,
            "reserved campaign-script opcode");
    }
    std::size_t operands{};
    try {
        operands = operand_count(first);
    } catch (const std::runtime_error&) {
        throw script_error(
            source,
            "unsupported campaign-script opcode");
    }
    std::vector<std::uint8_t> raw(operands + 1);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = rom_.read_byte(file_offset + index);
    }
    return ScriptInstruction{
        .source = source,
        .next = advance(source, raw.size()),
        .opcode = first,
        .raw_bytes = std::move(raw),
    };
}

std::uint16_t CampaignScriptDecoder::read_little_pointer(
    const RomScriptAddress source) const {
    return rom_.read_little_u16(
        rom_.banked_file_offset(source.bank, source.address));
}

RomScriptAddress CampaignScriptProfile::vasu_entry(
    const core::Campaign campaign) noexcept {
    if (campaign == core::Campaign::ages) {
        return RomScriptAddress{0x0c, 0x49de};
    }
    return RomScriptAddress{0x0b, 0x49e2};
}

CampaignScriptRuntime::CampaignScriptRuntime(
    const content::RomSource& rom)
    : rom_{rom},
      decoder_{rom},
      text_decoder_{rom} {
    state_.write(OriginalStateKey::input_enabled, 1);
}

void CampaignScriptRuntime::start(
    const RomScriptAddress entry,
    const core::ActorSlotHandle actor,
    const std::uint8_t high_text_index) {
    instance_ = ScriptInstance{
        .actor = actor,
        .program_counter = entry,
        .high_text_index = high_text_index,
    };
    dialogue_.reset();
    dialogue_page_ = 0;
    selected_option_ = 0;
    trace_.clear();
    tick_ = 0;
}

void CampaignScriptRuntime::tick(
    const input::InputFrame& input,
    const bool actor_received_a_button) {
    if (!instance_.has_value() || instance_->ended) {
        ++tick_;
        return;
    }
    if (dialogue_.has_value()) {
        update_dialogue(input);
        ++tick_;
        return;
    }
    if (instance_->counter != 0) {
        --instance_->counter;
        ++tick_;
        return;
    }
    run_instructions(actor_received_a_button);
    ++tick_;
}

bool CampaignScriptRuntime::captures_input() const noexcept {
    return
        dialogue_.has_value() ||
        state_.read(OriginalStateKey::input_enabled) == 0;
}

ui::DialogueModel CampaignScriptRuntime::dialogue_model() const {
    if (!dialogue_.has_value()) {
        return {};
    }
    ui::DialogueModel model{
        .message = dialogue_->id,
        .page_text = dialogue_->pages[dialogue_page_],
        .page_index = dialogue_page_,
        .page_count = dialogue_->pages.size(),
        .selected_option = selected_option_,
        .visible = true,
    };
    if (
        dialogue_page_ + 1 == dialogue_->pages.size() &&
        !dialogue_->option_labels.empty()) {
        for (std::size_t index = 0;
             index < dialogue_->option_labels.size();
             ++index) {
            model.options.push_back(
                ui::DialogueOption{
                    static_cast<std::uint8_t>(index),
                    dialogue_->option_labels[index],
                });
        }
    }
    return model;
}

const ScriptInstance* CampaignScriptRuntime::instance() const noexcept {
    return instance_.has_value() ? &*instance_ : nullptr;
}

OriginalStateStore& CampaignScriptRuntime::state() noexcept {
    return state_;
}

const OriginalStateStore& CampaignScriptRuntime::state() const noexcept {
    return state_;
}

const std::vector<ScriptTraceEvent>&
CampaignScriptRuntime::trace() const noexcept {
    return trace_;
}

std::uint64_t CampaignScriptRuntime::deterministic_state() const noexcept {
    if (!instance_.has_value()) {
        return 0;
    }
    const auto dialogue_id = dialogue_.has_value() ? dialogue_->id : 0;
    return
        (static_cast<std::uint64_t>(
             instance_->program_counter.bank) << 56u) |
        (static_cast<std::uint64_t>(
             instance_->program_counter.address) << 40u) |
        (static_cast<std::uint64_t>(dialogue_id) << 16u) |
        (static_cast<std::uint64_t>(dialogue_page_) << 8u) |
        static_cast<std::uint64_t>(selected_option_);
}

void CampaignScriptRuntime::run_instructions(
    const bool actor_received_a_button) {
    auto& instance = *instance_;
    for (std::size_t count = 0;
         count < maximum_immediate_instructions;
         ++count) {
        const auto instruction =
            decoder_.decode(instance.program_counter);
        if (instruction.compact_jump) {
            instance.program_counter = RomScriptAddress{
                instruction.source.bank,
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(
                         instruction.raw_bytes[0]) << 8u) |
                    instruction.raw_bytes[1]),
            };
            record(instruction, ScriptStepOutcome::jumped);
            continue;
        }

        instance.program_counter = instruction.next;
        const auto& raw = instruction.raw_bytes;
        switch (instruction.opcode) {
        case 0x00:
            instance.ended = true;
            record(instruction, ScriptStepOutcome::ended);
            return;
        case 0x8d:
            instance.collision_radius_y = raw[1];
            instance.collision_radius_x = raw[2];
            record(instruction, ScriptStepOutcome::continued);
            break;
        case 0x98:
        case 0x9a: {
            const auto message = static_cast<content::MessageId>(
                (static_cast<content::MessageId>(
                     instance.high_text_index) << 8u) |
                raw[1]);
            open_dialogue(message, instruction.opcode == 0x98);
            record(instruction, ScriptStepOutcome::opened_dialogue);
            return;
        }
        case 0x9b:
            instance.a_button_sensitive = true;
            record(instruction, ScriptStepOutcome::continued);
            break;
        case 0x9e:
            if (
                !instance.a_button_sensitive ||
                !actor_received_a_button) {
                instance.program_counter = instruction.source;
                record(instruction, ScriptStepOutcome::waiting);
                return;
            }
            record(instruction, ScriptStepOutcome::continued);
            break;
        case 0xb5: {
            const auto key =
                OriginalStateResolver::global_flag_key(raw[1]);
            if (!key.has_value()) {
                throw script_error(
                    instruction.source,
                    "unmapped global flag");
            }
            if (state_.read(*key) != 0) {
                instance.program_counter = RomScriptAddress{
                    instruction.source.bank,
                    little_u16(raw, 2),
                };
                record(instruction, ScriptStepOutcome::jumped);
            } else {
                record(instruction, ScriptStepOutcome::continued);
            }
            break;
        }
        case 0xbd:
            state_.write(OriginalStateKey::input_enabled, 0);
            record(instruction, ScriptStepOutcome::continued);
            break;
        case 0xbe:
            state_.write(OriginalStateKey::input_enabled, 1);
            record(instruction, ScriptStepOutcome::continued);
            break;
        case 0xc3: {
            const auto selected =
                state_.read(
                    OriginalStateKey::selected_text_option);
            if (selected == raw[1]) {
                instance.program_counter = RomScriptAddress{
                    instruction.source.bank,
                    little_u16(raw, 2),
                };
                record(instruction, ScriptStepOutcome::jumped);
            } else {
                record(instruction, ScriptStepOutcome::continued);
            }
            break;
        }
        case 0xc6: {
            const auto field =
                OriginalStateResolver::actor_field(raw[1]);
            if (!field.has_value()) {
                throw script_error(
                    instruction.source,
                    "unmapped actor jump-table field");
            }
            const auto index =
                state_.read_actor(instance.actor, *field);
            const auto table_entry = advance(
                instruction.next,
                static_cast<std::size_t>(index) * 2);
            instance.program_counter = RomScriptAddress{
                instruction.source.bank,
                decoder_.read_little_pointer(table_entry),
            };
            record(instruction, ScriptStepOutcome::jumped);
            break;
        }
        case 0xcc: {
            const auto field =
                OriginalStateResolver::actor_field(raw[1]);
            if (!field.has_value()) {
                throw script_error(
                    instruction.source,
                    "unmapped actor comparison field");
            }
            if (
                state_.read_actor(instance.actor, *field) ==
                raw[2]) {
                instance.program_counter = RomScriptAddress{
                    instruction.source.bank,
                    little_u16(raw, 3),
                };
                record(instruction, ScriptStepOutcome::jumped);
            } else {
                record(instruction, ScriptStepOutcome::continued);
            }
            break;
        }
        case 0xe0:
            invoke_host(little_u16(raw, 1));
            record(instruction, ScriptStepOutcome::continued);
            break;
        default:
            throw script_error(
                instruction.source,
                "opcode reached without a native handler");
        }
    }
    throw script_error(
        instance.program_counter,
        "campaign script exceeded its immediate-instruction limit");
}

void CampaignScriptRuntime::update_dialogue(
    const input::InputFrame& input) {
    if (!dialogue_.has_value()) {
        return;
    }
    if (dialogue_page_ + 1 < dialogue_->pages.size()) {
        if (pressed_confirm(input)) {
            ++dialogue_page_;
        }
        return;
    }

    const auto option_count = dialogue_->option_labels.size();
    if (option_count != 0) {
        if (
            input.pressed(input::InputAction::up) ||
            input.pressed(input::InputAction::left)) {
            selected_option_ =
                selected_option_ == 0
                ? option_count - 1
                : selected_option_ - 1;
        }
        if (
            input.pressed(input::InputAction::down) ||
            input.pressed(input::InputAction::right)) {
            selected_option_ =
                (selected_option_ + 1) % option_count;
        }
        if (pressed_cancel(input)) {
            selected_option_ = option_count - 1;
            state_.write(
                OriginalStateKey::selected_text_option,
                static_cast<std::uint8_t>(selected_option_));
            dialogue_.reset();
        } else if (pressed_confirm(input)) {
            state_.write(
                OriginalStateKey::selected_text_option,
                static_cast<std::uint8_t>(selected_option_));
            dialogue_.reset();
        }
        return;
    }

    if (
        pressed_confirm(input) ||
        (dialogue_exitable_ && pressed_cancel(input))) {
        dialogue_.reset();
    }
}

void CampaignScriptRuntime::open_dialogue(
    const content::MessageId message,
    const bool exitable) {
    dialogue_ = text_decoder_.decode(message);
    dialogue_page_ = 0;
    selected_option_ = 0;
    dialogue_exitable_ = exitable;
}

void CampaignScriptRuntime::invoke_host(
    const std::uint16_t target) {
    auto& instance = *instance_;
    const auto campaign = rom_.metadata().campaign;
    const auto expected =
        campaign == core::Campaign::ages ? 0x42b2 : 0x496b;
    if (target != expected) {
        throw script_error(
            RomScriptAddress{primary_helper_bank, target},
            "unmapped campaign-script host command");
    }

    constexpr std::array<OriginalStateKey, 3> earned_keys{
        OriginalStateKey::global_1000_enemies_killed,
        OriginalStateKey::global_10000_rupees_collected,
        OriginalStateKey::global_beat_ganon,
    };
    constexpr std::array<OriginalStateKey, 3> obtained_keys{
        OriginalStateKey::global_got_slayers_ring,
        OriginalStateKey::global_got_wealth_ring,
        OriginalStateKey::global_got_victory_ring,
    };
    for (std::size_t index = 0; index < earned_keys.size(); ++index) {
        if (
            state_.read(earned_keys[index]) == 0 ||
            state_.read(obtained_keys[index]) != 0) {
            continue;
        }
        state_.write(obtained_keys[index], 1);
        state_.write_actor(
            instance.actor,
            OriginalActorField::var3a,
            static_cast<std::uint8_t>(0x30 + index));
        state_.write_actor(
            instance.actor,
            OriginalActorField::var3b,
            static_cast<std::uint8_t>(index));
        return;
    }
    state_.write_actor(
        instance.actor,
        OriginalActorField::var3b,
        3);
}

void CampaignScriptRuntime::record(
    const ScriptInstruction& instruction,
    const ScriptStepOutcome outcome) {
    if (trace_.size() >= maximum_trace_events) {
        throw std::runtime_error{
            "campaign script trace exceeds its safety limit"};
    }
    trace_.push_back(
        ScriptTraceEvent{
            tick_,
            instruction.source,
            instruction.opcode,
            outcome,
        });
}

}  // namespace oracle::script
