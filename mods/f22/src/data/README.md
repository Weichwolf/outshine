# `mods/f22/src/data/` — the campaign's real ground

| | |
|---|---|
| **`mekong-dem-90m.bin`** | the baked DEM of the campaign box. `mod.json` names it (`"dem"`), so `fb-gym`'s elevation default over this mod is `baked` and every sortie runs on real northern-Thai/Laotian terrain. **Untracked** — `.gitignore` and the rule behind it: [`doc/assets.md`](../../../../doc/assets.md) §0. Rebuild with a local `fb-tiles` on :8081: `python3 sim/tools/bake_dem.py --region mekong --verify 400` (~7 min, 7 885 unique z13 tiles). |
| **No weather fixture** | no sortie carries a `wx` line, so every run is `wx calm`. |
| **Aircraft and models come from `f16`** | `mod.json` declares `"depends": "f16"` and names no `aircraft` and no `models` key, so both resolve to the sibling mod's (`missions/FBMod.cpp`, the depends search). `"dem"` is deliberately NOT part of that search: a theatre's ground is the one asset a sibling's cannot stand in for. This mod owns declarations plus its own terrain. |

## The raster

| | Value | Provenance |
|---|---|---|
| Box | 17.90–21.70 N / 98.85–102.35 E | `[DERIV]` [`../../doc/terrain.md`](../../doc/terrain.md) §4's campaign box widened to enclose every lat/lon the eight sorties name — their CAP spawns sit outside it, up to 21.506 N / 102.156 E — plus ~0.15° of margin, rounded outward to 0.05° |
| Grid | 4 076 × 4 675, 38.11 MB | `[DERIV]` 90 m at the box's mid-latitude, WGS84 degree lengths (110 702 m/° lat, 104 786 m/° lon at 19.80 N) |
| Source | Terrarium z13 = **18.0 m/px** at 19.8 N | `[SET]` the same zoom `tiles/src/elev.c`'s `FB_DEM_Z` samples, so the baked surface IS `--elev tiles`' surface |
| No edge blend | — | the f16 fixture ramps its outer 15 km to 0 m because it is an ISLAND. This ground continues past its own box; a ramp would invent a cliff where 1.7's and 1.8's run-ins cross it |
| Elevation range in the box | **97 – 2 547 m**, mean 723 m | measured on the baked raster |
| Agreement with `/elev` | bias **+0.28 m**, rms **3.96 m**, max **15.74 m** over 400 pseudo-random interior points | measured, `bake_dem.py --verify 400` |

The rms is the **cost of the 90 m output grid** against the 18 m source it was sampled from, and it is
the whole error budget: source and reader are the same surface by construction.

Independent check on the anchoring rather than on the raster: `[MEAS]` the DEM answers **208.34 m** at
Nan Nakhon (VTCN, 18.80778 N / 100.78333 E) against the published field elevation of 209 m — 0.7 m,
and the only elevation datum inside the box that came from outside this tree.

## What the ground did to the campaign

`[MEAS]` real ground under the eight sorties runs **271 – 1 485 m**; FOB Tyler's reconstructed point is
a **741.9 m** hilltop and Chiang Rai comes out at 402.3 m. The full before/after is
[`../../doc/terrain.md`](../../doc/terrain.md) §8.
