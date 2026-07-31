# Oracle Redux project status

Last updated: 2026-07-31

This is the canonical handoff document. Every implementation slice updates it
before committing so a new session can resume without reconstructing history
from chat messages.

## Current checkpoint

The first playable fidelity spine can load either supported US ROM, render
authentic rooms and a widescreen world view, move Link through decoded terrain
and transitions, run the Vasu interaction, fight an Octorok with the sword,
open a persistent ROM-defined chest, and use the level-one Roc's Feather.

The latest gameplay checkpoint consumes ROM-derived top-down water through a
capability-aware Flippers/Mermaid Suit policy, retail `SPEED_80` swimming,
campaign ROM swim/drown frames, and the shared hidden local respawn, one-heart
damage, 60-tick invulnerability, and 16-tick recovery path. The `water`
scenario locates a safe edge from either supplied ROM and lets `F4` toggle
Flippers to exercise both swimming and drowning. Saved local respawn
coordinates remain explicit room/warp checkpoints rather than a moving
“last safe tile.”

The current structural checkpoint groups implementation files by responsibility
while preserving stable public include paths. Visual Studio mirrors the same
tree with solution folders and project filters.

Broad-spectrum progress is tracked in `specs/slices.json`: 32 slices across
foundation, world, traversal, interactions, combat, items, hazards,
persistence, audio, presentation, UI, campaign-content, and verification.
`python tools/slice_status.py` validates and summarizes the live matrix.
The MSVC room-slice launch starts the latest scenario immediately. `F5` and
`Shift+F5` switch forward and backward at runtime across the latest integrated
room, exploration, chest, Vasu, Octorok, ROM-derived ordinary hole, water, and
whole-world atlas paths without changing startup projects or recompiling.
Every application, engine, and public-header file is explicitly visible under
the `OracleRoomSlice/src` filter tree rather than hidden behind project globs.

## Resume in the next session

The water implementation checkpoint is the repository tip. The worktree
should be clean. Begin by reading this file, `docs/development/conventions.md`,
`docs/development/slice-management.md`, and then run:

```powershell
git status --short
python tools/slice_status.py
```

The next bounded gameplay slice is `lava-lifecycle`. Before writing the
runtime, map the shared lava entry, campaign-specific immunity gates, forced
drowning animation, damage, and respawn branches for both US ROMs. Start from:

- `docs/reverse-engineering/link-tile-types-and-z-contact.md`;
- `docs/reverse-engineering/water-swimming-and-drowning.md`;
- `docs/architecture/player-traversal.md`;
- `include/oracle/gameplay/player_hazard_runtime.h` and its implementation;
- the water and ordinary-hole runtime/tests as integration patterns.

Keep the result backend-neutral and split retail research from uncertain
behavior. Add headless dual-campaign tests and a ROM-derived `lava` developer
scenario once safe-spawn and immunity policy are pinned. New `.cpp` and `.h`
files must also be added explicitly to the `OracleRoomSlice` project and its
`src` filter tree. Do not begin the SDL_GPU/2.5D presentation work during this
slice.

## Completed slices

- ROM validation, campaign cataloguing, routine-overlap analysis, and the
  player-supplied-ROM distribution boundary.
- Authentic room, large-room, atlas, animation, mutation, and visible-world
  rendering with viewport culling and performance diagnostics.
- ROM collision shapes, Link probes, actor collision, room seams, and live
  tile/screen-edge warp execution.
- ROM room-object decoding and bounded actor-slot allocation semantics.
- Vasu scripts, dialogue, interaction collision, and deterministic input.
- Octorok state machine, projectiles, contact damage, sword hits, defeat puff,
  deterministic drops, and pickup collection.
- Persistent ROM-defined chest contents and room-flag mutation.
- Shared top-down Roc's Feather arc with ROM Link poses and visual elevation.
- Separate ROM Link terrain types and Z-aware ordinary object contact.
- Shared ordinary-hole pull, fall frames, local respawn, damage, and recovery.
- Top-down water entry, Flippers capability, swim movement/frames, Ages
  seawater policy, drowning, local respawn, damage, and recovery.
