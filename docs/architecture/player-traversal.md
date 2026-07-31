# Player traversal foundation

The standalone room slice now has a renderer-independent traversal controller.
Its first visual representation was deliberately a diagnostic marker. The
viewer now composes Link's original 16x16 visual independently from this
collision state.

## Coordinate model

`PlayerState` stores a world-group/room identity and room-local pixel
coordinates. For small-layout groups `0–3`, the renderer derives continuous
world coordinates from the room's hexadecimal grid address:

```text
world X = room column × 160 + local X
world Y = room row    × 128 + local Y
```

Keeping the authoritative position room-local makes ROM warp destinations
natural, while the derived coordinates allow a widescreen camera to follow
smoothly across room seams.

The original packed warp position includes the Game Boy status-bar row.
Conversion therefore maps:

```text
local X = packed X × 16 + 8
local Y = packed Y × 16 - 8
```

The conversion and its inverse are tested independently.

## Collision body

Retail terrain collision is not a centered rectangle. The shared
`calculateAdjacentWallsBitset` routine consumes a delta-encoded probe table;
accumulating those deltas produces these eight offsets from Link's position:

| Wall bit | Y | X | Side |
| ---: | ---: | ---: | --- |
| 7 | -3 | -3 | upper-left |
| 6 | -3 | +2 | upper-right |
| 5 | +7 | -3 | lower-left |
| 4 | +7 | +2 | lower-right |
| 3 | 0 | -5 | left-upper |
| 2 | +5 | -5 | left-lower |
| 1 | 0 | +4 | right-upper |
| 0 | +5 | +4 | right-lower |

This asymmetric footprint reaches farther below Link than above him. It is
independent of the 6-by-6 object collision radii used when Link contacts an
NPC or another actor. Every terrain probe is tested against
`RoomCollisionMap`, using Link's ordinary collision profile:

- ordinary four-quadrant collision masks block matching samples;
- special bridge shapes use their original two-pixel stripe masks;
- holes, water, and lava remain enterable because their fall/swim/damage
  reactions are separate gameplay behaviors.

The resulting eight-bit wall mask is filtered through the retail
`bitsToCheck` table for Link's 32-step movement angle. The original
`tileEdgeAdjust` and `slideAngleTable` rules redirect movement when only one
leading corner touches a tile, reproducing the characteristic edge slide.
Blocked vertical and horizontal components are then applied independently.
Large elapsed movements are split into steps no larger than one pixel to
prevent tunneling, and ordinary `SPEED_100` traversal advances one cardinal
pixel per 60 Hz logic tick.

Terrain traversal remains responsible for enterability, while
`PlayerHazardRuntime` consumes the typed terrain contact after movement.
Ordinary holes disable the separate object-contact channel during fall and
recovery without turning the terrain wall probes into a hazard state machine.
Raised-floor, swimming, lava, and other policy changes remain explicit future
gameplay states rather than presentation shortcuts.

## Seam crossing

When the collision body overlaps a small-room boundary, its samples are
resolved against the adjacent room before movement is accepted. If that room
is missing or its destination edge is solid, crossing is blocked. Otherwise
the player room id changes and local coordinates wrap while derived world
coordinates remain continuous.

The outer edges of the 16×16 room grid are always blocked. Groups `4–7` do not
infer spatial seams because their room ids are not a continuous overworld
layout.

## Viewer behavior

Normal interactive launch decodes the selected small-layout group as one
cached 256-room world. The camera starts at the selected room and follows the
player at the existing widescreen zoom. `--atlas` remains the free-camera
overview mode.

Current controls:

- `WASD` or arrow keys move the player in normal launch mode;
- the mouse wheel adjusts zoom without changing simulation coordinates;
- `R` restores the initial room and position;
- `F1` toggles authentic and metatile rendering;
- `F2` toggles the collision overlay;
- `F3` toggles unresolved room-object spawn anchors.

Standing, walking, Feather, sword, and ordinary hole falling select original
Link graphics frames. The room slice hides Link during the retail local
respawn delay, but gameplay owns that visibility decision; the renderer only
consumes it.
