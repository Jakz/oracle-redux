# Visible-world rendering

The room slice keeps all 256 small rooms decoded and composed into a continuous
`2560×2048` world texture. That representation is useful for seamless travel,
widescreen cameras, and the World Overview mode, but it does not mean every
room must be processed every frame.

## Measured bottleneck

The MSVC x64 Debug build originally rendered the default Ages scenario at
`2.88 FPS`. A 20-frame stage profile attributed `4.79` of `5.10` seconds to
the update phase; rendering took `0.006` seconds and presentation took `0.307`
seconds. The dominant work was not SDL draw-call submission:

1. each logic tick scanned all 256 rooms to build a world animation signature;
2. every room using a changed animation group was decoded again;
3. each rebuilt room was copied into the full CPU world surface even though
   SDL subsequently uploaded that room separately;
4. the resulting delay caused fixed-step catch-up updates, multiplying the
   same work.

This is why splitting the static world into more draw calls or separate room
buffers would not have addressed the measured problem.

## Runtime policy

The optimized path preserves one continuous cached world:

- the full world is decoded and uploaded once at startup;
- the camera produces a backend-neutral `WorldViewport`;
- animation checks visit only rooms intersecting that viewport plus a
  16-pixel, one-metatile margin;
- a visible room is decoded and uploaded only when its own cached animation
  signature differs from the current group signature;
- an off-screen room may remain on an old animation frame, but it refreshes
  while still outside the visible viewport because of the margin;
- runtime animation updates no longer copy pixels into the unused full CPU
  composition;
- SDL samples only the visible source rectangle from the world texture;
- room borders, collision cells, and diagnostic metatiles use the same
  visibility rejection.

Gameplay simulation remains independent of this policy. Camera visibility can
change presentation work, but it cannot activate or deactivate enemies,
scripts, collisions, or room state.

The crop and intersection math lives in
`oracle/presentation/world_viewport.h`, with no SDL types. A future SDL GPU
backend can reuse it when building view constants, instance ranges, or render
passes.

## Verification

`--benchmark-frames N` runs exactly `N` presented frames, then prints total FPS
and accumulated update, render, and present durations. The rolling value in the
diagnostic header is intended for interactive checks.

After the optimization, 120-frame MSVC x64 Debug runs measured:

| Cartridge | FPS | Update | Render | Present |
| --- | ---: | ---: | ---: | ---: |
| Ages US | 61.67 | 0.691 s | 0.015 s | 1.238 s |
| Seasons US | 61.52 | 0.137 s | 0.018 s | 1.794 s |

Both runs are presentation-limited near the display refresh rate. The Ages
update time is higher because the default combat scene performs more active
runtime work, but it remains comfortably within the frame budget.

Native tests cover centered crops, map-edge clamping, screen offsets, room
intersection, and a camera fully outside the texture.
