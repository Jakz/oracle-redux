# Separate pixel-snapped and subpixel presentation

Status: Accepted

Fidelity presentation snaps the camera and presented geometry to the original
pixel grid and uses integer output scaling whenever the window permits. Modern
presentation may interpolate camera and actor transforms at subpixel positions
between completed logic ticks. Both policies use exact nearest sampling for
Indexed Texel Atlases; subpixel motion does not authorize linear filtering or
changes to authoritative simulation coordinates. Lighting, shadows, particles,
and post-processing are evaluated in output-resolution presentation space.
Teleports and other discontinuities explicitly suppress interpolation.
