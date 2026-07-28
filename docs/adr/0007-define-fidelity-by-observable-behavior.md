# Define fidelity by observable behavior

Status: Accepted

Oracle Redux defines fidelity as Behavioral Equivalence: given the same campaign state, input, RNG state, and logic tick, the Native Behavioral Reimplementation must produce equivalent authoritative state changes and gameplay events. C++ types, subsystem boundaries, algorithms, and function structure may differ from the original assembly when that does not change observable behavior. This makes fidelity measurable with traces and differential tests while allowing Ages and Seasons to share a maintainable modern runtime.
