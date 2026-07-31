# Oracle Redux

This workspace is the beginning of a research-oriented C++ reimplementation of
*The Legend of Zelda: Oracle of Seasons* and *Oracle of Ages* as two campaigns
over one shared runtime.

Current implementation progress and the next fidelity queue live in
[`PROJECT_STATUS.md`](PROJECT_STATUS.md). Source placement, `.h` naming,
verification, documentation, and per-slice commit rules live in
[`docs/development/conventions.md`](docs/development/conventions.md); repository
agents are required by [`AGENTS.md`](AGENTS.md) to keep both current.

The repository does not distribute cartridge images. Put locally owned `.gb` or
`.gbc` inputs in `roms/`; that directory is ignored.

## Current state

- Validates and fingerprints both cartridges without changing them.
- Detects cartridge identity from the internal game code rather than filenames.
- Catalogs every 16 KiB bank.
- Treats the byte-exact US ROMs supported by `oracles-disasm` as the primary
  reverse-engineering baseline.
- Imports `oracles-disasm` linker symbols and sections into normalized,
  bank-aware CSV maps.
- Decodes a conservative named call graph from the retail ROM bytes, including
  recognizable `callab`/`jpab` far-bank calls.
- Measures US cross-game reuse and aligns each US game with its European
  counterpart.
- Conservatively traces code reachable from the fixed-bank hardware vectors and
  the bank-1 entry point.
- Writes reproducible JSON and CSV reports under `analysis/generated/`.

The current findings and their limitations are in
[`docs/reverse-engineering/initial-rom-catalog.md`](docs/reverse-engineering/initial-rom-catalog.md).
The symbol-guided overlap and call-graph results are in
[`docs/reverse-engineering/oracles-disasm-reference.md`](docs/reverse-engineering/oracles-disasm-reference.md).
Relocation-normalized routine and subsystem results are in
[`docs/reverse-engineering/routine-overlap.md`](docs/reverse-engineering/routine-overlap.md).
The first implementation slice is specified in
[`docs/reverse-engineering/item-trace-contract.md`](docs/reverse-engineering/item-trace-contract.md).

## Run the catalog

Python 3.10 or newer is sufficient; the analyzer uses only the standard
library.

```sh
python tools/rom_catalog.py
python -m unittest discover -s tests -v
```

After building a local `oracles-disasm` checkout so that `ages.sym`,
`seasons.sym`, `ages.gbc`, and `seasons.gbc` exist:

```sh
python tools/oracles_reference.py \
  --reference-dir reference/oracles-disasm \
  --rom-dir roms
python tools/routine_catalog.py \
  --reference-dir reference/oracles-disasm \
  --rom-dir roms
python tools/trace_contract.py specs/traces/plans/*.json
```

External checkouts and generated reports are ignored. They are research inputs,
not distributable project source.

## Research references

