# Sample semantic input on logic ticks

Status: Accepted

Platform backends translate keyboard, controller, mouse, touch, and future
device events into backend-neutral semantic actions. The Oracle Runtime
consumes one deterministic Input Frame per logic tick containing held, pressed,
and released action state; it never reads SDL events or device identifiers.
Fidelity actions reproduce the original directional and button semantics, with
analog sources quantized through documented dead zones. Remapping changes
physical bindings rather than action meaning, while expanded item actions
remain explicit Gameplay Extensions. Recorded Input Frames can drive replays,
headless tests, and original-versus-C++ trace comparisons.
