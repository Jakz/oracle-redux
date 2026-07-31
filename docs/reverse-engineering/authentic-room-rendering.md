# Authentic room rendering

Oracle Redux now renders a static overworld background directly from a
player-supplied US cartridge image. No extracted tile, palette, room, or
metatile asset is checked into the repository.

## Runtime decode chain

For every visible room, the runtime follows the same indirections as the
original game:

```text
room group + room id
        |
roomTilesetsGroupTable
        |
8-byte tileset descriptor
        +---- tileset mapping header -> compressed mapping indices
        |                                  |
        |                           global 3-byte mapping table
        |                                  |
        |                         4 tile ids + 4 attributes
        |
        +---- main GFX header -----> two-bank emulated VRAM
        +---- unique GFX header --> overwrites emulated VRAM
        +---- animation group ----> timed, looping animated tile frames
        +---- palette header -----> emulated BG palette RAM
```

The renderer then expands each 16×16 metatile into four 8×8 Game Boy Color
tiles. It honors palette number, VRAM bank, horizontal flip, and vertical flip
from the original attribute bytes. A room becomes a 160×128 RGBA surface.
Neighboring surfaces are composed in continuous world coordinates, so SDL3 can
show a 16:9 viewport or zoomed-out overview without changing simulation data.

## Cartridge tables

Offsets below are file offsets in the exact 1 MiB US ROMs.

| Structure | Ages | Seasons |
| --- | ---: | ---: |
| `tilesetData` | `0x10f9c` | `0x10c84` |
| `roomTilesetsGroupTable` | `0x112d4` | `0x1133c` |
| tileset layout pointer table | `0x0787e` | `0x07964` |
| layout dictionary pointer table | `0x07870` | `0x0794e` |
| main GFX header pointer table | `0x069da` | `0x06926` |
| unique GFX header pointer table | `0x11b28` | `0x1195e` |
| animation group pointer table | `0x11b52` | `0x119b0` |
| animation GFX headers | `0x11be9` | `0x11a48` |
| palette header pointer table | `0x0632c` | `0x06290` |

The global mapping lookup is in bank `0x18` for Ages and bank `0x17` for
Seasons. Its first two words point to the shared tile-index and tile-attribute
arrays; 3-byte mapping records pack a 12-bit offset for each array.

Oracle backgrounds use the Game Boy Color LCD controller's signed tile-data
selection at `$8800`. Tile IDs `$80` through `$ff` address `$8800` through
`$8ff0`, while IDs `$00` through `$7f` address `$9000` through `$97f0`.
Treating those IDs as an unsigned `$8000` table reads an unpopulated VRAM
range and produces large single-color squares. Sprite tile addressing remains
separate and is not changed by this background rule.

Seasons tilesets `0x00` through `0x1a` are seasonal indirections. Their first
record byte is `0xff`, followed by a bank-4 pointer to four ordinary 8-byte
descriptors in spring, summer, autumn, winter order.

## Compression implemented

- Small-room common-byte modes 0, 1, and 2.
- Tileset dictionary modes 0 and 1.
- GFX modes 0 through 3, including overlapping back-references.

The decoder keeps the cartridge formats behind `oracle_content`; SDL receives
only composed RGBA pixels. This boundary lets a later renderer replace nearest
neighbor presentation with shaders, higher-resolution artwork, lighting, or
world-scale effects without rewriting content semantics.

## Current fidelity boundary

Implemented:

- static small-room layouts;
- room-specific tileset selection;
- all four Seasons descriptor variants;
- main and unique background graphics;
- startup background palette plus tileset palette overrides;
- cartridge-native animation initialization, countdowns, DMA queue, persistent
  VRAM writes, frames, and loops;
- deterministic animation selection from a 60 Hz logic tick;
- standard persistent room-flag tile substitutions for flags 0, 1, 2, 3,
  and 7;
- the Ages past-cliff palette substitution;
- cross-room composition and a diagnostic fallback;
- complete 16×16 atlases for active small-room groups 0 through 3;
- lossless `2560×2048` BMP export at a deterministic animation tick;
- dictionary-compressed 15×11 large dungeon and side-scrolling rooms;
- native-resolution export and dimension-aware rendering for either room size.

Not yet applied:

- common dynamic substitutions such as chests, shutters, toggle blocks, and
  water-state changes;
- room-specific pre- and post-graphics event overrides;
- object-specific sprites and behavior, hazards, and complete game state;
- tileset overrides tied to save flags, companions, or dungeon water level.

The animation decoder treats the first sequence byte as the initial countdown,
then reads alternating graphics-header indices and countdowns. A countdown of
`0xff` is followed by the low byte of the original signed backward jump. Ages
performs the original three forced initialization advances; Seasons performs
one. Every graphics header loaded during initialization remains resident in
emulated VRAM.

Later ticks first apply one pending animation-queue entry, then decrement all
enabled sequence counters and enqueue indices that reach zero. This persistence
is required by the Ages waterfall: its sequence updates several different VRAM
destinations over staggered ticks, so selecting only the latest graphics index
produces an incomplete cycle. Cycle detection accelerates large deterministic
`--tick` values only after both the control state and its resident overwrite
pattern have stabilized. At runtime, only visible rooms whose resident
animation signature changed are recomposed and uploaded.

`--describe` reports `tileset_animation_group` and
`room_animation_signature` for the selected room. These make individual ROM
animation states testable without storing copyrighted frame images.

These omissions are explicit layers, not baked visual approximations. The room
mutation stages and the boundary for the next implementation are recorded in
[`room-mutation-pipeline.md`](room-mutation-pipeline.md).
