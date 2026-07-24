# Presentation Architecture

## Goals

The presentation layer must support:

- faithful 160×144 composition;
- widescreen windows without stretching;
- arbitrary world zoom, including zooming out;
- a World Overview spanning many rooms;
- seamless multi-room composition in the Modern Profile;
- render-frame interpolation independent of logic ticks;
- optional lighting, fog, and color-grading passes;
- independently scalable HUD and menus;
- renderer replacement without changing gameplay code.

## Boundary

The Simulation World owns gameplay. The presentation layer receives immutable
`WorldScene` snapshots and never decides collision, activation, scripts,
transitions, RNG, or timing.

```text
Simulation World
  active actors, rules, state
            |
            | immutable snapshot/events
            v
Scene composition
  active room visuals
  cached neighboring room visuals
  world-space sprites/effects
            |
            v
Presentation Camera
  center, output size, zoom, mode
            |
            v
Render plan
  world, optional effects, interface
            |
            v
Renderer adapter
  SDL3 GPU initially
```

## Camera modes

**Fidelity** renders the original 160×144 composition and is the behavioral
visual baseline.

**Widescreen** expands the visible world rectangle. Static neighboring-room
visuals may be composed into that rectangle, but neighboring gameplay remains
inactive unless the original rules activate it.

**World Overview** can request a much larger cached visual region at reduced
scale. It is a view over decoded/cached campaign geometry, not a command to run
the entire world.

All modes use the same world-space scene format. Camera zoom is measured as
output pixels per world unit, so values below one naturally express a zoomed-out
overview.

## World and UI scaling

World rendering and interface rendering use separate projections:

- the world camera may zoom smoothly or use integer/pixel-perfect scale;
- HUD and menus can retain readable size;
- an optional fidelity HUD can remain anchored to a 160×144 safe frame;
- modern widescreen UI can occupy side panels without changing world
  coordinates.

This avoids the common failure mode where zooming out makes menus unreadable or
where widening the HUD changes the gameplay viewport.

## Multi-room composition

The eventual scene composer should distinguish:

1. **Active room snapshot** — terrain and simulated actors.
2. **Neighbor visual cache** — decoded terrain, decorations, and other
   non-authoritative visuals.
3. **Presentation-only continuity** — seams, masks, or background extensions
   needed when original room edges become visible.

Actors outside the active Simulation World are not invented or advanced by the
renderer. If a future enhancement deliberately simulates more rooms, that is a
separate gameplay capability represented by an Active Simulation Region and
must not masquerade as widescreen rendering.

## Smooth rendering

The Simulation World advances on deterministic logic ticks. A render frame may
interpolate between two completed snapshots, allowing high-refresh camera and
sprite motion without changing collision, scripts, RNG, or game speed.

Teleports, warps, and other discontinuities must mark state that cannot be
interpolated. See
`docs/adr/0002-decouple-logic-ticks-from-render-frames.md`.

## Modern effect passes

The presentation layer resolves a backend-independent pass order:

1. world;
2. optional dynamic lighting;
3. optional atmospheric fog;
4. optional color grading;
5. interface.

The interface remains last and uses its own projection, so atmosphere and world
zoom do not reduce HUD clarity. Each effect is independently configurable even
when enabled by the Modern Profile.

## Backend choice

SDL3 GPU is the first rendering backend. `oracle_presentation` contains camera,
scene, timing, and render-plan data rather than SDL objects, so another backend
can still be implemented without changing gameplay. Backend-specific textures,
windows, input objects, and audio devices must stay outside `oracle_core`.
