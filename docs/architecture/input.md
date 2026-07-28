# Input architecture

## Boundary

Physical devices and platform events are outside the Oracle Runtime:

```text
SDL3 or future device backend
  keyboard, controller, mouse, touch
                 |
                 v
      physical binding profile
                 |
                 v
       semantic action state
                 |
          logic-tick sample
                 |
                 v
             InputFrame
                 |
                 v
          Oracle Runtime
```

An `InputFrame` contains held, newly pressed, and newly released semantic
actions for one deterministic logic tick. Public runtime headers use project
types and the `.h` extension; SDL event, controller, scancode, and device types
do not cross this boundary.

## Actions and bindings

Stable actions describe intent such as move up, use A, use B, start, select,
confirm, cancel, or move a menu selection. A binding profile maps physical
inputs to those actions. Rebinding a controller button does not change campaign
state or action identity.

Fidelity input exposes the original directional and button semantics. Analog
sticks are quantized using documented dead zones and directional resolution;
they do not introduce fractional authoritative movement. Smooth presented
motion continues to come from render interpolation.

Additional direct item actions, radial selection, macros, turbo, or other
outcome-changing conveniences are explicit Gameplay Extensions. Their enabled
state belongs in replay and relevant save metadata, not silently in a platform
binding profile.

## Tick sampling

Platform adapters collect events between logic ticks and maintain current
physical state. At each logic boundary, the input mapper produces exactly one
`InputFrame`:

- `held` represents actions active for the tick;
- `pressed` represents inactive-to-active edges since the prior sample;
- `released` represents active-to-inactive edges since the prior sample.

Short taps that begin and end between ticks must still produce a defined press
and release sequence rather than disappearing. Focus loss, controller removal,
and backend reset synthesize releases so held actions cannot become stuck.

Input sampling is independent from render frequency. A 240 Hz display and a
60 Hz display therefore provide the same sequence of authoritative frames for
the same physical action timing relative to logic ticks.

## UI pointing

Mouse and touch hit testing occurs against retained UI layout, but produces
semantic actions or stable selection identities. It does not mutate inventory,
dialogue, saves, or campaign state directly. The Oracle Runtime validates and
applies the resulting action on a logic tick.

## Replays and verification

A replay records:

- campaign and compatible ROM identity;
- initial authoritative state and RNG state;
- gameplay-affecting capability settings;
- one semantic `InputFrame` per logic tick.

Physical device identifiers and binding profiles are not required for replay.
The same stream can drive headless tests and Fidelity Baseline trace comparison
without SDL or a window.
