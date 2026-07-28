# Oracle Redux

This context describes the shared product formed by the two Oracle adventures
and the language used to separate common rules from campaign-specific content.

## Language

**Oracle Redux**:
The single product containing both Oracle campaigns and their shared player-facing shell.
_Avoid_: Oracle Collection, Combined ROM, merged game

**Campaign**:
One complete adventure selected within Oracle Redux: Seasons or Ages.
_Avoid_: Version, ROM

**Oracle Runtime**:
The rules and state transitions shared by both campaigns, independent of presentation and platform services.
_Avoid_: Emulator, common ROM

**Native Behavioral Reimplementation**:
The C++ rewrite of original campaign behavior, informed by the ROM, disassembly, and observed execution without running or mechanically translating original gameplay machine code; the isolated Oracle Audio VM is the sole machine-code exception.
_Avoid_: Gameplay emulator, transpilation, general embedded ROM execution

**Campaign Definition**:
The campaign-specific rules, identifiers, and content references supplied to the Oracle Runtime.
_Avoid_: Fork, game switch

**Campaign Script**:
A ROM-supplied domain-specific bytecode program for interactions, dialogue events, cutscenes, movement patterns, or tile sequences; it is content interpreted by native C++, not cartridge machine code.
_Avoid_: LR35902 code, rewritten cutscene

**Native Script Runtime**:
The C++ decoder and interpreter that preserves Campaign Script control flow and tick behavior while routing hardware- and assembly-facing operations through typed runtime services.
_Avoid_: CPU emulator, arbitrary address execution

**Oracle Audio VM**:
A sandboxed, non-authoritative LR35902 and Game Boy APU environment that executes only validated original sound-driver code and data from the supplied ROM Source behind typed audio commands.
_Avoid_: Game emulator, gameplay VM, native audio sequencer

**Original State Key**:
A validated symbolic identity for a campaign state field that retains its original memory address for decoding and diagnostics while resolving to typed C++ state.
_Avoid_: Host pointer, emulated RAM access

**ROM Source**:
A locally supplied, compatible original cartridge image from which campaign content is decoded at runtime.
_Avoid_: Bundled assets, game data package

**Derived Asset Cache**:
An optional local and reproducible acceleration of content decoded from a ROM Source. It is not product source or distributable campaign content.
_Avoid_: Asset pack, extracted game files

**Original Save Image**:
An Ages or Seasons cartridge-compatible SRAM image that Oracle Redux can validate, import, preserve, and explicitly export.
_Avoid_: Redux settings file, live ROM

**Journey Slot**:
A player-facing Oracle Redux slot that pairs independent Ages and Seasons campaign records under one shared linkage identity.
_Avoid_: Cartridge slot, merged campaign state

**Redux Save Envelope**:
Oracle Redux's primary save container, holding typed campaign state, preserved original-save bytes, and Redux-only metadata without making extensions part of the cartridge format.
_Avoid_: Patched SRAM, emulator save state

**Fidelity Baseline**:
The original campaign behavior against which the reimplementation is compared: identical authoritative starting state, input, RNG state, and logic tick must produce equivalent gameplay state changes and events.
_Avoid_: Old mode, emulation mode

**Behavioral Equivalence**:
Agreement with the Fidelity Baseline at observable gameplay boundaries, without requiring the C++ design or function structure to resemble the original assembly.
_Avoid_: Line-by-line translation, code-shape matching

**Provisional Behavior**:
Reimplemented gameplay behavior supported by disassembly evidence and ordinary tests but not yet compared against original execution traces.
_Avoid_: Verified, complete

**Fidelity-Verified Behavior**:
Reimplemented gameplay behavior demonstrated equivalent to the Fidelity Baseline through representative original-versus-C++ execution traces.
_Avoid_: Looks correct, source-inspired

