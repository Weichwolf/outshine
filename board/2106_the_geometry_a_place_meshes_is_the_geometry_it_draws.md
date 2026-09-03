Type: bug
State: open
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

## What is not yet known

Whether the geometry is outside the frustum, behind the near plane, culled by the cluster pass, or
never submitted. The measurement that separates these does not exist yet, which is the first thing
this item owes: **built triangles, submitted triangles and drawn triangles as three numbers beside
each other**, so the gap says WHERE it opens rather than THAT it opens.

`SceneRenderer::ReadClusterCounts` already reads `into.Indices` and `into.Batches` back off the
draw-argument buffer, so the third number is one readback away from standing beside the first.

## What will be true

- every place publishes triangles meshed, triangles submitted and triangles drawn
- a place whose drawn count is zero while its meshed count is not REFUSES, the way one that fails
  to preload now does
- CentralPark draws what it meshed, or says which pass dropped it

## The measurement that shows this was wrong

`shots --measures CentralPark` prints the three counts and they agree to within what culling
explains. The negative control is a place with the camera turned away from its buildings: drawn
falls to near zero while meshed does not, and the refusal fires.
