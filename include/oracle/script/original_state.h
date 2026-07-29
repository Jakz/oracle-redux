#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "oracle/core/actor_slot_domain.h"
#include "oracle/core/campaign.h"

namespace oracle::script {

enum class OriginalStateKey : std::uint8_t {
    selected_text_option,
    input_enabled,
    is_linked_game,
    obtained_ring_box,
    global_1000_enemies_killed,
    global_10000_rupees_collected,
    global_beat_ganon,
    global_got_slayers_ring,
    global_got_wealth_ring,
    global_got_victory_ring,
    global_obtained_ring_box,
    global_appraised_hundredth_ring,
    count,
};

enum class OriginalActorField : std::uint8_t {
    var36,
    var37,
    var38,
    var3a,
    var3b,
    count,
};

class OriginalStateResolver {
public:
    [[nodiscard]] static std::optional<OriginalStateKey> memory_key(
        core::Campaign campaign,
        std::uint16_t original_address) noexcept;

    [[nodiscard]] static std::optional<OriginalStateKey> global_flag_key(
        std::uint8_t original_flag) noexcept;

    [[nodiscard]] static std::optional<OriginalActorField> actor_field(
        std::uint8_t original_low_address) noexcept;
};

class OriginalStateStore {
public:
    [[nodiscard]] std::uint8_t read(OriginalStateKey key) const noexcept;
    void write(OriginalStateKey key, std::uint8_t value) noexcept;

    [[nodiscard]] std::uint8_t read_actor(
        core::ActorSlotHandle actor,
        OriginalActorField field) const noexcept;
    void write_actor(
        core::ActorSlotHandle actor,
        OriginalActorField field,
        std::uint8_t value);

private:
    struct ActorFields {
        core::ActorSlotHandle actor;
        std::array<
            std::uint8_t,
            static_cast<std::size_t>(OriginalActorField::count)> values{};
    };

    [[nodiscard]] ActorFields* find_actor(
        core::ActorSlotHandle actor) noexcept;
    [[nodiscard]] const ActorFields* find_actor(
        core::ActorSlotHandle actor) const noexcept;

    std::array<
        std::uint8_t,
        static_cast<std::size_t>(OriginalStateKey::count)> values_{};
    std::vector<ActorFields> actor_values_;
};

}  // namespace oracle::script