- [`Stewmath/oracles-disasm`](https://github.com/Stewmath/oracles-disasm)
  supplies the documented US-ROM symbols and source-level behavioral map.
- [`ladxhd/projectz`](https://github.com/ladxhd/projectz) is an architectural
  reference for separating a continuous HD tile world, active objects,
  depth-sorted sprites, camera scale, and effects. It is not a source-code
  dependency and no ProjectZ implementation or assets are redistributed.

Optional local checkouts live under ignored `reference/` directories:

```sh
git clone --depth 1 \
  https://github.com/ladxhd/projectz.git reference/projectz
```

## Build the C++ runtime

Headless libraries and tests have no SDL dependency. The optional standalone
room slice uses SDL 3.4 or newer. If SDL3 is not installed as a CMake package,
place an ignored source checkout at `reference/SDL`:

```sh
git clone --depth 1 --branch release-3.4.10 \
  https://github.com/libsdl-org/SDL.git reference/SDL
```

Then, from a compiler-enabled shell:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The scaffold currently contains:

- deterministic item-state primitives;
- the two identified Ages/Seasons parent-item policy hooks;
- renderer-independent fidelity, widescreen, and World Overview cameras;
- world-space scene types that can contain multiple cached rooms;
- Classic and Modern Experience Profiles with granular overrides;
- explicit single-room and seamless Active Simulation Regions;
- high-refresh render interpolation over deterministic logic ticks;
- backend-independent lighting, fog, grading, and interface render passes.
- exact US ROM validation and small-room layout decompression;
- cartridge-native tileset assignment, metatile, graphics, animation-frame,
  attribute, and palette decoding;
- deterministic cartridge-native background animation advancement;
- persistent room-flag tile substitutions decoded from both cartridges;
- cartridge-native per-metatile collision properties, including the original
  bridge, hole, track, and stair shape rules;
- separate cartridge-native Link terrain types, sampled at the original
  five-pixel foot offset, plus the retail Z gate for enemy/projectile contact;
- cartridge-native room seams, position-specific warps, screen-edge overrides,
  transition metadata, and season-adjusted destinations;
- collision-aware diagnostic player movement with wall sliding, anti-tunneling,
  continuous small-room seam crossing, and a following widescreen camera;
- authentic 16x16 Link visuals decoded at runtime from the original sprite,
  OAM-composition, animation-frame, and sprite-palette tables;
- typed room-object bytecode catalogs for interactions, enemies, parts, and
  item drops, with original spawn coordinates and diagnostic anchors;
- tick-sampled backend-neutral Input Frames and original-size bounded actor
  slot bands with deterministic lowest-slot allocation;
- a dual-campaign Vasu developer scenario with ROM-decoded Vasu/snakes OAM,
  recursive text dictionaries, semantic dialogue pages/options, a bounded
  native interpreter for the relocated retail Vasu scripts, typed Original
  State Keys, source-coordinate traces, script-derived NPC collision and wall
  slide, and replayed interaction input;
- a shared native Octorok combat path with cartridge-decoded enemy properties,
  compression mode, directional OAM, authentic sprite pixels, original RNG
  and action counters, non-solid contact damage, damage invincibility,
  level-one sword damage, knockback, palette flashing, the original
  enemy-destroyed puff, and deterministic heart/rupee drops;
- a shared native sword path with reserved parent-item slot allocation,
  original Link attack timing and frames, byte-mapped collision arcs, and
  cartridge-decoded `spr_swords` item OAM;
- live ROM tile-warp and vertical screen-edge execution with destination-group
  loading, transition-aware placement, and warp re-entry suppression;
- all 256 small rooms in world groups 0–3, with full-resolution atlas export;
- individual 15×11 large rooms from dungeon and side-scrolling groups 4–7;
- an SDL3 GPU room viewer using authentic ROM-derived 3×3 world regions.

All C++ headers use `.h`; the Python test suite enforces this convention.
The item implementations are source-faithful scaffolding, but remain
provisional until their planned emulator traces are captured and verified.
The presentation boundary is described in
[`docs/architecture/presentation.md`](docs/architecture/presentation.md), and
the modern target is decomposed in
[`docs/architecture/modern-engine-roadmap.md`](docs/architecture/modern-engine-roadmap.md).

## Run the standalone room slice

The executable requires a locally owned, exact US *Oracle of Ages* or *Oracle
of Seasons* ROM. It does not contain or generate a distributable asset pack.

```sh
build/oracle_room_slice "roms/Legend of Zelda, The - Oracle of Ages (USA).gbc"
```

Controls:

- in normal launch mode, `WASD` or arrow keys move Link;
- `Z` uses the level-one Roc's Feather when no contextual interaction consumes
  A; `Z` or Enter also opens chests, interacts, and advances dialogue;
  directions choose a dialogue option; `X` attacks outside dialogue and
  cancels inside dialogue;
- in `--atlas` mode, `WASD` or arrow keys pan the camera;
- the mouse wheel zooms between overview and close-up scales;
- `R` resets the camera;
- `F1` switches between authentic pixels and the metatile diagnostic view;
- `F2` overlays the decoded ROM collision shapes;
- `F3` toggles anchors for ROM room objects whose actual actors are not yet
  simulated and rendered;
- `Escape` exits.

A ROM-only launch now opens the newest playable fidelity slice: a real dungeon
chest whose record, treasure descriptor, and 30-rupee value are decoded from
the supplied cartridge. Link starts below it; press `Z` or Enter. The same
slice can be selected explicitly:

```sh
build/oracle_room_slice path/to/oracle.gbc --chest-scenario
```

The runtime replaces only that room's pixels and collision cache, sets the
original `ROOMFLAG_ITEM`, and prevents repeat collection. The binary formats
and current fidelity boundary are documented in
[`docs/reverse-engineering/chest-and-treasure-data.md`](docs/reverse-engineering/chest-and-treasure-data.md).

After opening the default chest, press `Z` again to exercise the shared
top-down Roc's Feather path. Link uses the ROM's directional jump poses and
signed 8.8 retail Z arc while his shadow remains on the ground. The status line
shows the ROM-derived terrain under Link and records the type on landing. The
source mapping and deferred terrain reactions are documented in
[`docs/reverse-engineering/rocs-feather-slice.md`](docs/reverse-engineering/rocs-feather-slice.md).

Launch the first deterministic actor-interaction scenario. The runtime chooses
the correct Vasu shop for either campaign:

```sh
build/oracle_room_slice path/to/oracle.gbc --vasu-scenario
```

The scenario and current fidelity boundary are documented in
[`docs/reverse-engineering/vasu-interaction-slice.md`](docs/reverse-engineering/vasu-interaction-slice.md).

Launch the first shared enemy/combat scenario. The runtime chooses a real
positioned Octorok room for either campaign:

```sh
build/oracle_room_slice path/to/oracle.gbc --octorok-scenario
```

Octoroks now allocate their original `$18` projectile in the bounded part
band. Its ROM graphics, wall impact, reverse bounce, expiry, and player damage
are active. Use `X` for the native sword swing. A defeated red Octorok follows
the original knockback, death-puff, drop-probability, bounce, and collection
path. The decoded enemy and projectile paths are documented in
[`docs/reverse-engineering/octorok-combat-slice.md`](docs/reverse-engineering/octorok-combat-slice.md).
The aftermath trace is in
[`docs/reverse-engineering/octorok-defeat-slice.md`](docs/reverse-engineering/octorok-defeat-slice.md).
The parent-item timing, arc, and OAM map is in
[`docs/reverse-engineering/sword-swing-slice.md`](docs/reverse-engineering/sword-swing-slice.md).

All playable developer checkpoints are also available through the stable
named selector:

```sh
build/oracle_room_slice path/to/oracle.gbc --list-scenarios
build/oracle_room_slice path/to/oracle.gbc --scenario hole
build/oracle_room_slice path/to/oracle.gbc --scenario-menu
```

The same interactive menu is the default when launching `OracleRoomSlice`
from `projects/msvc/oracle-redux.sln`. The ROM-derived `hole` scenario starts
beside a suitable ordinary hole in either campaign. Broad subsystem status,
scenario coverage, dependencies, verification, and known gaps are tracked in
[`specs/slices.json`](specs/slices.json) and summarized with
`python tools/slice_status.py`.

Choose another room by its hexadecimal overworld coordinate:

```sh
build/oracle_room_slice path/to/oracle.gbc --room 91
```

Select another active small-room group:

```sh
build/oracle_room_slice path/to/oracle.gbc --group 1 --room 91
```

For *Seasons*, select the visual season without changing the ROM:

```sh
build/oracle_room_slice path/to/seasons.gbc --room 91 --season winter
```

Validate and decode without opening a window:

```sh
build/oracle_room_slice path/to/oracle.gbc --room 91 --describe
```

Start with the collision diagnostic visible:

```sh
build/oracle_room_slice path/to/oracle.gbc --room 91 --collisions
```

List the selected room's graph edges, then follow an edge by its printed
index:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --group 0 --room 09 --list-exits
build/oracle_room_slice path/to/oracle.gbc \
  --group 0 --room 09 --follow-exit 3 --describe
```

Decode and validate the complete conditional warp graph:

```sh
build/oracle_room_slice path/to/oracle.gbc --catalog-topology
```

Catalog all room-object streams for the selected cartridge:

```sh
build/oracle_room_slice path/to/oracle.gbc --catalog-objects
```

Start at an exact original packed room position for transition diagnostics:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --group 0 --room ba --spawn-yx 65
```

Set a deterministic starting animation tick, or preview the original
persistent room-flag substitutions:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --room 91 --tick 120 --room-flags 80 --describe
```

`--room-flags` accepts one hexadecimal byte. It is currently a content-decoder
preview and applies the same flags to every visible room; save-file-backed
per-room state comes in a later simulation milestone.

View all 256 room addresses in one pannable, zoomable atlas:

```sh
build/oracle_room_slice path/to/oracle.gbc --group 0 --atlas
```

Export the complete `2560×2048` ROM-derived atlas without opening a window:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --group 0 --tick 120 --export-atlas build/group0.bmp
```

Groups `0` through `3` are the small-room address spaces in both campaigns.
Groups `4` through `7` select individual large dungeon or side-scrolling
rooms. They render one room at a time because their room ids do not describe a
continuous overworld grid:

```sh
build/oracle_room_slice path/to/oracle.gbc --group 4 --room c3
```

Export the selected neighborhood or large room at native resolution:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --group 4 --room c3 --export-region build/room-c3.bmp
```

Save one local frame and exit:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --room 91 --screenshot build/room-slice.bmp
```

Run a fixed-length renderer benchmark and print update, render, and present
timings:

```sh
build/oracle_room_slice path/to/oracle.gbc --benchmark-frames 120
```

Authentic rendering is the default. `--diagnostic` starts with deterministic
metatile colors instead. The cartridge is decoded in memory at runtime; the
repository and executable contain no extracted Nintendo graphics.

The ROM structures and current rendering boundary are documented in
[`docs/reverse-engineering/authentic-room-rendering.md`](docs/reverse-engineering/authentic-room-rendering.md).
The staged room-state boundary is cataloged in
[`docs/reverse-engineering/room-mutation-pipeline.md`](docs/reverse-engineering/room-mutation-pipeline.md).
The world-group topology and atlas semantics are documented in
[`docs/reverse-engineering/world-atlas-rendering.md`](docs/reverse-engineering/world-atlas-rendering.md).
The visible-world update and texture-cropping optimization is documented in
[`docs/architecture/visible-world-rendering.md`](docs/architecture/visible-world-rendering.md).
The large-room format is documented in
[`docs/reverse-engineering/large-room-rendering.md`](docs/reverse-engineering/large-room-rendering.md).
The decoded traversal properties and collision profiles are documented in
[`docs/reverse-engineering/room-collisions.md`](docs/reverse-engineering/room-collisions.md).
The separate Link terrain-type tables, foot sample, and Z-aware object contact
are documented in
[`docs/reverse-engineering/link-tile-types-and-z-contact.md`](docs/reverse-engineering/link-tile-types-and-z-contact.md).
Room adjacency, warp records, and graph navigation are documented in
[`docs/reverse-engineering/room-topology.md`](docs/reverse-engineering/room-topology.md).
The current player coordinate, collision-body, and seamless traversal model is
documented in
[`docs/architecture/player-traversal.md`](docs/architecture/player-traversal.md).
The shared Semantic UI Model and dual Fidelity/Modern retained layouts are
documented in
[`docs/architecture/ui.md`](docs/architecture/ui.md).
The sandboxed original sound-driver boundary and Blargg CPU/APU integration are
documented in
[`docs/architecture/audio.md`](docs/architecture/audio.md).
Stable message identities, semantic control tokens, and localization-pack
fallback are documented in
[`docs/architecture/localization.md`](docs/architecture/localization.md).
Deterministic logic-tick input and replaceable device bindings are documented
in
[`docs/architecture/input.md`](docs/architecture/input.md).
The assessed First Playable milestones, acceptance criteria, and principal
risks are documented in
[`docs/architecture/implementation-sequence.md`](docs/architecture/implementation-sequence.md).
Live warp-tile recognition, transition resolution, and destination streaming
are documented in
[`docs/architecture/live-room-transitions.md`](docs/architecture/live-room-transitions.md).
The original Link sprite pipeline is documented in
[`docs/reverse-engineering/link-sprite-rendering.md`](docs/reverse-engineering/link-sprite-rendering.md),
and the dynamic room-object boundary in
[`docs/reverse-engineering/room-objects.md`](docs/reverse-engineering/room-objects.md).
The original interaction, movement, and simple-script bytecode formats and
their planned native C++ execution boundary are documented in
[`docs/reverse-engineering/campaign-script-format.md`](docs/reverse-engineering/campaign-script-format.md).

Windows developers can instead open the maintained solution at
[`projects/msvc/oracle-redux.sln`](projects/msvc/oracle-redux.sln).
It builds the room slice, the native runtime tests, and the ignored official
SDL3 project without requiring CMake. Setup details are in
[`projects/msvc/README.md`](projects/msvc/README.md).

## Project direction

The target shape is a behavior-first port:

```text
SDL3 platform + GPU adapter
        |
shared deterministic Oracle runtime
        |
campaign definition + content
   /                       \
Seasons                    Ages
```

SDL3 is the first platform and GPU backend, behind a replaceable adapter.
Original rendering dimensions and timing are the fidelity baseline. Widescreen
composition, modern UI, input remapping, and visual effects remain
presentation capabilities. Gameplay changes such as multi-room activation are
separate, explicit settings so they cannot accidentally leak into fidelity
behavior.
