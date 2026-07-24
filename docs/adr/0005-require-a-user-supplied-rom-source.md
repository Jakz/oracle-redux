# Require a user-supplied ROM source

Status: Accepted

## Context

The runtime needs original room layouts, graphics, palettes, scripts, text, and
audio, but distributing those assets would make the project dependent on
copyrighted Nintendo content. Recreating and shipping an extracted asset pack
would preserve the same problem in another format.

## Decision

Oracle Redux distributes engine code and compatibility metadata only.
Each campaign requires a compatible original cartridge image supplied by the
player. Content is decoded from that ROM Source at runtime. Any Derived Asset
Cache is local, reproducible, fingerprint-bound, and excluded from source and
release packages.

The initial compatibility baseline accepts only the exact US revisions matched
by the reverse-engineering reference. Broader revision and localization support
must add explicit compatibility profiles rather than weakening validation.

## Consequences

- Releases contain no original campaign assets or reconstructed ROM.
- Players must provide their own compatible ROM for each campaign.
- Startup errors can identify unsupported revisions before decoding bad data.
- Cache invalidation is keyed by the ROM compatibility fingerprint.
- Tests use synthetic fixtures unless an optional local integration test is
  explicitly given a ROM Source.
