# Vasu interaction slice

## Why this family

`INTERAC_VASU` (`$89`) is the first dual-campaign actor family implemented by
the native runtime. It is unusually useful because both supported US
cartridges define the same five-object shop:

| Campaign | Room | Records |
| --- | --- | --- |
| Ages | `02:ee` | Vasu `$89:00` at `28,50`; snakes `$89:01` at `38,38` and `$89:06` at `38,68`; two ring books |
| Seasons | `01:91` | The same five records and coordinates |

Room-object Y includes the original 16-pixel status-bar row. Runtime ground
anchors therefore subtract 16: Vasu's native room anchor is `(80, 24)`.
Objects are allocated in bytecode order, so these five interactions occupy
dynamic slots `2` through `6`. Original interaction slots `0` and `1` remain
reserved.

The evidence comes from `objects/ages/mainData.s`,
`objects/seasons/mainData.s`, `constants/common/interactions.s`, and
`object_code/common/interactions/vasu.s` in `Stewmath/oracles-disasm`.
Runtime data is read from the player-supplied ROM rather than those files.

## Actor memory model

Game Boy WRAM bank 1 divides the `$dx00-$dxff` region into four bands of
sixteen `$40`-byte objects:

| Category | Index range | Dynamic allocation |
| --- | --- | --- |
| Item/special | `$d0-$df` | `$d7-$db` |
| Interaction | `$d0-$df` | `$d2-$df` |
| Enemy | `$d0-$df` | `$d0-$df` |
| Part | `$d0-$df` | `$d0-$df` |

`ActorSlotDomain` preserves those bounded bands, lowest-free-slot allocation,
and generations for rejecting stale native handles. Room loading reports
overflow instead of silently creating an unconstrained actor. This behavior is
authoritative; a later presentation snapshot may reorganize draw instances
without changing it.

## Graphics and OAM

Vasu graphics remain cartridge-derived and indexed until the diagnostic
texture upload:

| Campaign | Compressed graphics | OAM pointer table | OAM data bank |
| --- | ---: | ---: | ---: |
| Ages | file offset `0xaaa16` | `0x5b3cc` | `$14` |
| Seasons | file offset `0xa5e3e` | `0x52c7d` | `$13` |

Both sheets decompress with graphics mode 3 to `0x200` bytes. Vasu's animation
zero uses the original eight-frame sequence
`00,02,00,02,04,06,04,06`, with sixteen logic ticks per entry. Snake subids
retain their separate startup and two-frame loops.

The OAM bounds are not forced into Link's 16-by-16 box. Each 8-by-16 OAM
object contributes its signed anchor, flip flags, tile pair, palette, and
source priority. The runtime computes the complete frame bounds relative to
the actor's ground contact. That is why Vasu renders at his original
multi-sprite height instead of the half-size or clipped result seen in the
earlier marker-only viewer.

Actors with a ground anchor above Link draw before Link; actors below Link draw
after him. The rule is currently provisional pending trace comparison, but the
presentation already consumes explicit ground anchors rather than sprite
rectangle centers.

## US text format

`RomTextDecoder` implements the retail US text indirection described by
`tools/dump/dumpText.py`:

1. Campaign-specific three-byte pointers select text base 1, text base 2, and
   the relative high-index table.
2. A public message such as `TX_3003` maps to internal high index `$34`
   because internal groups `$00-$03` are compression dictionaries.
3. The high-index entry locates the low-index pointer table.
4. The low pointer is relative to base 1 for groups below `$2c`, otherwise
   base 2.
5. Bytes `$02-$05` recursively expand one of four dictionary groups.
6. Bytes `$06-$0f` retain their parameter and become semantic control atoms.

The decoder keeps the decompressed original byte stream and produces atoms for
glyphs, newlines, colors, substitutions, stops, option markers, symbols, and
unhandled commands. Option labels are derived from the original option-marker
stream rather than supplied by C++. It does not bake dialogue into a texture
or discard control flow.

For the selected post-ring-box scenario, Vasu follows the original normal
welcome route:

```text
TX_3003
  Appraise -> TX_3014 (no unappraised rings)
  List     -> TX_3015 (no appraised rings)
  Quit     -> TX_3008
```

The dialogue strings, page stops, and three option markers are decoded from
each supplied cartridge. Ages and Seasons happen to produce the same English
content for this branch.

## Current fidelity boundary

This stage deliberately boots with `GLOBALFLAG_OBTAINED_RING_BOX` set, no
earned special ring, and empty appraised/unappraised lists. It covers a real
branch of `vasuScript`, not the first-visit ring tutorial.

`VasuInteractionRuntime` is now only the scenario adapter. It locates the
ROM-instantiated Vasu actor, establishes the documented starting state, and
starts the generic `CampaignScriptRuntime` at:

| Campaign | Retail entry |
| --- | ---: |
| Ages | `$0c:$49de` |
| Seasons | `$0b:$49e2` |

The runtime decodes and executes the cartridge bytes on demand, including the
global-flag branch, campaign-specific bank-`$15` host helper, actor-field jump
table, text-option branches, and ROM message commands. It uses typed Original
State Keys for original global flags and actor offsets. An instruction trace
retains the original source coordinates and scheduling outcomes.

Coverage is intentionally fail-closed: only opcodes and the two helper targets
reached by this route are registered. The first-visit ring tutorial, ring-list
state, and broad script coverage remain outside this slice.

Opcode `0x8d` also feeds the authoritative actor slot with Vasu's original
Y=`$12`, X=`$06` collision radii. Link uses his original 6-by-6 actor radii;
overlaps resolve on the shallower axis and movement slides along the remaining
axis. This is independent of the smaller tile-collision foot box and all
rendering geometry. See
[`actor-collision.md`](actor-collision.md).

## Developer scenario

Both campaigns use one command:

```powershell
build\oracle_room_slice.exe path\to\oracle.gbc --vasu-scenario
```

The flag chooses Ages `02:ee` or Seasons `01:91`, uses packed Link position
`$35`, and leaves the viewer in authentic-pixel mode. `Z` or Enter talks,
directions select an option, and `X` cancels. The MSVC room project launches
the Ages version of this scenario by default.
