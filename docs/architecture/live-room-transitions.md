# Live ROM room transitions

The interactive room slice now resolves and executes the cartridge's ordinary
tile and vertical screen-edge warps. This connects the player traversal
controller to the previously decoded room graph.

## Warp-tile recognition

Warp source records identify the source room and sometimes its packed Y/X
position, but a whole-room source does not identify the triggering metatile.
The original engine answers that separately with `warpTileTable`, indexed by
the tileset's active collision mode.

Oracle Redux reads those tables from bank 1 at:

| Campaign | `warpTileTable` file offset |
| --- | ---: |
| Ages | `0x06238` |
| Seasons | `0x060e8` |

Each of the six collision modes points to pairs of metatile id and property
byte, terminated by metatile zero. The Seasons chimney property is retained
even though its special held-item behavior is not implemented yet.

`RoomTopologyDecoder::resolve_tile_warp` first verifies that the current
metatile is present in the active warp-tile table. It then applies source
precedence:

1. exact position entry;
2. whole-room tile source;
3. final catch-all source.

This prevents topology fallback entries from turning ordinary ground into
warps.

## Activation

At every fixed 60 Hz gameplay update, the interactive viewer:

1. moves the player against the current collision maps;
2. resolves a vertical seam crossing against any matching screen-edge mask;
3. checks a centered player against the current metatile's warp property;
4. resolves the room/position source;
5. loads the destination world group or individual large room;
6. places the player using the destination Y/X and transition metadata;
7. rebuilds the authentic ROM-derived texture and collision cache;
8. re-centers the widescreen camera.

Screen-edge bits preserve the original top-left, top-right, bottom-left, and
bottom-right quadrant meanings. The retail routine only invokes this lookup
for vertical screen movement, so horizontal seams remain ordinary spatial
connections.

## Destination positioning

Ordinary packed positions use the conversion documented in
`player-traversal.md`. Destination transition type 3 is the enter-screen
transition. When its packed Y or X nibble is `f`, Oracle Redux interprets the
special form instead of treating it as an off-room metatile:

- X nibble `f` selects the horizontal center of the decoded room;
- parameter `9` places the player at the bottom edge facing north;
- other enter-screen parameters place the player at the top facing south.

If a decoded destination is still obstructed, the diagnostic runtime searches
outward for the nearest valid collision position. This is a temporary safety
rule until original Link elevation, door animation, and transition motion are
implemented.

The destination warp position is deactivated until the player leaves its
metatile. This mirrors the original `wEnteredWarpPosition` guard and prevents
immediate ping-pong between paired doors.

## Diagnostics

The fourth viewer status line shows current group/room, packed Y/X, and the
last executed edge. For deterministic testing, `--spawn-yx HEX` starts at an
exact original room position and prints:

- the source metatile;
- its collision byte;
- its warp-table property;
- every active warp metatile in the room;
- the resolved destination, if any.

The command is diagnostic-only input and does not represent save-game spawn
state.

## Current boundary

Transitions are currently immediate. Source fades, walk-in/walk-out motion,
sound, respawn updates, dungeon-floor stair inference, horizontal special
transitions, cutscene/script warps, and save-backed room conditions remain
future behavior slices. The important boundary established here is that live
world loading consumes typed ROM content and transition metadata rather than
hard-coded room links.
