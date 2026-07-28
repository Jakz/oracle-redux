# Use SDL3 GPU as the first platform backend

Status: Accepted

## Context

The first renderer needs widescreen output, render targets, shaders, multiple
effect passes, high-refresh presentation, controllers, audio, and portable
window management. A basic 2D blitter would be quick to start but would become
a constraint once lighting, fog, grading, and large zoomable scenes arrive.
A full general-purpose game engine would provide those features but would also
impose its own scene, component, serialization, and asset models on the
reverse-engineered Oracle Runtime.

SDL3 provides platform services and a cross-platform GPU API while allowing the
project to keep its own deterministic world and content model.

## Decision

Use SDL3 for the first platform adapter and SDL3 GPU for the first renderer.
The production renderer uses the direct SDL GPU API rather than the high-level
SDL Render API. The existing SDL Render implementation remains a diagnostic
viewer while the production renderer is brought up.

Rendering is split into three C++ layers:

1. `oracle_presentation` publishes an immutable, backend-independent
   `PresentationSnapshot` containing visible world instances, presentation
   metadata, camera state, interpolation state, and interface instances.
2. `oracle_renderer` turns that snapshot into a semantic `RenderGraphPlan`.
   The plan names logical resources and ordered passes such as world geometry,
   depth/material output, lighting, atmosphere, post-processing, and interface.
   It does not expose SDL handles or duplicate SDL GPU's low-level API.
3. `oracle_sdl_gpu` owns SDL GPU devices, windows, shaders, pipelines, command
   buffers, render targets, uploads, and resource lifetimes. It translates the
   semantic plan into SDL GPU render and compute passes.

The first graph is expected to produce world color, depth, and material data,
apply optional lighting and post-processing, and then composite the interface
through a separate projection. This keeps the HUD sharp when world zoom,
depth-of-field, fog, or color grading is enabled.

SDL handles windows, events, controllers, audio devices, and GPU submission.
SDL types and lifetimes do not cross into `oracle_core`, campaign definitions,
save data, reverse-engineered rules, `PresentationSnapshot`, or
`RenderGraphPlan`.

Shaders are project assets compiled for the formats required by the selected
SDL GPU backends. A headless implementation remains available for tests and
analysis tools. Public C++ headers use the `.h` extension.

## Consequences

- The initial renderer can implement lighting and post-processing without a
  later migration away from a simple 2D API.
- Windows, Linux, and macOS can share most platform and renderer code.
- Shader compilation and GPU-resource lifetime management become explicit
  build concerns.
- Presentation and graph construction can be tested without a window or GPU.
- Gameplay systems cannot issue draw calls or depend on render-target layout.
- Semantic render passes remain stable while SDL GPU pipelines and resource
  strategies can evolve independently.
- The project retains control over world streaming and deterministic state.
- Another backend can replace SDL3 GPU by implementing the same adapter
  boundary, though SDL remains the default platform dependency.
