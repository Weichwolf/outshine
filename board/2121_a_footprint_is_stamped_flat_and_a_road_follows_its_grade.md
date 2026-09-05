Type: debt
State: active
Area: generators, engine
Tags: architecture, owner
Supersedes: 2114

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
  is the profile's own easing and the lattice under it is not touched. Where OSM does not say
  (`bridge=yes`, `layer`, `tunnel`), the HEURISTIC decides: a way crossing water, a rail or a
  way of a higher `layer` without a shared node is raised, with the approach graded at the
  class's maximum gradient -- the `Crosses` / `Bridges` passes carry this today and move with
  board:2101, as a rule with its numbers derived rather than tuned

## What will be true

- [x] A building's floor is level: no lattice node inside a footprint stands above its seat --
      0.000015 m at OldTown on the page the shader draws, 5.711 m before the press as the
      control, `ScoreAFootprintStandsOnALevelFloor` (2026-09-05); a pad cuts and never fills,
      so the floor claim is one-sided and the foundation's depth is published beside it
- [x] A road's cross-slope is level: every node inside a filling corridor piece reads the
      piece's graded plane, 0.000027 m at OldTown, 17.8 m above and 26.6 m below before
- [ ] A road's grade along matches the terrain it crosses within the class's bound: corridor
      pieces at OldTown ask for 60-90 m of yield, the grade solve's defect, with board:2101
- [ ] The junction solve publishes its residual and it is under the bar it derives; no lane
      moves further than its class's gradient allows over its length
- [ ] `streets: vertices FLYING` returns as a CASE with an oracle, not a ledger line, and reads 0
- [x] `refining`, `cutting the seams`, `sewing them` no longer exist as passes or as measures:
      board:2115 closed 2026-09-05 with the lattice as the ground and the ring's CPU path
      deleted; a stamp is a press into the sheets' nodes, bounded by `kMostEarthworkM`
- [ ] FROM board:2115: the constrained edge exists only at the KERB. The CDT shrank to the kerb
      the day the ground moved to the GPU lattice; the road stays a ribbon (a car drives on it,
      a kerb has an edge) pressed by its corridor stamp under it, and a case counts the kerb's
      edges that coincide with the ribbon's -- a footprint's outline is likewise the pad's
      rim, stamped, never a ground edge

## Ruled out, measured

- the data layout of the levelling (flat node-sorted vector, one offset per lane) -- done, all
  digests bit-identical; it was the cost, not the verdict
- a continuity filter on the elevation tiles made the sunken-ring count WORSE; the damage came
  in bands, and the tile's own distribution (median ± MAD) was the predicate

## Decided 2026-09-04, with board:2115's height field: a stamp is a WRITE into the height texture

With the ground a GPU height field, steps 2 to 6 become one compute pass and one ribbon:

- **stamp**: every footprint polygon and every corridor is rasterised into the height texture
  of each level it touches (a compute kernel over the polygon's bounding box, a point-in-polygon
  per texel, the seat height written); the slab is level because every texel under it holds one
  number, and the rim is as sharp as the finest level's texel (sub-metre near the eye)
- **grade**: a corridor writes its centreline profile across its width, level across, so the
  road's texels carry the grade; the ribbon mesh is swept on the same profile, so ribbon and
  ground agree by construction
- **junction**: the profiles meeting at a node are solved ONCE (least squares over the meeting
  profiles, bounded by the class's gradient) before the stamp is written; no relaxation
- **bridge**: a corridor above the field is a ribbon with no stamp; the heuristic (OSM tags, a
  crossing without a shared node) decides, board:2101 carries it

The floor's spread across a footprint is then measured on the texture, and it is zero by
construction; the oracle case reads it anyway.

## Landed 2026-09-05: the floor is measured on the page the shader draws, and a case holds it

The stamp of board:2115 is a PRESS into the sheets' nodes (`PressPoints`, two passes, bounded by
`kMostEarthworkM`), and nothing measured what it left: `buildings: a stamp would fill` reads
the seat spread BEFORE the press, on the drape, which is the stamp's work and not its result.
The press now reports, per stamp kind (`Stamp::Pad` · `Corridor` · `Basin`, one enum where a
`bool Basin` stood), every lattice node inside a stamp's ring and WHICH stamp decided its
height -- the stamp itself, another stamp whose cut or fill won or whose cut capped this one's
fill (`CappedBy`), or nobody. `HeightSheets::Press` re-reads the written float pages after the
write-back and measures the decided nodes against their stamp's plane (`FloorsOf`), signed:
above the plane, below it where the stamp fills, below it where it does not (a pad's
foundation, a deck's clearance). Unreal's flatten brush and RAGE's cook write the same number
into the height field; here the number is read back from the page.

```
  OldTown, measured 2026-09-05             pads        corridor pieces
  stamps with a lattice node inside        4 473       13 132
  stamps no node reaches                   8 126       73 903
  nodes inside, another stamp decided      5 972 of 57 642   16 824 of 59 901
  above the plane before / after           5.711 / 0.000 m   17.809 / 0.000 m
  below it, filling, before / after        -- (a pad cuts only)  26.617 / 0.000 m
  below it, not filling                    11.222 m (foundation)  14.823 m (deck clearance)
```

`ScoreAFootprintStandsOnALevelFloor` holds both zeros at a centimetre with the before-numbers
as the negative control. Three things the measure says that the counts did not:

- **a pad CUTS and never fills, by decision.** A house on a slope stands on a foundation (its
  walls run down to `FootM`), not on an embankment with a 2:3 batter around it -- the mound
  would be wrong in every terraced street. The corridor fills because a road IS an embankment.
  So the floor claim for a pad is one-sided: no ground above the seat
- **two of three stamps reach no node.** At the base level a lattice node stands every ~26 m
  (z15, 32 quads) and only the virtual levels near the eye refine toward the DEM's 3 m
  posting; a footprint narrower than a cell is pressed by nothing and sits on a blend the
  foundation hides. Unreal's Landscape writes at 0.5-2 m and RAGE's terrain is authored at
  1-4 m: both are an order finer. The resolution is board:2123's level of detail to decide
- **a corridor asks for 60-90 m of yield.** Pieces with `YieldM` of 62, 73, 82, 90 m at
  OldTown: lanes whose designed grade stands that far off the terrain, filled to the 30 m
  bound. That is the grade solve's defect (the `375 m` family above), measured per piece now,
  and it is the junction solve's to remove -- with board:2101, which reopens that code

What stays open here: the junction's one solve and its residual, the grade bound per class,
the kerb's constrained edge, the flying-vertices case -- all in the street code that
board:2101 moves, and opened once, there.
