# Assessed implementation sequence

## Outcome

The architecture assessment establishes one native C++ Oracle Runtime shared by
Ages and Seasons, with campaign-specific definitions and ROM-derived content.
Gameplay is behaviorally reimplemented and deterministic. The sole original
machine-code exception is the isolated Oracle Audio VM.

The project should now optimize for a dual-campaign First Playable Slice before
building the final 2.5D renderer or attempting full-campaign breadth.

```text
player-supplied ROM Sources
          |
          +--> campaign content decoders
          +--> isolated original Audio VM
          |
          v
shared deterministic Oracle Runtime
  input, actors, scripts, rules, state, saves
          |
          v
immutable presentation and Semantic UI models
          |
          v
diagnostic renderer first
direct SDL GPU production renderer later
```

## Completed slice: deterministic actor interaction

The next implementation slice should make one ROM-defined interaction playable
in each campaign while using the current diagnostic renderer.

### Stage status

The first committed stage selects the shared Vasu family in Ages `02:ee` and
Seasons `01:91`. It now includes semantic tick input, original bounded actor
bands, room-record instantiation, interaction targeting, cartridge text
decompression, authentic Vasu/snakes graphics and OAM, ground-anchor ordering,
a minimal dialogue model, deterministic headless replay, and a
`--vasu-scenario` bootstrap.

The documented post-ring-box, no-rings branch now runs from the original
relocated Vasu bytecode in both cartridges. The bounded Native Script Runtime,
validated Original State Keys, campaign-specific host registry, option labels,
and source-coordinate execution traces are implemented. Headless replay proves
that Ages and Seasons execute the same normalized instruction path. Script-set
NPC collision radii, retail overlap separation, and axis-preserving wall slide
now complete the immediate slice. See
[`../reverse-engineering/vasu-interaction-slice.md`](../reverse-engineering/vasu-interaction-slice.md).

### Scope

1. Add the backend-neutral `InputFrame` and adapt current SDL input to produce
   it at logic-tick boundaries.
2. Add the bounded and ordered Actor Slot Domain with original category and
   allocation semantics.
3. Decode and instantiate one suitable ROM-defined NPC object family shared by
   representative Ages and Seasons rooms.
4. Decode its sprite/OAM frames without precomposing world rooms, preserving
   original size, anchor, palette, priority, and ground-contact position.
5. Add the initial Native Script Runtime opcodes needed to start and advance
   that NPC's original Campaign Script.
6. Resolve every script-visible address through validated Original State Keys.
7. Produce a minimal immutable Semantic UI Model for original dialogue and
   render it through a Fidelity UI layout.
8. Add developer scenario bootstraps for the selected Ages and Seasons rooms.

### Acceptance

- Both scenarios start from documented deterministic state.
- Link and the NPC render at their decoded size and correct ground layer.
- Directional input, collision, facing, interaction, dialogue, and dismissal
  work through semantic Input Frames.
- The same initial state and Input Frame sequence produce identical actor,
  script, and dialogue traces in repeated headless runs.
- Campaign differences are expressed through Campaign Definitions or decoded
  content rather than copied runtime implementations.
- CMake and `projects/msvc/oracle-redux.sln` build the slice and its tests.
- Public C++ headers use `.h`.

### Explicitly deferred from that stage

- inventory breadth and persistent pickups;
- music and sound-effect output;
- Original Save Image import/export;
- direct SDL GPU production rendering and 2.5D effects;
- full campaign-script opcode coverage.

This is intentionally a runtime slice, not an engine-framework rewrite. The
current viewer remains useful until gameplay interactions can validate correct
sprite composition, anchors, ordering, and presentation snapshots.

### Combat checkpoint

The first representative combat checkpoint now selects the shared Octorok
family in Ages `00:64` and Seasons `00:a6`. It decodes the relocated
`enemyData`, subid, `extraEnemyData`, object graphics header, graphics, palette,
animation, and OAM tables. The native runtime preserves the shared retail RNG,
states `$08-$0b`, counters, cardinal subpixel speed, contact radius, damage,
health, and deterministic replay. Enemy contact is explicitly separate from
solid NPC collision. Semantic action B supplies a visible provisional
two-strike defeat loop. See
[`../reverse-engineering/octorok-combat-slice.md`](../reverse-engineering/octorok-combat-slice.md).

