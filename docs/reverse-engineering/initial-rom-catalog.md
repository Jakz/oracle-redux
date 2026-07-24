# Initial ROM Catalog

## Outcome

The added US ROMs are the preferred reverse-engineering baseline. Their exact
MD5 hashes match the manifests checked into `Stewmath/oracles-disasm`, so its
labels, bank layout, and source organization can be tied directly to these
bytes.

| Game | Region | Game code | MD5 | `oracles-disasm` |
| --- | --- | --- | --- | --- |
| Ages | US | `AZ8E` | `c4639cc61c049e5a085526bb6cac03bb` | Exact match |
| Seasons | US | `AZ7E` | `f2dc6c4e093e4f8c6cbea80e8dbd62cb` | Exact match |
| Ages | Europe | `AZ8P` | `825de040ea4dff66661693f8712b1bdb` | Unsupported regional build |
| Seasons | Europe | `AZ7P` | `4ca44cbdd4e05c9b3c22da96d3de6338` | Unsupported regional build |

All four Nintendo logo areas and header/global checksums validate. The US images
are 1 MiB (64 banks); the multilingual European images are 2 MiB (128 banks).
All declare 8 KiB SRAM and `MBC5+RAM+BATTERY`.

The European filenames are reversed: the filename saying Ages contains `AZ7P`
(Seasons), while the filename saying Seasons contains `AZ8P` (Ages). No input
files were renamed or modified. The analyzer trusts the internal game code,
never the filename.

## What is demonstrably shared

The primary numbers now compare US Seasons against US Ages:

| Measurement | Result | Interpretation |
| --- | ---: | --- |
| Equal bytes at the same file offset | 77,691 / 1,048,576 (7.41%) | Same-layout physical reuse |
| Informative aligned 16-byte blocks found in the other game | 4,361 / 60,801 (7.17%) | Fast exact-block lower bound |
| Unique Seasons bytes covered by relocated exact regions ≥64 bytes | 230,557 (21.99%) | Lower-bound all-content reuse; includes assets and tables |
| Identical complete banks | 0 | Shared material is mixed with game-specific material |
| Fixed-bank code candidates equal at the same address | 126 / 429 (29.37%) | Small conservative reachable subset |
| Entry-bank code candidates equal at the same address | 98 / 109 (89.91%) | Strong evidence that the startup/runtime spine is shared |

The ROM-wide numbers are **not a code-overlap percentage**. Code and data share
the same banks, exact matching misses address-adjusted routines, and the current
control-flow pass intentionally refuses to guess dynamic MBC5 bank state. A
credible whole-program code percentage needs symbol-guided bank resolution and
instruction normalization.

Even so, the answer to “can the implementation be shared?” is already yes:

1. Both cartridges execute an exact 390-byte region beginning at `00:0150`
   before jumping into the switchable window at `01:4000`.
2. The conservatively reached part of bank 1 is almost identical.
3. Exact regions recur after moving between banks, including multi-kilobyte
   regions.
4. The established US disassembly independently organizes both builds around
   common `code/bank*.s` modules and many `object_code/common/*` modules.

The correct C++ shape is therefore one Oracle Runtime with campaign definitions
and campaign-specific behavior extensions—not two game ports joined by a menu.

The later symbol-guided pass strengthens this conclusion: 1,783 named routine
targets occur in both US games (about 71.1% of each game's resolved routine
vocabulary), and 2,514 named caller/kind/target relationships occur in both
static call graphs. See
[`oracles-disasm-reference.md`](oracles-disasm-reference.md) for provenance,
method, and limitations.

## Regional alignment

Exact-region matching maps approximately 50.00% of US Ages and 48.91% of US
Seasons into their European counterparts. This is enough to transfer many
symbols automatically, but not enough to treat localization as a raw byte swap:
the European builds double the ROM capacity, move banks, contain five languages,
and may include behavior fixes.

Localization in the C++ product should still be straightforward once text is
decoded into stable message IDs. The important distinction is that reverse
engineering the European binary layout is not necessary for the first playable
port; it becomes a validation and regional-difference task.

## Initial bank map

This is a working map, not a final disassembly. Rows marked “corroborated” use
the US-only `oracles-disasm` layout to interpret the corresponding European
banks. European localization can move boundaries, especially after bank `1C`.

