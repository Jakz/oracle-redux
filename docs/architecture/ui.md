# User-interface architecture

## Boundary

The two shipped interface presentations share one semantic model:

```text
Oracle Runtime
  inventory, selection, dialogue, pause and prompt state
             |
             | immutable SemanticUiModel
             v
      +------+------+
      |             |
Fidelity layout  Modern adaptive layout
      |             |
      +------+------+
             |
             v
backend-neutral interface instances
             |
             v
SDL GPU interface pass
```

The Oracle Runtime owns state that can affect campaign outcomes, including
inventory selection, dialogue advancement, menu timing, pause behavior, and
script-visible choices. The retained UI layer owns layout, styling,
presentation-only animation, focus decoration, and input-prompt representation.
It cannot edit campaign state directly.

Input is expressed as semantic actions such as move selection, confirm, cancel,
open inventory, or choose a stable item identifier. Controller, keyboard,
mouse, and touch adapters produce those actions; the Oracle Runtime validates
and applies them on deterministic logic ticks.

## Semantic UI Model

The immutable model describes meaning rather than pixel coordinates or GPU
resources. Representative nodes include:

- health and capacity;
- equipped item identities and bindings;
- rupees, keys, essences, and other counters;
- inventory slots and the authoritative current selection;
- dialogue text, speaker, prompt, and progression state;
- contextual actions and input prompts;
- pause, save, settings, and campaign-shell state.

ROM-derived text, glyph, frame, icon, and palette references remain stable
content identifiers. A layout resolves those identifiers into presentation
instances through the same ROM Source and derived-cache boundary used by world
graphics.

## Fidelity UI

The Fidelity layout reproduces the original 160×144 safe frame, HUD placement,
dialogue box, glyph metrics, menu arrangement, visibility, and any timing that
is observable by gameplay or scripts. It uses Pixel-Snapped Presentation and
the original decoded visual resources.

Window framing and integer scaling may add unused output area, but do not
stretch or reflow the fidelity composition.

## Modern adaptive UI

The Modern layout may anchor HUD regions outside the original safe frame,
reflow menus for 16:9, scale text independently from the world camera, expose
additional input prompts, and accommodate localized strings or accessibility
settings. It consumes the same semantic values and stable selection identities
as the Fidelity layout.

Modern presentation cannot reveal hidden campaign information, add inventory
capacity, change dialogue choices, or alter menu timing merely because it has
more screen space. Such changes would require an explicit Gameplay Extension.

Localized text enters through stable Message Keys and a validated Semantic Text
Stream. Layout may reflow translated spans, but variables, choices, waits,
semantic page boundaries, and script-resume points remain those of the decoded
ROM message. See `docs/architecture/localization.md`.

## Rendering and tools

Both layouts emit backend-neutral interface instances into
`PresentationSnapshot`. The SDL GPU backend draws them after world
post-processing and downsampling, so world depth-of-field, color grading, and
zoom never soften the interface.

Dear ImGui may be integrated for development inspectors, render-graph views,
state traces, and metadata authoring. It is not used for the shipped HUD,
dialogue, inventory, launcher, or settings interface.
