# Routine and Subsystem Overlap

## Outcome

The first symbol pass found 1,783 routine names in both US games. This pass maps
those routines back to their reference source modules, conservatively decodes
their reachable instruction bodies from the retail ROMs, and compares three
fingerprints.

Of the 1,783 shared names:

- 1,766 pairs (99.05%) decode completely under the current conservative
  boundary rules.
- 511 pairs (28.94% of complete pairs) have identical decoded instruction
  bytes.
- Another 807 pairs become identical after resolved addresses, relative branch
  offsets, and recognized `callab`/`jpab` banks are normalized.
- The byte-identical and relocation-normalized tiers together cover 1,318
  pairs, or 74.63% of complete shared routines.
- Another 302 pairs have the same opcode shape but differing operands.
- 146 complete pairs differ structurally, and 17 pairs remain incomplete.

Opcode-shape equality reaches 91.73%, but it is only a review signal: ignoring
immediates can hide meaningful constants or state differences. The 74.63%
normalized tier is the safer estimate for routines that should begin from one
shared C++ implementation.

## Source ownership

Source lookup resolves 2,505 of 2,507 Ages routine names and all 2,508 Seasons
routine names.

For the 1,783 shared names:

- 1,744 map to the same common source module in both builds.
- 39 map to campaign-owned modules in each build.
- No shared name maps to a common module on one side and a campaign module on
  the other.

This is stronger than name similarity alone. Nearly every shared routine is
explicitly organized as common implementation in the established reference,
while the smaller campaign-owned set represents parallel concepts that happen
to retain the same name.

## Fingerprint definitions

**Raw fingerprint**:
SHA-256 over reachable instruction bytes ordered by ROM address.

**Relocation-normalized fingerprint**:
Direct 16-bit ROM/RAM operands are replaced with resolved symbol names,
relative-branch displacement is removed, and the canonical
`ld hl,target; ld e,bank; call/jp interBankCall` sequence is normalized to its
far target name.

**Opcode-shape fingerprint**:
Keeps opcodes and CB-prefixed sub-opcodes but removes ordinary immediates. This
is useful for locating likely variants, not for asserting equivalent behavior.

Routine walks follow local conditional/unconditional branches but do not enter
called routines. They stop at returns, indirect jumps, hard stops, another
known routine entry, invalid opcodes, or a 4,096-instruction safety bound.

## Subsystem results

The normalized column includes byte-identical pairs.

| Subsystem | Complete pairs | Normalized equivalent | Opcode shape equal | Different |
| --- | ---: | ---: | ---: | ---: |
| Core banks | 830 | 535 (64.46%) | 743 (89.52%) | 87 |
| Enemies | 286 | 250 (87.41%) | 281 (98.25%) | 5 |
| Special objects | 171 | 135 (78.95%) | 143 (83.63%) | 28 |
| General gameplay modules | 118 | 85 (72.03%) | 116 (98.31%) | 2 |
| Items | 62 | 56 (90.32%) | 60 (96.77%) | 2 |
| Parts/projectiles | 57 | 52 (91.23%) | 54 (94.74%) | 3 |
| Interactions | 58 | 48 (82.76%) | 53 (91.38%) | 5 |
| Audio code | 50 | 50 (100.00%) | 50 (100.00%) | 0 |
| Rooms and tiles | 37 | 31 (83.78%) | 33 (89.19%) | 4 |
| Item parents | 26 | 24 (92.31%) | 24 (92.31%) | 2 |
| Object framework | 24 | 21 (87.50%) | 23 (95.83%) | 1 |

Core-bank code has the most shared routines but also the most genuine
variation. The object families are better early port units: they have narrower
state surfaces and much higher normalized equivalence.

## Recommended first gameplay slice

Start with two adjacent layers of the item system.

### 1. Bank 07 common item primitives

`object_code/common/items/commonCode1.s` contains 20 shared routines and 449
decoded instructions per game. All 20 are equivalent after normalization:

- 5 are byte-identical;
- 15 differ only through resolved/relocated operands;
- none are incomplete or structurally different.

The module covers item attributes and graphics references, animation,
knockback transfer, vertical throwing, gravity/hazard handling, tile
passability, conveyor movement, and child/interactions creation.

This is a good fidelity unit because its contract can be expressed as:

```text
input
  Item state
  Link state needed for knockback
  room/tile collision view
  item attribute/animation tables

output
  changed Item and Link fields
  spawn/delete/hazard events
  animation/OAM reference changes
```

### 2. Bank 06 parent-item glue

`object_code/common/itemParents/commonCode.s` contains 21 shared routines and
roughly 227 decoded instructions:

- 19 are byte-identical or relocation-normalized;
- 2 are intentionally different.

The two differences already identify clean campaign hooks:

1. `checkLinkOnGround` adds Ages-specific underwater-map handling.
2. `parentItemLoadAnimationAndIncState` adds Ages-specific raft/underwater
   animation selection.

That suggests a narrow C++ campaign policy rather than two item implementations:

```cpp
struct ItemCampaignPolicy {
    bool is_link_considered_grounded(const RuntimeState&) const;
    LinkAnimation select_parent_item_animation(
        const RuntimeState&, LinkAnimation base) const;
};
```

The exact API should wait for behavioral traces, but the variation boundary is
now concrete.

## Supporting module candidates

| Reference module | Shared routines | Normalized equivalent | Decoded Ages instructions |
| --- | ---: | ---: | ---: |
| `code/audio.s` | 51 | 50/50 complete | 903 |
| `object_code/common/enemies/commonCode.s` | 59 | 55/59 | 593 |
| `code/textbox.s` | 48 | 45/48 | 1,140 |
| `object_code/common/items/commonCode1.s` | 20 | 20/20 | 449 |
| `object_code/common/itemParents/commonCode.s` | 21 | 19/21 | 227 |
| `object_code/common/parts/commonCode.s` | 10 | 10/10 | 137 |

Audio is nearly exact but is less useful as the first gameplay trace.
Enemy-common code is a strong second gameplay subsystem after item/object state
and collision services exist.

## Generated reports

- `analysis/generated/reference/ages-routines.csv`
- `analysis/generated/reference/seasons-routines.csv`
- `analysis/generated/reference/shared-routines.csv`
- `analysis/generated/reference/subsystem-overlap.csv`
- `analysis/generated/reference/module-overlap.csv`
- `analysis/generated/reference/routine-summary.json`

Regenerate with:

```sh
python tools/routine_catalog.py \
  --reference-dir reference/oracles-disasm \
  --rom-dir roms
```

The next implementation-oriented task is a trace schema for the two candidate
item modules: explicit state inputs, observable state deltas, emitted events,
and golden cross-game cases for the two Ages-only policy branches.

That schema and its initial golden-case matrix are now defined in
[`item-trace-contract.md`](item-trace-contract.md).
