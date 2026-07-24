# Room mutation pipeline

Oracle Redux keeps the immutable ROM layout separate from mutations caused by
save state and live simulation. The original games already make this
distinction: they decompress a room, apply reusable substitutions, and then run
room-specific hooks around graphics expansion.

## Original load order

The shared high-level order recovered from `oracles-disasm` is:

```text
loadTilesetAndRoomLayout
        |
decompress base room metatiles
        |
roomTileChanges.applyAllTileSubstitutions
        |
generateVramTilesWithRoomChanges
        |
expand metatiles into tile/attribute buffers
        |
applyRoomSpecificTileChangesAfterGfxLoad
```

This produces three useful C++ layers:

1. immutable content decode from the user-supplied ROM;
2. generic state-driven metatile substitutions;
3. campaign and room-specific hooks with typed simulation state.

Keeping the stages explicit avoids making the renderer interpret save flags.
It also lets Classic and Modern presentation profiles consume the same mutated
world state.

## Standard persistent substitutions

The first generic layer is implemented by `RoomMutationDecoder`. The original
tables contain `(replacement, target)` byte pairs, terminated by a zero
replacement. For each enabled persistent room flag, every matching target
metatile is replaced. Flags run in original order, so a later substitution can
observe the output of an earlier one.

The games use room flag bits `0`, `1`, `2`, `3`, and `7`. Their table selectors
differ:

| Campaign | Selector | Table file offset |
| --- | --- | ---: |
| Ages | active collision mode from the tileset descriptor | `0x120f5` |
| Seasons | active room group | `0x11e26` |

The corresponding symbols are `standardTileSubstitutions` in bank 4. Ages has
six collision-mode entries; Seasons has eight group entries.

The standalone viewer exposes `--room-flags HEX` for deterministic inspection.
For now, the selected byte is applied to every room in the 3×3 preview. This is
deliberately not presented as save emulation: the eventual runtime will supply
one persistent flag byte per room.

`--describe` prints a `layout_signature` over room coordinates and post-mutation
metatiles. It makes a substitution observable even when the number of unique
metatiles happens to remain unchanged.

## Dynamic shared substitutions

The next layer should model common rules that need more context than one
persistent byte. The original room-change modules cover reusable cases such as:

- opened chests and key blocks;
- shutters and toggle blocks;
- underwater or dungeon water-state variants;
- other state-derived metatile swaps performed during room loading.

These should receive a narrow, read-only `RoomStateView`, not raw global memory.
A suitable first shape is:

```cpp
struct RoomStateView {
    std::uint8_t persistent_flags;
    std::uint8_t dungeon_flags;
    std::uint8_t water_level;
    bool linked_game;
};
```

The exact fields remain provisional until each shared routine is traced. The
interface should grow from demonstrated dependencies rather than mirror all
Game Boy WRAM.

## Room-specific hooks

Both campaigns have jump-table-driven room-specific code before or after
graphics expansion. Examples in the disassembly include gasha spots, seed
trees, shops, the Maku tree, dungeon entrances, the pirate ship, and Blaino's
gym. These routines mix persistent state, objects, and sometimes direct tile
buffer edits.

They should not become a single switch in the renderer. The intended boundary
is a campaign-owned registry keyed by room identity:

```text
RoomMutationPipeline
    -> apply standard substitutions
    -> apply shared dynamic rules
    -> invoke campaign pre-expansion hook
    -> expand metatiles
    -> invoke campaign post-expansion hook
```

Hooks that only replace metatiles belong before expansion. Hooks that reproduce
direct tile or attribute edits require a typed post-expansion surface. This
separation preserves original ordering while leaving room for a later
high-resolution renderer to implement an equivalent semantic effect.

## Next implementation target

The next narrow slice is chest state:

1. trace the Ages and Seasons chest substitution routines and their state
   inputs;
2. add per-room persistent state to the simulation region;
3. expose that state through `RoomStateView`;
4. apply the shared chest rule before graphics expansion;
5. verify closed and opened layouts against deterministic ROM-backed fixtures.

This exercises per-room state without yet requiring objects, combat, or a full
save-file implementation.