### Next implementation boundary

Allocate and simulate the requested Octorok projectile part, including ROM
attributes/OAM, terrain impact, and player damage. Then replace the diagnostic
sword hitbox with the original sword item state and OAM path before broadening
to random-position enemy records, drops, or another family.

## Following slices

### 2. Combat and item loop

Add one representative enemy family, damage and defeat flow, sword behavior,
one secondary item, one persistent pickup or chest, and the minimal HUD in both
campaigns. Preserve actor allocation and update order.

### 3. Persistence spine

Implement typed campaign persistence for the selected scenarios, Redux Save
Envelope storage, and the first validated Original Save Image round trip.
Retain unknown original bytes and make export explicit and backed up.

### 4. Oracle Audio VM

Integrate the required maintained Game_Music_Emu CPU, Game Boy APU, Blip, and
buffer components. Build the bounded campaign-specific audio memory map, invoke
the validated driver entry points, support concurrent music and SFX, and record
APU-register traces. Complete LGPL packaging compliance.

### 5. Dual-campaign First Playable

Connect movement, transitions, NPC dialogue, combat, an item, persistent state,
HUD, save/reload, music, and representative sound effects into one coherent
Ages scenario and one coherent Seasons scenario.

### 6. Fidelity verification gate

After the First Playable is usable, compare representative original and C++
execution traces for inputs, actor state, scripts, RNG, room mutations, audio
events, and save results. Promote only demonstrated behavior from Provisional
to Fidelity-Verified.

### 7. Production SDL GPU renderer

Replace the high-level SDL Render diagnostic path with the direct SDL GPU
backend behind `PresentationSnapshot` and the Semantic Render Graph. Implement
Indexed Texel Atlases, shared static instance arenas, dynamic instance streams,
palette resolution, a separate UI pass, and resolved-color downsampling.

### 8. Modern 2.5D presentation

Author Presentation Metadata incrementally for the playable content, then add
visual height, stylized lighting and shadows, local dungeon spotlights,
gameplay-aware depth-of-field, atmosphere, color grading, widescreen framing,
and World Overview. Each effect remains independently configurable.

### 9. Campaign breadth

Expand object families, Campaign Script commands, rooms, dungeons, bosses,
progression systems, secrets, save fields, text, and audio coverage by shared
domain. Add campaign-specific handlers only where evidence shows a real
behavioral difference.

## Primary risks

| Risk | Mitigation |
| --- | --- |
| Object and OAM breadth hides format variants | Build family-level catalogs and trace one representative before scaling |
| Script commands call arbitrary assembly helpers | Map each helper to a named typed host command; fail unmapped calls explicitly |
| Wide rendering accidentally activates gameplay | Keep Presentation Camera and Active Simulation Region independent |
| Modern depth makes important sprites unreadable | Require ground anchors, Presentation Metadata, and Semantic Focus Masks |
| Save conversion destroys unknown data | Preserve original bytes, validate checksums, export explicitly, and create backups |
| Audio driver reaches unexpected memory or code | Validate ROMs, bound memory, whitelist entry and executable ranges, trace accesses |
| Shared runtime becomes campaign-condition soup | Put stable differences in Campaign Definitions and use explicit specialized handlers |
| Final renderer delays gameplay validation | Retain the diagnostic renderer through the First Playable runtime slices |
| Reverse-engineered behavior is assumed correct | Label behavior Provisional until representative trace comparison passes |

## Definition of readiness for full production

Full-campaign expansion should begin only when the dual First Playable:

- is usable through both CMake and MSVC builds;
- saves and reloads;
- produces original-compatible save output for its covered state;
- exercises original scripts and the Audio VM;
- passes repeatable headless tests;
- has representative Fidelity-Verified traces;
- feeds presentation entirely through backend-neutral snapshots.
