# Localize through stable message keys

Status: Accepted

ROM-derived text remains canonical for the Fidelity Baseline. The text decoder
maps each campaign message to a stable Message Key and a Semantic Text Stream
that separates human-readable spans from original controls such as variables,
choices, waits, page boundaries, speed, color, and sound. A localization pack
may replace readable spans and presentation hints for a Message Key but must
pass validation against the original control-token signature; it cannot add or
remove script-visible choices, waits, variables, or progression boundaries.
Missing or invalid entries fall back to decoded ROM text. Modern UI supports
Unicode shaping, reflow, and font fallback without changing the authoritative
Campaign Script.
