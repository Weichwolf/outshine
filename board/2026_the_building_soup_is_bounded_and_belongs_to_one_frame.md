Type: bug
State: active
Parent: 1946
Area: world
Tags: measured

# The building soup is bounded, and every vertex in it belongs to one frame

**Benchmark** — Unreal: streamed geometry is owned by the cell that streamed it and goes when the cell goes. Cesium: a tile's buffers are freed when the tile unloads. **They agree**, so the matter is closed: geometry belongs to the thing that brought it and is bounded by it.

MEASURED, from the telemetry the places now attach:

    t=0.0 INFO world buildings added=3228 total=5140 osmHeight=3136 defaultHeight=2004 vertsMB=59.7692

`BuildingField::Verts_` GROWS on every `Restand` and is never trimmed. At Rothenburg it reached
59.8 MB; at Shibuya the meshed count reads 6 121 139 triangles, which is roughly 600 MB of soup.
Folding that into the ring's own arrays -- a bisection, not a design -- crashed all six places with
SIGNAL, which is how the growth was found.

The field already carries the answer and nobody uses it: `AddedFirst()` and `AddedCount()` mark the
range added by the last build, exactly so a consumer can take a DELTA. Reading `Verts()` whole is
what makes the size unbounded.

## THE ANCHOR HYPOTHESIS IS DEAD, killed by the number this item demanded

It read: vertices meshed before an anchor move would be read against the wrong anchor and land
elsewhere, leaving a handful of survivors as the two pixels board:1946 measured. The measurement it
named was the anchor's ECEF against the frame's origin.

    buildings: their anchor lies from the frame's origin    0 m

Zero. The anchor and the render frame's origin coincide exactly, so no vertex is read against the
wrong point and this explains nothing about board:1946. WITHDRAWN.

What the same measurement did show is the size:

    floats in the soup                14 942 304    = 1 867 788 vertices = 622 596 triangles
    the field's last delta began at    5 682 768
    and ran for                        9 259 536    -- 62 per cent of the array, in ONE build
    footprints the field holds             5 140    -- 121 triangles each

## What will be true

- [ ] the soup is bounded: a build takes the delta the field already marks, or the field trims what it replaced
- [ ] every vertex handed over is in the frame of the anchor it is read against, and the anchor's movement is a published number
- [ ] Shibuya does not mesh 600 MB of buildings to draw a square kilometre of them


## AND IT NOW BLOCKS A MEASUREMENT (board:2029)

Rothenburg's building triangle count over three runs of the SAME code: 604 309, 582 147, 601 897 --
a spread of 3.7 per cent with nothing changed between them. `BuildingField::Build` adds to `Verts_`
and the telemetry reads `added=3228 total=5140` on a place that holds ONE vector tile, so a tile's
buildings are meshed more than once and how many times varies with how the restands fall.

That makes board:2029's needle count unusable: 13, then 8, then 760 across changes whose direction
should have been monotone. A defect cannot be measured against a population that moves on its own,
so this item now stands ahead of that one.

## The measurement that settles THIS

1. **Run the same place twice and subtract.** The building triangle count must be identical. Today
   it is not, and that difference is the whole item
2. `added` against `total` per build: a tile already ingested must add ZERO
