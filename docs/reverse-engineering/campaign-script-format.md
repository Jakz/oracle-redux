# Campaign script formats

Oracle Redux will preserve the original campaign scripts as ROM-supplied
content and run them through a native C++ interpreter. This document describes
the formats established from the exact US ROMs and the corresponding
`oracles-disasm` sources. No script runtime is implemented yet.

## Formats at a glance

The cartridges use at least three reusable script languages:

| Format | Typical users | Main state | Command range |
| --- | --- | --- | --- |
| Interaction script | NPCs, events, cutscenes, dungeon controllers | interaction script pointer, one return pointer, counters | `0x80`–`0xfc`, plus `0x00` and compact jumps |
| Movement script | guards, platforms, simple patrols | object `var30`–`var33`, state, counter | `0x00`–`0x06` |
| Simple script | doors and short tile sequences | interaction script pointer and counter | `0x00`–`0x04`, with Seasons commands through `0x08` |

Some interactions also define private “mini-script” formats interpreted by
their own object handlers. Those are not instances of the shared interaction
language and must be cataloged with the corresponding handler.

The reference definitions are:

- `reference/oracles-disasm/include/script_commands.s`
- `reference/oracles-disasm/code/scripting.s`
- `reference/oracles-disasm/include/movementscript_commands.s`
- `reference/oracles-disasm/code/objectMovementScript.s`
- `reference/oracles-disasm/include/simplescript_commands.s`
- `reference/oracles-disasm/code/bank0.s`

## Interaction-script storage

The primary interpreter and script body occupy different banks in each
campaign:

| Campaign | Primary script bank | Primary script start | Secondary script bank |
| --- | ---: | ---: | ---: |
| Ages | `0x0c` | `0x45ef` (`stubScript`) | `0x15` |
| Seasons | `0x0b` | `0x45d8` (`stubScript`) | `0x14` |

An interaction stores a 16-bit script pointer. Scripts in the primary bank can
jump and call within that bank. Opcode `0x83` (`loadscript`) supplies a bank
byte and little-endian address, copies up to 256 bytes into the original
`wBigBuffer` at `0xc300`, and continues there. This provides access to
secondary-bank scripts.

The native representation should retain a source coordinate:

```text
RomScriptAddress
  campaign
  bank
  address
```

Decoded instructions should also retain their original offset and raw operand
bytes. That makes disassembly listings, runtime diagnostics, and later trace
comparison possible without treating a host pointer as a cartridge address.

## Interaction instruction encoding

The first byte has three meanings:

| First byte | Meaning |
| ---: | --- |
| `0x00` | End the script |
| `0x01`–`0x7f` | High byte of an unconditional absolute jump; the next byte is the low byte |
| `0x80`–`0xfc` | Command opcode, followed by command-specific operands |
| `0xfd`–`0xff` | No standard dispatcher entry; reject unless a verified private format proves otherwise |

The compact jump is unusual. It has no separate opcode:

```text
4c b5
```

means “jump to address `0x4cb5`.” It is big-endian because the first byte is
both the command discriminator and the high address byte.

Most explicit 16-bit operands emitted with `.dw` are little-endian. Some
logical identifiers, including combined object and text IDs, are stored as a
high-byte/low-byte pair. The decoder therefore needs operand schemas per
opcode; it cannot apply one endianness rule to every two-byte value.

## Interaction command families

The standard table contains immediate commands, commands that yield until the
next logic tick, and commands that wait for a condition. Important families
are:

| Range / opcode | Family | Examples |
| --- | --- | --- |
| `0x80`–`0x8f` | Actor state and creation | state, coordinates, speed, collision radii, animation, spawn interaction/enemy |
| `0x90`–`0x9f` | Actor data and dialogue | random bits, text IDs, show text, A-button interaction |
| `0xa0`–`0xaf` | Eight packed event bits | wait for or toggle a bit in the original `0xcfc0` byte |
| `0xb0`–`0xbf` | Persistent flags and input gates | room/global flags, menu and Link control |
| `0xc0`–`0xcf` | Calls and conditional control flow | one-level call/return, jump tables, item and memory tests |
| `0xd0`–`0xdf` | Wait conditions and items | collision waits, enemy-clear waits, counters, spawn/give item |
| `0xe0` / `0xe1` | Assembly-helper bridge | call a bank-`0x15` routine, optionally with one byte parameter |
| `0xe2`–`0xeb` | World services | effects, sound, music, tile changes, respawn point, shake, NPC collision |
| `0xec`–`0xef` | Cardinal NPC movement | move for a supplied frame count |
| `0xf0`–`0xfc` | Compact delays | `1, 4, 8, 10, 15, 20, 30, 40, 60, 90, 120, 180, 240` frames |

Reserved entries are present inside the table, including `0x82`, `0xb2`,
`0xb7`, `0xbf`, `0xc2`, `0xc5`, and `0xdc`. The decoder should preserve and
report them rather than silently assigning a guessed meaning.

### Scheduling behavior

`interactionRunScript` first respects global text/death gates and actor
counters. When runnable, the interpreter executes consecutive immediate
commands in the same logic tick. A command can:

1. continue immediately at another instruction;
2. yield with its updated program counter;
3. wait by leaving the program counter on the condition command;
4. end the script.