| Bank(s) | Initial role | Confidence | Sharing implication |
| --- | --- | --- | --- |
| `00` | Reset/interrupt vectors, hardware setup, fixed-bank helpers | Verified + corroborated | Shared boot and low-level runtime; 16.5% covered by exact ≥64-byte regions |
| `01` | Main entry and central runtime dispatch | Verified + corroborated | High-priority shared runtime; reached bootstrap code is 89.91% equal |
| `02` | Core engine and room initialization | Corroborated | Shared service with campaign room hooks |
| `03` | Core engine plus campaign cutscenes | Corroborated | Split shared mechanics from campaign sequences |
| `04` | Room, music, tileset, animation, tile-change, and warp logic/data | Corroborated | Shared loaders; campaign-owned tables |
| `05` | Special-object and tile-property behavior | Corroborated | Mostly common concepts with campaign additions |
| `06` | Interactable tiles, item-parent logic, special objects | Corroborated | Best early vertical slice: 26.0% exact-region coverage and many known common handlers |
| `07` | File management, collision effects, item update code | Corroborated | Shared save/item services |
| `08-0F` | Interaction and enemy behavior groups | Corroborated | Common dispatcher/types; mixed common and campaign actors |
| `10` | Part/projectile behavior and room initialization | Corroborated | Shared component family with campaign subclasses/definitions |
| `11-16` | Object data/loading, OAM, scripts, serial/link, palettes, collisions | Corroborated | Shared formats and loaders; campaign data |
| `17-3E` | Tile mappings, room layouts, graphics, text, and other packed assets | Corroborated/unknown | Build asset extractors; do not translate blobs into C++ |
| `3F` | Graphics loading, treasure/drop logic, textbox, object metadata | Corroborated | Mixed shared services and campaign tables |
| `40-6B` | European extension; likely localization/content storage | Inferred | EU-specific extraction work; do not assume US addresses |
| `6C-7F` | Empty cartridge padding | Verified | Ignore for overlap claims |

Notable exact-match candidates from the current scan include:

| Seasons location | Ages location | Length | Provisional interpretation |
| --- | --- | ---: | --- |
| `00:0150` | `00:0150` | 390 | Boot/setup path |
| `06` (combined regions) | best candidate banks | 4,263 bytes | Item/special-object engine candidates |
| `1D:4000` | `1D:4000` | 13,856 | Data/asset block; not code |
| `2F:5975` | `33:5150` | 9,867 | Relocated packed content |
| `30:4828` | `34:4000` | 5,408 | Relocated packed content |
| `31:59EE` | `35:51D9` | 7,353 | Relocated packed content |

The machine-generated manifest is `analysis/generated/rom-manifest.json`.
Detailed reports are grouped by comparison:

- `analysis/generated/comparisons/us-games/`
- `analysis/generated/comparisons/europe-games/`
- `analysis/generated/comparisons/ages-us-to-europe/`
- `analysis/generated/comparisons/seasons-us-to-europe/`

Regenerate them with `python tools/rom_catalog.py`.

## Architecture implications

Keep original behavior deterministic and independent from SDL:

```text
oracle-runtime/
  simulation      player, actors, collision, rooms, transitions, RNG, timing
  data-model      stable IDs and decoded campaign data
  campaign-api    narrowly scoped variation points

campaigns/
  seasons         Holodrum, Subrosia, seasons, campaign actors/scripts
  ages            Labrynna, eras, time travel, campaign actors/scripts

platform-sdl/
  window, renderer, audio device, input, persistence

presentation/
  original viewport composition, 16:9 extension, modern UI, accessibility
```

The simulation should use original integer coordinates and tick boundaries.
A 16:9 renderer can expose more composed scenery or UI, but widening the active
simulation area immediately would change enemy activation, scrolling, room
transitions, and puzzle assumptions.

## Next reverse-engineering targets

1. **Completed:** Build the US `oracles-disasm` targets and retain their `.sym`
   files as local analysis inputs.
2. **Completed:** Import US symbols into a bank-aware ROM map and emit a named
   call graph.
3. Resolve the bank-switch/far-call ABI from bank `00`, then propagate selected
   bank state through that graph.
4. Transfer high-confidence US symbols to the European banks and record address
   deltas or changed routines.
5. Classify every ≥64-byte candidate region as code, table, text, graphics,
   audio, or map data.
6. Start a behavior trace harness around one common subsystem in bank `06`
   (sword/item-parent behavior is a good candidate).
7. Define golden traces: inputs plus initial RAM/state produce identical state
   deltas, spawned objects, audio events, and draw commands.
8. Only after a trace passes, translate that subsystem into shared C++.

## External reference and provenance

The public [`Stewmath/oracles-disasm`](https://github.com/Stewmath/oracles-disasm)
is a mature, documented disassembly that builds these exact US releases. Its
README explicitly says JP/EU are unsupported, so it is a byte-exact semantic
and address reference for the US baseline but only a transfer reference for the
European cartridges. Its shared include structure corroborates the
common-engine conclusion.

Using that source means this project is not a clean-room reimplementation.
Before substantial translation, decide and document whether the project will:

- use the public disassembly directly as research material, or
- enforce a clean-room boundary where one side writes behavioral
  specifications and another implements only from those specifications.

No external disassembly source or copyrighted asset has been copied into this
workspace in this pass.
