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
