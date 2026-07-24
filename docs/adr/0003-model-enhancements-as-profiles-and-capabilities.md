# Model enhancements as profiles and capabilities

Status: Accepted

## Context

Oracle Redux needs both a faithful baseline and a modern experience.
The modern target includes seamless room presentation, wider views, smooth
rendering, atmospheric effects, and expanded item access. Implementing these as
a second engine would duplicate behavior and make fixes diverge. Implementing a
single global "modern mode" flag would hide which changes affect rules and
which affect only presentation.

## Decision

Classic and Modern are named Experience Profiles that resolve to explicit,
independently configurable capabilities. Both campaigns use the same engine and
the same settings types.

Capabilities that alter authoritative gameplay, such as expanding the Active
Simulation Region or changing the number of item actions, live in gameplay
settings. Visual composition, interpolation, lighting, fog, grading, and
pixel-perfect output live in presentation settings.

Presets provide defaults only. The resolved settings are validated at startup
and then passed explicitly to the systems that need them.

## Consequences

- Players can start from a profile and disable individual effects or gameplay
  changes.
- Regression tests can isolate one enhancement at a time.
- Widescreen and zoom cannot accidentally activate neighboring gameplay.
- Save files and replays must record gameplay-affecting settings.
- Combinations need validation and a compatibility policy as capabilities grow.
