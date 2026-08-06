# `mods/armored-fist/src/data/` — Overwatch's real ground

| | |
|---|---|
| **`sindh-dem-90m.bin`** | the baked DEM of the campaign box. `mod.json` names it (`"dem"`), so `fb-gym`'s elevation default over this mod is `baked` and every mission runs on the real Thar desert and lower Indus. **Untracked** — `.gitignore` and the rule behind it: [`doc/assets.md`](../../../../doc/assets.md) §0. Rebuild with a local `fb-tiles` on :8081: `python3 sim/tools/bake_dem.py --region sindh --verify 400` (5.3 min, 7 345 unique z13 tiles). |
| **No weather fixture** | no mission carries a `wx` line, so every run is `wx calm`. |
| **Aircraft and models come from `f16`** | `mod.json` declares `"depends": "f16"` and names no `aircraft` and no `models` key, so both resolve to the sibling mod's (`missions/FBMod.cpp`, the depends search). `"dem"` is deliberately NOT part of that search: a theatre's ground is the one asset a sibling's cannot stand in for. |

## The raster

| | Value | Provenance |
|---|---|---|
| Box | 24.65–27.10 N / 66.55–71.40 E | `[DERIV]` [`../../doc/terrain.md`](../../doc/terrain.md) §4's campaign union box widened to enclose every lat/lon the seven missions name — their run-in spawns sit up to 40 km outside it and Corrosion's egress crosses the Karachi coast — then rounded outward to 0.05° |
| Grid | 5 402 × 3 017, 32.60 MB | `[DERIV]` 90 m at the box's mid-latitude, WGS84 degree lengths |
| Source | Terrarium z13 = **17.9 m/px** at 25.9 N | `[SET]` the same zoom `tiles/src/elev.c`'s `FB_DEM_Z` samples, so the baked surface IS `--elev tiles`' surface |
| No edge blend | — | this ground continues past its own box, and the Indus delta's real 0 m coastline is inside it: a synthetic ramp would be indistinguishable from the sea that is actually there |
| Elevation range in the box | **−33 … 2 028 m**, mean 144.0 m | measured on the baked raster; the maximum is the Kirthar range off the box's north-west corner, the minimum the Rann of Kutch salt flats |
| Tiles | 7 345 unique, **0 holes** | measured, `bake_dem.py` |
| Agreement with `/elev` | bias **+0.14 m**, rms **1.63 m**, max **7.86 m** over 400 pseudo-random interior points | measured, `bake_dem.py --verify 400` |

The rms is the **cost of the 90 m output grid** against the 17.9 m source it was sampled from, and it is
the whole error budget: source and reader are the same surface by construction.

## What the ground did to the campaign

`[MEAS]` ground under the seven objectives runs **21.1 – 248.6 m** — Night's Quest lowest on the Indus
plain, Night Forger highest in the Thar dune belt. Every attack waypoint is its own aim point's ground
plus 900 m, rounded up to the next 10 m, so all seven missions fly the same 900 m AGL laydown the
engine's own attack fixtures use.

**It changed no outcome that was measured.** The `stores DELIVERY` lines report `planeM` against
`groundAslM` — the computation plane against the real ground at the impact point — and the largest
disagreement across 29 deliveries is under 3 m. This ground is flat where the fighting is; the DEM is
here because the theatre is real, not because the relief decides anything.
