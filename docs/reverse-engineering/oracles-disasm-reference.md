# `oracles-disasm` Reference Import

## Outcome

The US ROMs are confirmed as the right primary reverse-engineering inputs.
`Stewmath/oracles-disasm` was built locally at commit
`dfaba5238b102fa2b70f737a977a32219ba80c6b` with WLA-DX 10.6.

- Ages rebuilds byte-for-byte as MD5
  `c4639cc61c049e5a085526bb6cac03bb`.
- Seasons produces the reference project’s documented changed-empty-fill build,
  MD5 `d109f98637a19c57af16c5e5396a8d51`.
- The Seasons comparison has 35,112 differing bytes in 31 runs. Two bytes are
  the global checksum; the other 35,110 bytes are unused bank-tail fill. The
  retail image fills each affected tail with its bank number while stock
  WLA-DX 10.6 emits zeroes.
- No assembled Seasons payload byte outside those fill/checksum runs differs
  from retail. The emitted symbols therefore describe the retail program
  layout even though the rebuilt file has a different whole-file hash.

The reference checkout and toolchain remain under ignored `reference/`
directories. No ROM or external disassembly source is part of this project’s
tracked source.

## Symbol inventory

| Measurement | Ages | Seasons |
| --- | ---: | ---: |
| ROM labels | 26,467 | 23,723 |
| Unique ROM addresses | 24,180 | 22,013 |
| Linker sections | 79 | 70 |
| Source-referenced routine names resolved | 2,507 | 2,508 |
| Resolved routine entry addresses | 2,714 | 2,710 |

Aliases explain why entry-address and routine-name counts are not identical.
RAM labels and definitions are deliberately excluded from these ROM counts.

The normalized inventories are:

- `analysis/generated/reference/ages-symbols.csv`
- `analysis/generated/reference/seasons-symbols.csv`
- `analysis/generated/reference/ages-sections.csv`
- `analysis/generated/reference/seasons-sections.csv`

Each symbol row includes its `BB:AAAA` address, physical file offset, canonical
alias, containing linker section, and a conservative section-kind
classification.

## How much implementation is shared?

There are several different answers; none should be collapsed into a single
“percent of the ROM” number.

| Measurement | Result | Meaning |
| --- | ---: | --- |
| Common ROM symbol names | 14,236 | Code and data vocabulary combined |
| Common resolved routine names | 1,783 | 71.12% of Ages and 71.09% of Seasons |
| Routine-name Jaccard similarity | 55.17% | Common names divided by the union |
| Common routine entries with the same first 8 bytes | 874 | 49.02% of common routine names |
| Common routine entries with the same first 16 bytes | 554 | 31.07% of common routine names |
| Common semantic call edges | 2,514 | Same named caller, edge kind, and named target |
| Semantic-edge coverage | 65.38% Ages; 65.95% Seasons | Shared relationships within each graph |
| Semantic-edge Jaccard similarity | 48.88% | Shared relationships divided by the union |

The 71% routine-vocabulary overlap is the most useful first architecture
signal. It supports one runtime with campaign-owned content and narrowly scoped
behavior extensions.

The 8/16-byte figures are deliberately lower bounds, not function equality
rates. A moved routine may differ immediately because an absolute address or
bank number changed while its logic stayed equivalent. Exact function sizing
and relocation-normalized instruction hashes are the next analysis step.

`analysis/generated/reference/shared-symbols.csv` lists every common name and
its locations, section classifications, and capped exact-prefix result.
`shared-call-edges.csv` lists the common named control-flow relationships.

## Static call graph

The graph does not linearly sweep arbitrary ROM data. It uses a more
conservative process:

1. Direct `call`, `jp`, `jr`, `callab`, and `jpab` operands in the applicable
   reference assembly provide candidate routine-entry names.
2. Those names are resolved through each game’s linker symbols.
3. LR35902 instructions are decoded from the verified retail ROM bytes.
4. Fixed-bank targets and same-switchable-bank targets are resolved according
   to the Game Boy memory map.
5. The emitted byte sequence
   `ld hl,target; ld e,bank; call/jp interBankCall` is recognized as a far edge.
6. Paths stop at returns, indirect jumps, invalid opcodes, another known routine
   entry, or a safety bound.

| Graph | Edges | Named target | Far edges | Callers represented |
| --- | ---: | ---: | ---: | ---: |
| Ages | 4,557 | 4,411 | 34 | 1,999 |
| Seasons | 4,570 | 4,403 | 14 | 1,999 |

Edge confidence is recorded per row:

- `verified`: instruction semantics establish the edge directly.
- `verified-pattern`: a concrete `callab`/`jpab` trampoline pattern supplies
  both target address and bank.
- `corroborated`: the entry came from a named source reference and the edge was
  decoded from retail bytes, but the pass is not yet a complete proof of every
  possible bank state.

The files are `ages-call-graph.csv` and `seasons-call-graph.csv` under
`analysis/generated/reference/`.

Known RST vectors are assigned their conventional reference names
(`rst_jumpTable`, `rst_addAToHl`, and `rst_addDoubleIndex`) even though the
linker file does not place ordinary labels at those vector addresses.

## Known gaps

- Data-driven dispatch tables and local-label-only routines are incomplete.
- Calls from fixed bank `00` into `$4000-$7fff` remain bank-unresolved unless a
  recognized far-call sequence provides the selected bank.
- Other hand-written bank-switch patterns still need recognition.
- Linker labels do not state whether a symbol is a function, table, or asset.
  Source-reference seeding reduces false code classification but does not
  replace subsystem review.
- WLA-DX address-to-line/list metadata was tested but rejected: on this Windows
  build most generated address-map records failed the basic identity between
  bank/CPU address and physical ROM offset. Those records are not used.

## Reproduce the reports

After building the local reference checkout:

```sh
python tools/oracles_reference.py \
  --reference-dir reference/oracles-disasm \
  --rom-dir roms \
  --output-dir analysis/generated/reference
```

The tool refuses to use a US input unless its MD5 matches the checked
`oracles-disasm` manifest. It always decodes calls from those retail bytes.
Run `python -m unittest discover -s tests -v` for the parser, far-call, and ROM
catalog tests.

The follow-up relocation-normalized comparison is documented in
[`routine-overlap.md`](routine-overlap.md).

## Port implication

The evidence now favors this split:

```text
shared C++ runtime
  boot/timing/input/save
  room and transition services
  object/interaction/enemy frameworks
  collision, inventory, text, audio event APIs
  script interpreters and common commands

campaign definitions
  stable IDs and decoded tables
  maps, scripts, dialogue, graphics, audio
  campaign actors and special mechanics
  Ages era/time-travel rules
  Seasons season/Subrosia rules

presentation/platform
  SDL3 window, renderer, input, audio, persistence
  original 160x144 fidelity composition
  optional 16:9 composition and modern UI
```

SDL3 should remain outside deterministic gameplay state. Widescreen rendering
can initially compose additional scenery or side UI without widening the
active room simulation, enemy activation area, or collision space.
