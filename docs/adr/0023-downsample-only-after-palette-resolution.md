# Downsample only after palette resolution

Status: Accepted

Fidelity presentation and Modern magnification retain nearest-sampled source
texels. When Modern presentation shows original texels at less than one output
pixel each, the renderer first resolves indexed tiles and sprites into a world
working surface at sufficient source-texel density, applies world-space
lighting and effects with its depth and material data, and then downsamples the
resolved color result into the output viewport. Indexed atlas values are never
linearly filtered. Interface composition occurs afterward at output resolution.
The initial World Overview may use a source-resolution render target; tiled or
alternative reconstruction strategies remain valid measured optimizations for
regions exceeding practical target limits.
