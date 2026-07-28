#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "oracle/core/world_id.h"

namespace oracle::core {

enum class ActorCategory : std::uint8_t {
    item,
    interaction,
    enemy,
    part,
};

struct ActorIdentity {
    std::uint8_t id{};
    std::uint8_t subid{};
    std::uint8_t parameter{};

    [[nodiscard]] friend bool operator==(
        const ActorIdentity&,
        const ActorIdentity&) = default;
};

struct ActorSlotHandle {
    ActorCategory category{};
    std::uint8_t slot{};
    std::uint32_t generation{};

    [[nodiscard]] friend bool operator==(
        const ActorSlotHandle&,
        const ActorSlotHandle&) = default;
};

struct ActorSlotState {
    ActorIdentity identity;
    WorldRoomId room;
    std::int16_t local_x{};
    std::int16_t local_y{};
    std::size_t source_record_index{};
    std::uint32_t generation{};
    bool active{};
    bool positioned{};
    bool conditional{};
};

// Mirrors the four $dx00-$dxff Game Boy object bands. Every band has 16
// stable slots. Dynamic items use low slots 7-b; dynamic interactions use
// 2-f because slots 0 and 1 retain their original reserved roles.
class ActorSlotDomain {
public:
    static constexpr std::size_t slots_per_category = 16;

    [[nodiscard]] std::optional<ActorSlotHandle> allocate_dynamic(
        ActorCategory category,
        ActorIdentity identity,
        WorldRoomId room,
        std::int16_t local_x,
        std::int16_t local_y,
        bool positioned,
        bool conditional,
        std::size_t source_record_index);

    [[nodiscard]] std::optional<ActorSlotHandle> allocate_at(
        ActorCategory category,
        std::uint8_t slot,
        ActorIdentity identity,
        WorldRoomId room = {});

    [[nodiscard]] ActorSlotState* get(ActorSlotHandle handle) noexcept;
    [[nodiscard]] const ActorSlotState* get(
        ActorSlotHandle handle) const noexcept;
    [[nodiscard]] bool release(ActorSlotHandle handle) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::span<const ActorSlotState> slots(
        ActorCategory category) const noexcept;
    [[nodiscard]] std::size_t active_count(
        ActorCategory category) const noexcept;

private:
    using SlotBand =
        std::array<ActorSlotState, slots_per_category>;

    [[nodiscard]] SlotBand& band(ActorCategory category) noexcept;
    [[nodiscard]] const SlotBand& band(
        ActorCategory category) const noexcept;

    std::array<SlotBand, 4> bands_{};
};

}  // namespace oracle::core
