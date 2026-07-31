# ROM-backed Link sprite rendering

## Why the marker looked half-sized

Link has two intentionally different sizes:

- terrain collision uses eight asymmetric probes spanning X offsets -5 through
  +4 and Y offsets -3 through +7, while actor collision uses 6-by-6 radii;
- the ordinary visual is a 16x16 composition made from two Game Boy 8x16
  hardware sprites.

The earlier green 8x8 marker represented the collision location, not the
visual bounds. Scaling that marker made Link appear roughly half-sized. The
viewer now keeps collision and presentation separate and renders the complete
16x16 visual.

## Original pipeline traced

The implementation follows the cartridge tables documented by
`oracles-disasm`, but reads all bytes from the user-supplied ROM:

| Data | Ages file offset | Seasons file offset |
| --- | ---: | ---: |
| `spr_link` graphics | `0x68000` | `0x68000` |
| `specialObject00GfxPointers` | `0x199b1` | `0x19739` |
| `specialObject00OamDataPointers` | `0x1a0a7` | `0x19d9e` |
| palette-header table | `0x632c` | `0x6290` |
| base special-object OAM bank | `0x13` | `0x12` |

The original special-object loader selects a frame record, loads its small
graphics span, then selects an OAM layout. An ordinary layout contains two
8x16 entries using relative tiles 0 and 2. OAM X/Y offsets, horizontal and
vertical flips, and palette selection are applied before the two entries are
composed into a transparent 16x16 RGBA frame.

Palette header `0x0f` loads `standardSpritePaletteData`; color index zero is
transparent for sprite composition. Ordinary walking alternates the
direction-adjusted `0x54` and `0x80` frame families on the original six-tick
cadence. When directional movement stops, the runtime holds the current
direction's `0x54` pose. This is important because the original graphics loader
only adds `w1Link.direction` to frame indices at or above `0x54`; the lower
`0x1c` frame emitted by `LINK_ANIM_MODE_STAND` is not the base of four ordinary
directional idle frames.

This is intentionally a first animation subset. The original tables contain
the remaining lift, swim, jump, item, attack, transformation, and cutscene
frames and can be exposed as gameplay states are implemented.

## ProjectZ influence and clean boundary

[`ladxhd/projectz`](https://github.com/ladxhd/projectz) was reviewed at local
reference commit `c9f744b2c06633f40cc79cbffb7745caae1a3a70`. Its useful
architectural ideas are:

- keep the tilemap and dynamic objects as separate world layers;
- give visual components explicit layers and sort same-layer objects by Y;
- update only objects in an active spatial region;
- keep camera scale and render targets independent from simulation units;
- treat shadows and later effects as separate presentation passes.

The reviewed ProjectZ checkout does not contain a license file, so Oracle
Redux does not copy, compile, or redistribute its source or binary assets.
It is used only as a high-level architectural reference. Oracle behavior and
content continue to come from the original Oracles ROMs and independently
written C++ decoders.

## Current boundary

Link now renders at the correct size with authentic standing and walking
frames. The small shadow remains a separate modern presentation element and
can later be controlled by the experience profile. Room interactions,
enemies, parts, and item drops are now cataloged, but their object-specific
graphics and behavior are not yet active.
