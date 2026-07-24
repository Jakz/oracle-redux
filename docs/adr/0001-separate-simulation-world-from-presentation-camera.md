# Separate the Simulation World from the Presentation Camera

The Oracle Runtime keeps original gameplay coordinates, activation, collision, room transitions, and timing in an authoritative Simulation World. Rendering consumes immutable world-space scene snapshots through an independent Presentation Camera, allowing widescreen framing, arbitrary zoom, and a multi-room World Overview without silently simulating additional rooms; this costs an explicit scene-composition boundary but prevents presentation improvements from forking campaign behavior.
