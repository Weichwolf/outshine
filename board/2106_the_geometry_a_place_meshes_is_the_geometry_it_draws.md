Type: bug
State: withdrawn
Area: engine, compositor, render
Tags: measured, owner

# The geometry a place MESHES is the geometry it DRAWS

**Benchmark** -- Unreal: `r.Streaming.SyncStates` and the `Stat Streaming` overlay exist so that
"built but not drawn" is a NUMBER rather than a look; a mesh whose bounds miss the frustum is
culled and COUNTED as culled. RAGE: `grcDrawableSpu` reports batches submitted against batches
issued, and a drawable that produced no batch is a streaming bug rather than a silent zero.
**Both agree**: the count that was built and the count that reached the frame are two numbers, and
a gap between them is a defect with a name. This tree has the first number and not the second.

## Measured 2026-09-03, CentralPark

Once board:2105's refusal made the place preload, its case reads:

    ROW  CentralPark  d56ae2e1  p50 5.82  p95 6.55  p99 6.79 ms  1 of 120 over 16.67
    UNPREPARED CentralPark meshed 3932159 building triangle(s) and its picture varies by
    0.993900 of 255 along its rows -- the frame holds the sky and the ground and NONE of
    the geometry that was built for it

So: 3 932 159 building triangles meshed, 577 MB of vertices carried, a frame that holds none of
them, and a frame time INSIDE budget precisely because it draws nothing. **A place that draws no
buildings is fast, and the p99 column cannot tell that from a place that draws them quickly.**

This was invisible while the place refused to preload at all -- one loud refusal uncovered the
next, which is the argument for making them loud.

## And the ring cut made it a SECOND place, which sharpens what it is

After buildings were held to one tile-ring (board:2105), `outshine/places` reads 8 PASS /
2 UNPREPARED with Shibuya green and **Jura** newly red:

| place | triangles | variation | camera |
|---|---|---|---|
| CentralPark | 1 239 419 | 0.959 | a park |
| Jura | 62 328 | 0.743 | 94 km at the Alps |

The bar is `Triangles > 0 && Variation < 1.0`, and the case's own words are that a picture
of nothing is a vertical gradient with no horizontal variation. Jura passed before the cut
and fails after it -- so the cut removed the buildings that were carrying its horizontal
variation, and what is left is built and not seen.

**That is this item exactly, and it is now the cheaper case to reason about.** Jura's
camera looks at mountains 94 km away; the buildings inside one kilometre are behind or
below it. Sixty-two thousand triangles are meshed for a frame that cannot contain them.
Whichever way it resolves -- they should be drawn, or they should never have been built --
the defect is the same one: **nothing in this engine compares what was BUILT against what
was DRAWN**, so neither answer can be told from the other.

The ring cut is a distance test. What Jura wants is a FRUSTUM test, which is the same
question Unreal answers with `MaxDrawDistance` PLUS its cull volumes and RAGE with
streaming radii PLUS the portal graph: distance is the cheap half and visibility is the
other. This tree has the cheap half now.

## THE PROOF: twice the geometry, the same picture

Measured 2026-09-03 by moving the building ring between one tile and two:

| ring | CentralPark triangles | CentralPark variation | Jura variation | Shibuya |
|---|---|---|---|---|
| 1 | 1 239 419 | 0.959 | 0.743 | loads |
| 2 | 2 371 451 | 0.994 | 1.361 | does NOT load |

**CentralPark's geometry nearly doubles and its picture does not change.** 1.13 million
extra triangles move the horizontal variation by 0.035 of 255. If those triangles were
reaching the frame, doubling them would not leave the picture where it was.

So for CentralPark the answer is settled without the counter this item still owes: what
is built is not drawn. Jura is the other case -- there the ring cut removed geometry the
picture WAS carrying, and it crosses back over the bar at ring 2. Two places, two
directions, one missing number between them.

Ring 1 is what stands, because it is the only setting where all nine places load, and
because Jura's red is a true finding rather than an artefact: sixty-two thousand
triangles meshed for a frame that cannot hold them is the same defect seen from the
other side.

## AND THE GROUND IT STANDS ON IS 24 KILOMETRES DOWN

Measured 2026-09-03, the same measure at two places:

| place | `buildings: the ring within 3.2 km runs from` | eye, up | case |
|---|---|---|---|
| OldTown | **+317.7 m** | 496.9 m | green |
| CentralPark | **-24 149.8 m** | 83.2 m | red |

Twenty-four kilometres below the surface, WITHIN 3.2 km of the eye. Earth's curvature
drops 0.8 m over 3.2 km, so this is not the sphere -- it is a ring vertex placed where no
camera can see it, and the buildings are meshed onto that ring.

`tiles laid bare on the ellipsoid` reads 0, so the case's other guard -- elevation never
arrived, everything drawn at sea level -- does not catch it: the tiles ARE resident and
the height is wrong anyway. Sea level would be 0 m, not -24 km.

That is a sharper question than "built but not drawn", and probably the same one: geometry
does reach the frame and sits below the world. The counter this item owes would have said
`drawn > 0` and sent the search here in one step instead of three.