**Compatibility Quirk**:
An original non-destructive irregularity preserved because it affects observable gameplay, scripts, RNG, established techniques, or save compatibility.
_Avoid_: Accidental cleanup, unsafe defect

**Safety Deviation**:
A documented departure from original behavior required to prevent crashes, save corruption, unsafe memory behavior, or platform-security problems.
_Avoid_: Balance fix, quality-of-life change

**First Playable Slice**:
A thin end-to-end gameplay loop implemented in both campaigns that exercises movement, transitions, interaction, combat, an item, persistent state, interface, saving, and audio.
_Avoid_: Room viewer, full campaign

**Presentation Extension**:
An optional visual, audio, input, or interface improvement that does not alter the Fidelity Baseline.
_Avoid_: Engine hack

**Presentation Metadata**:
Engine-owned annotations that assign visual height, sorting, occlusion, shadow, material, focus, or lighting semantics to stable ROM-derived identifiers without containing original campaign content.
_Avoid_: Extracted asset pack, replacement graphics

**Experience Profile**:
A named set of default rules and presentation capabilities. Classic and Modern profiles are starting points whose individual settings can be overridden.
_Avoid_: Separate engine, hard-coded mode

**Classic Profile**:
An Experience Profile whose defaults preserve the Fidelity Baseline, including original room activation and item-button limits.
_Avoid_: Emulator mode, legacy engine

**Modern Profile**:
An Experience Profile whose defaults enable modern presentation while preserving the Fidelity Baseline's gameplay rules.
_Avoid_: Remake fork, incompatible mode

**Gameplay Extension**:
An explicit, opt-in rule change that intentionally departs from the Fidelity Baseline and is configured independently from presentation.
_Avoid_: Modern graphics, invisible enhancement

**Simulation World**:
The authoritative gameplay state whose coordinates, activation rules, collisions, scripts, and timing reproduce the selected campaign.
_Avoid_: Render world, visible world

**Actor Slot Domain**:
The authoritative, bounded, and ordered actor storage that preserves the original object categories, allocation behavior, and update ordering.
_Avoid_: Unbounded entity pool, render object list

**Active Simulation Region**:
The explicitly declared set of rooms whose actors, scripts, collisions, and other authoritative gameplay advance during a logic tick.
_Avoid_: Visible rooms, loaded rooms

**Fidelity Activation**:
The Fidelity Baseline's original room-defined boundary for advancing actors, scripts, collisions, and other authoritative gameplay.
_Avoid_: Camera activation, visible-region simulation

**Seamless Simulation Extension**:
An opt-in Gameplay Extension that advances a deterministic multi-room region around Link independently of camera framing, output resolution, and zoom.
_Avoid_: Widescreen mode, camera-driven simulation

**Presentation Camera**:
A non-authoritative view over world-space presentation data that controls framing, aspect ratio, and zoom without changing Simulation World behavior.
_Avoid_: Gameplay camera, simulation viewport

**Presentation Snapshot**:
An immutable, backend-neutral frame description containing visible world and interface instances, presentation metadata, camera state, and interpolation state derived from completed simulation ticks.
_Avoid_: Draw-call list, mutable scene, SDL state

**Semantic Render Graph**:
The renderer-owned description of logical frame resources and ordered visual passes, expressed in terms such as world depth, materials, lighting, atmosphere, post-processing, and interface rather than SDL GPU commands.
_Avoid_: Gameplay graph, generic copy of SDL GPU, scene simulation

**SDL GPU Backend**:
The production adapter that translates the Semantic Render Graph into concrete SDL GPU resources, pipelines, command buffers, render or compute passes, and swapchain presentation.
_Avoid_: Simulation renderer, high-level SDL Render API

**Static World Instance Arena**:
A large persistent GPU allocation containing contiguous ranges of ROM-derived tile instances for many rooms or chunks, rendered through indexed instancing or indirect range submission.
_Avoid_: One buffer per room, flattened room texture, mutable actor stream

