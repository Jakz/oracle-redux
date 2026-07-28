#include "oracle/gameplay/room_actor_loader.h"

#include <cstdint>

namespace oracle::gameplay {
namespace {

core::ActorCategory actor_category(
    const content::RoomObjectKind kind) noexcept {
    switch (kind) {
    case content::RoomObjectKind::interaction:
        return core::ActorCategory::interaction;
    case content::RoomObjectKind::enemy:
        return core::ActorCategory::enemy;
    case content::RoomObjectKind::part:
        return core::ActorCategory::part;
    case content::RoomObjectKind::item_drop:
        return core::ActorCategory::item;
    }
    return core::ActorCategory::interaction;
}

}  // namespace

RoomActorLoadReport RoomActorLoader::load(
    const content::RoomObjectCatalog& catalog,
    core::ActorSlotDomain& actors) {
    RoomActorLoadReport report;
    report.spawned.reserve(catalog.records.size());
    for (std::size_t index = 0; index < catalog.records.size(); ++index) {
        const auto& record = catalog.records[index];
        const auto handle = actors.allocate_dynamic(
            actor_category(record.kind),
            core::ActorIdentity{
                record.id,
                record.subid,
                record.parameter,
            },
            catalog.room,
            static_cast<std::int16_t>(record.original_x),
            static_cast<std::int16_t>(
                record.positioned
                    ? static_cast<int>(record.original_y) - 16
                    : 0),
            record.positioned,
            record.conditional,
            index);
        if (handle.has_value()) {
            report.spawned.push_back(*handle);
        } else {
            report.failures.push_back(
                RoomActorLoadFailure{index, record.kind});
        }
    }
    return report;
}

}  // namespace oracle::gameplay