- CMake and hand-maintained MSVC build paths, with the room slice as the
  practical Visual Studio launch target.
- Machine-validated broad-spectrum slice matrix and named developer scenarios.

## Next implementation queue

1. Implement lava entry, damage, and respawn without coupling gameplay to
   presentation.
2. Add trampoline and cliff-specific Feather landing policies; keep
   side-scrolling Feather and Roc's Cape as separate policies.
3. Add the first persistence spine: Redux save envelope plus a validated,
   lossless original-save import/export round trip.
4. Continue representative object families and script commands toward a
   complete dual-campaign first playable.
5. Begin the isolated audio VM only after its opcode/APU boundary and trace
   fixtures are pinned.

## Known fidelity gaps

- Warp-hole topology, hole/water game-over and ring modifiers, lava, ice,
  conveyor, current, spike, trampoline, and cliff reactions are incomplete.
  Top-down ordinary water is consumed; stroke acceleration, diving, Mermaid
  Suit velocity, underwater transitions, and side-scrolling water remain.
- Side-scrolling movement/Feather behavior and Seasons Roc's Cape are deferred.
- Only representative interaction, enemy, item, part, and script families run
  natively; the campaigns are not yet completable.
- Original save conversion and audio playback are architectural decisions, not
  implemented systems.
- The SDL renderer is still a fidelity/debug slice, not the final SDL_GPU
  indexed-atlas, 2.5D lighting, depth-of-field, and downsampling pipeline.

## Verification baseline

Run after ordinary engine changes:

```powershell
cmake --build build --target oracle_core_tests oracle_room_slice --config Debug
ctest --test-dir build -C Debug --output-on-failure
python -m unittest discover -s tests
python tools/slice_status.py --check
```

Run before committing changes that affect project structure or launchability:

```powershell
msbuild projects\msvc\OracleRoomSlice.vcxproj /m /p:Configuration=Debug /p:Platform=x64
msbuild projects\msvc\OracleRuntimeTests.vcxproj /m /p:Configuration=Debug /p:Platform=x64
projects\msvc\x64\Debug\oracle_runtime_tests.exe
```

For renderer changes, also launch `oracle_room_slice` for at least three
benchmark frames with a locally supplied supported ROM.

## Slice completion record

| Date | Slice | Result | Verification |
| --- | --- | --- | --- |
| 2026-07-31 | Water swimming and drowning | ROM-derived safe scenario; Flippers/Mermaid policy; `SPEED_80` swim; campaign frames; shared damage/respawn | Dual-campaign native tests, CMake, Python, both MSVC projects, both ROM scenario smoke tests |
| 2026-07-31 | MSVC visibility and runtime scenario switching | Explicit `OracleRoomSlice/src` tree; F5/Shift+F5 hot scenario reconstruction | Python project/catalog checks, CMake, MSVC, CTest, injected F5 launch smoke test |
| 2026-07-31 | Slice matrix and scenario selector | 31 cross-subsystem entries plus named/interactive dual-campaign developer scenarios | CMake/native tests, Python catalog checks, both MSVC projects, all scenario smoke tests |
| 2026-07-31 | Ordinary hole lifecycle | Shared pull, retail fall frames, hidden checkpoint respawn, damage and recovery | CMake/native tests, Python, both MSVC projects, launch smoke test |
| 2026-07-31 | Source-tree and MSVC organization | Implementations grouped by responsibility; persistent workflow/status conventions added | CMake, Python, both MSVC projects, native tests, launch smoke test |
| 2026-07-31 | Link terrain and Z contact | Both ROM tile-type tables, Y+5 sample, landing report, exact Z gate | CMake/native tests, both MSVC projects, launch smoke test |
| 2026-07-31 | Roc's Feather | Shared top-down fixed-point jump and ROM poses | CMake/native tests, MSVC projects |

Older slice evidence remains in `docs/reverse-engineering`,
`docs/architecture/implementation-sequence.md`, and Git history.
