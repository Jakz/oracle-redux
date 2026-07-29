# Actor collision and NPC solidity

## Retail behavior

Ordinary NPC solidity is an actor-to-actor collision path, separate from
Link's collision against room metatiles. The relevant shared routines in
`oracles-disasm/code/bank0.s` are:

- `checkObjectsCollidedFromVariables`;
- `preventObjectHFromPassingObjectD`;
- `objectPreventLinkFromPassing`;
- `interactionPushLinkAwayAndUpdateDrawPriority`.

Link initializes with collision radii Y=`$06`, X=`$06` in
`object_code/common/specialObjects/link.s`. The shared Vasu script executes:

```text
8d 12 06
```

so Vasu's actor radii are Y=`$12`, X=`$06`. These bytes are decoded from each
player-supplied cartridge rather than duplicated as Vasu constants.

Two actors overlap when the absolute distance on both axes is strictly less
than the sum of their radii. Touching the exact boundary is not an overlap.
For Link and Vasu this produces combined extents of:

| Axis | Link | Vasu | Separation |
| --- | ---: | ---: | ---: |
| Y | 6 | 18 | 24 pixels |
| X | 6 | 6 | 12 pixels |

`preventObjectHFromPassingObjectD` computes penetration on both axes and locks
Link to the boundary on the shallower axis. Equal penetration resolves
horizontally. This is important: merely rejecting a candidate position would
leave Link trapped if an actor became solid while already overlapping him.

## Native boundary

`ActorSlotState` owns authoritative collision radii and the explicit
`blocks_player` policy. `collect_actor_collision_bodies` snapshots active
bodies in original object-category and slot order. Its fixed 64-entry capacity
matches the four original 16-slot object bands and requires no per-tick heap
allocation. The traversal runtime then:

1. resolves any existing overlap using the retail shallower-axis rule;
2. advances movement in one-pixel-or-smaller substeps;
3. checks horizontal and vertical movement separately, preserving wall slide;
4. tests actor bodies in world coordinates, including adjacent small rooms.

The actor snapshot is backend-neutral. Sprite rectangles, transparent pixels,
render scale, visual height, and future 2.5D meshes cannot affect gameplay
collision.

Link deliberately retains two shapes:

- the existing 4-by-3-pixel half-extents sample the ground-facing foot box
  against metatile collision;
- the original 6-by-6 actor radii interact with NPC and later enemy bodies.

Combining those shapes would make either narrow tile passages or NPC contact
incorrect.

## Current coverage

The Vasu scenario adapter marks Vasu as player-blocking and synchronizes the
radii produced by the generic Campaign Script Runtime. Both US cartridges are
tested for the same `$12/$06` result, exact 12-pixel horizontal separation,
continued interaction targeting at the boundary, deterministic overlap
resolution, and diagonal sliding.

Still unmapped:

- `wLinkCanPassNpcs` and temporary pass-through states;
- light collision from `objectPushLinkAwayOnCollision`;
- thrown Dimitri's NPC collision;
- moving actor displacement and special interaction-specific solidity;
- enemy damage collision, which is a distinct system.

Those cases must become typed policies or actor state as their object families
enter the playable slice. They must not be inferred from presentation data.
