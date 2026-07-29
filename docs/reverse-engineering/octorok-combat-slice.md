# Octorok Combat Slice

## Outcome

`ENEMY_OCTOROK` (`$09`) is the first native combat family. It is a useful
boundary because `object_code/common/enemies/octorok.s` supplies one shared
behavior implementation to both campaigns while the cartridges relocate its
data, graphics, animation, and OAM tables.

The `--octorok-scenario` launch selects a real positioned red Octorok:

| Campaign | Room record | Enemy Y,X | Link spawn |
| --- | --- | --- | --- |
| Ages | `group0Map64EnemyObjectData`, room `00:64` | `$48,$48` | `$64` |
| Seasons | `group0Mapa6EnemyObjectData`, room `00:a6` | `$48,$68` | `$66` |

Room records retain the original status-bar coordinate convention. The native
actor anchor therefore subtracts 16 from record Y, just like the existing room
actor loader.

## Cartridge data map

The four-byte `enemyData` entry points to a terminated two-byte subid table.
Subid zero selects extra-data index `$08` and visual byte `$20` in both games.

| Data | Ages | Seasons |
| --- | --- | --- |
| `enemyData` | `3f:5d4b` | `3f:5d71` |
| enemy `$09` entry | `3f:5d6f` | `3f:5d95` |
| `enemy09SubidData` | `3f:5f4b` | `3f:5f71` |
| `extraEnemyData` | `3f:5fb9` | `3f:5ff3` |
| selected extra entry `$08` | `3f:5fd9` | `3f:6013` |
| object graphics header table | `3f:5a8a` | `3f:5afb` |
| selected object header | `3f:5c37` (`$8f`) | `3f:5c57` (`$74`) |
| Octorok graphics | `1a:67e0` | `1a:6780` |
| enemy animation table | `0d:6d5c` | `0c:6df7` |
| enemy `$09` animations | `0d:6fec` | `0c:707f` |
| enemy OAM table | `0d:6e5c` | `0c:6ef7` |
| enemy `$09` OAM pointers | `0d:7b23` | `0c:7ad0` |

The selected properties are identical:

- collision enabled, collision mode `$10`;
- collision radius Y/X `$06/$06`;
- signed contact damage `$fe` (`-2`, half a heart in quarter-heart units);
- health `$02`;
- sprite palette 2 and relative tile base 0.

The object graphics table is important: its first byte carries both ROM bank
and compression mode. Both Octorok entries are `1a 67 e0` / `1a 67 80`, so
this sheet uses mode 0, not Vasu's mode 3. Reading that mode from the ROM avoids
silently producing corrupt but nonempty pixels.

Each direction owns the retail two-frame, eight-tick loop:

```text
north: frame bytes $00,$02
east:  frame bytes $04,$06
south: frame bytes $08,$0a
west:  frame bytes $0c,$0e
```

The OAM decoder also applies Game Boy hardware origins. Object X is written
directly to OAM and displays eight pixels to its left. Object Y is first
offset by `$10`, cancelling the hardware's `-16` origin before per-sprite Y is
added. This preserves the original ground anchor instead of merely drawing a
16x16 bitmap around an assumed center.

## Native behavior boundary

`OctorokRuntime` implements original states `$08-$0b`:

1. decide whether to stand or prepare a shot;
2. stand for a table-selected counter;
3. walk for `$19/$21/$29/$31` ticks at `SPEED_80` (or `SPEED_c0` when subid
   bit 1 is set);
4. request an Octorok projectile after the original 16-tick wind-up, then
   stand for 32 ticks.

The runtime uses the exact shared `getRandomNumber_noPreserveVars` transition:

```text
intermediate = (hRng2:hRng1) * 3 modulo 65536
hRng2 = high(intermediate)
hRng1 = high(intermediate) + old hRng1 modulo 256
```

The routine is relocated at Ages `00:0453` and Seasons `00:042f`. Octorok code
is likewise relocated at Ages `0d:458c` and Seasons `0c:45ab`, but comes from
one common source implementation.

Enemy terrain movement is fixed-step and cardinal, samples the existing
grounded-actor collision profile, and retains subpixel speed. A blocked move
uses the original random-cardinal recovery. The native terrain solver does
not yet reproduce the general enemy wall-sliding helper for non-cardinal
families.

## Combat contract

Enemy contact and solid NPC bodies remain separate:

- Octoroks publish their 6x6 damage radius but set `blocks_player=false`;
- contact removes two of Link's twelve quarter-heart units;
- a bounded 60-tick player damage-invincibility window prevents repeated
  overlap damage;
- `X` maps to semantic action B and starts the native 17-tick sword arc;
- each swing can damage a given actor once; two one-damage swings release the
  two-health Octorok slot.

The sword now follows the original parent/child item slot, Link animation,
arc-position, collision, graphics, and OAM path documented in
[`sword-swing-slice.md`](sword-swing-slice.md). Knockback, enemy hit effects,
drops, tile breaking, charging/spin states, and sound are not yet claimed.

## Projectile part path

The shooting request now allocates `PART_OCTOROK_PROJECTILE` (`$18`) in the
original 16-slot part band and copies its source Octorok room, position, and
cardinal angle. The generic `PartDefinitionDecoder` reads the retail eight-byte
`partData` record:

| Property | Ages | Seasons | Normalized value |
| --- | --- | --- | --- |
| `partData` | `3f:60cd` | `3f:6103` | campaign relocation |
| part `$18` graphics header | `$8f` | `$74` | same Octorok sheet |
| collision mode | `$87` | `$87` | enabled, mode `$07` |
| collision radius | `$22` | `$22` | Y/X `$02/$02` |
| damage | `$fc` | `$fc` | four quarter-heart units |
| tile base / OAM flags | `$0c/$03` | `$0c/$03` | palette 3 |

The part animation's first frame resolves through relocated pointer tables at
Ages `16:7c52` and Seasons `15:77a7`. Both select a two-object 16x16 OAM
composition using relative tile zero from the Octorok graphics sheet.

The native state path preserves the meaningful retail constants:

1. initialize visible with raw speed `$50`, which is 2 pixels per tick in the
   original speed table;
2. travel cardinally while the projectile remains inside its room and the
   decoded terrain sample is passable;
3. on terrain or player impact, disable further contact, reverse the angle,
   use `SPEED_40` (0.25 pixels per tick), and start vertical speed `-$e0`;
4. apply gravity `$0e` and release the part slot after counter `$20`.

The diagnostic presentation keeps the projectile's ground anchor for ordering,
renders its airborne height independently, and leaves the shadow at ground
level. Player contact applies the decoded four-unit damage through the same
60-tick invincibility gate as enemy contact.

## Verification

The headless dual-ROM tests assert:

- campaign-relocated graphics header IDs converge on the same properties;
- all four table layers decode the expected radii, damage, health, palette,
  and tile base;
- authentic two-object OAM produces a nonempty 16x16 frame;
- both developer rooms contain a positioned red Octorok;
- contact damage is invulnerability-gated and does not enter the actor solid
  collision snapshot;
- projectile attributes and nonempty 16x16 OAM decode identically from both
  campaign relocations;
- shooting requests allocate part slots, terrain produces the impact and
  reverse-bounce states, and the 32-tick lifetime releases the slot;
- projectile contact removes four health units and remains separate from
  solid NPC collision;
- two separate sword presses produce hit then defeat;
- identical seeds and tick sequences reproduce native runtime state;
- both campaigns traverse the same shared behavior state.

## Next boundary

Add original hit/death feedback and RNG-backed enemy drops now that the
representative enemy, part, and sword item paths are connected.
