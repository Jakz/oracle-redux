# Link tile types and Z-aware contact

## Terrain is not the collision byte

The byte decoded by `RoomCollisionDecoder` answers whether a particular point
inside a metatile is solid. It does not say whether Link is standing on a
hole, water, lava, ice, or another active terrain class. Retail keeps that
second concern in `tileTypesTable` and selects one of six sparse maps using
`wActiveCollisions`, exposed in C++ as `TilesetDescriptor::collision_mode`.

The exact supported US ROM roots are:

| Campaign | Bank/address | File offset | Collision-mode order |
| --- | --- | --- | --- |
| Ages | `05:7c6e` | `0x17c6e` | overworld, indoors, dungeons, sidescroll, underwater, group five |
| Seasons | `05:7bad` | `0x17bad` | overworld, Subrosia, Maku Tree, indoors, dungeons, sidescroll |

Each root contains six little-endian pointers. A pointed list consists of
`metatile, tile-type` pairs and ends with a zero metatile. Unlisted metatiles
are `TILETYPE_NORMAL`. `RoomTileTypeDecoder` reads these records from the
player-supplied ROM and expands them into a typed room cache; it does not copy
the tables into the executable.

Representative campaign differences include:

| Meaning | Ages overworld | Seasons overworld |
| --- | --- | --- |
| hole | `$f3` | `$f3`, `$f4` |
| water | `$fa`, `$fe`, `$ff` | `$fd` |
| lava | `$e4`-`$e8` | `$7b`-`$7f` |
| campaign-only water | `$fc` seawater | none |

Side-scrolling tables use bit flags instead of the top-down enum. The decoder
preserves the raw byte, but top-down hazard behavior must not be applied when
the tileset has `TILESETFLAG_SIDESCROLL`.

## Link's active sample

`@linkGetActiveTileType` calls `objectGetRelativeTile` with `bc=$0500`.
Consequently, the active metatile is sampled at Link X and Link Y plus five
pixels. This is independent of the eight asymmetric probes used for wall
collision. `RoomTileTypeDecoder::sample_link_feet` preserves that boundary.

The SDL slice shows the decoded active type on its status line. When the
Feather lands, the diagnostic line records the landing type. Ordinary holes
now consume the typed contact through the state machine documented in
`hole-fall-and-respawn.md`; swim, drown, lava, ice, and conveyor reactions
remain separate policies.

## Z-aware object contact

The shared `_checkCollidedWithLink` routine first compares the signed high
bytes of Link and object Z. It accepts contact only when their separation is
less than seven pixels, then performs the ordinary Y/X radii test. Therefore
six pixels contacts and seven pixels does not.

`object_z_contact` implements the signed 8.8 high-byte operation, including
flooring negative fractional heights as the LR35902 representation does.
Octorok body damage, Octorok projectiles, and their collectible drops now use
this gate. Solid-NPC separation remains a different retail path:
`preventObjectHFromPassingObjectD` uses its Y/X collision routine and is not
silently changed by this checkpoint.

## Sources and verification

Primary disassembly references:

- `data/{ages,seasons}/tile_properties/tileTypeMappings.s`;
- `constants/common/tileTypes.s`;
- `object_code/common/specialObjects/commonCode.s::@linkGetActiveTileType`;
- `code/bank0.s::_checkCollidedWithLink`.

ROM-backed tests validate both campaigns' representative hole, water, and lava
entries plus the five-pixel foot boundary. Headless tests also pin the signed
8.8 six/seven-pixel Z boundary.
