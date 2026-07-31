# Roc's Feather top-down slice

## Implemented boundary

Oracle Redux now provides a native C++ level-one `ITEM_FEATHER` (`$17`) path
shared by Ages and Seasons. The player-supplied ROM still supplies Link's
graphics and OAM composition; the runtime reimplements the documented parent
item and airborne state transitions.

This checkpoint covers ordinary top-down use:

- semantic A-button edge input;
- original parent item slot 1 allocation and same-update release;
- `wLinkInAir` state 1 advancing to state 2;
- signed 8.8 `SpecialObject.z` and `speedZ` state;
- initial `speedZ = -$01e0` and gravity `$0020` per logic tick;
- the original `$0300` terminal fall-speed cap;
- ground-plane clamping and airborne-state clearing on tick 31;
- directional `LINK_ANIM_MODE_JUMP` frames decoded from each supplied ROM;
- renderer-independent elevation with interpolated SDL presentation.

## Source mapping

Both cartridges use item id `$17` and usage parameter `$01`, selecting parent
slot 1 on a just-pressed button edge. The shared implementation is documented
by `object_code/common/itemParents/featherParent.s` and
`object_code/common/specialObjects/link.s::linkUpdateInAir` in
`oracles-disasm`.

The animation mode is shared, but its relocated data differs:

| Campaign | Jump animation | Directional frame bases | Durations |
| --- | --- | --- | --- |
| Ages | `animationData19f78` | `$e4`, `$e8`, `$ec` | 9, 9, 6 ticks |
| Seasons | `animationData19cd6` | `$d8`, `$dc`, `$e0` | 9, 9, 6 ticks |

Each base receives Link's direction index 0 through 3. Rendering therefore
continues through `LinkSpriteDecoder::decode_original_frame`; no Nintendo
graphics are embedded in the executable or repository output.

## Presentation boundary

`PlayerState` retains original-style fixed-point Z as authoritative gameplay
state. Presentation converts negative Z to positive visual elevation. The
ground position and shadow do not move, while only the Link sprite is raised.
This is the same separation needed later by a 2.5D renderer, but it does not
make rendering authoritative for collision or landing.

## Current limitations

This is intentionally the level-one top-down checkpoint. The following remain
separate fidelity work:

- side-scrolling Feather physics;
- Seasons level-two Roc's Cape chaining;
- hole, water, lava, trampoline, and cliff landing reactions;
- Z-aware enemy, projectile, and solid-actor contact;
- jump and landing sound events in the future audio service.

Until tile-type reactions are implemented, special terrain that is enterable
under Link's ordinary collision profile does not yet trigger its retail fall,
swim, or damage state. This limitation predates the Feather and is not hidden
by the visual elevation.

## Verification

Headless tests run the complete top-down arc in both campaigns, check the
first fixed-point integration result, animation phase boundaries, parent-slot
release, the tick-31 landing, side-scrolling rejection, and occupied-slot
failure. ROM-backed tests also decode an opaque directional jump pose from
both US cartridges.