That distinction is part of the Fidelity Baseline. Executing exactly one
instruction per tick would change cutscene timing, spawn order, flag
visibility, and RNG consumption.

`counter1` implements ordinary delays. `counter2` can continue applying actor
speed while it counts down. Commands whose reference macro begins with
`check` generally retain control until their condition becomes true.

### Calls

Opcode `0xc0` stores a single return address in the interaction and jumps to a
little-endian target. Opcode `0xc1` returns through that address. This is a
one-level return register, not an unrestricted call stack; the native runtime
must preserve that limitation for Compatibility Quirks.

Jump-table commands are followed directly by little-endian script pointers.
Their length depends on runtime index data and cannot always be inferred from
the byte stream alone without symbol/range metadata.

### Loaded scripts

Opcode `0x83` has this encoding:

```text
83 bb ll hh
```

where `bb` is the source bank and `hhll` is the little-endian source address.
The original copies 256 bytes into `wBigBuffer` and executes the copy. Local
jump handling has campaign-specific behavior, including relocation support in
Ages. The C++ decoder should model the resulting address space explicitly
rather than literally copying into emulated WRAM.

### Assembly-helper bridge

The original opcodes `0xe0` and `0xe1` call an address in bank `0x15`. They are
the principal boundary between generic script bytecode and object-specific
assembly:

```text
e0 ll hh       ; call helper
e1 ll hh pp    ; call helper with parameter pp
```

Oracle Redux will not execute those addresses. Compatibility metadata maps
each verified `(campaign, bank, address)` target to a named C++ host command.
Unknown targets are load-time errors with source diagnostics. Completing the
host-command inventory is a prerequisite for full campaign script coverage.

Raw memory commands such as `writememory`, `jumpifmemoryeq`, and object-byte
operations require a similar typed mapping from original addresses to runtime
state. The policy for that mapping remains an architectural decision; direct
host-memory access is not permitted.

## Movement scripts

Movement scripts are a separate compact language usable by non-interaction
objects. A script table entry first supplies object speed and direction,
followed by the instruction stream.

| Opcode | Operands | Meaning |
| ---: | --- | --- |
| `0x00` | little-endian address | Jump, commonly used to loop |
| `0x01` | destination Y | Move up; enter state `0x08` |
| `0x02` | destination X | Move right; enter state `0x09` |
| `0x03` | destination Y | Move down; enter state `0x0a` |
| `0x04` | destination X | Move left; enter state `0x0b` |
| `0x05` | frame count | Wait; enter state `0x0c` |
| `0x06` | frame count, state | Set counter and state; implemented by the Ages reference path |

The object stores the script pointer in original variables `var30/var31` and
destinations in `var32/var33`. Native C++ should expose typed fields while
retaining these original aliases in diagnostics.

## Simple scripts

Simple scripts drive short sound and tile sequences:

| Opcode | Operands | Meaning |
| ---: | --- | --- |
| `0x00` | — | End |
| `0x01` | frame count | Wait |
| `0x02` | sound ID | Play sound |
| `0x03` | packed position, tile | Set one metatile |
| `0x04` | position, tile A, tile B, mix | Set an interleaved tile |
| `0x05` | count, first position, tile | Seasons: set a row of tiles |
| `0x06` | — | Seasons: return status 1 |
| `0x07` | count, first position, tile B, mix | Seasons: set a row of interleaved tiles |
| `0x08` | — | Seasons: return status 2 |

As with the main language, immediate tile operations can continue in the same
tick while waits yield.

## Proposed native boundary

```text
validated ROM Source
        |
        v
Campaign Script decoder
  source address + raw operands
        |
        v
typed immutable ScriptProgram
        |
        v
ScriptInstance in an Actor Slot
  program counter
  one return address
  wait/counter state
        |
        v
Native Script Runtime
        |
        +--> typed actor/state/flag services
        +--> dialogue and input gates
        +--> spawn and item services
        +--> tile mutation events
        +--> audio events
        +--> registered C++ host commands
```

The interpreter belongs to authoritative simulation. Rendering consumes the
events and snapshots it produces but never advances scripts.

## Decoder safety

The native decoder must:

- read only from validated compatibility-profile ranges;
- reject unsupported opcodes and unmapped helper targets with bank/address
  diagnostics;
- cap decoded instruction and control-flow exploration;
- validate branch destinations and dynamically sized jump tables;
- retain original addresses for debugging without converting them to host
  pointers;
- keep ROM bytes immutable;
- avoid executing data as LR35902 instructions.

## Coverage plan

1. Catalog every standard-script entry point referenced by interaction
   handlers and object data.
2. Decode reachable instructions and emit a command/target inventory for both
   campaigns.
3. Map raw state addresses and every `asm15` target to typed runtime services.
4. Implement the scheduler, waits, branches, one-level calls, flags, and actor
   operations needed by the First Playable Slice.
5. Add dialogue, item, audio, and tile-mutation host services.
6. Add movement and simple-script interpreters.
7. Expand command and handler coverage by object family.
8. During the deferred fidelity phase, compare program counters, counters,
   flags, emitted events, and actor state against original execution traces.

The existing `reference/oracles-disasm/tools/dump/dumpScript.py` and
`dumpMovementScript.py` are useful research references, but they contain known
hard-coded ranges and incomplete secondary-bank handling. Oracle Redux needs
its own validated decoder rather than invoking those tools at runtime.
