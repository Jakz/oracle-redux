# ROM Mapping Conventions

These conventions keep observations reproducible while names and classifications
are still provisional.

## Cartridge identity

Use the internal four-character game code as the canonical cartridge identity.
Do not derive identity from a local filename.

- `AZ7P`: European Oracle of Seasons
- `AZ8P`: European Oracle of Ages
- `AZ7E`: US Oracle of Seasons
- `AZ8E`: US Oracle of Ages

For these cartridges, the `P` suffix identifies Europe and `E` identifies the
US release. A SHA-256 fingerprint identifies an exact dump.

## Baseline hierarchy

Use the US ROMs for code names, symbols, and behavior reconstruction because
they match the ROMs built by `oracles-disasm`. Use the European ROMs for
localization-format discovery and a later behavior-difference audit.

Do not silently assume that every US behavior is present in Europe: the
European builds have a different 2 MiB layout and may contain code fixes as well
as localized content.

## Addresses

Write ROM locations as `BB:AAAA`:

- `BB` is the hexadecimal ROM bank.
- `AAAA` is the CPU-visible address.
- Bank `00` occupies `$0000-$3fff`.
- A selected nonzero bank occupies `$4000-$7fff`.

For a nonzero bank:

```text
file offset = bank * $4000 + (cpu address - $4000)
```

Example: `06:5a20` is file offset `$019a20`.

## Classification

Every mapped region should eventually have one primary kind:

- `code`
- `pointer-table`
- `structured-data`
- `text`
- `graphics`
- `tilemap`
- `audio`
- `padding`
- `unknown`

Do not label high-entropy bytes as code merely because they decode into valid
LR35902 instructions.

## Confidence

- **Verified**: established directly from checksums, control flow, observed
  reads, or byte-perfect reconstruction.
- **Corroborated**: agrees with a symbol or source map for another region/version
  but is not yet proven for these European cartridges.
- **Inferred**: best current interpretation from layout or byte patterns.
- **Unknown**: deliberately unclassified.

## Shared-region language

**Same-address match** means equal bytes at the same file offset. **Relocated
exact match** means an exact sequence appears at a different bank/address.
Neither term means “shared code” until both regions are classified as code.

Addresses embedded in otherwise identical routines can prevent an exact match.
Exact-byte measurements are therefore lower bounds for semantic reuse.
