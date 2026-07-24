# Decouple logic ticks from render frames

Status: Accepted

## Context

The Fidelity Baseline must reproduce timing-sensitive Game Boy Color behavior,
while the Modern Profile should support smooth camera and actor motion at 120 Hz
or more. Advancing gameplay once per displayed frame would make rules depend on
the monitor refresh rate. Restricting display to the original logic cadence
would preserve rules but prevent the desired modern motion.

## Decision

The Simulation World advances only through discrete deterministic logic ticks.
Presentation receives two completed snapshots plus a bounded interpolation
factor and may render any number of frames between those ticks.

Interpolated state is never fed back into collision, scripts, RNG, room
activation, or saved state. Tests and replay data refer to logic ticks, not
render frames.

## Consequences

- High-refresh rendering does not change gameplay speed or outcomes.
- Fidelity tests can run headlessly without a graphics backend.
- Visual systems must retain enough previous state to interpolate motion.
- Discontinuous changes such as teleports and room warps must explicitly
  suppress interpolation.
- Audio scheduling needs a logic-tick timestamp even when mixing continuously.
