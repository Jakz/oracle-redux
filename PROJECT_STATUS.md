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

The latest gameplay checkpoint consumes the ROM-derived hole terrain through
the shared retail pull, 16/10/10 fall animation, hidden local respawn, one-heart
damage, 60-tick invulnerability, and 16-tick recovery path. Saved local
respawn coordinates are explicit room/warp checkpoints rather than a moving
“last safe tile.”

The current structural checkpoint groups implementation files by responsibility
while preserving stable public include paths. Visual Studio mirrors the same
tree with solution folders and project filters.

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
- CMake and hand-maintained MSVC build paths, with the room slice as the
  practical Visual Studio launch target.

## Next implementation queue

1. Implement water entry, flipper capability, swimming/drowning, and then lava
   entry/damage/respawn without coupling gameplay to presentation.
2. Add trampoline and cliff-specific Feather landing policies; keep
   side-scrolling Feather and Roc's Cape as separate policies.
3. Add the first persistence spine: Redux save envelope plus a validated,
   lossless original-save import/export round trip.
4. Continue representative object families and script commands toward a
   complete dual-campaign first playable.
5. Begin the isolated audio VM only after its opcode/APU boundary and trace
   fixtures are pinned.

## Known fidelity gaps

- Warp-hole topology, hole game-over/ring modifiers, water, lava, ice,
  conveyor, current, spike, trampoline, and cliff reactions are incomplete;
  ordinary nonfatal hole behavior is now consumed.
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
| 2026-07-31 | Ordinary hole lifecycle | Shared pull, retail fall frames, hidden checkpoint respawn, damage and recovery | CMake/native tests, Python, both MSVC projects, launch smoke test |
| 2026-07-31 | Source-tree and MSVC organization | Implementations grouped by responsibility; persistent workflow/status conventions added | CMake, Python, both MSVC projects, native tests, launch smoke test |
| 2026-07-31 | Link terrain and Z contact | Both ROM tile-type tables, Y+5 sample, landing report, exact Z gate | CMake/native tests, both MSVC projects, launch smoke test |
| 2026-07-31 | Roc's Feather | Shared top-down fixed-point jump and ROM poses | CMake/native tests, MSVC projects |

Older slice evidence remains in `docs/reverse-engineering`,
`docs/architecture/implementation-sequence.md`, and Git history.
