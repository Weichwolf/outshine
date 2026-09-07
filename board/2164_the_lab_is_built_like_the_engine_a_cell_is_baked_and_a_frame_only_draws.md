Type: debt
State: open · ACTIVE
Area: lab
Tags: architecture, owner
Depends: nothing
Lab rank: 1

# The lab is built like the ENGINE: a cell is BAKED and a frame only DRAWS

**Benchmark** -- RAGE: a fixed world grid of map sectors; every drawable, bound and path node is
addressed by sector, baked by the toolchain and streamed by distance; the frame draws and never
generates. Unreal: World Partition cells, HLOD built offline per cell, runtime streaming sources.
**Both agree on all four**: a FIXED grid fixed to the ground, an OFFLINE bake per cell, a LOD
ladder baked with the geometry, and a frame that only selects. Nothing here is a third answer.

## What is wrong

**The lab is a batch script wearing an engine's vocabulary.** Measured at OldTown, 2026-09-07:

| | |
|---|---|
| one picture, cold | 367 s roads + 40 s ground + the bodies, before Cycles saw a triangle |
| what decides the domain | a RADIUS AROUND THE CAMERA. Walk one metre and everything runs again |
| what is reused between pictures | nothing that a generator produced |
| ground | one flat CDT disc, 12 km, uniform density |
| bodies | 1 330 of 5 709 survive the cull and each is meshed ALONE |

**And `src/` already holds every structure the lab is missing.** This is the defect `CLAUDE.md`
names as the commonest in this tree -- a complete capability no caller reaches -- committed by the
lab against the engine:

| the engine has | the lab did instead |
|---|---|
| `GroundPatchwork.cpp: LayPatchwork` -- 4 tiles at the finest zoom, doubling per level, 8 levels, **240 km at constant cost per level** (`Laying.cpp:400`) | a polar disc, 72 rings x 96 spokes, uniform to 12 km |
| `SubjectCullStage.cpp` + `DepthPyramid.h` -- a 4-level Hi-Z with `ErrorPerMetre` | `occlusion.py`, **written from scratch** |
| `Generate.h: Detail / DetailAtRung / Unseen` -- "every level is BAKED when the geometry is built, never in a frame" | `visible.rung_for`, judging by SIZE, the rule that file records as already thrown away |
| `ContentStore` -- one content-addressed store, async | a different ad-hoc cache per module, each with its own key |

## Why the integration fails while each part is right

Every individual answer here was measurable and each one improved: 367 s to 140 s, 90 293
triangles to 23 976. **The optimisation target was always "this step, better" and never "the shape,
right"** -- a greedy local search over a wrong architecture converges to a well-polished wrong
shape. The reason it bites in INTEGRATION and not in a single problem is that a single problem has
an oracle (a residual, a picture, a number that goes red) and the integration had none.

## The decision

**Four structures, mirrored from `src/`, and nothing invented.**

1. **The CELL is the unit and it is fixed to the ground.** A slippy tile (`tile.py`, already
   cross-checked against `Tile.h`). Never a radius around a camera -- a camera-shaped domain is
   not deterministic, which is a defect before it is a cost
2. **BAKE and DRAW are different verbs.** `bake(cell, detail)` writes a content-addressed
   artefact; the frame reads it. The key carries the inputs AND the digest of the source that
   computes them (`bake.py`), so an edited solver can never be served a stale answer
3. **The ground is a CASCADE**, `LayPatchwork`'s shape: 4 tiles at the finest zoom, doubling per
   level. The CDT with the street cut out is laid on the FINEST level only
4. **One culler, and it is the engine's**: frustum plus a Hi-Z, chosen by the ERROR a
   simplification projects. `occlusion.py` is drawn back into that shape or deleted

## The proofs

| | claim | negative control |
|---|---|---|
| **A1** | the second render of one place touches NO generator: bake hits 100 % | change one source byte -> every bake misses -> red |
| **A2** | the picture is byte-identical whichever cells were already baked | drop one cell's bake -> still identical, or red |
| **A3** | moving the camera one metre does not move any node's height | the old camera-radius extract -> red by metres |
| **A4** | ground cost is O(levels) and not O(area): 240 km costs under twice 3 km | the disc -> red |
| **A5** | the culled picture equals the uncut picture pixel for pixel | hide one visible way -> red |

**A5 is the one that matters.** Every other number can be right while the picture has quietly lost
something, and only a pixel comparison against the uncut render can say it has not.