**Indexed Texel Atlas**:
A GPU texture atlas containing exact ROM-derived tile or sprite color indices whose colors and transparency are resolved from an explicitly selected palette domain in the world shader.
_Avoid_: Precolored sprite sheet, flattened RGBA room, filtered source art

**Pixel-Snapped Presentation**:
A Fidelity presentation policy that aligns the camera and presented geometry to the original pixel grid and prefers integer output scaling.
_Avoid_: Simulation quantization, mandatory window size

**Subpixel Presentation**:
A Modern presentation policy that interpolates presented transforms between completed logic ticks while retaining nearest-sampled source texels and unchanged simulation state.
_Avoid_: Linear sprite filtering, fractional gameplay state

**World Working Surface**:
An intermediate resolved-color, depth, and material target used for world effects and palette-correct downsampling before output-resolution interface composition.
_Avoid_: Indexed atlas, authoritative world state, final UI surface

**Semantic Focus Mask**:
Non-authoritative presentation data that protects selected gameplay-important world pixels from excessive depth-of-field while allowing ordinary depth-based focus elsewhere.
_Avoid_: Targeting mask, collision layer, blanket sharpness

**Presentation Light**:
A non-authoritative directional, point, or cone light used by Modern presentation with authored material, height, shadow, and focus metadata.
_Avoid_: Gameplay visibility source, PBR requirement, collision object

**Semantic UI Model**:
An immutable, renderer-neutral description of interface meaning derived from authoritative runtime and shell state, shared by Fidelity and Modern layouts.
_Avoid_: Pixel coordinates, mutable inventory, GPU widget

**Fidelity UI**:
The retained interface presentation that reproduces the original safe frame, HUD, dialogue, menus, resources, and timing-visible states.
_Avoid_: Emulator overlay, modern side panel

**Modern Adaptive UI**:
The retained interface presentation that may reflow and rescale the Semantic UI Model for widescreen, localization, accessibility, and input prompts without changing gameplay state.
_Avoid_: Separate menu logic, gameplay expansion

**Message Key**:
A stable campaign-qualified identity for an original text record used by decoding, localization, diagnostics, and fallback without depending on translated prose.
_Avoid_: English string, screen coordinate, transient pointer

**Semantic Text Stream**:
A campaign-neutral sequence that separates readable text from variables, choices, waits, page boundaries, sound cues, and other behaviorally meaningful original text controls.
_Avoid_: Plain string, arbitrary campaign script, rendered glyph list

**Localization Pack**:
A versioned set of UTF-8 text substitutions and presentation hints keyed by Message Key and validated against each original message's semantic control-token signature.
_Avoid_: Patched ROM, replacement script, save content

**Input Frame**:
The backend-neutral held, pressed, and released semantic action state consumed by the Oracle Runtime for one deterministic logic tick.
_Avoid_: SDL event, controller snapshot, render-frame input

**Binding Profile**:
A platform-side mapping from physical keys, buttons, axes, pointers, or touches to stable semantic actions without changing those actions' gameplay meaning.
_Avoid_: Save slot, action script, gameplay extension

**Seamless World Presentation**:
A Presentation Extension that visually composes room content into a continuous world. It does not by itself add rooms to the Active Simulation Region.
_Avoid_: Seamless simulation, room removal

**2.5D World Presentation**:
An orthographic presentation of original pixel art with explicit visual height, depth, layers, and shadows for sorting and effects, without requiring perspective geometry or replacement artwork.
_Avoid_: 3D remake, flat framebuffer

**World Overview**:
A Presentation Extension that draws multiple cached rooms or larger world regions at a reduced scale without activating their gameplay.
_Avoid_: Zoomed simulation, active overworld

**Linked Journey**:
A campaign playthrough whose initial state incorporates completion data from the other campaign, whether linked directly within a Journey Slot or through an original transfer secret.
_Avoid_: New Game Plus
