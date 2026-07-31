# Ordinary hole pull, fall, and local respawn

## Shared retail path

Both campaigns use the shared `@tileType_hole` branch in
`object_code/common/specialObjects/commonCode.s` and `linkPullIntoHole` plus
`linkState02` in `object_code/common/specialObjects/link.s`. The C++ runtime
therefore implements this as one campaign-neutral player state machine fed by
the ROM-derived `LinkTileContact`.

The active contact retains the sampled metatile row and column as well as its
type. This is necessary because retail resets `wStandingOnTileCounter` when
the active tile changes, and primes it to `$0e` when Link moves directly from
one hole tile to another hole tile.

## Pull rules

`linkPullIntoHole` uses the low two bits of the standing counter:

| `counter & 3` | Action |
| ---: | --- |
| 0 | move Y one pixel toward the metatile center |
| 1 | move X one pixel toward the metatile center |
| 2 or 3 | no automatic movement |

Link retains partial player control for counters below `$10`. At `$10` and
above, retail sets immobilized bit 4. Center detection runs only after an X or
Y pull and accepts an integer within-tile coordinate of 7, 8, or 9 on both
axes. The following state-initialization tick snaps Link to the exact 8,8
center. Holes are ignored while `wLinkInAir` is nonzero. Feather landing
primes the standing counter to 4, matching the shared landing code.

## Fall and respawn timing

`LINK_STATE_RESPAWNING` parameter 0 disables object collision, centers Link,
selects `LINK_ANIM_MODE_FALLINHOLE`, and plays `SND_LINK_FALL` (`$65`). Both
campaigns contain identical animation records:

| Campaign label | Record |
| --- | --- |
| Ages `animationData19ef3` | `$10 $08`, `$0a $09`, `$0a $0a`, end |
| Seasons `animationData19c59` | `$10 $08`, `$0a $09`, `$0a $0a`, end |

The shared native sequence is consequently 16 ticks of original Link frame
`$08`, 10 of `$09`, and 10 of `$0a`. At animation completion Link is moved to
the saved `wLinkLocalRespawnY/X/Dir` equivalent and hidden for two ticks. He
then reappears, receives signed damage `$fc` (four quarter-heart units), gets
`$3c` invulnerability ticks, and remains input-captured with object collision
disabled for a final `$10` ticks.

Retail updates the local respawn coordinates deliberately during screen/warp
setup and selected scripts through `updateLinkLocalRespawnPosition`; it is not
a continuously moving “last safe tile.” `PlayerHazardRuntime` therefore owns
an explicit checkpoint updated by the room slice on a completed warp or room
seam.

## Deliberate boundaries

- `TILETYPE_WARPHOLE` reaches dungeon-specific topology code after the same
  fall animation. It is not treated as an ordinary damaging hole in this
  slice.
- The Gold Luck Ring halves `$fc` to `$fe`; ring equipment policy is not yet
  available, so ordinary damage remains four health units.
- Zero health is exposed as `fatal_damage`, but the game-over state machine is
  still pending.
- Sound command `$65`, respawn-tile breaking, and the retail
  `wEnteredWarpPosition` guard await their owning audio/tile/warp systems.

## Native verification

Headless coverage pins alternating pull axes, the 7–9 center window, airborne
hole immunity, identical 16/10/10 animation timing, two hidden ticks, saved
position and facing restoration, four-unit damage, 60 invulnerability ticks,
and 16 recovery ticks. The same runtime is compiled into both CMake and MSVC
targets.
