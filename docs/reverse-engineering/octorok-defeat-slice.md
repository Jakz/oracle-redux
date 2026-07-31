# Octorok Defeat and Drop Slice

## Outcome

A red Octorok now follows the shared retail defeat continuation instead of
disappearing immediately. The level-one sword applies its original two damage
units, the enemy flashes through sprite palette 5, moves through 11 ticks of
knockback, becomes a 20-tick `PART_ENEMY_DESTROYED` puff, and then evaluates
the Octorok item-drop table. A selected heart, one-rupee, or five-rupee drop
uses the bounded part band, original ROM graphics and OAM, vertical bounce,
flickering lifetime, and collection reward.

The path is native C++. Graphics, OAM composition, palettes, constants, and
drop tables are derived from the player-supplied cartridge.

## Sword collision result

Both campaigns define `ITEM_SWORD` `$05` with damage byte `$fe` in
`itemAttributes`. `COLLISIONEFFECT_SWORD` selects `ENEMYDMG_04`, whose shared
damage record is:

```text
f1 15 0b 00
```

The flags apply damage, collision metadata, invincibility, knockback, and the
normal enemy-damage sound. The remaining bytes specify 21 invincibility ticks
and 11 knockback ticks. Red Octorok `enemyData` supplies health 2, so the
level-one sword defeats it in one hit.

While invincibility is active, the retail enemy update substitutes sprite
palette 5 whenever frame-counter bit 2 is clear. The renderer predecodes the
same eight direction/phase OAM frames with gameplay palette 5 and switches
between cached textures. No ROM decoding occurs in the presentation loop.

Knockback uses `SPEED_200`, two pixels per tick, in the cardinal direction
away from Link. Terrain can stop it early. Health-zero handling waits until
the knockback counter has drained, matching `enemyStandardUpdate` returning
`ENEMYSTATUS_KNOCKBACK` before `ENEMYSTATUS_NO_HEALTH`.

## Enemy-destroyed part

`enemyDie` creates `PART_ENEMY_DESTROYED` (`$02`) in the original 16-slot
part band and copies the enemy position. The normal, non-knockback puff uses
these animation records:

| Ticks | Frame byte | OAM index |
| --- | --- | --- |
| 0-1 | `$00` | 0 |
| 2-3 | `$02` | 1 |
| 4-5 | `$00` | 0 |
| 6-9 | `$04` | 2 |
| 10-13 | `$06` | 3 |
| 14-17 | `$08` | 4 |
| 18-19 | `$0a` | 5 |

The final animation parameter `$ff` triggers the drop decision. The generic
part decoder resolves `partOamDataTable`, the part-specific pointer table,
OAM data, `partData`, object graphics header 0, tile base `$0c`, and palette
2 independently in each campaign.

## Octorok drop decision

Octorok ID `$09` maps to drop-table byte `$8e` in both cartridges:

- bits 5-7 select probability row 4;
- bits 0-4 select item set `$0e`;
- the first retail RNG sample chooses one of 64 probability bits;
- if set, a second sample chooses one of 32 item-set entries.

Set `$0e` contains hearts (`$01`), one rupee (`$02`), and five rupees (`$03`).
All three are unconditionally available in the shared availability table.
The runtime uses the same 16-bit `getRandomNumber_noPreserveVars` transition
already driving Octorok decisions, so enemy actions and drops remain one
deterministic stream.

## Item-drop part

A successful decision replaces the puff with `PART_ITEM_DROP` (`$01`) in the
same slot and generation, as `objectReplaceWithID` does. The decoder applies
the item-specific tile-base and palette records from `partCode01@spriteData`
and resolves the campaign-relocated graphics headers:

| Property | Ages | Seasons |
| --- | --- | --- |
| item-drop graphics header | `$78` | `$5c` |
| heart tile/palette | `$02/$05` | `$02/$05` |
| one-rupee tile/palette | `$04/$00` | `$04/$00` |
| five-rupee tile/palette | `$06/$05` | `$06/$05` |

The drop begins with vertical speed `-$160` in original signed-Z notation,
uses gravity `$20`, and negates and halves its speed on each ground contact
until the rebound is below one pixel per tick. It then waits for the original
240 half-rate countdown steps, equivalent to 480 runtime ticks, flickering
during the final 120 ticks. A collected heart restores four health units;
rupee drops add one or five, capped at 999.

## Verification

The native ROM-backed suite runs against exact US Ages and Seasons cartridges
and verifies:

- all six puff OAM frames and all three possible Octorok drops decode to
  nonempty authentic pixels;
- the sword applies two damage and exposes the palette-flash phase;
- the defeated actor remains present for all 11 knockback ticks and moves
  away from Link;
- the enemy slot becomes a part slot and the puff reaches OAM frame 5 at the
  end of its 20-tick sequence;
- seed `$5a17` selects no drop through the exact probability bitset;
- seed `$0003` selects a heart through set `$0e`;
- the heart bounces, settles, restores four health, and releases its slot;
- identical seeds and update sequences remain deterministic across both
  campaigns.

## Deferred boundary

Room kill flags and total/Maple/Gasha kill counters are not persistent yet,
so reloading the room can respawn the source record. Joy Ring reward
multipliers, hazards, conveyors, fairy motion, sound effects, and non-Octorok
drop tables also remain outside this slice.

The next first-playable boundary is one secondary item and one persistent
pickup or chest, followed by the typed save-state spine.
