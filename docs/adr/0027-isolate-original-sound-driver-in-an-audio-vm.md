# Isolate the original sound driver in an Audio VM

Status: Accepted

The original Oracle sound driver is the sole permitted LR35902 execution path
in the shipped product. A sandboxed Oracle Audio VM uses the maintained
Game_Music_Emu `Gb_Cpu`, `Gb_Apu`, and Blip components to execute validated
audio code and read audio data from the player-supplied ROM Source. It exposes
only typed operations such as initialize, update, play sound, stop sound, and
set volume. The VM provides private bounded WRAM/HRAM, controlled ROM banking,
APU registers, clocking, and the known executable WRAM routine; it provides no
PPU, cartridge save access, gameplay state, input, or native-memory access.

The VM uses complete LR35902 instruction semantics from the third-party CPU
core rather than a project-authored partial opcode interpreter. Executable
ranges, ROM fingerprints, memory accesses, and entry points are validated.
Audio remains non-authoritative: the native Oracle Runtime emits semantic audio
events, and VM state cannot affect gameplay outcomes. Game_Music_Emu licensing
and redistribution obligations are part of release compliance.
