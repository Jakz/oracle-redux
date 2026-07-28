# Share semantic UI across Fidelity and Modern layouts

Status: Accepted

Oracle Redux uses one immutable Semantic UI Model with two shipped retained
presentations. Fidelity UI reproduces the original 160×144 HUD, menus, dialogue
and timing-visible states; Modern UI may reflow and rescale the same semantics
for widescreen, localization, accessibility, and alternate input prompts. The
Oracle Runtime remains authoritative for inventory selection, dialogue
progression, pause behavior, and other outcome-relevant menu state. UI layouts
emit semantic input actions rather than mutating gameplay directly. Both
presentations produce backend-neutral interface instances for the SDL GPU
interface pass. Dear ImGui, if used, is restricted to developer diagnostics and
is not a shipped player interface.
