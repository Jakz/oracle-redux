# Room topology and warp graph

Oracle Redux now exposes two different kinds of room connectivity:

- spatial seams inferred from the 16×16 room address grid in groups `0–3`;
- explicit warp sources and destinations decoded from cartridge bank 4.

Keeping these separate is intentional. A neighboring room is spatially
adjacent, but collision and later runtime state determine whether Link can
actually cross that seam. A warp is a scripted transition with its own source
and destination transition modes.

## Destination tables

The bank-4 destination table contains one pointer for each of the eight world
groups. Every destination is three bytes:

| Byte | Meaning |
| --- | --- |
| 0 | destination room |
| 1 | packed destination Y/X position |
| 2 high nibble | transition parameter |
| 2 low nibble | destination transition type |

The table file offsets for the exact US ROMs are `0x12f5b` in Ages and
`0x12d4e` in Seasons. Destination group is implicit in the pointer selected by
the source record.

Seasons has one dynamic lookup rule: destination group 2 adds the active
season (`0–3`) to the source's base destination index. The topology decoder
takes that variant explicitly, and the room-slice CLI passes its `--season`
selection.

## Source tables

The source-pointer tables are at `0x1359e` in Ages and `0x13457` in Seasons.
Each of the eight entries points to a list of four-byte records.

For a standard source record:

| Field | Meaning |
| --- | --- |
| opcode low nibble | screen-edge quadrant mask; zero means a tile warp |
| byte 1 | source room |
| byte 2 | destination index |
| byte 3 high nibble | destination group |
| byte 3 low nibble | source transition type |

The edge bits are top-left, top-right, bottom-left, and bottom-right from bit 0
through bit 3.

Opcode bit 6 denotes a pointer record. Its second byte is the source room and
its final two bytes point to a position-specific sublist. In that sublist,
byte 1 is packed source Y/X rather than a room. Opcode bit 7 marks a final
catch-all entry: it is used if no earlier position matched.

Ages may terminate a group with an explicit `0xff` record because its dungeon
stair fallback can run without source data. Seasons instead uses final
catch-all records. A few original pointed lists intentionally fall through
without setting bit 7. The decoder permits this within the source group's data
and bounds it at the next group's start, matching the behavior and safety
boundary used by `oracles-disasm`'s `dumpWarps.py`.

## Graph API

`RoomTopologyDecoder::exits` returns:

- up to four seam candidates for small-layout groups;
- matching whole-room tile warps;
- position-specific exits and their catch-all entry;
- screen-edge warp overrides;
- a group fallback when the lookup routine would use one.

Each `RoomExit` retains packed source/destination positions, source and
destination transition modes, transition parameter, original destination
index, and whether it is a fallback. This is enough for a later traversal
simulation to resolve a transition without coupling it to rendering.

Group fallback edges are conditional: they matter only when the original game
invokes warp lookup, normally because a warp-class metatile or another game
system requested it. They do not mean every point in every room teleports.

## CLI diagnostics

List the selected room's seam and warp edges without opening the viewer:

```sh
oracle_room_slice oracle.gbc --group 0 --room 09 --list-exits
```

The output numbers every edge. Follow one edge and render or describe its
destination:

```sh
oracle_room_slice oracle.gbc \
  --group 0 --room 09 --follow-exit 3 --describe
```

`--follow-exit` also works for ordinary seam entries, providing a small
deterministic room-graph navigator before Link movement exists.

Validate every source group, pointed sublist, destination reference, and
season-adjusted destination in one pass:

```sh
oracle_room_slice oracle.gbc --catalog-topology
```

The catalog prints counts and a deterministic graph signature. The counts are
resolved conditional room edges rather than a count of unique four-byte ROM
records; in particular, a group fallback can appear for many source rooms.

## Current boundary

This slice catalogs possible connectivity. It does not yet test Link's body
against the collision map, detect warp-class metatiles, apply room flags to
transition availability, execute fades, update respawn state, or stream the
destination into a live simulation. Those behaviors are now able to build on
typed collision and topology data instead of reopening raw ROM parsing inside
gameplay code.
