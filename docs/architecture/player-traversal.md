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

The current diagnostic body is an 8×6 pixel rectangle centered on the player.
The shallower vertical footprint permits the original partially-solid door
centering window. Eight perimeter samples are tested against
`RoomCollisionMap`, using Link's original collision profile:

- ordinary four-quadrant collision masks block matching samples;
- special bridge shapes use their original two-pixel stripe masks;
- holes, water, and lava remain enterable because their fall/swim/damage
  reactions are separate gameplay behaviors.

Movement is resolved independently on X and Y so the player slides along
walls. Large elapsed movements are split into steps no larger than one pixel
to prevent tunneling through narrow collision shapes.

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

Standing and ordinary walking now select original Link graphics frames by
facing direction. Full animation state, elevation, hazards, object
interaction, and attacks remain separate future slices.
