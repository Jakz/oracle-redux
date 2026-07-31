# Broad-spectrum slice management

Oracle Redux advances breadth-first toward a playable dual-campaign spine.
Each implementation slice should improve at least one player-visible loop
without hiding fidelity gaps in adjacent systems.

## Sources of truth

- `specs/slices.json` is the machine-readable subsystem matrix. It records
  status, priority, playability contribution, dual-campaign scope, developer
  scenarios, dependencies, evidence, verification, and explicit gaps.
- `PROJECT_STATUS.md` is the concise current handoff and ordered near-term
  queue.
- Git commits are immutable slice history.
- Reverse-engineering and architecture documents contain the detailed retail
  evidence and design boundary for each matrix entry.

Run this at any checkpoint:

```powershell
python tools/slice_status.py
python tools/slice_status.py --check
```

The Python suite validates the catalog, referenced files, dependency ids,
campaign coverage, scenario names, verified-slice evidence, and track
coverage. A slice is not complete if the code is committed but its matrix
entry, handoff, scenario coverage, gaps, or verification state is stale.

## Status meanings

| Status | Meaning |
| --- | --- |
| `verified` | Native behavior exists with automated evidence and an integration path. |
| `implemented` | Native behavior exists, but verification or integration remains incomplete. |
| `partial` | A representative family works; the broader subsystem is knowingly incomplete. |
| `researching` | Retail boundary or architecture is being pinned before implementation. |
| `planned` | Scoped and ordered, but no trustworthy native implementation exists. |
| `deferred` | Intentionally postponed behind a named playability milestone. |

Priorities are independent of status. `now` includes the currently supported
playable spine as well as active breadth work; `next` is the near queue;
`later` is required for campaign completion; `post-first-playable` protects
the fidelity-first order from premature presentation work.

## Breadth rule

Choose the next slice by considering all of these together:

1. unblock a missing play loop such as traversal, interaction, combat,
   progression, continuity, or presentation;
2. prefer a shared Ages/Seasons behavior before campaign-specific expansion;
3. add or extend a named scenario when the result can be exercised directly;
4. pin a headless deterministic test for the retail-observable behavior;
5. keep gaps explicit rather than inflating a representative implementation
   into a completed subsystem.

This prevents long runs of narrow renderer, enemy, or tooling work while core
playability remains absent.

## Developer scenario contract

The room slice exposes named scenarios from
`apps/room_slice/scenario_catalog.cpp`. `--scenario NAME` is stable for scripts
and smoke tests, while `--scenario-menu` is the Visual Studio default. Legacy
flags remain compatibility aliases.

Every newly testable player-facing feature should either:

- join an existing scenario whose purpose genuinely covers it; or
- add a focused scenario and reference that name from `specs/slices.json`.

The selector must resolve campaign-specific rooms from the supplied ROM or a
documented campaign policy. The ordinary-hole scenario demonstrates the
preferred approach: it searches decoded terrain for a hole and a traversable
adjacent spawn instead of embedding extracted content.
