# Item-System Trace Contract

## Purpose

The first C++ slice should be translated against observed behavior, not merely
against equivalent-looking assembly. This contract defines what to capture at
routine entry and exit for the selected bank-07 item primitives and bank-06
parent-item glue.

The machine-readable observation format is
[`specs/traces/oracle-routine-trace.schema.json`](../../specs/traces/oracle-routine-trace.schema.json).
It distinguishes planned, captured, and verified observations so planned cases
cannot be mistaken for evidence.

Three validated starter plans are under `specs/traces/plans/`:

- `item-set-var3c-to-ff.json`
- `check-link-on-ground-underwater.json`
- `parent-item-animation-underwater.json`

Validate trace files with:

```sh
python tools/trace_contract.py specs/traces/plans/*.json
```

## Canonical state vocabulary

Trace state keys should describe meaning rather than raw addresses:

| Trace prefix | Meaning |
| --- | --- |
| `item.*` | Active item fields (`id`, `state`, position, speed, damage, animation, related objects) |
| `link.*` | Link state touched by the routine |
| `room.*` | Room flags, tileset mode, and collision-query context |
| `runtime.*` | Object counts, active slots, input, and global control flags |
| `table.*` | Stable identifiers for attribute, animation, OAM, or collision-table entries |

Raw memory regions may accompany canonical state for auditability, but
cross-game comparison is made on named state deltas and events. A moved WRAM or
ROM address must not create a false behavioral difference.

## Captured outputs

Every verified observation records:

- output registers and flags that are part of the routine contract;
- named state deltas, including unchanged values when an unchanged result is
  semantically important;
- ordered emitted events;
- the exact campaign ROM SHA-256 and routine `BB:AAAA` location.

The initial event vocabulary is:

- `animation`
- `collision-query`
- `delete`
- `graphics-reference`
- `hazard`
- `sound`
- `spawn`
- `tile-query`

These become interfaces in the deterministic runtime. SDL rendering and audio
playback are consumers of events, not part of the trace.

## Golden case matrix

### Common item primitives

| Routine | Required cases | Expected relation |
| --- | --- | --- |
| `itemSetVar3cToFF` | zero and nonzero initial `var3c` | Identical |
| `itemUpdateDamageToApply` | no damage, ordinary damage, underflow/carry | Identical |
| `itemTransferKnockbackToLink` | zero counter, weaker item knockback, stronger item knockback | Identical |
| `itemSetAnimation` / `itemNextAnimationFrame` | ordinary frame, `$ff` loop marker, parameter update | Identical |
| `itemMergeZPositionIfSidescrollingArea` | top-down and side-scrolling tilesets | Identical |
| `itemUpdateSpeedZAndCheckHazards` | grounded, airborne, water, lava, and hole | Identical |
| `itemUpdateThrowingVertically` | rising, apex, landing, and hazard landing | Identical |
| `itemCheckCanPassSolidTileAt` | open, solid, one-way, and hole-permitted collision | Identical |
| `itemUpdateConveyorBelt` | no conveyor and each conveyor direction | Identical |
| `itemCreateChildWithID` | free slot, object cap, existing related object, allocation failure | Identical |

### Parent-item glue

| Routine | Required cases | Expected relation |
| --- | --- | --- |
| `checkNoOtherParentItemsInUse` | none, self only, another active parent | Identical |
| `getFreeItemSlotWithObjectCap` | free slot, full slots, below/at cap | Identical |
| movement/turning enable/disable helpers | each flag initially clear and set | Identical |
| `itemCreateChildWithID` | successful child link and failure cleanup | Identical |
| `checkLinkOnGround` | normal ground, airborne, swimming, companion, Ages underwater | Campaign policy |
| `parentItemLoadAnimationAndIncState` | on foot, mounted, Ages raft, Ages underwater | Campaign policy |

## Campaign policy boundary

Only two routines in `object_code/common/itemParents/commonCode.s` differ
structurally:

### `checkLinkOnGround`

Seasons checks the ordinary Link-object, in-air, and swimming state. Ages adds
an underwater-map test. The trace must establish the exact truth table for
`w1Link.var2f` bit 7 and companion/raft state.

### `parentItemLoadAnimationAndIncState`

Ages adds:

- a raft exception;
- underwater detection;
- replacement of `LINK_ANIM_MODE_22` with `LINK_ANIM_MODE_2d`.

The common implementation should calculate the base animation and delegate only
this selection rule to campaign policy.

## Comparison rules

For `expected_relation: identical`:

1. Canonical entry state must be equivalent.
2. Ordered state deltas must match by target, before value, and after value.
3. Ordered events and payloads must match.
4. Contract registers/flags must match.
5. Raw addresses, PCs, stack addresses, and bank numbers may differ when their
   named targets are equivalent.

For `expected_relation: campaign-policy`, the trace must name `policy_hook` and
document the allowed output differences. Everything outside that hook remains
subject to identical comparison.

## Capture sequence

1. Set a breakpoint at the routine’s symbol address in each exact US ROM.
2. Record registers and the named memory regions used by the routine.
3. Step until the routine returns or performs a tail jump.
4. Record changed memory plus spawn/delete, sound, animation, graphics, hazard,
   tile, and collision interactions.
5. Convert raw addresses to linker symbol names.
6. Save as `captured`; independently replay or recapture before marking
   `verified`.
7. Use the verified canonical input/output pair as a C++ golden test.

The first minimal trace should be `itemSetVar3cToFF`, followed by
`itemUpdateDamageToApply`, then `itemTransferKnockbackToLink`. They require no
tile or object-allocation harness and establish the trace pipeline before the
more connected movement/hazard cases.
