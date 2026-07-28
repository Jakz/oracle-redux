# Batch static world in shared instance arenas

Status: Accepted

Decoded rooms remain collections of tile instances rather than flattened RGBA
images. Static tile instances occupy contiguous ranges in a small number of
large, persistent GPU buffers grouped by compatible atlas and pipeline state;
rooms do not own separate GPU buffers. The SDL GPU backend renders visible
ranges with indexed instancing and may combine disjoint ranges through indirect
draw commands. Dynamic actors, particles, and transient effects use separate
cycled per-frame instance streams. Room mutations mark only affected static
ranges dirty for upload. This preserves batching and GPU residency while
retaining per-tile palette, material, height, depth, animation, and occlusion
data required by the 2.5D presentation.
