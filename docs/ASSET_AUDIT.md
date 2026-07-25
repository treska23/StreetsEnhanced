# Asset audit — Streets of Rage (Master System)

This document tracks known public references for reconstructing the visual and gameplay assets of the Master System version.

## Important repository rule

Do not commit the original ROM or copyrighted ripped assets to the public repository. Keep original/ripped files in a local ignored folder and use this document as a provenance and completeness manifest.

## Confirmed public sprite sheets

Source: The Spriters Resource — Master System / Streets of Rage

- Playable characters: Adam Hunter, Axel Stone, Blaze Fielding
- Enemies and bosses: Abadede, Antonio, Bongo, Galsia, Hakuyo, Jack, Mr. X, Nora, Onihime/Yasha, SMS-exclusive boss, Souther, Y. Signal
- Miscellaneous: font

Index:
https://www.spriters-resource.com/master_system/streetsofrage/

## Confirmed stage maps

Source: VGMaps — Master System atlas

- Round 1 — 2040 × 176 PNG
- Round 2 — 2040 × 176 PNG
- Round 3 — 2040 × 176 PNG
- Round 4 — 2040 × 176 PNG
- Round 5 — 3576 × 176 PNG
- Round 6 — 3576 × 176 PNG
- Round 7 — 248 × 1408 PNG
- Round 8 — 3825 × 176 PNG

Index:
https://www.vgmaps.com/Atlas/MasterSystem/index.htm

## Reference screenshots

MobyGames and GameFAQs have screenshots useful for checking HUD, title screen, palettes, weapons, props and states not represented cleanly in existing sheets.

- MobyGames Master System screenshot gallery
- GameFAQs screenshot gallery and walkthrough

## Missing or not yet verified as complete

- HUD elements and life bars
- Title screen and menus
- Ending screens and interstitials
- Weapons and consumable items
- Breakable props and environmental objects
- Hit effects, shadows and particles
- Complete palette variants for every enemy
- Exact animation timing and frame order
- Per-frame collision/hurt/hit boxes
- Stage tile layers and parallax separation
- Music data and sound effects
- Enemy spawn tables and stage scripting
- Original object coordinates and collision geometry

## Recommended acquisition pipeline

1. Use public sprite sheets and maps only as an audit/reference layer.
2. Obtain a legally owned ROM locally.
3. Build a local `ResourceImporter` that reads the ROM and emits normalized PNG/JSON assets.
4. Compare extracted data against public sheets to detect missing frames or palette variants.
5. Store generated copyrighted assets outside Git or in a local ignored directory.
6. Commit only importer code, metadata schemas, checksums and original replacement assets.

## Proposed local-only directory layout

```text
LocalAssets/
  OriginalRom/
  Extracted/
    Characters/
    Enemies/
    Stages/
    UI/
    Audio/
  ReferenceSheets/
```

`LocalAssets/` must be added to `.gitignore` before importing any original material.

## Next audit step

Create a frame-by-frame inventory for Axel, Adam and Blaze, then repeat it for each enemy. Every animation should record frame rectangle, pivot, duration, hurtbox, hitbox, movement delta and palette id.
