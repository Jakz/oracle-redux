# Large-room rendering

Oracle Redux decodes and renders the large layout families used by active world
groups 4 through 7 in both US cartridges. These include dungeon and
side-scrolling address spaces.

## Dimensions and storage

The playable large-room background is 15×11 metatiles:

```text
15 × 16 = 240 pixels
11 × 16 = 176 pixels
```

The original `wRoomLayout` buffer has a 16-byte row stride. Large-room streams
therefore decompress to 176 bytes: 15 playable metatiles plus one padding byte
for each of 11 rows. The content layer removes the padding after decompression
and exposes a compact 165-metatile `RoomLayout`.

Small rooms remain compact 10×8 layouts. `RoomLayout` and `RenderedRoom` carry
their dimensions explicitly, so the renderer, diagnostic view, animation
updates, and BMP export use one pipeline for both sizes.

## Cartridge indirections

Each entry in `roomLayoutGroupTable` contains:

1. a format discriminator (`0` for large, `1` for small);
2. a banked pointer to the group's lookup data;
3. a banked pointer to the group's first compressed room stream.

For a large group, the lookup data is:

```text
0x0000–0x0fff  4 KiB shared dictionary
0x1000–0x11ff  256 little-endian room offsets
```

The stored room offsets have a `0x200` bias. The original loader and the C++
decoder both resolve a stream as:

```text
stream = group_data_base + stored_offset - 0x200
```

The lookup is selected through the tileset descriptor's layout group, not
directly through the active world group. Active groups 6 and 7 can therefore
reuse the two large-layout families while retaining distinct room identity and
state.

## Dictionary stream

The stream is divided into groups of up to eight commands. Each group begins
with one key byte, consumed least-significant bit first:

- key bit `0`: copy one literal byte from the stream;
- key bit `1`: read one packed little-endian dictionary reference.

A packed reference contains:

```text
bits  0–11  dictionary offset
bits 12–15  copy length minus 3
```

References may copy between 3 and 18 bytes. Decoding stops after the complete
176-byte padded buffer, even if the last reference extends beyond the required
length.

The implementation validates truncated literals, truncated references,
dictionary bounds, room-offset bias, and layout dimensions. The generic
dictionary decoder has a native unit fixture. Representative Ages and Seasons
rooms were also compared against the decompressed files produced by
`oracles-disasm`; their compact layout signatures match byte-for-byte.

## Inspection commands

Open a large room:

```sh
oracle_room_slice oracle.gbc --group 4 --room c3
```

Export its native 240×176 background without opening a window:

```sh
oracle_room_slice oracle.gbc \
  --group 4 --room c3 --tick 60 \
  --export-region build/large-room-c3.bmp
```

`--describe` reports `region=large-room`, the 15×11 metatile dimensions, the
240×176 pixel dimensions, and deterministic layout and animation signatures.

`--atlas` remains limited to small-layout groups. Large room ids are catalog
indices, not coordinates in a seamless 16×16 map.

## Fidelity boundary

The rendered result includes the immutable room layout, tileset mapping,
graphics, palettes, animation frames, and standard persistent substitutions.
It does not yet include:

- chests and other object-specific visuals represented by the object system;
- enemies, NPCs, particles, or interface sprites;
- common dynamic substitutions such as shutters and toggle blocks;
- room-specific pre- and post-expansion hooks;
- connectivity inferred from doors, stairs, warps, or dungeon metadata.

The next useful dungeon slice is a topology catalog: recover exits and warps,
associate each large room with its dungeon, and build a navigable room graph.
