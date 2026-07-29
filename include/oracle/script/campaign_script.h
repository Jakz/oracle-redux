#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "oracle/content/rom_source.h"
#include "oracle/content/rom_text.h"
#include "oracle/core/actor_slot_domain.h"
#include "oracle/input/input_frame.h"
#include "oracle/script/original_state.h"
#include "oracle/ui/dialogue_model.h"

namespace oracle::script {

struct RomScriptAddress {
    std::uint8_t bank{};
    std::uint16_t address{};

    [[nodiscard]] friend bool operator==(
        const RomScriptAddress&,
        const RomScriptAddress&) = default;
};

struct ScriptInstruction {
    RomScriptAddress source;
    RomScriptAddress next;
    std::uint8_t opcode{};
    bool compact_jump{};
    std::vector<std::uint8_t> raw_bytes;
};

class CampaignScriptDecoder {
public:
    explicit CampaignScriptDecoder(const content::RomSource& rom);

    [[nodiscard]] ScriptInstruction decode(
        RomScriptAddress source) const;
    [[nodiscard]] std::uint16_t read_little_pointer(
        RomScriptAddress source) const;

private:
    const content::RomSource& rom_;
};

enum class ScriptStepOutcome : std::uint8_t {
    continued,
    jumped,
    waiting,
    opened_dialogue,
    ended,
};

struct ScriptTraceEvent {
    std::uint64_t tick{};
    RomScriptAddress source;
    std::uint8_t opcode{};
    ScriptStepOutcome outcome{};

    [[nodiscard]] friend bool operator==(
        const ScriptTraceEvent&,
        const ScriptTraceEvent&) = default;
};

struct ScriptInstance {
    core::ActorSlotHandle actor;
    RomScriptAddress program_counter;
    std::optional<RomScriptAddress> return_address;
    std::uint8_t high_text_index{};
    std::uint8_t collision_radius_y{};
    std::uint8_t collision_radius_x{};
    std::uint16_t counter{};
    bool a_button_sensitive{};
    bool ended{};
};

class CampaignScriptProfile {
public:
    [[nodiscard]] static RomScriptAddress vasu_entry(
        core::Campaign campaign) noexcept;
};

// Executes validated cartridge script bytes as typed native operations. This
// initial command surface is deliberately bounded to the shared Vasu route;
// any other reachable opcode or host target fails with its ROM coordinate.
class CampaignScriptRuntime {
public:
    explicit CampaignScriptRuntime(const content::RomSource& rom);

    void start(
        RomScriptAddress entry,
        core::ActorSlotHandle actor,
        std::uint8_t high_text_index);
    void tick(
        const input::InputFrame& input,
        bool actor_received_a_button);

    [[nodiscard]] bool captures_input() const noexcept;
    [[nodiscard]] ui::DialogueModel dialogue_model() const;
    [[nodiscard]] const ScriptInstance* instance() const noexcept;
    [[nodiscard]] OriginalStateStore& state() noexcept;
    [[nodiscard]] const OriginalStateStore& state() const noexcept;
    [[nodiscard]] const std::vector<ScriptTraceEvent>& trace() const noexcept;
    [[nodiscard]] std::uint64_t deterministic_state() const noexcept;

private:
    void run_instructions(bool actor_received_a_button);
    void update_dialogue(const input::InputFrame& input);
    void open_dialogue(content::MessageId message, bool exitable);
    void invoke_host(std::uint16_t target);
    void record(
        const ScriptInstruction& instruction,
        ScriptStepOutcome outcome);

    const content::RomSource& rom_;
    CampaignScriptDecoder decoder_;
    content::RomTextDecoder text_decoder_;
    OriginalStateStore state_;
    std::optional<ScriptInstance> instance_;
    std::optional<content::DecodedMessage> dialogue_;
    std::size_t dialogue_page_{};
    std::size_t selected_option_{};
    bool dialogue_exitable_{};
    std::uint64_t tick_{};
    std::vector<ScriptTraceEvent> trace_;
};

}  // namespace oracle::script
