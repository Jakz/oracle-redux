# Preserve original actor-slot semantics

Status: Accepted

The authoritative C++ actor runtime preserves the original object categories, bounded slot allocation, and stable update ordering because spawn failure, interactions, RNG consumption, and Compatibility Quirks can depend on them. Object handlers become typed C++ state machines using shared collision, movement, damage, animation, script, and spawn services, with campaign-specific behavior isolated behind explicit policies. Presentation consumes actor snapshots and does not replace the Actor Slot Domain with an unconstrained ECS.