## The vertex is NEAR, and the sphere does not put it there

Following it one step further, all at CentralPark:

| measure | value |
|---|---|
| `ground: the ring within 3.2 km runs from` | -24 149.775 m |
| `relief: the ring's lowest vertex above the ellipsoid` | -24 149.425 m |
| `the ring's vertex that sinks furthest below its own altitude` | 12 128.314 m |
| `a sphere would sink it by` | 12 115.279 m |
| `relief: and how far out` the tallest lies | 149 142.865 m |

The curvature check passes: the deepest sink is 12 128 m against 12 115 m the sphere
accounts for, thirteen metres apart over a ring that reaches 393 km. **So the sphere is
modelled correctly and is not the cause.**

But `kFootprintReachM` is 3 200.0 and the filter is
`east*east + south*south > kFootprintReachM*kFootprintReachM`, so the -24 149.775 m vertex
lies within 3.2 km of the frame origin, where curvature drops 0.8 m. And it is within
0.35 m of the ring's GLOBAL minimum, on a ring that runs out to 393 km.

A vertex 24 km down, 3 km away, at nearly the same depth as the farthest point of a
393 km ring. That is not a place on Earth; it is one vertex carrying a value from
somewhere else in the ring, and the buildings inside 3.2 km are meshed onto it.

## FOUR HUNDRED AND FORTY-ONE OF THEM, AND NONE AT OLDTOWN

Instrumented and measured, 2026-09-03:

| | CentralPark | OldTown |
|---|---|---|
| ring vertices within 3.2 km | 132 660 | 150 033 |
| of those, below -100 m | **441** | **0** |
| the deepest one | vertex 5690 | -- |
| how far out it lies | 2 110.8 m | -- |
| how deep | -24 149.8 m | -- |

A sphere sinks a vertex by `R(1 - cos t)` and moves it out by `R sin t`, so a 24 km sink
belongs 554 km away. This one sits 2.1 km out, where the sphere accounts for 0.35 m --
**a factor of 69 000** -- and it is not alone: 441 of CentralPark's near vertices are
below -100 m and none of OldTown's are.

So it is a shape, not a bad row. Each of those 441 drags every triangle that touches it
24 km down, which is geometry that reaches the frame as near-vertical sheets rather than
ground, and is the mechanism behind "the frame holds the sky and the ground and NONE of
the geometry that was built for it": the buildings are meshed onto a ring that is partly
in the mantle.

**And they are not one tile.** Their box spans -2 583.9 to +2 193.8 m east and -1 722.3 to
+2 921.2 m south -- the whole of the 3.2 km neighbourhood, not a corner of it. So this is
not one bad tile and not one bad fetch: it is a rule that fires all over the ring, on 0.33
per cent of its vertices.

**The height is sound when it is READ.** `GroundSample::At` now refuses anything outside
[-11 500, +9 500] m -- the Mariana Trench to Everest with margin -- and CentralPark still
carries its 441, unchanged, digest unmoved. So nothing enters the tree at -24 149 m: the
value is correct at the sample and wrong by the time it is a ring vertex. That rules out
the elevation source and the sampler, and leaves the pass between them and `inFrame`.

## FOUND: the elevation tiles decode off the planet

`TerrainGrid::FromTerrariumPng` turns every byte triple into a height and checked nothing.
Terrarium packs `R*256 + G + B/256 - 32768`, so ANY three bytes decode to something: a
green pixel of (33, 171, 0) reads as -24 149 m. The decoder now says when a tile leaves
the planet, and at CentralPark it says it fifteen times:

    dem_off_earth pixels=475 deepestM=-24651 ofPixels=65536
    dem_off_earth pixels=475 deepestM=-24651 ofPixels=65536
    dem_off_earth pixels=214 deepestM=-14939 ofPixels=65536

Four hundred and seventy-five of 65 536 pixels -- 0.7 per cent of one tile. OldTown,
Venice and Kaiserberg report zero. **That is the whole chain**: bad pixels in this place's
elevation tiles, 441 ring vertices dragged 24 km down, triangles that reach the frame as
near-vertical sheets, and a picture that does not change when the buildings double.

### Filling the scattered holes takes two thirds of it, and shows the bound is too coarse

A pixel that decoded off the planet is filled from its sound neighbours, and a tile with no
sound pixel anywhere is refused whole. Measured:

| | before | after |
|---|---|---|
| CentralPark's deepest near vertex | -24 149.8 m | **-9 513.7 m** |
| near vertices below -100 m | 441 | **375** |
| OldTown, Venice, Kaiserberg digests | -- | unmoved |

Two thirds of the depth, a sixth of the count -- and then it stops, because **the bound is
global and the nonsense is local.** [-11 500, +9 500] m admits the Mariana Trench, so a
Manhattan pixel reading -9 514 m passes it and gets filled INTO the neighbourhood rather
than out of it. A height that is possible on Earth can still be impossible here.

### A continuity test was tried and MADE IT WORSE, and the reason was already measured

