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
The renderer will consume backend-independent `WorldScene`, camera, timing, and
render-plan data.

SDL handles windows, events, controllers, audio devices, and GPU submission.
SDL types and lifetimes do not cross into `oracle_core`, campaign definitions,
save data, or reverse-engineered rules.

Shaders are project assets compiled for the formats required by the selected
SDL GPU backends. A headless implementation remains available for tests and
analysis tools.

## Consequences

- The initial renderer can implement lighting and post-processing without a
  later migration away from a simple 2D API.
- Windows, Linux, and macOS can share most platform and renderer code.
- Shader compilation and GPU-resource lifetime management become explicit
  build concerns.
- The project retains control over world streaming and deterministic state.
- Another backend can replace SDL3 GPU by implementing the same adapter
  boundary, though SDL remains the default platform dependency.
