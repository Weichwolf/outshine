Type: debt
State: open
Area: generators, engine
Tags: architecture, owner
Supersedes: 2114
Depends: 2115

# A footprint is stamped flat, a road follows its grade, and a junction agrees in one solve

**Benchmark** -- Unreal: `FlattenHeightEditBrush` RASTERISES a polygon over the height lattice
and writes; a Landscape Spline's centreline carries the grade and a level cross-section is
swept along it; every junction is authored and baked. RAGE: the same -- roads are cut into the
heightfield at cook time and the carriageway is a swept level section on a graded centreline.
**Both agree on the shape** and NEITHER solves a junction at run time, because the studio did.
outshine takes its world over the wire, so what they bake happens during preload -- the choice
is mine, and the solve has to be one whose answer has a derivation.

## The requirement, whole

1. an OSM footprint is PROJECTED onto the ground -- **done**: `TriangleBvh::Under`, a ray down
2. it is TESSELLATED -- board:2115's CDT, the outline a required edge
3. materials are set on it
4. the ground is FLATTENED to those stamps
5. a road follows the grade ALONG its length and is level ACROSS it -- **structurally present**:
   `RoadMesh.cpp:222-245` builds `Knot` rises from `GradeM` and calls `line.Rise` / `line.Bank`
6. a building footprint has NO SLOPE AT ALL

## Where it stands, measured 2026-09-04

```
  a stamp                              none; the ground is meshed FIRST and buildings placed on
                                       whatever came out
  the floor's spread across a footprint  MEASURED and acted on by nothing:
                                       `buildings: a stamp would fill p50/p95/worst` (Laying.cpp:426)
  Generate.h:157 Stamp                 "a generator's request that the ground become FLAT" -- the
                                       interface exists, the flatten does not
  junction levelling                   Jacobi over kLevelPasses = 24, bar kLevelledM = 0.01 m;
                                       breaks on convergence and NEVER converges: last shift
                                       5.67 m; one lane lifted 375 m at OldTown
  bridges                              decks RAISED off a ground never flattened, ramps eased after,
                                       ends trimmed -- compensation for the missing stamp
```

## The solution

On the lattice (board:2115), every step is a write into a grid and not a search over a mesh:

- **stamp**: rasterise the footprint over the lattice, write the seat height into every cell it
  covers; the CDT carries the outline as an edge so the slab's rim is exact
- **grade**: a corridor is a centreline with a height profile; the lattice cells under the
  corridor are written from the profile across, level, so the road is a stamp with a slope along
- **junction**: where centrelines meet, the shared node's height is ONE unknown; the profiles
  meeting there are solved together as a small least-squares system per junction -- one solve,
  a residual that is the answer's own number, no round count. A lane shift is bounded by the
  grade a corridor of that class may carry, a number with a derivation (a road class's maximum
  gradient), so 375 m cannot happen
- **bridge**: a corridor whose profile is ABOVE the lattice is a deck by definition; the ramp
  is the profile's own easing and the lattice under it is not touched

## What will be true

- [ ] A building's floor is level: the height spread across its footprint vertices reads 0 at
      every place, published beside the count of stamps
- [ ] A road's cross-slope is level and its grade along matches the terrain it crosses within the
      class's bound
- [ ] The junction solve publishes its residual and it is under the bar it derives; no lane
      moves further than its class's gradient allows over its length
- [ ] `streets: vertices FLYING` returns as a CASE with an oracle, not a ledger line, and reads 0
- [ ] `refining`, `cutting the seams`, `sewing them` read 0.000 ms (board:2115)

## Ruled out, measured

- the data layout of the levelling (flat node-sorted vector, one offset per lane) -- done, all
  digests bit-identical; it was the cost, not the verdict
- a continuity filter on the elevation tiles made the sunken-ring count WORSE; the damage came
  in bands, and the tile's own distribution (median ± MAD) was the predicate
