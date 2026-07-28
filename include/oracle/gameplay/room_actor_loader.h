#pragma once

#include <cstddef>
#include <vector>

#include "oracle/content/room_objects.h"
#include "oracle/core/actor_slot_domain.h"

namespace oracle::gameplay {

struct RoomActorLoadFailure {
    std::size_t source_record_index{};
    content::RoomObjectKind kind{};
};

struct RoomActorLoadReport {
    std::vector<core::ActorSlotHandle> spawned;
    std::vector<RoomActorLoadFailure> failures;
};

class RoomActorLoader {
public:
    [[nodiscard]] static RoomActorLoadReport load(
        const content::RoomObjectCatalog& catalog,
        core::ActorSlotDomain& actors);
};

}  // namespace oracle::gameplay
