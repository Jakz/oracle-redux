# Audio architecture

## Decision boundary

Oracle Redux preserves the original interactive sound driver through one
explicit machine-code exception:

```text
Native Oracle Runtime
  start music, play SFX, stop, volume
                  |
                  | typed AudioCommand
                  v
Oracle Audio VM
  validated original driver and audio data
  Gb_Cpu + private memory + controlled ROM banking
                  |
                  v
Blargg Gb_Apu + Blip buffers
                  |
                  v
native mix/effects buses
                  |
                  v
SDL3 audio stream
```

The VM is an audio appliance, not a general Game Boy emulator. It has no PPU,
joypad, serial link, cartridge SRAM, gameplay scripts, collision state, actor
state, or access to host pointers. Only the native Oracle Runtime can decide
which semantic audio command occurs.

## Why a complete CPU core remains

Executing original driver code requires correct semantics for every LR35902
instruction that any reachable driver path may use. Maintaining a guessed
audio-only opcode subset would add a second, fragile compatibility surface and
could fail only in rare tracks, fades, or sound-effect arbitration paths.

Oracle Redux therefore reuses Game_Music_Emu's compact `Gb_Cpu` implementation
rather than writing a CPU interpreter. This is a full instruction core inside a
strictly reduced machine: only audio memory, timing, banking, and APU I/O are
modeled. `Gb_Apu` and Blip components synthesize the original pulse, wave, and
noise channels.

The high-level stock `Gbs_Emu` track-player API is not sufficient by itself
because gameplay must request sound effects while music continues. The Oracle
Audio VM adapts the same CPU/APU components and exposes the original driver's
interactive entry points.

## Driver surface

The disassembly identifies these primary operations:

- `initSound`;
- `updateSound`;
- `playSound`;
- `stopSound`;
- `updateMusicVolume`.

The driver owns eight logical music and sound-effect channels, fades, channel
arbitration, APU register writes, and sound-data bank reads. The VM calls only
validated entry points and permits execution only in known audio code ranges
plus the known driver routine copied into private WRAM.

Both campaigns receive campaign-specific validated mappings. No extracted
driver or audio data is committed to the repository or shipped with binaries;
the VM obtains bytes from a compatible player-supplied ROM Source at runtime.

## Timing and authority

The deterministic Oracle Runtime emits typed `AudioCommand` events on logic
ticks. The audio host timestamps those commands and advances the original sound
driver at its expected cadence. PCM generation and SDL buffering can run ahead
asynchronously, but must not feed state back into gameplay.

Fidelity diagnostics can record:

- semantic audio commands;
- driver entry calls;
- ROM-bank changes;
- APU register writes with clock timestamps;
- per-channel or mixed PCM hashes.

These traces allow the Audio VM to be compared with cartridge or emulator
reference behavior without exposing emulated memory to the rest of the engine.

## Modern processing

Authentic synthesis enters native mix buses after `Gb_Apu`. Optional EQ,
reverb, spatial treatment, accessibility mixing, or alternate arrangements are
Presentation Extensions. They do not change driver execution, channel
arbitration, or authoritative timing.

## Dependency boundary

Only the required maintained Game_Music_Emu CPU, Game Boy APU, Blip, and buffer
components are integrated. Their LGPL-2.1-or-later notices and replacement or
relinking requirements must be satisfied by project packaging. Public
Oracle Redux C++ headers continue to use the `.h` extension and do not expose
third-party implementation types.
