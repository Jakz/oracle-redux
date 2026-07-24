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

**Campaign Definition**:
The campaign-specific rules, identifiers, and content references supplied to the Oracle Runtime.
_Avoid_: Fork, game switch

**ROM Source**:
A locally supplied, compatible original cartridge image from which campaign content is decoded at runtime.
_Avoid_: Bundled assets, game data package

**Derived Asset Cache**:
An optional local and reproducible acceleration of content decoded from a ROM Source. It is not product source or distributable campaign content.
_Avoid_: Asset pack, extracted game files

**Fidelity Baseline**:
The original campaign behavior against which the reimplementation is compared before optional improvements are applied.
_Avoid_: Old mode, emulation mode

**Presentation Extension**:
An optional visual, audio, input, or interface improvement that does not alter the Fidelity Baseline.
_Avoid_: Engine hack

**Experience Profile**:
A named set of default rules and presentation capabilities. Classic and Modern profiles are starting points whose individual settings can be overridden.
_Avoid_: Separate engine, hard-coded mode

**Classic Profile**:
An Experience Profile whose defaults preserve the Fidelity Baseline, including original room activation and item-button limits.
_Avoid_: Emulator mode, legacy engine

**Modern Profile**:
An Experience Profile whose defaults enable the collection's modern capabilities, including seamless presentation, smooth rendering, and expanded controls.
_Avoid_: Remake fork, incompatible mode

**Simulation World**:
The authoritative gameplay state whose coordinates, activation rules, collisions, scripts, and timing reproduce the selected campaign.
_Avoid_: Render world, visible world

**Active Simulation Region**:
The explicitly declared set of rooms whose actors, scripts, collisions, and other authoritative gameplay advance during a logic tick.
_Avoid_: Visible rooms, loaded rooms

**Presentation Camera**:
A non-authoritative view over world-space presentation data that controls framing, aspect ratio, and zoom without changing Simulation World behavior.
_Avoid_: Gameplay camera, simulation viewport

**Seamless World Presentation**:
A Presentation Extension that visually composes room content into a continuous world. It does not by itself add rooms to the Active Simulation Region.
_Avoid_: Seamless simulation, room removal

**World Overview**:
A Presentation Extension that draws multiple cached rooms or larger world regions at a reduced scale without activating their gameplay.
_Avoid_: Zoomed simulation, active overworld

**Linked Journey**:
A campaign playthrough whose initial state incorporates completion data from the other campaign.
_Avoid_: New Game Plus
