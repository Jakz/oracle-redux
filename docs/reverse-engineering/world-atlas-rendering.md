# World atlas rendering

Oracle Redux can decode, compose, view, and export every room address in the
small-room portions of both US cartridges. The atlas is generated at runtime
from a user-supplied ROM; no extracted map or graphics are stored in the
repository.

## Two different group identities

The original engine keeps two related values:

- the **active world group**, which selects room tilesets and persistent-state
  storage;
- the **tileset layout group**, which selects the room-layout compression
  table.

They usually correspond for the overworlds but are not interchangeable. A room
can select a layout group through its tileset descriptor, and several active
groups reuse tileset tables. Oracle Redux therefore resolves each atlas cell
in original order:

```text
active world group + room
        |
roomTilesetsGroupTable
        |
tileset descriptor
        |
tileset layout group
        |
roomLayoutGroupTable
        |
compressed room layout
```

The active group remains the room's identity and the selector for Seasons'
standard persistent substitutions.

## Cataloged group families

Both games expose eight active group numbers. The cartridge behavior observed
by the decoder is:

| Active groups | Ages | Seasons | Current support |
| --- | --- | --- | --- |
| `0` | present Labrynna address space | Holodrum address space | full atlas |
| `1` | past Labrynna address space | Subrosia/small-room address space | full atlas |
| `2` | present auxiliary small rooms | auxiliary small rooms | full atlas |
| `3` | past auxiliary small rooms | auxiliary small rooms | full atlas |
| `4`, `6` | first large-layout family | first large-layout family | cataloged, not decoded |
| `5`, `7` | second large-layout family | second large-layout family | cataloged, not decoded |

The group descriptions above are address-space roles, not a claim that every
cell is a playable geographic room. Groups 2 and 3 contain interiors, caves,
and other small-room allocations. In Seasons, the original room-flag lookup
aliases groups 1 through 3 to Subrosia's persistent room-state array.

`roomLayoutGroupTable` has six entries in Ages and seven in Seasons. Its first
byte selects the original large- or small-room decompressor. The current C++
decoder reads this discriminator and reports the exact active group, room, and
selected layout group when a large layout is requested.

## Raw atlas versus canonical map bounds

An atlas covers the complete 8-bit room coordinate space:

```text
16 columns × 16 rows
160 × 128 pixels per small room
= 2560 × 2048 pixels
```

This intentionally includes unused, filler, and out-of-map sectors. They are
valuable evidence: they expose shared templates, alternate layouts, and data
stored outside the normal map cursor bounds.

The canonical map dimensions are smaller in some cases:

- Ages' overworld map is 14×14 rooms;
- Seasons' Holodrum map is 16×16 rooms;
- Seasons' Subrosia map is 11×8 rooms.

Cropping to those presentation bounds should be a separate map-definition
layer. The ROM atlas remains lossless and address-complete.

## Command-line inspection

Open a complete group in the SDL3 viewer:

```sh
oracle_room_slice oracle.gbc --group 0 --atlas
```

The initial camera fits the full atlas. Pan and zoom work as in neighborhood
mode, and `R` restores the atlas overview.

Export full-resolution pixels at an exact logic tick:

```sh
oracle_room_slice oracle.gbc \
  --group 1 --tick 120 --export-atlas build/group1.bmp
```

For Seasons, `--season spring|summer|autumn|winter` resolves all seasonal
tileset descriptors before composition. `--room-flags HEX` previews the same
persistent byte in every room; it is not yet per-room save state.

`--atlas --describe` decodes all 256 rooms without opening a window and reports
aggregate layout and animation signatures. These signatures are useful as
deterministic regression fixtures without checking copyrighted output into
Git.

## Runtime behavior

Initial atlas composition renders each room independently into one continuous
world-space pixel surface. Animation signatures are cached per animation group.
At runtime, only rooms whose selected animation frame changed are rerendered,
and SDL receives one texture update per changed room rather than a full
20 MiB atlas upload.

The atlas is a content and presentation diagnostic. It does not expand the
active simulation region to 256 rooms, so enabling it cannot silently alter
gameplay behavior.

## Next boundary

Large rooms are 15×11 metatiles and use a different dictionary-based layout
format. Implementing that decompressor will unlock active groups 4 through 7,
including dungeon and side-scrolling address spaces. Their placement is not a
16×16 seamless overworld, so they need a room catalog or dungeon topology view
rather than being forced into the small-room atlas model.
