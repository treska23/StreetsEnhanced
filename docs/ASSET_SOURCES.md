# Asset source manifest

This file preserves every confirmed public reference located for the Master System version of *Streets of Rage*.

## Repository policy

The repository is public. Do not commit the original ROM or ripped Sega graphics, maps, music or sound effects. Keep those files locally under `LocalAssets/`, which is ignored by Git. This manifest preserves the source locations so the material can be reacquired and audited without losing the research.

## Sprite sheets

### The Spriters Resource

Game index:

https://www.spriters-resource.com/master_system/streetsofrage/

Confirmed categories:

- Adam Hunter
- Axel Stone
- Blaze Fielding
- Abadede
- Antonio
- Bongo
- Galsia
- Hakuyo
- Jack
- Mr. X
- Nora
- Onihime / Yasha
- Master System-exclusive boss
- Souther
- Y. Signal
- Font

Status: confirmed public reference; individual sheet URLs and local checksums still to be catalogued.

## Stage maps

### VGMaps

Master System atlas:

https://www.vgmaps.com/Atlas/MasterSystem/index.htm

Confirmed maps:

- Round 1 — 2040 × 176 PNG
- Round 2 — 2040 × 176 PNG
- Round 3 — 2040 × 176 PNG
- Round 4 — 2040 × 176 PNG
- Round 5 — 3576 × 176 PNG
- Round 6 — 3576 × 176 PNG
- Round 7 — 248 × 1408 PNG
- Round 8 — 3825 × 176 PNG

Status: confirmed public reference; direct image URLs and local checksums still to be catalogued.

## Screenshot references

Useful for checking HUD, menus, palettes, weapons, props, transitions and animation states not represented cleanly in sprite sheets:

- MobyGames screenshot gallery
- GameFAQs screenshot gallery and walkthrough material

Status: exact page URLs still to be added.

## Local preservation layout

```text
LocalAssets/
  OriginalRom/
  ReferenceSheets/
    Characters/
    Enemies/
    UI/
    Stages/
  Extracted/
    Characters/
    Enemies/
    UI/
    Stages/
    Audio/
  Checksums/
```

## Required catalogue fields

Every locally preserved file should eventually have a manifest entry containing:

- Source page
- Original filename
- Local filename
- Category
- Character, enemy, stage or UI element
- Width and height
- SHA-256 checksum
- Acquisition date
- Notes about completeness
- Whether it is reference-only or extracted from a legally owned ROM

## Next task

Catalogue the direct page and file URL for every character, enemy, font and round map; then generate a local SHA-256 inventory after the files are downloaded outside Git.
