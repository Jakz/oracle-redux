# Room object bytecode

Background metatiles do not contain every visible thing in a room. NPCs,
enemies, signs, effects, collectibles, and many apparently static decorations
are created by the object system. This explains why a decoded background can
be structurally correct while some ground-level sprites are absent.

## Entry tables

| Campaign | Group-pointer table | Pointer bank | Object-data bank |
| --- | ---: | ---: | ---: |
| Ages | `0x5432b` | `0x15` | `0x12` |
| Seasons | `0x45b3b` | `0x11` | `0x11` |

Each group selects a 256-entry room pointer table. The selected stream is the
same compact bytecode interpreted by the original `parseObjectData` routine.

## Decoded operations

| Low opcode | Meaning | Current catalog result |
| ---: | --- | --- |
| `0` | room-state conditional | following records marked conditional |
| `1` | interaction without coordinates | identity retained |
| `2` | positioned interaction | identity and full Y/X retained |
| `3` | object-data pointer | recursively followed |
| `4` / `5` | before/after-event pointer | selected from room flag bit 7 |
| `6` | random-position enemies | count and identity retained |
| `7` | specifically positioned enemies | identity and Y/X retained |
| `8` | parts with packed YX | expanded to pixel centers |
| `9` | parameterized interaction/enemy/part | type, parameter, and Y/X retained |
| `a` | Ages item drops | item and packed position retained |
| `e` / `f` | pointer return / stream end | terminates the current branch |

Original object Y coordinates include the 16-pixel status-bar row. The modern
world-space anchor therefore uses `original_y - 16`; X is already local room
space.

## Viewer diagnostics

The viewer reports `room_object_count` and `positioned_object_count`. Press
`F3`, or launch with `--objects`, to show the unresolved spawn anchors:

- cyan: interactions;
- red: enemies;
- yellow: parts;
- magenta: item drops;
- translucent: conditional entries.

These markers are diagnostic only. They make missing dynamic content
measurable without pretending that a generic icon is the actual object.

## Next implementation boundary

The next actor slice needs to connect each catalog record to the original
object graphics header, animation data, OAM composition, and update handler.
That layer will own explicit render layers and Y sorting, following the
presentation architecture described in `link-sprite-rendering.md`.
