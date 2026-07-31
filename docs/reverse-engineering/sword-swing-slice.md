# Sword Swing Slice

## Outcome

The ordinary level-one sword swing is now a native C++ item path shared by
both campaigns. Semantic action B allocates `ITEM_SWORD` (`$05`) in original
parent slot 2 and weapon slot 6, freezes movement for the swing, advances the
retail Link attack frames, positions the child item through `swordArcData`,
and renders cartridge-decoded sword pixels. The former diagnostic rectangle
is gone.

Charging, sword spin, sword poke, sword beams, rings, higher sword levels,
tile breaking, knockback, hit effects, sound, and drops remain outside this
slice.

## Parent and child contract

`itemUsageParameterTable` contains `$63, wGameKeysJustPressed` for the sword in
both games. The high nibble selects priority 6; low-nibble mode 3 selects
`w1ParentItem2`. The shared `parentItemCode_sword` then:

1. disables Link movement and turning;
2. selects `LINK_ANIM_MODE_22`;
3. creates child `ITEM_SWORD` in `w1WeaponItem`;
4. keeps the child related to the parent until animation parameter bit 7 marks
   the end of the swing.

`SwordRuntime` preserves this ownership boundary with the bounded
`ActorSlotDomain`: slot 2 owns the parent action and slot 6 (`w1WeaponItem`)
owns the positioned child, rather than using dynamic item slots 7-b.

## Link timing

`LINK_ANIM_MODE_22` is relocated but equivalent:

| Campaign | Pointer entry | First record |
| --- | --- | --- |
| Ages | `06:5cf5` entry `$22` | `06:5fef` |
| Seasons | `06:5a65` entry `$22` | `06:5d48` |

The records intentionally run into the following animation label. Direction
is added to every frame because all bases are at least `$54`.

| Logic ticks | Base frame | Parameter | Meaning |
| --- | --- | --- | --- |
| 0-2 | `$ac` | `$00` | first swing pose |
| 3-5 | `$b0` | `$02` | second arc pose |
| 6-13 | `$b4` | `$64` | third pose; bit 6 requests tile break |
| 14-16 | `$b0` | `$06` | final active pose |
| 17 | `$b0` | `$86` | bit 7 ends the swing |

The C++ slice exposes the four active records and ends before the `$86` record
can collide. `LinkSpriteDecoder::decode_original_frame` uses the selected
retail frame, so Link no longer remains in his walking pose while attacking.

## Arc mapping

`updateSwingableItemAnimation` packs an arc index in the high nibble and item
animation index in the low three bits:

```text
02 41 80 c0
10 51 92 d2
26 65 a4 e4
30 77 b6 f6
```

Rows are north, east, south, and west. Columns are animation parameters
`$00/$02/$04/$06`. This means the sword sweeps around Link rather than
occupying one forward rectangle. For example, north starts at arc 0:
radius Y/X `$09/$06`, offset Y/X `-$02/+$10`; its later active records are
arcs 4, 8, and 12.

The first 16 records of shared `swordArcData` are represented byte-for-byte as
signed offsets and unsigned collision radii. Combat consumes that same pose
which presentation renders, and an enemy can still be damaged only once per
swing.

## Sword graphics and OAM

The child `itemData` record is identical:

| Data | Ages | Seasons | Value |
| --- | --- | --- | --- |
| `itemData` | `3f:63a5` | `3f:63a3` | relocated |
| item `$05` | entry 5 | entry 5 | gfx `$00`, tile `$52`, flags `$08` |
| item `$05` OAM pointers | `07:68ca` | `07:6668` | 8 pointers |
| OAM data bank | `13` | `12` | relocated, identical records |
| weapon graphics | `1a:6de0` | `1a:6dc0` | `spr_swords`, 10 tiles |

`UNCMP_GFXH_1a` copies ten 16-byte tiles from `spr_swords` into VRAM tile
`$52`. The decoder resolves those source addresses from the campaign's
`UNCMP_GFXH_1a` record (file offsets `$06ade0` / `$06adc0`), resolves the
original one- or two-object OAM composition, applies horizontal and vertical
flip flags, and reads palette 0 from the sword's `itemData` flags and gameplay
palette header `$0f`.

The eight animation indices resolve to frame bytes `$00,$02,...,$0e`. Their
OAM pointers select the shared bank-relative records `$4f69`, `$4ffc`,
`$4fb4`, `$500e`, `$4f6e`, `$5005`, `$4fab`, and `$4ff3`. These are file
offsets `$04cfxx/$04d0xx` in Ages bank 13 and `$048fxx/$0490xx` in Seasons
bank 12.

### Presentation coordinates

Game Boy OAM stores sprite Y with a hardware bias: the visible top of an
8-by-16 object is `OAM Y - 16`. The runtime already expresses Link and item
positions in world coordinates without that bias, so both Link and sword
decoders retain a signed draw origin relative to their world anchor. The
sword child is also drawn two pixels above its logical Y, matching
`itemInitializeFromLinkPosition` setting `Item.zh = Link.zh - 2`.

Attack poses are not constrained to a synthetic 16-by-16 canvas. Their OAM
bounds may extend above or sideways from Link (`$b4` shifts the north pose
upward, for example), and the decoder expands the returned frame so those
pixels are preserved instead of clipped.

All eight sword frames and all eight Octorok direction/phase frames are
decoded and uploaded once when the renderer starts. Link uploads only when
its selected original frame changes, and Vasu does the same for its selected
OAM frame. Animated-room checks reuse cached tileset descriptions. This keeps
ROM decoding out of the per-present path, which is particularly important in
an unoptimized Visual Studio Debug build.

## Verification

The native tests cover both exact US cartridges and assert that:

- the attack Link frame can be decoded by its retail frame index;
- shifted Link attack OAM expands its bounds without clipping;
- all eight sword OAM compositions decode to nonempty cartridge pixels;
- the opening north sword visual is anchored above-right of Link after the
  hardware Y bias and original two-pixel elevation are applied;
- a press allocates parent slot 2 and weapon slot 6, then selects north arc 0
  / animation 2;
- the four retail timing ranges select the expected parameters and poses;
- the slot is released at the end marker;
- a second press can allocate a new generation;
- the shared arc damages each Octorok once and the level-one sword's original
  two-damage byte defeats a red Octorok in one swing.

## Next boundary

The enemy defeat aftermath is implemented and documented in
[`octorok-defeat-slice.md`](octorok-defeat-slice.md). The next combat boundary
is a secondary item plus a persistent pickup or chest.
