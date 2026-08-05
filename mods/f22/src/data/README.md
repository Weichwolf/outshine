# `mods/f22/src/data/` — empty, and that is the declaration

This mod ships no baked terrain and no weather fixture, and the emptiness is load-bearing rather than
a to-do:

| | |
|---|---|
| **No `swiss-dem-90m.bin`** | `fb-gym`'s elevation default is *swiss if the mod's `data/swiss-dem-90m.bin` exists, else const* (`clients/FBAppGym.cpp`). The campaign box is northern Thailand and Laos; the Swiss island would answer 0 m over all of it while claiming to be a DEM. With no file, `--elev const` is the default, no `.fbm` declares a `runway`, and the ground is an honest flat 0 m plane everywhere — stated as disclosure **D3** in `../missions/c01m01-snake-eyes.fbm`. |
| **No weather fixture** | no sortie carries a `wx` line, so every run is `wx calm`. |
| **Aircraft and models come from `f16`** | `mod.json` declares `"depends": "f16"` and names no `aircraft` and no `models` key, so both resolve to the sibling mod's (`missions/FBMod.cpp`, the depends search). This mod owns declarations only. |

**The real ground was never measured.** `../../doc/terrain.md` §5.4 gives 300–2 000 m for the region
from a description, with exactly one verified datum inside the box (VTCN at 209 m), and the box has
never been fetched from `fb-tiles`. Running these missions with `--elev tiles` against a live
tileserver is the first thing that would change every altitude in them.
