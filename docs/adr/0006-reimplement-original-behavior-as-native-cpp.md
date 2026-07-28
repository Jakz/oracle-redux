# Reimplement original behavior as native C++

Status: Accepted

Oracle Redux uses a Native Behavioral Reimplementation: the shipped product executes maintainable C++ that reproduces the original campaigns' behavior, while the ROM, `oracles-disasm`, and observed execution provide reference evidence. Gameplay, scripts, actors, collision, progression, saving, and interface behavior do not execute LR35902 machine code or mechanically translated original code; campaign content remains decoded from the player-supplied ROM Source as established by ADR-0005. ADR-0027 defines the sole exception: a sandboxed, non-authoritative Audio VM may execute validated original sound-driver code to preserve music and sound effects. This keeps the gameplay runtime compatible with a modern world and presentation model, at the cost of proving its fidelity through traces, differential tests, and documented campaign-specific rules rather than inheriting correctness from original code execution.
