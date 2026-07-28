# Text and localization architecture

## Canonical source

Compatible US ROM Sources provide the initial canonical dialogue, labels,
control codes, and original glyph references. The decoder does not expose a
finished line-broken string directly:

```text
ROM text record
      |
      v
MessageKey + SemanticTextStream
      |
      +--------------------+
      |                    |
decoded ROM spans     validated localized spans
      |                    |
      +---------+----------+
                |
                v
       Semantic UI Model
                |
        +-------+-------+
        |               |
 Fidelity layout   Modern adaptive layout
```

A Message Key combines campaign identity with the original stable text group,
table, and message identity required to locate the record. Human-readable names
may be added for authoring, but are aliases rather than persistence keys.

## Semantic text stream

The stream distinguishes readable content from behaviorally meaningful
controls. Representative token classes include:

- Unicode text spans;
- original glyph or icon references;
- explicit line and semantic page boundaries;
- variable and player-name substitutions;
- choices and choice identities;
- waits, pauses, and text-speed changes;
- color or emphasis changes;
- sound and other presentation cues;
- end-of-message and script-resume boundaries.

The exact original byte format remains a reverse-engineering responsibility.
The campaign-neutral token model is the stable boundary consumed by UI and
localization.

## Localization packs

A pack is keyed by Message Key and supplies UTF-8 text spans, optional font
preferences, and non-authoritative layout hints. It does not replace Campaign
Scripts or arbitrary control bytecode.

Pack validation compares each entry with the original message's control-token
signature. Variables, choices, waits, script-resume boundaries, and other
required controls must remain present and compatible. Invalid entries are
reported with their Message Key and fall back to the decoded ROM message.
Missing messages also fall back individually, allowing incomplete translations
to remain usable.

Localization packs are versioned against:

- campaign identity and compatible ROM fingerprint family;
- localization schema version;
- stable Message Key schema;
- expected semantic-token signatures.

They are independent from Redux saves. A save never embeds localized campaign
text and remains loadable after changing or removing a pack.

## Layout and progression

Visual wrapping is not a new Campaign Script boundary. Modern UI can reflow,
scroll, resize, or reveal text within one semantic page without inventing an
additional authoritative confirm action. Original waits, choices, and
script-resume boundaries remain the only gameplay-visible progression points.

Fidelity Baseline rendering uses original text, original glyphs, and original
layout. A localized presentation can use the Fidelity visual skin, but ceases
to be pixel-exact text output and must still preserve the semantic progression
contract.

## Fonts

Original ROM glyphs remain available for compatible text. Modern localization
supports Unicode shaping and fallback fonts behind a font-provider boundary;
the exact third-party shaping and rasterization libraries are postponed. Font
files are engine or pack resources and are never extracted from unrelated
installed software into distributable caches.
