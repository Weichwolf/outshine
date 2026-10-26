# Verified Python vegetation takeover

PR [#202](https://github.com/Weichwolf/outshine/pull/202) preserves the entire old
`outshine_vegetation` head `782f1dde25ea2aa92bd4f3bc35188d2583e325a8` by merging
it onto master `3ec037459`; merge producer `af28666aa`, consumer/path corrections
`05073dc12`. Owner is now `outshine_research`. This delivers bounded reference
procedures and reproducible rejected studies, not a new accepted tree morphology.
Current #182/#643, #131/#168 and native #1/#48 retain their unmet acceptance.

## Contracts and consumer corrections

- Opt-in `branching-plant-4` actual attachment radii reach tables, bark, collars,
  surfaces and needle offsets. Conservative limits and area/window preflight
  require no axes. Explicit adaptive paths permit 256 siblings; legacy defaults
  retain 64. No existing geometry tolerance or native gate was increased.
- `NeedledPlant.area_summary` counts every stable global variant across orders
  without individual organs. Expected squared uniform scale is `1+variation²/3`.
  Half the closed physical prototype area is the declared needle-area convention;
  author crown footprint and nominal LAI are not measured stand LAI.
- The existing parent-profile study and historical Place/grass consumers are
  preserved. Scripts touched by this takeover now require explicit source/output
  paths. New private outputs must lie below the system temp directory, never the
  old `build/experiments/` defaults. Coupled catalog/check/tile readers were migrated.
- The original DarmstadtWest camera control now names its archived scenario
  explicitly. All four historical heights/lenses remain asserted; no substitute
  camera or relaxed test. Pinned raster compatibility loads the exact historical
  git source through importlib, without changing its output oracle.
- #208's old four-Place presentation was superseded by PR #290. No gallery,
  README/doc images, native implementation or eight-form/350-site rebake is added.

## Fresh correctness and installation evidence

Linux Intel i5-1135G7, Python 3.13.5 in `.venv`; NumPy 2.5.4. Source tests at
`af28666aa`: **611 passed**, 14.962 s. A fresh no-dependency wheel was independently
installed below the evidence root; **611 passed**, 26.847 s, with source `lib/src`
absent from that test path. Wheel SHA256:
`2825cf18e56d06b2acb947955a6842cfccf932cde7aba6adc67baf0eeadcebb0`.
Library source is unchanged through `05073dc12` and this report.

At `05073dc12`, **53 affected experiment controls passed** from source (4.848 s)
and independently installed package (12.356 s). Nineteen coupled CLI help/import
checks passed. Format/Ruff and functions ≤120 lines cover the affected Python
closure; documentation contracts and diff whitespace are checked separately.
Missing declared scikit-image, tifffile and pyproj dependencies were installed
in `.venv`; native builds/gates were not run for this Python delivery.

Seven full legacy cases were recomputed against frozen pre-takeover master
`942a94961`: three original saplings with seeds 0,17,2⁶⁴−1 and four original young
conifers. Recipe identities, all typed table/window arrays and twelve near queries
match exactly. The historical pinned raster also matches. Independent small
radius and area tests retain invalid-input, no-detail preflight and seed-boundary
controls; no pins or tolerances were rewritten.

`mature_radius_probe.py` checks complete existing mature v4 recipes, independently
recursing every attachment radius, enumerating every global variant in bounded
chunks and requesting three exact near ranges. Whole foliage and new needle
meshes are forbidden during the full window stage; preflight forbids axes.

| Recipe, variant 1 | Spruce/source | Scots pine/installed wheel |
|---|---:|---:|
| Axes / windows | 50,401 / 61,760 | 20,185 / 25,266 |
| Represented needles | 11,407,570 | 2,764,884 |
| Requested / expected area, m² | 204.140691 / 200.001725 | 200.950992 / 201.435118 |
| Relative error, unchanged 5% limit | 2.02751% | 0.240917% |
| Maximum independent radius error, m | 1.215e−17 | 1.388e−17 |
| Largest near offset error, m | 3.056e−15 | 3.005e−15 |
| Window / radii array bytes | 8,461,120 / 806,416 | 3,461,442 / 322,960 |
| Full validation / preflight, s | 54.3723 / 0.7822 | 27.5009 / 0.2407 |
| Process peak RSS, KiB | 211,420 | 115,064 |

Times include independent validation and private serialization under concurrent
host load, not native runtime. Original 65,536-axis/100,000-window profile stays.
Spruce identity `7b0ab06dee97c8904e3673db9dcb6355191fe1a38252efd4b498a3de8016f97c`
matches the old columnar control. Successful physical invariants do not accept it.

## Complete current tree and actual images

The versioned `data/picea-mature-study.json` was reconstructed independently at
`942a94961` and `af28666aa` with `spruce_study_source.py --verify-reference`.
All pinned physical mesh/catalog fields match; source graph identity is
`56efdf502cb2c878835fca972292a10cd45d6abd7ae3a4960431bd21ceebed1b`.
Both complete executions contain **48,511 axes, 77,175 windows and 13,837,866
needles**, identity `55c289474ce80fb2e09573130e278e9b2fcc8dc22faaf4ab4448a0839b5d1b85`.
All twenty saved axis/window columns are byte-identical. Three fresh cached near
requests also match all 27 typed array descriptors, without regrowth/whole foliage.
Original 60,000-axis/80,000-window study bounds remain. Expected physical area is
242.610567 m² against nominal author target 250.072346 m².

Both **2560×1440, 32-sample Eevee** full portraits were actually rendered and
opened. Official `test/scripts/pixels.py` reports **0 / 3,686,400 changed pixels**.
Both show the full spruce, light and ground shadow. The pointed crown remains
too narrow, transparent and regularly tiered, with exposed stem: compatibility
is verified, botanical/SpeedTree/light/LOD quality remains unaccepted. Existing
#182/#643, not another count/profile search, own the next visible improvement.

Complete execution before/after: 49.905/49.346 s, peak 284.04/284.12 MiB RSS.
Portraits before/after: 472.255/472.543 s render, peak 3044.64/2974.78 MiB RSS.
Two concurrent software render processes used nice10, LP_NUM_THREADS=2 and BLAS=1;
these are descriptive observations, not a speed/memory ranking or device proof.
Both use 2,328,560 wood triangles and 2,760,454 total camera/shadow-role triangles;
four repeated wood prototypes reduce buffer work, not triangle count. Decoded
axis/window arrays: 20,034,839 B; catalog images: 2,359,296 B; assembled geometry
outputs: 48,412,644 B; wood prototype/instance buffers: 21,696/1,768,092 B;
explicit Blender bulk-write payload: 1,760,380 B. Total renderer allocation/copy
traffic remains unmeasured; these payloads are not that total.

## Counting costs and reproducible entry points

`needle_area_cost.py` compares the actual full v4 recipes with bounded explicit
global-variant enumeration, same physical areas and per-order squared-scale
expectation. Per-order areas/counts agree at the original 2e−15 tolerance; seeds
0,17,2⁶⁴−1 also pass. Five ordinary repetitions are separate from five Memray
repetitions with Python allocators traced; preloaded query scope excludes IO and
geometry. Analytical work uses four orders × three variants, no enumerated IDs.
The enumerator visits all 11,407,570/2,764,884 IDs with at most 65,536 per batch;
its explicit integer-cast copies are 91,260,560/22,119,072 B per call. These copy
counts exclude other buffer writes and are not complete memory traffic.

| Query | Ordinary median, ms | Allocations / bytes, five calls | Tracked peak, B | Process RSS, KiB |
|---|---:|---:|---:|---:|
| Spruce analytical | 0.047242 | 1,115 / 70,513 | 3,464 | 48,828 |
| Spruce enumerated | 83.3445 | 31,970 / 914,320,105 | 1,051,200 | 49,844 |
| Pine analytical | 0.026362 | 1,100 / 70,033 | 3,464 | 49,144 |
| Pine enumerated | 20.9832 | 8,390 / 221,647,625 | 1,051,200 | 49,888 |

The raw profile JSONs record allocations, allocated bytes and live tracked peak;
process RSS additionally includes interpreter/imported libraries. A first default
Memray run recorded no allocator events for five tiny analytical queries and its
stats histogram failed; that capture was replaced with explicit Python-allocator
tracking. No algorithm/output correction or lowered requirement followed.

All current original inputs, reports, arrays and opened image pairs are retained
under `/tmp/outshine-research-vegetation-handoff-3ec037459/` on this host. Historical
`/var/tmp/outshine-vegetation-*` paths in earlier reports refer to the old host;
they were not required to reconstruct the versioned study and are not claimed
lost elsewhere. Frozen producers remain separate from the working branch.

Use `PYTHONPATH=lib/src:test/experiments/vegetation` and `.venv/bin/python`:

```sh
.venv/bin/python test/experiments/vegetation/spruce_study_source.py \
  --output "$probe_root/source" --verify-reference
.venv/bin/python test/experiments/vegetation/mature_radius_probe.py \
  --source "$probe_root/young-reference" \
  --profile data/world/species/spruce.json --species picea-abies \
  --variant 1 --output "$probe_root/spruce"
.venv/bin/python test/experiments/vegetation/needle_area_cost.py \
  --source "$probe_root/young-reference" --recipe "$probe_root/spruce/recipe.json" \
  --species picea-abies --method enumerated --output "$probe_root/cost"
```

`probe_root` must be a fresh explicit directory below the system temp directory.
For each species in `crown_conifers()[:2]`, save `species.plant().recipe()` as
`<species>/whole-plan.json`; spruce `shoot.json` and its three meshes are the
rebuilt study's `source/models/`. Pine prototypes use the existing
`conifer_crown_products.needle_meshes(species, directory, "whole")`; `shoot.json`
lists those three filenames as leaf products. Spruce study mesh tessellation
differs from that young-product helper, so they must not be substituted silently.
Each probe records actual input hashes. Full study execution/render/replay use
the existing `spruce_growth_execute.py`, `window_portrait.py` and
`spruce_growth_replay.py` explicit path arguments, with inputs derived from that
same versioned source. Earlier eight-form forest evidence remains historical;
no current eight-form image, native integration or 1280×720/60 claim is made.
