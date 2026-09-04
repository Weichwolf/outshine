# A footprint is stamped flat and a road follows its grade

State: open

The requirement, stated whole so that no part of it is inferred from the code that stands there
now:

1. **an OSM footprint is PROJECTED onto the ground**
2. **it is TESSELLATED**
3. **materials are set on it**
4. **the ground is then FLATTENED to those stamps**
5. **roads and paths follow the grade ALONG THEIR LENGTH, and are level ACROSS it**
6. **a building footprint has NO SLOPE AT ALL** -- it is horizontal, by definition of a floor

Points 5 and 6 are the ones the current tree gets structurally wrong, and they are the reason the
stamps exist: a building sits on a level slab, a road is a ribbon that climbs.

## This is a solved problem and the shape is not ours to invent

| | how it is done | where |
|---|---|---|
| project a point onto a surface | ray straight down against an acceleration structure | shrinkwrap (Blender), conform (3ds Max), project-to-surface (Maya), `sampleHeightMostDetailed` (Cesium), heightmap fetch (Unreal) |
| flatten ground under a footprint | RASTERISE the polygon over the height lattice and write | Unreal Landscape `FlattenHeightEditBrush`, every terrain editor since ~2004 |
| a road that climbs | the CENTRELINE carries the grade; the cross-section is level and swept along it | RAGE and Unreal both, and it is what a real carriageway is |

Step 1 landed on 2026-09-04: `TriangleBvh::Under` replaced an 89-line cell grid, and the query is
now the ray every package uses. Steps 2 to 6 are open.

## What the tree does today, and why it fights the requirement

- the ground is meshed FIRST and the buildings are placed onto whatever came out, so a footprint
  inherits whatever slope the terrain had there -- there is no stamp, so nothing is flattened
- `Sew`, `Cut` and `Refine` exist to repair a mesh that was never coherent (board:2115); a stamped
  lattice has no seams to repair
- the bridge code is grown around this: decks are RAISED off a ground that was never flattened,
  ramps eased afterwards, ends trimmed. That is compensation for a missing step, and it is to be
  rewritten rather than tuned

## The order this has to be built in

Ground lattice (board:2115, projected grid) -> stamps for footprints and road corridors -> flatten
-> mesh once -> place. Every step after the first is cheaper on a lattice than on the irregular
mesh, which is why the lattice comes first and why optimising the repair passes was work spent on
the wrong side of the problem.

## What will show I was wrong

A building whose floor is not level, measured as the height spread across its footprint vertices --
today unmeasured, and it should be zero. A road whose cross-slope is not level, and whose grade
along its length does not match the terrain it crosses. And `refining`, `cutting the seams`,
`sewing them` reading 0.000 ms because the passes are gone.
