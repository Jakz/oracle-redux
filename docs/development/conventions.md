# Development and source-tree conventions

## Repository roles

```text
apps/                    executable entry points
  room_slice/            SDL room-slice application
include/oracle/          stable public C++ API paths
src/                     native implementation, grouped by responsibility
tests/cpp/runtime/       native integration/runtime suite
tests/                   Python structure and tooling tests
docs/architecture/       durable engine decisions and subsystem designs
docs/reverse-engineering/ROM evidence, formats, addresses, and fidelity bounds
projects/msvc/           hand-maintained Visual Studio solution and filters
reference/               ignored or separately sourced external references
tools/                   analysis and trace tooling
```

Public headers intentionally remain at stable paths such as
`oracle/content/room_layout.h`. Implementation depth does not leak into API
include names.

## Implementation placement

| Tree | Responsibility |
| --- | --- |
| `src/content/rom` | validated ROM access, text, and low-level cartridge formats |
| `src/content/world` | room layouts, rendering data, mutation, collision, terrain types, topology |
| `src/content/actors` | ROM actor/treasure/part definitions |
| `src/content/sprites` | ROM OAM and sprite composition decoders |
| `src/core/actors` | bounded actor-domain primitives |
| `src/core/items` | campaign-neutral item state and policy primitives |
| `src/core/world` | simulation-region primitives |
| `src/gameplay/actors` | shared actor loading and contact/collision services |
| `src/gameplay/combat` | native enemy and combat item state machines |
| `src/gameplay/interactions` | interaction targeting and scripted NPC behavior |
| `src/gameplay/items` | native inventory/secondary-item/chest behavior |
| `src/gameplay/player` | authoritative Link traversal and player state |
| `src/presentation/camera` | view transforms and visible-world calculations |
| `src/presentation/render` | backend-neutral render plans |
| `src/presentation/timing` | deterministic-to-presented frame timing |
| `src/script/runtime` | native campaign-script execution |
| `src/script/state` | typed mappings of original state |

Create a new responsibility folder only when at least two files share a clear
role or an upcoming slice establishes a durable subsystem. Avoid generic
`misc`, `common`, or `utils` folders.

## C++ rules

- C++20, warning level 4, and conforming mode are the shared baseline.
- All public and private C++ headers use `.h`, including C++-only headers.
- Namespaces follow the public subsystem (`oracle::content`,
  `oracle::gameplay`, and so on), not the physical implementation depth.
- Gameplay state and ROM-derived content remain backend-neutral. SDL and GPU
  objects stay in application/backend code.
- Presentation may interpolate or enhance snapshots but never feeds lighting,
  camera, or frame-rate state back into authoritative simulation.
- Preserve original integer widths, fixed-point semantics, bounded slots,
  update order, and campaign-specific branches where observable behavior
  depends on them.
- Decode copyrighted content from the player's ROM; do not add extracted
  graphics, text, music, or mechanically translated Nintendo code.

## Build-file rules

- CMake lists engine implementations explicitly so ownership changes are
  reviewable.
- MSVC projects use recursive `src`, `include`, and native-test globs so a new
  file builds immediately.
- Each MSVC `.vcxproj.filters` file mirrors physical implementation folders and
  logical public-header responsibilities. Update filters when adding a new
  durable folder.
- The solution keeps application, test, dependency, and project-guidance
  groups distinct.

## Mandatory slice workflow

1. Read `PROJECT_STATUS.md` and relevant design/evidence documents.
2. Define one observable, bounded checkpoint and its deferred boundary.
3. Implement with ROM-derived evidence and headless regression coverage.
4. Update the relevant documentation and `PROJECT_STATUS.md`.
5. Run proportional CMake, Python, MSVC, and launch verification.
6. Commit the coherent slice and leave no unrelated or uncommitted changes.

If a slice discovers that the next queue is wrong, update the queue rather
than preserving a stale plan. Git history is evidence; `PROJECT_STATUS.md` is
the current handoff.
