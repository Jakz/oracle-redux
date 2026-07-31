# Chest and treasure data

## Slice outcome

The first persistent-treasure slice decodes chest placement and contents from
the player-supplied US ROM, opens one representative rupee chest in either
campaign, awards its ROM-derived value, and stores the original per-room item
flag. The standalone executable selects the appropriate fixture with
`--chest-scenario`; a ROM-only launch selects this newest playable slice.

| Campaign | Room | Chest YX | Chest contents | Parameter | Award | Spawn YX |
| --- | --- | --- | --- | --- | --- | --- |
| Ages | `04:5c` | `$27` | `$28:$04` | `$07` | 30 rupees | `$47` |
| Seasons | `04:05` | `$5d` | `$28:$04` | `$07` | 30 rupees | `$7d` |

These are existing visible dungeon chests, not presentation-only props. The
runtime starts Link immediately below the selected chest and facing north.
Press `Z` or Enter to open it.

## Retail chest table

`getChestData` selects one of eight group pointers using `wActiveGroup`, then
walks four-byte records until a position byte of `$ff`:

```text
byte 0  packed wRoomLayout Y/X position
byte 1  low room ID
byte 2  treasure index
byte 3  treasure-object subid
```

The final two bytes are emitted big-endian by `m_ChestData`, which is why a
rupee record appears as `28 04`. The relocated US table coordinates are:

| Campaign | Bank:address | File offset |
| --- | --- | --- |
| Ages | `16:5108` | `$59108` |
| Seasons | `15:4f6c` | `$54f6c` |

The implementation follows the group pointers instead of copying the assembly
catalog. `ChestDataDecoder::decode_group` therefore remains useful as the
supported treasure families expand.

## Packed position conversion

The original `wRoomLayout` reserves 16 bytes per metatile row. Small rooms use
only ten of those columns; large rooms use fifteen. Native room layouts remove
the unused padding, so position `$yx` converts as:

```text
compact index = y * native room columns + x
```

This conversion is intentionally separate from Link's packed object position,
whose Y coordinate includes the original status-bar convention. For the
playable fixtures, Link's spawn is two packed Y rows below the chest table
position, which places his local collision center one metatile below it.

## Treasure descriptor indirection

The chest table does not contain the reward amount or graphics. Its `$28:$04`
pair selects `TREASURE_RUPEES`, subid 4, through `treasureObjectData`:

```text
byte 0  spawn/grab behavior flags  = $38
byte 1  giveTreasure parameter     = $07
byte 2  pickup text low ID         = $05
byte 3  treasure graphic           = $2b
```

Main treasure entries are four bytes. If bit 7 of byte 0 is set, bytes 1-2 are
a little-endian pointer to a four-byte-per-subid table in the same ROM bank.
The decoder handles both inline and indirect descriptors.

`$07` is a rupee *kind*, not the number seven. `getRupeeValue` indexes the
bank-0 rupee table, whose words use the original packed-decimal representation.
Both ROMs store `$0030` for kind `$07`; the native decoder validates each BCD
nibble and returns decimal 30. Values above the retail table range clamp to its
last entry, matching `getRupeeValue`.

## Interaction and persistence

The bounded native path mirrors the important `nextToChestTile` decisions:

1. consume a semantic A/confirm press;
2. require Link immediately below the chest and facing north;
3. replace its metatile with `TILEINDEX_CHEST_OPENED` (`$f0`);
4. resolve the treasure descriptor and award its rupee parameter;
5. set `ROOMFLAG_ITEM` (`$20`) for that world room;
6. reject repeat collection;
7. reapply `$f0` when a fresh room layout is loaded.

The SDL slice refreshes only the changed room: its layout, collision map,
rendered-room cache, CPU region pixels, and one texture sub-rectangle. Opening
a chest does not recompose the atlas or upload the whole world.

The state representation is deliberately the original room flag so the future
Original Save Image adapter can map it without translating a Redux-specific
achievement. The current state survives room reloads during the process. SRAM
import/export is part of the persistence-spine slice and is not claimed here.

## Current fidelity boundary

Implemented:

- campaign-relocated chest and treasure tables;
- exact group/room lookup and packed layout position;
- original bottom-facing A rule;
- original opened metatile and item flag;
- ROM-derived rupee kind and packed-BCD amount;
- 999-rupee wallet cap;
- repeat-collection prevention and room-reload substitution;
- dual-ROM headless tests and an MSVC/CMake launch path.

Still provisional or deferred:

- the raised treasure interaction, its `$2b` sprite, and its timing;
- pickup text `$0005`, chest sound, and object-disable interval;
- non-rupee treasure families;
- puzzle/event conditions that cause dynamically hidden chests to appear;
- persistence to an Original Save Image.

The relevant disassembly sources are
`data/{ages,seasons}/chestData.s`, `code/bank0.s:getChestData`,
`code/interactableTiles.s:nextToChestTile`,
`code/commonTileSubstitutions.s:replaceOpenedChest`, and
`data/{ages,seasons}/treasureObjectData.s`.
