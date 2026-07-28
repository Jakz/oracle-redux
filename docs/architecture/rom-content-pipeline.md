# ROM Content Pipeline

## Distribution boundary

The repository and release packages contain engine code, reverse-engineered
structure descriptions, and compatibility fingerprints. They do not contain
cartridge images, extracted graphics, room layouts, text, audio, or a
reconstructed ROM.

At runtime, the player selects a ROM Source. The loader currently requires one
of the exact 1 MiB US revisions used by the reverse-engineering baseline. It
checks:

1. file size;
2. Nintendo header logo;
3. header checksum;
4. global checksum;
5. Oracle game code;
6. whole-ROM compatibility fingerprint.

Structural offsets are selected only after that validation. This prevents an
unsupported revision from being silently decoded with the wrong addresses.

## Decode boundary

Decoded data is an in-memory representation owned by the running process:

```text
player-selected ROM Source
          |
          v
identity and compatibility validation
          |
          v
banked read-only ROM view
          |
          v
room / tileset / script / audio decoders
          |
          v
campaign-neutral runtime structures
```

The first slice decodes small overworld room layouts. It reads the campaign's
layout-group table, resolves banked pointers and relative room offsets, then
handles raw, 8-value common-byte, and 16-value common-byte layouts. A decoded
room contains 10×8 metatile identifiers.

The room's high and low hexadecimal digits are its overworld grid coordinates.
A 3×3 neighborhood can therefore be placed into continuous world space without
copying data into a project asset.

## Derived Asset Cache

No cache is implemented yet. When profiling shows one is useful, it must:

- live in a user-data or build directory, never source control;
- be reproducible solely from the selected ROM Source and engine version;
- include the ROM compatibility fingerprint and decoder schema version;
- be rejected when either fingerprint changes;
- contain no requirement to redistribute it with the engine.

The uncached decode path remains authoritative and testable.

## Current and next visual layers

The standalone slice maps each ROM-derived metatile identifier to a
deterministic diagnostic color. This verifies room identity, decompression,
neighbor placement, camera movement, zoom, and GPU presentation independently
of the more complex graphics pipeline.

The next layer resolves each metatile through the selected room tileset into
four 8×8 tile references and attributes, loads the corresponding 2-bit tile
pixels and Game Boy Color palettes from the ROM Source, and uploads the
resulting atlas to the renderer. Dynamic tile substitutions and animated tiles
remain separate later stages.

That composed RGBA path is a diagnostic output, not the production content
boundary. Production decoding publishes exact tile color indices, tile
attributes, palette domains, palette selections, and decoded palette entries.
The SDL GPU backend uploads indexed atlases and palette-table data. Palette
changes can therefore preserve the original tile atlas and update only palette
state.
