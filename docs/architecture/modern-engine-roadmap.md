# Modern Engine Roadmap

The modern target is a continuous, high-refresh interpretation of the Oracle
world rather than a stretched 160x144 framebuffer. The original campaigns
remain the behavioral and content reference; the engine is built to express
both their exact room-based rules and optional modern capabilities.

## Quality bar translated into systems

| Desired result | Engine capability |
| --- | --- |
| Rooms appear to form one connected world | World-space room topology, neighbor streaming, and Seamless World Presentation |
| Actors can cross former room seams | Explicit multi-room Active Simulation Region, separate from what is merely visible |
| Smooth scrolling at 120 Hz or more | Fixed deterministic logic ticks with interpolated render frames |
| Widescreen and whole-world zoom | Resolution-independent camera over a multi-room `WorldScene` |
| Lighting, fog, and richer atmosphere | Optional backend-independent render passes |
| Classic and modern looks | Experience Profiles with granular capability overrides |
| More convenient item access | Input actions separated from original two-button item state |
| Readable modern interface | UI projection independent from the world camera |

## Runtime shape

```text
Campaign Definition + decoded assets
                 |
                 v
       World topology / streaming
                 |
        +--------+---------+
        |                  |
        v                  v
Active Simulation     Visual room cache
Region                (may be larger)
        |                  |
        +--------+---------+
                 v
       immutable tick snapshots
                 |
                 v
   render interpolation + camera
                 |
                 v
 world -> lighting -> fog -> grade -> UI
                 |
                 v
       SDL3 GPU backend adapter
```

The Active Simulation Region is authoritative. The visual cache may contain
many more rooms for widescreen and overview rendering. This keeps simple visual
enhancements safe while allowing a later seamless-gameplay capability to be
implemented intentionally.

## Profiles

The initial Classic Profile defaults to one active room, two item action slots,
pixel-perfect output, and no interpolated or atmospheric passes.

The Modern Profile enables interpolated rendering, continuous world
composition, scalable interface, lighting, fog, and color grading while
retaining Fidelity Baseline gameplay rules. A seamless multi-room simulation
region, expanded item actions, and other outcome-changing behavior are separate
explicit Gameplay Extensions rather than presentation defaults.

Gameplay-affecting choices belong in save/replay metadata. Pure presentation
choices do not affect deterministic state.

## Backend direction

The engine-facing API remains backend-independent. The first implementation
uses SDL3 for windows, input, controllers, audio, and platform integration, and
SDL3 GPU for render targets and programmable passes. It does not expose SDL
objects to `oracle_core`.

The first decoded-room renderer spike must demonstrate:

1. a multi-room tile scene at arbitrary window sizes;
2. smooth camera interpolation at a display rate above the logic rate;
3. pixel-perfect magnification and palette-resolved downsampling as separate
   choices;
4. one optional lighting pass and independently scaled UI;
5. deterministic headless tests continuing to pass without the backend.

## Historical exploration sequence

1. Decode one overworld room and its neighbors into stable asset and topology
   structures. **Initial metatile layout slice complete.**
2. Render that region through the existing camera and render-plan contracts.
   **Diagnostic SDL3 GPU renderer complete.**
3. Decode tileset mappings, tile pixels, attributes, and palettes from the ROM
   Source, replacing diagnostic metatile colors with original room graphics.
4. Add fixed-tick snapshot interpolation and verify discontinuity handling.
   **Camera interpolation contract and slice integration complete; actor
   snapshots remain.**
5. Implement classic one-room activation before multi-room activation.
6. Add world streaming and seamless transitions behind a gameplay capability.
7. Add input actions and a scalable HUD, then atmospheric passes.

This ordering gives the reverse-engineered rules a working visual host early
without coupling their correctness to the final renderer.

The exploration above produced the current diagnostic room and traversal
slices. The assessed production sequence now prioritizes a dual-campaign First
Playable and is maintained in
`docs/architecture/implementation-sequence.md`.
