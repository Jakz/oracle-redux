# Oracle Redux

This workspace is the beginning of a research-oriented C++ reimplementation of
*The Legend of Zelda: Oracle of Seasons* and *Oracle of Ages* as two campaigns
over one shared runtime.

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
- an SDL3 GPU room viewer using ROM-derived 3×3 world regions.

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

- `WASD` or arrow keys pan across room seams;
- the mouse wheel zooms between overview and close-up scales;
- `R` resets the camera;
- `Escape` exits.

Choose another room by its hexadecimal overworld coordinate:

```sh
build/oracle_room_slice path/to/oracle.gbc --room 91
```

Validate and decode without opening a window:

```sh
build/oracle_room_slice path/to/oracle.gbc --room 91 --describe
```

Save one local diagnostic frame and exit:

```sh
build/oracle_room_slice path/to/oracle.gbc \
  --room 91 --screenshot build/room-slice.bmp
```

The current picture uses deterministic colors for decoded metatile identifiers.
It validates ROM loading, room decompression, continuous world placement,
widescreen camera composition, fixed-tick interpolation, and the SDL3 GPU
path. Decoding the metatile mappings, original tile pixels, attributes, and
palettes is the next visual layer.

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
