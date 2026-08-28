Type: bug
State: open
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

## And it may be the reason the buildings cannot be seen (board:1946)

Every vertex in `Verts_` is an ECEF offset from `Anchor_`, which is set ONCE by `AnchorAt`. If the
anchor moves between builds -- and `GroundStack::Restand` re-anchors on the class field's origin --
then vertices meshed before the move are read against the wrong anchor and land somewhere else
entirely. That would put most of a town outside the frame while a handful of survivors show as the
two pixels board:1946 measured.

**This is a hypothesis and it is NOT yet measured.** The measurement that settles it: the anchor's
ECEF at each build, and whether it changes. If it never moves, this explains nothing and board:1946
must look elsewhere.

## What will be true

- [ ] the soup is bounded: a build takes the delta the field already marks, or the field trims what it replaced
- [ ] every vertex handed over is in the frame of the anchor it is read against, and the anchor's movement is a published number
- [ ] Shibuya does not mesh 600 MB of buildings to draw a square kilometre of them
