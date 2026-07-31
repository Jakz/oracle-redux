# Water entry, swimming, drowning, and respawn

## Shared top-down entry path

Both campaigns dispatch `TILETYPE_WATER` through `@swimming` in
`object_code/common/specialObjects/commonCode.s::linkApplyTileTypes`. The
global airborne check runs first, so water does not take effect until Link is
on the ground. On the first grounded contact, retail clears the swim-stroke
state and knockback counter, sets `wLinkSwimmingState` to 1, and lets
`linkUpdateSwimming` consume the transition later in the same Link update.

The native `PlayerHazardRuntime` follows this shared boundary. Water remains
enterable terrain; the typed foot contact selects swimming or drowning after
movement instead of changing the collision map.

## Capability policy

`overworldSwimmingState1` calls `checkTreasureObtained` for
`TREASURE_FLIPPERS` (`$2e`). The observable top-down policy is:

| Contact | Capability | Result |
| --- | --- | --- |
| ordinary water, either campaign | none | drown |
| ordinary water, either campaign | Flippers | swim |
| ordinary water, Ages | Mermaid Suit | swim |
| Ages seawater | none or Flippers | drown |
| Ages seawater | Mermaid Suit | swim |

Seawater is emitted only by the Ages top-down tile-type tables. The Mermaid
Suit sets Link `var2f` bit 6 during special-object setup; the Ages-only
`checkSwimmingOverSeawater` branch uses that bit rather than accepting the
Flippers.

The first native scenario grants the Flippers explicitly. This is a developer
capability, not a fabricated original save or an implicit global inventory
grant. `F4` toggles it so the same ROM-derived water edge can exercise both
branches.

## Entry lock and movement

Capable Link enters swimming state 2 with `counter1=$0a`. While it counts
down, retail updates position at the already selected swimming speed without
returning to normal movement selection. The Mermaid Suit shortens this entry
lock to two ticks. State 3 is ordinary controllable swimming.

`linkSetSwimmingSpeed` selects `SPEED_80` without the Swimmer's Ring. Since
`SPEED_100` is one cardinal pixel per 60 Hz frame, the native traversal policy
uses 30 pixels per second while swimming and restores 60 on leaving water.
The shared swim animation records are identical:

| Campaign | Record | Frames |
| --- | --- | --- |
| Ages | `animationData19eeb` | `$10:$0b`, `$10:$0c`, loop |
| Seasons | `animationData19c51` | `$10:$0b`, `$10:$0c`, loop |

Entering water cancels action/item input in the room slice while preserving
directional traversal. Collision with ordinary actors stays enabled, matching
retail state 3 when Link is not diving.

## Drowning and shared respawn

Without the required capability, state 1 forces `LINK_STATE_RESPAWNING`
parameter 4, disables object collision, and writes `$88` to Link's
invincibility counter. The campaign-relocated drowning animations are:

| Campaign | Record | Frames |
| --- | --- | --- |
| Ages | `animationData19eda` | `$06:$d4`, `$10:$0b`, end |
| Seasons | `animationData19c40` | `$06:$c8`, `$10:$0b`, end |

After 6+16 ticks, `linkState02` substate 5 joins the same `@respawn` label used
by ordinary holes. Link returns to the explicit local respawn checkpoint,
stays hidden for two ticks, takes signed damage `$fc` (four quarter-heart
units), receives `$3c` invulnerability ticks, and completes the shared
16-tick input-captured recovery.

## Deliberate boundaries

- Flipper stroke acceleration on A, the Swimmer's Ring speed, diving, Zora
  Ring duration, Mermaid Suit velocity, and underwater-map transitions remain
  separate item/traversal policies.
- Side-scrolling water uses bit-flag tile types and
  `linkUpdateSwimming_sidescroll`; it is not sent through this top-down state
  machine.
- Splash objects and sounds await their owning presentation/audio systems.
- Drowning at zero health reports fatal damage, but game-over continuation is
  still pending.

## Verification

Headless tests run the water state machine under both campaign policies and
pin capability entry, the 10-tick Flippers lock, `SPEED_80`, ROM swim frames,
campaign-specific drowning frames, 6+16 animation timing, hidden respawn,
damage, invulnerability, recovery, airborne immunity, and the Ages seawater
Mermaid Suit branch. The `water` developer scenario independently finds a
normal traversable tile beside ROM-derived ordinary water for either US ROM.
