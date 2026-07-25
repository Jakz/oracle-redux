# Room collision decoding

Oracle Redux decodes the collision property assigned to every visible
metatile directly from the user-supplied cartridge. This is the first
traversal-oriented content layer; it does not yet move Link or execute room
transitions.

## Cartridge path

Each tileset layout header contains two eight-byte compressed-data records:

1. metatile mappings;
2. metatile collision properties.

The collision record has the same structure as the mapping record:

| Offset | Meaning |
| --- | --- |
| `+0` | dictionary header index |
| `+1` | compressed source bank |
| `+2` | big-endian compressed source address |
| `+4` | big-endian destination address used by the original runtime |
| `+6` | big-endian decompressed size and flags |

The retail US ROMs decompress every collision record to 256 bytes. The byte at
index `n` is the collision property for metatile `n`. After room layout
substitutions are applied, Oracle Redux maps every room metatile through that
table to produce `RoomCollisionMap`.

The bank-one table offsets used by the exact US cartridges are:

| Campaign | tileset layout headers | dictionary headers |
| --- | ---: | ---: |
| Ages | `0x0787e` | `0x07870` |
| Seasons | `0x07964` | `0x0794e` |

Compression control bits are consumed least-significant bit first. Dictionary
mode zero packs a 12-bit offset and a length of `high nibble + 3`; mode one
stores the length byte followed by a little-endian dictionary offset. These
rules match `tools/dump/dumpTilesets.py` in the `oracles-disasm` research
reference.

## Collision meaning

Values below `0x10` are four-bit masks for the four 8×8 quadrants of a 16×16
metatile:

| Bit | Quadrant |
| ---: | --- |
| 3 | top-left |
| 2 | top-right |
| 1 | bottom-left |
| 0 | bottom-right |

Values `0x10` through `0x1f` are special collision classes. Classes
`0x10–0x17` select one of eight two-pixel-wide vertical columns; classes
`0x18–0x1f` select horizontal rows. The original engine selects a different
eight-bit shape for Link, ordinary grounded actors, and actors that cannot use
small bridges. `RoomCollisionDecoder::is_solid` preserves those three
profiles.

Important named classes include:

- `0x10`: holes, water, and lava;
- `0x11`: vertical bridge;
- `0x17`: minecart track;
- `0x18`: slowing stairs;
- `0x19`: horizontal bridge;
- `0x1e`: rising floor.

Link's ordinary collision profile permits entering `0x10`; the fall, swim, or
damage behavior is a separate interaction. The diagnostic overlay therefore
shows holes and similar tiles in blue rather than claiming they are ordinary
walls.

## Viewer diagnostics

Press `F2`, or start the viewer with `--collisions`, to draw the decoded ROM
collision layer:

- translucent red marks solid samples for Link's movement profile;
- blue marks hole/water/lava special tiles;
- cyan outlines all special collision classes;
- orange-red stripes show the exact two-pixel special shape.

`--describe` also prints an aggregate collision signature and counts for the
selected room region. These make later decoder changes regression-checkable
without extracting collision assets into the repository.

## Current boundary

The map represents the base collision table after previewed metatile
substitutions. Runtime object-driven changes, opened doors, pits created during
play, screen-edge blanking, elevation, and transition triggers are deliberately
not synthesized yet. Those belong to room state and the traversal simulation,
not the immutable cartridge content decoder.
