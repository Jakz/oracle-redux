# Use stylized palette-preserving lighting

Status: Accepted

Modern presentation treats palette-resolved Game Boy colors as the visual base
and applies constrained, art-directed lighting rather than a metallic-roughness
PBR workflow. Presentation Metadata assigns simple material response, visual
height, shadow behavior, and optional emission. Scenes may combine a
directional key light with sparse point or cone spotlights, including authored
dungeon and cutscene lights. Light placement and tuning are postponed until
playable fidelity content can be evaluated. Fidelity presentation bypasses
modern lighting, and lighting never changes visibility, targeting, collision,
scripts, or other authoritative gameplay.
