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
            | immutable state/events
            v
Scene composition + Presentation Camera
  active room visuals
  cached neighboring room visuals
  world-space sprites/effects
  framing, output size, zoom, mode
            |
            v
PresentationSnapshot
  immutable backend-neutral instances and metadata
            |
            v
RenderGraphPlan
  logical resources and semantic pass ordering
            |
            v
SDL GPU backend
  pipelines, GPU resources, command buffers, submission
```

The graph boundary is deliberately semantic. `oracle_renderer` may request
world color, depth, material, lighting, atmosphere, post-processing, and
interface passes, but it does not reproduce SDL GPU structures behind a generic
API. `oracle_sdl_gpu` decides concrete texture formats, pipeline variants,
upload strategy, attachment reuse, and whether a semantic pass is implemented
as a render pass or compute pass.

The current high-level SDL Render path is a diagnostic viewer. It is not the
production implementation of this graph.

## World submission

Decoded terrain is represented as tile instances, not precomposed RGBA room
textures. All tiles share indexed unit-quad geometry and reference decoded
texture atlases. Static tiles occupy contiguous room or chunk ranges in a small
number of large GPU instance arenas grouped by compatible atlas and pipeline
state. A room range is an allocation within an arena, not a separate GPU
buffer.

The backend submits camera-visible ranges with indexed instancing. When visible
ranges are disjoint, it may build a compact indirect-command stream so they can
be submitted together. Tile substitutions and other persistent room mutations
dirty only the affected ranges. Rebuilding and uploading the entire visible
world each frame is not the default path.

Actors, particles, and transient effects use separately cycled per-frame
instance streams because their transforms, animation, or lifetime commonly
change every logic or render frame. Static and dynamic instances can still
share pipelines and atlas resources when their material state is compatible.

This representation preserves per-instance palette, material, visual height,
depth, animation, and occlusion metadata for 2.5D effects. A flattened room
image is permitted only as a diagnostic output or an explicitly measured
distant-overview optimization; it is not canonical presentation data.

## Indexed texels and palettes

Production texture atlases store the original 2-bit tile texels as exact
single-channel palette indices rather than precolored pixels. Atlas sampling is
nearest-neighbor. Each world instance identifies its palette domain and palette
selection along with the original flip, graphics-bank, and priority attributes.

The world shader performs these steps in order:

1. sample an integer-equivalent color index from the atlas;
2. apply the correct background or sprite transparency rule;
3. resolve the selected Game Boy Color palette entry;
4. apply the instance's Presentation Metadata and material treatment;
5. write the resolved world color, depth, and material outputs.

Sprite index zero is transparent. Background index zero is an ordinary opaque
palette entry unless a separately decoded original effect gives it other
semantics. Background and sprite palettes remain distinct domains even if their
resolved colors happen to match.

Palette animation, fades, campaign palette loads, and color substitutions
update palette-table state rather than rebuilding tile textures. Lighting,
fog, depth-of-field, bloom, and grading operate only after palette resolution.
The diagnostic room viewer may continue producing RGBA surfaces, but those
surfaces are not input to the production world graph.

## Pixel grid and interpolation

Fidelity presentation snaps the camera and presented world geometry to the
original pixel grid. It chooses integer output scaling whenever the output
surface can contain the requested 160×144 composition. Any remaining window
area is handled by framing or interface composition rather than stretching the
baseline image.

Modern presentation permits subpixel camera and actor transforms interpolated
between completed logic ticks. Indexed source texels remain nearest-sampled;
smoothness comes from transformed geometry and output-resolution effects, not
from blurring the original pixels. Atlas coordinates use texel-safe bounds so
subpixel motion cannot sample adjacent atlas entries.

When Modern presentation minifies original texels below one output pixel,
filtering occurs only after palette resolution. The renderer draws the visible
world into a World Working Surface at sufficient source-texel density, applies
world lighting and effects using the associated depth and material data, and
then downsamples resolved colors into the viewport. It never linearly filters
palette indices.

The working surface can match the source-resolution extent for an initial World
Overview implementation. If a requested extent exceeds practical GPU texture
limits or memory budgets, the backend may use tiled intermediate surfaces or a
palette-aware reconstruction shader, provided the visible result preserves the
same resolve-before-filter rule. These are measured backend optimizations, not
changes to the Presentation Snapshot.

Interface composition occurs after world downsampling at the output surface's
native resolution. World minification therefore cannot soften text, inventory
icons, dialogue frames, or other interface elements.

## Gameplay-aware focus

Modern depth-of-field uses both the presentation depth target and a semantic
focus mask. The default focus anchor is Link's ground-contact position, not the
center of the camera or the top of Link's sprite. Presentation Metadata can
protect Link, nearby interactable actors, important pickups, and
gameplay-critical effects from losing legibility even when their physical
depth would otherwise blur them.

Cutscene presentation may supply an explicit focus target or protected subject.
Those cues remain presentation data and cannot influence targeting, collision,
actor activation, interaction range, or script control flow. Missing focus
metadata falls back to the Link-centered policy rather than making a gameplay
object invisible.

The interface graph is outside the depth-of-field pass. World Overview disables
the effect by default, and depth-of-field strength is independently adjustable
down to off regardless of the selected Experience Profile.

## Stylized lighting

Modern lighting begins with palette-resolved Game Boy colors and preserves
their identity. Presentation Metadata assigns a small material class, visual
height, shadow behavior, and optional emission to ROM-derived visual
identifiers. The initial model is deliberately art-directed rather than a
metallic-roughness PBR workflow and does not require replacement normal maps.

A scene can provide a directional key light and sparse local point or cone
spotlights. Spotlights are available for dungeons, windows, torches, cutscenes,
and similar authored presentation, but their placement and tuning are postponed
until the relevant content is playable. Height and occlusion metadata can
produce projected or contact shadows without turning tile art into full 3D
geometry.

Material response and light intensity are constrained so grass, water, stone,
foliage, actors, and effects remain recognizable in their original palette
families. Fidelity presentation bypasses this lighting graph. Modern lighting
cannot affect simulation visibility, targeting, collision, script conditions,
or actor activation.

Interpolation state belongs to `PresentationSnapshot`. The Simulation World
continues to advance in its original coordinate and timing domains. Teleports,
warps, room discontinuities, and explicitly marked animation changes select the
new completed state without interpolating through invalid intermediate states.

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

## Modern render graph

The initial semantic graph is:

1. opaque and cutout world geometry producing color, depth, and material data;
2. actors and world effects participating in the same visual depth model;
3. optional lighting and projected shadows;
4. optional water, fog, and atmospheric composition;
5. optional depth-of-field, bloom, and color grading;
6. interface composition using its independent projection;
7. final presentation to the swapchain.

The interface remains last and uses its own projection, so atmosphere and world
zoom do not reduce HUD clarity. Each effect is independently configurable even
when enabled by the Modern Profile. Fidelity rendering can select a much
simpler graph without changing the Simulation World or scene composition.

## Backend choice

Direct SDL GPU is the production rendering backend. `oracle_presentation`
contains camera, scene, timing, and snapshot data; `oracle_renderer` contains
the semantic graph; and `oracle_sdl_gpu` contains the concrete implementation.
Another backend can still be implemented without changing gameplay.
Backend-specific textures, windows, input objects, and audio devices must stay
outside `oracle_core`.