The obvious next predicate: at zoom 14 a Terrarium texel spans 28.9 m, so a step of 200 m
over one texel is a slope of 82 degrees, steeper than any rock face holds across 29 m. A
sample departing from every sound neighbour by more than that is an outlier wherever it
sits. Written, measured, and reverted:

| | before | after the filter |
|---|---|---|
| deepest near vertex | -9 513.7 m | -9 145.7 m |
| near vertices below -100 m | 375 | **426** |

Worse. And the reason was in a measurement taken twenty minutes earlier and not used: the
damage comes in BANDS -- `rowsSpanned=11 colsSpanned=2`, `rowsSpanned=55 colsSpanned=4` --
not only as scattered pixels. A pixel inside a band has bad neighbours, so it agrees with
them and no neighbourhood test can see it. Only the 256x253 case is scattered.

**So the shape of the damage is two shapes, and one predicate cannot hold both.** Scattered
pixels want their neighbours' mean. Bands want something that knows a band when it sees
one -- and the tile's own distribution does, whatever shape the damage takes:

    median +/- 20 * median-absolute-deviation, over the samples that are on Earth at all

With 0.7 per cent damaged, both statistics are carried by the sound 99.3 per cent, and a
band sits outside them however wide it is. A tile of real mountains has a large deviation
and the same arithmetic leaves it alone, so this needs no knowledge of the place.

| | before | after |
|---|---|---|
| CentralPark's deepest near vertex | -24 149.8 m | **-76.6 m** |
| near vertices below -100 m | 441 | **0** |
| Jura's variation (the Alps, real relief) | 0.7432 | 0.7421 |
| OldTown's variation | 1.6656 | 1.6656 |
| ZurichPlan's variation | 6.5414 | 6.5399 |

**The ring is sound at every place and no place lost its relief.**

## AND THE PICTURE IS STILL EMPTY

CentralPark's ground now runs from -76.6 m, and its variation is 0.9584 against 0.9592
before. **So the sunken ring was not why the frame holds none of its geometry.** It was a
real defect, measured and repaired, and it was not this one.

What that leaves is the counter this item has owed from the first line: built, submitted
and drawn as three numbers. Every hypothesis reachable by arithmetic on the existing
measures has now been spent -- the geometry doubles and the picture does not, the ground
is sound and the picture does not, and nothing published says which pass drops it.

What it does NOT do is repair it. What stands in for a sample that decoded off the planet
-- the neighbour, the tile's median, a refusal of the whole tile -- is a decision about
what the world IS, and inventing one quietly is how a place ends up drawn from data nobody
checked. That is the next item's to make.

`GroundYield` cuts seams and sews them (`ground: of that, cutting the seams`, `sewing
them`), which is the pass that joins detail levels and the one place a vertex can be
given a neighbour's height. That is where this item looks next.

## 2026-09-03: the first of the three numbers now stands, and it moves the search

`Engine::State::TellsWhatCrossed` reads the `Geometry` at the last statement before
`Picture.Standing->Restand` -- the handover itself -- and counts the triangles in it. Every count
this tree published about ground geometry before it read `inFrame`, the BUILDER's array, so the
handover was the one boundary nothing looked at.

| place | built | handed to the renderer | parts |
|---|---|---|---|
| CentralPark | 1 239 419 | 2 190 429 | 5 |
| OldTown | 251 526 | 592 583 | 5 |

**The geometry crosses, and it crosses LARGER than it was built.** So whatever loses CentralPark's
buildings is downstream of the handover -- the frustum, the near plane, the cluster pass or the
submission -- and every hypothesis on the build side of it is closed. That is the first measurement
in this chain that moves the search instead of narrowing it, and it cost three lines.

Two things it does NOT say. It does not say why handed exceeds built: the ring is meshed per
detail level and `inFrame` is one of them, so the excess is expected and unquantified -- naming the
factor is the next reading, not a conclusion to draw here. And it says nothing about what the
renderer then does, which is the other two numbers.

## What is not yet known

Whether the geometry is outside the frustum, behind the near plane, culled by the cluster pass, or
never submitted. The measurement that separates these does not exist yet, which is the first thing
this item owes: **built triangles, submitted triangles and drawn triangles as three numbers beside
each other**, so the gap says WHERE it opens rather than THAT it opens.

`SceneRenderer::ReadClusterCounts` already reads `into.Indices` and `into.Batches` back off the
draw-argument buffer, so the third number is one readback away from standing beside the first.

## What will be true

- [x] every place publishes triangles HANDED OVER -- `TellsWhatCrossed`, 2026-09-03
- every place publishes triangles submitted and triangles drawn beside it
- a place whose drawn count is zero while its meshed count is not REFUSES, the way one that fails
  to preload now does
- CentralPark draws what it meshed, or says which pass dropped it

## The measurement that shows this was wrong

`shots --measures CentralPark` prints the three counts and they agree to within what culling
explains. The negative control is a place with the camera turned away from its buildings: drawn
falls to near zero while meshed does not, and the refusal fires.
